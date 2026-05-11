/*
  Q Light Controller Plus
  audiocrawler.js

  Copyright (c) QLC+ contributors
  Ported from LedFX "Crawler" effect (MIT License)

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
    algo.name = "Audio Crawler";
    algo.author = "Ported from LedFx";
    algo.acceptColors = 0;
    algo.usesAudio = true;
    algo.properties = new Array();

    algo.speed = 0.5;
    algo.reactivity = 0.25;
    algo.sway = 20;
    algo.chop = 30;
    algo.stretch = 2.5;

    algo.properties.push("name:speed|type:float|display:Speed|write:setSpeed|read:getSpeed");
    algo.properties.push("name:reactivity|type:float|display:Reactivity|write:setReactivity|read:getReactivity");
    algo.properties.push("name:sway|type:float|display:Sway|write:setSway|read:getSway");
    algo.properties.push("name:chop|type:float|display:Chop|write:setChop|read:getChop");
    algo.properties.push("name:stretch|type:float|display:Stretch|write:setStretch|read:getStretch");

    function clamp(v, lo, hi) { var n = parseFloat(v); return isNaN(n) ? lo : Math.max(lo, Math.min(hi, n)); }
    algo.setSpeed = function(v) { algo.speed = clamp(v, 0.00001, 1); };
    algo.getSpeed = function() { return algo.speed; };
    algo.setReactivity = function(v) { algo.reactivity = clamp(v, 0.00001, 1); };
    algo.getReactivity = function() { return algo.reactivity; };
    algo.setSway = function(v) { algo.sway = clamp(v, 0.00001, 50); };
    algo.getSway = function() { return algo.sway; };
    algo.setChop = function(v) { algo.chop = clamp(v, 0.00001, 100); };
    algo.getChop = function() { return algo.chop; };
    algo.setStretch = function(v) { algo.stretch = clamp(v, 0.00001, 10); };
    algo.getStretch = function() { return algo.stretch; };

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
        var sway = algo.sway;
        var chop = algo.chop;
        var stretch = algo.stretch;

        var timeAccum = RGBUtil.beatPosition(1.0, timeState, bpm, dtMs);
        var timestep = timeAccum + lows * reactivity * speed;

        var t1 = hsvTime(speed * sway, timestep);
        var t2 = hsvTime(speed * chop, timestep);
        var t3 = hsvTime(speed * chop + lows * reactivity, timeAccum);

        var sinT1 = hsvSin(t1);

        for (var x = 0; x < width; x++) {
            var i1 = x / Math.max(1, width - 1);

            var h = (x + t3 * width) / width;
            h *= stretch;
            h = ((h % (stretch / 10)) + (stretch / 10)) % (stretch / 10);
            h += i1;
            h += sinT1;

            var v = hsvSin(h);
            v = v * v;

            h = RGBUtil.mod1(h);
            for (var y = 0; y < height; y++)
                RGBUtil.setPixel(map, width, x, y, h, 1, v);
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
