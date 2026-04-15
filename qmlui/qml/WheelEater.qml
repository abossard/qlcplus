/*
  Q Light Controller Plus
  WheelEater.qml

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
 * Transparent overlay that consumes wheel events to prevent them
 * from propagating to parent Flickable containers.
 *
 * Uses acceptedButtons: Qt.NoButton so all mouse clicks, presses,
 * and releases pass through to siblings/children unaffected.
 *
 * Usage: place as a child of any non-scrollable panel or widget.
 *
 *   WheelEater { anchors.fill: parent }
 */

MouseArea
{
    acceptedButtons: Qt.NoButton

    onWheel: (wheel) => { wheel.accepted = true }
}
