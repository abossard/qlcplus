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

    algo.presetSpeed = 0.075;
    algo.properties.push(
      "name:presetSpeed|type:float|display:Speed (cyc/beat)|" +
      "write:setSpeed|read:getSpeed");

    algo.presetReactivity = 0.4;
    algo.properties.push(
      "name:presetReactivity|type:float|display:Reactivity|" +
      "write:setReactivity|read:getReactivity");

    algo.setSpeed = function(_v) { algo.presetSpeed = parseFloat(_v); };
    algo.getSpeed = function() { return algo.presetSpeed; };
    algo.setReactivity = function(_v) { algo.presetReactivity = parseFloat(_v); };
    algo.getReactivity = function() { return algo.presetReactivity; };

    var REACTIVITY_SCALE = 5.0;
    var SAT_BASE = 0.9;
    var SAT_REACT_ADD = 0.3;
    var OFFSET_AMP = 2.0;

    algo.phase = 0;
    var energyState = { phase: 0 };

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var map = RGBUtil.createMap(width, height);
        if (!audio) return map;

        var dtMs = audio.timing.consumerDtMs;
        var bpm = (audio && audio.beat) ? audio.beat.bpm : 0;

        var speed = algo.presetSpeed;
        var reactivity01 = algo.presetReactivity;
        var reactivity = reactivity01 * REACTIVITY_SCALE;
        var lowPower = audio.power.low;

        algo.phase = RGBUtil.beatTime(speed, energyState, bpm, dtMs);

        var hue = RGBUtil.mod1(lowPower + algo.phase);
        var satThreshold = SAT_BASE - (reactivity01 + SAT_REACT_ADD) * lowPower;
        var offset = OFFSET_AMP * RGBUtil.sin01(algo.phase + reactivity01 * lowPower);

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
