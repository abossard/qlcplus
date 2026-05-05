/*
  Q Light Controller Plus
  liveaudioanalyzer.cpp

  Copyright (c) Massimo Callegari

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#include <QtMath>

#include "audiocapture.h"
#include "liveaudioanalyzer.h"

namespace
{
    float amplitudeToDb(double value)
    {
        return float(20.0 * qLn(qMax(value, 0.00001585)) / qLn(10.0));
    }

    double bandCenterHz(int index, int count)
    {
        if (count <= 0)
            return 0.0;

        const double minFreq = double(AudioCapture::minFrequency());
        const double maxFreq = double(AudioCapture::maxFrequency());
        const double logRange = qLn(maxFreq / minFreq);
        const double start = minFreq * qExp(logRange * (double(index) / double(count)));
        const double end = minFreq * qExp(logRange * (double(index + 1) / double(count)));
        return qSqrt(start * end);
    }

    void addBand(int &count, float *target, float value)
    {
        *target += value;
        count++;
    }
}

AudioFeatures LiveAudioAnalyzer::analyze(double rms,
                                          double peak,
                                          const std::array<double, AUDIO_FEATURE_BANDS> &logBands,
                                          double maxMagnitude)
{
    AudioFeatures features;
    const int bandCount = AUDIO_FEATURE_BANDS;
    features.rmsDb = amplitudeToDb(rms);
    features.peakDb = amplitudeToDb(peak);
    features.crestFactor = (rms > 0.0) ? float(peak / rms) : 0.0f;

    double magnitudeSum = 0.0;
    double weightedFrequencySum = 0.0;
    double rolloffTarget = 0.0;
    double rolloffSum = 0.0;
    double geometricSum = 0.0;
    double arithmeticSum = 0.0;

    int subCount = 0;
    int bassCount = 0;
    int lowMidCount = 0;
    int midCount = 0;
    int highCount = 0;

    for (int i = 0; i < bandCount; i++)
    {
        const double magnitude = qMax(0.0, logBands[i]);
        const float normalized = (maxMagnitude > 0.0) ? float(qBound(0.0, magnitude / maxMagnitude, 1.0)) : 0.0f;
        const double centerHz = bandCenterHz(i, bandCount);

        features.bandsLog[i] = float(magnitude);
        features.bandsNormalized[i] = normalized;
        features.bandsDb[i] = amplitudeToDb(normalized);

        magnitudeSum += magnitude;
        weightedFrequencySum += magnitude * centerHz;
        arithmeticSum += magnitude;
        geometricSum += qLn(qMax(magnitude, 0.000000001));

        if (centerHz < 80.0)
            addBand(subCount, &features.bands.sub, normalized);
        else if (centerHz < 250.0)
            addBand(bassCount, &features.bands.bass, normalized);
        else if (centerHz < 500.0)
            addBand(lowMidCount, &features.bands.lowMid, normalized);
        else if (centerHz < 2000.0)
            addBand(midCount, &features.bands.mid, normalized);
        else
            addBand(highCount, &features.bands.high, normalized);
    }

    if (subCount > 0)
        features.bands.sub /= float(subCount);
    if (bassCount > 0)
        features.bands.bass /= float(bassCount);
    if (lowMidCount > 0)
        features.bands.lowMid /= float(lowMidCount);
    if (midCount > 0)
        features.bands.mid /= float(midCount);
    if (highCount > 0)
        features.bands.high /= float(highCount);

    if (magnitudeSum > 0.0)
    {
        features.spectralCentroidHz = float(weightedFrequencySum / magnitudeSum);
        rolloffTarget = magnitudeSum * 0.85;
        for (int i = 0; i < bandCount; i++)
        {
            rolloffSum += qMax(0.0, logBands[i]);
            if (rolloffSum >= rolloffTarget)
            {
                features.spectralRolloffHz = float(bandCenterHz(i, bandCount));
                break;
            }
        }
    }

    if (bandCount > 0 && arithmeticSum > 0.0)
    {
        const double geometricMean = qExp(geometricSum / double(bandCount));
        const double arithmeticMean = arithmeticSum / double(bandCount);
        features.spectralFlatness = float(qBound(0.0, geometricMean / arithmeticMean, 1.0));
    }

    float flux = 0.0f;
    for (int i = 0; i < bandCount; i++)
    {
        const float rise = features.bandsNormalized[i] - m_previousBands[i];
        if (rise > 0.0f)
            flux += rise;
        m_previousBands[i] = features.bandsNormalized[i];
    }

    features.spectralFlux = flux;
    m_fluxAverage = (0.9f * m_fluxAverage) + (0.1f * flux);
    features.onset = (features.rmsDb > -54.0f && flux > qMax(0.35f, m_fluxAverage * 1.8f));

    return features;
}

AudioFeatures LiveAudioAnalyzer::analyzeSilence()
{
    AudioFeatures features;
    features.bandsDb.fill(-96.0f);
    m_previousBands.fill(0.0f);
    m_fluxAverage = 0.0f;
    return features;
}
