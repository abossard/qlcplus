/*
  Q Light Controller Plus
  dmxcapture.cpp

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

#include "dmxcapture.h"
#include "doc.h"
#include "fixture.h"
#include "inputoutputmap.h"
#include "universe.h"

QList<SceneValue> DmxCapture::captureAllFixtures(Doc *doc, bool nonZeroOnly)
{
    QList<SceneValue> result;

    if (doc == nullptr || doc->inputOutputMap() == nullptr)
        return result;

    QList<Universe *> ua = doc->inputOutputMap()->claimUniverses();

    // Read pre-GM values from all universes, applying passthrough where enabled
    QByteArray preGMValues(ua.size() * UNIVERSE_SIZE, 0);

    for (int i = 0; i < ua.count(); ++i)
    {
        const int offset = i * UNIVERSE_SIZE;
        preGMValues.replace(offset, UNIVERSE_SIZE, ua.at(i)->preGMValues());
        if (ua.at(i)->passthrough())
        {
            for (int j = 0; j < UNIVERSE_SIZE; ++j)
            {
                const int ofs = offset + j;
                preGMValues[ofs] =
                    static_cast<char>(ua.at(i)->applyPassthrough(j, static_cast<uchar>(preGMValues[ofs])));
            }
        }
    }

    doc->inputOutputMap()->releaseUniverses(false);

    // Iterate all fixtures and collect channel values
    for (Fixture *fixture : doc->fixtures())
    {
        quint32 baseAddress = fixture->universeAddress();

        for (quint32 chIndex = 0; chIndex < fixture->channels(); chIndex++)
        {
            uchar value = preGMValues.at(baseAddress + chIndex);
            if (!nonZeroOnly || value > 0)
            {
                result.append(SceneValue(fixture->id(), chIndex, value));
            }
        }
    }

    return result;
}
