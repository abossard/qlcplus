/*
  Q Light Controller Plus
  audiolava.js

  Copyright (c) QLC+ contributors
  Ported from LedFX "Lava lamp" effect (MIT License)

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
    algo.name = "Audio Lava Lamp";
    algo.author = "Ported from LedFx";
    algo.acceptColors = 0;
    algo.usesAudio = true;
    algo.properties = new Array();

    algo.speed = 7;
    algo.contrast = 0.6;
    algo.reactivity = 0.3;

    algo.properties.push("name:speed|type:float|display:Speed|write:setSpeed|read:getSpeed");
    algo.properties.push("name:contrast|type:float|display:Contrast|write:setContrast|read:getContrast");
    algo.properties.push("name:reactivity|type:float|display:Reactivity|write:setReactivity|read:getReactivity");

    algo.setSpeed = function(v) { algo.speed = parseFloat(v); };
    algo.getSpeed = function() { return algo.speed; };
    algo.setContrast = function(v) { algo.contrast = parseFloat(v); };
    algo.getContrast = function() { return algo.contrast; };
    algo.setReactivity = function(v) { algo.reactivity = parseFloat(v); };
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
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var map = HSVUtil.createMap(width, height);
        if (!audio) return map;

        var dt = audio.dt;
        // BPM-scaled free-running time: one unit per beat (matches seconds at 60 BPM).
        var timeAccum = ((timeState.position = (timeState.position || 0) + audio.dt) && timeState.position);

        var rawLows = audio.low;
        var alpha = (rawLows > emaLows) ? algo.reactivity : 0.05;
        emaLows = alpha * rawLows + (1 - alpha) * emaLows;
        var lows = emaLows;

        var speed = algo.speed;
        var contrastInv = 1 - algo.contrast;

        var t1 = hsvTime(speed * Math.max(1, 1 + lows * 0.004), timeAccum);
        var t2 = hsvTime(speed * 2 * Math.max(1, 1 + lows * 0.007), timeAccum);

        for (var x = 0; x < width; x++) {
            var il = x / Math.max(1, width - 1);

            var w1 = hsvSin(t1 + il);
            var w2 = hsvSin(t2 - il);
            var w3raw = (il + w1 + w2) % 1;
            var w3 = hsvSin(w3raw);

            var h = t1 + il;

            w1 += 0.1;
            w2 += lows * 0.7;
            w3 += lows * 0.9;

            var pattern = w1 * w2 * w3;
            h += pattern * 0.1;

            var val = pattern + contrastInv;
            val = val * val;

            var hc = HSVUtil.mod1(h);
            var vc = HSVUtil.clamp01(val);
            for (var y = 0; y < height; y++)
                HSVUtil.setPixel(map, width, x, y, hc, 1, vc);
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
