/*
  Q Light Controller Plus
  liveaudioanalyzer.h

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

#ifndef LIVEAUDIOANALYZER_H
#define LIVEAUDIOANALYZER_H

#include <array>

#include "audiofeatures.h"

class LiveAudioAnalyzer final
{
public:
    AudioFeatures analyze(double rms,
                          double peak,
                          quint32 sampleRate,
                          const std::array<double, AUDIO_FEATURE_BANDS> &logBands,
                          double maxMagnitude);

    AudioFeatures analyzeSilence(quint32 sampleRate);

private:
    std::array<float, AUDIO_FEATURE_BANDS> m_previousBands {};
    float m_fluxAverage = 0.0f;
};

#endif // LIVEAUDIOANALYZER_H
