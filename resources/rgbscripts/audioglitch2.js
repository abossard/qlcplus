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

    algo.presetSpeed = 50;
    algo.properties.push(
      "name:presetSpeed|type:range|display:Speed|" +
      "values:1,100|write:setSpeed|read:getSpeed");

    algo.presetReactivity = 40;
    algo.properties.push(
      "name:presetReactivity|type:range|display:Reactivity|" +
      "values:1,100|write:setReactivity|read:getReactivity");

    algo.presetSaturation = 100;
    algo.properties.push(
      "name:presetSaturation|type:range|display:Saturation|" +
      "values:0,100|write:setSaturation|read:getSaturation");

    algo.setSpeed = function(_v) { algo.presetSpeed = parseInt(_v); };
    algo.getSpeed = function() { return algo.presetSpeed; };
    algo.setReactivity = function(_v) { algo.presetReactivity = parseInt(_v); };
    algo.getReactivity = function() { return algo.presetReactivity; };
    algo.setSaturation = function(_v) { algo.presetSaturation = parseInt(_v); };
    algo.getSaturation = function() { return algo.presetSaturation; };

    // Speed multipliers picked so default presetSpeed (50) yields a glitch
    // tempo close to LedFx's default. Time is kept in seconds.
    var SPEED_SCALE = 0.5;

    algo.timestep = 0;
    algo.lastMs = 0;

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var map = RGBUtil.createMap(width, height);
        if (!audio) return map;

        var dtMs = (audio.timing && audio.timing.consumerDtMs)
                   ? audio.timing.consumerDtMs : 0;
        if (dtMs <= 0) {
            var now = Date.now();
            dtMs = algo.lastMs ? (now - algo.lastMs) : 20;
            algo.lastMs = now;
        }
        var dt = dtMs / 1000.0;

        var speed = (algo.presetSpeed / 100.0) * SPEED_SCALE;
        var reactivity01 = algo.presetReactivity / 100.0;
        var satThreshold = algo.presetSaturation / 100.0;
        var lowPower = audio.power.low;

        algo.timestep += dt + (lowPower * reactivity01) / Math.max(0.001, speed);

        var ts = algo.timestep * speed;
        var t1 = ts * 0.5;
        var t2 = ts * 0.5;
        var t3 = ts * 2.5;
        var t4 = ts * 1.0;
        var t5 = ts * 0.25;
        var t6 = ts * 10.0;

        var m = 0.3 + RGBUtil.triangle(t2) * 0.2;
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
                map[y][x] = packed;
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
