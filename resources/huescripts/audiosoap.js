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
    algo.presetMode = "Artistic";
    algo.properties.push("name:mode|type:list|display:Response|values:Artistic,LedFx Soap|write:setMode|read:getMode");
    algo.setMode = function(v) {
        var next = v === "LedFx Soap" ? v : "Artistic";
        if (next !== algo.presetMode) {
            ledAudioIdentity = "";
            ledW = 0;
            ledH = 0;
        }
        algo.presetMode = next;
    };
    algo.getMode = function() { return algo.presetMode; };

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

    algo.setDensity = function(v) { algo.density = parseFloat(v); };
    algo.getDensity = function() { return algo.density; };
    algo.setSpeed = function(v) { algo.speed = parseFloat(v); };
    algo.getSpeed = function() { return algo.speed; };
    algo.setIntensity = function(v) { algo.intensity = parseFloat(v); };
    algo.getIntensity = function() { return algo.intensity; };
    algo.setFrequencyRange = function(v) { algo.frequency_range = String(v); };
    algo.getFrequencyRange = function() { return algo.frequency_range; };

    var NOISE_FREQ = 3.0;
    var SMOOTH = 0.5;

    // Artistic state (kept unchanged).
    var phaseX = Math.random() * 256;
    var phaseY = Math.random() * 256;
    var noiseField = null;
    var prevPixels = null;
    var lastW = 0, lastH = 0;

    // LedFx Soap state.
    var ledPhaseSeedX = Math.random() * 256;
    var ledPhaseSeedY = Math.random() * 256;
    var ledPhaseX = ledPhaseSeedX;
    var ledPhaseY = ledPhaseSeedY;
    var ledNoise = null;
    var ledPrevRgb = null;
    var ledW = 0;
    var ledH = 0;
    var ledAudioIdentity = "";

    function gradientStops() {
        return algo.hasUserColors ? algo.colors : DEFAULT_HSV_STOPS;
    }

    function powerFor(audio) {
        var power = audio.low;
        if (algo.frequency_range === "Beat") power = audio.beat;
        else if (algo.frequency_range === "Bass") power = audio.bass;
        else if (algo.frequency_range === "Mids") power = audio.mid;
        else if (algo.frequency_range === "High") power = audio.high;
        return HSVUtil.clamp01(isFinite(power) ? power : 0);
    }

    function phaseStep(audio, dtSec) {
        var impulse = powerFor(audio) * 6.0;
        var audioSpeed = (algo.intensity === 0)
            ? algo.speed
            : (algo.speed * impulse * algo.intensity);
        return audioSpeed * audioSpeed * 0.5 * dtSec;
    }

    function initBuffers(w, h) {
        noiseField = new Array(h);
        for (var y = 0; y < h; y++) {
            noiseField[y] = new Array(w);
            for (var x = 0; x < w; x++)
                noiseField[y][x] = 0.5;
        }
        prevPixels = null;
        lastW = w; lastH = h;
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

    function syncLedStateIdentity(w, h, audio) {
        var nextAudioIdentity = HSVUtil.audioIdentityKey(audio, ledAudioIdentity);
        if (ledAudioIdentity !== nextAudioIdentity || ledW !== w || ledH !== h) {
            ledAudioIdentity = nextAudioIdentity;
            initLedState(w, h);
        }
    }

    function initLedState(w, h) {
        ledNoise = new Array(h);
        for (var y = 0; y < h; y++) {
            ledNoise[y] = new Array(w);
            for (var x = 0; x < w; x++)
                ledNoise[y][x] = 0;
        }
        ledPrevRgb = null;
        ledW = w;
        ledH = h;
        ledPhaseX = ledPhaseSeedX;
        ledPhaseY = ledPhaseSeedY;
    }

    function updateLedNoise(w, h, dtSeconds) {
        if (dtSeconds <= 0) return;
        var spanX = NOISE_FREQ * 2;
        var spanY = NOISE_FREQ * 2;
        var stepX = spanX / Math.max(1, w - 1);
        var stepY = spanY / Math.max(1, h - 1);
        var x0 = ledPhaseX - spanX * 0.5;
        var y0 = ledPhaseY - spanY * 0.5;
        var frameScale = dtSeconds * 60;
        var alpha = 1 - Math.pow(0.5, frameScale);
        for (var iy = 0; iy < h; iy++) {
            var ny = y0 + iy * stepY;
            for (var ix = 0; ix < w; ix++) {
                var nx = x0 + ix * stepX;
                var n = (HSVUtil.simplex2d(nx * 0.3, ny * 0.3) + 1) * 0.5;
                ledNoise[iy][ix] = ledNoise[iy][ix] * (1 - alpha) + n * alpha;
            }
        }
    }

    function smoothstep(x) {
        return x * x * (3 - 2 * x);
    }

    function mixRgb(a, b, wA, wB) {
        return [
            a[0] * wA + b[0] * wB,
            a[1] * wA + b[1] * wB,
            a[2] * wA + b[2] * wB
        ];
    }

    function mixHsv(a, b, wA, wB) {
        return {
            h: HSVUtil.lerpHue(a.h, b.h, wB),
            s: a.s * wA + b.s * wB,
            v: a.v * wA + b.v * wB
        };
    }

    function smear(source, palette, amount, axis, w, h, mix) {
        var out = new Array(h);
        for (var y = 0; y < h; y++) out[y] = new Array(w);
        if (axis === 1) {
            for (var row = 0; row < h; row++) {
                var amt = amount[row];
                var sgn = amt > 0 ? 1 : (amt < 0 ? -1 : 0);
                var mag = Math.abs(amt);
                var di = Math.floor(mag);
                var frac = mag - di;
                var wB = smoothstep(frac);
                var wA = 1 - wB;
                for (var x = 0; x < w; x++) {
                    var zD = x + sgn * di;
                    var zF = zD + sgn;
                    var a, b;
                    if (zD >= 0 && zD < w) a = source[row][zD];
                    else { var cx = Math.max(0, Math.min(w - 1, zD)); a = palette[row][cx]; }
                    if (zF >= 0 && zF < w) b = source[row][zF];
                    else { var cx2 = Math.max(0, Math.min(w - 1, zF)); b = palette[row][cx2]; }
                    out[row][x] = mix(a, b, wA, wB);
                }
            }
            return out;
        }
        for (var col = 0; col < w; col++) {
            var amtCol = amount[col];
            var sgnCol = amtCol > 0 ? 1 : (amtCol < 0 ? -1 : 0);
            var magCol = Math.abs(amtCol);
            var diCol = Math.floor(magCol);
            var fracCol = magCol - diCol;
            var wBCol = smoothstep(fracCol);
            var wACol = 1 - wBCol;
            for (var y2 = 0; y2 < h; y2++) {
                var zDCol = y2 + sgnCol * diCol;
                var zFCol = zDCol + sgnCol;
                var aCol, bCol;
                if (zDCol >= 0 && zDCol < h) aCol = source[zDCol][col];
                else { var cy = Math.max(0, Math.min(h - 1, zDCol)); aCol = palette[cy][col]; }
                if (zFCol >= 0 && zFCol < h) bCol = source[zFCol][col];
                else { var cy2 = Math.max(0, Math.min(h - 1, zFCol)); bCol = palette[cy2][col]; }
                out[y2][col] = mix(aCol, bCol, wACol, wBCol);
            }
        }
        return out;
    }

    function buildPalette(noise, w, h, sample) {
        var gradient = gradientStops();
        var palette = new Array(h);
        for (var y = 0; y < h; y++) {
            palette[y] = new Array(w);
            for (var x = 0; x < w; x++)
                palette[y][x] = sample(gradient, HSVUtil.mod1((1 - noise[y][x]) * 3.0));
        }
        return palette;
    }

    function smearFrame(prev, palette, noise, w, h, mix) {
        var ampFactor = 1 + 7 * algo.density;
        var ampX = Math.max(1, (w - 8) / 8) * ampFactor;
        var ampY = Math.max(1, (h - 8) / 8) * ampFactor;
        var amtRows = new Array(h);
        for (var row = 0; row < h; row++)
            amtRows[row] = (noise[row][0] - 0.5) * ampX;
        var amtCols = new Array(w);
        for (var col = 0; col < w; col++)
            amtCols[col] = (noise[0][col] - 0.5) * ampY;
        var afterRows = smear(prev, palette, amtRows, 1, w, h, mix);
        return smear(afterRows, palette, amtCols, 0, w, h, mix);
    }

    function writeMapFromRgb(map, rgb2d, width, height) {
        for (var y = 0; y < height; y++) {
            for (var x = 0; x < width; x++) {
                var hsv = HSVUtil.rgbToHsv(rgb2d[y][x][0], rgb2d[y][x][1], rgb2d[y][x][2]);
                var i3 = (y * width + x) * 3;
                map[i3] = hsv.h;
                map[i3 + 1] = hsv.s;
                map[i3 + 2] = hsv.v;
            }
        }
        return map;
    }

    function renderArtistic(map, width, height, audio) {
        if (lastW !== width || lastH !== height) initBuffers(width, height);

        var move = phaseStep(audio, HSVUtil.audioSeconds(audio));
        phaseX += move;
        phaseY += move;

        genNoiseField(width, height);
        var palette = buildPalette(noiseField, width, height, HSVUtil.gradientAt);

        prevPixels = smearFrame(prevPixels || palette, palette, noiseField, width, height, mixHsv);
        for (var yOut = 0; yOut < height; yOut++) {
            for (var xOut = 0; xOut < width; xOut++) {
                var px = prevPixels[yOut][xOut];
                var i3 = (yOut * width + xOut) * 3;
                map[i3] = px.h;
                map[i3 + 1] = HSVUtil.clamp01(px.s);
                map[i3 + 2] = HSVUtil.clamp01(px.v);
            }
        }

        return map;
    }

    function renderLedFx(map, width, height, audio) {
        syncLedStateIdentity(width, height, audio);

        var dtSec = HSVUtil.audioSeconds(audio);
        if (dtSec > 0) {
            var move = phaseStep(audio, dtSec);
            ledPhaseX += move;
            ledPhaseY += move;
            updateLedNoise(width, height, dtSec);
        }

        var palette = buildPalette(ledNoise, width, height, HSVUtil.gradientRgbAt);
        if (!ledPrevRgb)
            ledPrevRgb = palette;
        if (dtSec <= 0)
            return writeMapFromRgb(map, ledPrevRgb, width, height);

        ledPrevRgb = smearFrame(ledPrevRgb, palette, ledNoise, width, height, mixRgb);
        return writeMapFromRgb(map, ledPrevRgb, width, height);
    }

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var map = HSVUtil.createMap(width, height);
        if (!audio) return map;
        if (width <= 0 || height <= 0) return map;
        if (algo.presetMode === "LedFx Soap")
            return renderLedFx(map, width, height, audio);
        return renderArtistic(map, width, height, audio);
    };

    testAlgo = algo;
    return algo;
  }
)();
