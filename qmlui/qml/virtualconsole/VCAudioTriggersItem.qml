/*
  Q Light Controller Plus
  VCAudioTriggersItem.qml

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
import QtQuick.Controls

import org.qlcplus.classes 1.0
import "."

VCWidgetItem
{
    id: audioTriggerRoot
    property VCAudioTriggers audioTriggerObj: null

    property variant barValues: audioTriggerObj ? audioTriggerObj.audioLevels : null

    clip: true

    onAudioTriggerObjChanged:
    {
        setCommonProperties(audioTriggerObj)
    }

    GridLayout
    {
        id: itemsLayout
        anchors.fill: parent
        columns: 2
        rows: 2

        // bars area
        Rectangle
        {
            id: barsItem
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.rowSpan: 2

            color: "transparent"

            // Beat flash overlay
            Rectangle
            {
                anchors.fill: parent
                color: "#FFFFFF"
                opacity: audioTriggerObj && audioTriggerObj.beatActive ? 0.12 : 0.0
                Behavior on opacity { NumberAnimation { duration: 120 } }
                z: 2
            }

            // Bars row (leaves room for the monitor row below)
            Row
            {
                id: bars
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: monitorRow.top

                Repeater
                {
                    id: barsRep
                    model: audioTriggerObj ? audioTriggerObj.barsNumber : 0

                    Rectangle
                    {
                        width: barsItem.width / Math.max(1, audioTriggerObj ? audioTriggerObj.barsNumber : 1)
                        height: parent.height
                        color: UISettings.bgStrong
                        border.width: 1
                        border.color: UISettings.bgLight

                        Rectangle
                        {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: barValues ? parent.height * (Math.max(0, Math.min(255, barValues[index] || 0)) / 255.0) : 0
                            radius: 3
                            color:
                            {
                                if (index === 0) return "#00FF00"; // volume bar stays green
                                if (!audioTriggerObj) return UISettings.selection;
                                var specIndex = index - 1;
                                if (specIndex < audioTriggerObj.lowCutBin) return "#FF6633";   // warm orange for lows
                                if (specIndex < audioTriggerObj.highCutBin) return "#FFCC00";  // yellow for mids
                                return "#33CCFF";                                              // cyan for highs
                            }
                        }
                    }
                }
            }

            // Split markers between volume/lows, lows/mids, mids/highs
            Item
            {
                id: splitMarkers
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: monitorRow.top
                z: 1

                property real barW: width / Math.max(1, audioTriggerObj ? audioTriggerObj.barsNumber : 1)

                // Volume / spectrum boundary
                Rectangle
                {
                    visible: audioTriggerObj !== null
                    width: 1
                    height: parent.height
                    x: splitMarkers.barW * 1
                    color: "#888888"
                    opacity: 0.6
                }
                // Low / mid boundary
                Rectangle
                {
                    visible: audioTriggerObj !== null
                    width: 2
                    height: parent.height
                    x: splitMarkers.barW * (1 + audioTriggerObj.lowCutBin) - 1
                    color: "#FFCC00"
                    opacity: 0.7
                }
                // Mid / high boundary
                Rectangle
                {
                    visible: audioTriggerObj !== null
                    width: 2
                    height: parent.height
                    x: splitMarkers.barW * (1 + audioTriggerObj.highCutBin) - 1
                    color: "#33CCFF"
                    opacity: 0.7
                }
            }

            // Monitor row: beat dot + L/M/H readouts (hidden when widget is too small)
            Row
            {
                id: monitorRow
                visible: barsItem.height > 80
                height: visible ? 18 : 0
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.leftMargin: 4
                anchors.rightMargin: 4
                spacing: 8
                z: 3

                Rectangle
                {
                    width: 12; height: 12
                    radius: 6
                    color: audioTriggerObj && audioTriggerObj.beatActive ? "#FF3333" : "#333333"
                    border.width: 1
                    border.color: "#000000"
                    anchors.verticalCenter: parent.verticalCenter
                    Behavior on color { ColorAnimation { duration: 80 } }
                }

                Text
                {
                    text: "L:" + (audioTriggerObj ? Math.round(audioTriggerObj.lowsPower * 100) : 0) + "%"
                    color: "#FF6633"
                    font.pixelSize: 10
                    font.bold: true
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text
                {
                    text: "M:" + (audioTriggerObj ? Math.round(audioTriggerObj.midsPower * 100) : 0) + "%"
                    color: "#FFCC00"
                    font.pixelSize: 10
                    font.bold: true
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text
                {
                    text: "H:" + (audioTriggerObj ? Math.round(audioTriggerObj.highsPower * 100) : 0) + "%"
                    color: "#33CCFF"
                    font.pixelSize: 10
                    font.bold: true
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }

        // enable button
        IconButton
        {
            width: height
            height: UISettings.iconSizeMedium
            Layout.alignment: Qt.AlignHCenter
            radius: 0
            border.width: 0
            checkable: true
            tooltip: qsTr("Enable/Disable the audio capture")
            faSource: FontAwesome.fa_check
            faColor: "lime"
            imgMargins: 1
            checked: audioTriggerObj ? audioTriggerObj.captureEnabled : false
            onToggled: if (audioTriggerObj) audioTriggerObj.captureEnabled = checked
        }

        // the volume fader
        QLCPlusFader
        {
            enabled: audioTriggerObj ? !audioTriggerObj.isDisabled : false
            Layout.alignment: Qt.AlignHCenter
            Layout.fillHeight: true
            width: parent.width
            from: 0
            to: 100
            value: audioTriggerObj ? audioTriggerObj.volumeLevel : 0
            onMoved: if (audioTriggerObj) audioTriggerObj.volumeLevel = valueAt(position)
        }
    }
}
