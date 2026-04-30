/*
  Q Light Controller Plus
  FlowLabelItem.qml

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
import QtQuick.Layouts

import org.qlcplus.classes 1.0
import "."

FlowWidgetItem
{
    id: labelRoot

    Layout.preferredHeight: 40
    Layout.minimumHeight: 24
    radius: 2

    Text
    {
        x: 2
        width: parent.width - 4
        height: parent.height
        font: wObj ? wObj.font : Qt.font({ family: UISettings.robotoFontName })
        text: wObj ? wObj.caption : ""
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        color: wObj ? wObj.foregroundColor : "white"
    }
}
