/*
  Q Light Controller Plus
  audioenergy2.js

  Copyright (c) QLC+ contributors
  Ported from LedFX "Energy 2" effect (MIT License)
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
    algo.name = "Audio Energy 2";
    algo.author = "Ported from LedFx";
    algo.acceptColors = 0;
    algo.usesAudio = true;
    algo.properties = new Array();

    algo.presetSpeed = 30;
    algo.properties.push(
      "name:presetSpeed|type:range|display:Speed|" +
      "values:1,100|write:setSpeed|read:getSpeed");

    algo.presetReactivity = 40;
    algo.properties.push(
      "name:presetReactivity|type:range|display:Reactivity|" +
      "values:1,100|write:setReactivity|read:getReactivity");

    algo.setSpeed = function(_v) { algo.presetSpeed = parseInt(_v); };
    algo.getSpeed = function() { return algo.presetSpeed; };
    algo.setReactivity = function(_v) { algo.presetReactivity = parseInt(_v); };
    algo.getReactivity = function() { return algo.presetReactivity; };

    // Phase advance per second at presetSpeed = 100. Tuned so default speed (30)
    // gives a slow, musical drift similar to LedFx default.
    var SPEED_SCALE = 0.5;

    algo.phase = 0;

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var map = RGBUtil.createMap(width, height);
        if (!audio) return map;

        var dt = audio.timing.consumerDtMs / 1000.0;

        var speed = algo.presetSpeed / 100.0;
        var reactivity01 = algo.presetReactivity / 100.0;
        var reactivity = reactivity01 * 5.0;
        var lowPower = audio.power.low;

        algo.phase = RGBUtil.mod1(algo.phase + dt * speed * SPEED_SCALE);

        var hue = RGBUtil.mod1(lowPower + algo.phase);
        var satThreshold = 0.9 - (reactivity01 + 0.3) * lowPower;
        var offset = 2.0 * RGBUtil.sin01(algo.phase + reactivity01 * lowPower);

        var n = Math.max(2, width);
        for (var x = 0; x < width; x++) {
            var u = x / (n - 1);
            var v0 = RGBUtil.mod1(u + offset);
            var tri = RGBUtil.triangle(v0);
            var v = tri * tri;
            var sat = v < satThreshold ? 1 : 0;
            var packed = RGBUtil.hsvToRgb(hue, sat, v);
            for (var y = 0; y < height; y++)
                map[y][x] = packed;
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
