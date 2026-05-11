/*
  Q Light Controller Plus
  audiogravimeter.js

  Copyright (c) QLC+ contributors

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
    algo.name = "Audio Gravimeter";
    algo.author = "QLC+ contributors";
    algo.acceptColors = 3;
    algo.usesAudio = true;
    algo.properties = new Array();

    algo.presetGravity = 200;
    algo.properties.push(
      "name:presetGravity|type:range|display:Gravity (units/s^2)|" +
      "values:50,800|write:setGravity|read:getGravity");
    algo.setGravity = function(v) { algo.presetGravity = parseInt(v); };
    algo.getGravity = function() { return algo.presetGravity; };

    algo.presetPeakDecay = 50;
    algo.properties.push(
      "name:presetPeakDecay|type:range|display:Peak Decay (units/s)|" +
      "values:5,300|write:setPeakDecay|read:getPeakDecay");
    algo.setPeakDecay = function(v) { algo.presetPeakDecay = parseInt(v); };
    algo.getPeakDecay = function() { return algo.presetPeakDecay; };

    algo.presetPeakHoldMs = 800;
    algo.properties.push(
      "name:presetPeakHoldMs|type:range|display:Peak Hold (ms)|" +
      "values:100,3000|write:setPeakHoldMs|read:getPeakHoldMs");
    algo.setPeakHoldMs = function(v) { algo.presetPeakHoldMs = parseInt(v); };
    algo.getPeakHoldMs = function() { return algo.presetPeakHoldMs; };

    algo.presetPeakFlashMs = 150;
    algo.properties.push(
      "name:presetPeakFlashMs|type:range|display:Peak Flash Decay (ms)|" +
      "values:30,1000|write:setPeakFlashMs|read:getPeakFlashMs");
    algo.setPeakFlashMs = function(v) { algo.presetPeakFlashMs = parseInt(v); };
    algo.getPeakFlashMs = function() { return algo.presetPeakFlashMs; };

    algo.presetBands = "3";
    algo.properties.push(
      "name:presetBands|type:list|display:Band Count|" +
      "values:1,3|write:setBands|read:getBands");
    algo.setBands = function(v) { algo.presetBands = String(v); };
    algo.getBands = function() { return algo.presetBands; };

    algo.presetAxis = "Vertical";
    algo.properties.push(
      "name:presetAxis|type:list|display:Axis|" +
      "values:Vertical,Horizontal|write:setAxis|read:getAxis");
    algo.setAxis = function(v) { algo.presetAxis = String(v); };
    algo.getAxis = function() { return algo.presetAxis; };

    algo.heights    = [0, 0, 0];
    algo.velocities = [0, 0, 0];
    algo.peaks      = [0, 0, 0];
    algo.peakAge    = [0, 0, 0];
    algo.peakFlash  = [0, 0, 0];

    // 0xFF4000=orange-red, 0x00FF64=green, 0x4080FF=blue
    var DEFAULT_BANDS = [
        {h: 0.042, s: 1.0,   v: 1.0},
        {h: 0.399, s: 1.0,   v: 1.0},
        {h: 0.611, s: 0.749, v: 1.0}
    ];
    var BAND_NAMES = ["low", "mid", "high"];

    algo.rgbMapStepCount = function(_w, _h) { return 1; };
    algo.rgbMapSetColors = function(_raw) { };
    algo.rgbMapGetColors = function() {
        return (algo.gradientBandColors && algo.gradientBandColors.length >= 3)
            ? algo.gradientBandColors.slice() : DEFAULT_BANDS.slice();
    };

    algo.blendToWhite = function(c, t) {
        if (t <= 0) return c;
        if (t > 1) t = 1;
        return {h: c.h, s: c.s * (1 - t), v: c.v + (1 - c.v) * t};
    };

    algo.rgbMap = function(width, height, rgb, step, audio) {
        var map = RGBUtil.createMap(width, height);
        var dt = audio.timing.consumerDtMs / 1000.0;
        var nBands = (algo.presetBands === "1") ? 1 : 3;
        var gravity   = algo.presetGravity / 100.0;
        var peakDecay = algo.presetPeakDecay / 100.0;
        var peakHoldMs = algo.presetPeakHoldMs;
        var flashTau   = algo.presetPeakFlashMs / 1000.0;

        var bandPowers = audio.power.bands;

        for (var i = 0; i < nBands; i++) {
            var srcVal = (nBands === 1) ? audio.power.low : bandPowers[i];

            if (srcVal > algo.heights[i]) {
                algo.heights[i] = srcVal;
                algo.velocities[i] = 0;
            } else {
                algo.velocities[i] += gravity * dt;
                var nh = algo.heights[i] - algo.velocities[i] * dt;
                algo.heights[i] = nh < 0 ? 0 : nh;
            }

            if (algo.heights[i] > algo.peaks[i]) {
                algo.peaks[i] = algo.heights[i];
                algo.peakAge[i] = 0;
            } else {
                algo.peakAge[i] += audio.timing.consumerDtMs;
                if (algo.peakAge[i] > peakHoldMs) {
                    var np = algo.peaks[i] - peakDecay * dt;
                    algo.peaks[i] = np < 0 ? 0 : np;
                }
            }

            var fireKick = audio.beat.kick &&
                (i === 0 || audio.power.dominant === BAND_NAMES[i]);
            if (fireKick) algo.peakFlash[i] = 1.0;
            algo.peakFlash[i] *= Math.exp(-dt / flashTau);
        }

        var bandColors = (algo.gradientBandColors && algo.gradientBandColors.length >= 3)
            ? algo.gradientBandColors : DEFAULT_BANDS;
        var horizontal = (algo.presetAxis === "Horizontal");

        var totalLong  = horizontal ? width  : height;
        var totalCross = horizontal ? height : width;

        for (var b = 0; b < nBands; b++) {
            var barColor = bandColors[b];
            var c0 = Math.floor(b * totalCross / nBands);
            var c1 = (b === nBands - 1) ? totalCross - 1 : Math.floor((b + 1) * totalCross / nBands) - 1;
            if (c1 < c0) c1 = c0;

            var rawBarLen = Math.floor(algo.heights[b] * totalLong);
            // Clamp PIXEL extent (not the audio value).
            var barLen = rawBarLen < 0 ? 0 : (rawBarLen > totalLong ? totalLong : rawBarLen);

            var rawPeakIdx = Math.floor(algo.peaks[b] * Math.max(1, totalLong - 1));
            var peakIdx = rawPeakIdx < 0 ? 0 : (rawPeakIdx > totalLong - 1 ? totalLong - 1 : rawPeakIdx);
            var flashColor = algo.blendToWhite(barColor, algo.peakFlash[b]);

            for (var c = c0; c <= c1; c++) {
                if (horizontal) {
                    // bars extend from left
                    for (var x = 0; x < barLen; x++) {
                        if (c >= 0 && c < height && x >= 0 && x < width)
                            RGBUtil.setPixel(map, width, x, c, barColor.h, barColor.s, barColor.v);
                    }
                    if (peakIdx >= 0 && peakIdx < width && c >= 0 && c < height)
                        RGBUtil.setPixel(map, width, peakIdx, c, flashColor.h, flashColor.s, flashColor.v);
                } else {
                    // bars rise from bottom
                    for (var y = 0; y < barLen; y++) {
                        var rowY = height - 1 - y;
                        if (rowY >= 0 && rowY < height && c >= 0 && c < width)
                            RGBUtil.setPixel(map, width, c, rowY, barColor.h, barColor.s, barColor.v);
                    }
                    var prow = height - 1 - peakIdx;
                    if (prow >= 0 && prow < height && c >= 0 && c < width)
                        RGBUtil.setPixel(map, width, c, prow, flashColor.h, flashColor.s, flashColor.v);
                }
            }
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
