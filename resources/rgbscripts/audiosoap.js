/*
  Q Light Controller Plus
  audiosoap.js

  Copyright (c) QLC+ contributors
  Ported from LedFx "Soap" effect (MIT License)

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
    algo.name = "Audio Soap";
    algo.author = "Ported from LedFx";
    algo.acceptColors = 5;
    algo.usesAudio = true;
    algo.properties = new Array();

    var DEFAULT_HSV_STOPS = [
        { h: 0.000, s: 1.0, v: 1.0 },
        { h: 0.078, s: 1.0, v: 1.0 },
        { h: 0.131, s: 1.0, v: 1.0 },
        { h: 0.333, s: 1.0, v: 1.0 },
        { h: 0.446, s: 1.0, v: 0.78 },
        { h: 0.667, s: 1.0, v: 1.0 },
        { h: 0.833, s: 1.0, v: 0.50 },
        { h: 0.884, s: 1.0, v: 1.0 }
    ];

    algo.density = 0.5;
    algo.speed = 0.5;
    algo.intensity = 1.0;
    algo.frequency_range = "Lows (beat+bass)";

    algo.properties.push("name:density|type:float|display:Density|write:setDensity|read:getDensity");
    algo.properties.push("name:speed|type:float|display:Speed|write:setSpeed|read:getSpeed");
    algo.properties.push("name:intensity|type:float|display:Intensity|write:setIntensity|read:getIntensity");
    algo.properties.push("name:frequency_range|type:list|display:Frequency Range|values:Beat,Bass,Lows (beat+bass),Mids,High|write:setFrequencyRange|read:getFrequencyRange");

    function clamp(v, lo, hi) { var n = parseFloat(v); return isNaN(n) ? lo : Math.max(lo, Math.min(hi, n)); }
    algo.setDensity = function(v) { algo.density = clamp(v, 0, 1); };
    algo.getDensity = function() { return algo.density; };
    algo.setSpeed = function(v) { algo.speed = clamp(v, 0, 1); };
    algo.getSpeed = function() { return algo.speed; };
    algo.setIntensity = function(v) { algo.intensity = clamp(v, 0, 2); };
    algo.getIntensity = function() { return algo.intensity; };
    algo.setFrequencyRange = function(v) { algo.frequency_range = String(v); };
    algo.getFrequencyRange = function() { return algo.frequency_range; };

    var NOISE_FREQ = 3.0;
    var SMOOTH = 0.5;
    var phaseX = Math.random() * 256;
    var phaseY = Math.random() * 256;
    var noiseField = null;
    var prevPixels = null;
    var lastW = 0, lastH = 0;
    var needSeed = true;

    function gradientStops() {
        return (algo.colors && algo.colors.length > 0)
            ? algo.colors : DEFAULT_HSV_STOPS;
    }

    function powerFor(audio) {
        if (algo.frequency_range === "Beat") return audio.power.detail.beat;
        if (algo.frequency_range === "Bass") return audio.power.detail.bass;
        if (algo.frequency_range === "Mids") return audio.power.mid;
        if (algo.frequency_range === "High") return audio.power.high;
        return audio.power.low;
    }

    function initBuffers(w, h) {
        noiseField = new Array(h);
        prevPixels = new Array(h);
        for (var y = 0; y < h; y++) {
            noiseField[y] = new Array(w);
            prevPixels[y] = new Array(w);
            for (var x = 0; x < w; x++) {
                noiseField[y][x] = 0.5;
                prevPixels[y][x] = {h: 0, s: 0, v: 0};
            }
        }
        lastW = w; lastH = h;
        needSeed = true;
    }

    function genNoiseField(w, h) {
        var spanX = NOISE_FREQ * 2;
        var spanY = NOISE_FREQ * 2;
        var stepX = spanX / Math.max(1, w - 1);
        var stepY = spanY / Math.max(1, h - 1);
        var x0 = phaseX - spanX * 0.5;
        var y0 = phaseY - spanY * 0.5;
        var newMix = 1 - SMOOTH;
        for (var iy = 0; iy < h; iy++) {
            var ny = y0 + iy * stepY;
            for (var ix = 0; ix < w; ix++) {
                var nx = x0 + ix * stepX;
                var n = (HSVUtil.simplex2d(nx, ny) + 1) * 0.5;
                noiseField[iy][ix] = noiseField[iy][ix] * SMOOTH + n * newMix;
            }
        }
    }

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var map = HSVUtil.createMap(width, height);
        if (!audio) return map;
        if (width <= 0 || height <= 0) return map;

        if (lastW !== width || lastH !== height) initBuffers(width, height);

        var dtMs = audio.timing.consumerDtMs > 0 ? audio.timing.consumerDtMs : 40;
        var dtSec = dtMs / 1000;

        var power = powerFor(audio);
        var impulse = power * 6.0;

        var audioSpeed = (algo.intensity === 0)
            ? algo.speed
            : (algo.speed * impulse * algo.intensity);
        var move = audioSpeed * audioSpeed * 0.5 * dtSec;
        phaseX += move;
        phaseY += move;

        genNoiseField(width, height);

        // Palette from noise
        var gradient = gradientStops();
        var palette = new Array(height);
        for (var y = 0; y < height; y++) {
            palette[y] = new Array(width);
            for (var x = 0; x < width; x++) {
                var palIdx = ((1 - noiseField[y][x]) * 3) % 1;
                if (palIdx < 0) palIdx += 1;
                palette[y][x] = HSVUtil.gradientAt(gradient, palIdx);
            }
        }

        // Seed prevPixels on first frame or resize
        if (needSeed) {
            for (var y = 0; y < height; y++)
                for (var x = 0; x < width; x++)
                    prevPixels[y][x] = {h: palette[y][x].h, s: palette[y][x].s, v: palette[y][x].v};
            needSeed = false;
        }

        // Smear amplitudes
        var ampX = Math.max(1, (width - 8) / 8) * (1 + 7 * algo.density);
        var ampY = Math.max(1, (height - 8) / 8) * (1 + 7 * algo.density);

        var amtRows = new Array(height);
        for (var y = 0; y < height; y++)
            amtRows[y] = (noiseField[y][0] - 0.5) * ampX;
        var amtCols = new Array(width);
        for (var x = 0; x < width; x++)
            amtCols[x] = (noiseField[0][x] - 0.5) * ampY;

        // Pass 1: smear rows (horizontal)
        var afterRow = new Array(height);
        for (var y = 0; y < height; y++) {
            afterRow[y] = new Array(width);
            var amt = amtRows[y];
            var sgn = amt > 0 ? 1 : (amt < 0 ? -1 : 0);
            var mag = Math.abs(amt);
            var di = Math.floor(mag);
            var frac = mag - di;
            var wB = frac * frac * (3 - 2 * frac);
            var wA = 1 - wB;

            for (var x = 0; x < width; x++) {
                var zD = x + sgn * di;
                var zF = zD + sgn;
                var a, b;
                if (zD >= 0 && zD < width) a = prevPixels[y][zD];
                else { var cx = Math.max(0, Math.min(width - 1, zD)); a = palette[y][cx]; }
                if (zF >= 0 && zF < width) b = prevPixels[y][zF];
                else { var cx2 = Math.max(0, Math.min(width - 1, zF)); b = palette[y][cx2]; }

                // Shortest-arc hue interpolation
                var dh = b.h - a.h;
                if (dh > 0.5) dh -= 1; else if (dh < -0.5) dh += 1;
                var rh = a.h + wB * dh;
                rh = rh - Math.floor(rh);
                afterRow[y][x] = {
                    h: rh,
                    s: a.s * wA + b.s * wB,
                    v: a.v * wA + b.v * wB
                };
            }
        }

        // Pass 2: smear cols (vertical)
        for (var x = 0; x < width; x++) {
            var amt = amtCols[x];
            var sgn = amt > 0 ? 1 : (amt < 0 ? -1 : 0);
            var mag = Math.abs(amt);
            var di = Math.floor(mag);
            var frac = mag - di;
            var wB = frac * frac * (3 - 2 * frac);
            var wA = 1 - wB;

            for (var y = 0; y < height; y++) {
                var zD = y + sgn * di;
                var zF = zD + sgn;
                var a, b;
                if (zD >= 0 && zD < height) a = afterRow[zD][x];
                else { var cy = Math.max(0, Math.min(height - 1, zD)); a = palette[cy][x]; }
                if (zF >= 0 && zF < height) b = afterRow[zF][x];
                else { var cy2 = Math.max(0, Math.min(height - 1, zF)); b = palette[cy2][x]; }

                var dh = b.h - a.h;
                if (dh > 0.5) dh -= 1; else if (dh < -0.5) dh += 1;
                var rh = a.h + wB * dh;
                rh = rh - Math.floor(rh);
                var rs = a.s * wA + b.s * wB;
                var rv = a.v * wA + b.v * wB;

                prevPixels[y][x] = {h: rh, s: rs, v: rv};
                var i3 = (y * width + x) * 3;
                map[i3] = rh;
                map[i3 + 1] = Math.max(0, Math.min(1, rs));
                map[i3 + 2] = Math.max(0, Math.min(1, rv));
            }
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
