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

        var bandPowers = audio.power.bands;
        var bandColors = algo.colors || DEFAULT_BAND_COLORS;

        var beatBoost = 1.0 + BEAT_PULSE_AMP * audio.beat.cosPulse;

        var multiplier = algo.presetMultiplier;
        for (var k = 0; k < 3; k++)
            idx[k] = Math.min(width, Math.floor(multiplier * width * bandPowers[k]));

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
                    var brightness = Math.min(1.0, beatBoost);
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
