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
    algo.setAxis = function(_v) { algo.presetAxis = _v; };
    algo.getAxis = function() { return algo.presetAxis; };

    algo.presetSmoothing = 5;
    algo.properties.push(
      "name:presetSmoothing|type:range|display:Smoothing|" +
      "values:1,10|write:setSmoothing|read:getSmoothing");
    algo.setSmoothing = function(_v) { algo.presetSmoothing = parseInt(_v); };
    algo.getSmoothing = function() { return algo.presetSmoothing; };

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

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };

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

    algo.rgbMap = function(width, height, rgb, step, audio) {
        var map = HSVUtil.createMap(width, height);
        if (!audio) return map;

        var n = (algo.presetAxis === "Vertical") ? height : width;
        if (n <= 0) return map;

        if (algo.lastN !== n) {
            for (var i = 0; i < 3; i++) {
                algo.scans[i].pos = 0;
                algo.scans[i].returning = false;
            }
            algo.lastN = n;
        }

        var dt = audio.dt * 60.0 / audio.bpm;
        var bpmEff = (audio.bpm > 0) ? audio.bpm : 120;
        var beatsPerSec = bpmEff / 60.0;

        var multiplier = algo.presetMultiplier / 100.0;
        var scanW = Math.max(1, Math.round(n * algo.presetWidth / 100.0));
        var stepPerSec = Math.max(1, n - scanW) * algo.presetSpeed * beatsPerSec;
        var bounce = algo.presetBounce === 1;
        var colorIntensity = algo.presetColorIntensity === 1;
        var bandColors = (algo.colors && algo.colors.length >= 3) ? algo.colors : [
            {h: 0.0,   s: 1.0, v: 1.0},
            {h: 0.333, s: 1.0, v: 1.0},
            {h: 0.667, s: 1.0, v: 1.0}
        ];

        var strip = new Array(n);
        for (var p = 0; p < n; p++) strip[p] = {h: 0, s: 0, v: 0};

        var maxStart = n - scanW;
        if (maxStart < 0) maxStart = 0;

        // Asymmetric EMA on per-band power (brightness path)
        var smoothing = algo.presetSmoothing / 10.0;
        var riseAlpha = 0.5 * (1 - smoothing) + 0.05;
        var decayAlpha = 0.02 + 0.03 * (1 - smoothing);
        var rawPows = [audio.low, audio.mid, audio.high];
        for (var sb = 0; sb < 3; sb++) {
            var sa = rawPows[sb] > smoothPow[sb] ? riseAlpha : decayAlpha;
            smoothPow[sb] += sa * (rawPows[sb] - smoothPow[sb]);
        }

        // --- Bar-level build-up (shared across all bands) ---
        var rawEnergy = (rawPows[0] + rawPows[1] * 0.5 + rawPows[2] * 0.3) / 1.8;
        barEnergy += rawEnergy * dt;
        if (barEnergy > peakEnergy) peakEnergy = barEnergy;
        if (audio.downbeat) {
            releaseFlash = Math.min(1, peakEnergy * 0.5);
            barEnergy = 0;
            peakEnergy = 0;
        }
        releaseFlash *= 0.85;

        for (var b = 0; b < 3; b++) {
            var scan = algo.scans[b];
            var power = rawPows[b];          // responsive speed
            var briPower = smoothPow[b];     // smoothed brightness
            var bar = power * multiplier;
            var stepSize = dt * stepPerSec * bar;

            if (scan.returning) scan.pos -= stepSize;
            else scan.pos += stepSize;

            if (bounce) {
                if (scan.pos > maxStart) { scan.pos = maxStart; scan.returning = true; }
                if (scan.pos < 0) { scan.pos = 0; scan.returning = false; }
            } else {
                if (n > 0) {
                    scan.pos = ((scan.pos % n) + n) % n;
                    if (scan.pos > maxStart) scan.pos = scan.pos - n;
                }
            }

            var bc = bandColors[b];
            // Release adds a synced brightness flare across all 3 scanners
            var briBoost = colorIntensity ? Math.min(1, briPower) : 1.0;
            var cv = bc.v * Math.min(1, briBoost + releaseFlash * 0.5);
            drawScan(strip, n, scan.pos, scanW, {h: bc.h, s: bc.s, v: cv});
        }

        if (algo.presetAxis === "Vertical") {
            for (var y = 0; y < height; y++) {
                var c = strip[y];
                for (var x = 0; x < width; x++) {
                    var i3 = (y * width + x) * 3;
                    map[i3] = c.h; map[i3+1] = c.s; map[i3+2] = c.v;
                }
            }
        } else {
            for (var y2 = 0; y2 < height; y2++) {
                for (var x2 = 0; x2 < width; x2++) {
                    var c2 = strip[x2];
                    var i3 = (y2 * width + x2) * 3;
                    map[i3] = c2.h; map[i3+1] = c2.s; map[i3+2] = c2.v;
                }
            }
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
