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

    AudioParams.installContinuous(algo, {gain: 5, reactivity: 5});

    // --- Configurable Properties ---

    algo.presetMixing = 0;
    algo.properties.push(
      "name:presetMixing|type:list|display:Mixing Mode|" +
      "values:Additive,Overlap|write:setMixing|read:getMixing");

    algo.presetMultiplier = 16;
    algo.properties.push(
      "name:presetMultiplier|type:range|display:Fill Amount|" +
      "values:5,30|write:setMultiplier|read:getMultiplier");
    AudioParams.installBandPowerControls(algo);

    algo.setMixing = function(_v) { algo.presetMixing = (_v === "Overlap") ? 1 : 0; };
    algo.getMixing = function()  { return algo.presetMixing ? "Overlap" : "Additive"; };
    algo.setMultiplier = function(_v) { algo.presetMultiplier = parseInt(_v); };
    algo.getMultiplier = function() { return algo.presetMultiplier; };

    // --- Internal state ---
    var initialized = false;

    // Default 3-bank palette (low, mid, high).
    var DEFAULT_BAND_COLORS = [0xFF0040, 0xFFFF00, 0x4080FF];

    function initFilters()
    {
        initialized = true;
    }

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
     * @param {number} width  - Grid width
     * @param {number} height - Grid height
     * @param {number} rgb    - Current color from RGBMatrix (unused, we use our own)
     * @param {number} step   - Current step (always 0 for audio effects)
     * @param {object} audio  - Audio data: { spectrum[], volume, beat, bpm, maxMagnitude }
     */
    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        if (!initialized) initFilters();

        var map = RGBUtil.createMap(width, height);

        // If no audio data, return black
        if (!audio || !audio.mel || audio.mel.length === 0)
            return map;

        // Pull the 3 mel-bank powers and matching gradient colors.
        var bandPowers = AudioParams.bandWeights(algo, audio);
        var bandColors = algo.gradientBandColors || DEFAULT_BAND_COLORS;

        // Beat-pulse brightness boost
        var beatBoost = 1.0 + 0.25 * AudioParams.beatPulse(audio);

        // Convert each packed 0xRRGGBB to a [r,g,b] array.
        var cols = new Array(3);
        for (var k = 0; k < 3; k++) {
            var packed = bandColors[k] | 0;
            cols[k] = [(packed >> 16) & 0xFF, (packed >> 8) & 0xFF, packed & 0xFF];
        }

        // Calculate how many columns each band fills (from left)
        var multiplier = algo.presetMultiplier / 10.0;
        var idx = new Array(3);
        for (var k = 0; k < 3; k++)
            idx[k] = Math.min(width, Math.floor(multiplier * width * bandPowers[k]));

        // Build pixel array
        for (var y = 0; y < height; y++)
        {
            for (var x = 0; x < width; x++)
            {
                var r = 0, g = 0, b2 = 0;

                if (algo.presetMixing === 0) {
                    // Additive mode: layer all bands whose bar covers x.
                    for (var k = 0; k < 3; k++) {
                        if (x < idx[k]) {
                            r += cols[k][0];
                            g += cols[k][1];
                            b2 += cols[k][2];
                        }
                    }
                } else {
                    // Overlap mode: lowest-frequency band covering x wins.
                    for (var k = 2; k >= 0; k--) {
                        if (x < idx[k]) {
                            r = cols[k][0]; g = cols[k][1]; b2 = cols[k][2];
                        }
                    }
                }

                var brightness = (r > 0 || g > 0 || b2 > 0) ? AudioParams.applyFloor(algo, 1.0) * beatBoost : 0;
                map[y][x] = RGBUtil.rgb(r * brightness, g * brightness, b2 * brightness);
            }
        }

        return map;
    };

    // For devtool testing
    testAlgo = algo;
    return algo;
  }
)();
