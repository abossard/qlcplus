/*
  Q Light Controller Plus
  audiofire.js

  Copyright (c) QLC+ contributors
  Ported from LedFX "Fire" effect (MIT License)

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
    algo.name = "Audio Fire";
    algo.author = "Ported from LedFx";
    algo.acceptColors = 0;
    algo.usesAudio = true;
    algo.properties = new Array();

    algo.speed = 0.04;
    algo.color_shift = 0.15;
    algo.intensity = 8;
    algo.fade_chance = 0.5;

    algo.properties.push("name:speed|type:float|display:Speed|write:setSpeed|read:getSpeed");
    algo.properties.push("name:color_shift|type:float|display:Color Shift|write:setColorShift|read:getColorShift");
    algo.properties.push("name:intensity|type:float|display:Intensity|write:setIntensity|read:getIntensity");
    algo.properties.push("name:fade_chance|type:float|display:Fade Chance|write:setFadeChance|read:getFadeChance");

    function clamp(v, lo, hi) { var n = parseFloat(v); return isNaN(n) ? lo : Math.max(lo, Math.min(hi, n)); }
    algo.setSpeed = function(v) { algo.speed = clamp(v, 0.00001, 0.5); };
    algo.getSpeed = function() { return algo.speed; };
    algo.setColorShift = function(v) { algo.color_shift = clamp(v, 0, 1); };
    algo.getColorShift = function() { return algo.color_shift; };
    algo.setIntensity = function(v) { algo.intensity = Math.max(1, Math.min(30, Math.round(parseFloat(v) || 8))); };
    algo.getIntensity = function() { return algo.intensity; };
    algo.setFadeChance = function(v) { algo.fade_chance = clamp(v, 0.05, 1); };
    algo.getFadeChance = function() { return algo.fade_chance; };

    var sparkPixels = null;
    var sparks = null;
    var sparkX = null;
    var emaLows = 0;
    var cooling = 0.95;
    var curSpeed = 0.04;
    var curFadeChance = 0.05;

    function init(n) {
        sparkPixels = new Array(n);
        for (var i = 0; i < n; i++) sparkPixels[i] = 0;
        var sc = algo.intensity;
        sparks = new Array(sc);
        sparkX = new Array(sc);
        for (var i = 0; i < sc; i++) {
            sparks[i] = 0;
            sparkX[i] = Math.random() * 5;
        }
    }

    algo.rgbMapStepCount = function(width, height) { return 1; };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var N = width;
        if (!sparkPixels || sparkPixels.length !== N || sparks.length !== algo.intensity) init(N);
        var map = RGBUtil.createFlatMap(width, height);
        if (!audio) return map;

        var dtMs = audio.timing.consumerDtMs > 0 ? audio.timing.consumerDtMs : 40;

        // EMA filter on lows (decay=0.05, rise=0.99)
        var rawLows = audio.power.low;
        var alphaL = (rawLows > emaLows) ? 0.99 : 0.05;
        emaLows = alphaL * rawLows + (1 - alphaL) * emaLows;

        // Audio modulation
        cooling = 0.75 + emaLows * 0.25;
        curSpeed = algo.speed + emaLows * 0.01;
        curFadeChance = algo.fade_chance / 10;

        var deltaScaled = dtMs * curSpeed;

        // Cool all pixels
        for (var i = 0; i < N; i++)
            sparkPixels[i] *= cooling;

        // Heat diffusion
        if (N > 5) {
            for (var i = N - 1; i >= 5; i--)
                sparkPixels[i] = (sparkPixels[i - 1] + sparkPixels[i - 2] + sparkPixels[i - 3] * 2 + sparkPixels[i - 4] * 3) / 7;
        }

        var sc = sparks.length;
        for (var i = 0; i < sc; i++) {
            // Reset dead sparks
            if (sparks[i] <= 0) {
                sparks[i] = 0.5 + Math.random() * 0.5;
                sparkX[i] = Math.random() * 5;
            }

            // Advance
            var step = sparks[i] * sparks[i] * deltaScaled * (N / 100);
            sparkX[i] += step;

            // Fade or out of bounds
            if (sparkX[i] >= N || Math.random() < curFadeChance) {
                sparks[i] = 0;
                sparkX[i] = 0;
                continue;
            }

            // Heat up pixels where sparks pass
            var jStart = Math.max(0, Math.floor(sparkX[i] - step));
            var jEnd = Math.floor(sparkX[i]);
            var heat = Math.max(0, Math.min(1, 1 - sparks[i] * 0.4)) * 0.5;
            for (var j = jStart; j < jEnd && j < N; j++)
                sparkPixels[j] += heat;
        }

        // HSV mapping
        var colorShift = algo.color_shift;
        for (var x = 0; x < N; x++) {
            var px = sparkPixels[x];

            var h = Math.max(0, Math.min(1, px * px)) * 0.1 + colorShift;
            var s = 1 - (px - 1) * 2;
            var v = px * 2;

            var packed = RGBUtil.hsvLedFx(h, s, v);
            for (var y = 0; y < height; y++)
                map[(y) * width + (x)] = packed;
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
