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
    algo.presetMode = "Artistic";
    algo.properties.push("name:mode|type:list|display:Response|values:Artistic,LedFx Scan|write:setMode|read:getMode");
    algo.setMode = function(v) { algo.presetMode = v === "LedFx Scan" ? v : "Artistic"; };
    algo.getMode = function() { return algo.presetMode; };

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
    algo.full_grad_mod = "Off";
    algo.full_grad_mod_speed = 0.25;

    algo.properties.push("name:blur|type:float|display:Blur|write:setBlur|read:getBlur");
    algo.properties.push("name:bounce|type:list|display:Bounce|values:Yes,No|write:setBounce|read:getBounce");
    algo.properties.push("name:scan_width|type:range|display:Scan Width (%)|values:1,100|write:setScanWidth|read:getScanWidth");
    algo.properties.push("name:speed|type:range|display:Speed (%/s)|values:0,100|write:setSpeed|read:getSpeed");
    algo.properties.push("name:frequency_range|type:list|display:Frequency Range|values:Beat,Bass,Lows (beat+bass),Mids,High|write:setFrequencyRange|read:getFrequencyRange");
    algo.properties.push("name:multiplier|type:float|display:Multiplier|write:setMultiplier|read:getMultiplier");
    algo.properties.push("name:color_intensity|type:list|display:Color Intensity|values:Yes,No|write:setColorIntensity|read:getColorIntensity");
    algo.properties.push("name:use_grad|type:list|display:Use Gradient|values:Yes,No|write:setUseGrad|read:getUseGrad");
    algo.properties.push("name:full_grad|type:list|display:Full Gradient|values:Yes,No|write:setFullGrad|read:getFullGrad");
    algo.properties.push("name:full_grad_mod|type:list|display:Full Gradient Modulation|values:Off,Sine,Breath|write:setFullGradMod|read:getFullGradMod");
    algo.properties.push("name:full_grad_mod_speed|type:float|values:0,1|display:Full Gradient Mod Speed|write:setFullGradModSpeed|read:getFullGradModSpeed");
    algo.properties.push("name:count|type:range|display:Count|values:1,10|write:setCount|read:getCount");

    algo.setBlur = function(v) { algo.blur = parseFloat(v); };
    algo.getBlur = function() { return algo.blur; };
    algo.setBounce = function(v) { algo.bounce = (v === "No") ? "No" : "Yes"; };
    algo.getBounce = function() { return algo.bounce; };
    algo.setScanWidth = function(v) { algo.scan_width = parseFloat(v); };
    algo.getScanWidth = function() { return algo.scan_width; };
    algo.setSpeed = function(v) { algo.speed = parseFloat(v); };
    algo.getSpeed = function() { return algo.speed; };
    algo.setFrequencyRange = function(v) { algo.frequency_range = String(v); };
    algo.getFrequencyRange = function() { return algo.frequency_range; };
    algo.setMultiplier = function(v) { algo.multiplier = parseFloat(v); };
    algo.getMultiplier = function() { return algo.multiplier; };
    algo.setColorIntensity = function(v) { algo.color_intensity = (v === "No") ? "No" : "Yes"; };
    algo.getColorIntensity = function() { return algo.color_intensity; };
    algo.setUseGrad = function(v) { algo.use_grad = (v === "Yes") ? "Yes" : "No"; };
    algo.getUseGrad = function() { return algo.use_grad; };
    algo.setFullGrad = function(v) { algo.full_grad = (v === "Yes") ? "Yes" : "No"; };
    algo.getFullGrad = function() { return algo.full_grad; };
    algo.setFullGradMod = function(v) { algo.full_grad_mod = (v === "Sine" || v === "Breath") ? v : "Off"; };
    algo.getFullGradMod = function() { return algo.full_grad_mod; };
    algo.setFullGradModSpeed = function(v) { algo.full_grad_mod_speed = Math.max(0, Math.min(1, parseFloat(v) || 0)); };
    algo.getFullGradModSpeed = function() { return algo.full_grad_mod_speed; };
    algo.setCount = function(v) { algo.count = parseInt(v); };
    algo.getCount = function() { return algo.count; };

    algo.presetSmoothing = 5;
    algo.properties.push("name:presetSmoothing|type:range|display:Smoothing|values:1,10|write:setSmoothing|read:getSmoothing");
    algo.setSmoothing = function(v) { algo.presetSmoothing = parseInt(v); };
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

    var smoothPower = 0;

    // Bar-level build-up / release state
    var envelope = {bar: 0, flash: 0};

    var scanPos = 0.0;
    var returning = false;
    var lastSpan = 0;
    var lastMode = "";
    var modulationClock = 0;
    var lastIdentityKey = "";

    function gradientStops() {
        return (algo.colors && algo.colors.length > 0) ? algo.colors : DEFAULT_GRADIENT;
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
        var ledFxMode = algo.presetMode === "LedFx Scan";
        var span = ledFxMode ? (width * height) : width;
        var identityKey = HSVUtil.audioIdentityKey(audio, lastIdentityKey);
        var identityChanged = identityKey !== lastIdentityKey;
        lastIdentityKey = identityKey;
        if (identityChanged) {
            smoothPower = 0;
            envelope.bar = 0;
            envelope.flash = 0;
            scanPos = 0.0;
            returning = false;
            modulationClock = 0;
        }
        if (lastSpan !== span || lastMode !== algo.presetMode) {
            scanPos = 0.0;
            returning = false;
            lastSpan = span;
            lastMode = algo.presetMode;
            modulationClock = 0;
        }

        var passed = HSVUtil.audioSeconds(audio);
        modulationClock += passed;
        var count = Math.max(1, Math.min(10, algo.count));
        var block = span / count;
        var stepPerSec = span / 100.0 * Math.max(0, Math.min(100, algo.speed));
        var scanWidthPixels = Math.max(1, Math.floor(block / 100.0 * Math.max(1, Math.min(100, algo.scan_width))));
        var rawPower = HSVUtil.powerForRange(audio, algo.frequency_range, false) * 2.0;
        var power = rawPower;
        if (!ledFxMode) {
            smoothPower = HSVUtil.smoothFollow(smoothPower, rawPower, algo.presetSmoothing);
            power = smoothPower;
            HSVUtil.barEnvelope(envelope, rawPower, audio.dt, audio.downbeat, 0.85);
        }

        // Build-up subtly widens the scan; release flares it
        var widthBoost = ledFxMode ? 1.0 : (1.0 + envelope.flash * 0.4);
        var bar = power * widthBoost * Math.max(0, Math.min(5, parseFloat(algo.multiplier)));
        var stepSize = passed * stepPerSec * bar;

        scanPos += returning ? -stepSize : stepSize;
        if (algo.bounce === "Yes") {
            if (scanPos > span - scanWidthPixels) { returning = true; scanPos = span - scanWidthPixels; }
            if (scanPos < 0) { returning = false; scanPos = 0; }
        } else {
            if (scanPos > span) scanPos = scanPos % span;
            if (scanPos < 0) returning = false;
        }

        var strip = new Array(span);
        if (algo.full_grad === "Yes") {
            for (var gx = 0; gx < span; gx++) {
                var t = span <= 1 ? 0 : gx / (span - 1);
                var color = ledFxMode
                    ? HSVUtil.gradientLedfxAt(gradientStops(), t)
                    : HSVUtil.gradientAt(gradientStops(), t);
                if (ledFxMode && algo.full_grad_mod !== "Off") {
                    var phase = HSVUtil.mod1(modulationClock * algo.full_grad_mod_speed + t);
                    if (algo.full_grad_mod === "Sine")
                        color.v *= HSVUtil.sin01(phase);
                    else
                        color.v *= 0.2 + 0.8 * HSVUtil.triangle(phase);
                }
                strip[gx] = color;
            }
        } else {
            for (var zx = 0; zx < span; zx++) strip[zx] = {h: 0, s: 0, v: 0};
        }

        var scanColor;
        if (algo.use_grad === "Yes") {
            var gt = HSVUtil.mod1(scanPos / span);
            scanColor = ledFxMode
                ? HSVUtil.gradientLedfxAt(gradientStops(), gt)
                : HSVUtil.gradientAt(gradientStops(), gt);
        } else {
            scanColor = algo.colors[0];
        }
        if (algo.color_intensity === "Yes") {
            var intensity = Math.min(1.0, power + (ledFxMode ? 0 : envelope.flash * 0.6));
            scanColor = {h: scanColor.h, s: scanColor.s, v: scanColor.v * intensity};
        } else if (!ledFxMode && envelope.flash > 0.01) {
            // Even without color-intensity, release adds a brightness flare
            scanColor = {h: scanColor.h, s: scanColor.s,
                         v: Math.min(1, scanColor.v + envelope.flash * 0.4)};
        }

        for (var bi = 0; bi < count; bi++) {
            var blockStart = Math.floor(block * bi);
            var blockMid = Math.floor(block * bi + scanWidthPixels);
            var blockEnd = Math.floor(block * bi + block);
            if (algo.full_grad === "Yes") {
                var midPos = Math.floor(blockMid + scanPos);
                var endPos = Math.floor(blockEnd + scanPos);
                for (var c1 = Math.min(midPos, span); c1 < Math.min(endPos, span); c1++) clearStrip(strip, c1);
                var endFlow = endPos - span;
                if (endFlow > 0) {
                    var midFlow = Math.max(0, midPos - span);
                    for (var c2 = midFlow; c2 < endFlow; c2++) clearStrip(strip, c2);
                }
            } else {
                var startPos = Math.floor(blockStart + scanPos);
                var mid = Math.floor(blockMid + scanPos);
                for (var s1 = Math.min(startPos, span); s1 < Math.min(mid, span); s1++) setStrip(strip, s1, scanColor);
                var midFlow2 = mid - span;
                if (midFlow2 > 0) {
                    var startFlow = Math.max(0, startPos - span);
                    for (var s2 = startFlow; s2 < midFlow2; s2++) setStrip(strip, s2, scanColor);
                }
            }
        }

        if (!ledFxMode) {
            strip = boxBlur(strip, algo.blur);
            for (var y = 0; y < height; y++)
                for (var x = 0; x < width; x++)
                    HSVUtil.setPixel(map, width, x, y, strip[x].h, strip[x].s, strip[x].v);
            return map;
        }

        for (var i = 0; i < strip.length; i++) {
            var o = i * 3;
            map[o] = strip[i].h;
            map[o + 1] = strip[i].s;
            map[o + 2] = strip[i].v;
        }
        return HSVUtil.applyStripTransforms(map, width, height, readOutputControls());
    };

    testAlgo = algo;
    return algo;
  }
)();
