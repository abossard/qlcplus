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

    // Upper bounds from the LedFx Crawler schema
    algo.setSpeed = function(v) { algo.speed = HSVUtil.parseNumber(v, algo.speed, 0, 1); };
    algo.getSpeed = function() { return algo.speed; };
    algo.setReactivity = function(v) { algo.reactivity = HSVUtil.parseNumber(v, algo.reactivity, 0, 1); };
    algo.getReactivity = function() { return algo.reactivity; };
    algo.setSway = function(v) { algo.sway = HSVUtil.parseNumber(v, algo.sway, 0, 50); };
    algo.getSway = function() { return algo.sway; };
    algo.setChop = function(v) { algo.chop = HSVUtil.parseNumber(v, algo.chop, 0, 100); };
    algo.getChop = function() { return algo.chop; };
    algo.setStretch = function(v) { algo.stretch = HSVUtil.parseNumber(v, algo.stretch, 0, 10); };
    algo.getStretch = function() { return algo.stretch; };

    var timePosition = 0;
    var emaLows = 0;
    var ledFxLowsPower = 0;
    var ledFxFieldMs = 0;
    var ledFxTimestepMs = 0;
    var ledFxKeys = {audioKey: "", geometryKey: ""};
    var NOMINAL_HZ = 60;
    var getLedFxOutputOptions = HSVUtil.attachStripTransformControls(algo, {
        prefix: "ledFx",
        display: "LedFx "
    });

    function hsvTime(modifier, ts) {
        var t = (ts * modifier / 65.536) % 1;
        return t < 0 ? t + 1 : t;
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

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var map = HSVUtil.createMap(width, height);
        if (!audio) return map;

        var ledFxMode = algo.presetMode === "LedFx Crawler";
        var speed = algo.speed;
        var reactivity = algo.reactivity;
        var sway = algo.sway;
        var chop = algo.chop;
        var stretch = Math.max(0.00001, algo.stretch);
        var stretchSpan = stretch / 10;
        var lows, t1, t3;
        if (ledFxMode) {
            if (HSVUtil.audioStateChanged(ledFxKeys, audio, width, height)) {
                ledFxFieldMs = 0;
                ledFxTimestepMs = 0;
                ledFxLowsPower = 0;
            }
            var dt = HSVUtil.audioSeconds(audio);
            var rawLows = HSVUtil.powerForRange(audio, "Lows", true);
            var ledFxLowsStepSum = smoothedStepSum(ledFxLowsPower, rawLows, 0.1, dt * NOMINAL_HZ);
            ledFxLowsPower += (rawLows - ledFxLowsPower) * elapsedAlpha(0.1, dt);
            lows = ledFxLowsPower;
            var dtMs = dt * 1000.0;
            ledFxFieldMs += dtMs;
            ledFxTimestepMs += dtMs + ledFxLowsStepSum * reactivity * speed * 1000.0;
            t1 = HSVUtil.time01(speed * sway, ledFxTimestepMs);
            t3 = HSVUtil.time01(speed * chop + lows * reactivity, ledFxFieldMs);
        } else {
            emaLows = HSVUtil.attackRelease(emaLows, audio.low, 0.1, 0.1, HSVUtil.elapsedFrames(audio));
            lows = emaLows;
            timePosition = (timePosition || 0) + audio.dt;
            t1 = hsvTime(speed * sway, timePosition + lows * reactivity * speed);
            t3 = hsvTime(speed * chop + lows * reactivity, timePosition);
        }

        var sinT1 = HSVUtil.sin01(t1);
        var pixelCount = ledFxMode ? Math.max(1, width * height) : Math.max(1, width);
        var denom = Math.max(1, pixelCount - 1);
        var stops = ledFxMode ? HSVUtil.ledFxGradientStops(algo) : null;

        for (var i = 0; i < pixelCount; i++) {
            var x = ledFxMode ? (i % width) : i;
            var y = ledFxMode ? Math.floor(i / width) : 0;
            var i1 = i / denom;

            var h = (i + t3 * pixelCount) / pixelCount;
            h *= stretch;
            h = HSVUtil.wrap(h, stretchSpan);
            h += i1;
            h += sinT1;

            var v = HSVUtil.sin01(h);
            v = v * v;

            h = HSVUtil.mod1(h);
            if (ledFxMode) {
                var base = HSVUtil.gradientRgbAt(stops, h);
                var hsvOut = HSVUtil.rgbToHsvUnclipped(base[0] * v, base[1] * v, base[2] * v);
                HSVUtil.setPixel(map, width, x, y, hsvOut.h, hsvOut.s, hsvOut.v);
            } else {
                for (var yy = 0; yy < height; yy++)
                    HSVUtil.setPixel(map, width, x, yy, h, 1, v);
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
