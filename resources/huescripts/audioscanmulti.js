/*
  Q Light Controller Plus
  audioscanmulti.js

  Copyright (c) QLC+ contributors
  Ported from LedFx "Scan Multi" effect (MIT License)

  Three independent scanners — low / mid / high — each driven by its own
  power band, additively blended on a 1D strip and replicated to 2D.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

var testAlgo;

(
  function () {
    var algo = new Object;
    algo.apiVersion = 3;
    algo.name = "Audio Scan Multi";
    algo.author = "Ported from LedFx";
    algo.acceptColors = 3;
    algo.usesAudio = true;
    algo.properties = new Array();
    algo.presetMode = "Artistic";
    algo.properties.push("name:mode|type:list|display:Response|values:Artistic,LedFx Scan Multi|write:setMode|read:getMode");
    algo.setMode = function(v) { algo.presetMode = v === "LedFx Scan Multi" ? v : "Artistic"; };
    algo.getMode = function() { return algo.presetMode; };

    algo.presetSpeed = 0.5;
    algo.properties.push(
      "name:presetSpeed|type:float|display:Speed (cyc/beat)|" +
      "write:setSpeed|read:getSpeed");

    algo.presetWidth = 10;
    algo.properties.push(
      "name:presetWidth|type:range|display:Scan Width|" +
      "values:1,50|write:setWidth|read:getWidth");

    algo.presetMultiplier = 100;
    algo.properties.push(
      "name:presetMultiplier|type:range|display:Multiplier|" +
      "values:1,500|write:setMultiplier|read:getMultiplier");

    algo.presetBounce = 1;
    algo.properties.push(
      "name:presetBounce|type:list|display:Bounce|" +
      "values:Yes,No|write:setBounce|read:getBounce");

    algo.presetColorIntensity = 1;
    algo.properties.push(
      "name:presetColorIntensity|type:list|display:Color Intensity|" +
      "values:Yes,No|write:setColorIntensity|read:getColorIntensity");

    algo.presetAxis = "Horizontal";
    algo.properties.push(
      "name:presetAxis|type:list|display:Axis|" +
      "values:Horizontal,Vertical|write:setAxis|read:getAxis");
    algo.presetMatrixLayout = "Matrix";
    algo.properties.push(
      "name:matrixLayout|type:list|display:Layout|" +
      "values:Matrix,Repeated rows|write:setMatrixLayout|read:getMatrixLayout");
    algo.presetSourceMode = "Power";
    algo.properties.push(
      "name:presetSourceMode|type:list|display:Source|" +
      "values:Power,Melbank|write:setSourceMode|read:getSourceMode");
    algo.presetMelbank = "Processed";
    algo.properties.push(
      "name:presetMelbank|type:list|display:Melbank|" +
      "values:Processed,Novelty|write:setMelbank|read:getMelbank");
    algo.presetFilter = "On";
    algo.properties.push(
      "name:presetFilter|type:list|display:Filter|" +
      "values:Off,On|write:setFilter|read:getFilter");
    algo.presetAttack = 0.2;
    algo.properties.push(
      "name:presetAttack|type:float|values:0,1|display:Attack|" +
      "write:setAttack|read:getAttack");
    algo.presetDecay = 0.05;
    algo.properties.push(
      "name:presetDecay|type:float|values:0,1|display:Decay|" +
      "write:setDecay|read:getDecay");

    algo.setSpeed = function(_v) { algo.presetSpeed = parseFloat(_v); };
    algo.getSpeed = function() { return algo.presetSpeed; };
    algo.setWidth = function(_v) { algo.presetWidth = parseFloat(_v); };
    algo.getWidth = function() { return algo.presetWidth; };
    algo.setMultiplier = function(_v) { algo.presetMultiplier = parseFloat(_v); };
    algo.getMultiplier = function() { return algo.presetMultiplier; };
    algo.setBounce = function(_v) { algo.presetBounce = (_v === "Yes") ? 1 : 0; };
    algo.getBounce = function() { return algo.presetBounce ? "Yes" : "No"; };
    algo.setColorIntensity = function(_v) { algo.presetColorIntensity = (_v === "Yes") ? 1 : 0; };
    algo.getColorIntensity = function() { return algo.presetColorIntensity ? "Yes" : "No"; };
    algo.presetGradient = "No";
    algo.properties.push(
      "name:presetGradient|type:list|display:Gradient|" +
      "values:No,Yes|write:setGradient|read:getGradient");
    algo.setGradient = function(_v) { algo.presetGradient = _v === "Yes" ? "Yes" : "No"; };
    algo.getGradient = function() { return algo.presetGradient; };
    algo.setAxis = function(_v) { algo.presetAxis = _v; };
    algo.getAxis = function() { return algo.presetAxis; };
    algo.setMatrixLayout = function(_v) { algo.presetMatrixLayout = _v === "Repeated rows" ? "Repeated rows" : "Matrix"; };
    algo.getMatrixLayout = function() { return algo.presetMatrixLayout; };
    algo.setSourceMode = function(_v) { algo.presetSourceMode = _v === "Melbank" ? "Melbank" : "Power"; };
    algo.getSourceMode = function() { return algo.presetSourceMode; };
    algo.setMelbank = function(_v) { algo.presetMelbank = _v === "Novelty" ? "Novelty" : "Processed"; };
    algo.getMelbank = function() { return algo.presetMelbank; };
    algo.setFilter = function(_v) { algo.presetFilter = _v === "Off" ? "Off" : "On"; };
    algo.getFilter = function() { return algo.presetFilter; };
    algo.setAttack = function(_v) { algo.presetAttack = Math.max(0, Math.min(1, parseFloat(_v) || 0)); };
    algo.getAttack = function() { return algo.presetAttack; };
    algo.setDecay = function(_v) { algo.presetDecay = Math.max(0, Math.min(1, parseFloat(_v) || 0)); };
    algo.getDecay = function() { return algo.presetDecay; };

    algo.presetSmoothing = 5;
    algo.properties.push(
      "name:presetSmoothing|type:range|display:Smoothing|" +
      "values:1,10|write:setSmoothing|read:getSmoothing");
    algo.setSmoothing = function(_v) { algo.presetSmoothing = parseInt(_v); };
    algo.getSmoothing = function() { return algo.presetSmoothing; };
    var readOutputControls = HSVUtil.attachStripTransformControls(algo, {
        display: "",
        defaults: {
            flip: "Off",
            mirror: "Off",
            backgroundMode: "Off",
            backgroundColor: "#000000",
            backgroundBrightness: 1,
            brightness: 1
        }
    });

    var smoothPow = [0, 0, 0];

    // Bar-level build-up / release state (shared across all 3 scanners)
    var barEnergy = 0;
    var peakEnergy = 0;
    var releaseFlash = 0;

    algo.scans = [
        { pos: 0, returning: false },
        { pos: 0, returning: false },
        { pos: 0, returning: false }
    ];
    algo.lastN = 0;
    var lastMode = "";
    var lastIdentityKey = "";

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };

    function thirdMeans(audio, useNovelty) {
        var bank = audio && audio.banks && audio.banks.full;
        if (!bank || !bank.count) return [audio.low || 0, audio.mid || 0, audio.high || 0];
        var values = useNovelty ? bank.novelty : bank.processed;
        if (!Array.isArray(values) || values.length === 0) return [audio.low || 0, audio.mid || 0, audio.high || 0];
        var count = Math.min(bank.count, values.length);
        var a = Math.floor(count * 0.2);
        var b = Math.floor(count * 0.5);
        function mean(start, end) {
            if (end <= start) return 0;
            var sum = 0;
            for (var i = start; i < end; i++) sum += isFinite(values[i]) ? values[i] : 0;
            return sum / (end - start);
        }
        return [HSVUtil.clamp01(mean(0, a)), HSVUtil.clamp01(mean(a, b)), HSVUtil.clamp01(mean(b, count))];
    }

    // Additive blend: keep brighter hue, sum values
    function drawScan(strip, n, startPos, scanW, color) {
        var start = Math.floor(startPos);
        for (var s = 0; s < scanW; s++) {
            var idx = start + s;
            if (idx < 0 || idx >= n) continue;
            var existing = strip[idx];
            var newV = Math.min(1, existing.v + color.v);
            if (existing.v < 0.001) {
                strip[idx] = {h: color.h, s: color.s, v: newV};
            } else {
                var total = existing.v + color.v;
                var t = color.v / Math.max(0.001, total);
                var dh = color.h - existing.h;
                if (dh > 0.5) dh -= 1;
                else if (dh < -0.5) dh += 1;
                var h = existing.h + t * dh;
                h = h - Math.floor(h);
                strip[idx] = {
                    h: h,
                    s: existing.s * (1 - t) + color.s * t,
                    v: newV
                };
            }
        }
    }

    function drawScanRgb(strip, n, startPos, scanW, rgb) {
        var start = Math.floor(startPos);
        for (var s = 0; s < scanW; s++) {
            var idx = start + s;
            if (idx < 0 || idx >= n) continue;
            strip[idx][0] = HSVUtil.clamp01(strip[idx][0] + rgb[0]);
            strip[idx][1] = HSVUtil.clamp01(strip[idx][1] + rgb[1]);
            strip[idx][2] = HSVUtil.clamp01(strip[idx][2] + rgb[2]);
        }
    }

    function overlap(a0, a1, b0, b1) {
        var lo = Math.max(a0, b0);
        var hi = Math.min(a1, b1);
        return Math.max(0, hi - lo);
    }

    function drawScanMatrixRgb(strip, width, height, n, startPos, scanW, rgb, axis, band) {
        var intervalStart = Math.max(0, Math.min(n, startPos));
        var intervalEnd = Math.max(0, Math.min(n, startPos + scanW));
        if (intervalEnd <= intervalStart) return;

        var horizontal = axis !== "Vertical";
        var L = horizontal ? width : height;
        var C = horizontal ? height : width;
        var travelStart = intervalStart * L / n;
        var travelEnd = intervalEnd * L / n;
        var laneCenter = (band + 0.5) * C / 3;
        var laneExtent = C / 6;
        var laneStart = laneCenter - laneExtent;
        var laneEnd = laneCenter + laneExtent;

        for (var y = 0; y < height; y++) {
            for (var x = 0; x < width; x++) {
                var travelCell = horizontal ? x : y;
                var crossCell = horizontal ? y : x;
                var travelCoverage = overlap(travelCell, travelCell + 1, travelStart, travelEnd);
                if (travelCoverage <= 0) continue;
                var laneCoverage = overlap(crossCell, crossCell + 1, laneStart, laneEnd);
                if (laneCoverage <= 0) continue;
                var weight = travelCoverage * laneCoverage;
                var idx = y * width + x;
                strip[idx][0] = HSVUtil.clamp01(strip[idx][0] + rgb[0] * weight);
                strip[idx][1] = HSVUtil.clamp01(strip[idx][1] + rgb[1] * weight);
                strip[idx][2] = HSVUtil.clamp01(strip[idx][2] + rgb[2] * weight);
            }
        }
    }

    function renderInternal(width, height, audio) {
        var map = HSVUtil.createMap(width, height);
        if (!audio) return map;
        var ledFxMode = algo.presetMode === "LedFx Scan Multi";
        var matrixLedFx = ledFxMode && width > 1 && height > 1;
        var n = ledFxMode ? (width * height) : ((algo.presetAxis === "Vertical") ? height : width);
        if (n <= 0) return map;
        var identityKey = HSVUtil.audioIdentityKey(audio, lastIdentityKey);
        var identityChanged = identityKey !== lastIdentityKey;
        lastIdentityKey = identityKey;
        if (identityChanged) {
            for (var reset = 0; reset < 3; reset++) {
                algo.scans[reset].pos = 0;
                algo.scans[reset].returning = false;
                smoothPow[reset] = 0;
            }
            barEnergy = 0;
            peakEnergy = 0;
            releaseFlash = 0;
        }

        if (algo.lastN !== n || lastMode !== algo.presetMode) {
            for (var i = 0; i < 3; i++) {
                algo.scans[i].pos = 0;
                algo.scans[i].returning = false;
            }
            algo.lastN = n;
            lastMode = algo.presetMode;
        }

        var dt = ledFxMode ? HSVUtil.audioSeconds(audio) : (audio.dt * 60.0 / audio.bpm);
        var bpmEff = (audio.bpm > 0) ? audio.bpm : 120;
        var beatsPerSec = bpmEff / 60.0;
        var multiplier = algo.presetMultiplier / 100.0;
        var scanW = Math.max(1, Math.round(n * algo.presetWidth / 100.0));
        var stepPerSec = ledFxMode
            ? (n / 100.0 * Math.max(0, Math.min(100, algo.presetSpeed)))
            : (Math.max(1, n - scanW) * algo.presetSpeed * beatsPerSec);
        var bounce = algo.presetBounce === 1;
        var colorIntensity = algo.presetColorIntensity === 1;
        var useGradient = algo.presetGradient === "Yes";
        var stops = (algo.colors && algo.colors.length > 0) ? algo.colors : [
            {h: 0.0, s: 1.0, v: 1.0},
            {h: 0.333, s: 1.0, v: 1.0},
            {h: 0.667, s: 1.0, v: 1.0}
        ];
        var bandColors = (algo.colors && algo.colors.length >= 3) ? algo.colors : [
            {h: 0.0,   s: 1.0, v: 1.0},
            {h: 0.333, s: 1.0, v: 1.0},
            {h: 0.667, s: 1.0, v: 1.0}
        ];

        var strip = new Array(n);
        var stripRgb = ledFxMode ? new Array(n) : null;
        for (var p = 0; p < n; p++) {
            strip[p] = {h: 0, s: 0, v: 0};
            if (stripRgb) stripRgb[p] = [0, 0, 0];
        }
        var maxStart = n - scanW;
        if (maxStart < 0) maxStart = 0;

        var smoothing = algo.presetSmoothing / 10.0;
        var riseAlpha = 0.5 * (1 - smoothing) + 0.05;
        var decayAlpha = 0.02 + 0.03 * (1 - smoothing);
        var rawPows = ledFxMode && algo.presetSourceMode === "Melbank"
            ? thirdMeans(audio, algo.presetMelbank === "Novelty")
            : [audio.low, audio.mid, audio.high];
        var effective = [0, 0, 0];
        for (var sb = 0; sb < 3; sb++) {
            var source = HSVUtil.clamp01(rawPows[sb]) * 2;
            if (ledFxMode) {
                if (algo.presetFilter === "Off") {
                    smoothPow[sb] = source;
                } else {
                    var coeff = source > smoothPow[sb] ? algo.presetAttack : algo.presetDecay;
                    smoothPow[sb] += coeff * (source - smoothPow[sb]);
                }
                effective[sb] = smoothPow[sb];
                continue;
            }
            var sa = rawPows[sb] > smoothPow[sb] ? riseAlpha : decayAlpha;
            smoothPow[sb] += sa * (rawPows[sb] - smoothPow[sb]);
            effective[sb] = smoothPow[sb];
        }

        if (!ledFxMode) {
            var rawEnergy = (rawPows[0] + rawPows[1] * 0.5 + rawPows[2] * 0.3) / 1.8;
            barEnergy += rawEnergy * dt;
            if (barEnergy > peakEnergy) peakEnergy = barEnergy;
            if (audio.downbeat) {
                releaseFlash = Math.min(1, peakEnergy * 0.5);
                barEnergy = 0;
                peakEnergy = 0;
            }
            releaseFlash *= 0.85;
        }

        var cumulative = 1.0;
        for (var b = 0; b < 3; b++) {
            var scan = algo.scans[b];
            var power = ledFxMode ? effective[b] : rawPows[b];
            var briPower = ledFxMode ? effective[b] : smoothPow[b];
            var bar = power * multiplier;
            var stepScale = ledFxMode ? cumulative : 1.0;
            var stepSize = dt * stepPerSec * bar * stepScale;

            if (scan.returning) scan.pos -= stepSize;
            else scan.pos += stepSize;

            if (bounce) {
                if (scan.pos > maxStart) { scan.pos = maxStart; scan.returning = true; }
                if (scan.pos < 0) { scan.pos = 0; scan.returning = false; }
            } else {
                scan.pos = ((scan.pos % n) + n) % n;
                if (scan.pos > maxStart) scan.pos = scan.pos - n;
            }

            var baseColor = useGradient
                ? HSVUtil.gradientLedfxAt(stops, n <= 1 ? 0 : HSVUtil.mod1(scan.pos / n))
                : bandColors[b];
            var briBoost = colorIntensity ? Math.min(1, briPower) : 1.0;
            var cv = baseColor.v * Math.min(1, briBoost + (ledFxMode ? 0 : releaseFlash * 0.5));
            if (ledFxMode) {
                var colorRgb = HSVUtil.hsvToRgb(baseColor.h, baseColor.s, cv);
                if (matrixLedFx)
                    drawScanMatrixRgb(stripRgb, width, height, n, scan.pos, scanW, colorRgb, algo.presetAxis, b);
                else
                    drawScanRgb(stripRgb, n, scan.pos, scanW, colorRgb);
            } else {
                drawScan(strip, n, scan.pos, scanW, {h: baseColor.h, s: baseColor.s, v: cv});
            }
            if (ledFxMode)
                cumulative *= bar;
        }

        if (ledFxMode) {
            for (var i = 0; i < n; i++) {
                var hsv = HSVUtil.rgbToHsvUnclipped(stripRgb[i][0], stripRgb[i][1], stripRgb[i][2]);
                var o = i * 3;
                map[o] = hsv.h;
                map[o + 1] = hsv.s;
                map[o + 2] = HSVUtil.clamp01(hsv.v);
            }
            return HSVUtil.applyStripTransforms(map, width, height, readOutputControls());
        }

        if (algo.presetAxis === "Vertical") {
            for (var y = 0; y < height; y++) {
                var c = strip[y];
                for (var x = 0; x < width; x++) {
                    var i3 = (y * width + x) * 3;
                    map[i3] = c.h; map[i3+1] = c.s; map[i3+2] = c.v;
                }
            }
            return map;
        }
        for (var y2 = 0; y2 < height; y2++) {
            for (var x2 = 0; x2 < width; x2++) {
                var c2 = strip[x2];
                var i3 = (y2 * width + x2) * 3;
                map[i3] = c2.h; map[i3+1] = c2.s; map[i3+2] = c2.v;
            }
        }
        return map;
    };

    algo.rgbMap = function(width, height, rgb, step, audio) {
        var repeatedRows = algo.presetMode === "LedFx Scan Multi" &&
            algo.presetMatrixLayout === "Repeated rows" &&
            width > 1 && height > 1;
        if (!repeatedRows)
            return renderInternal(width, height, audio);

        var row = renderInternal(width, 1, audio);
        var map = HSVUtil.createMap(width, height);
        var rowSize = width * 3;
        for (var y = 0; y < height; y++)
            map.set(row, y * rowSize);
        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
