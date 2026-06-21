/*
  Q Light Controller Plus
  dmxcapture.h

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

#ifndef DMXCAPTURE_H
#define DMXCAPTURE_H

#include <QList>
#include "scenevalue.h"

class Doc;

/** @addtogroup engine Engine
 * @{
 */

/**
 * Utility class for capturing current DMX output values.
 * Reads pre-GrandMaster values from all universes and builds
 * a list of SceneValues suitable for creating Scenes.
 *
 * This is a pure data-extraction helper with no side effects.
 * Thread safety: caller must ensure Doc access is on the main thread.
 */
class DmxCapture
{
public:
    /**
     * Capture current pre-GM DMX values for all fixtures in the Doc.
     * Handles passthrough correction when universes have passthrough enabled.
     *
     * @param doc The document containing fixtures and universes
     * @param nonZeroOnly If true, only channels with value > 0 are included
     * @return List of SceneValues representing the current DMX state
     */
    static QList<SceneValue> captureAllFixtures(Doc *doc, bool nonZeroOnly = false);
};

/** @} */

#endif // DMXCAPTURE_H
