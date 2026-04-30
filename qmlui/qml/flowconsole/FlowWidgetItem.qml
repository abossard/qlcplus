/*
  Q Light Controller Plus
  FlowWidgetItem.qml

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

Rectangle
{
    id: fwRoot

    property VCWidget wObj: null
    property int colSpan: 1
    property bool isSelected: wObj ? flowConsole.selectedWidgetId === wObj.id : false

    // Size is managed by the parent GridLayout
    Layout.columnSpan: colSpan
    Layout.fillWidth: true
    Layout.preferredHeight: 60
    Layout.minimumHeight: 40

    color: wObj ? wObj.backgroundColor : UISettings.bgStrong
    visible: wObj ? wObj.isVisible : true

    // Background image
    Image
    {
        id: bgImage
        anchors.fill: parent
        source: wObj ? wObj.backgroundImage : ""
        fillMode: Image.Stretch
        visible: source !== ""
    }

    // Click-to-select in edit mode — z=1 so it's BELOW DropAreas (z=2)
    // and below the edit toolbar (z=10+)
    MouseArea
    {
        anchors.fill: parent
        enabled: flowConsole.editMode
        z: flowConsole.editMode ? 1 : -1

        onClicked:
        {
            if (wObj)
                flowConsole.selectWidget(wObj.id)
        }
    }

    // Selection highlight + edit toolbar (edit mode only)
    Rectangle
    {
        anchors.fill: parent
        color: "transparent"
        border.width: isSelected ? 3 : 0
        border.color: "yellow"
        radius: 2
        z: 15
        visible: flowConsole.editMode && isSelected

        // Edit toolbar overlay at top of selected widget
        Row
        {
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: 4
            spacing: 2

            IconButton
            {
                width: UISettings.iconSizeMedium
                height: width
                bgColor: UISettings.bgMedium
                faSource: FontAwesome.fa_arrow_up
                faColor: UISettings.fgMain
                tooltip: qsTr("Move up")
                onClicked: flowConsole.moveWidgetUp(wObj.id)
            }
            IconButton
            {
                width: UISettings.iconSizeMedium
                height: width
                bgColor: UISettings.bgMedium
                faSource: FontAwesome.fa_arrow_down
                faColor: UISettings.fgMain
                tooltip: qsTr("Move down")
                onClicked: flowConsole.moveWidgetDown(wObj.id)
            }
            IconButton
            {
                width: UISettings.iconSizeMedium
                height: width
                bgColor: UISettings.bgMedium
                faSource: FontAwesome.fa_minus
                faColor: UISettings.fgMain
                tooltip: qsTr("Decrease span")
                onClicked:
                {
                    var cs = flowConsole.widgetColSpan(wObj.id)
                    if (cs > 1)
                        flowConsole.setWidgetColSpan(wObj.id, cs - 1)
                }
            }
            IconButton
            {
                width: UISettings.iconSizeMedium
                height: width
                bgColor: UISettings.bgMedium
                faSource: FontAwesome.fa_plus
                faColor: UISettings.fgMain
                tooltip: qsTr("Increase span")
                onClicked:
                {
                    var cs = flowConsole.widgetColSpan(wObj.id)
                    flowConsole.setWidgetColSpan(wObj.id, cs + 1)
                }
            }
            IconButton
            {
                width: UISettings.iconSizeMedium
                height: width
                bgColor: "#660000"
                faSource: FontAwesome.fa_trash_can
                faColor: "red"
                tooltip: qsTr("Delete widget")
                onClicked: flowConsole.removeWidget(wObj.id)
            }
        }
    }
}
