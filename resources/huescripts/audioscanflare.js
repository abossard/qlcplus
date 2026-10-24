/*
  Q Light Controller Plus
  audioscanflare.js

  Copyright (c) QLC+ contributors
  Ported from LedFX "Scan and Flare" effect (MIT License)

  Single audio-driven scanner with white sparkle particles spawned on
  power spikes at the scanner's trailing edge.

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
    algo.name = "Audio Scan and Flare";
    algo.author = "Ported from LedFx";
    algo.acceptColors = 3; // low / mid / high gradient (scanner uses [0])
    algo.usesAudio = true;
    algo.properties = new Array();
    algo.presetMode = "Artistic";
    algo.properties.push("name:mode|type:list|display:Response|values:Artistic,LedFx Scan and Flare|write:setMode|read:getMode");
    algo.setMode = function(v) { algo.presetMode = v === "LedFx Scan and Flare" ? v : "Artistic"; };
    algo.getMode = function() { return algo.presetMode; };

    algo.presetSpeed = 0.5;
    algo.properties.push(
      "name:presetSpeed|type:float|display:Speed (cyc/beat)|" +
      "write:setSpeed|read:getSpeed");

    algo.presetWidth = 15;
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

    algo.presetMaxSparkles = 8;
    algo.properties.push(
      "name:presetMaxSparkles|type:range|display:Max Sparkles|" +
      "values:1,20|write:setMaxSparkles|read:getMaxSparkles");

    algo.presetSparkleSize = 10;
    algo.properties.push(
      "name:presetSparkleSize|type:range|display:Sparkle Size|" +
      "values:1,30|write:setSparkleSize|read:getSparkleSize");

    algo.presetSparkleTime = 500;
    algo.properties.push(
      "name:presetSparkleTime|type:range|display:Sparkle Time|" +
      "values:100,2000|write:setSparkleTime|read:getSparkleTime");

    algo.presetSparkleThreshold = 40;
    algo.properties.push(
      "name:presetSparkleThreshold|type:range|display:Sparkle Threshold|" +
      "values:10,90|write:setSparkleThreshold|read:getSparkleThreshold");

    algo.presetColorIntensity = 1;
    algo.properties.push(
      "name:presetColorIntensity|type:list|display:Color Intensity|" +
      "values:Yes,No|write:setColorIntensity|read:getColorIntensity");

    algo.presetAxis = "Horizontal";
    algo.properties.push(
      "name:presetAxis|type:list|display:Axis|" +
      "values:Horizontal,Vertical|write:setAxis|read:getAxis");
    algo.presetFrequencyRange = "Lows (beat+bass)";
    algo.properties.push(
      "name:presetFrequencyRange|type:list|display:Frequency Range|" +
      "values:Beat,Bass,Lows (beat+bass),Mids,High|write:setFrequencyRange|read:getFrequencyRange");
    algo.presetUseGradient = "No";
    algo.properties.push(
      "name:presetUseGradient|type:list|display:Gradient|" +
      "values:No,Yes|write:setUseGradient|read:getUseGradient");

    algo.setSpeed = function(_v) { algo.presetSpeed = HSVUtil.parseNumber(_v, algo.presetSpeed, 0, 10); };
    algo.getSpeed = function() { return algo.presetSpeed; };
    algo.setWidth = function(_v) { algo.presetWidth = HSVUtil.parseNumber(_v, algo.presetWidth, 1, 50); };
    algo.getWidth = function() { return algo.presetWidth; };
    algo.setMultiplier = function(_v) { algo.presetMultiplier = HSVUtil.parseNumber(_v, algo.presetMultiplier, 1, 500); };
    algo.getMultiplier = function() { return algo.presetMultiplier; };
    algo.setBounce = function(_v) { algo.presetBounce = (_v === "Yes") ? 1 : 0; };
    algo.getBounce = function() { return algo.presetBounce ? "Yes" : "No"; };
    algo.setMaxSparkles = function(_v) { algo.presetMaxSparkles = Math.round(HSVUtil.parseNumber(_v, algo.presetMaxSparkles, 1, 20)); };
    algo.getMaxSparkles = function() { return algo.presetMaxSparkles; };
    algo.setSparkleSize = function(_v) { algo.presetSparkleSize = HSVUtil.parseNumber(_v, algo.presetSparkleSize, 1, 30); };
    algo.getSparkleSize = function() { return algo.presetSparkleSize; };
    algo.setSparkleTime = function(_v) { algo.presetSparkleTime = HSVUtil.parseNumber(_v, algo.presetSparkleTime, 100, 2000); };
    algo.getSparkleTime = function() { return algo.presetSparkleTime; };
    algo.setSparkleThreshold = function(_v) { algo.presetSparkleThreshold = HSVUtil.parseNumber(_v, algo.presetSparkleThreshold, 10, 90); };
    algo.getSparkleThreshold = function() { return algo.presetSparkleThreshold; };
    algo.setColorIntensity = function(_v) { algo.presetColorIntensity = (_v === "Yes") ? 1 : 0; };
    algo.getColorIntensity = function() { return algo.presetColorIntensity ? "Yes" : "No"; };
    algo.setAxis = function(_v) { algo.presetAxis = _v; };
    algo.getAxis = function() { return algo.presetAxis; };
    algo.setFrequencyRange = function(_v) { algo.presetFrequencyRange = String(_v); };
    algo.getFrequencyRange = function() { return algo.presetFrequencyRange; };
    algo.setUseGradient = function(_v) { algo.presetUseGradient = _v === "Yes" ? "Yes" : "No"; };
    algo.getUseGradient = function() { return algo.presetUseGradient; };

    algo.presetSmoothing = 5;
    algo.properties.push(
      "name:presetSmoothing|type:range|display:Smoothing|" +
      "values:1,10|write:setSmoothing|read:getSmoothing");
    algo.setSmoothing = function(_v) { algo.presetSmoothing = Math.round(HSVUtil.parseNumber(_v, algo.presetSmoothing, 1, 10)); };
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

    var smoothLow = 0;
    var prevPower = 0;

    // Bar-level build-up / release state
    var envelope = {bar: 0, flash: 0};

    var SPARKLE_MIN_INTERVAL_MS = 50;

    algo.scanPos = 0;
    algo.returning = false;
    algo.sparkles = [];
    algo.lastSparkleMs = 0;
    algo.elapsedMs = 0;
    algo.lastN = 0;
    algo.lastMode = "";
    var lastIdentityKey = "";

    function bandColors() {
        if (algo.colors && algo.colors.length > 0)
            return algo.colors;
        return [
            {h: 0.0,   s: 1.0, v: 1.0},
            {h: 0.333, s: 1.0, v: 1.0},
            {h: 0.667, s: 1.0, v: 1.0}
        ];
    }

    function drawSegment(strip, n, startPos, w, color) {
        var start = Math.floor(startPos);
        for (var s = 0; s < w; s++) {
            var idx = start + s;
            if (idx < 0 || idx >= n) continue;
            var old = strip[idx];
            strip[idx] = {
                h: (color.v > old.v) ? color.h : old.h,
                s: (color.v > old.v) ? color.s : old.s,
                v: Math.min(1, old.v + color.v)
            };
        }
    }

    function wrapPosition(pos, n) {
        if (n <= 0) return 0;
        var out = pos % n;
        if (out < 0) out += n;
        return out;
    }

    // Age and move a sparkle; false once it faded out or, unless it wraps, left the strip.
    // It slows down as it fades: the distance is the integral of speed x health since its birth.
    function advanceSparkle(sp, n, wraps) {
        var age = (algo.elapsedMs - sp.bornMs) / 1000;
        var life = sp.dieMs / 1000;
        sp.health = 1 - age / life;
        if (sp.health <= 0) return false;
        sp.pos = sp.startPos + sp.speed * (age - age * age / (2 * life));
        if (wraps) {
            sp.pos = wrapPosition(sp.pos, n);
            return true;
        }
        return sp.pos >= -sp.width && sp.pos < n;
    }

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step, audio) {
        var map = HSVUtil.createMap(width, height);
        if (!audio) return map;
        var ledFxMode = algo.presetMode === "LedFx Scan and Flare";
        var n = ledFxMode ? (width * height) : ((algo.presetAxis === "Vertical") ? height : width);
        if (n <= 0) return map;
        var identityKey = HSVUtil.audioIdentityKey(audio, lastIdentityKey);
        var identityChanged = identityKey !== lastIdentityKey;
        lastIdentityKey = identityKey;
        if (identityChanged) {
            smoothLow = 0;
            prevPower = 0;
            envelope.bar = 0;
            envelope.flash = 0;
            algo.scanPos = 0;
            algo.returning = false;
            algo.sparkles = [];
            algo.lastSparkleMs = 0;
            algo.elapsedMs = 0;
        }

        if (algo.lastN !== n || algo.lastMode !== algo.presetMode) {
            algo.scanPos = 0;
            algo.returning = false;
            algo.sparkles = [];
            algo.lastN = n;
            algo.lastMode = algo.presetMode;
        }

        var dt = HSVUtil.audioSeconds(audio);
        var frames = HSVUtil.elapsedFrames(audio);
        algo.elapsedMs += dt * 1000;
        var beatsPerSec = audio.bpm / 60.0;

        var rawLow = HSVUtil.powerForRange(audio, algo.presetFrequencyRange, false) * 2.0;
        var power = rawLow;
        if (!ledFxMode) {
            smoothLow = HSVUtil.smoothFollow(smoothLow, rawLow, algo.presetSmoothing, frames);
            power = smoothLow;
            HSVUtil.barEnvelope(envelope, rawLow, dt, audio.downbeat, Math.pow(0.85, frames));
        }

        var multiplier = algo.presetMultiplier / 100.0;
        var bar = (ledFxMode ? power : (prevPower + power) / 2) * multiplier;
        prevPower = power;
        var scanW = Math.max(1, Math.round(n * algo.presetWidth / 100.0));
        var stepPerSec = ledFxMode
            ? (n * Math.min(1, algo.presetSpeed))
            : (Math.max(1, n - scanW) * algo.presetSpeed * beatsPerSec);
        var stepSize = dt * stepPerSec * bar;

        if (algo.returning) algo.scanPos -= stepSize;
        else algo.scanPos += stepSize;

        var maxStart = n - scanW;
        if (maxStart < 0) maxStart = 0;

        if (algo.presetBounce === 1 && !ledFxMode && maxStart > 0) {
            // Artistic folds the travel back at both ends, so no distance is lost at any render rate.
            var period = 2 * maxStart;
            var folded = wrapPosition(algo.scanPos, period);
            algo.returning = (folded > maxStart) !== algo.returning;
            algo.scanPos = folded > maxStart ? period - folded : folded;
        } else if (algo.presetBounce === 1) {
            if (algo.scanPos > maxStart) { algo.scanPos = maxStart; algo.returning = true; }
            if (algo.scanPos < 0) { algo.scanPos = 0; algo.returning = false; }
        } else {
            algo.scanPos = wrapPosition(algo.scanPos, n);
            if (algo.scanPos > maxStart) algo.scanPos = algo.scanPos - n;
        }

        // Sparkles move before the spawn check, so a slot freed in this render is reused at once.
        algo.sparkles = algo.sparkles.filter(function(sp) { return advanceSparkle(sp, n, ledFxMode); });

        var threshold = algo.presetSparkleThreshold / 100.0;
        var sparkleGate = ledFxMode ? (threshold * 2.0) : threshold;
        if (power > sparkleGate &&
            algo.sparkles.length < algo.presetMaxSparkles &&
            (algo.elapsedMs - algo.lastSparkleMs) >= SPARKLE_MIN_INTERVAL_MS) {

            // Sparkles are born on a fixed SPARKLE_MIN_INTERVAL_MS grid of audio time, at most one
            // render back, where the scanner was then; so they do not depend on the render rate.
            var born = Math.max(Math.floor(algo.elapsedMs / SPARKLE_MIN_INTERVAL_MS) * SPARKLE_MIN_INTERVAL_MS,
                algo.elapsedMs - dt * 1000);
            var scanAtBirth = algo.scanPos;
            if (dt > 0)
                scanAtBirth -= (algo.returning ? -stepSize : stepSize) * (algo.elapsedMs - born) / (dt * 1000);
            if (algo.presetBounce === 1)
                scanAtBirth = Math.max(0, Math.min(maxStart, scanAtBirth));
            var pixelPos = Math.floor(wrapPosition(scanAtBirth, n));
            var sparkleW = Math.max(1, Math.round(scanW * algo.presetSparkleSize / 100.0));
            var trailingPos = algo.returning
                ? (pixelPos + scanW)
                : (pixelPos - sparkleW);
            var sparkleSpeed = ledFxMode
                ? (algo.returning ? -stepPerSec : stepPerSec)
                : (stepPerSec * (algo.returning ? 1 : -1));

            // Artistic sparkles never wrap, so they spawn unwrapped next to the scanner.
            var sparkle = {
                startPos: ledFxMode ? wrapPosition(trailingPos, n) : trailingPos,
                width: sparkleW,
                speed: sparkleSpeed,
                bornMs: born,
                dieMs: algo.presetSparkleTime
            };
            if (advanceSparkle(sparkle, n, ledFxMode))
                algo.sparkles.push(sparkle);
            algo.lastSparkleMs = born;
        }

        var strip = new Array(n);
        var stripRgb = new Array(n);
        for (var p = 0; p < n; p++) {
            strip[p] = {h: 0, s: 0, v: 0};
            stripRgb[p] = [0, 0, 0];
        }

        var bc = bandColors();
        var scanHsv = algo.presetUseGradient === "Yes"
            ? HSVUtil.gradientLedfxAt(
                bc,
                n <= 1 ? 0 : HSVUtil.mod1(algo.scanPos / n))
            : {h: bc[0].h, s: bc[0].s, v: bc[0].v};
        if (algo.presetColorIntensity === 1)
            scanHsv = {h: scanHsv.h, s: scanHsv.s, v: scanHsv.v * Math.min(1, power)};
        // Release flares the scanner brightness on downbeat
        if (!ledFxMode && envelope.flash > 0.01)
            scanHsv = {h: scanHsv.h, s: scanHsv.s, v: Math.min(1, scanHsv.v + envelope.flash * 0.5)};
        // Without bounce a scanner that crosses the strip end also covers its start.
        var scanStarts = [Math.floor(algo.scanPos)];
        if (algo.scanPos < 0)
            scanStarts.push(scanStarts[0] + n);
        var scanRgb = ledFxMode ? HSVUtil.hsvToRgb(scanHsv.h, scanHsv.s, scanHsv.v) : null;
        for (var si = 0; si < scanStarts.length; si++) {
            if (!ledFxMode) {
                drawSegment(strip, n, scanStarts[si], scanW, scanHsv);
                continue;
            }
            for (var s = 0; s < scanW; s++) {
                var idx = scanStarts[si] + s;
                if (idx >= 0 && idx < n)
                    stripRgb[idx] = [scanRgb[0], scanRgb[1], scanRgb[2]];
            }
        }

        for (var i = 0; i < algo.sparkles.length; i++) {
            var sp = algo.sparkles[i];
            if (!ledFxMode) {
                drawSegment(strip, n, sp.pos, sp.width, {h: 0, s: 0, v: sp.health});
                continue;
            }
            var base = Math.floor(sp.pos);
            for (var ws = 0; ws < sp.width; ws++) {
                var wi = (base + ws) % n;
                stripRgb[wi][0] = HSVUtil.clamp01(stripRgb[wi][0] + sp.health);
                stripRgb[wi][1] = HSVUtil.clamp01(stripRgb[wi][1] + sp.health);
                stripRgb[wi][2] = HSVUtil.clamp01(stripRgb[wi][2] + sp.health);
            }
        }

        if (ledFxMode)
            return HSVUtil.applyStripTransforms(HSVUtil.writeRgbPixels(map, stripRgb), width, height, readOutputControls());

        if (algo.presetAxis === "Vertical") {
            for (var y = 0; y < height; y++) {
                var pix = strip[y];
                for (var x = 0; x < width; x++)
                    HSVUtil.setPixel(map, width, x, y, pix.h, pix.s, pix.v);
            }
        } else {
            for (var y2 = 0; y2 < height; y2++) {
                for (var x2 = 0; x2 < width; x2++)
                    HSVUtil.setPixel(map, width, x2, y2, strip[x2].h, strip[x2].s, strip[x2].v);
            }
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
