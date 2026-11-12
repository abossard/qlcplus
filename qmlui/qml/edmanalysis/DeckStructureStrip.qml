/*
  Q Light Controller Plus
  DeckStructureStrip.qml

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

/** The structure of the song on one deck: the whole song, a scrolling zoom and the countdowns */
Rectangle
{
    id: strip
    objectName: "deckStructureStrip" + deck

    property int deck: 1
    readonly property QtObject info: edmStructure ? edmStructure.deck(deck) : null
    readonly property string state: info ? info.state : "none"
    readonly property bool ready: state === "ready"

    readonly property var labelColors: ({ "intro": "#5b6b82", "buildup": "#e0a030", "drop": "#e0408a",
                                          "breakdown": "#3d7fe0", "groove": "#3fae6a", "outro": "#6b6b78" })
    readonly property real zoomBars: 24
    readonly property real zoomPlayhead: 0.25
    readonly property real dropWarnBars: 16
    readonly property real sureConfidence: 50
    readonly property real sureDropConfidence: 90

    function colorOf(label) { return labelColors[label] || "#6b6b78" }

    readonly property string message: {
        switch (state)
        {
            case "lookup": return qsTr("looking up")
            case "queued": return qsTr("queued")
            case "analysing": return "⟳ " + qsTr("analysing") + (info.stage ? ": " + info.stage : "")
            case "failed": return qsTr("failed") + (info.error ? ": " + info.error : "")
            default: return qsTr("no structure")
        }
    }

    visible: state !== "none" || (info && info.path !== "")
    height: !visible ? 0 : ready ? 124 : 24
    color: "#262626"
    border.color: "#444"

    Connections
    {
        target: strip.info
        function onStaticChanged() { overview.requestPaint(); zoom.requestPaint() }
        function onLiveChanged() { zoom.requestPaint() }
    }

    RowLayout
    {
        visible: !strip.ready
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: 8
        Text
        {
            Layout.fillWidth: true
            text: qsTr("Deck %1: %2").arg(strip.deck).arg(strip.message)
            color: strip.state === "failed" ? "#e74c3c" : strip.state === "analysing" ? "#f1c40f" : "#999"
            font.pixelSize: 12
            font.bold: strip.state === "analysing"
            elide: Text.ElideRight
        }
        Button
        {
            visible: strip.state === "none"
            text: qsTr("Analyse now")
            implicitHeight: 20
            onClicked: edmStructure.analyse(strip.info.path)
        }
        Button
        {
            visible: strip.state === "failed"
            text: qsTr("Retry")
            implicitHeight: 20
            onClicked: edmStructure.retry(strip.info.path)
        }
    }

    ColumnLayout
    {
        visible: strip.ready
        anchors.fill: parent
        anchors.margins: 4
        spacing: 2

        RowLayout
        {
            Layout.fillWidth: true
            spacing: 12
            readonly property int section: strip.ready ? strip.info.section : -1
            readonly property var current: section >= 0 ? strip.info.sections[section] : null

            Text
            {
                objectName: "stripNow"
                text: parent.current
                      ? qsTr("D%1  NOW %2 · bar %3 of %4").arg(strip.deck).arg(parent.current.label.toUpperCase())
                            .arg(Math.max(1, Math.floor(strip.info.bar) + 1)).arg(Math.round(strip.info.totalBars))
                      : qsTr("D%1  bar %2 of %3").arg(strip.deck).arg(Math.max(1, Math.floor(strip.info.bar) + 1)).arg(Math.round(strip.info.totalBars))
                color: parent.current ? strip.colorOf(parent.current.label) : "#999"
                font.pixelSize: 13
                font.bold: true
            }
            Text
            {
                objectName: "stripNext"
                Layout.fillWidth: true
                text: strip.info.barsToNextSection >= 0
                      ? qsTr("next: %1 in %2 bars").arg(strip.info.nextSectionLabel).arg(Math.ceil(strip.info.barsToNextSection))
                      : qsTr("last section")
                color: "#aaa"
                font.pixelSize: 12
                elide: Text.ElideRight
            }
            Text
            {
                objectName: "stripDrop"
                visible: strip.info.barsToDrop >= 0 && strip.info.barsToDrop <= strip.dropWarnBars
                text: qsTr("DROP in %1").arg(Math.ceil(strip.info.barsToDrop))
                color: strip.colorOf("drop")
                opacity: strip.info.dropConfidence >= strip.sureDropConfidence ? 1 : 0.45
                font.pixelSize: 20
                font.bold: true
            }
        }

        Item
        {
            Layout.fillWidth: true
            Layout.preferredHeight: 26

            Canvas
            {
                id: overview
                anchors.fill: parent
                onPaint: strip.paintBand(getContext("2d"), width, height, 0, strip.info.totalBars, false)
            }
            Rectangle
            {
                width: 2
                height: parent.height
                color: "white"
                x: strip.info.totalBars > 0 ? Math.max(0, Math.min(1, strip.info.bar / strip.info.totalBars)) * (parent.width - 2) : 0
            }
        }

        Item
        {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Canvas
            {
                id: zoom
                anchors.fill: parent
                onPaint: {
                    var from = strip.info.bar - strip.zoomBars * strip.zoomPlayhead
                    strip.paintBand(getContext("2d"), width, height, from, from + strip.zoomBars, true)
                }
            }
            Rectangle
            {
                width: 2
                height: parent.height
                color: "white"
                x: parent.width * strip.zoomPlayhead - 1
            }
        }
    }

    // sections that are not sure are faded and hatched; confidence itself is never shown
    function paintBand(g, w, h, from, to, big) {
        g.clearRect(0, 0, w, h)
        if (!strip.ready || to <= from)
            return

        var px = function(bar) { return (bar - from) / (to - from) * w }
        var top = big ? 2 : 0, bh = big ? h - 14 : h
        var sections = info.sections

        for (var i = 0; i < sections.length; i++)
        {
            var s = sections[i]
            if (s.endBar < from || s.startBar > to)
                continue
            var x = px(s.startBar), sw = px(s.endBar) - x - 1
            var sure = s.confidence >= sureConfidence
            g.globalAlpha = sure ? 1 : 0.45
            g.fillStyle = colorOf(s.label)
            g.fillRect(x, top, sw, bh)
            g.globalAlpha = 1
            if (!sure)
            {
                g.save()
                g.beginPath(); g.rect(x, top, sw, bh); g.clip()
                g.strokeStyle = "rgba(255,255,255,0.35)"; g.lineWidth = 1
                for (var k = -bh; k < sw; k += 7)
                {
                    g.beginPath(); g.moveTo(x + k, top + bh); g.lineTo(x + k + bh, top); g.stroke()
                }
                g.restore()
            }
            if (big)
            {
                g.save()
                g.beginPath(); g.rect(Math.max(x, 0), top, sw, bh); g.clip()
                g.fillStyle = "white"
                g.font = "bold 11px sans-serif"
                var text = s.label.toUpperCase()
                if (g.measureText(text).width < sw - 8)
                    g.fillText(text, Math.max(x, 0) + 5, top + 14)
                g.restore()
            }
        }

        if (big)
        {
            g.font = "9px sans-serif"
            for (var b = Math.max(0, Math.ceil(from)); b <= to; b++)
            {
                g.fillStyle = b % 8 === 0 ? "rgba(255,255,255,0.55)" : "rgba(255,255,255,0.15)"
                g.fillRect(px(b), top, 1, bh)
                if (b % 4 === 0)
                {
                    g.fillStyle = "#9aa1ad"
                    g.fillText(b + 1, px(b) + 2, h - 3)
                }
            }
        }

        var events = info.events
        for (var e = 0; e < events.length; e++)
        {
            if (events[e].kind !== "drop" || events[e].bar < from || events[e].bar > to)
                continue
            var ex = px(events[e].bar), u = big ? 6 : 3
            g.fillStyle = colorOf("drop")
            g.strokeStyle = "white"
            g.lineWidth = 1
            g.beginPath(); g.moveTo(ex - u, top); g.lineTo(ex + u, top); g.lineTo(ex, top + u * 1.5); g.closePath()
            g.fill(); g.stroke()
        }
    }
}
