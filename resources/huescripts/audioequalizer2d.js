/*
  Q Light Controller Plus
  audioequalizer2d.js

  Copyright (c) QLC+ contributors

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

var testAlgo;

(function() {
    var algo = {};
    algo.apiVersion = 3;
    algo.name = "Audio Equalizer 2D";
    algo.author = "QLC+ contributors";
    algo.acceptColors = 5;
    algo.usesAudio = true;
    algo.properties = [];

    algo.presetMode = "Bars";
    algo.presetCenter = "Off";
    algo.presetBandCount = 8;
    algo.presetAggregation = "Mean";
    algo.presetSource = "Filtered";
    algo.presetColorMode = "Progressive";
    algo.presetPeakColor = "#ffffff";
    algo.presetPeakThickness = 1;
    algo.presetPeakDecay = 0.15;
    algo.presetSpin = 0.0;
    algo.presetSpinSource = "Lows";
    algo.presetMultiplier = 1.0;
    algo.presetDecay = 0.12;
    algo.presetRoll = 0.0;
    algo.properties.push("name:mode|type:list|display:Mode|values:Bars,Ring|write:setMode|read:getMode");
    algo.properties.push("name:center|type:list|display:Center|values:On,Off|write:setCenter|read:getCenter");
    algo.properties.push("name:bandCount|type:range|display:Band Count|values:1,32|write:setBandCount|read:getBandCount");
    algo.properties.push("name:aggregation|type:list|display:Aggregation|values:Mean,Max|write:setAggregation|read:getAggregation");
    algo.properties.push("name:source|type:list|display:Source|values:Filtered,Novelty|write:setSource|read:getSource");
    algo.properties.push("name:colorMode|type:list|display:Color Mode|values:Off,Solid,Progressive,Stretch|write:setColorMode|read:getColorMode");
    algo.properties.push("name:peakColor|type:string|display:Peak Color|write:setPeakColor|read:getPeakColor");
    algo.properties.push("name:peakThickness|type:range|display:Peak Thickness|values:1,4|write:setPeakThickness|read:getPeakThickness");
    algo.properties.push("name:peakDecay|type:float|values:0.01,0.5|display:Peak Decay|write:setPeakDecay|read:getPeakDecay");
    algo.properties.push("name:spin|type:float|values:0,2|display:Spin|write:setSpin|read:getSpin");
    algo.properties.push("name:spinSource|type:list|display:Spin Source|values:Beat,Bass,Lows,Mids,High|write:setSpinSource|read:getSpinSource");
    algo.properties.push("name:multiplier|type:float|values:0,4|display:Multiplier|write:setMultiplier|read:getMultiplier");
    algo.properties.push("name:decay|type:float|values:0.01,0.5|display:Decay|write:setDecay|read:getDecay");
    algo.properties.push("name:roll|type:float|values:0,1|display:Palette Roll|write:setRoll|read:getRoll");
    algo.setMode = function(v) { algo.presetMode = v === "Ring" ? "Ring" : "Bars"; };
    algo.getMode = function() { return algo.presetMode; };
    algo.setCenter = function(v) { algo.presetCenter = v === "On" ? "On" : "Off"; };
    algo.getCenter = function() { return algo.presetCenter; };
    algo.setBandCount = function(v) { algo.presetBandCount = Math.max(1, Math.min(32, parseInt(v) || 1)); };
    algo.getBandCount = function() { return algo.presetBandCount; };
    algo.setAggregation = function(v) { algo.presetAggregation = v === "Max" ? "Max" : "Mean"; };
    algo.getAggregation = function() { return algo.presetAggregation; };
    algo.setSource = function(v) { algo.presetSource = v === "Novelty" ? "Novelty" : "Filtered"; };
    algo.getSource = function() { return algo.presetSource; };
    algo.setColorMode = function(v) {
        var modes = ["Off", "Solid", "Progressive", "Stretch"];
        algo.presetColorMode = modes.indexOf(v) >= 0 ? v : "Progressive";
    };
    algo.getColorMode = function() { return algo.presetColorMode; };
    algo.setPeakColor = function(v) { algo.presetPeakColor = /^#[0-9a-f]{6}$/i.test(String(v)) ? String(v) : "#ffffff"; };
    algo.getPeakColor = function() { return algo.presetPeakColor; };
    algo.setPeakThickness = function(v) { algo.presetPeakThickness = Math.max(1, Math.min(4, parseInt(v) || 1)); };
    algo.getPeakThickness = function() { return algo.presetPeakThickness; };
    algo.setPeakDecay = function(v) { algo.presetPeakDecay = Math.max(0.01, Math.min(0.5, parseFloat(v) || 0.01)); };
    algo.getPeakDecay = function() { return algo.presetPeakDecay; };
    algo.setSpin = function(v) { algo.presetSpin = Math.max(0, Math.min(2, parseFloat(v) || 0)); };
    algo.getSpin = function() { return algo.presetSpin; };
    algo.setSpinSource = function(v) {
        var values = ["Beat", "Bass", "Lows", "Mids", "High"];
        algo.presetSpinSource = values.indexOf(v) >= 0 ? v : "Lows";
    };
    algo.getSpinSource = function() { return algo.presetSpinSource; };
    algo.setMultiplier = function(v) { algo.presetMultiplier = Math.max(0, Math.min(4, parseFloat(v) || 0)); };
    algo.getMultiplier = function() { return algo.presetMultiplier; };
    algo.setDecay = function(v) { algo.presetDecay = Math.max(0.01, Math.min(0.5, parseFloat(v) || 0.01)); };
    algo.getDecay = function() { return algo.presetDecay; };
    algo.setRoll = function(v) { algo.presetRoll = HSVUtil.clamp01(parseFloat(v) || 0); };
    algo.getRoll = function() { return algo.presetRoll; };
    algo.setBands = function(v) { algo.setBandCount(v); };
    algo.getBands = function() { return algo.getBandCount(); };

    var state = {
        geometryKey: "",
        audioKey: "",
        peaks: [],
        spinImpulse: 0,
        spinAngle: 0,
        rollPhase: 0
    };

    function sourceSamples(audio, count) {
        var bank = audio && audio.banks && audio.banks.full;
        var values = null;
        if (bank && bank.count) {
            if (algo.presetSource === "Novelty") values = bank.novelty || bank.processed || [];
            else values = bank.processed || bank.novelty || [];
        }
        if (!values || values.length === 0) values = [audio && audio.low || 0, audio && audio.mid || 0, audio && audio.high || 0];
        var out = HSVUtil.interpolate(values, Math.max(1, count));
        for (var i = 0; i < out.length; i++) out[i] = HSVUtil.clamp01(out[i]);
        return out;
    }

    function aggregateBands(samples, bands) {
        var out = new Array(bands);
        var base = Math.floor(samples.length / bands);
        var extra = samples.length % bands;
        var cursor = 0;
        for (var i = 0; i < bands; i++) {
            var len = base + (i < extra ? 1 : 0);
            var end = cursor + Math.max(1, len);
            var sum = 0;
            var peak = 0;
            var n = 0;
            for (var j = cursor; j < end && j < samples.length; j++) {
                var v = HSVUtil.clamp01(samples[j]);
                sum += v;
                if (v > peak) peak = v;
                n++;
            }
            out[i] = n > 0 ? (algo.presetAggregation === "Max" ? peak : sum / n) : 0;
            cursor = end;
        }
        return out;
    }

    function updatePeaks(levels, dt) {
        for (var i = 0; i < levels.length; i++) {
            if (levels[i] >= state.peaks[i]) state.peaks[i] = levels[i];
            else state.peaks[i] *= Math.pow(1 - algo.presetPeakDecay, Math.max(0, dt) * 60);
        }
    }

    function updateSpin(audio, dt) {
        var target = HSVUtil.powerForRange(audio, algo.presetSpinSource, false) * algo.presetMultiplier;
        var alpha = target > state.spinImpulse ? 0.99 : algo.presetDecay;
        state.spinImpulse += (target - state.spinImpulse) * alpha;
        if (algo.presetSpin > 0)
            state.spinAngle = HSVUtil.mod1(state.spinAngle + state.spinImpulse * Math.max(0, dt) * algo.presetSpin * 0.5);
    }

    function updateRoll(audio, dt) {
        if (audio && audio.timing && isFinite(audio.timing.absoluteSeconds)) {
            state.rollPhase = HSVUtil.mod1(audio.timing.absoluteSeconds * algo.presetRoll);
            return;
        }
        state.rollPhase = HSVUtil.mod1(state.rollPhase + Math.max(0, dt) * algo.presetRoll);
    }

    function gradientColor(coord) {
        return HSVUtil.gradientLedfxAt(algo.colors || [{ h: 0.6, s: 1, v: 1 }], HSVUtil.mod1(coord + state.rollPhase));
    }

    function modeColor(mode, bandIndex, bands, level, coord) {
        if (mode === "Off") return gradientColor(bands <= 1 ? 0 : bandIndex / bands);
        if (mode === "Solid") return gradientColor(level);
        return gradientColor(coord);
    }

    function pointInTriangle(px, py, a, b, c) {
        var v0x = c.x - a.x;
        var v0y = c.y - a.y;
        var v1x = b.x - a.x;
        var v1y = b.y - a.y;
        var v2x = px - a.x;
        var v2y = py - a.y;
        var dot00 = v0x * v0x + v0y * v0y;
        var dot01 = v0x * v1x + v0y * v1y;
        var dot02 = v0x * v2x + v0y * v2y;
        var dot11 = v1x * v1x + v1y * v1y;
        var dot12 = v1x * v2x + v1y * v2y;
        var denom = (dot00 * dot11 - dot01 * dot01);
        if (Math.abs(denom) < 1e-9) return false;
        var inv = 1 / denom;
        var u = (dot11 * dot02 - dot01 * dot12) * inv;
        var v = (dot00 * dot12 - dot01 * dot02) * inv;
        return u >= 0 && v >= 0 && (u + v) <= 1;
    }

    function lerpPoint(a, b, t) {
        return { x: a.x + (b.x - a.x) * t, y: a.y + (b.y - a.y) * t };
    }

    function calcRingGeometry(width, height, bands, rotation) {
        var cx = width / 2;
        var cy = height / 2;
        var verts = new Array(bands + 1);
        for (var i = 0; i <= bands; i++) {
            var angle = (2 * Math.PI * i / Math.max(1, bands)) + rotation;
            verts[i] = {
                x: cx + ((width - 1) * 0.5) * Math.cos(angle),
                y: cy + ((height - 1) * 0.5) * Math.sin(angle)
            };
        }
        var mids = new Array(bands + 1);
        for (var j = 0; j < bands; j++) mids[j] = lerpPoint(verts[j], verts[j + 1], 0.5);
        mids[bands] = { x: mids[0].x, y: mids[0].y };
        return { center: { x: cx, y: cy }, verts: verts, mids: mids };
    }

    function setPixel(map, width, x, y, color) {
        if (x < 0 || y < 0) return;
        var o = (y * width + x) * 3;
        map[o] = color.h;
        map[o + 1] = color.s;
        map[o + 2] = color.v;
    }

    function paintSquare(map, width, height, x, y, color, size) {
        var start = -Math.floor((size - 1) * 0.5);
        var end = Math.ceil((size - 1) * 0.5);
        for (var dy = start; dy <= end; dy++) {
            var yy = y + dy;
            if (yy < 0 || yy >= height) continue;
            for (var dx = start; dx <= end; dx++) {
                var xx = x + dx;
                if (xx < 0 || xx >= width) continue;
                setPixel(map, width, xx, yy, color);
            }
        }
    }

    function drawLine(map, width, height, a, b, color, size) {
        var x0 = Math.round(a.x);
        var y0 = Math.round(a.y);
        var x1 = Math.round(b.x);
        var y1 = Math.round(b.y);
        var dx = Math.abs(x1 - x0);
        var sx = x0 < x1 ? 1 : -1;
        var dy = -Math.abs(y1 - y0);
        var sy = y0 < y1 ? 1 : -1;
        var err = dx + dy;
        while (true) {
            paintSquare(map, width, height, x0, y0, color, size);
            if (x0 === x1 && y0 === y1) break;
            var e2 = err * 2;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }

    function barCoord(y, level, height) {
        var center = algo.presetCenter === "On";
        var h = Math.max(height - 1, 1);
        var volumeScaled = Math.floor(height * level);
        if (center) {
            var half = Math.floor(height / 2);
            var norm = algo.presetColorMode === "Stretch"
                ? Math.max(Math.floor(volumeScaled * 0.5), 1)
                : Math.max(half, 1);
            return HSVUtil.clamp01(Math.abs(y - half) / norm);
        }
        var n = algo.presetColorMode === "Stretch"
            ? Math.max(volumeScaled, 1)
            : h;
        return HSVUtil.clamp01((h - y) / n);
    }

    function ringCoord(x, y, width, height, level) {
        var cx = width / 2;
        var cy = height / 2;
        var rx = Math.max(width / 2, 1);
        var ry = Math.max(height / 2, 1);
        var dist = Math.sqrt(Math.pow((x - cx) / rx, 2) + Math.pow((y - cy) / ry, 2));
        dist = HSVUtil.clamp01(dist);
        if (algo.presetColorMode === "Stretch") {
            var base = algo.presetCenter === "On" ? dist : (1 - dist);
            return HSVUtil.clamp01(base / Math.max(level, 0.01));
        }
        return algo.presetCenter === "On" ? dist : (1 - dist);
    }

    function drawBars(map, width, height, levels, peakColor) {
        var bands = levels.length;
        var half = Math.floor(height / 2);
        for (var i = 0; i < bands; i++) {
            var start = Math.floor((width / bands) * i);
            var end = Math.max(start, Math.floor((width / bands) * (i + 1) - 1));
            var level = levels[i];
            var volumeScaled = Math.floor(height * level);
            var bottom;
            var top;
            if (algo.presetCenter === "On") {
                bottom = Math.max(0, half - Math.floor(volumeScaled / 2));
                top = Math.min(height - 1, half + Math.floor(volumeScaled / 2));
            } else {
                bottom = Math.max(0, (height - 1) - volumeScaled);
                top = height - 1;
            }
            for (var y = bottom; y <= top; y++) {
                var coord = barCoord(y, level, height);
                var color = modeColor(algo.presetColorMode, i, bands, level, coord);
                for (var x = start; x <= end && x < width; x++) setPixel(map, width, x, y, color);
            }
        }

        for (var p = 0; p < bands; p++) {
            var ps = Math.floor((width / bands) * p);
            var pe = Math.max(ps, Math.floor((width / bands) * (p + 1) - 1));
            if (algo.presetCenter === "On") {
                var peakScaledC = Math.floor(half * state.peaks[p]);
                var low = Math.max(0, half - peakScaledC - algo.presetPeakThickness + 1);
                var high = Math.min(height - 1, half + peakScaledC + algo.presetPeakThickness - 1);
                for (var yc = low; yc <= high; yc++)
                    for (var xc = ps; xc <= pe && xc < width; xc++) setPixel(map, width, xc, yc, peakColor);
            } else {
                var peakScaled = Math.floor((height - 1) * state.peaks[p]);
                var markerTop = Math.max(0, (height - 1) - peakScaled - algo.presetPeakThickness + 1);
                var markerBottom = Math.min(height - 1, (height - 1) - peakScaled);
                for (var y0 = markerTop; y0 <= markerBottom; y0++)
                    for (var x0 = ps; x0 <= pe && x0 < width; x0++) setPixel(map, width, x0, y0, peakColor);
            }
        }
    }

    function drawRing(map, width, height, levels, peakColor) {
        var bands = levels.length;
        var geometry = calcRingGeometry(width, height, bands, state.spinAngle * 2 * Math.PI);

        for (var i = 0; i < bands; i++) {
            var tri;
            if (algo.presetCenter === "On") {
                tri = [
                    lerpPoint(geometry.center, geometry.verts[i], levels[i]),
                    lerpPoint(geometry.center, geometry.verts[i + 1], levels[i]),
                    geometry.center
                ];
            } else {
                tri = [
                    geometry.verts[i],
                    geometry.verts[i + 1],
                    lerpPoint(geometry.mids[i], geometry.center, levels[i])
                ];
            }

            var minX = Math.max(0, Math.floor(Math.min(tri[0].x, tri[1].x, tri[2].x)));
            var maxX = Math.min(width - 1, Math.ceil(Math.max(tri[0].x, tri[1].x, tri[2].x)));
            var minY = Math.max(0, Math.floor(Math.min(tri[0].y, tri[1].y, tri[2].y)));
            var maxY = Math.min(height - 1, Math.ceil(Math.max(tri[0].y, tri[1].y, tri[2].y)));

            for (var y = minY; y <= maxY; y++) {
                for (var x = minX; x <= maxX; x++) {
                    if (!pointInTriangle(x + 0.5, y + 0.5, tri[0], tri[1], tri[2])) continue;
                    var coord = ringCoord(x + 0.5, y + 0.5, width, height, levels[i]);
                    var color = modeColor(algo.presetColorMode, i, bands, levels[i], coord);
                    setPixel(map, width, x, y, color);
                }
            }
        }

        for (var p = 0; p < bands; p++) {
            var a;
            var b;
            var nextPeak = state.peaks[(p + 1) % bands];
            if (algo.presetCenter === "On") {
                a = lerpPoint(geometry.center, geometry.mids[p], state.peaks[p]);
                b = lerpPoint(geometry.center, geometry.mids[p + 1], nextPeak);
            } else {
                a = lerpPoint(geometry.mids[p], geometry.center, state.peaks[p]);
                b = lerpPoint(geometry.mids[p + 1], geometry.center, nextPeak);
            }
            drawLine(map, width, height, a, b, peakColor, Math.max(1, algo.presetPeakThickness));
        }
    }

    algo.rgbMapStepCount = function() { return 1; };
    algo.rgbMapSetColors = function() {};
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step, audio) {
        var map = HSVUtil.createMap(width, height);
        var bands = Math.max(1, Math.min(algo.presetBandCount, Math.max(1, width * height)));
        var geometryKey = [width, height, bands].join("|");
        var audioKey = HSVUtil.audioIdentityKey(audio, state.audioKey);
        if (state.geometryKey !== geometryKey || state.audioKey !== audioKey || state.peaks.length !== bands) {
            state.geometryKey = geometryKey;
            state.audioKey = audioKey;
            state.peaks = new Array(bands).fill(0);
            state.spinImpulse = 0;
            state.spinAngle = 0;
            state.rollPhase = 0;
        }

        var dt = HSVUtil.audioSeconds(audio);
        updateRoll(audio, dt);
        updateSpin(audio, dt);
        var samples = sourceSamples(audio, Math.max(1, width * height));
        var levels = aggregateBands(samples, bands);
        updatePeaks(levels, dt);

        var peakRgb = HSVUtil.parseHexRgb(algo.presetPeakColor);
        var peakColor = HSVUtil.rgbToHsv(peakRgb[0], peakRgb[1], peakRgb[2]);

        if (algo.presetMode === "Ring") drawRing(map, width, height, levels, peakColor);
        else drawBars(map, width, height, levels, peakColor);

        return map;
    };

    testAlgo = algo;
    return algo;
})();
