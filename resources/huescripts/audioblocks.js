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
    algo.acceptColors = 3;
    algo.usesAudio = true;
    algo.properties = new Array();
    algo.presetMode = "Artistic";
    algo.properties.push("name:mode|type:list|display:Response|values:Artistic,LedFx Block Reflections|write:setMode|read:getMode");
    algo.setMode = function(v) { algo.presetMode = v === "LedFx Block Reflections" ? v : "Artistic"; };
    algo.getMode = function() { return algo.presetMode; };

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

    algo.setSpeed = function(_v) { algo.speed = parseFloat(_v); };
    algo.getSpeed = function() { return algo.speed; };
    algo.setReactivity = function(_v) { algo.reactivity = parseFloat(_v); };
    algo.getReactivity = function() { return algo.reactivity; };
    algo.setFixHues = function(_v) { algo.fix_hues = (_v === "No") ? "No" : "Yes"; };
    algo.getFixHues = function() { return algo.fix_hues; };

    // Per-track ratios relative to algo.speed (the base rate).
    var T3_RATIO = 5.0;
    var T4_RATIO = 2.0;
    var PI_SQUARED = Math.PI * Math.PI;

    algo.lowsPower = 0;
    var ledFxLowsPower = 0;
    var blocksState1 = { phase: 0 };
    var blocksState3 = { phase: 0 };
    var blocksState4 = { phase: 0 };
    var blocksLedMs1 = 0;
    var blocksLedMs3 = 0;
    var blocksLedMs4 = 0;
    var ledFxAudioIdentityKey = "";
    var ledFxGeometryKey = "";
    var getLedFxOutputOptions = HSVUtil.attachStripTransformControls(algo, {
        prefix: "ledFx",
        display: "LedFx "
    });

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };


    function mod(x, m) {
        return ((x % m) + m) % m;
    }

    function triangle(x) {
        return 1 - 2 * Math.abs(HSVUtil.mod1(x) - 0.5);
    }

    function sin01(x) {
        return 0.5 * Math.sin(x * 2 * Math.PI) + 0.5;
    }

    function fixHueFast(hue) {
        hue = HSVUtil.mod1(hue);
        return sin01((hue - 0.5) / 2.0);
    }

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

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var map = HSVUtil.createMap(width, height);
        if (!audio) return map;

        var ledFxMode = algo.presetMode === "LedFx Block Reflections";
        if (ledFxMode) {
            var audioIdentityKey = HSVUtil.audioIdentityKey(audio, ledFxAudioIdentityKey);
            var geometryKey = width + "x" + height;
            if (audioIdentityKey !== ledFxAudioIdentityKey || geometryKey !== ledFxGeometryKey) {
                ledFxAudioIdentityKey = audioIdentityKey;
                ledFxGeometryKey = geometryKey;
                blocksLedMs1 = 0;
                blocksLedMs3 = 0;
                blocksLedMs4 = 0;
                ledFxLowsPower = 0;
            }
        }
        var dt = ledFxMode
            ? (audio.timing && isFinite(audio.timing.deltaSeconds)
                ? Math.max(0, audio.timing.deltaSeconds)
                : ((audio.bpm > 0 && isFinite(audio.dt)) ? audio.dt * 60 / audio.bpm : 0))
            : audio.dt;
        var rawLows = ledFxMode ? rawLowPower(audio) : audio.low;
        if (ledFxMode) {
            var ledFxAlpha = dt > 0 ? (1 - Math.pow(0.95, dt * 60)) : 0;
            ledFxLowsPower += (rawLows - ledFxLowsPower) * ledFxAlpha;
        } else {
            algo.lowsPower = rawLows * 0.05 + algo.lowsPower * 0.95;
        }
        var lowsPower = ledFxMode ? ledFxLowsPower : algo.lowsPower;

        var speed = algo.speed;
        var reactivity = algo.reactivity;
        var t1;
        var t2;
        var t3;
        var t4;
        if (ledFxMode) {
            var dtMs = dt * 1000.0;
            blocksLedMs1 += dtMs;
            blocksLedMs3 += dtMs;
            blocksLedMs4 += dtMs;
            t1 = HSVUtil.time01(speed, blocksLedMs1);
            t2 = t1 * PI_SQUARED + (0.8 * reactivity * lowsPower);
            t3 = HSVUtil.time01(speed * T3_RATIO, blocksLedMs3) + (reactivity * lowsPower);
            t4 = HSVUtil.time01(speed * T4_RATIO, blocksLedMs4) * PI_SQUARED;
        } else {
            blocksState1.phase = (blocksState1.phase + audio.dt * speed) % 1.0;
            t1 = blocksState1.phase;
            t2 = t1 * PI_SQUARED + (0.8 * reactivity * lowsPower);
            t3 = (blocksState3.phase = (blocksState3.phase + audio.dt * speed * T3_RATIO) % 1.0) + (reactivity * lowsPower);
            blocksState4.phase = (blocksState4.phase + audio.dt * speed * T4_RATIO) % 1.0;
            t4 = blocksState4.phase * PI_SQUARED;
        }

        var m = 0.3 + triangle(t1) * 0.2;
        var c = triangle(t3) * 10.0 + 4.0 * sin01(t4);
        var fixHues = algo.fix_hues !== "No";
        var pixelCount = Math.max(1, ledFxMode ? (width * height) : width);
        var stops = ledFxMode ? ledFxGradientStops() : null;
        var ledFxOutputOptions = ledFxMode ? getLedFxOutputOptions() : null;

        for (var i = 0; i < pixelCount; i++) {
            var x = ledFxMode ? (i % width) : i;
            var y = ledFxMode ? Math.floor(i / width) : 0;
            var h = i;
            h -= pixelCount / 2.0;
            h /= pixelCount;
            h *= c;
            h = mod(h, m);
            h += sin01(t2);

            var v = Math.abs(h);
            v += Math.abs(m) + t1;
            v = HSVUtil.mod1(v);
            v *= v;

            var hue = fixHues ? fixHueFast(h) : HSVUtil.mod1(h);
            if (ledFxMode) {
                var base = HSVUtil.gradientRgbAt(stops, hue);
                var rgbOut = [base[0] * v, base[1] * v, base[2] * v];
                var hsvOut = HSVUtil.rgbToHsvUnclipped(rgbOut[0], rgbOut[1], rgbOut[2]);
                HSVUtil.setPixel(map, width, x, y, hsvOut.h, hsvOut.s, hsvOut.v);
            } else {
                for (var yy = 0; yy < height; yy++)
                    HSVUtil.setPixel(map, width, x, yy, hue, 1.0, v);
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
