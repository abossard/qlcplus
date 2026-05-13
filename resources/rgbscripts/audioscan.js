/*
  Q Light Controller Plus
  audioscan.js

  Copyright (c) QLC+ contributors
  Ported from LedFx "Scan" effect (MIT License)

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
    algo.name = "Audio Scan";
    algo.author = "Ported from LedFx";
    algo.acceptColors = 3;
    algo.usesAudio = true;
    algo.properties = new Array();

    var DEFAULT_GRADIENT = [
        {h: 0.0,   s: 1.0, v: 1.0},
        {h: 0.078, s: 1.0, v: 1.0},
        {h: 0.131, s: 1.0, v: 1.0},
        {h: 0.333, s: 1.0, v: 1.0},
        {h: 0.453, s: 1.0, v: 0.780},
        {h: 0.667, s: 1.0, v: 1.0},
        {h: 0.833, s: 1.0, v: 0.502},
        {h: 0.884, s: 1.0, v: 1.0}
    ];

    algo.blur = 3.0;
    algo.bounce = "Yes";
    algo.scan_width = 30;
    algo.speed = 50;
    algo.frequency_range = "Lows (beat+bass)";
    algo.multiplier = 3.0;
    algo.color_intensity = "Yes";
    algo.use_grad = "No";
    algo.full_grad = "No";
    algo.count = 1;

    algo.properties.push("name:blur|type:float|display:Blur|write:setBlur|read:getBlur");
    algo.properties.push("name:bounce|type:list|display:Bounce|values:Yes,No|write:setBounce|read:getBounce");
    algo.properties.push("name:scan_width|type:range|display:Scan Width (%)|values:1,100|write:setScanWidth|read:getScanWidth");
    algo.properties.push("name:speed|type:range|display:Speed (%/s)|values:0,100|write:setSpeed|read:getSpeed");
    algo.properties.push("name:frequency_range|type:list|display:Frequency Range|values:Beat,Bass,Lows (beat+bass),Mids,High|write:setFrequencyRange|read:getFrequencyRange");
    algo.properties.push("name:multiplier|type:float|display:Multiplier|write:setMultiplier|read:getMultiplier");
    algo.properties.push("name:color_intensity|type:list|display:Color Intensity|values:Yes,No|write:setColorIntensity|read:getColorIntensity");
    algo.properties.push("name:use_grad|type:list|display:Use Gradient|values:Yes,No|write:setUseGrad|read:getUseGrad");
    algo.properties.push("name:full_grad|type:list|display:Full Gradient|values:Yes,No|write:setFullGrad|read:getFullGrad");
    algo.properties.push("name:count|type:range|display:Count|values:1,10|write:setCount|read:getCount");

    function clamp(v, lo, hi) { if (isNaN(v)) return lo; return Math.max(lo, Math.min(hi, v)); }
    algo.setBlur = function(v) { algo.blur = clamp(parseFloat(v), 0, 10); };
    algo.getBlur = function() { return algo.blur; };
    algo.setBounce = function(v) { algo.bounce = (v === "No") ? "No" : "Yes"; };
    algo.getBounce = function() { return algo.bounce; };
    algo.setScanWidth = function(v) { algo.scan_width = clamp(parseInt(v), 1, 100); };
    algo.getScanWidth = function() { return algo.scan_width; };
    algo.setSpeed = function(v) { algo.speed = clamp(parseInt(v), 0, 100); };
    algo.getSpeed = function() { return algo.speed; };
    algo.setFrequencyRange = function(v) { algo.frequency_range = String(v); };
    algo.getFrequencyRange = function() { return algo.frequency_range; };
    algo.setMultiplier = function(v) { algo.multiplier = clamp(parseFloat(v), 0, 5); };
    algo.getMultiplier = function() { return algo.multiplier; };
    algo.setColorIntensity = function(v) { algo.color_intensity = (v === "No") ? "No" : "Yes"; };
    algo.getColorIntensity = function() { return algo.color_intensity; };
    algo.setUseGrad = function(v) { algo.use_grad = (v === "Yes") ? "Yes" : "No"; };
    algo.getUseGrad = function() { return algo.use_grad; };
    algo.setFullGrad = function(v) { algo.full_grad = (v === "Yes") ? "Yes" : "No"; };
    algo.getFullGrad = function() { return algo.full_grad; };
    algo.setCount = function(v) { algo.count = clamp(parseInt(v), 1, 10); };
    algo.getCount = function() { return algo.count; };

    var scanPos = 0.0;
    var returning = false;
    var lastWidth = 0;

    function gradientStops() {
        return (algo.colors && algo.colors.length > 0) ? algo.colors : DEFAULT_GRADIENT;
    }

    function bandColors() {
        if (algo.colors && algo.colors.length > 0)
            return algo.colors;
        return [
            {h: 0.0,   s: 1.0, v: 1.0},
            {h: 0.333, s: 1.0, v: 1.0},
            {h: 0.667, s: 1.0, v: 1.0}
        ];
    }

    function powerFor(audio) {
        if (algo.frequency_range === "Beat") return audio.power.detail.beat;
        if (algo.frequency_range === "Bass") return audio.power.detail.bass;
        if (algo.frequency_range === "Mids") return audio.power.mid;
        if (algo.frequency_range === "High") return audio.power.high;
        // Lows (beat+bass)
        return (audio.power.detail.beat + audio.power.detail.bass) * 0.5;
    }

    function setStrip(strip, idx, color) {
        if (idx < 0 || idx >= strip.length) return;
        strip[idx] = {h: color.h, s: color.s, v: color.v};
    }

    function clearStrip(strip, idx) {
        if (idx < 0 || idx >= strip.length) return;
        strip[idx] = {h: 0, s: 0, v: 0};
    }

    function boxBlur(strip, amount) {
        var radius = Math.round(amount);
        if (radius <= 0 || strip.length <= 3) return strip;
        var n = strip.length;
        var out = new Array(n);
        for (var i = 0; i < n; i++) {
            var bh = 0, bs = 0, bv = 0, c = 0;
            for (var k = -radius; k <= radius; k++) {
                var idx = i + k;
                if (idx < 0 || idx >= n) continue;
                bh += strip[idx].h; bs += strip[idx].s; bv += strip[idx].v; c++;
            }
            out[i] = {h: bh / c, s: bs / c, v: bv / c};
        }
        return out;
    }

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return DEFAULT_GRADIENT.slice(); };

    algo.rgbMap = function(width, height, rgb, step, audio) {
        var map = HSVUtil.createMap(width, height);
        if (!audio) return map;
        if (lastWidth !== width) {
            scanPos = 0.0;
            returning = false;
            lastWidth = width;
        }

        var dtMs = audio.timing.consumerDtMs > 0 ? audio.timing.consumerDtMs : 40;
        var passed = dtMs / 1000.0;
        var count = clamp(parseInt(algo.count), 1, 10);
        var block = width / count;
        var stepPerSec = width / 100.0 * clamp(parseInt(algo.speed), 0, 100);
        var scanWidthPixels = Math.max(1, Math.floor(block / 100.0 * clamp(parseInt(algo.scan_width), 1, 100)));
        var power = powerFor(audio) * 2.0;
        var bar = power * clamp(parseFloat(algo.multiplier), 0, 5);
        var stepSize = passed * stepPerSec * bar;

        scanPos += returning ? -stepSize : stepSize;
        if (algo.bounce === "Yes") {
            if (scanPos > width - scanWidthPixels) { returning = true; scanPos = width - scanWidthPixels; }
            if (scanPos < 0) { returning = false; scanPos = 0; }
        } else {
            if (scanPos > width) scanPos = scanPos % width;
            if (scanPos < 0) returning = false;
        }

        var strip = new Array(width);
        if (algo.full_grad === "Yes") {
            for (var gx = 0; gx < width; gx++) {
                var t = width <= 1 ? 0 : gx / (width - 1);
                strip[gx] = HSVUtil.gradientAt(gradientStops(), t);
            }
        } else {
            for (var zx = 0; zx < width; zx++) strip[zx] = {h: 0, s: 0, v: 0};
        }

        var scanColor;
        if (algo.use_grad === "Yes") {
            var gt = ((scanPos / width) % 1 + 1) % 1;
            scanColor = HSVUtil.gradientAt(gradientStops(), gt);
        } else {
            var bc = bandColors();
            scanColor = {h: bc[0].h, s: bc[0].s, v: bc[0].v};
        }
        if (algo.color_intensity === "Yes") {
            var intensity = Math.min(1.0, power);
            scanColor = {h: scanColor.h, s: scanColor.s, v: scanColor.v * intensity};
        }

        for (var bi = 0; bi < count; bi++) {
            var blockStart = Math.floor(block * bi);
            var blockMid = Math.floor(block * bi + scanWidthPixels);
            var blockEnd = Math.floor(block * bi + block);
            if (algo.full_grad === "Yes") {
                var midPos = Math.floor(blockMid + scanPos);
                var endPos = Math.floor(blockEnd + scanPos);
                for (var c1 = Math.min(midPos, width); c1 < Math.min(endPos, width); c1++) clearStrip(strip, c1);
                var endFlow = endPos - width;
                if (endFlow > 0) {
                    var midFlow = Math.max(0, midPos - width);
                    for (var c2 = midFlow; c2 < endFlow; c2++) clearStrip(strip, c2);
                }
            } else {
                var startPos = Math.floor(blockStart + scanPos);
                var mid = Math.floor(blockMid + scanPos);
                for (var s1 = Math.min(startPos, width); s1 < Math.min(mid, width); s1++) setStrip(strip, s1, scanColor);
                var midFlow2 = mid - width;
                if (midFlow2 > 0) {
                    var startFlow = Math.max(0, startPos - width);
                    for (var s2 = startFlow; s2 < midFlow2; s2++) setStrip(strip, s2, scanColor);
                }
            }
        }

        strip = boxBlur(strip, algo.blur);
        for (var y = 0; y < height; y++)
            for (var x = 0; x < width; x++)
                HSVUtil.setPixel(map, width, x, y, strip[x].h, strip[x].s, strip[x].v);
        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
