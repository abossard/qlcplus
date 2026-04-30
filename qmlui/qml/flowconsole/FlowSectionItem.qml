/*
  Q Light Controller Plus
  FlowSectionItem.qml

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
    id: sectionRoot

    property var sectionData: null
    property real parentFlowWidth: 400

    color: UISettings.bgStrong
    radius: 4
    border.width: 1
    border.color: UISettings.bgLight

    implicitWidth: {
        if (!sectionData) return parentFlowWidth
        var preset = sectionData.sizePreset
        if (preset === "half") return (parentFlowWidth - 10) / 2
        if (preset === "third") return (parentFlowWidth - 20) / 3
        if (preset === "quarter") return (parentFlowWidth - 30) / 4
        return parentFlowWidth  // "full"
    }
    implicitHeight: headerRow.height + (sectionData && sectionData.isCollapsed ? 0 : contentArea.implicitHeight)

    clip: true

    // Section header
    Rectangle
    {
        id: headerRow
        width: parent.width
        height: UISettings.listItemHeight
        radius: 4
        color: UISettings.bgMedium

        RowLayout
        {
            anchors.fill: parent
            anchors.leftMargin: 4
            anchors.rightMargin: 4
            spacing: 4

            // Collapse toggle
            IconButton
            {
                implicitWidth: UISettings.iconSizeMedium
                implicitHeight: UISettings.iconSizeMedium
                Layout.alignment: Qt.AlignVCenter
                bgColor: "transparent"
                faSource: sectionData && sectionData.isCollapsed ? FontAwesome.fa_caret_right : FontAwesome.fa_caret_down
                faColor: UISettings.fgMain
                onClicked:
                {
                    if (sectionData)
                        flowConsole.setSectionCollapsed(sectionData.id, !sectionData.isCollapsed)
                }
            }

            // Solo indicator
            Rectangle
            {
                visible: sectionData ? sectionData.isSolo : false
                implicitWidth: UISettings.iconSizeMedium
                implicitHeight: UISettings.iconSizeMedium
                Layout.alignment: Qt.AlignVCenter
                radius: width / 2
                color: "#FFAA00"
                border.width: 1
                border.color: UISettings.bgLight

                RobotoText
                {
                    anchors.centerIn: parent
                    label: "S"
                    fontSize: parent.height * 0.6
                    fontBold: true
                }
            }

            // Caption (editable in edit mode)
            RobotoText
            {
                visible: !flowConsole.editMode
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                label: sectionData ? sectionData.caption : ""
                fontSize: UISettings.textSizeDefault
                fontBold: true
            }

            CustomTextEdit
            {
                visible: flowConsole.editMode
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                text: sectionData ? sectionData.caption : ""
                onEditingFinished:
                {
                    if (sectionData)
                        flowConsole.setSectionCaption(sectionData.id, text)
                }
            }

            // Columns control
            Row
            {
                visible: flowConsole.editMode
                spacing: 2
                Layout.alignment: Qt.AlignVCenter

                IconButton
                {
                    width: UISettings.iconSizeMedium; height: width
                    bgColor: UISettings.bgLight
                    faSource: FontAwesome.fa_minus; faColor: UISettings.fgMain
                    onClicked: { if (sectionData) flowConsole.setSectionColumns(sectionData.id, sectionData.columns - 1) }
                }
                RobotoText
                {
                    label: sectionData ? sectionData.columns + " col" : ""
                    fontSize: UISettings.textSizeDefault * 0.85
                }
                IconButton
                {
                    width: UISettings.iconSizeMedium; height: width
                    bgColor: UISettings.bgLight
                    faSource: FontAwesome.fa_plus; faColor: UISettings.fgMain
                    onClicked: { if (sectionData) flowConsole.setSectionColumns(sectionData.id, sectionData.columns + 1) }
                }
            }

            // Column count badge (non-edit)
            Rectangle
            {
                visible: !flowConsole.editMode
                implicitWidth: colText.implicitWidth + 10
                implicitHeight: UISettings.iconSizeMedium
                Layout.alignment: Qt.AlignVCenter
                radius: 3; color: UISettings.bgLight

                RobotoText
                {
                    id: colText
                    anchors.centerIn: parent
                    label: sectionData ? sectionData.columns + " col" : ""
                    fontSize: UISettings.textSizeDefault * 0.85
                }
            }

            // Size preset selector (edit mode)
            Row
            {
                visible: flowConsole.editMode
                spacing: 2
                Layout.alignment: Qt.AlignVCenter

                Repeater
                {
                    model: ["full", "half", "third", "quarter"]

                    GenericButton
                    {
                        implicitHeight: UISettings.iconSizeMedium
                        label: modelData
                        fontSize: UISettings.textSizeDefault * 0.75
                        bgColor: sectionData && sectionData.sizePreset === modelData ? UISettings.highlight : UISettings.bgLight
                        onClicked: { if (sectionData) flowConsole.setSectionSizePreset(sectionData.id, modelData) }
                    }
                }
            }

            // Size preset badge (non-edit)
            Rectangle
            {
                visible: !flowConsole.editMode
                implicitWidth: presetText.implicitWidth + 10
                implicitHeight: UISettings.iconSizeMedium
                Layout.alignment: Qt.AlignVCenter
                radius: 3; color: UISettings.bgLight

                RobotoText
                {
                    id: presetText
                    anchors.centerIn: parent
                    label: sectionData ? sectionData.sizePreset : ""
                    fontSize: UISettings.textSizeDefault * 0.85
                }
            }

            // Edit-mode: delete button
            IconButton
            {
                visible: flowConsole.editMode
                implicitWidth: UISettings.iconSizeMedium
                implicitHeight: UISettings.iconSizeMedium
                Layout.alignment: Qt.AlignVCenter
                bgColor: "transparent"
                faSource: FontAwesome.fa_trash_can
                faColor: "red"
                onClicked:
                {
                    if (sectionData)
                        flowConsole.removeSection(sectionData.id)
                }
            }
        }
    }

    // Content area — GridLayout for widgets
    GridLayout
    {
        id: contentArea
        visible: sectionData ? !sectionData.isCollapsed : true
        y: headerRow.height
        width: parent.width - 8
        x: 4
        columns: sectionData ? sectionData.columns : 4
        columnSpacing: 4
        rowSpacing: 4

        // Placeholder when empty
        Rectangle
        {
            visible: !sectionData || sectionData.widgetCount === 0
            Layout.columnSpan: sectionData ? sectionData.columns : 4
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            color: "transparent"
            border.width: 1
            border.color: UISettings.bgLight
            border.pixelAligned: true
            radius: 3

            RobotoText
            {
                anchors.centerIn: parent
                label: flowConsole.editMode ? qsTr("Drop widgets here") : qsTr("Empty section")
                fontSize: UISettings.textSizeDefault * 0.9
                labelColor: UISettings.fgMedium
            }
        }

        // Widget repeater
        Repeater
        {
            model: sectionData ? flowConsole.widgetsForSection(sectionData.id) : []

            Loader
            {
                id: widgetLoader
                Layout.columnSpan: Math.min(modelData.colSpan, sectionData ? sectionData.columns : 1)
                Layout.fillWidth: true
                Layout.preferredHeight: flowWidgetHeight(modelData.type)

                function flowWidgetHeight(wType)
                {
                    // VCWidget::WidgetType enum: 0=Unknown, 1=Button, 2=Slider,
                    // 3=XYPad, 4=Frame, 5=SoloFrame, 6=Speed, 7=CueList,
                    // 8=Label, 9=AudioTriggers, 10=Animation, 11=Clock
                    switch (wType)
                    {
                        case 1: return 60    // ButtonWidget
                        case 2: return 200   // SliderWidget
                        case 3: return 250   // XYPadWidget
                        case 6: return 250   // SpeedWidget
                        case 7: return 300   // CueListWidget
                        case 8: return 40    // LabelWidget
                        case 9: return 150   // AudioTriggersWidget
                        case 10: return 150  // AnimationWidget
                        case 11: return 60   // ClockWidget
                        default: return 60
                    }
                }

                source:
                {
                    // VCWidget::WidgetType enum values
                    switch (modelData.type)
                    {
                        case 1: return "qrc:/FlowButtonItem.qml"         // ButtonWidget
                        case 2: return "qrc:/FlowSliderItem.qml"         // SliderWidget
                        case 3: return "qrc:/FlowXYPadItem.qml"          // XYPadWidget
                        case 6: return "qrc:/FlowSpeedDialItem.qml"      // SpeedWidget
                        case 7: return "qrc:/FlowCueListItem.qml"        // CueListWidget
                        case 8: return "qrc:/FlowLabelItem.qml"          // LabelWidget
                        case 9: return "qrc:/FlowAudioTriggersItem.qml"  // AudioTriggersWidget
                        case 10: return "qrc:/FlowAnimationItem.qml"     // AnimationWidget
                        case 11: return "qrc:/FlowClockItem.qml"         // ClockWidget
                        default: return ""
                    }
                }

                onLoaded:
                {
                    var wObj = flowConsole.widget(modelData.id)
                    if (!wObj || !item) return

                    // VCWidget::WidgetType -> QML property name
                    var propMap = {
                        1: "buttonObj", 2: "sliderObj", 3: "xyPadObj",
                        6: "speedObj", 7: "cueListObj", 8: "wObj",
                        9: "audioTriggerObj", 10: "animationObj", 11: "clockObj"
                    }
                    var prop = propMap[modelData.type]
                    if (prop)
                        item[prop] = wObj

                    item.colSpan = modelData.colSpan
                }
            }
        }

        // Edit-mode: add widget buttons
        Flow
        {
            visible: flowConsole.editMode && sectionData
            Layout.columnSpan: sectionData ? sectionData.columns : 1
            Layout.fillWidth: true
            spacing: 4

            GenericButton { label: qsTr("+ Button"); onClicked: flowConsole.addWidget(sectionData.id, "button", 1) }
            GenericButton { label: qsTr("+ Slider"); onClicked: flowConsole.addWidget(sectionData.id, "slider", 1) }
            GenericButton { label: qsTr("+ Label"); onClicked: flowConsole.addWidget(sectionData.id, "label", 1) }
            GenericButton { label: qsTr("+ Clock"); onClicked: flowConsole.addWidget(sectionData.id, "clock", 1) }
            GenericButton { label: qsTr("+ CueList"); onClicked: flowConsole.addWidget(sectionData.id, "cuelist", 2) }
            GenericButton { label: qsTr("+ XY Pad"); onClicked: flowConsole.addWidget(sectionData.id, "xypad", 2) }
            GenericButton { label: qsTr("+ Speed"); onClicked: flowConsole.addWidget(sectionData.id, "speedDial", 2) }
            GenericButton { label: qsTr("+ Matrix"); onClicked: flowConsole.addWidget(sectionData.id, "matrix", 1) }
            GenericButton { label: qsTr("+ Audio"); onClicked: flowConsole.addWidget(sectionData.id, "audioTrigger", 1) }
        }
    }
}
