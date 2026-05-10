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
    algo.acceptColors = 0;
    algo.usesAudio = true;
    algo.properties = new Array();

    algo.speed = 0.5;
    algo.reactivity = 0.5;

    algo.properties.push("name:speed|type:float|display:Speed|write:setSpeed|read:getSpeed");
    algo.properties.push("name:reactivity|type:float|display:Reactivity|write:setReactivity|read:getReactivity");

    function clamp(v, lo, hi) { var n = parseFloat(v); return isNaN(n) ? lo : Math.max(lo, Math.min(hi, n)); }
    algo.setSpeed = function(v) { algo.speed = clamp(v, 0.001, 1); };
    algo.getSpeed = function() { return algo.speed; };
    algo.setReactivity = function(v) { algo.reactivity = clamp(v, 0.0001, 1); };
    algo.getReactivity = function() { return algo.reactivity; };

    var TWO_PI = 2 * Math.PI;
    var timeState = { position: 0 };
    var emaLows = 0;

    function hsvTime(modifier, ts) {
        var t = (ts * modifier / 65.536) % 1;
        return t < 0 ? t + 1 : t;
    }

    function hsvSin(v) { return 0.5 + 0.5 * Math.sin(v * TWO_PI); }

    algo.rgbMapStepCount = function(width, height) { return 1; };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var map = RGBUtil.createMap(width, height);
        if (!audio) return map;

        var dtMs = audio.timing.consumerDtMs > 0 ? audio.timing.consumerDtMs : 40;
        var bpm = (audio.beat) ? audio.beat.bpm : 0;

        var rawLows = audio.power.low;
        var alpha = 0.1;
        emaLows = alpha * rawLows + (1 - alpha) * emaLows;
        var lows = emaLows;

        var speed = algo.speed;
        var reactivity = algo.reactivity;

        // BPM-scaled free-running time + audio-reactive offset (not accumulated).
        var baseTime = RGBUtil.beatPosition(1.0, timeState, bpm, dtMs);
        var timestep = baseTime + lows * reactivity / speed;

        var t1 = hsvTime(speed * 5, timestep);
        var t2 = hsvTime(speed * 6.5, timestep);

        for (var x = 0; x < width; x++) {
            var il = 1 - x / Math.max(1, width - 1);

            var v = hsvSin(il);
            v = hsvSin(v + t1);
            v = hsvSin(v + t1);
            v = v * v;

            var h = il + t2;

            var packed = RGBUtil.hsvLedFx(h, 1, v);
            for (var y = 0; y < height; y++)
                map[y][x] = packed;
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
