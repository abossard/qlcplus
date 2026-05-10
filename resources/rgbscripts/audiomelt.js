/*
  Q Light Controller Plus
  audiomelt.js

  Copyright (c) QLC+ contributors
  Ported from LedFX "Melt" effect (MIT License)

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
    algo.name = "Audio Melt";
    algo.author = "Ported from LedFx";
    algo.acceptColors = 3; // low/mid/high mel-bank gradient
    algo.usesAudio = true;
    algo.properties = new Array();

    algo.presetReactivity = 5;
    algo.presetFloor = 0;

    algo.presetSpeed = 5;
    algo.properties.push(
      "name:presetSpeed|type:range|display:Speed|" +
      "values:1,10|write:setSpeed|read:getSpeed");
    algo.presetColorSpeed = 5;
    algo.properties.push(
      "name:presetColorSpeed|type:range|display:Color Speed|" +
      "values:1,10|write:setColorSpeed|read:getColorSpeed");

    algo.setSpeed = function(_v) { algo.presetSpeed = parseInt(_v); };
    algo.getSpeed = function() { return algo.presetSpeed; };
    algo.setColorSpeed = function(_v) { algo.presetColorSpeed = parseInt(_v); };
    algo.getColorSpeed = function() { return algo.presetColorSpeed; };

    var DEFAULT_BAND_COLORS = [0x8000FF, 0x4066D0, 0x00FF80];
    var timestep = 0;

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() {
        return AudioColors.bands(algo).slice();
    };


    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var map = RGBUtil.createMap(width, height);
        if (!audio) return map;

        var dt = audio.timing.consumerDtMs;

        var lowPower = audio.power.low;

        // Accumulate time with audio reactivity
        var speed = algo.presetSpeed / 10.0;
        var reactivity = algo.presetReactivity / 10.0;
        timestep += dt;
        timestep += lowPower * reactivity / speed * 50;

        var t1 = (timestep * speed * 0.0005) % 1;
        var t2 = (timestep * speed * 0.00065) % 1;
        var colorT = (timestep * algo.presetColorSpeed * 0.0001) % 1;
        var blendedPacked = AudioColors.blendByPower(algo, audio);
        var blended = [(blendedPacked >> 16) & 0xFF, (blendedPacked >> 8) & 0xFF, blendedPacked & 0xFF];
        var beatBoost = 1.0 + 0.20 * audio.beat.cosPulse;
        var noveltyBoost = AudioColors.noveltyBoost(audio);
        var fluxPunch = AudioColors.fluxPunch(audio);

        for (var x = 0; x < width; x++) {
            var il = 1 - x / Math.max(1, width - 1);

            // Melt: layered sine waves creating organic patterns
            var v = Math.sin((il + t1) * Math.PI * 2);
            v = Math.sin((v + t1) * Math.PI * 2);
            v = Math.sin((v + t1) * Math.PI * 2);
            v = v * v; // Square for contrast

            var huePos = (il + t2 + colorT) % 1;
            var colorScale = 0.65 + huePos * 0.35;
            var r = blended[0] * colorScale;
            var g = blended[1] * colorScale;
            var b = blended[2] * colorScale;

            var floored = algo.presetFloor/100 + (1 - algo.presetFloor/100) * v;
            var bright = Math.min(1, floored * fluxPunch) * beatBoost * noveltyBoost;
            var packed = RGBUtil.rgb(r * bright, g * bright, b * bright);

            for (var y = 0; y < height; y++)
                map[y][x] = packed;
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
