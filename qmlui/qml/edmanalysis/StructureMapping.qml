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

/** Which Virtual Console button each section label presses, and the AUTO switch */
Rectangle
{
    id: mappingPanel
    objectName: "structureMappingPanel"

    property bool expanded: false
    property var buttons: []

    readonly property var labelColors: ({ "intro": "#5b6b82", "buildup": "#e0a030", "drop": "#e0408a",
                                          "breakdown": "#3d7fe0", "groove": "#3fae6a", "outro": "#6b6b78" })
    readonly property string status: {
        if (!edmMapping.autoEnabled)
            return qsTr("off")
        if (edmDirector.overridden)
            return qsTr("yours until the next section")
        if (edmDirector.followedDeck > 0)
            return qsTr("following deck %1: %2").arg(edmDirector.followedDeck).arg(edmDirector.followedLabel.toUpperCase())
        return qsTr("waiting for a song with a structure")
    }

    // the buttons by caption; page and number when a caption repeats
    function loadButtons() {
        var list = virtualConsole.widgetsList([VCWidget.ButtonWidget]), seen = {}, out = [{ "id": -1, "text": qsTr("none") }]
        for (var i = 0; i < list.length; i++)
            if (list[i].id !== undefined)
                seen[list[i].label] = (seen[list[i].label] || 0) + 1
        for (var j = 0; j < list.length; j++)
        {
            if (list[j].id === undefined)
                continue
            var l = list[j]
            out.push({ "id": l.id, "text": seen[l.label] > 1 ? l.label + "  (page " + (l.classRef.page + 1) + ", #" + l.id + ")" : l.label })
        }
        buttons = out
    }

    function indexOfWidget(id) {
        for (var i = 0; i < buttons.length; i++)
            if (buttons[i].id === id)
                return i
        return 0
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
        Switch
        {
            id: autoSwitch
            objectName: "structureAutoSwitch"
            text: qsTr("AUTO")
            checked: edmMapping.autoEnabled
            onToggled: edmMapping.autoEnabled = checked
            contentItem: Text
            {
                text: autoSwitch.text
                color: autoSwitch.checked ? "#2ecc71" : "#ccc"
                font.pixelSize: 12
                font.bold: autoSwitch.checked
                verticalAlignment: Text.AlignVCenter
                leftPadding: autoSwitch.indicator.width + 6
            }
        }
        Text
        {
            objectName: "structureStatus"
            Layout.fillWidth: true
            text: mappingPanel.status
            color: edmDirector.overridden ? "#e67e22" : "#aaa"
            font.pixelSize: 12
            elide: Text.ElideRight
        }
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
                    currentIndex: mappingPanel.indexOfWidget(edmMapping.widgetId(modelData))
                    onActivated: edmMapping.setWidgetId(modelData, mappingPanel.buttons[index].id)
                }
            }
        }
    }
}
