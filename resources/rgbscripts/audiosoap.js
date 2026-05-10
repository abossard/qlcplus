/*
  Q Light Controller Plus
  audiosoap.js

  Copyright (c) QLC+ contributors
  Ported from LedFX "Soap" effect (MIT License)

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

    var DEFAULT_GRADIENT = [0xFF0000, 0xFF7800, 0xFFC800, 0x00FF00, 0x00C78C, 0x0000FF, 0x800080, 0xFF00B2];

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
        return (algo.gradientColors && algo.gradientColors.length > 0) ? algo.gradientColors : DEFAULT_GRADIENT;
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
                prevPixels[y][x] = [0, 0, 0];
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
                var n = (RGBUtil.simplex2d(nx, ny) + 1) * 0.5;
                noiseField[iy][ix] = noiseField[iy][ix] * SMOOTH + n * newMix;
            }
        }
    }

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) {
        algo.gradientColors = RGBUtil.buildGradientColors(rawColors);
    };
    algo.rgbMapGetColors = function() {
        return gradientStops().slice();
    };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var map = RGBUtil.createMap(width, height);
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

        // Palette from noise: palIdx = ((1 - noise) * 3) % 1
        var gradient = gradientStops();
        var paletteR = new Array(height);
        var paletteG = new Array(height);
        var paletteB = new Array(height);
        for (var y = 0; y < height; y++) {
            paletteR[y] = new Array(width);
            paletteG[y] = new Array(width);
            paletteB[y] = new Array(width);
            for (var x = 0; x < width; x++) {
                var palIdx = ((1 - noiseField[y][x]) * 3) % 1;
                if (palIdx < 0) palIdx += 1;
                var packed = RGBUtil.gradientColorAt(gradient, palIdx);
                paletteR[y][x] = (packed >> 16) & 0xFF;
                paletteG[y][x] = (packed >> 8) & 0xFF;
                paletteB[y][x] = packed & 0xFF;
            }
        }

        // Seed prevPixels from palette on first frame or resize
        if (needSeed) {
            for (var y = 0; y < height; y++)
                for (var x = 0; x < width; x++)
                    prevPixels[y][x] = [paletteR[y][x], paletteG[y][x], paletteB[y][x]];
            needSeed = false;
        }

        // Smear amplitudes
        var ampX = Math.max(1, (width - 8) / 8) * (1 + 7 * algo.density);
        var ampY = Math.max(1, (height - 8) / 8) * (1 + 7 * algo.density);

        // Per-row and per-col shift amounts
        var amtRows = new Array(height);
        for (var y = 0; y < height; y++)
            amtRows[y] = (noiseField[y][0] - 0.5) * ampX;
        var amtCols = new Array(width);
        for (var x = 0; x < width; x++)
            amtCols[x] = (noiseField[0][x] - 0.5) * ampY;

        // Pass 1: smear rows (horizontal) — prevPixels → afterRows
        var afterR = new Array(height);
        var afterG = new Array(height);
        var afterB = new Array(height);
        for (var y = 0; y < height; y++) {
            afterR[y] = new Array(width);
            afterG[y] = new Array(width);
            afterB[y] = new Array(width);
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
                var ar, ag, ab, br, bg, bb;
                if (zD >= 0 && zD < width) {
                    var p = prevPixels[y][zD]; ar = p[0]; ag = p[1]; ab = p[2];
                } else {
                    var cx = Math.max(0, Math.min(width - 1, zD));
                    ar = paletteR[y][cx]; ag = paletteG[y][cx]; ab = paletteB[y][cx];
                }
                if (zF >= 0 && zF < width) {
                    var p = prevPixels[y][zF]; br = p[0]; bg = p[1]; bb = p[2];
                } else {
                    var cx = Math.max(0, Math.min(width - 1, zF));
                    br = paletteR[y][cx]; bg = paletteG[y][cx]; bb = paletteB[y][cx];
                }
                afterR[y][x] = ar * wA + br * wB;
                afterG[y][x] = ag * wA + bg * wB;
                afterB[y][x] = ab * wA + bb * wB;
            }
        }

        // Pass 2: smear cols (vertical) — afterRows → output
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
                var ar, ag, ab, br, bg, bb;
                if (zD >= 0 && zD < height) {
                    ar = afterR[zD][x]; ag = afterG[zD][x]; ab = afterB[zD][x];
                } else {
                    var cy = Math.max(0, Math.min(height - 1, zD));
                    ar = paletteR[cy][x]; ag = paletteG[cy][x]; ab = paletteB[cy][x];
                }
                if (zF >= 0 && zF < height) {
                    br = afterR[zF][x]; bg = afterG[zF][x]; bb = afterB[zF][x];
                } else {
                    var cy = Math.max(0, Math.min(height - 1, zF));
                    br = paletteR[cy][x]; bg = paletteG[cy][x]; bb = paletteB[cy][x];
                }
                var r = ar * wA + br * wB;
                var g = ag * wA + bg * wB;
                var b = ab * wA + bb * wB;

                prevPixels[y][x] = [r, g, b];
                map[y][x] = RGBUtil.rgb(
                    Math.max(0, Math.min(255, r)),
                    Math.max(0, Math.min(255, g)),
                    Math.max(0, Math.min(255, b))
                );
            }
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
