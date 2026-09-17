/*
  Q Light Controller Plus
  audiowater.js

  Copyright (c) QLC+ contributors
  Ported from LedFx "Water" effect (MIT License)

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
    algo.name = "Audio Water";
    algo.author = "Ported from LedFx";
    algo.acceptColors = 3;
    algo.usesAudio = true;
    algo.properties = new Array();
    algo.presetMode = "Artistic";
    algo.properties.push("name:mode|type:list|display:Response|values:Artistic,LedFx Water|write:setMode|read:getMode");
    algo.setMode = function(v) { algo.presetMode = v === "LedFx Water" ? v : "Artistic"; };
    algo.getMode = function() { return algo.presetMode; };

    algo.speed = 1;
    algo.vertical_shift = 0.12;
    algo.bass_size = 8;
    algo.mids_size = 6;
    algo.high_size = 3;
    algo.viscosity = 6;

    algo.properties.push("name:speed|type:float|display:Speed|write:setSpeed|read:getSpeed");
    algo.properties.push("name:vertical_shift|type:float|display:Vertical Shift|write:setVerticalShift|read:getVerticalShift");
    algo.properties.push("name:bass_size|type:float|display:Bass Size|write:setBassSize|read:getBassSize");
    algo.properties.push("name:mids_size|type:float|display:Mids Size|write:setMidsSize|read:getMidsSize");
    algo.properties.push("name:high_size|type:float|display:High Size|write:setHighSize|read:getHighSize");
    algo.properties.push("name:viscosity|type:float|display:Viscosity|write:setViscosity|read:getViscosity");

    function clamp(v, lo, hi) { var n = parseFloat(v); return isNaN(n) ? lo : Math.max(lo, Math.min(hi, n)); }
    algo.setSpeed = function(v) { algo.speed = clamp(v, 1, 3); };
    algo.getSpeed = function() { return algo.speed; };
    algo.setVerticalShift = function(v) { algo.vertical_shift = clamp(v, -0.2, 1); };
    algo.getVerticalShift = function() { return algo.vertical_shift; };
    algo.setBassSize = function(v) { algo.bass_size = clamp(v, 0, 15); };
    algo.getBassSize = function() { return algo.bass_size; };
    algo.setMidsSize = function(v) { algo.mids_size = clamp(v, 0, 15); };
    algo.getMidsSize = function() { return algo.mids_size; };
    algo.setHighSize = function(v) { algo.high_size = clamp(v, 0, 15); };
    algo.getHighSize = function() { return algo.high_size; };
    algo.setViscosity = function(v) { algo.viscosity = clamp(v, 2, 12); };
    algo.getViscosity = function() { return algo.viscosity; };

    var buf0 = null, buf1 = null, curBuf = 0;
    var EMITTER_DRIFT_RATE = 0.0002;
    var midsEmitters = [[0.25, 1.0], [0.75, -1.0]];
    var highEmitters = [[0.125, 1.5], [0.375, -2.5], [0.625, 2.5], [0.875, -1.5]];
    var ledFxAudioIdentityKey = "";
    var ledFxGeometryKey = "";
    var getLedFxOutputOptions = HSVUtil.attachStripTransformControls(algo, {
        prefix: "ledFx",
        display: "LedFx "
    });

    function init(w) {
        buf0 = new Array(w); buf1 = new Array(w);
        for (var i = 0; i < w; i++) { buf0[i] = 0; buf1[i] = 0; }
        curBuf = 0;
        midsEmitters = [[0.25, 1.0], [0.75, -1.0]];
        highEmitters = [[0.125, 1.5], [0.375, -2.5], [0.625, 2.5], [0.875, -1.5]];
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

    function createDrop(pos, h, w) {
        if (pos < 1 || pos >= w - 1) return;
        buf0[pos] = buf0[pos - 1] = buf0[pos + 1] = h;
        buf1[pos] = buf1[pos - 1] = buf1[pos + 1] = h;
    }

    function smooth3(arr, w) {
        if (w < 3) return;
        var prev = arr[0];
        for (var i = 1; i < w - 1; i++) {
            var cur = arr[i];
            arr[i] = (prev + cur + arr[i + 1]) / 3;
            prev = cur;
        }
    }

    function doRipple(dampFactor, w) {
        var src = (curBuf === 0) ? buf1 : buf0;
        var dst = (curBuf === 0) ? buf0 : buf1;
        for (var i = 1; i < w - 1; i++)
            dst[i] = ((src[i - 1] + src[i + 1] + src[i] * 2) / 2) - dst[i];
        smooth3(dst, w);
        for (var i = 0; i < w; i++)
            dst[i] -= dst[i] / dampFactor;
        curBuf = 1 - curBuf;
    }

    function thirdMaxPowers(audio) {
        var bank = audio && audio.banks && audio.banks.full;
        if (!bank || !bank.count) {
            return {
                low: HSVUtil.clamp01(audio && isFinite(audio.low) ? audio.low : 0),
                mid: HSVUtil.clamp01(audio && isFinite(audio.mid) ? audio.mid : 0),
                high: HSVUtil.clamp01(audio && isFinite(audio.high) ? audio.high : 0)
            };
        }
        var values = Array.isArray(bank.processed) ? bank.processed : [];
        if (!values.length) {
            return {
                low: HSVUtil.clamp01(audio && isFinite(audio.low) ? audio.low : 0),
                mid: HSVUtil.clamp01(audio && isFinite(audio.mid) ? audio.mid : 0),
                high: HSVUtil.clamp01(audio && isFinite(audio.high) ? audio.high : 0)
            };
        }
        var count = Math.min(bank.count, values.length);
        var a = Math.floor(count * 0.2);
        var b = Math.floor(count * 0.5);
        function maxIn(start, end) {
            var peak = 0;
            for (var i = start; i < end; i++) {
                var v = isFinite(values[i]) ? values[i] : 0;
                if (v > peak) peak = v;
            }
            return HSVUtil.clamp01(peak);
        }
        return { low: maxIn(0, a), mid: maxIn(a, b), high: maxIn(b, count) };
    }

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var map = HSVUtil.createMap(width, height);
        if (!audio) return map;
        var ledFxMode = algo.presetMode === "LedFx Water";
        var stripLength = ledFxMode ? Math.max(1, width * height) : Math.max(1, width);
        var stops = ledFxMode ? ledFxGradientStops() : null;
        var ledFxOutputOptions = ledFxMode ? getLedFxOutputOptions() : null;
        if (ledFxMode) {
            var audioIdentityKey = HSVUtil.audioIdentityKey(audio, ledFxAudioIdentityKey);
            var geometryKey = width + "x" + height;
            if (audioIdentityKey !== ledFxAudioIdentityKey || geometryKey !== ledFxGeometryKey) {
                ledFxAudioIdentityKey = audioIdentityKey;
                ledFxGeometryKey = geometryKey;
                init(stripLength);
            }
        }
        if (!buf0 || buf0.length !== stripLength) init(stripLength);
        if (stripLength < 5) {
            if (ledFxMode)
                HSVUtil.applyStripTransforms(map, width, height, ledFxOutputOptions);
            return map;
        }

        var speed = algo.speed;
        var dampFactor = Math.pow(2, algo.viscosity);
        var shift = algo.vertical_shift;
        var ledFxDrift = ledFxMode ? (HSVUtil.audioSeconds(audio) * 60) : 1;

        var power = ledFxMode ? thirdMaxPowers(audio) : {
            low: HSVUtil.clamp01(audio.low),
            mid: HSVUtil.clamp01(audio.mid),
            high: HSVUtil.clamp01(audio.high)
        };
        var lowP = Math.pow(power.low, 2);
        var midP = Math.pow(power.mid, 2);
        var hiP  = Math.pow(power.high, 2);

        function injectDrops() {
            createDrop(1, lowP * algo.bass_size, stripLength);
            createDrop(Math.floor(stripLength / 2), lowP * algo.bass_size, stripLength);
            createDrop(stripLength - 2, lowP * algo.bass_size, stripLength);

            for (var i = 0; i < midsEmitters.length; i++) {
                var pos = 1 + Math.floor(midsEmitters[i][0] * (stripLength - 2));
                createDrop(pos, midP * algo.mids_size, stripLength);
                midsEmitters[i][0] += EMITTER_DRIFT_RATE * midsEmitters[i][1] * speed * ledFxDrift;
                if (midsEmitters[i][0] < 0) midsEmitters[i][0] += 1;
                else if (midsEmitters[i][0] > 1) midsEmitters[i][0] -= 1;
            }

            for (var i = 0; i < highEmitters.length; i++) {
                var pos = 1 + Math.floor(highEmitters[i][0] * (stripLength - 2));
                createDrop(pos, hiP * algo.high_size, stripLength);
                highEmitters[i][0] += EMITTER_DRIFT_RATE * highEmitters[i][1] * speed * ledFxDrift;
                if (highEmitters[i][0] < 0) highEmitters[i][0] += 1;
                else if (highEmitters[i][0] > 1) highEmitters[i][0] -= 1;
            }
        }
        if (!ledFxMode)
            injectDrops();

        var speedSteps = ledFxMode ? Math.floor(speed) : Math.round(speed);
        for (var s = 0; s < speedSteps; s++)
            doRipple(dampFactor, stripLength);

        if (ledFxMode)
            injectDrops();

        var current = (curBuf === 0) ? buf0 : buf1;
        for (var i = 0; i < stripLength; i++) {
            var x = ledFxMode ? (i % width) : i;
            var yBase = ledFxMode ? Math.floor(i / width) : 0;
            var val = current[i];
            var h = HSVUtil.triangle(val);
            var vScaled = (val + shift) / (1 + shift);
            var sv = Math.max(0, Math.min(1, 2 - (vScaled + shift)));
            var v = Math.max(0, Math.min(1, vScaled));

            if (ledFxMode) {
                var base = HSVUtil.gradientRgbAt(stops, h);
                var maxRgb = Math.max(base[0], Math.max(base[1], base[2]));
                var r = (base[0] + (maxRgb - base[0]) * (1 - sv)) * v;
                var g = (base[1] + (maxRgb - base[1]) * (1 - sv)) * v;
                var b = (base[2] + (maxRgb - base[2]) * (1 - sv)) * v;
                var hsvOut = HSVUtil.rgbToHsvUnclipped(r, g, b);
                HSVUtil.setPixel(map, width, x, yBase, hsvOut.h, hsvOut.s, hsvOut.v);
            } else {
                for (var y = 0; y < height; y++) {
                    var i3 = (y * width + x) * 3;
                    map[i3] = h;
                    map[i3 + 1] = sv;
                    map[i3 + 2] = v;
                }
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
