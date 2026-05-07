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

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import org.qlcplus.classes 1.0
import "."

VCWidgetItem
{
    id: audioTriggerRoot
    property VCAudioTriggers audioTriggerObj: null
    property var perceptualBandColors: [ "#ff3333", "#ff9900", "#ffdd33", "#33cc66", "#33ccff" ]
    property var perceptualBandShortNames: [ "S", "B", "LM", "M", "H" ]

    // QLC+ DERIVATION (NOT raw aubio): count of onset methods firing this hop,
    // 0..9. Aubio raw output is the per-method booleans; this is a UI sum.
    function onsetVoteCount()
    {
        if (!audioTriggerObj) return 0
        var f = audioTriggerObj.onsetFlags
        if (!f) return 0
        var n = 0
        for (var i = 0; i < f.length; i++) if (f[i]) n++
        return n
    }

    // QLC+ DERIVATION (NOT raw aubio): collapse the per-bin transient/steady
    // cvec norm arrays into a single 0..1 ratio for visualization. Aubio's
    // raw output is the per-bin arrays; this UI sum/ratio is QLC+'s.
    function tssDerivedRatio()
    {
        if (!audioTriggerObj) return 0
        var t = audioTriggerObj.tssTransientNorm
        var s = audioTriggerObj.tssSteadyNorm
        if (!t || !s) return 0
        var n = Math.min(t.length, s.length)
        var tSum = 0, sSum = 0
        for (var i = 0; i < n; i++) { tSum += t[i]; sSum += s[i] }
        var total = tSum + sSum
        return total > 1e-10 ? (tSum / total) : 0
    }

    // Width/height breakpoints driving layout density
    property bool showFull:    barsItem.width >= 300 && barsItem.height >= 260
    property bool showMedium:  !showFull && barsItem.width >= 220 && barsItem.height >= 160
    property bool showCompact: !showFull && !showMedium && barsItem.height >= 80
    // else: minimal (mel bars only)

    function perceptualBandPower(index)
    {
        if (!audioTriggerObj) return 0
        switch (index)
        {
        case 0: return audioTriggerObj.subPower
        case 1: return audioTriggerObj.bassPower
        case 2: return audioTriggerObj.lowMidPower
        case 3: return audioTriggerObj.midPower
        case 4: return audioTriggerObj.highPower
        default: return 0
        }
    }

    function melBandColor(index)
    {
        if (!audioTriggerObj) return perceptualBandColors[0]
        if (index < audioTriggerObj.melCrossSub)    return perceptualBandColors[0]
        if (index < audioTriggerObj.melCrossBass)   return perceptualBandColors[1]
        if (index < audioTriggerObj.melCrossLowMid) return perceptualBandColors[2]
        if (index < audioTriggerObj.melCrossMid)    return perceptualBandColors[3]
        return perceptualBandColors[4]
    }

    function midiToNoteName(hz)
    {
        if (hz <= 0) return "--"
        var midi = 69 + 12 * Math.log(hz / 440) / Math.log(2)
        var notes = ["C","C#","D","D#","E","F","F#","G","G#","A","A#","B"]
        var rounded = Math.round(midi)
        var note = notes[((rounded % 12) + 12) % 12]
        var octave = Math.floor(rounded / 12) - 1
        return note + octave
    }

    function confidenceColor(conf)
    {
        if (conf < 0.2) return "#666666"
        if (conf < 0.4) return "#cc3333"
        if (conf < 0.6) return "#cc9933"
        if (conf < 0.8) return "#cccc33"
        return "#33cc66"
    }

    function onsetVoteColor(votes)
    {
        if (votes <= 0) return "#666666"
        if (votes >= 5) return "#33cc66"
        if (votes >= 3) return "#cccc33"
        return "#cc9933"
    }

    clip: true

    onAudioTriggerObjChanged:
    {
        setCommonProperties(audioTriggerObj)
    }

    GridLayout
    {
        id: itemsLayout
        anchors.fill: parent
        columns: 2
        rows: 2

        // Mel spectrum + status area
        Rectangle
        {
            id: barsItem
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.rowSpan: 2
            color: "transparent"

            // Beat flash overlay
            Rectangle
            {
                anchors.fill: parent
                color: "#FFFFFF"
                opacity: audioTriggerObj && audioTriggerObj.beatActive ? 0.12 : 0.0
                Behavior on opacity { NumberAnimation { duration: 120 } }
                z: 2
            }

            // Mel spectrum bars (40 bands)
            Row
            {
                id: melBars
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: bottomColumn.visible ? bottomColumn.top : parent.bottom

                Repeater
                {
                    model: audioTriggerObj ? audioTriggerObj.melSpectrum.length : 0
                    Rectangle
                    {
                        width: melBars.width / 40
                        height: melBars.height
                        color: UISettings.bgStrong
                        border.width: 0

                        Rectangle
                        {
                            anchors.bottom: parent.bottom
                            anchors.left: parent.left
                            anchors.right: parent.right
                            height: parent.height * Math.max(0, Math.min(1, audioTriggerObj.melSpectrum[index] || 0))
                            // Tint mel bars by transient ratio: pure-steady stays at the
                            // band's natural color, pure-transient shifts toward white to
                            // show where the energy is percussive vs harmonic.
                            color: {
                                var base = melBandColor(index)
                                var r = audioTriggerObj ? tssDerivedRatio() : 0
                                return Qt.tint(base, Qt.rgba(1, 1, 1, r * 0.5))
                            }
                        }
                    }
                }
            }

            // Bottom area: per-band horizontal bars + status row(s)
            Column
            {
                id: bottomColumn
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.leftMargin: 4
                anchors.rightMargin: 4
                spacing: 2
                visible: showFull || showMedium || showCompact
                z: 3

                // Perceptual band bars (full + medium)
                Column
                {
                    width: parent.width
                    spacing: 2
                    visible: showFull || showMedium

                    Repeater
                    {
                        model: 5
                        Row
                        {
                            width: parent.width
                            height: showFull ? 14 : 10
                            spacing: 4

                            Text
                            {
                                width: 18
                                text: perceptualBandShortNames[index]
                                color: perceptualBandColors[index]
                                font.pixelSize: showFull ? 10 : 9
                                font.bold: true
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            Rectangle
                            {
                                width: parent.width - 60
                                height: showFull ? 8 : 6
                                radius: 3
                                color: "#222222"
                                anchors.verticalCenter: parent.verticalCenter

                                Rectangle
                                {
                                    anchors.left: parent.left
                                    anchors.top: parent.top
                                    anchors.bottom: parent.bottom
                                    width: parent.width * Math.max(0, Math.min(1, perceptualBandPower(index)))
                                    radius: parent.radius
                                    color: perceptualBandColors[index]
                                }
                            }

                            // Trigger state dot
                            Rectangle
                            {
                                width: 10; height: 10; radius: 5
                                anchors.verticalCenter: parent.verticalCenter
                                border.width: 1
                                property var ts: audioTriggerObj ? audioTriggerObj.triggerStates[index] : null
                                color: {
                                    if (!ts) return "#333333"
                                    if (ts.fired) return Qt.lighter(perceptualBandColors[index], 1.5)
                                    if (ts.active) return perceptualBandColors[index]
                                    if (ts.cooldownMs > 0) return "#cc9933"
                                    return "#444444"
                                }
                                border.color: (ts && ts.active) ? "#FFFFFF" : "#222222"
                            }

                            Text
                            {
                                visible: showFull
                                width: 28
                                text: Math.round(perceptualBandPower(index) * 100) + "%"
                                color: "#AAAAAA"
                                font.pixelSize: 9
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }
                }

                // Pitch row (full only)
                Row
                {
                    width: parent.width
                    height: 14
                    spacing: 6
                    visible: showFull

                    Text
                    {
                        text: audioTriggerObj ? midiToNoteName(audioTriggerObj.pitchHz) : "--"
                        color: audioTriggerObj ? confidenceColor(audioTriggerObj.pitchConfidence) : "#666666"
                        font.pixelSize: 11
                        font.bold: true
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Text
                    {
                        text: audioTriggerObj && audioTriggerObj.pitchHz > 0 ?
                              audioTriggerObj.pitchHz.toFixed(0) + " Hz" : ""
                        color: audioTriggerObj ? confidenceColor(audioTriggerObj.pitchConfidence) : "#666666"
                        font.pixelSize: 10
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                // TSS (transient/steady) split row — full only.
                // Shows a single horizontal bar split between cyan (steady,
                // harmonic energy) and orange (transient, percussive energy).
                Row
                {
                    width: parent.width
                    height: 12
                    spacing: 6
                    visible: showFull

                    Text
                    {
                        width: 18
                        text: "T/S"
                        color: "#888888"
                        font.pixelSize: 9
                        font.bold: true
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Rectangle
                    {
                        width: parent.width - 60
                        height: 6
                        radius: 3
                        color: "#222222"
                        anchors.verticalCenter: parent.verticalCenter

                        // Steady (left, cyan)
                        Rectangle
                        {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            width: parent.width * (1 - (audioTriggerObj ? tssDerivedRatio() : 0))
                            radius: parent.radius
                            color: "#3FA9F5"
                        }
                        // Transient (right, orange)
                        Rectangle
                        {
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            width: parent.width * (audioTriggerObj ? tssDerivedRatio() : 0)
                            radius: parent.radius
                            color: "#FF8C29"
                        }
                    }

                    Text
                    {
                        width: 28
                        text: audioTriggerObj
                              ? Math.round(tssDerivedRatio() * 100) + "%"
                              : "--"
                        color: "#AAAAAA"
                        font.pixelSize: 9
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                // Status row: full = text labels; compact = dots only
                Row
                {
                    id: statusRow
                    width: parent.width
                    height: 16
                    spacing: 6
                    visible: showFull || showMedium || showCompact

                    // Onset vote count
                    Rectangle
                    {
                        visible: showFull || showMedium
                        width: onsetText.width + 8
                        height: 14
                        radius: 3
                        color: "#1a1a1a"
                        border.width: 1
                        border.color: audioTriggerObj ?
                                      onsetVoteColor(onsetVoteCount()) : "#444444"
                        anchors.verticalCenter: parent.verticalCenter

                        Text
                        {
                            id: onsetText
                            anchors.centerIn: parent
                            text: audioTriggerObj ?
                                  qsTr("Onset") + " " + onsetVoteCount() + "/9" : "Onset 0/9"
                            color: audioTriggerObj ?
                                   onsetVoteColor(onsetVoteCount()) : "#666666"
                            font.pixelSize: 9
                            font.bold: true
                        }
                    }

                    // Beat dot + BPM
                    Rectangle
                    {
                        width: 10; height: 10; radius: 5
                        color: audioTriggerObj && audioTriggerObj.beatActive ? "#FF3333" : "#333333"
                        border.width: 1; border.color: "#000000"
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Text
                    {
                        visible: showFull || showMedium
                        text: {
                            if (!audioTriggerObj) return "-- BPM"
                            var bpm = audioTriggerObj.detectedBpm
                            var conf = audioTriggerObj.beatConfidence
                            return Math.round(bpm) + " BPM " + Math.round(conf * 100) + "%"
                        }
                        color: audioTriggerObj ? confidenceColor(audioTriggerObj.beatConfidence) : "#666666"
                        font.pixelSize: 9
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    // Gate dot
                    Rectangle
                    {
                        width: 10; height: 10; radius: 5
                        color: audioTriggerObj && audioTriggerObj.noiseGateOpen ? "#33cc66" : "#cc3333"
                        border.width: 1; border.color: "#000000"
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Text
                    {
                        visible: showFull
                        text: qsTr("Gate")
                        color: "#AAAAAA"
                        font.pixelSize: 9
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    // Volume
                    Text
                    {
                        text: audioTriggerObj ?
                              (showFull ? qsTr("Vol") + " " : "") + Math.round(audioTriggerObj.volumeNormalized * 100) + "%"
                              : "--"
                        color: "#AAAAAA"
                        font.pixelSize: 10
                        font.bold: true
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }
        }

        // enable button
        IconButton
        {
            width: height
            height: UISettings.iconSizeMedium
            Layout.alignment: Qt.AlignHCenter
            radius: 0
            border.width: 0
            checkable: true
            tooltip: qsTr("Enable/Disable the audio capture")
            faSource: FontAwesome.fa_check
            faColor: "lime"
            imgMargins: 1
            checked: audioTriggerObj ? audioTriggerObj.captureEnabled : false
            onToggled: if (audioTriggerObj) audioTriggerObj.captureEnabled = checked
        }

        // the volume fader
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
