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

    var timePosition = 0;
    var ledFxTimePosition = 0;
    var emaLows = 0;
    var ledfxRawLows = 0;
    var ledFxKeys = {audioKey: "", geometryKey: ""};
    var getLedFxOutputOptions = HSVUtil.attachStripTransformControls(algo, {
        prefix: "ledFx",
        display: "LedFx "
    });

    function hsvTime(modifier, ts) {
        var t = (ts * modifier / 65.536) % 1;
        return t < 0 ? t + 1 : t;
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
            if (HSVUtil.audioStateChanged(ledFxKeys, audio, width, height)) {
                ledFxTimePosition = 0;
                ledfxRawLows = 0;
            }
        }

        var dtSeconds = HSVUtil.audioSeconds(audio);
        // BPM-scaled free-running time: one unit per beat (matches seconds at 60 BPM).
        var timeAccum;
        if (ledFxMode) {
            ledFxTimePosition += dtSeconds;
            timeAccum = ledFxTimePosition;
        } else {
            timePosition = (timePosition || 0) + audio.dt;
            timeAccum = timePosition;
        }

        var rawLows = audio.low;
        var lows;
        if (ledFxMode) {
            var low = HSVUtil.powerForRange(audio, "Lows", true);
            ledfxRawLows = HSVUtil.attackRelease(ledfxRawLows, low, algo.reactivity, 0.05, dtSeconds * 50);
            lows = ledfxRawLows;
        } else {
            var alpha = (rawLows > emaLows) ? algo.reactivity : 0.05;
            emaLows = alpha * rawLows + (1 - alpha) * emaLows;
            lows = emaLows;
        }

        var speed = algo.speed;
        var contrastInv = 1 - algo.contrast;
        var stops = ledFxMode ? HSVUtil.ledFxGradientStops(algo) : null;
        var ledFxOutputOptions = ledFxMode ? getLedFxOutputOptions() : null;

        var t1 = hsvTime(speed * Math.max(1, 1 + lows * 0.004), timeAccum);
        var t2 = hsvTime(speed * 2 * Math.max(1, 1 + lows * 0.007), timeAccum);
        var pixelCount = ledFxMode ? Math.max(1, width * height) : Math.max(1, width);

        for (var i = 0; i < pixelCount; i++) {
            var x = ledFxMode ? (i % width) : i;
            var y = ledFxMode ? Math.floor(i / width) : 0;
            var il = i / Math.max(1, pixelCount - 1);

            var w1 = HSVUtil.sin01(t1 + il);
            var w2 = HSVUtil.sin01(t2 - il);
            var w3raw = (il + w1 + w2) % 1;
            var w3 = HSVUtil.sin01(w3raw);

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
