import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import org.qlcplus.classes 1.0

Rectangle
{
    id: djMgrContainer
    anchors.fill: parent
    color: "transparent"
    property string contextName: "DJMGR"

    function fmtDuration(ms) {
        if (!ms || ms <= 0) return "--:--"
        var s = Math.floor(ms / 1000)
        var m = Math.floor(s / 60)
        s = s % 60
        return (m < 10 ? "0" : "") + m + ":" + (s < 10 ? "0" : "") + s
    }

    // Fixed-length clock: always "HH:MM:SS" (8 chars) so columns don't jitter.
    function fmtClock(ms) {
        var total = (!ms || ms <= 0) ? 0 : Math.floor(ms / 1000)
        var h = Math.floor(total / 3600)
        var m = Math.floor((total % 3600) / 60)
        var s = total % 60
        function pad(n) { return (n < 10 ? "0" : "") + n }
        return pad(h) + ":" + pad(m) + ":" + pad(s)
    }

    function fileName(fp) {
        if (!fp) return ""
        var parts = ("" + fp).split("/")
        return parts.length > 1 ? parts[parts.length - 1] : fp
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ── Status + Perform bar ──
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            color: "#333"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 12

                Text {
                    text: vdjBridge
                          ? "VDJ: " + vdjBridge.telemetryStatus
                            + (vdjBridge.telemetryConnected ? " ●" : "")
                          : "VDJ: n/a"
                    color: vdjBridge && vdjBridge.telemetryConnected ? "#2ecc71" : "#aaa"
                    font.pixelSize: 14
                }

                Item { Layout.fillWidth: true }

                // Active deck/song
                Text {
                    visible: djManager && djManager.activeDeck > 0
                    text: djManager
                          ? "Active: Deck " + djManager.activeDeck
                            + (djManager.activeTitle ? " — " + djManager.activeTitle : "")
                            + (djManager.activeArtist ? " (" + djManager.activeArtist + ")" : "")
                          : ""
                    color: "#f1c40f"
                    font.pixelSize: 13
                    elide: Text.ElideRight
                    Layout.maximumWidth: 320
                }

                // Active play position (time + beat)
                Text {
                    visible: djManager && djManager.activeDeck > 0
                    text: djManager
                          ? "⏱ " + fmtClock(djManager.activeElapsedMs)
                            + "  ♪ " + djManager.activeBeatPos.toFixed(1)
                          : ""
                    color: "#2ecc71"
                    font.pixelSize: 13
                }

                // Perform toggle
                Switch {
                    id: performSwitch
                    text: "Perform"
                    checked: djManager ? djManager.performMode : false
                    onToggled: {
                        if (djManager)
                            djManager.performMode = checked
                    }
                    contentItem: Text {
                        text: performSwitch.text
                        color: performSwitch.checked ? "#2ecc71" : "#ccc"
                        font.pixelSize: 13
                        font.bold: performSwitch.checked
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: performSwitch.indicator.width + 6
                    }
                }

                // Deck count selector (default 2; parameter for the future)
                Text { text: "Decks"; color: "#aaa"; font.pixelSize: 12 }
                ComboBox {
                    id: deckCountCombo
                    Layout.preferredWidth: 56
                    Layout.preferredHeight: 28
                    model: [1, 2, 3, 4]
                    currentIndex: djManager ? djManager.deckCount - 1 : 1
                    onActivated: {
                        if (djManager)
                            djManager.deckCount = index + 1
                    }
                    background: Rectangle { color: "#555"; radius: 4 }
                    contentItem: Text {
                        text: deckCountCombo.displayText
                        color: "#eee"
                        font.pixelSize: 12
                        verticalAlignment: Text.AlignVCenter
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }
        }

        // ── FSM deck table (4 decks) ──
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 26
            color: "#444"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 6
                Text { text: "Deck"; color: "#ccc"; Layout.preferredWidth: 44; font.pixelSize: 11; font.bold: true }
                Text { text: "State"; color: "#ccc"; Layout.preferredWidth: 70; font.pixelSize: 11; font.bold: true }
                Text { text: "Song"; color: "#ccc"; Layout.fillWidth: true; font.pixelSize: 11; font.bold: true }
                Text { text: "BPM"; color: "#ccc"; Layout.preferredWidth: 56; font.pixelSize: 11; font.bold: true }
                Text { text: "Pos"; color: "#ccc"; Layout.preferredWidth: 116; font.pixelSize: 11; font.bold: true }
                Text { text: "Vol"; color: "#ccc"; Layout.preferredWidth: 44; font.pixelSize: 11; font.bold: true }
            }
        }

        ListView {
            id: deckTable
            Layout.fillWidth: true
            Layout.preferredHeight: (djManager ? djManager.deckCount : 2) * 30
            interactive: false
            model: djManager ? djManager.deckModel : null
            delegate: Rectangle {
                width: deckTable.width
                height: 30
                color: model.isActive ? "#3a3a1a"
                     : ((model.deckNumber % 2 === 0) ? "#2a2a2a" : "#303030")
                border.color: model.isActive ? "#f1c40f" : "transparent"
                border.width: model.isActive ? 1 : 0

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 6

                    Text {
                        Layout.preferredWidth: 44
                        text: (model.isPlaying ? "▶ " : "") + model.deckNumber
                        color: model.isActive ? "#f1c40f" : "#ddd"
                        font.pixelSize: 12
                        font.bold: model.isActive
                    }
                    Text {
                        Layout.preferredWidth: 70
                        text: model.state
                        color: {
                            switch (model.state) {
                            case "Playing": return "#2ecc71"
                            case "Paused":  return "#e67e22"
                            case "Loaded":  return "#3498db"
                            case "Loading": return "#f1c40f"
                            default:        return "#888"
                            }
                        }
                        font.pixelSize: 12
                    }
                    Text {
                        Layout.fillWidth: true
                        text: model.title
                              ? (model.title + (model.artist ? " — " + model.artist : ""))
                              : "—"
                        color: "#eee"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.preferredWidth: 56
                        text: model.bpm > 0 ? model.bpm.toFixed(1) : ""
                        color: "#aaa"
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignRight
                    }
                    Text {
                        Layout.preferredWidth: 116
                        text: model.filepath
                              ? fmtClock(model.elapsedMs) + " ♪" + model.beatPos.toFixed(1)
                              : ""
                        color: "#888"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignRight
                    }
                    Text {
                        Layout.preferredWidth: 44
                        text: Math.round(model.volume * 100) + "%"
                        color: "#888"
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignRight
                    }
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
                        Text { text: "🔍"; color: "#aaa"; font.pixelSize: 13; Layout.alignment: Qt.AlignVCenter }
                        TextInput {
                            id: searchInput
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignVCenter
                            color: "#eee"
                            font.pixelSize: 13
                            clip: true
                            onTextChanged: {
                                if (djManager)
                                    djManager.searchFilter = text
                            }
                            Text {
                                anchors.fill: parent
                                visible: !searchInput.text && !searchInput.activeFocus
                                text: "Search songs…"
                                color: "#888"
                                font.pixelSize: 13
                            }
                        }
                    }
                }

                ComboBox {
                    id: sortCombo
                    Layout.preferredWidth: 150
                    Layout.preferredHeight: 28
                    model: ["Alphabetical", "Recently Played", "Recently Edited"]
                    currentIndex: djManager ? djManager.sortMode : 0
                    onCurrentIndexChanged: {
                        if (djManager)
                            djManager.sortMode = currentIndex
                    }
                    background: Rectangle { color: "#555"; radius: 4 }
                    contentItem: Text {
                        text: sortCombo.displayText
                        color: "#eee"
                        font.pixelSize: 12
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 6
                    }
                }

                Rectangle {
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: 28
                    radius: 4
                    color: sortDirMouse.containsMouse ? "#666" : "#555"
                    Text {
                        anchors.centerIn: parent
                        text: djManager && djManager.sortAscending ? "▲" : "▼"
                        color: "#eee"
                        font.pixelSize: 14
                    }
                    MouseArea {
                        id: sortDirMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            if (djManager)
                                djManager.sortAscending = !djManager.sortAscending
                        }
                    }
                }
            }
        }

        // ── Song list with per-row show actions ──
        ListView {
            id: songList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: djManager ? djManager.songListModel : null
            delegate: Rectangle {
                width: songList.width
                height: 48
                color: model.isActive ? "#3a3a1a"
                     : model.isPlaying ? "#2a4a2a"
                     : ListView.isCurrentItem ? "#3366aa"
                     : ((index % 2 === 0) ? "#2a2a2a" : "#333")
                border.color: model.isActive ? "#f1c40f"
                            : model.isPlaying ? "#2ecc71" : "transparent"
                border.width: (model.isActive || model.isPlaying) ? 1 : 0

                MouseArea {
                    anchors.fill: parent
                    onClicked: songList.currentIndex = index
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 6

                    Text {
                        Layout.preferredWidth: 18
                        text: model.isPlaying ? "▶" : (model.isActive ? "★" : "")
                        color: model.isPlaying ? "#2ecc71" : "#f1c40f"
                        font.pixelSize: 14
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        Text {
                            Layout.fillWidth: true
                            text: (model.title || "?") + (model.artist ? " — " + model.artist : "")
                            color: model.isPlaying ? "#2ecc71" : "#eee"
                            font.pixelSize: 13
                            font.bold: model.isPlaying || model.isActive
                            elide: Text.ElideRight
                        }
                        Text {
                            Layout.fillWidth: true
                            text: fileName(model.filepath)
                            color: "#888"
                            font.pixelSize: 10
                            elide: Text.ElideLeft
                        }
                    }

                    Text {
                        Layout.preferredWidth: 56
                        text: fmtDuration(model.duration)
                        color: "#aaa"
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignRight
                    }

                    // Show assignment indicator
                    Text {
                        Layout.preferredWidth: 70
                        text: model.hasShow ? ("Show " + model.showId) : "no show"
                        color: model.hasShow ? "#3498db" : "#777"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignRight
                    }

                    // --- Per-row actions ---
                    Button {
                        text: "Load"
                        enabled: model.hasShow
                        implicitHeight: 26
                        onClicked: djManager.loadShow(model.filepath)
                    }
                    Button {
                        text: "Assign"
                        implicitHeight: 26
                        onClicked: {
                            assignPopup.targetFilepath = model.filepath
                            assignPopup.shows = djManager.availableShows()
                            assignPopup.open()
                        }
                    }
                    Button {
                        text: model.hasShow ? "New" : "Create"
                        implicitHeight: 26
                        onClicked: djManager.createShow(model.filepath)
                    }
                    Button {
                        text: "Clear"
                        enabled: model.hasShow
                        implicitHeight: 26
                        onClicked: djManager.clearShow(model.filepath)
                    }
                }
            }

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

    // ── Assign-show picker popup ──
    Popup {
        id: assignPopup
        modal: true
        focus: true
        anchors.centerIn: Overlay.overlay
        width: 360
        height: 420
        property string targetFilepath: ""
        property var shows: []

        background: Rectangle { color: "#2b2b2b"; border.color: "#555"; radius: 6 }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 8

            Text { text: "Assign a show"; color: "#eee"; font.pixelSize: 15; font.bold: true }

            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: assignPopup.shows
                delegate: Rectangle {
                    width: ListView.view.width
                    height: 32
                    color: maPick.containsMouse ? "#3366aa" : "transparent"
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.leftMargin: 6
                        text: modelData.name + "  (#" + modelData.id + ")"
                        color: "#eee"
                        font.pixelSize: 13
                    }
                    MouseArea {
                        id: maPick
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            djManager.assignShow(assignPopup.targetFilepath, modelData.id)
                            assignPopup.close()
                        }
                    }
                }
                Text {
                    anchors.centerIn: parent
                    visible: assignPopup.shows.length === 0
                    text: "No shows available"
                    color: "#888"
                }
            }

            Button {
                text: "Cancel"
                Layout.alignment: Qt.AlignRight
                onClicked: assignPopup.close()
            }
        }
    }
}
