/*
  Q Light Controller Plus
  audiopower.js

  Copyright (c) QLC+ contributors
  Ported from LedFx "Power" effect (MIT License)

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
    algo.name = "Audio Power";
    algo.author = "Ported from LedFx";
    algo.acceptColors = 3;
    algo.usesAudio = true;
    algo.properties = new Array();

    var DEFAULT_GRADIENT = [0xFF0000, 0xFF7800, 0xFFC800, 0x00FF00, 0x00C78C, 0x0000FF, 0x800080, 0xFF00B2];

    algo.mirror = "Yes";
    algo.blur = 0.0;
    algo.sparks_color = "#ffffff";
    algo.bass_decay_rate = 0.05;
    algo.sparks_decay_rate = 0.15;

    algo.properties.push("name:mirror|type:list|display:Mirror|values:Yes,No|write:setMirror|read:getMirror");
    algo.properties.push("name:blur|type:float|display:Blur|write:setBlur|read:getBlur");
    algo.properties.push("name:sparks_color|type:string|display:Sparks Color|write:setSparksColor|read:getSparksColor");
    algo.properties.push("name:bass_decay_rate|type:float|display:Bass Decay Rate|write:setBassDecayRate|read:getBassDecayRate");
    algo.properties.push("name:sparks_decay_rate|type:float|display:Sparks Decay Rate|write:setSparksDecayRate|read:getSparksDecayRate");

    algo.setMirror = function(v) { algo.mirror = (v === "No") ? "No" : "Yes"; };
    algo.getMirror = function() { return algo.mirror; };
    algo.setBlur = function(v) { algo.blur = clamp(parseFloat(v), 0, 10); };
    algo.getBlur = function() { return algo.blur; };
    algo.setSparksColor = function(v) { algo.sparks_color = String(v); };
    algo.getSparksColor = function() { return algo.sparks_color; };
    algo.setBassDecayRate = function(v) { algo.bass_decay_rate = clamp(parseFloat(v), 0, 1); };
    algo.getBassDecayRate = function() { return algo.bass_decay_rate; };
    algo.setSparksDecayRate = function(v) { algo.sparks_decay_rate = clamp(parseFloat(v), 0, 1); };
    algo.getSparksDecayRate = function() { return algo.sparks_decay_rate; };

    var sparksOverlay = [];
    var bassOverlay = [];
    var bassFilter = null;
    var lastWidth = 0;

    function clamp(v, lo, hi) {
        if (isNaN(v)) return lo;
        return Math.max(lo, Math.min(hi, v));
    }

    function colorArray(packed) {
        packed = packed & 0xFFFFFF;
        return [(packed >> 16) & 0xFF, (packed >> 8) & 0xFF, packed & 0xFF];
    }

    function parseColor(value, fallback) {
        if (typeof value === "number") return value & 0xFFFFFF;
        var s = String(value || "").replace(/^#/, "");
        if (s.length === 3)
            s = s.charAt(0) + s.charAt(0) + s.charAt(1) + s.charAt(1) + s.charAt(2) + s.charAt(2);
        var n = parseInt(s, 16);
        return isNaN(n) ? fallback : (n & 0xFFFFFF);
    }

    function zeroStrip(n) {
        var out = new Array(n);
        for (var i = 0; i < n; i++) out[i] = [0, 0, 0];
        return out;
    }

    function ensure(width) {
        if (lastWidth === width && sparksOverlay.length === width) return;
        sparksOverlay = zeroStrip(width);
        bassOverlay = zeroStrip(width);
        bassFilter = null;
        lastWidth = width;
    }

    function scaleInPlace(strip, factor) {
        for (var i = 0; i < strip.length; i++) {
            strip[i][0] *= factor;
            strip[i][1] *= factor;
            strip[i][2] *= factor;
        }
    }

    function expFilter(value) {
        if (bassFilter === null) {
            bassFilter = value;
            return bassFilter;
        }
        var alpha = value > bassFilter ? 0.8 : 0.1;
        bassFilter = alpha * value + (1.0 - alpha) * bassFilter;
        return bassFilter;
    }

    function gradientStops() {
        return (algo.gradientColors && algo.gradientColors.length > 0) ? algo.gradientColors : DEFAULT_GRADIENT;
    }

    function applyMirror(strip) {
        if (algo.mirror !== "Yes") return strip;
        var n = strip.length;
        var out = new Array(n);
        for (var i = 0; i < n; i++) {
            var a = strip[n - 1 - (2 * i)];
            var b = strip[n - 2 - (2 * i)];
            if (i >= Math.ceil(n / 2)) {
                var j = 2 * i - n;
                a = strip[j];
                b = strip[j + 1];
            }
            if (!b) b = a;
            out[i] = [Math.max(a[0], b[0]), Math.max(a[1], b[1]), Math.max(a[2], b[2])];
        }
        return out;
    }

    function boxBlur(strip, amount) {
        var radius = Math.round(amount);
        if (radius <= 0 || strip.length <= 3) return strip;
        var n = strip.length;
        var out = new Array(n);
        for (var i = 0; i < n; i++) {
            var r = 0, g = 0, b = 0, c = 0;
            for (var k = -radius; k <= radius; k++) {
                var idx = i + k;
                if (idx < 0 || idx >= n) continue;
                r += strip[idx][0]; g += strip[idx][1]; b += strip[idx][2]; c++;
            }
            out[i] = [r / c, g / c, b / c];
        }
        return out;
    }

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { algo.gradientColors = RGBUtil.buildGradientColors(rawColors); };
    algo.rgbMapGetColors = function() { return gradientStops().slice(); };

    algo.rgbMap = function(width, height, rgb, step, audio) {
        ensure(width);
        var map = RGBUtil.createMap(width, height);
        if (!audio) return map;

        scaleInPlace(sparksOverlay, 1.0 - clamp(algo.sparks_decay_rate, 0, 1));
        scaleInPlace(bassOverlay, 1.0 - clamp(algo.bass_decay_rate, 0, 1));

        if (audio.onset.fired) {
            var sparks = Math.floor(width / 20);
            var sc = colorArray(parseColor(algo.sparks_color, 0xFFFFFF));
            for (var s = 0; s < sparks; s++) {
                var sx = Math.floor(Math.random() * width);
                sparksOverlay[sx] = [sc[0], sc[1], sc[2]];
            }
        }

        var bass = expFilter(Math.max(0, audio.power.low));
        var bassIdx = Math.floor(bass * width);
        var bassColor = colorArray(RGBUtil.gradientColorAt(gradientStops(), bass));
        for (var i = 0; i < bassIdx && i < width; i++)
            bassOverlay[i] = [bassColor[0], bassColor[1], bassColor[2]];

        var mel = RGBUtil.interpolate(audio.spectrum.full, width);
        var strip = new Array(width);
        for (var x = 0; x < width; x++) {
            var spatial = width <= 1 ? 0 : x / (width - 1);
            var gcol = colorArray(RGBUtil.gradientColorAt(gradientStops(), spatial));
            var m = mel[x];
            strip[x] = [
                gcol[0] * m + bassOverlay[x][0] + sparksOverlay[x][0],
                gcol[1] * m + bassOverlay[x][1] + sparksOverlay[x][1],
                gcol[2] * m + bassOverlay[x][2] + sparksOverlay[x][2]
            ];
        }

        strip = boxBlur(applyMirror(strip), algo.blur);
        for (var y = 0; y < height; y++)
            for (var px = 0; px < width; px++)
                map[y][px] = RGBUtil.rgb(strip[px][0], strip[px][1], strip[px][2]);
        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
