/*
  Q Light Controller Plus
  audioglitch2.js

  Copyright (c) QLC+ contributors
  Ported from LedFX "Glitch" effect (MIT License)
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
    algo.name = "Audio Glitch 2";
    algo.author = "Ported from LedFx";
    algo.acceptColors = 0;
    algo.usesAudio = true;
    algo.properties = new Array();

    algo.presetSpeed = 0.125;
    algo.properties.push(
      "name:presetSpeed|type:float|display:Speed (cyc/beat)|" +
      "write:setSpeed|read:getSpeed");

    algo.presetReactivity = 0.4;
    algo.properties.push(
      "name:presetReactivity|type:float|display:Reactivity|" +
      "write:setReactivity|read:getReactivity");

    algo.presetSaturation = 1.0;
    algo.properties.push(
      "name:presetSaturation|type:float|display:Saturation|" +
      "write:setSaturation|read:getSaturation");

    algo.setSpeed = function(_v) { algo.presetSpeed = parseFloat(_v); };
    algo.getSpeed = function() { return algo.presetSpeed; };
    algo.setReactivity = function(_v) { algo.presetReactivity = parseFloat(_v); };
    algo.getReactivity = function() { return algo.presetReactivity; };
    algo.setSaturation = function(_v) { algo.presetSaturation = parseFloat(_v); };
    algo.getSaturation = function() { return algo.presetSaturation; };

    // Per-track ratios relative to presetSpeed (preserve the original PHASE_MULT_T*
    // proportions). T4 runs at the base rate (no ratio constant).
    var T1_RATIO = 0.5;
    var T2_RATIO = 0.5;
    var T3_RATIO = 2.5;
    var T5_RATIO = 0.25;
    var T6_RATIO = 10.0;
    var STRIPE_MID = 0.3;
    var STRIPE_AMP = 0.2;

    var stT1 = { phase: 0 };
    var stT2 = { phase: 0 };
    var stT3 = { phase: 0 };
    var stT4 = { phase: 0 };
    var stT5 = { phase: 0 };
    var stT6 = { phase: 0 };

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var map = HSVUtil.createMap(width, height);
        if (!audio) return map;

        var dt = audio.dt;
        var speed = algo.presetSpeed;
        var reactivity01 = algo.presetReactivity;
        var satThreshold = algo.presetSaturation;
        var lowPower = audio.low;

        // Audio-reactive time boost (extra beats per frame)
        var boostBeats = (lowPower * reactivity01) / Math.max(0.001, speed) * 1000 * audio.bpm / 60000;
        var effectiveDt = audio.dt + boostBeats;

        stT1.phase = (stT1.phase + effectiveDt * speed * T1_RATIO) % 1.0;
        var t1 = stT1.phase;
        stT2.phase = (stT2.phase + effectiveDt * speed * T2_RATIO) % 1.0;
        var t2 = stT2.phase;
        stT3.phase = (stT3.phase + effectiveDt * speed * T3_RATIO) % 1.0;
        var t3 = stT3.phase;
        stT4.phase = (stT4.phase + effectiveDt * speed) % 1.0;
        var t4 = stT4.phase;
        stT5.phase = (stT5.phase + effectiveDt * speed * T5_RATIO) % 1.0;
        var t5 = stT5.phase;
        stT6.phase = (stT6.phase + effectiveDt * speed * T6_RATIO) % 1.0;
        var t6 = stT6.phase;

        var m = STRIPE_MID + HSVUtil.triangle(t2) * STRIPE_AMP;
        var c = HSVUtil.triangle(t3) * 10 + 4 * Math.sin(2 * Math.PI * t4);
        var sinT1 = Math.sin(2 * Math.PI * t1);

        var n = Math.max(2, width);
        for (var x = 0; x < width; x++) {
            var u = x / (n - 1) - 0.5;
            var i2 = (x / (n - 1)) * 5;
            var i3 = (x / (n - 1)) - 1;

            var band = ((u * c) % m + m) % m;
            var hue = HSVUtil.mod1(band + sinT1);

            var s1 = HSVUtil.triangle(HSVUtil.mod1(i2 + t5));
            s1 = s1 * s1;
            var s2 = HSVUtil.triangle(HSVUtil.mod1(t6 - i3));
            s2 = s2 * s2 * s2 * s2;
            var sat = 1 - HSVUtil.triangle(s1 * s2);
            if (sat < satThreshold) sat = satThreshold;
            if (sat > 1) sat = 1;

            for (var y = 0; y < height; y++)
                HSVUtil.setPixel(map, width, x, y, hue, sat, 1);
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
