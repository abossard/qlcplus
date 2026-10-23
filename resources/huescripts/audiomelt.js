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

    var position = 0;
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

        var seconds = HSVUtil.audioSeconds(audio);
        var dtBeats = isFinite(audio.dt) ? audio.dt : (seconds * (audio.bpm > 0 ? audio.bpm : 120) / 60);
        var rawLows = ledFxMode ? HSVUtil.powerForRange(audio, "Lows", true) : audio.low;
        var alpha = 1 - Math.pow(0.9, seconds * 50);
        if (ledFxMode) {
            ledFxLow += (rawLows - ledFxLow) * alpha;
        } else {
            emaLows += (rawLows - emaLows) * alpha;
        }
        var lows = ledFxMode ? ledFxLow : emaLows;

        var speed = algo.speed;
        var reactivity = algo.reactivity;
        var stops = ledFxMode ? HSVUtil.ledFxGradientStops(algo) : null;

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
            position += dtBeats;
            var timestep = speed * position + lows * reactivity;
            t1 = HSVUtil.mod1(timestep * 5 / 65.536);
            t2 = HSVUtil.mod1(timestep * 6.5 / 65.536);
        }

        for (var i = 0; i < pixelCount; i++) {
            var x = ledFxMode ? (i % width) : i;
            var y = ledFxMode ? Math.floor(i / width) : 0;
            var il = 1 - i / Math.max(1, pixelCount - 1);

            var v = HSVUtil.sin01(il);
            v = HSVUtil.sin01(v + t1);
            v = HSVUtil.sin01(v + t1);
            v = v * v;

            var hc = HSVUtil.mod1(il + t2);
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
            HSVUtil.applyStripTransforms(map, width, height, getLedFxOutputOptions());

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
