/*
  Q Light Controller Plus
  SmoothZoomHandler.qml

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

import QtQuick

/**
 * Reusable zoom handler for mouse wheel and macOS trackpad.
 *
 * Properly handles both angleDelta (mouse wheel, 120-unit steps)
 * and pixelDelta (trackpad, smooth sub-pixel values).
 * Always consumes the event (wheel.accepted = true) to prevent
 * parent Flickable scroll interference.
 *
 * Usage:
 *   SmoothZoomHandler {
 *       anchors.fill: parent
 *       sensitivity: 0.01
 *       onZoomDelta: (delta) => {
 *           myScale += delta
 *       }
 *   }
 */

Item
{
    id: root

    /** Scale factor applied to raw wheel delta. */
    property real sensitivity: 0.01

    /** Maximum absolute zoom velocity per event. */
    property real maxVelocity: 0.05

    /** Emitted with a clamped zoom delta value. */
    signal zoomDelta(real delta)

    MouseArea
    {
        anchors.fill: parent
        acceptedButtons: Qt.NoButton

        onWheel: (wheel) =>
        {
            // Prefer angleDelta (works for both mouse and trackpad).
            // Fall back to pixelDelta for trackpads that report zero angleDelta.
            var rawDelta = wheel.angleDelta.y / 120.0
            if (rawDelta === 0 && wheel.pixelDelta.y !== 0)
                rawDelta = wheel.pixelDelta.y / 120.0

            var velocity = rawDelta * root.sensitivity
            velocity = Math.max(-root.maxVelocity, Math.min(root.maxVelocity, velocity))
            root.zoomDelta(velocity)
            wheel.accepted = true
        }
    }
}
