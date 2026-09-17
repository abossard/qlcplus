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
    algo.acceptColors = 3;
    algo.usesAudio = true;
    algo.properties = new Array();
    algo.presetMode = "Artistic";
    algo.properties.push("name:mode|type:list|display:Response|values:Artistic,LedFx Lava Lamp|write:setMode|read:getMode");
    algo.setMode = function(v) { algo.presetMode = v === "LedFx Lava Lamp" ? v : "Artistic"; };
    algo.getMode = function() { return algo.presetMode; };

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
    var ledFxTimePosition = 0;
    var emaLows = 0;
    var ledfxRawLows = 0;
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
        var ledFxMode = algo.presetMode === "LedFx Lava Lamp";
        if (ledFxMode) {
            var audioIdentityKey = HSVUtil.audioIdentityKey(audio, ledFxAudioIdentityKey);
            var geometryKey = width + "x" + height;
            if (audioIdentityKey !== ledFxAudioIdentityKey || geometryKey !== ledFxGeometryKey) {
                ledFxAudioIdentityKey = audioIdentityKey;
                ledFxGeometryKey = geometryKey;
                ledFxTimePosition = 0;
                ledfxRawLows = 0;
            }
        }

        var dt = audio.dt;
        var dtSeconds = audio.timing
            ? Math.max(0, audio.timing.deltaSeconds || 0)
            : ((audio.bpm > 0 && isFinite(dt)) ? dt * 60 / audio.bpm : 0);
        // BPM-scaled free-running time: one unit per beat (matches seconds at 60 BPM).
        var timeAccum;
        if (ledFxMode) {
            ledFxTimePosition += dtSeconds;
            timeAccum = ledFxTimePosition;
        } else {
            timeAccum = ((timeState.position = (timeState.position || 0) + audio.dt) && timeState.position);
        }

        var rawLows = audio.low;
        var lows;
        if (ledFxMode) {
            var low = rawLowPower(audio);
            var alphaRaw = low > ledfxRawLows ? algo.reactivity : 0.05;
            var adapted = 1 - Math.pow(1 - alphaRaw, dtSeconds * 50);
            ledfxRawLows += (low - ledfxRawLows) * adapted;
            lows = ledfxRawLows;
        } else {
            var alpha = (rawLows > emaLows) ? algo.reactivity : 0.05;
            emaLows = alpha * rawLows + (1 - alpha) * emaLows;
            lows = emaLows;
        }

        var speed = algo.speed;
        var contrastInv = 1 - algo.contrast;
        var stops = ledFxMode ? ledFxGradientStops() : null;
        var ledFxOutputOptions = ledFxMode ? getLedFxOutputOptions() : null;

        var t1 = hsvTime(speed * Math.max(1, 1 + lows * 0.004), timeAccum);
        var t2 = hsvTime(speed * 2 * Math.max(1, 1 + lows * 0.007), timeAccum);
        var pixelCount = ledFxMode ? Math.max(1, width * height) : Math.max(1, width);

        for (var i = 0; i < pixelCount; i++) {
            var x = ledFxMode ? (i % width) : i;
            var y = ledFxMode ? Math.floor(i / width) : 0;
            var il = i / Math.max(1, pixelCount - 1);

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
            if (ledFxMode) {
                var base = HSVUtil.gradientRgbAt(stops, hc);
                var hsvOut = HSVUtil.rgbToHsvUnclipped(base[0] * val, base[1] * val, base[2] * val);
                HSVUtil.setPixel(map, width, x, y, hsvOut.h, hsvOut.s, hsvOut.v);
            } else {
                var vc = HSVUtil.clamp01(val);
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
