/*
  Q Light Controller Plus
  ShowDebugPanel.qml

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
import QtQuick.Controls

import "."

// The one shared, read-only debug panel, docked below the current view.
// Opening it starts observation; closing it, hiding it or its window, or
// unloading it clears it.
Rectangle
{
    id: debugPanel
    objectName: "showDebugPanel"
    color: UISettings.bgStrong
    border.color: UISettings.borderColorDark
    visible: false

    // shown again whenever the panel opens, even after something hid it
    Binding on visible { value: showEvents ? showEvents.open : false }

    readonly property int rowHeight: 22
    /** The event the operator selected, by arrival sequence; -1 for none */
    property real selectedSeq: -1
    property bool selectionExpired: false
    property bool followTail: true
    property bool problemsOnly: false
    readonly property var references: showCommandRecorder && showEvents && showEvents.active
                                      ? showCommandRecorder.referencedControls : []
    readonly property int problemCount:
    {
        var n = 0
        for (var i = 0; i < references.length; i++)
            if (references[i].problem)
                n++
        return n
    }
    readonly property var shownReferences:
        problemsOnly ? references.filter(function(row) { return row.problem }) : references

    function selectedRow() { return showEvents ? showEvents.rowOfSeq(selectedSeq) : -1 }

    // the rendered panel owns the observation: hidden or gone, it ends
    function release()
    {
        if (typeof showEvents !== "undefined" && showEvents && showEvents.open)
            showEvents.open = false
    }

    onVisibleChanged:
    {
        selectedSeq = -1
        selectionExpired = false
        followTail = true
        if (!visible)
            release()
    }
    Component.onDestruction: release()

    Connections
    {
        target: showEvents
        function onFocusRequested() { debugPanel.forceActiveFocus() }
    }

    Connections
    {
        target: showEvents
        function onCountsChanged()
        {
            if (debugPanel.selectedSeq >= 0 && debugPanel.selectedRow() < 0)
            {
                debugPanel.selectedSeq = -1
                debugPanel.selectionExpired = true
            }
            if (debugPanel.followTail)
                eventsView.positionViewAtEnd()
        }
    }

    ColumnLayout
    {
        anchors.fill: parent
        anchors.margins: 4
        spacing: 4

        RowLayout
        {
            Layout.fillWidth: true
            spacing: 8

            TabBar
            {
                id: tabBar
                currentIndex: showEvents ? showEvents.tab : 0
                onCurrentIndexChanged: if (showEvents) showEvents.tab = currentIndex
                // Qt offers no press action for a tab: a click is the tab's action
                TabButton
                {
                    objectName: "eventsTab"
                    text: qsTr("Events")
                    width: implicitWidth + 20
                    Accessible.onPressAction: click()
                }
                TabButton
                {
                    objectName: "referencesTab"
                    text: qsTr("Referenced controls")
                    width: implicitWidth + 20
                    Accessible.onPressAction: click()
                }
            }

            Text
            {
                Layout.fillWidth: true
                color: UISettings.fgLight
                font.pixelSize: 12
                elide: Text.ElideRight
                text: qsTr("Events are observed only while this panel is open. Closing or hiding it clears the list.")
            }

            Button
            {
                objectName: "debugClose"
                text: qsTr("Close")
                onClicked: showEvents.open = false
            }
        }

        // Events
        ColumnLayout
        {
            visible: tabBar.currentIndex === 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 2

            RowLayout
            {
                Layout.fillWidth: true
                spacing: 8
                Text
                {
                    objectName: "eventCounts"
                    Accessible.role: Accessible.StaticText
                    Accessible.name: text
                    color: UISettings.fgMain
                    font.pixelSize: 12
                    text: showEvents ? qsTr("%1 events · %2 evicted · %3 dropped")
                                       .arg(showEvents.count).arg(showEvents.evicted).arg(showEvents.dropped) : ""
                }
                Text
                {
                    objectName: "selectionExpired"
                    visible: debugPanel.selectionExpired
                    color: "#e67e22"
                    font.pixelSize: 12
                    text: qsTr("The selected event left the list")
                }
                Item { Layout.fillWidth: true }
                Button
                {
                    objectName: "eventsLive"
                    text: qsTr("Live")
                    checkable: true
                    checked: debugPanel.followTail
                    onClicked:
                    {
                        debugPanel.followTail = true
                        eventsView.positionViewAtEnd()
                    }
                }
            }

            Row
            {
                spacing: 0
                Repeater
                {
                    model: [ [qsTr("#"), 60], [qsTr("Arrival s"), 80], [qsTr("Show s"), 80], [qsTr("Phase"), 80],
                             [qsTr("Origin"), 90], [qsTr("Control"), 150], [qsTr("Action"), 130],
                             [qsTr("Value"), 80], [qsTr("Result"), 90], [qsTr("Reason"), 300] ]
                    Text
                    {
                        width: modelData[1]
                        text: modelData[0]
                        color: UISettings.fgMedium
                        font.pixelSize: 12
                        font.bold: true
                    }
                }
            }

            RowLayout
            {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 6

                ListView
                {
                    id: eventsView
                    objectName: "eventsList"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: showEvents
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar { }
                    // reading older rows pauses following, not observation
                    onMovementStarted: debugPanel.followTail = false

                    delegate: Rectangle
                    {
                        id: eventRow
                        objectName: "eventRow"
                        readonly property real rowSeq: seq
                        readonly property bool rowFailed: failed
                        width: eventsView.width
                        height: debugPanel.rowHeight
                        color: rowSeq === debugPanel.selectedSeq ? UISettings.highlight
                                                                 : index % 2 ? UISettings.bgMedium : UISettings.bgStrong

                        function select()
                        {
                            debugPanel.selectedSeq = eventRow.rowSeq
                            debugPanel.selectionExpired = false
                        }
                        Accessible.role: Accessible.ListItem
                        Accessible.id: "debugEvent-" + String(rowSeq)
                        Accessible.name: [ seq, arrival, showTime, phase, origin, control, action, value, outcome, reason ]
                                         .filter(function(part) { return part !== undefined && String(part) !== "" })
                                         .join(", ")
                        Accessible.selectable: true
                        Accessible.selected: rowSeq === debugPanel.selectedSeq
                        Accessible.onPressAction: select()
                        Row
                        {
                            anchors.verticalCenter: parent.verticalCenter
                            Repeater
                            {
                                model: [ [seq, 60], [arrival, 80], [showTime, 80], [phase, 80], [origin, 90],
                                         [control, 150], [action, 130], [value, 80], [outcome, 90], [reason, 300] ]
                                Text
                                {
                                    width: modelData[1]
                                    text: modelData[0]
                                    elide: Text.ElideRight
                                    color: eventRow.rowFailed && index === 8 ? "#e67e22" : UISettings.fgMain
                                    font.pixelSize: 12
                                }
                            }
                        }
                        MouseArea
                        {
                            anchors.fill: parent
                            onClicked: eventRow.select()
                        }
                    }
                }

                // read-only details of the selected event
                Rectangle
                {
                    Layout.preferredWidth: 300
                    Layout.fillHeight: true
                    color: UISettings.bgStronger
                    visible: debugPanel.selectedSeq >= 0

                    Text
                    {
                        objectName: "eventDetails"
                        anchors.fill: parent
                        anchors.margins: 6
                        color: UISettings.fgMain
                        font.pixelSize: 12
                        wrapMode: Text.WrapAnywhere
                        text:
                        {
                            if (!showEvents || debugPanel.selectedSeq < 0)
                                return ""
                            // re-read when rows move
                            var unused = showEvents.count
                            var d = showEvents.details(debugPanel.selectedRow())
                            var unknown = qsTr("unknown")
                            var lines = []
                            var keys = [ ["seq", qsTr("Event")], ["trace", qsTr("Trace")], ["take", qsTr("Take")],
                                         ["traversal", qsTr("Traversal")], ["showId", qsTr("Show")],
                                         ["commandId", qsTr("Command")], ["functionId", qsTr("Function")],
                                         ["widgetId", qsTr("Widget")], ["controlId", qsTr("Control identity")],
                                         ["before", qsTr("Before")], ["after", qsTr("After")], ["reason", qsTr("Reason")] ]
                            for (var i = 0; i < keys.length; i++)
                            {
                                var v = d[keys[i][0]]
                                lines.push(keys[i][1] + ": " + (v === undefined || v === "" ? unknown : v))
                            }
                            return lines.join("\n")
                        }
                    }
                }
            }
        }

        // Referenced controls
        ColumnLayout
        {
            visible: tabBar.currentIndex === 1
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 2

            RowLayout
            {
                spacing: 8
                Button
                {
                    objectName: "referencesAll"
                    text: qsTr("All (%1)").arg(debugPanel.references.length)
                    checkable: true
                    checked: !debugPanel.problemsOnly
                    onClicked: debugPanel.problemsOnly = false
                }
                Button
                {
                    objectName: "referencesProblems"
                    text: qsTr("Problems (%1)").arg(debugPanel.problemCount)
                    checkable: true
                    checked: debugPanel.problemsOnly
                    onClicked: debugPanel.problemsOnly = true
                }
                Text
                {
                    color: UISettings.fgLight
                    font.pixelSize: 12
                    text: qsTr("Ready means the configuration can be replayed, not that anything runs or reaches hardware.")
                }
            }

            Row
            {
                Repeater
                {
                    model: [ [qsTr("Status"), 130], [qsTr("Control"), 160], [qsTr("Identity"), 280],
                             [qsTr("Current binding"), 180], [qsTr("Refs"), 50], [qsTr("Expected roles"), 600] ]
                    Text
                    {
                        width: modelData[1]
                        text: modelData[0]
                        color: UISettings.fgMedium
                        font.pixelSize: 12
                        font.bold: true
                    }
                }
            }

            ListView
            {
                id: referencesView
                objectName: "referencesList"
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: debugPanel.shownReferences
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { }

                delegate: Rectangle
                {
                    id: referenceRow
                    objectName: "referenceRow"
                    readonly property bool problem: modelData.problem
                    width: referencesView.width
                    height: debugPanel.rowHeight
                    color: index % 2 ? UISettings.bgMedium : UISettings.bgStrong
                    Accessible.role: Accessible.ListItem
                    Accessible.name: [ modelData.statusText, modelData.caption, modelData.controlId, modelData.binding,
                                       modelData.count, modelData.detail ]
                                     .filter(function(part) { return part !== undefined && String(part) !== "" })
                                     .join(", ")
                    Row
                    {
                        anchors.verticalCenter: parent.verticalCenter
                        Repeater
                        {
                            model: [ [modelData.statusText, 130], [modelData.caption, 160], [modelData.controlId, 280],
                                     [modelData.binding, 180], [modelData.count, 50], [modelData.detail, 600] ]
                            Text
                            {
                                width: modelData[1]
                                text: modelData[0]
                                elide: Text.ElideRight
                                color: index === 0 && referenceRow.problem ? "#e67e22" : UISettings.fgMain
                                font.pixelSize: 12
                            }
                        }
                    }
                }
            }
        }
    }
}
