/*
  Q Light Controller Plus
  RecordControl.qml

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

// REC for the one command recorder, the same in every performance view.
// It arms capture only: it never plays, pauses or seeks. While a take is
// bound, the Show Manager shows that Show, so its transport is the take's.
RowLayout
{
    id: recordControl
    objectName: "recordControl"
    spacing: 8

    readonly property bool recording: showCommandRecorder ? showCommandRecorder.recording : false
    readonly property string transportName: !showManager.isPlaying ? qsTr("Stopped")
                                            : showManager.pausing ? qsTr("Pausing")
                                            : showManager.isPaused ? qsTr("Paused")
                                            : qsTr("Playing")
    readonly property string statusText:
    {
        if (!recording)
            return ""
        var target = showCommandRecorder.targetName
        if (target.length === 0)
            return qsTr("REC: %1 - waiting for a show").arg(showCommandRecorder.phaseName)
        return qsTr("REC: %1 - %2 - %3").arg(showCommandRecorder.phaseName).arg(target).arg(transportName)
    }

    Switch
    {
        id: recordSwitch
        objectName: "recordSwitch"
        text: qsTr("Record")
        checked: recordControl.recording
        focusPolicy: Qt.StrongFocus
        Accessible.name: qsTr("Record commands")
        // focused, Space toggles REC instead of a view's Space shortcut (Play)
        Keys.onShortcutOverride: (event) => event.accepted = (event.key === Qt.Key_Space)

        onToggled:
        {
            if (showCommandRecorder)
                showCommandRecorder.recording = checked
            // the recorder decides: a refused toggle shows its state again
            checked = Qt.binding(function() { return recordControl.recording })
        }

        contentItem: Text
        {
            text: recordSwitch.text
            color: recordSwitch.checked ? "#e74c3c" : UISettings.fgMain
            font.pixelSize: 13
            font.bold: recordSwitch.checked
            verticalAlignment: Text.AlignVCenter
            leftPadding: recordSwitch.indicator.width + 6
        }
    }

    Text
    {
        objectName: "recordStatus"
        Accessible.role: Accessible.StaticText
        Accessible.name: text
        Accessible.id: "recordStatus"
        visible: recordControl.recording
        text: recordControl.statusText
        color: showCommandRecorder && showCommandRecorder.phase === 2 ? "#e74c3c" : "#f1c40f"
        font.pixelSize: 13
        font.bold: true
        elide: Text.ElideRight
        Layout.maximumWidth: 320
    }

    Button
    {
        objectName: "recordDebugButton"
        text: qsTr("Debug")
        focusPolicy: Qt.StrongFocus
        Accessible.name: qsTr("Open the debug panel")
        onClicked: if (showEvents) showEvents.showTab(showEvents.tab)
    }

    // failed operations while the panel is closed: a count and the latest reason
    Button
    {
        objectName: "problemsIndicator"
        visible: showEvents ? !showEvents.open && showEvents.problemCount > 0 : false
        text: showEvents ? qsTr("Problems: %1").arg(showEvents.problemCount) : ""
        ToolTip.visible: hovered
        ToolTip.text: showEvents ? showEvents.problemReason : ""
        onClicked: showEvents.showProblems()
        contentItem: Text
        {
            text: parent.text
            color: "#e67e22"
            font.pixelSize: 12
            font.bold: true
            verticalAlignment: Text.AlignVCenter
        }
    }

    Text
    {
        objectName: "problemsReason"
        Accessible.role: Accessible.StaticText
        Accessible.name: text
        visible: showEvents ? !showEvents.open && showEvents.problemCount > 0 : false
        text: showEvents ? showEvents.problemReason : ""
        color: "#e67e22"
        font.pixelSize: 12
        elide: Text.ElideRight
        Layout.maximumWidth: 260
    }

    Text
    {
        objectName: "recordError"
        Accessible.role: Accessible.StaticText
        Accessible.name: text
        visible: text.length > 0
        text: showCommandRecorder ? showCommandRecorder.lastError : ""
        color: "#e67e22"
        font.pixelSize: 12
        elide: Text.ElideRight
        Layout.maximumWidth: 260
    }
}
