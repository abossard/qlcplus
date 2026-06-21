/*
  Q Light Controller Plus
  VCRecordPanelProperties.qml

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

Rectangle
{
    color: "transparent"
    height: rpPropsColumn.height

    property VCRecordPanel widgetRef: null

    property int gridItemsHeight: UISettings.listItemHeight

    Column
    {
        id: rpPropsColumn
        width: parent.width
        spacing: 5

        SectionBox
        {
            id: folderSection
            sectionLabel: qsTr("Folder and naming")

            sectionContents:
              GridLayout
              {
                width: parent.width
                columns: 2
                columnSpacing: 5
                rowSpacing: 4

                // Target folder
                RobotoText
                {
                    height: gridItemsHeight
                    Layout.fillWidth: true
                    label: qsTr("Target folder")
                }
                CustomTextEdit
                {
                    Layout.fillWidth: true
                    height: gridItemsHeight
                    text: widgetRef ? widgetRef.targetFolder : ""
                    onTextChanged: if (widgetRef) widgetRef.targetFolder = text
                }

                // Scene prefix
                RobotoText
                {
                    height: gridItemsHeight
                    Layout.fillWidth: true
                    label: qsTr("Scene prefix")
                }
                CustomTextEdit
                {
                    Layout.fillWidth: true
                    height: gridItemsHeight
                    text: widgetRef ? widgetRef.scenePrefix : ""
                    onTextChanged: if (widgetRef) widgetRef.scenePrefix = text
                }

                // Chaser prefix
                RobotoText
                {
                    height: gridItemsHeight
                    Layout.fillWidth: true
                    label: qsTr("Chaser prefix")
                }
                CustomTextEdit
                {
                    Layout.fillWidth: true
                    height: gridItemsHeight
                    text: widgetRef ? widgetRef.chaserPrefix : ""
                    onTextChanged: if (widgetRef) widgetRef.chaserPrefix = text
                }
              }
        }

        SectionBox
        {
            id: timingSection
            sectionLabel: qsTr("Default timing")

            sectionContents:
              GridLayout
              {
                width: parent.width
                columns: 2
                columnSpacing: 5
                rowSpacing: 4

                // Fade In
                RobotoText
                {
                    height: gridItemsHeight
                    Layout.fillWidth: true
                    label: qsTr("Fade in (ms)")
                }
                CustomSpinBox
                {
                    Layout.fillWidth: true
                    height: gridItemsHeight
                    from: 0
                    to: 600000
                    stepSize: 100
                    value: widgetRef ? widgetRef.defaultFadeIn : 0
                    onValueModified: if (widgetRef) widgetRef.defaultFadeIn = value
                }

                // Hold
                RobotoText
                {
                    height: gridItemsHeight
                    Layout.fillWidth: true
                    label: qsTr("Hold (ms)")
                }
                CustomSpinBox
                {
                    Layout.fillWidth: true
                    height: gridItemsHeight
                    from: 0
                    to: 600000
                    stepSize: 100
                    value: widgetRef ? widgetRef.defaultHold : 0
                    onValueModified: if (widgetRef) widgetRef.defaultHold = value
                }

                // Fade Out
                RobotoText
                {
                    height: gridItemsHeight
                    Layout.fillWidth: true
                    label: qsTr("Fade out (ms)")
                }
                CustomSpinBox
                {
                    Layout.fillWidth: true
                    height: gridItemsHeight
                    from: 0
                    to: 600000
                    stepSize: 100
                    value: widgetRef ? widgetRef.defaultFadeOut : 0
                    onValueModified: if (widgetRef) widgetRef.defaultFadeOut = value
                }
              }
        }
    }
}
