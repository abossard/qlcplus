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
    algo.acceptColors = 3;
    algo.usesAudio = true;
    algo.properties = new Array();
    algo.presetMode = "Artistic";
    algo.properties.push("name:mode|type:list|display:Response|values:Artistic,LedFx Melt|write:setMode|read:getMode");
    algo.setMode = function(v) { algo.presetMode = v === "LedFx Melt" ? v : "Artistic"; };
    algo.getMode = function() { return algo.presetMode; };

    algo.speed = 0.5;
    algo.reactivity = 0.5;

    algo.properties.push("name:speed|type:float|display:Speed|write:setSpeed|read:getSpeed");
    algo.properties.push("name:reactivity|type:float|display:Reactivity|write:setReactivity|read:getReactivity");

    algo.setSpeed = function(v) { algo.speed = parseFloat(v); };
    algo.getSpeed = function() { return algo.speed; };
    algo.setReactivity = function(v) { algo.reactivity = parseFloat(v); };
    algo.getReactivity = function() { return algo.reactivity; };

    var TWO_PI = 2 * Math.PI;
    var timeState = { position: 0 };
    var emaLows = 0;
    var ledFxLow = 0;
    var ledFxTimestepMs = 0;
    var NOMINAL_HZ = 60;
    var ledFxAudioIdentityKey = "";
    var ledFxGeometryKey = "";
    var getLedFxOutputOptions = HSVUtil.attachStripTransformControls(algo, {
        prefix: "ledFx",
        display: "LedFx "
    });

    function hsvTime(modifier, ts) {
        var t = (ts * modifier / 65.536) % 1;
        return t < 0 ? t + 1 : t;
    }

    function hsvSin(v) { return 0.5 + 0.5 * Math.sin(v * TWO_PI); }

    function rawLowPower(audio) {
        if (audio && audio.powers && audio.powers.raw && isFinite(audio.powers.raw.low))
            return HSVUtil.clamp01(audio.powers.raw.low);
        return HSVUtil.clamp01(audio && isFinite(audio.low) ? audio.low : 0);
    }

    function ledFxGradientStops() {
        if (algo.hasUserColors && Array.isArray(algo.colors) && algo.colors.length >= 2)
            return algo.colors;
        return [
            {h: 0 / 3, s: 1, v: 1},
            {h: 1 / 3, s: 1, v: 1},
            {h: 2 / 3, s: 1, v: 1}
        ];
    }

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var map = HSVUtil.createMap(width, height);
        if (!audio) return map;
        var ledFxMode = algo.presetMode === "LedFx Melt";
        if (ledFxMode) {
            var audioIdentityKey = HSVUtil.audioIdentityKey(audio, ledFxAudioIdentityKey);
            var geometryKey = width + "x" + height;
            if (audioIdentityKey !== ledFxAudioIdentityKey || geometryKey !== ledFxGeometryKey) {
                ledFxAudioIdentityKey = audioIdentityKey;
                ledFxGeometryKey = geometryKey;
                ledFxLow = 0;
                ledFxTimestepMs = 0;
            }
        }

        var seconds = audio.timing
            ? Math.max(0, audio.timing.deltaSeconds || 0)
            : ((audio.bpm > 0 && isFinite(audio.dt)) ? audio.dt * 60 / audio.bpm : 0);
        var dtBeats = isFinite(audio.dt) ? audio.dt : (seconds * (audio.bpm > 0 ? audio.bpm : 120) / 60);
        var rawLows = ledFxMode ? rawLowPower(audio) : audio.low;
        var alpha = 1 - Math.pow(0.9, seconds * 50);
        if (ledFxMode) {
            ledFxLow += (rawLows - ledFxLow) * alpha;
        } else {
            emaLows += (rawLows - emaLows) * alpha;
        }
        var lows = ledFxMode ? ledFxLow : emaLows;

        var speed = algo.speed;
        var reactivity = algo.reactivity;
        var stops = ledFxMode ? ledFxGradientStops() : null;
        var ledFxOutputOptions = ledFxMode ? getLedFxOutputOptions() : null;

        var t1;
        var t2;
        var pixelCount = ledFxMode ? Math.max(1, width * height) : Math.max(1, width);
        if (ledFxMode) {
            var speedSafe = speed > 0 ? speed : 0.001;
            ledFxTimestepMs += seconds * 1000.0;
            ledFxTimestepMs += lows * reactivity / speedSafe * 1000.0 * seconds * NOMINAL_HZ;
            t1 = HSVUtil.time01(speed * 5.0, ledFxTimestepMs);
            t2 = HSVUtil.time01(speed * 6.5, ledFxTimestepMs);
        } else {
            var timeStep = dtBeats;
            var baseTime = ((timeState.position = (timeState.position || 0) + timeStep) && timeState.position);
            var timestep = speed * baseTime + lows * reactivity;
            t1 = hsvTime(5, timestep);
            t2 = hsvTime(6.5, timestep);
        }

        for (var i = 0; i < pixelCount; i++) {
            var x = ledFxMode ? (i % width) : i;
            var y = ledFxMode ? Math.floor(i / width) : 0;
            var il = 1 - i / Math.max(1, pixelCount - 1);

            var v = hsvSin(il);
            v = hsvSin(v + t1);
            v = hsvSin(v + t1);
            v = v * v;

            var h = il + t2;

            var hc = HSVUtil.mod1(h);
            if (ledFxMode) {
                var base = HSVUtil.gradientRgbAt(stops, hc);
                var hsvOut = HSVUtil.rgbToHsvUnclipped(base[0] * v, base[1] * v, base[2] * v);
                HSVUtil.setPixel(map, width, x, y, hsvOut.h, hsvOut.s, hsvOut.v);
            } else {
                var vc = HSVUtil.clamp01(v);
                for (var yy = 0; yy < height; yy++)
                    HSVUtil.setPixel(map, width, x, yy, hc, 1, vc);
            }
        }

        if (ledFxMode)
            HSVUtil.applyStripTransforms(map, width, height, ledFxOutputOptions);

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
