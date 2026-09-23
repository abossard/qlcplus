/*
  Q Light Controller Plus
  ShowCommandList.qml

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
import QtQuick.Layouts

import "."

// Recorded commands of the current Show: time, action, target and value.
// Every edit goes through the recorder's model interface, which validates it
// and publishes the whole track, so a rejected edit changes nothing.
Rectangle
{
    id: commandListRoot
    color: UISettings.bgMedium
    border.color: UISettings.bgLight
    border.width: 1

    property int nudgeStep: 100

    ColumnLayout
    {
        anchors.fill: parent
        anchors.margins: 4
        spacing: 2

        RowLayout
        {
            Layout.fillWidth: true
            spacing: 6

            RobotoText
            {
                label: qsTr("Recorded commands")
                fontSize: UISettings.textSizeDefault * 0.8
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: "transparent" }

            RobotoText
            {
                label: qsTr("Ends at")
                fontSize: UISettings.textSizeDefault * 0.7
            }

            CustomTextEdit
            {
                id: extentEdit
                width: UISettings.bigItemHeight
                height: UISettings.listItemHeight - 4
                text: showCommandRecorder ? showCommandRecorder.extent : "0"
                onTextEdited: if (showCommandRecorder) showCommandRecorder.setExtent(parseInt(text))
            }
        }

        RobotoText
        {
            visible: showCommandRecorder && showCommandRecorder.lastError.length > 0
            Layout.fillWidth: true
            label: showCommandRecorder ? showCommandRecorder.lastError : ""
            labelColor: "#e67e22"
            fontSize: UISettings.textSizeDefault * 0.7
        }

        ListView
        {
            id: commandView
            objectName: "commandView"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: showCommandRecorder ? showCommandRecorder.commands : []

            delegate: Rectangle
            {
                width: commandView.width
                height: UISettings.listItemHeight
                color: index % 2 === 0 ? UISettings.bgLight : UISettings.bgMedium

                RowLayout
                {
                    anchors.fill: parent
                    anchors.leftMargin: 4
                    spacing: 4

                    CustomTextEdit
                    {
                        objectName: "commandTimeEdit"
                        implicitWidth: UISettings.bigItemHeight * 0.9
                        height: UISettings.listItemHeight - 4
                        text: modelData.time
                        onEditingFinished: showCommandRecorder.retimeCommand(modelData.id,
                                               parseInt(text))
                    }

                    CustomComboBox
                    {
                        objectName: "commandActionCombo"
                        width: UISettings.bigItemHeight
                        height: UISettings.listItemHeight - 4
                        property var actions: [ "Start", "Stop", "SetIntensity" ]
                        model: actions
                        currentIndex: actions.indexOf(modelData.action)
                        onValueChanged: function(value)
                        {
                            var picked = actions[value]
                            if (picked !== undefined && picked !== modelData.action)
                                showCommandRecorder.setCommandAction(modelData.id, picked)
                        }
                    }

                    CustomComboBox
                    {
                        objectName: "commandTargetCombo"
                        Layout.fillWidth: true
                        height: UISettings.listItemHeight - 4
                        model: showCommandRecorder ? showCommandRecorder.availableTargets : []
                        currValue: modelData.functionId
                        onValueChanged: function(value)
                        {
                            if (value !== modelData.functionId)
                                showCommandRecorder.setCommandTarget(modelData.id, value)
                        }
                    }

                    // Only a value command has a value: a trigger row offers no
                    // control to type one into.
                    Loader
                    {
                        active: modelData.isValue
                        visible: active
                        width: UISettings.bigItemHeight * 0.6
                        height: UISettings.listItemHeight - 4
                        sourceComponent: CustomTextEdit
                        {
                            objectName: "commandIntensityEdit"
                            implicitWidth: UISettings.bigItemHeight * 0.6
                            height: UISettings.listItemHeight - 4
                            text: Math.round(modelData.intensity * 100)
                            onEditingFinished: showCommandRecorder.setCommandIntensity(modelData.id,
                                                   parseFloat(text) / 100.0)
                        }
                    }

                    IconButton
                    {
                        width: UISettings.listItemHeight - 4
                        height: width
                        faSource: FontAwesome.fa_backward
                        tooltip: qsTr("Move earlier")
                        onClicked: showCommandRecorder.retimeCommand(modelData.id,
                                       Math.max(0, modelData.time - nudgeStep))
                    }

                    IconButton
                    {
                        width: UISettings.listItemHeight - 4
                        height: width
                        faSource: FontAwesome.fa_forward
                        tooltip: qsTr("Move later")
                        onClicked: showCommandRecorder.retimeCommand(modelData.id,
                                       modelData.time + nudgeStep)
                    }

                    IconButton
                    {
                        width: UISettings.listItemHeight - 4
                        height: width
                        faSource: FontAwesome.fa_trash
                        tooltip: qsTr("Delete")
                        onClicked: showCommandRecorder.removeCommand(modelData.id)
                    }
                }
            }
        }

        RobotoText
        {
            visible: commandView.count === 0
            Layout.fillWidth: true
            label: qsTr("Nothing recorded yet")
            fontSize: UISettings.textSizeDefault * 0.7
        }
    }
}
