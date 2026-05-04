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
    algo.acceptColors = 3; // lows, mids, highs colors
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

    algo.setMixing = function(_v) { algo.presetMixing = (_v === "Overlap") ? 1 : 0; };
    algo.getMixing = function()  { return algo.presetMixing ? "Overlap" : "Additive"; };
    algo.setMultiplier = function(_v) { algo.presetMultiplier = parseInt(_v); };
    algo.getMultiplier = function() { return algo.presetMultiplier; };

    // --- Internal state ---
    var lowsFilter = null;
    var midsFilter = null;
    var highsFilter = null;
    var initialized = false;

    // Colors: lows=red, mids=green, highs=blue
    var lowsColor  = [255, 0, 0];
    var midsColor  = [0, 255, 0];
    var highsColor = [0, 0, 255];

    function initFilters()
    {
        var decay = algo.presetReactivity / 10.0;
        lowsFilter  = AudioParams.createFilter(algo, decay);
        midsFilter  = AudioParams.createFilter(algo, decay);
        highsFilter = AudioParams.createFilter(algo, decay);
        initialized = true;
    }

    algo.rgbMapStepCount = function(width, height)
    {
        // Audio-reactive: always 1 step (real-time driven)
        return 1;
    };

    algo.rgbMapSetColors = function(rawColors)
    {
        // Override colors from palette if provided (3 colors: lows, mids, highs)
        if (rawColors && rawColors.length >= 1) {
            lowsColor  = [(rawColors[0] >> 16) & 0xFF, (rawColors[0] >> 8) & 0xFF, rawColors[0] & 0xFF];
        }
        if (rawColors && rawColors.length >= 2) {
            midsColor  = [(rawColors[1] >> 16) & 0xFF, (rawColors[1] >> 8) & 0xFF, rawColors[1] & 0xFF];
        }
        if (rawColors && rawColors.length >= 3) {
            highsColor = [(rawColors[2] >> 16) & 0xFF, (rawColors[2] >> 8) & 0xFF, rawColors[2] & 0xFF];
        }
    };

    algo.rgbMapGetColors = function()
    {
        return [
            LedFx.rgb(lowsColor[0], lowsColor[1], lowsColor[2]),
            LedFx.rgb(midsColor[0], midsColor[1], midsColor[2]),
            LedFx.rgb(highsColor[0], highsColor[1], highsColor[2])
        ];
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

        var map = LedFx.createMap(width, height);

        // If no audio data, return black
        if (!audio || !audio.spectrum || audio.spectrum.length === 0)
            return map;

        // Get frequency band powers
        var gain = AudioParams.gainFactor(algo);
        var rawLows = LedFx.lows_power(audio) * gain;
        var rawMids = LedFx.mids_power(audio) * gain;
        var rawHighs = LedFx.high_power(audio) * gain;

        // Apply smoothing
        var lows = lowsFilter.update(rawLows);
        var mids = midsFilter.update(rawMids);
        var highs = highsFilter.update(rawHighs);

        // Calculate how many columns each band fills (from left)
        var multiplier = algo.presetMultiplier / 10.0;
        var lowsIdx  = Math.min(width, Math.floor(multiplier * width * lows));
        var midsIdx  = Math.min(width, Math.floor(multiplier * width * mids));
        var highsIdx = Math.min(width, Math.floor(multiplier * width * highs));

        // Build pixel array
        for (var y = 0; y < height; y++)
        {
            for (var x = 0; x < width; x++)
            {
                var r = 0, g = 0, b = 0;

                if (algo.presetMixing === 0) {
                    // Additive mode: layer colors
                    if (x < lowsIdx) {
                        r += lowsColor[0]; g += lowsColor[1]; b += lowsColor[2];
                    }
                    if (x < midsIdx) {
                        r += midsColor[0]; g += midsColor[1]; b += midsColor[2];
                    }
                    if (x < highsIdx) {
                        r += highsColor[0]; g += highsColor[1]; b += highsColor[2];
                    }
                } else {
                    // Overlap mode: last band wins
                    if (x < highsIdx) {
                        r = highsColor[0]; g = highsColor[1]; b = highsColor[2];
                    }
                    if (x < midsIdx) {
                        r = midsColor[0]; g = midsColor[1]; b = midsColor[2];
                    }
                    if (x < lowsIdx) {
                        r = lowsColor[0]; g = lowsColor[1]; b = lowsColor[2];
                    }
                }

                var brightness = (r > 0 || g > 0 || b > 0) ? AudioParams.applyFloor(algo, 1.0) : 0;
                map[y][x] = LedFx.rgb(r * brightness, g * brightness, b * brightness);
            }
        }

        return map;
    };

    // For devtool testing
    testAlgo = algo;
    return algo;
  }
)();
