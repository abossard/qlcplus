/*
  Q Light Controller Plus
  audiofire.js

  Copyright (c) QLC+ contributors
  Ported from LedFX "Fire" effect (MIT License)
  Original by LedFX contributors: https://github.com/LedFx/LedFx

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
    algo.name = "Audio Fire";
    algo.author = "Ported from LedFx";
    algo.acceptColors = 0;
    algo.usesAudio = true;
    algo.properties = new Array();

    // --- Properties ---
    algo.presetSpeed = 4;
    algo.properties.push(
      "name:presetSpeed|type:range|display:Speed|" +
      "values:1,10|write:setSpeed|read:getSpeed");

    algo.presetIntensity = 8;
    algo.properties.push(
      "name:presetIntensity|type:range|display:Intensity|" +
      "values:1,20|write:setIntensity|read:getIntensity");

    algo.presetColorShift = 15;
    algo.properties.push(
      "name:presetColorShift|type:range|display:Color Shift|" +
      "values:0,100|write:setColorShift|read:getColorShift");

    algo.setSpeed = function(_v) { algo.presetSpeed = parseInt(_v); };
    algo.getSpeed = function()  { return algo.presetSpeed; };
    algo.setIntensity = function(_v) { algo.presetIntensity = parseInt(_v); };
    algo.getIntensity = function()  { return algo.presetIntensity; };
    algo.setColorShift = function(_v) { algo.presetColorShift = parseInt(_v); };
    algo.getColorShift = function()  { return algo.presetColorShift; };

    // --- Internal state ---
    var sparkPixels = null;
    var sparks = null;
    var sparkX = null;
    var lowsFilter = null;
    var initialized = false;
    var lastTime = 0;

    function init(pixelCount)
    {
        sparkPixels = new Array(pixelCount);
        for (var i = 0; i < pixelCount; i++) sparkPixels[i] = 0;

        var sparkCount = algo.presetIntensity;
        sparks = new Array(sparkCount);
        sparkX = new Array(sparkCount);
        for (var i = 0; i < sparkCount; i++) {
            sparks[i] = 0;
            sparkX[i] = Math.random() * 5;
        }

        lowsFilter = new LedFx.ExpFilter(0.05, 0.99);
        initialized = true;
        lastTime = Date.now();
    }

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var pixelCount = height; // fire rises vertically (bottom to top)
        if (!initialized || sparkPixels.length !== pixelCount) init(pixelCount);

        var map = LedFx.createMap(width, height);

        if (!audio || !audio.spectrum || audio.spectrum.length === 0)
            return map;

        // Time delta
        var now = Date.now();
        var deltaMs = now - lastTime;
        lastTime = now;
        if (deltaMs <= 0 || deltaMs > 200) deltaMs = 25;

        var speed = algo.presetSpeed / 100.0;
        var colorShift = algo.presetColorShift / 100.0;

        // Audio influence: bass drives the fire
        var rawLows = LedFx.lows_power(audio);
        var lowsPower = lowsFilter.update(rawLows);

        var cooling = 0.75 + lowsPower * 0.25;
        var accel = 0.02 + lowsPower * 0.1;
        var adjustedSpeed = speed + lowsPower * 0.01;
        var deltaScaled = deltaMs * adjustedSpeed;

        // Cool all pixels
        for (var i = 0; i < pixelCount; i++)
            sparkPixels[i] *= cooling;

        // Heat diffusion (spread upward)
        if (pixelCount > 5) {
            for (var i = pixelCount - 1; i >= 5; i--) {
                sparkPixels[i] = (
                    sparkPixels[i - 1] +
                    sparkPixels[i - 2] +
                    sparkPixels[i - 3] * 2 +
                    sparkPixels[i - 4] * 3
                ) / 7;
            }
        }

        var sparkCount = sparks.length;

        // Advance sparks
        for (var i = 0; i < sparkCount; i++) {
            if (sparks[i] <= 0) {
                // Respawn dead spark
                sparks[i] = 0.5 + Math.random() * 0.5;
                sparkX[i] = Math.random() * 5;
            }

            var stepSize = sparks[i] * sparks[i] * deltaScaled * (pixelCount / 100);
            sparkX[i] += stepSize;

            // Random fade or out of bounds
            if (sparkX[i] >= pixelCount || Math.random() < 0.05) {
                sparks[i] = 0;
                sparkX[i] = 0;
                continue;
            }

            // Heat up pixels where sparks pass
            var jStart = Math.max(0, Math.floor(sparkX[i] - stepSize));
            var jEnd = Math.floor(sparkX[i]);
            for (var j = jStart; j < jEnd && j < pixelCount; j++) {
                sparkPixels[j] += Math.max(0, Math.min(1, 1 - sparks[i] * 0.4)) * 0.5;
            }
        }

        // Map heat values to HSV colors and render into the 2D grid
        for (var y = 0; y < height; y++) {
            // Map row: bottom of grid = index 0 of fire
            var fireIdx = height - 1 - y;
            var heat = Math.max(0, Math.min(1, sparkPixels[fireIdx]));

            // HSV mapping (fire palette):
            // Hue: heat^2 * 0.1 + colorShift (red-orange-yellow)
            // Saturation: 1 - (heat-1)*2 (white-hot at full heat)
            // Value: heat * 2 (brightness from heat)
            var h = heat * heat * 0.1 + colorShift;
            var s = Math.max(0, Math.min(1, 1 - (heat - 1) * 2));
            var v = Math.max(0, Math.min(1, heat * 2));

            var color = LedFx.hsv2rgb(h, s, v);
            var packedColor = LedFx.rgb(color[0], color[1], color[2]);

            // Fill entire row with same color (fire column)
            for (var x = 0; x < width; x++)
                map[y][x] = packedColor;
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
