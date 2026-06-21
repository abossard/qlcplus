/*
  Q Light Controller Plus
  VCRecordPanelItem.qml

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
    id: recordPanelRoot
    property VCRecordPanel recordPanelObj: null
    property bool isRecording: recordPanelObj ? recordPanelObj.isRecordingChaser : false

    clip: true

    onRecordPanelObjChanged:
    {
        setCommonProperties(recordPanelObj)
    }

    ColumnLayout
    {
        anchors.fill: parent
        anchors.margins: 4
        spacing: 4

        // Header with folder name
        RobotoText
        {
            Layout.fillWidth: true
            height: UISettings.listItemHeight * 0.8
            label: recordPanelObj ? recordPanelObj.targetFolder : ""
            fontSize: UISettings.textSizeDefault * 0.8
            fontBold: true
            labelColor: recordPanelObj ? recordPanelObj.foregroundColor : "#ccc"
        }

        // Create Scene button
        GenericButton
        {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(UISettings.listItemHeight, (parent.height - UISettings.listItemHeight * 0.8 - 12) / 3)
            label: qsTr("Create Scene")
            enabled: recordPanelObj && !virtualConsole.editMode

            onClicked:
            {
                if (recordPanelObj)
                    recordPanelObj.createScene()
            }
        }

        // Start/Stop Chaser button (toggles)
        GenericButton
        {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(UISettings.listItemHeight, (parent.height - UISettings.listItemHeight * 0.8 - 12) / 3)
            label: isRecording ? qsTr("Stop Chaser") : qsTr("Start Chaser")
            enabled: recordPanelObj && !virtualConsole.editMode

            Rectangle
            {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 4
                width: 8
                height: 8
                radius: 4
                color: isRecording ? "red" : "transparent"
                border.width: 1
                border.color: isRecording ? "red" : "#666"

                SequentialAnimation on opacity
                {
                    running: isRecording
                    loops: Animation.Infinite
                    NumberAnimation { to: 0.3; duration: 500 }
                    NumberAnimation { to: 1.0; duration: 500 }
                }
            }

            onClicked:
            {
                if (recordPanelObj)
                {
                    if (isRecording)
                        recordPanelObj.stopChaser()
                    else
                        recordPanelObj.startChaser()
                }
            }
        }

        // Status label
        RobotoText
        {
            Layout.fillWidth: true
            Layout.fillHeight: true
            label: isRecording ? qsTr("Recording...") : qsTr("Ready")
            fontSize: UISettings.textSizeDefault * 0.7
            labelColor: isRecording ? "#FF4444" : "#888"
        }
    }

    WheelEater { anchors.fill: parent; z: 1 }
}
