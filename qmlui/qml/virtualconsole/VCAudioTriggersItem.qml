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

    // Color the mel bin by zone (Low/Mid/High). Comparisons only — no math.
    function melBandColor(index)
    {
        if (!audioTriggerObj) return perceptualBandColors[0]
        if (index < audioTriggerObj.melCrossLowMid) return perceptualBandColors[0]
        if (index < audioTriggerObj.melCrossMid)    return perceptualBandColors[1]
        return perceptualBandColors[2]
    }

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
            // Shared sparkline canvas — one allocation, ONE Canvas, one
            // requestPaint() per audio snapshot. Covers Onsets, Pitch,
            // Spectral, TSS lanes (channels 0..15).
            // -----------------------------------------------------------
            // Ring buffer: time-based capacity (seconds × visual fps).
            // Drawing downsamples to canvas pixel width — constant paint cost.
            property int  histChannelCount: 16  // 9 onset + 1 pitch + 4 spectral + 2 tss
            readonly property int visualFps: 28 // ~86Hz audio / 3 hop throttle
            property int  historySeconds: 10
            readonly property int histCapacity: Math.max(1, Math.round(historySeconds * visualFps))
            property var  history:     new Float64Array(histChannelCount * histCapacity)
            property var  beatHistory: new Uint8Array(histCapacity)
            property var  kickHistory: new Uint8Array(histCapacity)
            property int  writePos:    0
            property int  sampleCount: 0
            property bool prevBeat:    false
            property bool prevKick:    false
            property var  boundAudioTriggerObj: audioTriggerObj

            onBoundAudioTriggerObjChanged: syncHistoryCapacity(true)
            onHistCapacityChanged: resetHistory()
            Component.onCompleted: syncHistoryCapacity(true)

            function configuredHistorySeconds()
            {
                var seconds = audioTriggerObj ? +(audioTriggerObj.onsetHistorySeconds || 10) : 10
                if (isNaN(seconds) || seconds <= 0) seconds = 10
                return seconds
            }

            function syncHistoryCapacity(forceReset)
            {
                var seconds = configuredHistorySeconds()
                if (historySeconds !== seconds)
                {
                    var oldCapacity = histCapacity
                    historySeconds = seconds
                    if (histCapacity === oldCapacity)
                    {
                        if (forceReset)
                            resetHistory()
                        else
                            sparklineCanvas.requestPaint()
                    }
                    return
                }
                if (forceReset)
                    resetHistory()
                else
                    sparklineCanvas.requestPaint()
            }

            function resetHistory()
            {
                var histLen = histChannelCount * histCapacity
                if (!history || history.length !== histLen)
                    history = new Float64Array(histLen)
                else
                    history.fill(0)

                if (!beatHistory || beatHistory.length !== histCapacity)
                    beatHistory = new Uint8Array(histCapacity)
                else
                    beatHistory.fill(0)

                if (!kickHistory || kickHistory.length !== histCapacity)
                    kickHistory = new Uint8Array(histCapacity)
                else
                    kickHistory.fill(0)

                writePos    = 0
                sampleCount = 0
                prevBeat    = false
                prevKick    = false
                sparklineCanvas.requestPaint()
            }

            // pull a value out of an audioTriggerObj QVariantList safely
            function readListVal(name, idx)
            {
                if (!audioTriggerObj) return 0
                var arr = audioTriggerObj[name]
                if (!arr || idx >= arr.length) return 0
                var v = +arr[idx]
                if (isNaN(v)) return 0
                return v
            }

            function pushSample()
            {
                if (!audioTriggerObj) return
                var pos = writePos
                var cap = histCapacity
                if (cap <= 0) return

                // Channels 0..8: onset descriptors (display-ready 0..1 from C++).
                // Falls back to onsetDescriptorValues (raw) if Phase A has not
                // shipped yet — values still rendered as-is per Iron Rule.
                var onsArr = audioTriggerObj.onsetDescriptorDisplay
                if (!onsArr || onsArr.length === 0) onsArr = audioTriggerObj.onsetDescriptorValues
                for (var i = 0; i < 9; i++)
                {
                    var v = (onsArr && i < onsArr.length) ? +onsArr[i] : 0
                    if (isNaN(v)) v = 0
                    history[i * cap + pos] = v
                }
                // Channel 9: pitch sparkline (display-ready, 0..1)
                history[9  * cap + pos] = +(audioTriggerObj.pitchDisplay || 0)
                // Channels 10..13: spectral (all already 0..1 from C++)
                history[10 * cap + pos] = +(audioTriggerObj.spectralCentroidDisplay || 0)
                history[11 * cap + pos] = +(audioTriggerObj.spectralFlatnessDisplay || 0)
                history[12 * cap + pos] = +(audioTriggerObj.fluxDisplay             || 0)
                history[13 * cap + pos] = +(audioTriggerObj.rmsDisplay              || 0)
                // Channels 14..15: TSS
                history[14 * cap + pos] = +(audioTriggerObj.tssTransientLevel       || 0)
                history[15 * cap + pos] = +(audioTriggerObj.tssSteadyLevel          || 0)

                // Beat / kick gridlines (edge-triggered)
                var bNow = !!audioTriggerObj.beatActive
                var kNow = !!audioTriggerObj.kickFired
                beatHistory[pos] = (bNow && !prevBeat) ? 1 : 0
                var kv = +(audioTriggerObj.kickValue || 0)
                if (kv < 0) kv = 0
                if (kv > 1) kv = 1
                kickHistory[pos] = (kNow && !prevKick) ? Math.round(255 * kv) : 0
                prevBeat = bNow
                prevKick = kNow

                writePos = (pos + 1) % cap
                if (sampleCount < cap) sampleCount++

                if (sparklineCanvas.visible && sparklineCanvas.available)
                    sparklineCanvas.dirty = true
            }

            // Repaint sparkline at a fixed 20 Hz instead of on every sample
            Timer
            {
                interval: 67
                running: sparklineCanvas.visible && sparklineCanvas.available
                repeat: true
                onTriggered:
                {
                    if (sparklineCanvas.dirty)
                    {
                        sparklineCanvas.dirty = false
                        sparklineCanvas.requestPaint()
                    }
                }
            }

            Connections
            {
                target: audioTriggerObj
                ignoreUnknownSignals: true
                function onAudioSnapshotChanged() { barsItem.pushSample() }
            }
            Connections
            {
                target: audioTriggerObj
                ignoreUnknownSignals: true
                function onConfigChanged() { barsItem.syncHistoryCapacity(false) }
            }

            ColumnLayout
            {
                id: stack
                anchors.fill: parent
                anchors.margins: 2
                spacing: 3

                // Cached lists — one C++ getter call per signal instead of N
                property var melCache: audioTriggerObj ? audioTriggerObj.melSpectrumProcessed : []
                property var mfccCache: audioTriggerObj ? audioTriggerObj.mfccCoeffs : []

                // ============================================================
                // §1 — Master MEL spectrum (40 Rectangle bars)
                // ============================================================
                Item
                {
                    id: melSection
                    Layout.fillWidth: true
                    Layout.preferredHeight: 32
                    Layout.minimumHeight: 0

                    Row
                    {
                        id: melBars
                        anchors.fill: parent
                        spacing: 1

                        Repeater
                        {
                            model: stack.melCache ? stack.melCache.length : 0

                            Rectangle
                            {
                                width: Math.max(1, (melBars.width - (melBars.children.length - 1)) /
                                                Math.max(1, melBars.children.length))
                                height: melBars.height
                                color: "transparent"

                                Rectangle
                                {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    height: parent.height * Math.max(0, Math.min(1,
                                        +stack.melCache[index] || 0))
                                    color: melBandColor(index)
                                }
                            }
                        }
                    }
                }

                // ============================================================
                // §2 — Beat / Bass / Lows / Mids / Highs power bars
                //      LedFx audio.py:1283-1342 — 4 raw freq_power slots +
                //      lows = (beat + bass) / 2 composite. Vertical bars,
                //      same width, height proportional to 0..1 power.
                // ============================================================
                Item
                {
                    id: powerSection
                    Layout.fillWidth: true
                    Layout.preferredHeight: 64
                    Layout.minimumHeight: 0

                    // Per-band colors. Warm → cool across the spectrum.
                    readonly property color colorBeat:  "#DC143C"  // crimson — sub-bass / kick (0-100 Hz)
                    readonly property color colorBass:  "#FF4500"  // orange-red — bass (100-250 Hz)
                    readonly property color colorLows:  "#FF8C00"  // orange — composite (beat+bass)/2
                    readonly property color colorMids:  "#9ACD32"  // yellow-green — mids (250-3000 Hz)
                    readonly property color colorHighs: "#00CED1"  // cyan — highs (3000-10000 Hz)

                    property var bands: [
                        { name: "Beat", color: powerSection.colorBeat,
                          value: audioTriggerObj ? +(audioTriggerObj.beatPower        || 0) : 0,
                          active: false },
                        { name: "Bass", color: powerSection.colorBass,
                          value: audioTriggerObj ? +(audioTriggerObj.bassPower        || 0) : 0,
                          active: false },
                        { name: "Lows", color: powerSection.colorLows,
                          value: audioTriggerObj ? +(audioTriggerObj.lowsPowerSliced  || 0) : 0,
                          active: audioTriggerObj ? audioTriggerObj.triggerLowActive  : false },
                        { name: "Mids", color: powerSection.colorMids,
                          value: audioTriggerObj ? +(audioTriggerObj.midsPowerSliced  || 0) : 0,
                          active: audioTriggerObj ? audioTriggerObj.triggerMidActive  : false },
                        { name: "High", color: powerSection.colorHighs,
                          value: audioTriggerObj ? +(audioTriggerObj.highsPowerSliced || 0) : 0,
                          active: audioTriggerObj ? audioTriggerObj.triggerHighActive : false }
                    ]

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
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 80
                    Layout.maximumHeight: 300

                    // Sub-section heights (in fractions of section height)
                    property real onsetFrac:   0.45
                    property real pitchFrac:   0.15
                    property real spectralFrac:0.25
                    property real tssFrac:     0.15

                    Canvas
                    {
                        id: sparklineCanvas
                        anchors.fill: parent
                        renderStrategy: Canvas.Cooperative
                        renderTarget: Canvas.FramebufferObject
                        // Force 1x pixel ratio — Retina 2x quadruples paint cost
                        // for a diagnostic sparkline where sub-pixel detail is unnecessary
                        layer.enabled: true
                        layer.smooth: false
                        layer.textureSize: Qt.size(Math.max(1, Math.round(width)),
                                                   Math.max(1, Math.round(height)))
                        property bool dirty: false

                        function laneCount(n)
                        {
                            if (n <= 0) return 0
                            if (n <= 3) return 1
                            if (n <= 6) return 2
                            return 3
                        }

                        function drawSampleCount(n, w)
                        {
                            if (n <= 0 || w <= 0) return 0
                            return Math.min(n, Math.max(1, Math.round(w)))
                        }

                        function downsampledSourceIndex(i, n, drawN)
                        {
                            return drawN <= 1 ? 0 : Math.round(i * (n - 1) / (drawN - 1))
                        }

                        function downsampledX(x0, w, i, drawN)
                        {
                            return drawN <= 1 ? x0 : x0 + (Math.max(0, w - 1) * i) / (drawN - 1)
                        }

                        function paintChannelTrace(ctx, ch, x0, y0, w, h, color)
                        {
                            var cap = barsItem.histCapacity
                            var n   = barsItem.sampleCount
                            var drawN = drawSampleCount(n, w)
                            if (drawN <= 0) return
                            var wp  = barsItem.writePos
                            if (drawN === 1)
                            {
                                var onlyIdx = (wp - n + cap) % cap
                                var onlyV = barsItem.history[ch * cap + onlyIdx]
                                if (onlyV < 0) onlyV = 0
                                if (onlyV > 1) onlyV = 1
                                ctx.fillStyle = color
                                ctx.fillRect(Math.round(x0), Math.round(y0 + h - h * onlyV), 1, 1)
                                return
                            }
                            ctx.strokeStyle = color
                            ctx.lineWidth = 1
                            ctx.beginPath()
                            for (var i = 0; i < drawN; i++)
                            {
                                var srcI = downsampledSourceIndex(i, n, drawN)
                                var idx = (wp - n + srcI + cap) % cap
                                var v = barsItem.history[ch * cap + idx]
                                if (v < 0) v = 0
                                if (v > 1) v = 1
                                var px = downsampledX(x0, w, i, drawN)
                                var py = y0 + h - h * v
                                if (i === 0) ctx.moveTo(px, py)
                                else         ctx.lineTo(px, py)
                            }
                            ctx.stroke()
                        }

                        function paintTriggerBars(ctx, ch, x0, y0, w, h, color, threshold)
                        {
                            var cap = barsItem.histCapacity
                            var n   = barsItem.sampleCount
                            var drawN = drawSampleCount(n, w)
                            if (drawN <= 0) return
                            var wp  = barsItem.writePos
                            ctx.fillStyle = color
                            for (var i = 0; i < drawN; i++)
                            {
                                var srcI = downsampledSourceIndex(i, n, drawN)
                                var idx = (wp - n + srcI + cap) % cap
                                var v = barsItem.history[ch * cap + idx]
                                if (v >= threshold)
                                {
                                    var px = downsampledX(x0, w, i, drawN)
                                    ctx.fillRect(Math.round(px), y0, 1, h)
                                }
                            }
                        }

                        function paintGridlines(ctx, x0, y0, w, h)
                        {
                            var cap = barsItem.histCapacity
                            var n   = barsItem.sampleCount
                            var drawN = drawSampleCount(n, w)
                            if (drawN <= 0) return
                            var wp  = barsItem.writePos

                            // Kick gridlines first (orange, 3px, fades right)
                            for (var i = 0; i < drawN; i++)
                            {
                                var srcI = downsampledSourceIndex(i, n, drawN)
                                var idx = (wp - n + srcI + cap) % cap
                                var ki  = barsItem.kickHistory[idx]
                                if (ki > 0)
                                {
                                    var px = Math.round(downsampledX(x0, w, i, drawN))
                                    var alpha = ki / 255
                                    if (alpha < 0) alpha = 0
                                    if (alpha > 1) alpha = 1
                                    for (var t = 0; t < 6; t++)
                                    {
                                        var a = alpha * (1 - t / 6)
                                        ctx.fillStyle = "rgba(255,153,51," + a.toFixed(3) + ")"
                                        ctx.fillRect(px + t, y0, 1, h)
                                    }
                                    ctx.fillStyle = "rgba(255,153,51," + alpha.toFixed(3) + ")"
                                    ctx.fillRect(px, y0, 3, h)
                                }
                            }

                            // Beat gridlines on top (white 1px)
                            ctx.fillStyle = "#ffffff"
                            for (var j = 0; j < drawN; j++)
                            {
                                var srcJ = downsampledSourceIndex(j, n, drawN)
                                var idx2 = (wp - n + srcJ + cap) % cap
                                if (barsItem.beatHistory[idx2] === 1)
                                {
                                    var px2 = Math.round(downsampledX(x0, w, j, drawN))
                                    ctx.fillRect(px2, y0, 1, h)
                                }
                            }
                        }

                        function paintLabel(ctx, x, y, text, color)
                        {
                            ctx.fillStyle = color
                            ctx.font = "9px sans-serif"
                            ctx.textBaseline = "top"
                            ctx.fillText(text, x, y)
                        }

                        onWidthChanged:  requestPaint()
                        onHeightChanged: requestPaint()
                        onVisibleChanged: if (visible) requestPaint()

                        onPaint:
                        {
                            var t0 = Date.now()
                            var ctx = getContext("2d")
                            ctx.reset()
                            ctx.fillStyle = "#0a0a0a"
                            ctx.fillRect(0, 0, width, height)

                            var leftGutter = 18
                            var x0 = leftGutter
                            var w  = width - leftGutter - 2

                            // Vertical layout
                            var hOnset    = Math.round(height * canvasSection.onsetFrac)
                            var hPitch    = Math.round(height * canvasSection.pitchFrac)
                            var hSpectral = Math.round(height * canvasSection.spectralFrac)
                            var hTss      = height - hOnset - hPitch - hSpectral

                            var yOnset    = 0
                            var yPitch    = yOnset + hOnset
                            var ySpectral = yPitch + hPitch
                            var yTss      = ySpectral + hSpectral

                            // Draw gridlines ONCE across full canvas height
                            paintGridlines(ctx, x0, 0, w, height)

                            // ----- §3 ONSETS (merged lanes) ----------------
                            var enabledArr = audioTriggerObj ? audioTriggerObj.onsetMethodsEnabled : null
                            var enabledIdx = []
                            if (enabledArr)
                            {
                                for (var ei = 0; ei < 9 && ei < enabledArr.length; ei++)
                                    if (enabledArr[ei]) enabledIdx.push(ei)
                            }
                            var lanes = laneCount(enabledIdx.length)
                            if (lanes > 0)
                            {
                                var laneH = Math.floor(hOnset / lanes)
                                // background
                                ctx.fillStyle = "#0d0d0d"
                                ctx.fillRect(x0, yOnset, w, hOnset)
                                // lane separators
                                ctx.strokeStyle = "#1f1f1f"
                                ctx.lineWidth = 1
                                for (var ls = 1; ls < lanes; ls++)
                                {
                                    ctx.beginPath()
                                    ctx.moveTo(x0, yOnset + ls * laneH + 0.5)
                                    ctx.lineTo(x0 + w, yOnset + ls * laneH + 0.5)
                                    ctx.stroke()
                                }
                                // distribute methods across lanes
                                for (var mi = 0; mi < enabledIdx.length; mi++)
                                {
                                    var laneIdx = Math.floor((mi * lanes) / enabledIdx.length)
                                    var laneY   = yOnset + laneIdx * laneH
                                    var ch      = enabledIdx[mi]
                                    var mode    = (barsItem.onsetMethodModes && ch < barsItem.onsetMethodModes.length)
                                                  ? barsItem.onsetMethodModes[ch] : 0
                                    if (mode === 1)
                                        paintTriggerBars(ctx, ch, x0, laneY, w, laneH,
                                                         onsetMethodColors[ch],
                                                         barsItem.triggerModeThreshold)
                                    else
                                        paintChannelTrace(ctx, ch, x0, laneY, w, laneH,
                                                          onsetMethodColors[ch])
                                }
                                // method letters in left gutter (top of each lane)
                                for (var li = 0; li < lanes; li++)
                                {
                                    var letters = ""
                                    for (var mi2 = 0; mi2 < enabledIdx.length; mi2++)
                                    {
                                        if (Math.floor((mi2 * lanes) / enabledIdx.length) === li)
                                        {
                                            if (letters.length > 0) letters += " "
                                            letters += onsetMethodLetters[enabledIdx[mi2]]
                                        }
                                    }
                                    paintLabel(ctx, 1, yOnset + li * laneH + 1, letters, "#aaaaaa")
                                }
                            }
                            else
                            {
                                paintLabel(ctx, 1, yOnset + 1, "Ons", "#666666")
                            }

                            // ----- §4 PITCH ----------------------------------
                            ctx.fillStyle = "#0d0d0d"
                            ctx.fillRect(x0, yPitch, w, hPitch)
                            paintChannelTrace(ctx, 9, x0, yPitch, w, hPitch, "#ffcc33")
                            paintLabel(ctx, 1, yPitch + 1, "Pit", "#aaaaaa")

                            // ----- §5 SPECTRAL (4 stacked sparklines) --------
                            var rowH = Math.floor(hSpectral / 4)
                            var spLabels = ["Cen", "Flt", "Flx", "RMS"]
                            var spColors = ["#66ccff", "#cc99ff", "#ff99cc", "#99ff99"]
                            for (var sr = 0; sr < 4; sr++)
                            {
                                var rowY = ySpectral + sr * rowH
                                var thisH = (sr === 3) ? (ySpectral + hSpectral - rowY) : rowH
                                ctx.fillStyle = (sr % 2 === 0) ? "#0d0d0d" : "#101010"
                                ctx.fillRect(x0, rowY, w, thisH)
                                paintChannelTrace(ctx, 10 + sr, x0, rowY, w, thisH, spColors[sr])
                                paintLabel(ctx, 1, rowY + 1, spLabels[sr], "#aaaaaa")
                            }

                            // ----- §7 TSS (transient + steady) ---------------
                            var tssRowH = Math.floor(hTss / 2)
                            ctx.fillStyle = "#0d0d0d"
                            ctx.fillRect(x0, yTss, w, hTss)
                            paintChannelTrace(ctx, 14, x0, yTss, w, tssRowH, "#ff6666")
                            paintChannelTrace(ctx, 15, x0, yTss + tssRowH, w, hTss - tssRowH, "#66ffff")
                            paintLabel(ctx, 1, yTss + 1, "Tr",  "#ff8888")
                            paintLabel(ctx, 1, yTss + tssRowH + 1, "St", "#88ffff")

                            var elapsed = Date.now() - t0
                            console.log("sparkline paint: " + elapsed + "ms, w=" + w + " sampleCount=" + barsItem.sampleCount)
                        }
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
                                // Even with no methods enabled, still allow gutter click to
                                // re-enable a method by cycling through the full set.
                                if (px < leftGutter)
                                    return { gutter: true, method: 0, lane: 0, lanes: 0 }
                                return null
                            }
                            var laneH = Math.floor(hOnset / lanes)
                            var laneIdx = Math.min(lanes - 1, Math.floor(py / Math.max(1, laneH)))
                            // Find first method assigned to this lane
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
                                sparklineCanvas.requestPaint()
                            }
                            else if (mouse.button === Qt.RightButton && info.method >= 0)
                            {
                                var modes = barsItem.onsetMethodModes.slice()
                                modes[info.method] = (modes[info.method] === 0) ? 1 : 0
                                barsItem.onsetMethodModes = modes
                                sparklineCanvas.requestPaint()
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
                            text: audioTriggerObj
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
                                model: audioTriggerObj ? audioTriggerObj.beatsPerBar : 0
                                Rectangle
                                {
                                    width: 7
                                    height: 7
                                    radius: 4
                                    color: (audioTriggerObj &&
                                            index === Math.floor(audioTriggerObj.barPhase))
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
