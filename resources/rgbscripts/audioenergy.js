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
    algo.acceptColors = 0;
    algo.usesAudio = true;
    algo.properties = new Array();

    // --- Configurable Properties ---
    algo.presetSensitivity = 6;
    algo.properties.push(
      "name:presetSensitivity|type:range|display:Sensitivity|" +
      "values:1,10|write:setSensitivity|read:getSensitivity");

    algo.presetMixing = 0;
    algo.properties.push(
      "name:presetMixing|type:list|display:Mixing Mode|" +
      "values:Additive,Overlap|write:setMixing|read:getMixing");

    algo.setSensitivity = function(_v) { algo.presetSensitivity = parseInt(_v); };
    algo.getSensitivity = function()  { return algo.presetSensitivity; };
    algo.setMixing = function(_v) { algo.presetMixing = (_v === "Overlap") ? 1 : 0; };
    algo.getMixing = function()  { return algo.presetMixing ? "Overlap" : "Additive"; };

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
        var decay = algo.presetSensitivity / 10.0;
        var rise = 0.6 + (algo.presetSensitivity / 25.0);
        lowsFilter  = new LedFx.ExpFilter(decay, Math.min(rise, 0.99));
        midsFilter  = new LedFx.ExpFilter(decay, Math.min(rise, 0.99));
        highsFilter = new LedFx.ExpFilter(decay, Math.min(rise, 0.99));
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
        var rawLows = LedFx.lows_power(audio);
        var rawMids = LedFx.mids_power(audio);
        var rawHighs = LedFx.high_power(audio);

        // Apply smoothing
        var lows = lowsFilter.update(rawLows);
        var mids = midsFilter.update(rawMids);
        var highs = highsFilter.update(rawHighs);

        // Calculate how many columns each band fills (from left)
        var multiplier = 1.6;
        var lowsIdx  = Math.min(width, Math.floor(multiplier * width * lows));
        var midsIdx  = Math.min(width, Math.floor(multiplier * width * mids));
        var highsIdx = Math.min(width, Math.floor(multiplier * width * highs));

        // Build pixel array (1D concept mapped to columns, mirrored vertically)
        for (var y = 0; y < height; y++)
        {
            // Mirror: fill from both edges toward center
            var halfW = Math.floor(width / 2);

            for (var x = 0; x < width; x++)
            {
                // Distance from center (for mirror effect)
                var dist = Math.abs(x - halfW);
                var pos = halfW - dist; // 0 at edges, halfW at center

                var r = 0, g = 0, b = 0;

                if (algo.presetMixing === 0) {
                    // Additive mode: layer colors
                    if (pos < lowsIdx) {
                        r += lowsColor[0]; g += lowsColor[1]; b += lowsColor[2];
                    }
                    if (pos < midsIdx) {
                        r += midsColor[0]; g += midsColor[1]; b += midsColor[2];
                    }
                    if (pos < highsIdx) {
                        r += highsColor[0]; g += highsColor[1]; b += highsColor[2];
                    }
                } else {
                    // Overlap mode: last band wins
                    if (pos < highsIdx) {
                        r = highsColor[0]; g = highsColor[1]; b = highsColor[2];
                    }
                    if (pos < midsIdx) {
                        r = midsColor[0]; g = midsColor[1]; b = midsColor[2];
                    }
                    if (pos < lowsIdx) {
                        r = lowsColor[0]; g = lowsColor[1]; b = lowsColor[2];
                    }
                }

                map[y][x] = LedFx.rgb(r, g, b);
            }
        }

        return map;
    };

    // For devtool testing
    testAlgo = algo;
    return algo;
  }
)();
