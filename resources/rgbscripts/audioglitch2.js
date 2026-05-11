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
        var map = RGBUtil.createFlatMap(width, height);
        if (!audio) return map;

        var dtMs = audio.timing.consumerDtMs;
        var bpm = (audio && audio.beat) ? audio.beat.bpm : 0;

        var speed = algo.presetSpeed;
        var reactivity01 = algo.presetReactivity;
        var satThreshold = algo.presetSaturation;
        var lowPower = audio.power.low;

        // Old code added `(lowPower*reactivity01)/speed` to a seconds-based
        // accumulator. Convert to ms-equivalent boost (×1000).
        var boostMs = (lowPower * reactivity01) / Math.max(0.001, speed) * 1000;

        var t1 = RGBUtil.beatTime(speed * T1_RATIO, stT1, bpm, dtMs + boostMs);
        var t2 = RGBUtil.beatTime(speed * T2_RATIO, stT2, bpm, dtMs + boostMs);
        var t3 = RGBUtil.beatTime(speed * T3_RATIO, stT3, bpm, dtMs + boostMs);
        var t4 = RGBUtil.beatTime(speed,            stT4, bpm, dtMs + boostMs);
        var t5 = RGBUtil.beatTime(speed * T5_RATIO, stT5, bpm, dtMs + boostMs);
        var t6 = RGBUtil.beatTime(speed * T6_RATIO, stT6, bpm, dtMs + boostMs);

        var m = STRIPE_MID + RGBUtil.triangle(t2) * STRIPE_AMP;
        var c = RGBUtil.triangle(t3) * 10 + 4 * Math.sin(2 * Math.PI * t4);
        var sinT1 = Math.sin(2 * Math.PI * t1);

        var n = Math.max(2, width);
        for (var x = 0; x < width; x++) {
            var u = x / (n - 1) - 0.5;
            var i2 = (x / (n - 1)) * 5;
            var i3 = (x / (n - 1)) - 1;

            var band = ((u * c) % m + m) % m;
            var hue = RGBUtil.mod1(band + sinT1);

            var s1 = RGBUtil.triangle(RGBUtil.mod1(i2 + t5));
            s1 = s1 * s1;
            var s2 = RGBUtil.triangle(RGBUtil.mod1(t6 - i3));
            s2 = s2 * s2 * s2 * s2;
            var sat = 1 - RGBUtil.triangle(s1 * s2);
            if (sat < satThreshold) sat = satThreshold;
            if (sat > 1) sat = 1;

            var packed = RGBUtil.hsvToRgb(hue, sat, 1);
            for (var y = 0; y < height; y++)
                map[(y) * width + (x)] = packed;
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
