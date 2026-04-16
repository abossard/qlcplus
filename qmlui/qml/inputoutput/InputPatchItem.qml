/*
  Q Light Controller Plus
  InputPatchItem.qml

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
import "GenericHelpers.js" as Helpers
import "."

Rectangle
{
    id: ipRoot
    width: parent.width
    height: UISettings.bigItemHeight * 0.9
    color: "transparent"

    property int universeID
    property InputPatch patch

    signal removeProfile()

    Rectangle
    {
        id: profileBox
        width: parent.width
        height: UISettings.bigItemHeight * 0.85
        visible: patch ? (patch.profileName === "None" ? false : true) : false

        border.width: 2
        border.color: UISettings.borderColorDark
        color: "#269ABA"
        radius: 10

        IconButton
        {
            y: 4
            x: parent.width - width - 10
            height: UISettings.bigItemHeight * 0.25
            width: height
            faSource: FontAwesome.fa_xmark
            faColor: "white"
            tooltip: qsTr("Remove this input profile")
            onClicked: ipRoot.removeProfile()
        }

        RobotoText
        {
            x: 10
            y: 3
            height: UISettings.bigItemHeight * 0.3
            width: parent.width - 20
            label: patch ? patch.profileName : ""
            labelColor: "black"
            fontSize: UISettings.textSizeDefault
        }
    }

    Rectangle
    {
        id: patchBox
        width: profileBox.visible ? parent.width - 10 : parent.width
        height: profileBox.visible ? UISettings.bigItemHeight * 0.5 : UISettings.bigItemHeight * 0.8
        y: profileBox.visible ? UISettings.bigItemHeight * 0.3 : 0
        x: profileBox.visible ? 5 : 0
        z: 1
        radius: 3
        color: UISettings.bgLighter
        border.width: 2
        border.color: UISettings.borderColorDark

        /* LED kind-of signal indicator */
        Rectangle
        {
            id: valueChangeBox
            x: parent.width - width - 10
            y: 10
            z: 1
            width: UISettings.iconSizeMedium * 0.75
            height: width
            radius: height / 2
            border.width: 2
            border.color: UISettings.bgMedium

            // Persistent color based on plugin connection status:
            // 0 (Idle) = dark, 1 (Advertising) = orange, 2 (Connected) = green
            property int pStatus: patch ? patch.pluginStatus : 0
            color: pStatus === 2 ? "#00FF00" : pStatus === 1 ? "orange" : UISettings.bgLight

            ColorAnimation on color
            {
                id: cAnim
                from: "#00FF00"
                to: valueChangeBox.pStatus === 2 ? "#00FF00" : valueChangeBox.pStatus === 1 ? "orange" : UISettings.bgLight
                duration: 500
                running: false
            }

            Connections
            {
                id: valChangedSignal
                target: patch
                function onInputValueChanged(inputUniverse, channel, value, key)
                {
                    cAnim.restart()
                }
            }
        }

        RowLayout
        {
            x: 8
            width: parent.width - 16
            anchors.verticalCenter: parent.verticalCenter
            spacing: 3

            Image
            {
                height: ipRoot.height * 0.75
                width: height
                source: patch ? Helpers.pluginIconFromName(patch.pluginName) : ""
                sourceSize: Qt.size(width, height)
                fillMode: Image.Stretch
            }
            RobotoText
            {
                height: ipRoot.height
                Layout.fillWidth: true
                label: patch ? patch.inputName : ""
                labelColor: "black"
                wrapText: true
                fontSize: UISettings.textSizeDefault
            }
        }
    }
}

