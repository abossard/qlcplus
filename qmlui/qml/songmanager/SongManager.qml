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

    function fmtDuration(ms) {
        if (!ms || ms <= 0) return "--:--"
        var s = Math.floor(ms / 1000)
        var m = Math.floor(s / 60)
        s = s % 60
        return (m < 10 ? "0" : "") + m + ":" + (s < 10 ? "0" : "") + s
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ── Status bar ──
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            color: "#333"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                Text {
                    text: vdjBridge
                          ? "VDJ: " + vdjBridge.telemetryStatus
                            + (vdjBridge.telemetryConnected ? " ●" : "")
                          : "VDJ: n/a"
                    color: vdjBridge && vdjBridge.telemetryConnected ? "#2ecc71" : "#aaa"
                    font.pixelSize: 14
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: songManager ? songManager.songCount + " songs" : ""
                    color: "#aaa"
                    font.pixelSize: 12
                }
            }
        }

        // ── Search + Sort controls ──
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            color: "#3a3a3a"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 8

                // Search field
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 28
                    radius: 4
                    color: "#555"
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 6
                        anchors.rightMargin: 6
                        spacing: 4
                        Text {
                            text: "🔍"
                            color: "#aaa"
                            font.pixelSize: 13
                            Layout.alignment: Qt.AlignVCenter
                        }
                        TextInput {
                            id: searchInput
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignVCenter
                            color: "#eee"
                            font.pixelSize: 13
                            clip: true
                            onTextChanged: {
                                if (songManager)
                                    songManager.searchFilter = text
                            }
                            Text {
                                anchors.fill: parent
                                visible: !searchInput.text && !searchInput.activeFocus
                                text: "Search songs…"
                                color: "#888"
                                font.pixelSize: 13
                            }
                        }
                        Text {
                            visible: searchInput.text.length > 0
                            text: "✕"
                            color: "#aaa"
                            font.pixelSize: 14
                            Layout.alignment: Qt.AlignVCenter
                            MouseArea {
                                anchors.fill: parent
                                onClicked: searchInput.text = ""
                            }
                        }
                    }
                }

                // Sort mode combo
                ComboBox {
                    id: sortCombo
                    Layout.preferredWidth: 150
                    Layout.preferredHeight: 28
                    model: ["Alphabetical", "Recently Played", "Recently Edited"]
                    currentIndex: songManager ? songManager.sortMode : 0
                    onCurrentIndexChanged: {
                        if (songManager)
                            songManager.sortMode = currentIndex
                    }
                    background: Rectangle {
                        color: "#555"
                        radius: 4
                    }
                    contentItem: Text {
                        text: sortCombo.displayText
                        color: "#eee"
                        font.pixelSize: 12
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 6
                    }
                }

                // Sort direction toggle
                Rectangle {
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: 28
                    radius: 4
                    color: sortDirMouse.containsMouse ? "#666" : "#555"
                    Text {
                        anchors.centerIn: parent
                        text: songManager && songManager.sortAscending ? "▲" : "▼"
                        color: "#eee"
                        font.pixelSize: 14
                    }
                    MouseArea {
                        id: sortDirMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            if (songManager)
                                songManager.sortAscending = !songManager.sortAscending
                        }
                    }
                }
            }
        }

        // ── Column headers ──
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            color: "#444"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 4
                // Playing indicator column
                Text { text: ""; Layout.preferredWidth: 20 }
                Text { text: "Title"; color: "#ccc"; Layout.fillWidth: true; font.pixelSize: 12; font.bold: true }
                Text { text: "Duration"; color: "#ccc"; Layout.preferredWidth: 70; font.pixelSize: 12; font.bold: true }
                Text { text: "ID"; color: "#ccc"; Layout.preferredWidth: 50; font.pixelSize: 12; font.bold: true }
            }
        }

        // ── Song list ──
        ListView {
            id: songList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: songManager ? songManager.songListModel : null
            delegate: Rectangle {
                width: songList.width
                height: 40
                color: model.isPlaying ? "#2a4a2a"
                     : ListView.isCurrentItem ? "#3366aa"
                     : ((index % 2 === 0) ? "#2a2a2a" : "#333")
                border.color: model.isPlaying ? "#2ecc71" : "transparent"
                border.width: model.isPlaying ? 1 : 0

                MouseArea {
                    anchors.fill: parent
                    onClicked: songList.currentIndex = index
                }
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 4

                    // Playing indicator
                    Text {
                        Layout.preferredWidth: 20
                        text: model.isPlaying ? "▶" : ""
                        color: "#2ecc71"
                        font.pixelSize: 14
                    }

                    // Title (main) + filepath (subtitle)
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        Text {
                            Layout.fillWidth: true
                            text: model.title || "?"
                            color: model.isPlaying ? "#2ecc71" : "#eee"
                            font.pixelSize: 13
                            font.bold: model.isPlaying
                            elide: Text.ElideRight
                        }
                        Text {
                            Layout.fillWidth: true
                            visible: text.length > 0
                            text: {
                                var fp = model.filepath || ""
                                var parts = fp.split("/")
                                return parts.length > 1 ? parts[parts.length - 1] : fp
                            }
                            color: "#888"
                            font.pixelSize: 10
                            elide: Text.ElideLeft
                        }
                    }

                    // Duration
                    Text {
                        Layout.preferredWidth: 70
                        text: fmtDuration(model.duration)
                        color: "#aaa"
                        font.pixelSize: 13
                        horizontalAlignment: Text.AlignRight
                    }

                    // Show ID
                    Text {
                        Layout.preferredWidth: 50
                        text: "" + model.showId
                        color: "#666"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignRight
                    }
                }
            }

            // Empty state
            Text {
                anchors.centerIn: parent
                visible: songList.count === 0
                text: searchInput.text.length > 0
                      ? "No songs match \"" + searchInput.text + "\""
                      : "No songs yet — play a track in VDJ"
                color: "#888"
                font.pixelSize: 14
            }
        }
    }
}
