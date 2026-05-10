/*
  Q Light Controller Plus
  audioscanmulti.js

  Copyright (c) QLC+ contributors
  Ported from LedFX "Scan Multi" effect (MIT License)

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
    algo.acceptColors = 3; // low / mid / high band colors from the gradient
    algo.usesAudio = true;
    algo.properties = new Array();

    algo.presetSpeed = 30;
    algo.properties.push(
      "name:presetSpeed|type:range|display:Speed|" +
      "values:1,100|write:setSpeed|read:getSpeed");

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

    algo.setSpeed = function(_v) { algo.presetSpeed = parseInt(_v); };
    algo.getSpeed = function() { return algo.presetSpeed; };
    algo.setWidth = function(_v) { algo.presetWidth = parseInt(_v); };
    algo.getWidth = function() { return algo.presetWidth; };
    algo.setMultiplier = function(_v) { algo.presetMultiplier = parseInt(_v); };
    algo.getMultiplier = function() { return algo.presetMultiplier; };
    algo.setBounce = function(_v) { algo.presetBounce = (_v === "Yes") ? 1 : 0; };
    algo.getBounce = function() { return algo.presetBounce ? "Yes" : "No"; };
    algo.setColorIntensity = function(_v) { algo.presetColorIntensity = (_v === "Yes") ? 1 : 0; };
    algo.getColorIntensity = function() { return algo.presetColorIntensity ? "Yes" : "No"; };
    algo.setAxis = function(_v) { algo.presetAxis = _v; };
    algo.getAxis = function() { return algo.presetAxis; };

    algo.scans = [
        { pos: 0, returning: false },
        { pos: 0, returning: false },
        { pos: 0, returning: false }
    ];
    algo.lastMs = 0;
    algo.lastN = 0;

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() {
        return AudioColors.bands(algo).slice();
    };

    function drawScan(strip, n, startPos, scanW, color) {
        var start = Math.floor(startPos);
        for (var s = 0; s < scanW; s++) {
            var idx = start + s;
            if (idx < 0 || idx >= n) continue;
            strip[idx] = RGBUtil.blendAdd(strip[idx], color);
        }
    }

    algo.rgbMap = function(width, height, rgb, step, audio) {
        var map = RGBUtil.createMap(width, height);
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

        var dtMs = audio.timing.consumerDtMs > 0 ? audio.timing.consumerDtMs : 40;
        var dtSec = dtMs / 1000.0;

        var multiplier = algo.presetMultiplier / 100.0;
        var stepPerSec = (n / 100.0) * algo.presetSpeed;
        var scanW = Math.max(1, Math.round(n * algo.presetWidth / 100.0));
        var bounce = algo.presetBounce === 1;
        var colorIntensity = algo.presetColorIntensity === 1;
        var bandColors = AudioColors.bands(algo);

        var strip = new Array(n);
        for (var p = 0; p < n; p++) strip[p] = 0;

        var maxStart = n - scanW;
        if (maxStart < 0) maxStart = 0;

        for (var b = 0; b < 3; b++) {
            var scan = algo.scans[b];
            var power = audio.power.bands[b];
            var bar = power * multiplier;
            var stepSize = dtSec * stepPerSec * bar;

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

            var color = bandColors[b] | 0;
            if (colorIntensity)
                color = RGBUtil.scaleColor(color, Math.min(1, power));

            drawScan(strip, n, scan.pos, scanW, color);
        }

        if (algo.presetAxis === "Vertical") {
            for (var y = 0; y < height; y++) {
                var c = strip[y];
                for (var x = 0; x < width; x++) map[y][x] = c;
            }
        } else {
            for (var y2 = 0; y2 < height; y2++) {
                for (var x2 = 0; x2 < width; x2++) map[y2][x2] = strip[x2];
            }
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
