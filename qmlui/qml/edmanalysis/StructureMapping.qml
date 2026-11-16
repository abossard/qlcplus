/*
  Q Light Controller Plus
  StructureMapping.qml

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

/** The default Function of each section label: it pre-fills Song Director blocks */
Rectangle
{
    id: mappingPanel
    objectName: "structureMappingPanel"

    property bool expanded: false
    property var buttons: []
    property int mappingRevision: 0   // edmMapping.functionId() has no notify: the rows read it again on this

    // another workspace, a cleared one or an edit: the button list and every row follow
    Connections
    {
        target: edmMapping
        function onChanged() { mappingPanel.loadButtons(); mappingPanel.mappingRevision++ }
    }

    readonly property var labelColors: ({ "intro": "#5b6b82", "buildup": "#e0a030", "drop": "#e0408a",
                                          "breakdown": "#3d7fe0", "groove": "#3fae6a", "outro": "#6b6b78" })

    // the Functions that can be a look, by name
    function loadButtons() {
        buttons = [{ "id": -1, "text": qsTr("none") }].concat(edmMapping.lookFunctions())
    }

    function indexOfWidget(id) {
        for (var i = 0; i < buttons.length; i++)
            if (buttons[i].id === id)
                return i
        return id < 0 ? 0 : -1
    }

    Layout.fillWidth: true
    implicitHeight: header.height + (expanded ? rows.height + 8 : 0)
    color: "#2e2e2e"
    border.color: "#444"

    RowLayout
    {
        id: header
        width: parent.width
        height: 36
        spacing: 8

        Text
        {
            Layout.leftMargin: 8
            text: (mappingPanel.expanded ? "▾ " : "▸ ") + qsTr("Song looks")
            color: "#ddd"
            font.pixelSize: 12
            font.bold: true
            MouseArea
            {
                anchors.fill: parent
                onClicked: { mappingPanel.expanded = !mappingPanel.expanded; if (mappingPanel.expanded) mappingPanel.loadButtons() }
            }
        }
        Item { Layout.fillWidth: true }
    }

    GridLayout
    {
        id: rows
        visible: mappingPanel.expanded
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 8
        columns: 2
        columnSpacing: 12
        rowSpacing: 4

        Repeater
        {
            model: edmMapping.labels()
            delegate: RowLayout
            {
                Layout.columnSpan: 2
                Layout.fillWidth: true
                spacing: 12
                Rectangle
                {
                    Layout.preferredWidth: 90
                    Layout.preferredHeight: 22
                    radius: 3
                    color: mappingPanel.labelColors[modelData]
                    Text { anchors.centerIn: parent; text: modelData; color: "white"; font.pixelSize: 12; font.bold: true }
                }
                ComboBox
                {
                    Layout.preferredWidth: 280
                    Layout.preferredHeight: 26
                    model: mappingPanel.buttons
                    textRole: "text"
                    currentIndex: mappingPanel.mappingRevision, mappingPanel.indexOfWidget(edmMapping.functionId(modelData))
                    displayText: currentIndex < 0 ? qsTr("missing function #%1").arg(edmMapping.functionId(modelData)) : currentText
                    onActivated: edmMapping.setFunctionId(modelData, mappingPanel.buttons[index].id)
                }
            }
        }
    }
}
