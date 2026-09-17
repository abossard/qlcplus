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
    algo.acceptColors = 3;
    algo.usesAudio = true;
    algo.properties = new Array();
    algo.presetMode = "Artistic";
    algo.properties.push("name:mode|type:list|display:Response|values:Artistic,LedFx Crawler|write:setMode|read:getMode");
    algo.setMode = function(v) { algo.presetMode = v === "LedFx Crawler" ? v : "Artistic"; };
    algo.getMode = function() { return algo.presetMode; };

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

    algo.setSpeed = function(v) { algo.speed = parseFloat(v); };
    algo.getSpeed = function() { return algo.speed; };
    algo.setReactivity = function(v) { algo.reactivity = parseFloat(v); };
    algo.getReactivity = function() { return algo.reactivity; };
    algo.setSway = function(v) { algo.sway = parseFloat(v); };
    algo.getSway = function() { return algo.sway; };
    algo.setChop = function(v) { algo.chop = parseFloat(v); };
    algo.getChop = function() { return algo.chop; };
    algo.setStretch = function(v) { algo.stretch = parseFloat(v); };
    algo.getStretch = function() { return algo.stretch; };

    var TWO_PI = 2 * Math.PI;
    var timeState = { position: 0 };
    var emaLows = 0;
    var ledFxLowsPower = 0;
    var ledFxFieldMs = 0;
    var ledFxTimestepMs = 0;
    var ledFxAudioIdentityKey = "";
    var ledFxGeometryKey = "";
    var NOMINAL_HZ = 60;
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

    function elapsedAlpha(nominalAlpha, dtSeconds) {
        if (!(dtSeconds > 0) || !(nominalAlpha > 0)) return 0;
        if (nominalAlpha >= 1) return 1;
        return 1 - Math.pow(1 - nominalAlpha, dtSeconds * NOMINAL_HZ);
    }

    function smoothedStepSum(previous, target, nominalAlpha, stepCount) {
        if (!(stepCount > 0)) return 0;
        if (!(nominalAlpha > 0)) return previous * stepCount;
        if (nominalAlpha >= 1) return target * stepCount;
        var decay = 1 - nominalAlpha;
        return (stepCount * target) +
            ((previous - target) * decay * (1 - Math.pow(decay, stepCount)) / nominalAlpha);
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

        var ledFxMode = algo.presetMode === "LedFx Crawler";
        if (ledFxMode) {
            var audioIdentityKey = HSVUtil.audioIdentityKey(audio, ledFxAudioIdentityKey);
            var geometryKey = width + "x" + height;
            if (audioIdentityKey !== ledFxAudioIdentityKey || geometryKey !== ledFxGeometryKey) {
                ledFxAudioIdentityKey = audioIdentityKey;
                ledFxGeometryKey = geometryKey;
                ledFxFieldMs = 0;
                ledFxTimestepMs = 0;
                ledFxLowsPower = 0;
            }
        }

        var dt = ledFxMode
            ? (audio.timing && isFinite(audio.timing.deltaSeconds)
                ? Math.max(0, audio.timing.deltaSeconds)
                : ((audio.bpm > 0 && isFinite(audio.dt)) ? audio.dt * 60 / audio.bpm : 0))
            : audio.dt;
        var rawLows = ledFxMode ? rawLowPower(audio) : audio.low;
        var lows;
        if (ledFxMode) {
            var nominalSteps = dt * NOMINAL_HZ;
            var previousLows = ledFxLowsPower;
            var alphaLedFx = elapsedAlpha(0.1, dt);
            ledFxLowsPower += (rawLows - ledFxLowsPower) * alphaLedFx;
            var ledFxLowsStepSum = smoothedStepSum(previousLows, rawLows, 0.1, nominalSteps);
            lows = ledFxLowsPower;
        } else {
            emaLows = 0.1 * rawLows + 0.9 * emaLows;
            lows = emaLows;
        }

        var speed = algo.speed;
        var reactivity = algo.reactivity;
        var sway = algo.sway;
        var chop = algo.chop;
        var stretch = Math.max(0.00001, algo.stretch);
        var stretchSpan = stretch / 10;

        var timeAccum = timeState.position || 0;
        var timestep = timeAccum + lows * reactivity * speed;
        if (!ledFxMode) {
            timeAccum = ((timeState.position = (timeState.position || 0) + dt) && timeState.position);
            timestep = timeAccum + lows * reactivity * speed;
        }

        var t1 = ledFxMode ? 0 : hsvTime(speed * sway, timestep);
        var t3 = ledFxMode ? 0 : hsvTime(speed * chop + lows * reactivity, timeAccum);
        if (ledFxMode) {
            var dtMs = dt * 1000.0;
            ledFxFieldMs += dtMs;
            ledFxTimestepMs += dtMs + ledFxLowsStepSum * reactivity * speed * 1000.0;
            t1 = HSVUtil.time01(speed * sway, ledFxTimestepMs);
            t3 = HSVUtil.time01(speed * chop + lows * reactivity, ledFxFieldMs);
        }

        var sinT1 = hsvSin(t1);
        var pixelCount = ledFxMode ? Math.max(1, width * height) : Math.max(1, width);
        var denom = Math.max(1, pixelCount - 1);
        var stops = ledFxMode ? ledFxGradientStops() : null;
        var ledFxOutputOptions = ledFxMode ? getLedFxOutputOptions() : null;

        for (var i = 0; i < pixelCount; i++) {
            var x = ledFxMode ? (i % width) : i;
            var y = ledFxMode ? Math.floor(i / width) : 0;
            var i1 = i / denom;

            var h = (i + t3 * pixelCount) / pixelCount;
            h *= stretch;
            h = ((h % stretchSpan) + stretchSpan) % stretchSpan;
            h += i1;
            h += sinT1;

            var v = hsvSin(h);
            v = v * v;

            h = HSVUtil.mod1(h);
            if (ledFxMode) {
                var base = HSVUtil.gradientRgbAt(stops, h);
                var rgbOut = [base[0] * v, base[1] * v, base[2] * v];
                var hsvOut = HSVUtil.rgbToHsvUnclipped(rgbOut[0], rgbOut[1], rgbOut[2]);
                HSVUtil.setPixel(map, width, x, y, hsvOut.h, hsvOut.s, hsvOut.v);
            } else {
                for (var yy = 0; yy < height; yy++)
                    HSVUtil.setPixel(map, width, x, yy, h, 1, v);
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
