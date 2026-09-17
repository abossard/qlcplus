/*
  Q Light Controller Plus
  VCAudioTriggersItem.qml

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

// IRON RULE (see docs/audio-dsp-plans/plan-widget-impl.md §0):
// QML does ZERO math. Reads Q_PROPERTYs and maps 0..1 to pixel height/width.
// Allowed: Math.max/min as 0..1 pixel-safety guards, Math.round() for pixel
// coordinates, ring-buffer index arithmetic. Forbidden: log/pow/exp/sqrt of
// audio values, normalization, smoothing, dB mapping, peak tracking.

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import org.qlcplus.classes 1.0
import "."

VCWidgetItem
{
    id: audioTriggerRoot
    property VCAudioTriggers audioTriggerObj: null
    property bool diagnosticsExpanded: false

    // Color zones (red = lows, green = mids, cyan = highs)
    property var perceptualBandColors: [ "#ff6633", "#33cc66", "#33ccff" ]

    // 9 onset method letters / colors (E,H,C,P,W,D,K,M,F)
    property var onsetMethodLetters: [ "E", "H", "C", "P", "W", "D", "K", "M", "F" ]
    property var onsetMethodColors: [ "#ff3333", "#ff9900", "#ffdd33", "#33cc66", "#33ccff",
                                      "#9966ff", "#ff66cc", "#cc6600", "#66ffcc" ]

    // Per-method display mode: 0 = trace (sparkline), 1 = trigger (vertical bars).
    // QML-LOCAL state, not persisted. Decision: docs/audio-dsp-plans/plan-clean-engineering.md §0.
    // Resets to all-trace on widget reload.
    property var onsetMethodModes: [ 0, 0, 0, 0, 0, 0, 0, 0, 0 ]
    property real triggerModeThreshold: 0.45

    clip: true

    onAudioTriggerObjChanged: setCommonProperties(audioTriggerObj)

    GridLayout
    {
        id: itemsLayout
        anchors.fill: parent
        columns: 2
        rows: 2

        // ============================================================
        // Main visualization area: 8 sections stacked top → bottom
        // ============================================================
        Rectangle
        {
            id: barsItem
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.rowSpan: 2
            color: "transparent"

            // -----------------------------------------------------------
            // Sparkline is now drawn by a C++ QQuickPaintedItem
            // (AudioSparklineItem). It owns its own ring buffer, listens
            // to audioSnapshotChanged directly, and paints incrementally
            // onto its FBO. No JS math, no per-frame canvas reallocations.
            // -----------------------------------------------------------

            ColumnLayout
            {
                id: stack
                anchors.fill: parent
                anchors.margins: 2
                spacing: 3

                // Cached lists — one C++ getter call per signal instead of N
                property var mfccCache: audioTriggerObj ? audioTriggerObj.mfccCoeffs : []

                Text
                {
                    Layout.fillWidth: true
                    text: audioTriggerObj ? (audioTriggerObj.captureEnabled
                          ? qsTr("Mappings enabled") : qsTr("Mappings paused")) + " | " +
                          audioTriggerObj.analysisStatus + " | " +
                          audioTriggerObj.rmsDb.toFixed(1) + " dB | " +
                          (audioTriggerObj.noiseGateOpen ? qsTr("Gate open") : qsTr("Gate closed")) : ""
                    color: audioTriggerObj && audioTriggerObj.analysisAvailable ? "#bbbbbb" : "#ffaa55"
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }
                Button
                {
                    text: qsTr("Diagnostics")
                    checkable: true
                    checked: audioTriggerRoot.diagnosticsExpanded
                    onToggled: audioTriggerRoot.diagnosticsExpanded = checked
                }
                Text
                {
                    visible: audioTriggerRoot.diagnosticsExpanded
                    Layout.fillWidth: true
                    text: audioTriggerObj && audioTriggerObj.audioSource !== 0
                          ? qsTr("Diagnostic processing disabled for OSC profiles.")
                          : !audioTriggerObj || !audioTriggerObj.diagnosticsEnabled
                          ? qsTr("Diagnostic processing disabled. Enable it under Advanced analysis.")
                          : audioTriggerObj.appliedAudio.diagnosticsEnabled
                            ? qsTr("Diagnostic processing enabled")
                            : qsTr("Waiting for diagnostic configuration to be applied")
                    color: "#bbbbbb"
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                }

                // ============================================================
                // §2 — Kick power / Bass / Lows / Mids / High power bars
                //      LedFx audio.py:1283-1342 — 4 raw freq_power slots +
                //      lows = (beat + bass) / 2 composite. Vertical bars,
                //      same width, height proportional to 0..1 power.
                // ============================================================
                Item
                {
                    id: powerSection
                    objectName: "audioPowerSection"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 64
                    Layout.fillHeight: true
                    Layout.minimumHeight: 32

                    property var bands: audioTriggerObj ? audioTriggerObj.spectrumBars : []

                    Row
                    {
                        id: powerBarsRow
                        anchors.fill: parent
                        spacing: 2

                        Repeater
                        {
                            model: powerSection.bands

                            Item
                            {
                                width: Math.max(1, (powerBarsRow.width -
                                            (powerSection.bands.length - 1) * powerBarsRow.spacing) /
                                            powerSection.bands.length)
                                height: powerBarsRow.height

                                // Track (background + border highlights when active)
                                Rectangle
                                {
                                    id: pwrTrack
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    anchors.bottom: pwrLabel.top
                                    anchors.bottomMargin: 1
                                    color: "#1a1a1a"
                                    border.color: modelData.active ? "#ffffff" : "#333333"
                                    border.width: 1

                                    // Fill grows up from the bottom; height = value * available
                                    Rectangle
                                    {
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.bottom: parent.bottom
                                        anchors.margins: 1
                                        height: Math.max(0, (parent.height - 2) *
                                                Math.max(0, Math.min(1, modelData.value)))
                                        color: modelData.color
                                    }
                                }

                                // Band label below the bar
                                Text
                                {
                                    id: pwrLabel
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    horizontalAlignment: Text.AlignHCenter
                                    text: modelData.name
                                    color: "#cccccc"
                                    font.pixelSize: 10
                                }
                            }
                        }
                    }
                }

                // ============================================================
                // §3 + §4 + §5 + §7 — shared sparkline canvas
                // ============================================================
                Item
                {
                    id: canvasSection
                    visible: audioTriggerRoot.diagnosticsExpanded
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 80
                    Layout.maximumHeight: 300

                    // Sub-section heights (in fractions of section height)
                    property real onsetFrac:   0.45
                    property real pitchFrac:   0.15
                    property real spectralFrac:0.25
                    property real tssFrac:     0.15

                    AudioSparkline
                    {
                        id: sparkline
                        anchors.fill: parent
                        source: audioTriggerRoot.diagnosticsExpanded ? audioTriggerObj : null
                        pixelsPerSample: 3
                        onsetFrac:    canvasSection.onsetFrac
                        pitchFrac:    canvasSection.pitchFrac
                        spectralFrac: canvasSection.spectralFrac
                        tssFrac:      canvasSection.tssFrac
                        triggerModeThreshold: 0.45
                    }

                    // Pitch note text overlay (top-right of pitch lane)
                    Text
                    {
                        anchors.right: parent.right
                        anchors.rightMargin: 4
                        anchors.top: parent.top
                        anchors.topMargin: Math.round(parent.height * canvasSection.onsetFrac) + 2
                        text: {
                            if (!audioTriggerObj) return ""
                            var nt = audioTriggerObj.pitchNoteText
                            if (nt && nt.length > 0) return nt
                            // Fallback while Phase A is pending: only show numeric Hz
                            // to honor the Iron Rule (no Math.log of audio values).
                            var hz = +(audioTriggerObj.pitchHz || 0)
                            return hz > 0 ? (Math.round(hz) + " Hz") : "--"
                        }
                        color: "#ffcc33"
                        font.pixelSize: 10
                        font.bold: true
                    }

                    // Click handler for the onset section:
                    //   left-click on the left gutter (letter strip)  -> toggle method enable
                    //   right-click anywhere in the onset lane(s)     -> toggle trace/trigger mode
                    // Per-method mode is QML-LOCAL (not persisted). See plan §0.
                    // Click handler for the onset section:
                    //   left-click on the left gutter (letter strip)  -> toggle method enable
                    //   right-click anywhere in the onset lane(s)     -> toggle trace/trigger mode
                    // Per-method mode is QML-LOCAL state owned by AudioSparklineItem.
                    MouseArea
                    {
                        id: onsetClickArea
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        propagateComposedEvents: true

                        function hitInfo(px, py)
                        {
                            var leftGutter = 18
                            var hOnset = Math.round(parent.height * canvasSection.onsetFrac)
                            if (py < 0 || py > hOnset) return null
                            if (!audioTriggerObj) return null
                            var enabledArr = audioTriggerObj.onsetMethodsEnabled
                            var enabledIdx = []
                            for (var ei = 0; ei < 9 && enabledArr && ei < enabledArr.length; ei++)
                                if (enabledArr[ei]) enabledIdx.push(ei)
                            var lanes = (enabledIdx.length === 0) ? 0
                                      : (enabledIdx.length <= 3 ? 1
                                      : (enabledIdx.length <= 6 ? 2 : 3))
                            if (lanes === 0)
                            {
                                if (px < leftGutter)
                                    return { gutter: true, method: 0, lane: 0, lanes: 0 }
                                return null
                            }
                            var laneH = Math.floor(hOnset / lanes)
                            var laneIdx = Math.min(lanes - 1, Math.floor(py / Math.max(1, laneH)))
                            var methodIdx = -1
                            for (var mi = 0; mi < enabledIdx.length; mi++)
                            {
                                if (Math.floor((mi * lanes) / enabledIdx.length) === laneIdx)
                                {
                                    methodIdx = enabledIdx[mi]
                                    break
                                }
                            }
                            return { gutter: px < leftGutter, method: methodIdx, lane: laneIdx, lanes: lanes }
                        }

                        onClicked: function(mouse)
                        {
                            var info = hitInfo(mouse.x, mouse.y)
                            if (!info) { mouse.accepted = false; return }
                            if (mouse.button === Qt.LeftButton && info.gutter)
                            {
                                if (audioTriggerObj && info.method >= 0)
                                    audioTriggerObj.setOnsetMethodEnabled(info.method, false)
                            }
                            else if (mouse.button === Qt.RightButton && info.method >= 0)
                            {
                                sparkline.toggleOnsetMethodMode(info.method)
                            }
                            else
                            {
                                mouse.accepted = false
                            }
                        }
                    }
                }

                // ============================================================
                // §8 — Status bar (centered): BPM | Vol | AGC | Phase dots
                // ============================================================
                Item
                {
                    id: statusSection
                    Layout.fillWidth: true
                    Layout.preferredHeight: 18
                    Layout.minimumHeight: 14

                    Row
                    {
                        anchors.centerIn: parent
                        spacing: 10

                        Text
                        {
                            anchors.verticalCenter: parent.verticalCenter
                            text: audioTriggerObj && audioTriggerObj.appliedAudio.tempoValid
                                  ? ("\u266A " + Math.round(audioTriggerObj.detectedBpm) + " BPM")
                                  : ""
                            color: "#dddddd"
                            font.pixelSize: 11
                            font.bold: true
                        }
                        Text
                        {
                            anchors.verticalCenter: parent.verticalCenter
                            text: audioTriggerObj
                                  ? ("Vol " + Math.round(
                                        Math.max(0, Math.min(1, audioTriggerObj.volumeNormalized || 0)) * 100)
                                     + "%")
                                  : ""
                            color: "#bbbbbb"
                            font.pixelSize: 11
                        }
                        Text
                        {
                            anchors.verticalCenter: parent.verticalCenter
                            text: {
                                if (!audioTriggerObj) return ""
                                if (!audioTriggerObj.melPostEnabled) return "Mel post OFF"
                                var g = +(audioTriggerObj.melAgcGain || 0)
                                return "AGC " + g.toFixed(2)
                            }
                            color: "#aaaaaa"
                            font.pixelSize: 11
                        }
                        Row
                        {
                            id: barDots
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 4
                            Repeater
                            {
                                model: audioTriggerObj && audioTriggerObj.appliedAudio.tempoValid
                                       ? audioTriggerObj.appliedAudio.beatsPerBar : 0
                                Rectangle
                                {
                                    width: 7
                                    height: 7
                                    radius: 4
                                    color: (audioTriggerObj &&
                                            index === audioTriggerObj.appliedAudio.beatInBar)
                                           ? "#ffffff" : "#444444"
                                }
                            }
                        }
                    }
                }
            }
        }

        // ============================================================
        // Right column: enable button + volume fader (unchanged)
        // ============================================================
        IconButton
        {
            objectName: "audioMappingsToggle"
            Accessible.name: qsTr("Enable mappings")
            Accessible.description: checked ? qsTr("Mappings enabled") : qsTr("Mappings paused; meters still update")
            width: height
            height: UISettings.iconSizeMedium
            Layout.alignment: Qt.AlignHCenter
            radius: 0
            border.width: 0
            checkable: true
            tooltip: checked ? qsTr("Mappings enabled. Click to pause.") : qsTr("Mappings paused. Click to enable.")
            faSource: FontAwesome.fa_check
            faColor: "lime"
            imgMargins: 1
            checked: audioTriggerObj ? audioTriggerObj.captureEnabled : false
            onToggled: if (audioTriggerObj) audioTriggerObj.captureEnabled = checked
        }

        QLCPlusFader
        {
            enabled: audioTriggerObj ? !audioTriggerObj.isDisabled : false
            Layout.alignment: Qt.AlignHCenter
            Layout.fillHeight: true
            width: parent.width
            from: 0
            to: 100
            value: audioTriggerObj ? audioTriggerObj.volumeLevel : 0
            onMoved: if (audioTriggerObj) audioTriggerObj.volumeLevel = valueAt(position)
        }
    }
}
