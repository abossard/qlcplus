/*
  Q Light Controller Plus
  VCEditMessage.qml

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

import "."

/* Why an edit was refused, shown at the bottom of the console for a few
   seconds. Unlike a dialog it blocks nothing; a click hides it earlier */
Rectangle
{
    id: messageBar
    visible: false
    z: 100
    anchors.horizontalCenter: parent.horizontalCenter
    anchors.bottom: parent.bottom
    anchors.bottomMargin: UISettings.listItemHeight
    width: Math.min(parent.width * 0.6, messageText.implicitWidth + UISettings.listItemHeight)
    height: messageText.implicitHeight + UISettings.listItemHeight / 2
    radius: 4
    color: "#B03A2E"
    border.color: UISettings.bgLight

    /* the same as the dialog it replaces */
    property string message: ""
    property string title: ""

    function open()
    {
        visible = true
        hideTimer.restart()
    }

    function close()
    {
        visible = false
    }

    Text
    {
        id: messageText
        anchors.centerIn: parent
        width: Math.min(implicitWidth, messageBar.parent.width * 0.6 - UISettings.listItemHeight)
        wrapMode: Text.Wrap
        horizontalAlignment: Text.AlignHCenter
        color: UISettings.fgMain
        font.family: UISettings.robotoFontName
        font.pixelSize: UISettings.textSizeDefault
        text: messageBar.message
    }

    Connections
    {
        target: vcEditor
        function onMessageRequested(text)
        {
            messageBar.message = text
            messageBar.open()
        }
    }

    Timer
    {
        id: hideTimer
        interval: 5000
        onTriggered: messageBar.close()
    }

    MouseArea
    {
        anchors.fill: parent
        onClicked: messageBar.close()
    }
}
