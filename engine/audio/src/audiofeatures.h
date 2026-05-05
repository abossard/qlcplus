/*
  Q Light Controller Plus
  audiofeatures.h

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

#ifndef AUDIOFEATURES_H
#define AUDIOFEATURES_H

#include <array>
#include <QtGlobal>

static constexpr int AUDIO_FEATURE_BANDS = 32;

struct AudioFeatures final
{
    enum Source
    {
        Live,
        Cached
    };

    struct PerceptualBands
    {
        float sub = 0.0f;
        float bass = 0.0f;
        float lowMid = 0.0f;
        float mid = 0.0f;
        float high = 0.0f;
    };

    Source source = Live;
    qint64 trackId = -1;
    double positionMs = 0.0;

    float rmsDb = -96.0f;
    float peakDb = -96.0f;
    float crestFactor = 0.0f;

    std::array<float, AUDIO_FEATURE_BANDS> bandsLog {};
    std::array<float, AUDIO_FEATURE_BANDS> bandsDb {};
    std::array<float, AUDIO_FEATURE_BANDS> bandsNormalized {};
    PerceptualBands bands;

    float spectralCentroidHz = 0.0f;
    float spectralRolloffHz = 0.0f;
    float spectralFlatness = 0.0f;
    float spectralFlux = 0.0f;

    bool onset = false;
    bool beat = false;
    double bpm = 0.0;

    bool matchLocked = false;
    double matchConfidence = 0.0;
};

#endif // AUDIOFEATURES_H
