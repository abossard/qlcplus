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
    algo.acceptColors = 3;
    algo.usesAudio = true;
    algo.properties = new Array();
    algo.presetMode = "Artistic";
    algo.properties.push("name:mode|type:list|display:Response|values:Artistic,LedFx Fire|write:setMode|read:getMode");
    algo.setMode = function(v) { algo.presetMode = v === "LedFx Fire" ? v : "Artistic"; };
    algo.getMode = function() { return algo.presetMode; };

    algo.speed = 0.04;
    algo.color_shift = 0.15;
    algo.intensity = 8;
    algo.fade_chance = 0.5;

    algo.properties.push("name:speed|type:float|display:Speed|write:setSpeed|read:getSpeed");
    algo.properties.push("name:color_shift|type:float|display:Color Shift|write:setColorShift|read:getColorShift");
    algo.properties.push("name:intensity|type:range|display:Intensity|values:1,64|write:setIntensity|read:getIntensity");
    algo.properties.push("name:fade_chance|type:float|display:Fade Chance|write:setFadeChance|read:getFadeChance");

    algo.setSpeed = function(v) { algo.speed = parseFloat(v); };
    algo.getSpeed = function() { return algo.speed; };
    algo.setColorShift = function(v) { algo.color_shift = parseFloat(v); };
    algo.getColorShift = function() { return algo.color_shift; };
    algo.setIntensity = function(v) { algo.intensity = parseInt(v, 10); };
    algo.getIntensity = function() { return algo.intensity; };
    algo.setFadeChance = function(v) { algo.fade_chance = parseFloat(v); };
    algo.getFadeChance = function() { return algo.fade_chance; };

    var sparkPixels = null;
    var sparks = null;
    var sparkX = null;
    var emaLows = 0;
    var ledFxStepAccumMs = 0;
    var LEDFX_STEP_MS = 1000 / 60;
    var MAX_STEPS_PER_FRAME = 240;
    var NOMINAL_HZ = 50;
    var ledFxKeys = {audioKey: "", geometryKey: ""};
    var getLedFxOutputOptions = HSVUtil.attachStripTransformControls(algo, {
        prefix: "ledFx",
        display: "LedFx "
    });

    function init(n) {
        sparkPixels = new Array(n);
        for (var i = 0; i < n; i++) sparkPixels[i] = 0;
        ledFxStepAccumMs = 0;
        var sc = algo.intensity;
        sparks = new Array(sc);
        sparkX = new Array(sc);
        for (var i = 0; i < sc; i++) {
            sparks[i] = 0;
            sparkX[i] = Math.random() * 5;
        }
    }

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var map = HSVUtil.createMap(width, height);
        if (!audio) return map;

        var ledFxMode = algo.presetMode === "LedFx Fire";
        var N = ledFxMode ? Math.max(1, width * height) : Math.max(1, width);
        if (ledFxMode) {
            if (HSVUtil.audioStateChanged(ledFxKeys, audio, width, height)) {
                init(N);
                emaLows = 0;
            }
        }
        if (!sparkPixels || sparkPixels.length !== N || sparks.length !== algo.intensity) init(N);
        var dtSeconds = HSVUtil.audioSeconds(audio);

        // EMA filter on lows (decay=0.05, rise=0.99)
        var rawLows = HSVUtil.powerForRange(audio, "Lows", ledFxMode);
        var alphaL = (rawLows > emaLows) ? 0.99 : 0.05;
        emaLows = alphaL * rawLows + (1 - alphaL) * emaLows;

        // Audio modulation
        var cooling = 0.75 + emaLows * 0.25;
        var curSpeed = algo.speed + emaLows * 0.01;
        var curFadeChance = algo.fade_chance / 10;

        function simulateFireStep(stepMs, fadeChance) {
            for (var i = 0; i < N; i++)
                sparkPixels[i] *= cooling;
            if (N > 5) {
                for (var i = N - 1; i >= 5; i--)
                    sparkPixels[i] = (sparkPixels[i - 1] + sparkPixels[i - 2] + sparkPixels[i - 3] * 2 + sparkPixels[i - 4] * 3) / 7;
            }

            var sc = sparks.length;
            for (var i = 0; i < sc; i++) {
                if (sparks[i] <= 0) {
                    sparks[i] = 0.5 + Math.random() * 0.5;
                    sparkX[i] = Math.random() * 5;
                }

                var sparkStep = sparks[i] * sparks[i] * (stepMs * curSpeed) * (N / 100);
                sparkX[i] += sparkStep;

                if (sparkX[i] >= N || Math.random() < fadeChance) {
                    sparks[i] = 0;
                    sparkX[i] = 0;
                    continue;
                }

                var jStart = Math.max(0, Math.floor(sparkX[i] - sparkStep));
                var jEnd = Math.floor(sparkX[i]);
                var heat = Math.max(0, Math.min(1, 1 - sparks[i] * 0.4)) * 0.5;
                for (var j = jStart; j < jEnd && j < N; j++)
                    sparkPixels[j] += heat;
            }
        }

        if (ledFxMode) {
            ledFxStepAccumMs += dtSeconds * 1000.0;
            var steps = Math.floor(ledFxStepAccumMs / LEDFX_STEP_MS);
            if (steps > MAX_STEPS_PER_FRAME) steps = MAX_STEPS_PER_FRAME;
            if (steps > 0)
                ledFxStepAccumMs -= steps * LEDFX_STEP_MS;
            var stepFadeChance = 1 - Math.pow(1 - curFadeChance, (LEDFX_STEP_MS / 1000.0) * NOMINAL_HZ);
            for (var n = 0; n < steps; n++)
                simulateFireStep(LEDFX_STEP_MS, stepFadeChance);
        } else {
            simulateFireStep(audio.dt * 60000 / audio.bpm, curFadeChance);
        }

        var colorShift = algo.color_shift;
        var stops = ledFxMode ? HSVUtil.ledFxGradientStops(algo) : null;
        var ledFxOutputOptions = ledFxMode ? getLedFxOutputOptions() : null;
        for (var i = 0; i < N; i++) {
            var x = ledFxMode ? (i % width) : i;
            var y = ledFxMode ? Math.floor(i / width) : 0;
            var px = sparkPixels[i];

            var h = Math.max(0, Math.min(1, px * px)) * 0.1 + colorShift;
            var s = 1 - (px - 1) * 2;
            var v = px * 2;

            if (ledFxMode) {
                var hsvOut = HSVUtil.ledFxColor(stops, h, s, v);
                HSVUtil.setPixel(map, width, x, y, hsvOut.h, hsvOut.s, hsvOut.v);
            } else {
                var hc = HSVUtil.mod1(h);
                var scOut = HSVUtil.clamp01(s);
                var vc = HSVUtil.clamp01(v);
                for (var yy = 0; yy < height; yy++)
                    HSVUtil.setPixel(map, width, x, yy, hc, scOut, vc);
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
