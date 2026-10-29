/*
  Q Light Controller Plus
  NestedShowBack.qml

  Copyright (c) QLC+ contributors

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

import "."

/* Back to the Show a nested Show was opened from */
GenericButton
{
    objectName: "nestedShowBack"
    visible: showClipEditor.canGoBack
    width: Math.min(contentWidth + UISettings.iconSizeDefault, UISettings.bigItemHeight * 3)
    height: UISettings.iconSizeDefault - 10
    fontSize: UISettings.textSizeDefault * 0.8
    label: "◀ " + showClipEditor.parentShowName
    tooltip: qsTr("Back to %1").arg(showClipEditor.parentShowName)

    onClicked: showClipEditor.goBack()
}
