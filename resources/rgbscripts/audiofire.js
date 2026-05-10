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
    algo.acceptColors = 3; // spectrum heat gradient
    algo.usesAudio = true;
    algo.properties = new Array();

    algo.presetReactivity = 9;
    // --- Properties ---
    algo.presetSpeed = 0.04;
    algo.properties.push(
      "name:presetSpeed|type:float|display:Speed|" +
      "write:setSpeed|read:getSpeed");

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

    algo.presetSparkFade = 0.05;
    algo.properties.push(
      "name:presetSparkFade|type:float|display:Spark Fade|" +
      "write:setSparkFade|read:getSparkFade");

    algo.setSpeed = function(_v) { algo.presetSpeed = parseFloat(_v); };
    algo.getSpeed = function()  { return algo.presetSpeed; };
    algo.setIntensity = function(_v) { algo.presetIntensity = parseInt(_v); };
    algo.getIntensity = function()  { return algo.presetIntensity; };
    algo.setCooling = function(_v) { algo.presetCooling = parseInt(_v); };
    algo.getCooling = function() { return algo.presetCooling; };
    algo.setDirection = function(_v) { algo.presetDirection = (_v === "Down") ? 1 : 0; };
    algo.getDirection = function() { return algo.presetDirection ? "Down" : "Up"; };
    algo.setSpread = function(_v) { algo.presetSpread = (_v === "Yes") ? 1 : 0; };
    algo.getSpread = function() { return algo.presetSpread ? "Yes" : "No"; };

    algo.setSparkFade = function(_v) { algo.presetSparkFade = parseFloat(_v); };
    algo.getSparkFade = function() { return algo.presetSparkFade; };
    // --- Internal state ---
    var COOL_FLOOR = 0.85;
    var COOL_STEP = 0.015;
    var COOL_BASS_BOOST = 0.15;
    var SPEED_BASS_BOOST = 0.01;
    var SPARK_MIN = 0.5;
    var SPARK_BLEED = 0.4;
    var SPARK_AMP = 0.5;
    var MIX_SPREAD = 0.7;
    var MIX_SINGLE = 0.35;
    var BEAT_MOD_AMP = 0.15;

    var sparkPixels = null;
    var sparks = null;
    var sparkX = null;

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
    }

    // Colors for fire gradient. algo.gradientColors is auto-injected by C++.
    var DEFAULT_GRADIENT = [0x200000, 0xAA0000, 0xFF5500, 0xFFFF00, 0xFFFFFF];
    var columnLut = null;
    var columnLutWidth = -1;
    var columnLutSig = "";
    function unpackColor(packed) { return [(packed >> 16) & 0xFF, (packed >> 8) & 0xFF, packed & 0xFF]; }

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() {
        return algo.gradientColors ? algo.gradientColors.slice() : DEFAULT_GRADIENT.slice();
    };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var pixelCount = height; // fire rises vertically (bottom to top)
        if (sparkPixels === null || sparkPixels.length !== pixelCount) init(pixelCount);

        var map = RGBUtil.createMap(width, height);

        if (!audio) return map;
        var melSrc = audio.spectrum.full;
        if (!melSrc || melSrc.length === 0)
            return map;

        var deltaMs = audio.timing.consumerDtMs;

        var speed = algo.presetSpeed;
        var baseCooling = COOL_FLOOR + (10 - algo.presetCooling) * COOL_STEP;

        // Audio influence: bass drives the fire
        var bandPowers = audio.power.bands;
        var lowPower = bandPowers[0];

        var cooling = baseCooling + lowPower * COOL_BASS_BOOST;
        var adjustedSpeed = speed + lowPower * SPEED_BASS_BOOST;
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
                sparks[i] = SPARK_MIN + Math.random() * SPARK_MIN;
                sparkX[i] = Math.random() * 5;
            }

            var stepSize = sparks[i] * sparks[i] * deltaScaled * (pixelCount / 100);
            sparkX[i] += stepSize;

            // Random fade or out of bounds
            if (sparkX[i] >= pixelCount || Math.random() < algo.presetSparkFade) {
                sparks[i] = 0;
                sparkX[i] = 0;
                continue;
            }

            // Heat up pixels where sparks pass
            var jStart = Math.max(0, Math.floor(sparkX[i] - stepSize));
            var jEnd = Math.floor(sparkX[i]);
            for (var j = jStart; j < jEnd && j < pixelCount; j++) {
                sparkPixels[j] += Math.max(0, Math.min(1, 1 - sparks[i] * SPARK_BLEED)) * SPARK_AMP;
            }
        }

        var stops = (algo.gradientColors && algo.gradientColors.length > 0) ? algo.gradientColors : DEFAULT_GRADIENT;
        var sig = stops.length + ":" + stops.join(",");
        if (columnLut === null || columnLutWidth !== width || columnLutSig !== sig) {
            columnLut = RGBUtil.gradientLut(stops, width);
            columnLutWidth = width;
            columnLutSig = sig;
        }

        var spectrum = melSrc;
        var specBands = RGBUtil.interpolate(spectrum, algo.displayWidth);
        var spectrumMix = algo.presetSpread ? MIX_SPREAD : MIX_SINGLE;
        var beatMod = 1 + audio.beat.cosPulse * BEAT_MOD_AMP;

        // Map heat values to colors using fire gradient and render into 2D grid
        for (var y = 0; y < height; y++) {
            // Direction: Up = bottom-to-top, Down = top-to-bottom
            var fireIdx = algo.presetDirection ? y : (height - 1 - y);
            var heat = Math.max(0, Math.min(1, sparkPixels[fireIdx]));

            // Fill row with per-column gradient color and spectrum-scaled heat.
            for (var x = 0; x < width; x++) {
                var colHeat = heat * ((1 - spectrumMix) + specBands[x] * spectrumMix);
                var packed = columnLut[x];
                var cr = (packed >> 16) & 0xFF;
                var cg = (packed >> 8) & 0xFF;
                var cb = packed & 0xFF;
                var cb2 = (Math.min(1, colHeat * 2)) * beatMod;
                map[y][x] = RGBUtil.rgb(cr * cb2, cg * cb2, cb * cb2);
            }
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
