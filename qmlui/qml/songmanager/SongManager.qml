/*
  Q Light Controller Plus
  SongManager.qml

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

Rectangle
{
    id: songMgrContainer
    anchors.fill: parent
    color: "transparent"

    property string contextName: "SONGMGR"

    // Status derived purely from vdjBridge — no extra C++ state.
    function statusColor()
    {
        if (!vdjBridge)
            return "#666"
        if (vdjBridge.telemetryConnected)
            return "#3a3"
        if (vdjBridge.telemetryStatus === "Listening")
            return "#aa3"
        return "#a33"
    }

    function fmtDuration(ms)
    {
        if (!ms || ms <= 0)
            return "--:--"
        var s = Math.floor(ms / 1000)
        var m = Math.floor(s / 60)
        s = s % 60
        return (m < 10 ? "0" : "") + m + ":" + (s < 10 ? "0" : "") + s
    }

    function fmtTime(ms)
    {
        var totalSec = Math.floor(ms / 1000)
        var min = Math.floor(totalSec / 60)
        var sec = totalSec % 60
        return min + ":" + (sec < 10 ? "0" : "") + sec
    }

    property var masterDeck: vdjBridge && vdjBridge.decks.length > vdjBridge.masterDeck
                             ? vdjBridge.decks[vdjBridge.masterDeck] : null

    ColumnLayout
    {
        anchors.fill: parent
        spacing: 0

        // ---------- Status bar ----------
        Rectangle
        {
            id: statusBar
            Layout.fillWidth: true
            Layout.preferredHeight: UISettings.iconSizeDefault * 1.6
            color: UISettings.bgLight

            RowLayout
            {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 14

                // Connection indicator
                Rectangle
                {
                    Layout.preferredWidth: 14
                    Layout.preferredHeight: 14
                    radius: 7
                    color: statusColor()
                }

                RobotoText
                {
                    label: vdjBridge
                            ? (qsTr("VDJ: ") + vdjBridge.telemetryStatus)
                            : qsTr("VDJ: n/a")
                    fontSize: UISettings.textSizeDefault
                }

                Rectangle { Layout.preferredWidth: 1; Layout.fillHeight: true; color: UISettings.bgMedium }

                RobotoText
                {
                    label: qsTr("Master: ")
                            + (vdjBridge && vdjBridge.masterDeck >= 0
                               ? ("Deck " + (vdjBridge.masterDeck + 1))
                               : "—")
                    fontSize: UISettings.textSizeDefault
                }

                Rectangle { Layout.preferredWidth: 1; Layout.fillHeight: true; color: UISettings.bgMedium }

                // Per-deck states (loaded/playing)
                Row
                {
                    spacing: 10

                    Repeater
                    {
                        model: vdjBridge ? vdjBridge.decks : []

                        Rectangle
                        {
                            width: 80
                            height: statusBar.height - 10
                            radius: 3
                            color: modelData && modelData.playing
                                    ? "#284"
                                    : (modelData && modelData.loaded ? "#444" : "transparent")
                            border.color: UISettings.bgStrong
                            border.width: 1

                            RobotoText
                            {
                                anchors.centerIn: parent
                                label: "D" + (index + 1) + (modelData && modelData.loaded
                                        ? (modelData.playing ? " ▶" : " ■")
                                        : " ·")
                                fontSize: UISettings.textSizeDefault
                            }
                        }
                    }
                }

                Rectangle { Layout.preferredWidth: 1; Layout.fillHeight: true; color: UISettings.bgMedium }

                // Master deck artist — title
                RobotoText
                {
                    visible: masterDeck !== null && masterDeck.title !== ""
                    label: masterDeck
                           ? (masterDeck.artist !== ""
                              ? masterDeck.artist + " — " + masterDeck.title
                              : masterDeck.title)
                           : ""
                    fontSize: UISettings.textSizeDefault
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                    Layout.maximumWidth: 300
                }

                // Live BPM
                RobotoText
                {
                    visible: masterDeck !== null && masterDeck.bpm > 0
                    label: masterDeck ? masterDeck.bpm.toFixed(1) + " BPM" : ""
                    fontSize: UISettings.textSizeDefault
                    labelColor: "#f39c12"
                }

                // Play position mm:ss / mm:ss
                RobotoText
                {
                    visible: masterDeck !== null && masterDeck.loaded
                    label: masterDeck
                           ? fmtTime(masterDeck.timeElapsed) + " / " + fmtTime(masterDeck.timeTotal)
                           : ""
                    fontSize: UISettings.textSizeDefault
                    labelColor: "#aaa"
                }
            }
        }

        // ---------- Song list ----------
        Rectangle
        {
            Layout.fillWidth: true
            Layout.preferredHeight: parent.height * 0.4
            color: UISettings.bgMedium

            // Header
            Rectangle
            {
                id: songsHeader
                anchors.top: parent.top
                width: parent.width
                height: UISettings.iconSizeDefault
                color: UISettings.bgStrong

                RowLayout
                {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8

                    RobotoText { label: qsTr("Title"); Layout.fillWidth: true; fontSize: UISettings.textSizeDefault }
                    RobotoText { label: qsTr("Duration"); Layout.preferredWidth: 80; fontSize: UISettings.textSizeDefault }
                    RobotoText { label: qsTr("Show ID"); Layout.preferredWidth: 80; fontSize: UISettings.textSizeDefault }
                }
            }

            ListView
            {
                id: songList
                anchors.top: songsHeader.bottom
                anchors.bottom: parent.bottom
                width: parent.width
                clip: true
                model: songManager ? songManager.songListModel : null

                ScrollBar.vertical: ScrollBar { }

                delegate: Rectangle
                {
                    width: songList.width
                    height: UISettings.listItemHeight
                    color: ListView.isCurrentItem
                           ? UISettings.highlight
                           : ((index % 2 === 0) ? UISettings.bgMedium : UISettings.bgLight)

                    MouseArea
                    {
                        anchors.fill: parent
                        onClicked: songList.currentIndex = index
                    }

                    RowLayout
                    {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8

                        RobotoText
                        {
                            Layout.fillWidth: true
                            label: title || filepath
                            fontSize: UISettings.textSizeDefault
                            elide: Text.ElideRight
                        }
                        RobotoText
                        {
                            Layout.preferredWidth: 80
                            label: fmtDuration(duration)
                            fontSize: UISettings.textSizeDefault
                        }
                        RobotoText
                        {
                            Layout.preferredWidth: 80
                            label: "" + showId
                            fontSize: UISettings.textSizeDefault
                        }
                    }
                }

                // Empty-state hint
                RobotoText
                {
                    anchors.centerIn: parent
                    visible: songList.count === 0
                    label: qsTr("No songs loaded yet — play a track in VirtualDJ")
                    fontSize: UISettings.textSizeDefault
                }
            }
        }
    }
}
