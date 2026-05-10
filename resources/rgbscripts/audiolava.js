/*
  Q Light Controller Plus
  audiolava.js

  Copyright (c) QLC+ contributors
  Ported from LedFX "Lava lamp" effect (MIT License)

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
    algo.name = "Audio Lava Lamp";
    algo.author = "Ported from LedFx";
    algo.acceptColors = 3; // low/mid/high mel-bank gradient
    algo.usesAudio = true;
    algo.properties = new Array();

    algo.presetReactivity = 5;
    algo.presetSpeed = 7;
    algo.properties.push(
      "name:presetSpeed|type:range|display:Speed|" +
      "values:1,15|write:setSpeed|read:getSpeed");
    algo.presetContrast = 0.6;
    algo.properties.push(
      "name:presetContrast|type:float|display:Contrast|" +
      "write:setContrast|read:getContrast");

    algo.setSpeed = function(_v) { algo.presetSpeed = parseInt(_v); };
    algo.getSpeed = function() { return algo.presetSpeed; };
    algo.setContrast = function(_v) { algo.presetContrast = parseFloat(_v); };
    algo.getContrast = function() { return algo.presetContrast; };

    var DEFAULT_BAND_COLORS = [0xFF0040, 0xFFAA00, 0x80FF00];
    var BEAT_PULSE_AMP = 0.25;
    var LAVA_RATE_1 = 0.0001;
    var LAVA_RATE_2 = 0.0002;
    var BASS_MOD_1 = 0.004;
    var BASS_MOD_2 = 0.007;
    var WAVE1_Y_FREQ = 0.5;
    var WAVE2_Y_FREQ = 0.7;
    var WAVE4_T_FREQ = 1.7;
    var WAVE4_X_FREQ = 0.6;
    var WAVE5_T_FREQ = 1.3;
    var WAVE5_Y_FREQ = 1.2;
    var MIN_BRIGHTNESS = 0.01;
    var elapsedMs = 0;

    function unpackColor(packed) { return [(packed >> 16) & 0xFF, (packed >> 8) & 0xFF, packed & 0xFF]; }

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() {
        return algo.gradientBandColors ? algo.gradientBandColors.slice() : DEFAULT_BAND_COLORS.slice();
    };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var map = RGBUtil.createMap(width, height);
        if (!audio) return map;

        elapsedMs += audio.timing.consumerDtMs;

        var bandPowers = audio.power.bands;
        var lowPower = bandPowers[0];
        var midsPower = bandPowers[1];
        var highsPower = bandPowers[2];
        var colorStops = algo.gradientBandColors || DEFAULT_BAND_COLORS;
        var colors = [];
        for (var ci = 0; ci < 3; ci++)
            colors.push(unpackColor(colorStops[ci]));
        var speed = algo.presetSpeed;
        var contrast = 1 - algo.presetContrast;

        // Beat-pulse brightness boost
        var beatBoost = 1.0 + BEAT_PULSE_AMP * audio.beat.cosPulse;

        // Time phases — bass accelerates significantly
        var t1 = (elapsedMs * speed * LAVA_RATE_1 * Math.max(1, 1 + lowPower * BASS_MOD_1)) % 1;
        var t2 = (elapsedMs * speed * LAVA_RATE_2 * Math.max(1, 1 + lowPower * BASS_MOD_2)) % 1;

        // True 2D: each pixel gets unique wave value
        for (var y = 0; y < height; y++) {
            var yNorm = y / Math.max(1, height - 1);

            for (var x = 0; x < width; x++) {
                var xNorm = x / Math.max(1, width - 1);

                // Five wave layers for a richer lava pattern
                var w1 = Math.sin((t1 + xNorm + yNorm * WAVE1_Y_FREQ) * Math.PI * 2);
                var w2 = Math.sin((t2 - xNorm + yNorm * WAVE2_Y_FREQ) * Math.PI * 2);
                var w3 = Math.sin((xNorm + yNorm + w1 + w2) * Math.PI * 2);
                var w4 = Math.sin((t1 * WAVE4_T_FREQ + xNorm * WAVE4_X_FREQ - yNorm) * Math.PI * 2);
                var w5 = Math.sin((t2 * WAVE5_T_FREQ - xNorm + yNorm * WAVE5_Y_FREQ) * Math.PI * 2);

                // Combine waves for pattern
                var pattern = (w1 + 0.1) * (w2 + lowPower * 2) * (w3 + midsPower * 1.5) + (w4 * midsPower + w5 * highsPower) * 0.35;
                pattern = Math.pow(Math.max(0, pattern + contrast), 2);
                pattern = (Math.min(1, pattern)) * beatBoost;

                // Mix 3 gradient colors weighted by mel-bank powers
                var r = 0, g = 0, b = 0, total = 0;
                for (var bk = 0; bk < 3; bk++) {
                    r += colors[bk][0] * bandPowers[bk];
                    g += colors[bk][1] * bandPowers[bk];
                    b += colors[bk][2] * bandPowers[bk];
                    total += bandPowers[bk];
                }

                // Normalize and apply pattern
                total = Math.max(MIN_BRIGHTNESS, total);
                map[y][x] = RGBUtil.rgb(
                    r / total * pattern,
                    g / total * pattern,
                    b / total * pattern
                );
            }
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
