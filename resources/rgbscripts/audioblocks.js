/*
  Q Light Controller Plus
  audioblocks.js

  Copyright (c) QLC+ contributors
  Ported from LedFx "Block Reflections" effect (MIT License)
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
    algo.name = "Audio Blocks";
    algo.author = "Ported from LedFx";
    algo.acceptColors = 0;
    algo.usesAudio = true;
    algo.properties = new Array();

    algo.speed = 1.0;
    algo.properties.push(
      "name:speed|type:float|display:Speed (cyc/beat)|" +
      "write:setSpeed|read:getSpeed");

    algo.reactivity = 0.5;
    algo.properties.push(
      "name:reactivity|type:float|display:Reactivity|" +
      "write:setReactivity|read:getReactivity");

    algo.fix_hues = "Yes";
    algo.properties.push(
      "name:fix_hues|type:list|display:Fix Hues|" +
      "values:No,Yes|write:setFixHues|read:getFixHues");

    algo.setSpeed = function(_v) { algo.speed = clampFloat(_v, 0.00001, 50.0); };
    algo.getSpeed = function() { return algo.speed; };
    algo.setReactivity = function(_v) { algo.reactivity = clampFloat(_v, 0.00001, 1.0); };
    algo.getReactivity = function() { return algo.reactivity; };
    algo.setFixHues = function(_v) { algo.fix_hues = (_v === "No") ? "No" : "Yes"; };
    algo.getFixHues = function() { return algo.fix_hues; };

    // Per-track ratios relative to algo.speed (the base rate).
    var T3_RATIO = 5.0;
    var T4_RATIO = 2.0;

    algo.lowsPower = 0;
    var blocksState1 = { phase: 0 };
    var blocksState3 = { phase: 0 };
    var blocksState4 = { phase: 0 };

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };

    function clampFloat(v, min, max) {
        var parsed = parseFloat(v);
        if (isNaN(parsed)) parsed = min;
        return Math.max(min, Math.min(max, parsed));
    }

    function mod(x, m) {
        return ((x % m) + m) % m;
    }

    function triangle(x) {
        return 1 - 2 * Math.abs(RGBUtil.mod1(x) - 0.5);
    }

    function sin01(x) {
        return 0.5 * Math.sin(x * 2 * Math.PI) + 0.5;
    }

    function fixHueFast(hue) {
        hue = RGBUtil.mod1(hue);
        return sin01((hue - 0.5) / 2.0);
    }

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var map = RGBUtil.createMap(width, height);
        if (!audio) return map;

        var dtMs = audio.timing.consumerDtMs > 0 ? audio.timing.consumerDtMs : 40;
        var bpm = (audio && audio.beat) ? audio.beat.bpm : 0;

        var rawLows = audio.power.low;
        algo.lowsPower = rawLows * 0.05 + algo.lowsPower * 0.95;

        var speed = algo.speed;
        var reactivity = algo.reactivity;
        var t1 = RGBUtil.beatTime(speed, blocksState1, bpm, dtMs);
        var t2 = t1 * (Math.PI * Math.PI) + (0.8 * reactivity * algo.lowsPower);
        var t3 = RGBUtil.beatTime(speed * T3_RATIO, blocksState3, bpm, dtMs) + (reactivity * algo.lowsPower);
        var t4 = RGBUtil.beatTime(speed * T4_RATIO, blocksState4, bpm, dtMs) * (Math.PI * Math.PI);

        var m = 0.3 + triangle(t1) * 0.2;
        var c = triangle(t3) * 10.0 + 4.0 * sin01(t4);
        var fixHues = algo.fix_hues !== "No";
        var pixelCount = Math.max(1, width);

        for (var x = 0; x < width; x++) {
            var h = x;
            h -= pixelCount / 2.0;
            h /= pixelCount;
            h *= c;
            h = mod(h, m);
            h += sin01(t2);

            var v = Math.abs(h);
            v += Math.abs(m) + t1;
            v = RGBUtil.mod1(v);
            v *= v;

            var hue = fixHues ? fixHueFast(h) : RGBUtil.mod1(h);
            for (var y = 0; y < height; y++)
                RGBUtil.setPixel(map, width, x, y, hue, 1.0, v);
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
