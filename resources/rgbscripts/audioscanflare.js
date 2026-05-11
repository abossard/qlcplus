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

    algo.setSpeed = function(_v) { algo.presetSpeed = parseFloat(_v); };
    algo.getSpeed = function() { return algo.presetSpeed; };
    algo.setWidth = function(_v) { algo.presetWidth = parseInt(_v); };
    algo.getWidth = function() { return algo.presetWidth; };
    algo.setMultiplier = function(_v) { algo.presetMultiplier = parseInt(_v); };
    algo.getMultiplier = function() { return algo.presetMultiplier; };
    algo.setBounce = function(_v) { algo.presetBounce = (_v === "Yes") ? 1 : 0; };
    algo.getBounce = function() { return algo.presetBounce ? "Yes" : "No"; };
    algo.setMaxSparkles = function(_v) { algo.presetMaxSparkles = parseInt(_v); };
    algo.getMaxSparkles = function() { return algo.presetMaxSparkles; };
    algo.setSparkleSize = function(_v) { algo.presetSparkleSize = parseInt(_v); };
    algo.getSparkleSize = function() { return algo.presetSparkleSize; };
    algo.setSparkleTime = function(_v) { algo.presetSparkleTime = parseInt(_v); };
    algo.getSparkleTime = function() { return algo.presetSparkleTime; };
    algo.setSparkleThreshold = function(_v) { algo.presetSparkleThreshold = parseInt(_v); };
    algo.getSparkleThreshold = function() { return algo.presetSparkleThreshold; };
    algo.setColorIntensity = function(_v) { algo.presetColorIntensity = (_v === "Yes") ? 1 : 0; };
    algo.getColorIntensity = function() { return algo.presetColorIntensity ? "Yes" : "No"; };
    algo.setAxis = function(_v) { algo.presetAxis = _v; };
    algo.getAxis = function() { return algo.presetAxis; };

    var SPARKLE_MIN_INTERVAL_MS = 50;

    algo.scanPos = 0;
    algo.returning = false;
    algo.sparkles = [];
    algo.lastSparkleMs = 0;
    algo.elapsedMs = 0;
    algo.lastN = 0;

    function bandColors() {
        if (algo.gradientBandColors && algo.gradientBandColors.length > 0)
            return algo.gradientBandColors;
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

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() {
        return bandColors().slice();
    };

    algo.rgbMap = function(width, height, rgb, step, audio) {
        var map = RGBUtil.createMap(width, height);
        if (!audio) return map;

        var n = (algo.presetAxis === "Vertical") ? height : width;
        if (n <= 0) return map;

        if (algo.lastN !== n) {
            algo.scanPos = 0;
            algo.returning = false;
            algo.sparkles = [];
            algo.lastN = n;
        }

        var dt = audio.timing.consumerDtMs / 1000.0;
        algo.elapsedMs += audio.timing.consumerDtMs;

        var bpm = (audio && audio.beat) ? audio.beat.bpm : 0;
        var bpmEff = (bpm > 0) ? bpm : 120;
        var beatsPerSec = bpmEff / 60.0;

        var power = audio.power.low;
        var multiplier = algo.presetMultiplier / 100.0;
        var bar = power * multiplier;
        var scanW = Math.max(1, Math.round(n * algo.presetWidth / 100.0));
        var stepPerSec = Math.max(1, n - scanW) * algo.presetSpeed * beatsPerSec;
        var stepSize = dt * stepPerSec * bar;
        var bounce = algo.presetBounce === 1;

        if (algo.returning) algo.scanPos -= stepSize;
        else algo.scanPos += stepSize;

        var maxStart = n - scanW;
        if (maxStart < 0) maxStart = 0;

        if (bounce) {
            if (algo.scanPos > maxStart) { algo.scanPos = maxStart; algo.returning = true; }
            if (algo.scanPos < 0) { algo.scanPos = 0; algo.returning = false; }
        } else {
            algo.scanPos = ((algo.scanPos % n) + n) % n;
            if (algo.scanPos > maxStart) algo.scanPos = algo.scanPos - n;
        }

        var threshold = algo.presetSparkleThreshold / 100.0;
        if (power > threshold &&
            algo.sparkles.length < algo.presetMaxSparkles &&
            (algo.elapsedMs - algo.lastSparkleMs) >= SPARKLE_MIN_INTERVAL_MS) {

            var trailingPos = algo.returning
                ? (algo.scanPos + scanW)
                : algo.scanPos;
            var sparkleW = Math.max(1, Math.round(scanW * algo.presetSparkleSize / 100.0));
            var sparkleSpeed = stepPerSec * (algo.returning ? 1 : -1);

            algo.sparkles.push({
                pos: trailingPos,
                width: sparkleW,
                speed: sparkleSpeed,
                bornMs: algo.elapsedMs,
                dieMs: algo.presetSparkleTime
            });
            algo.lastSparkleMs = algo.elapsedMs;
        }

        var strip = new Array(n);
        for (var p = 0; p < n; p++) strip[p] = {h: 0, s: 0, v: 0};

        var bc = bandColors();
        var scanHsv = {h: bc[0].h, s: bc[0].s, v: bc[0].v};
        if (algo.presetColorIntensity === 1)
            scanHsv = {h: scanHsv.h, s: scanHsv.s, v: scanHsv.v * Math.min(1, power)};
        drawSegment(strip, n, algo.scanPos, scanW, scanHsv);

        var alive = [];
        for (var i = 0; i < algo.sparkles.length; i++) {
            var sp = algo.sparkles[i];
            var age = algo.elapsedMs - sp.bornMs;
            var health = 1 - (age / sp.dieMs);
            if (health <= 0) continue;
            sp.pos += sp.speed * dt * health;
            if (sp.pos < -sp.width || sp.pos >= n) continue;
            var sparkleColor = {h: 0, s: 0, v: health};
            drawSegment(strip, n, sp.pos, sp.width, sparkleColor);
            alive.push(sp);
        }
        algo.sparkles = alive;

        if (algo.presetAxis === "Vertical") {
            for (var y = 0; y < height; y++) {
                var pix = strip[y];
                for (var x = 0; x < width; x++)
                    RGBUtil.setPixel(map, width, x, y, pix.h, pix.s, pix.v);
            }
        } else {
            for (var y2 = 0; y2 < height; y2++) {
                for (var x2 = 0; x2 < width; x2++)
                    RGBUtil.setPixel(map, width, x2, y2, strip[x2].h, strip[x2].s, strip[x2].v);
            }
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
