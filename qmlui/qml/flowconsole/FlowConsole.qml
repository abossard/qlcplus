/*
  Q Light Controller Plus
  FlowConsole.qml

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
    id: flowConsoleView
    anchors.fill: parent
    objectName: "flowConsole"
    color: UISettings.bgMedium

    property var sectionsList: flowConsole.sectionsForPage(flowConsole.selectedPage)

    Connections
    {
        target: flowConsole
        function onSectionsChanged() { sectionsList = flowConsole.sectionsForPage(flowConsole.selectedPage) }
        function onSelectedPageChanged() { sectionsList = flowConsole.sectionsForPage(flowConsole.selectedPage) }
    }

    // Toolbar
    Rectangle
    {
        id: fcToolbar
        width: parent.width
        height: UISettings.iconSizeDefault
        z: 10
        gradient: Gradient
        {
            GradientStop { position: 0; color: UISettings.toolbarStartSub }
            GradientStop { position: 1; color: UISettings.toolbarEnd }
        }

        RowLayout
        {
            anchors.fill: parent
            spacing: 4

            // Edit mode toggle
            IconButton
            {
                id: editModeButton
                implicitWidth: UISettings.iconSizeDefault
                implicitHeight: UISettings.iconSizeDefault
                Layout.alignment: Qt.AlignTop
                checkable: true
                checked: flowConsole.editMode
                enabled: qlcplus.runningFunctionsCount === 0 || flowConsole.editMode === false
                bgColor: checked ? UISettings.highlight : "transparent"
                imgSource: "qrc:/edit.svg"
                imgMargins: 4
                tooltip: qlcplus.runningFunctionsCount > 0
                    ? qsTr("Cannot edit while functions are running")
                    : qsTr("Toggle edit mode")
                onToggled: flowConsole.editMode = checked
            }

            // Page tabs
            Repeater
            {
                model: flowConsole.pagesCount
                delegate: GenericButton
                {
                    implicitWidth: UISettings.bigItemHeight * 1.5
                    implicitHeight: UISettings.iconSizeDefault
                    Layout.alignment: Qt.AlignTop
                    label: qsTr("Page %1").arg(index + 1)
                    bgColor: flowConsole.selectedPage === index ? UISettings.highlight : "transparent"
                    onClicked: flowConsole.selectedPage = index
                }
            }

            // Add page button (edit mode only)
            IconButton
            {
                visible: flowConsole.editMode
                implicitWidth: UISettings.iconSizeDefault
                implicitHeight: UISettings.iconSizeDefault
                Layout.alignment: Qt.AlignTop
                bgColor: "transparent"
                faSource: FontAwesome.fa_plus
                faColor: UISettings.fgMain
                tooltip: qsTr("Add page")
                onClicked: flowConsole.addPage("")
            }

            // Spacer
            Rectangle
            {
                Layout.fillWidth: true
                color: "transparent"
            }

            RobotoText
            {
                label: qsTr("Flow Console")
                fontSize: UISettings.textSizeDefault
                Layout.alignment: Qt.AlignVCenter
                Layout.rightMargin: 8
            }
        }
    }

    // Main content area
    Flickable
    {
        id: fcContent
        width: parent.width
        height: parent.height - fcToolbar.height
        y: fcToolbar.height
        contentHeight: sectionsFlow.implicitHeight + addSectionArea.height + 40
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: CustomScrollBar { }

        // Sections arranged in a Flow (half-width sections sit side by side)
        Flow
        {
            id: sectionsFlow
            width: parent.width - 20
            x: 10
            y: 10
            spacing: 10

            Repeater
            {
                model: sectionsList

                FlowSectionItem
                {
                    sectionData: modelData
                    parentFlowWidth: sectionsFlow.width
                }
            }
        }

        // Add section button (edit mode only)
        Rectangle
        {
            id: addSectionArea
            visible: flowConsole.editMode
            y: sectionsFlow.y + sectionsFlow.implicitHeight + 10
            x: 10
            width: sectionsFlow.width
            height: visible ? 50 : 0
            color: "transparent"
            border.width: 2
            border.color: UISettings.bgLight
            radius: 4

            Row
            {
                anchors.centerIn: parent
                spacing: 8

                GenericButton
                {
                    label: qsTr("+ Full Section")
                    onClicked: flowConsole.addSection(flowConsole.selectedPage, qsTr("New Section"), "full", 4, false)
                }
                GenericButton
                {
                    label: qsTr("+ Half Section")
                    onClicked: flowConsole.addSection(flowConsole.selectedPage, qsTr("New Section"), "half", 3, false)
                }
                GenericButton
                {
                    label: qsTr("+ Third Section")
                    onClicked: flowConsole.addSection(flowConsole.selectedPage, qsTr("New Section"), "third", 2, false)
                }
                GenericButton
                {
                    label: qsTr("+ Solo Section")
                    onClicked: flowConsole.addSection(flowConsole.selectedPage, qsTr("Solo Section"), "full", 4, true)
                }
            }
        }
    }
}
