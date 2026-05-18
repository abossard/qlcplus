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

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            color: "#333"
            Text {
                anchors.centerIn: parent
                text: vdjBridge ? "VDJ: " + vdjBridge.telemetryStatus + (vdjBridge.telemetryConnected ? " connected" : "") : "VDJ: n/a"
                color: vdjBridge && vdjBridge.telemetryConnected ? "#2ecc71" : "#aaa"
                font.pixelSize: 14
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            color: "#444"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                Text { text: "Title"; color: "#ccc"; Layout.fillWidth: true; font.pixelSize: 12 }
                Text { text: "Duration"; color: "#ccc"; Layout.preferredWidth: 80; font.pixelSize: 12 }
                Text { text: "ID"; color: "#ccc"; Layout.preferredWidth: 60; font.pixelSize: 12 }
            }
        }

        ListView {
            id: songList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: songManager ? songManager.songListModel : null
            delegate: Rectangle {
                width: songList.width
                height: 36
                color: ListView.isCurrentItem ? "#3366aa" : ((index % 2 === 0) ? "#2a2a2a" : "#333")
                MouseArea { anchors.fill: parent; onClicked: songList.currentIndex = index }
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    Text { Layout.fillWidth: true; text: model.title || model.filepath || "?"; color: "#eee"; font.pixelSize: 13; elide: Text.ElideRight }
                    Text { Layout.preferredWidth: 80; text: fmtDuration(model.duration); color: "#aaa"; font.pixelSize: 13 }
                    Text { Layout.preferredWidth: 60; text: "" + model.showId; color: "#888"; font.pixelSize: 13 }
                }
            }
            Text {
                anchors.centerIn: parent
                visible: songList.count === 0
                text: "No songs yet - play a track in VDJ"
                color: "#888"
                font.pixelSize: 14
            }
        }
    }
}
