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
    algo.acceptColors = 2; // start + end for fire color gradient
    algo.usesAudio = true;
    algo.properties = new Array();

    // --- Properties ---
    algo.presetSpeed = 4;
    algo.properties.push(
      "name:presetSpeed|type:range|display:Speed|" +
      "values:1,10|write:setSpeed|read:getSpeed");

    algo.presetIntensity = 8;
    algo.properties.push(
      "name:presetIntensity|type:range|display:Spark Count|" +
      "values:1,20|write:setIntensity|read:getIntensity");

    algo.presetCooling = 5;
    algo.properties.push(
      "name:presetCooling|type:range|display:Cooling|" +
      "values:1,10|write:setCooling|read:getCooling");

    algo.presetDirection = 0;
    algo.properties.push(
      "name:presetDirection|type:list|display:Direction|" +
      "values:Up,Down|write:setDirection|read:getDirection");

    algo.presetSpread = 0;
    algo.properties.push(
      "name:presetSpread|type:list|display:Per Column|" +
      "values:No,Yes|write:setSpread|read:getSpread");

    algo.setSpeed = function(_v) { algo.presetSpeed = parseInt(_v); };
    algo.getSpeed = function()  { return algo.presetSpeed; };
    algo.setIntensity = function(_v) { algo.presetIntensity = parseInt(_v); };
    algo.getIntensity = function()  { return algo.presetIntensity; };
    algo.setCooling = function(_v) { algo.presetCooling = parseInt(_v); };
    algo.getCooling = function() { return algo.presetCooling; };
    algo.setDirection = function(_v) { algo.presetDirection = (_v === "Down") ? 1 : 0; };
    algo.getDirection = function() { return algo.presetDirection ? "Down" : "Up"; };
    algo.setSpread = function(_v) { algo.presetSpread = (_v === "Yes") ? 1 : 0; };
    algo.getSpread = function() { return algo.presetSpread ? "Yes" : "No"; };

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

    // Colors for fire gradient (default: red → yellow)
    var fireColorLow = [255, 0, 0];
    var fireColorHigh = [255, 255, 0];

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) {
        if (rawColors && rawColors.length >= 1)
            fireColorLow = [(rawColors[0] >> 16) & 0xFF, (rawColors[0] >> 8) & 0xFF, rawColors[0] & 0xFF];
        if (rawColors && rawColors.length >= 2)
            fireColorHigh = [(rawColors[1] >> 16) & 0xFF, (rawColors[1] >> 8) & 0xFF, rawColors[1] & 0xFF];
    };
    algo.rgbMapGetColors = function() {
        return [LedFx.rgb(fireColorLow[0], fireColorLow[1], fireColorLow[2]),
                LedFx.rgb(fireColorHigh[0], fireColorHigh[1], fireColorHigh[2])];
    };

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
        var baseCooling = 0.85 + (10 - algo.presetCooling) * 0.015;

        // Audio influence: bass drives the fire
        var rawLows = LedFx.lows_power(audio);
        var lowsPower = lowsFilter.update(rawLows);

        var cooling = baseCooling + lowsPower * 0.15;
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

        // Map heat values to colors using fire gradient and render into 2D grid
        for (var y = 0; y < height; y++) {
            // Direction: Up = bottom-to-top, Down = top-to-bottom
            var fireIdx = algo.presetDirection ? y : (height - 1 - y);
            var heat = Math.max(0, Math.min(1, sparkPixels[fireIdx]));

            // Interpolate between fireColorLow and fireColorHigh based on heat
            var r = fireColorLow[0] + (fireColorHigh[0] - fireColorLow[0]) * heat;
            var g = fireColorLow[1] + (fireColorHigh[1] - fireColorLow[1]) * heat;
            var b = fireColorLow[2] + (fireColorHigh[2] - fireColorLow[2]) * heat;

            // Apply heat as brightness
            var brightness = Math.min(1, heat * 2);
            var packedColor = LedFx.rgb(r * brightness, g * brightness, b * brightness);

            // Fill row — if "Per Column" is on, vary heat per column using spectrum
            if (algo.presetSpread && audio.spectrum) {
                var specBands = LedFx.melbank(audio, width);
                for (var x = 0; x < width; x++) {
                    var colHeat = heat * (0.3 + specBands[x] * 0.7);
                    var cr = fireColorLow[0] + (fireColorHigh[0] - fireColorLow[0]) * colHeat;
                    var cg = fireColorLow[1] + (fireColorHigh[1] - fireColorLow[1]) * colHeat;
                    var cb = fireColorLow[2] + (fireColorHigh[2] - fireColorLow[2]) * colHeat;
                    var cb2 = Math.min(1, colHeat * 2);
                    map[y][x] = LedFx.rgb(cr * cb2, cg * cb2, cb * cb2);
                }
            } else {
                for (var x = 0; x < width; x++)
                    map[y][x] = packedColor;
            }
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
