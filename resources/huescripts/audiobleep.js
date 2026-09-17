/*
  Q Light Controller Plus
  audiobleep.js

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
    algo.name = "Audio Bleep";
    algo.author = "QLC+ contributors";
    algo.acceptColors = 5;
    algo.usesAudio = true;
    algo.properties = [];

    algo.presetSource = "High";
    algo.presetMode = "Points";
    algo.presetMirror = "No";
    algo.presetColorBy = "Power";
    algo.presetPoints = 30;
    algo.presetScrollSeconds = 2.0;
    algo.presetLineWidth = 1;
    algo.properties.push("name:source|type:list|display:Source|values:Beat,Bass,Lows,Mids,High|write:setSource|read:getSource");
    algo.properties.push("name:mode|type:list|display:Mode|values:Points,Lines,Fill|write:setMode|read:getMode");
    algo.properties.push("name:mirror|type:list|display:Mirror|values:Yes,No|write:setMirror|read:getMirror");
    algo.properties.push("name:colorBy|type:list|display:Color By|values:Power,Time|write:setColorBy|read:getColorBy");
    algo.properties.push("name:points|type:range|display:Points|values:1,512|write:setPoints|read:getPoints");
    algo.properties.push("name:scrollSeconds|type:float|values:0.05,10|display:Scroll Seconds|write:setScrollSeconds|read:getScrollSeconds");
    algo.properties.push("name:lineWidth|type:range|display:Line Width|values:1,8|write:setLineWidth|read:getLineWidth");
    algo.setSource = function(v) { algo.presetSource = ["Beat", "Bass", "Lows", "Mids", "High"].indexOf(v) >= 0 ? v : "High"; };
    algo.getSource = function() { return algo.presetSource; };
    algo.setMode = function(v) { algo.presetMode = ["Points", "Lines", "Fill"].indexOf(v) >= 0 ? v : "Points"; };
    algo.getMode = function() { return algo.presetMode; };
    algo.setMirror = function(v) { algo.presetMirror = v === "Yes" ? "Yes" : "No"; };
    algo.getMirror = function() { return algo.presetMirror; };
    algo.setColorBy = function(v) { algo.presetColorBy = v === "Time" ? "Time" : "Power"; };
    algo.getColorBy = function() { return algo.presetColorBy; };
    algo.setPoints = function(v) { algo.presetPoints = Math.max(1, Math.min(512, parseInt(v) || 1)); };
    algo.getPoints = function() { return algo.presetPoints; };
    algo.setScrollSeconds = function(v) { algo.presetScrollSeconds = Math.max(0.05, Math.min(10, parseFloat(v) || 0.05)); };
    algo.getScrollSeconds = function() { return algo.presetScrollSeconds; };
    algo.setLineWidth = function(v) { algo.presetLineWidth = Math.max(1, Math.min(8, parseInt(v) || 1)); };
    algo.getLineWidth = function() { return algo.presetLineWidth; };
    // Compatibility aliases used by older tests.
    algo.setDecay = function(v) { algo.setScrollSeconds(Math.max(0.05, (1 - HSVUtil.clamp01(parseFloat(v) || 0)) * 2)); };
    algo.getDecay = function() { return HSVUtil.clamp01(1 - (algo.presetScrollSeconds / 2)); };
    algo.setSpeed = function(v) {
        var speed = Math.max(1, Math.min(30, parseFloat(v) || 1));
        algo.setScrollSeconds(30 / speed);
    };
    algo.getSpeed = function() { return 30 / algo.presetScrollSeconds; };

    var state = { key: "", audioKey: "", amplitudes: [0], progress: 0 };

    function seconds(audio) {
        return HSVUtil.audioSeconds(audio);
    }

    function sample(audio) {
        var value = 0;
        if (algo.presetSource === "Beat") value = audio && audio.beat || 0;
        else if (algo.presetSource === "Bass") value = audio && audio.bass || 0;
        else if (algo.presetSource === "Lows") value = audio && audio.low || 0;
        else if (algo.presetSource === "Mids") value = audio && audio.mid || 0;
        else value = audio && audio.high || 0;
        return HSVUtil.clamp01(value);
    }

    function bresenham(x0, y0, x1, y1, visit) {
        var dx = Math.abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
        var dy = -Math.abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
        var err = dx + dy;
        while (true) {
            visit(x0, y0);
            if (x0 === x1 && y0 === y1) break;
            var e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }

    function paint(map, width, height, x, y, color, size) {
        var thickness = Math.max(1, size || 1);
        var start = -Math.floor((thickness - 1) * 0.5);
        var end = Math.ceil((thickness - 1) * 0.5);
        for (var dy = start; dy <= end; dy++) {
            var yy = y + dy;
            if (yy < 0 || yy >= height) continue;
            for (var dx = start; dx <= end; dx++) {
                var xx = x + dx;
                if (xx < 0 || xx >= width) continue;
                var i = (yy * width + xx) * 3;
                map[i] = color.h;
                map[i + 1] = color.s;
                map[i + 2] = color.v;
            }
        }
    }

    function initHistory(key) {
        var target = Math.max(1, algo.presetPoints);
        if (state.key === key && state.amplitudes.length === target) return;
        var old = state.amplitudes.slice();
        state.key = key;
        state.amplitudes = new Array(target).fill(0);
        for (var i = 0; i < Math.min(old.length, target); i++)
            state.amplitudes[i] = old[i];
        state.progress = 0;
    }

    function advanceHistory(current, dt) {
        if (!isFinite(dt) || dt <= 0) return;
        var stepTime = Math.max(0.0001, algo.presetScrollSeconds / Math.max(1, state.amplitudes.length));
        state.progress += dt;
        var shifts = Math.floor(state.progress / stepTime);
        if (shifts <= 0) return;
        state.progress -= shifts * stepTime;
        var bounded = Math.min(state.amplitudes.length, shifts);
        while (bounded-- > 0) {
            state.amplitudes.pop();
            state.amplitudes.unshift(current);
        }
    }

    function lerp(a, b, t) { return a + (b - a) * t; }

    function yForAmplitude(amp, height) {
        if (height <= 1) return 0;
        return Math.round((1 - HSVUtil.clamp01(amp)) * (height - 1));
    }

    function mirroredYs(amp, height) {
        if (height <= 1) return [0, 0];
        var a = HSVUtil.clamp01(amp);
        var top = Math.round((0.5 - a * 0.5) * (height - 1));
        var bottom = Math.round((0.5 + a * 0.5) * (height - 1));
        return [Math.max(0, Math.min(height - 1, top)), Math.max(0, Math.min(height - 1, bottom))];
    }

    function colorForPoint(index, amp, count) {
        var t = algo.presetColorBy === "Time"
            ? (count <= 1 ? 0 : (index / (count - 1)))
            : HSVUtil.clamp01(amp);
        return HSVUtil.gradientLedfxAt(algo.colors || [{ h: 0.58, s: 1, v: 1 }], HSVUtil.clamp01(t));
    }

    function pointAtIndex(index, count, width) {
        if (count <= 1 || width <= 1) return 0;
        return Math.round((index * (width - 1)) / (count - 1));
    }

    algo.rgbMapStepCount = function() { return 1; };
    algo.rgbMapSetColors = function() {};
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step, audio) {
        var map = HSVUtil.createMap(width, height);
        var audioKey = HSVUtil.audioIdentityKey(audio, state.audioKey);
        state.audioKey = audioKey;
        var k = [width, height, audioKey].join("|");
        var current = sample(audio);
        initHistory(k);
        var dt = seconds(audio);
        advanceHistory(current, dt);

        var points = Math.max(1, algo.presetPoints);
        var coords = [];
        for (var p = 0; p < points; p++) {
            var amp = HSVUtil.clamp01(state.amplitudes[p]);
            var x = pointAtIndex(p, points, width);
            var color = colorForPoint(p, amp, points);
            if (algo.presetMirror === "Yes") {
                var ys = mirroredYs(amp, height);
                coords.push({ x: x, yTop: ys[0], yBottom: ys[1], color: color });
            } else {
                coords.push({ x: x, y: yForAmplitude(amp, height), color: color });
            }
        }

        if (algo.presetMode === "Points") {
            for (var i = 0; i < coords.length; i++) {
                if (algo.presetMirror === "Yes") {
                    paint(map, width, height, coords[i].x, coords[i].yTop, coords[i].color, 1);
                    paint(map, width, height, coords[i].x, coords[i].yBottom, coords[i].color, 1);
                } else {
                    paint(map, width, height, coords[i].x, coords[i].y, coords[i].color, 1);
                }
            }
            return map;
        }

        for (var i2 = 1; i2 < coords.length; i2++) {
            var a = coords[i2 - 1];
            var b = coords[i2];
            if (algo.presetMirror === "Yes") {
                bresenham(a.x, a.yTop, b.x, b.yTop, function(x, y) {
                    var t = b.x === a.x ? 1 : (x - a.x) / (b.x - a.x);
                    var color = HSVUtil.gradientLedfxAt([a.color, b.color], HSVUtil.clamp01(t));
                    paint(map, width, height, x, y, color, algo.presetLineWidth);
                });
                bresenham(a.x, a.yBottom, b.x, b.yBottom, function(x, y) {
                    var t = b.x === a.x ? 1 : (x - a.x) / (b.x - a.x);
                    var color = HSVUtil.gradientLedfxAt([a.color, b.color], HSVUtil.clamp01(t));
                    paint(map, width, height, x, y, color, algo.presetLineWidth);
                });
                if (algo.presetMode === "Fill") {
                    var x0 = Math.min(a.x, b.x);
                    var x1 = Math.max(a.x, b.x);
                    var span = Math.max(1, x1 - x0);
                    for (var x = x0; x <= x1; x++) {
                        var tx = span === 0 ? 1 : (x - a.x) / Math.max(1e-6, b.x - a.x);
                        var yTop = Math.round(lerp(a.yTop, b.yTop, tx));
                        var yBottom = Math.round(lerp(a.yBottom, b.yBottom, tx));
                        var yStart = Math.min(yTop, yBottom);
                        var yEnd = Math.max(yTop, yBottom);
                        var fillColor = HSVUtil.gradientLedfxAt([a.color, b.color], HSVUtil.clamp01(tx));
                        for (var yy = yStart; yy <= yEnd; yy++)
                            paint(map, width, height, x, yy, fillColor, 1);
                    }
                }
                continue;
            }

            bresenham(a.x, a.y, b.x, b.y, function(x2, y2) {
                var t = b.x === a.x ? 1 : (x2 - a.x) / (b.x - a.x);
                var color = HSVUtil.gradientLedfxAt([a.color, b.color], HSVUtil.clamp01(t));
                paint(map, width, height, x2, y2, color, algo.presetLineWidth);
                if (algo.presetMode === "Fill")
                    for (var yy2 = 0; yy2 <= y2; yy2++) paint(map, width, height, x2, yy2, color, 1);
            });
        }
        return map;
    };

    testAlgo = algo;
    return algo;
})();
