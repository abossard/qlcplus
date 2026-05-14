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

var testAlgo;

(
  function () {
    var algo = new Object;
    algo.apiVersion = 3;
    algo.name = "Audio Energy";
    algo.author = "Ported from LedFx";
    algo.acceptColors = 3;
    algo.usesAudio = true;
    algo.properties = new Array();

    algo.presetMultiplier = 1.6;
    algo.properties.push(
      "name:presetMultiplier|type:float|display:Fill Amount|" +
      "write:setMultiplier|read:getMultiplier");

    algo.setMultiplier = function(_v) { algo.presetMultiplier = parseFloat(_v); };
    algo.getMultiplier = function() { return algo.presetMultiplier; };

    algo.presetSmoothing = 5;
    algo.properties.push(
      "name:presetSmoothing|type:range|display:Smoothing|" +
      "values:1,10|write:setSmoothing|read:getSmoothing");
    algo.setSmoothing = function(_v) { algo.presetSmoothing = parseInt(_v); };
    algo.getSmoothing = function() { return algo.presetSmoothing; };

    var smoothPow = [0, 0, 0];

    // Bar-level build-up / release state
    var barEnergy = 0;
    var peakEnergy = 0;
    var releaseFlash = 0;

    var DEFAULT_BAND_COLORS = [
      {h: 0.958, s: 1.0, v: 1.0},
      {h: 0.167, s: 1.0, v: 1.0},
      {h: 0.611, s: 0.75, v: 1.0}
    ];
    var BEAT_PULSE_AMP = 0.25;
    var idx = new Array(3);

    algo.rgbMapStepCount = function(width, height)
    {
        return 1;
    };

    algo.rgbMapSetColors = function(rawColors) { };

    algo.rgbMapGetColors = function()
    {
        return algo.colors
            ? algo.colors.slice()
            : DEFAULT_BAND_COLORS.slice();
    };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var map = HSVUtil.createMap(width, height);

        if (!audio)
            return map;

        var rawPows = [audio.low, audio.mid, audio.high];
        // Asymmetric EMA: fast attack, slow decay on bar-fill power
        var smoothing = algo.presetSmoothing / 10.0;
        var riseAlpha = 0.5 * (1 - smoothing) + 0.05;
        var decayAlpha = 0.02 + 0.03 * (1 - smoothing);
        for (var sb = 0; sb < 3; sb++) {
            var sa = rawPows[sb] > smoothPow[sb] ? riseAlpha : decayAlpha;
            smoothPow[sb] += sa * (rawPows[sb] - smoothPow[sb]);
        }
        var bandPowers = smoothPow;
        var bandColors = algo.colors || DEFAULT_BAND_COLORS;

        // --- Bar-level build-up ---
        var rawEnergy = (rawPows[0] + rawPows[1] * 0.5 + rawPows[2] * 0.3) / 1.8;
        barEnergy += rawEnergy * audio.dt;
        if (barEnergy > peakEnergy) peakEnergy = barEnergy;
        if (audio.downbeat) {
            releaseFlash = Math.min(1, peakEnergy * 0.5);
            barEnergy = 0;
            peakEnergy = 0;
        }
        releaseFlash *= 0.85;

        var beatBoost = 1.0 + BEAT_PULSE_AMP * audio.cosPulse;

        var multiplier = algo.presetMultiplier;
        // Release momentarily extends bar fill so bars "breathe" with the phrase
        var fillBoost = 1.0 + releaseFlash * 0.35;
        for (var k = 0; k < 3; k++)
            idx[k] = Math.min(width, Math.floor(multiplier * fillBoost * width * bandPowers[k]));

        for (var y = 0; y < height; y++)
        {
            for (var x = 0; x < width; x++)
            {
                // Widest-reaching band's color wins (last band covering x)
                var col = null;
                for (var k = 0; k < 3; k++) {
                    if (x < idx[k]) col = bandColors[k];
                }

                if (col) {
                    var brightness = Math.min(1.0, beatBoost + releaseFlash * 0.4);
                    HSVUtil.setPixel(map, width, x, y, col.h, col.s, col.v * brightness);
                }
            }
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
