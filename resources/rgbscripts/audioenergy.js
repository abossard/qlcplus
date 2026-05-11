/*
  Q Light Controller Plus
  audioenergy.js

  Copyright (c) QLC+ contributors
  Ported from LedFX "Energy" effect (MIT License)
  Original by LedFX contributors: https://github.com/LedFx/LedFx

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

// Development tool access
var testAlgo;

(
  function () {
    var algo = new Object;
    algo.apiVersion = 3;
    algo.name = "Audio Energy";
    algo.author = "Ported from LedFx";
    algo.acceptColors = 3; // low/mid/high mel-bank gradient
    algo.usesAudio = true;
    algo.properties = new Array();

    // --- Configurable Properties ---

    algo.presetMultiplier = 1.6;
    algo.properties.push(
      "name:presetMultiplier|type:float|display:Fill Amount|" +
      "write:setMultiplier|read:getMultiplier");

    algo.setMultiplier = function(_v) { algo.presetMultiplier = parseFloat(_v); };
    algo.getMultiplier = function() { return algo.presetMultiplier; };

    // --- Internal state ---
    // Default 3-bank palette (low, mid, high).
    var DEFAULT_BAND_COLORS = [0xFF0040, 0xFFFF00, 0x4080FF];
    var BEAT_PULSE_AMP = 0.25;
    var cols = [[0, 0, 0], [0, 0, 0], [0, 0, 0]];
    var idx = new Array(3);

    algo.rgbMapStepCount = function(width, height)
    {
        // Audio-reactive: always 1 step (real-time driven)
        return 1;
    };

    // Required by apiVersion 3 loader; ignored because we read from the
    // auto-injected algo.gradientBandColors instead.
    algo.rgbMapSetColors = function(rawColors) { };

    algo.rgbMapGetColors = function()
    {
        return algo.gradientBandColors
            ? algo.gradientBandColors.slice()
            : DEFAULT_BAND_COLORS.slice();
    };

    /**
     * Main render function.
     * @param {object} audio  - Audio frame snapshot from the v4 audio engine.
     */
    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var map = RGBUtil.createFlatMap(width, height);

        // If no audio data, return black
        if (!audio)
            return map;

        // Pull the 3 mel-bank powers and matching gradient colors.
        var bandPowers = audio.power.bands;
        var bandColors = algo.gradientBandColors || DEFAULT_BAND_COLORS;

        // Beat-pulse brightness boost
        var beatBoost = 1.0 + BEAT_PULSE_AMP * audio.beat.cosPulse;

        // Convert each packed 0xRRGGBB to a [r,g,b] array.
        for (var k = 0; k < 3; k++) {
            var packed = bandColors[k] | 0;
            cols[k][0] = (packed >> 16) & 0xFF;
            cols[k][1] = (packed >> 8) & 0xFF;
            cols[k][2] = packed & 0xFF;
        }

        // Calculate how many columns each band fills (from left)
        var multiplier = algo.presetMultiplier;
        for (var k = 0; k < 3; k++)
            idx[k] = Math.min(width, Math.floor(multiplier * width * bandPowers[k]));

        // Build pixel array
        for (var y = 0; y < height; y++)
        {
            for (var x = 0; x < width; x++)
            {
                var r = 0, g = 0, b2 = 0;

                // Use widest-reaching band's color (highest index covering x wins)
                for (var k = 0; k < 3; k++) {
                    if (x < idx[k]) {
                        r = cols[k][0]; g = cols[k][1]; b2 = cols[k][2];
                    }
                }

                var brightness = (r > 0 || g > 0 || b2 > 0) ? (1.0) * beatBoost : 0;
                map[(y) * width + (x)] = RGBUtil.rgb(r * brightness, g * brightness, b2 * brightness);
            }
        }

        return map;
    };

    // For devtool testing
    testAlgo = algo;
    return algo;
  }
)();
