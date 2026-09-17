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
    var ledW;
    var ledH;
    var ledAudioIdentity;
    var ledGeometryKey;
    algo.presetMode = "Artistic";
    algo.properties.push("name:mode|type:list|display:Response|values:Artistic,LedFx Soap|write:setMode|read:getMode");
    algo.setMode = function(v) {
        var next = v === "LedFx Soap" ? v : "Artistic";
        if (next !== algo.presetMode) {
            ledAudioIdentity = "";
            ledGeometryKey = "";
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
    var needSeed = true;

    // LedFx Soap state.
    var ledPhaseSeedX = Math.random() * 256;
    var ledPhaseSeedY = Math.random() * 256;
    var ledPhaseX = ledPhaseSeedX;
    var ledPhaseY = ledPhaseSeedY;
    var ledNoise = null;
    var ledPrevRgb = null;
    ledW = 0;
    ledH = 0;
    var ledNeedSeed = true;
    ledAudioIdentity = "";
    ledGeometryKey = "";

    function gradientStops() {
        return algo.hasUserColors ? algo.colors : DEFAULT_HSV_STOPS;
    }

    function powerFor(audio) {
        if (algo.frequency_range === "Beat") return audio.beat;
        if (algo.frequency_range === "Bass") return audio.bass;
        if (algo.frequency_range === "Mids") return audio.mid;
        if (algo.frequency_range === "High") return audio.high;
        return audio.low;
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

    function syncLedStateIdentity(w, h, audio) {
        var nextAudioIdentity = HSVUtil.audioIdentityKey(audio, ledAudioIdentity);
        var nextGeometryKey = w + "x" + h;
        if (ledAudioIdentity !== nextAudioIdentity ||
            ledGeometryKey !== nextGeometryKey ||
            ledW !== w ||
            ledH !== h) {
            ledAudioIdentity = nextAudioIdentity;
            ledGeometryKey = nextGeometryKey;
            initLedState(w, h, true);
        }
    }

    function initLedState(w, h, resetPhase) {
        ledNoise = new Array(h);
        ledPrevRgb = new Array(h);
        for (var y = 0; y < h; y++) {
            ledNoise[y] = new Array(w);
            ledPrevRgb[y] = new Array(w);
            for (var x = 0; x < w; x++) {
                ledNoise[y][x] = 0;
                ledPrevRgb[y][x] = [0, 0, 0];
            }
        }
        ledW = w;
        ledH = h;
        ledNeedSeed = true;
        if (resetPhase) {
            ledPhaseX = ledPhaseSeedX;
            ledPhaseY = ledPhaseSeedY;
        }
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

    function smearRgb(source, palette, amount, axis, w, h) {
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
                    out[row][x] = [
                        a[0] * wA + b[0] * wB,
                        a[1] * wA + b[1] * wB,
                        a[2] * wA + b[2] * wB
                    ];
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
                out[y2][col] = [
                    aCol[0] * wACol + bCol[0] * wBCol,
                    aCol[1] * wACol + bCol[1] * wBCol,
                    aCol[2] * wACol + bCol[2] * wBCol
                ];
            }
        }
        return out;
    }

    function writeMapFromRgb(map, rgb2d, width, height) {
        for (var y = 0; y < height; y++) {
            for (var x = 0; x < width; x++) {
                var hsv = HSVUtil.rgbToHsvUnclipped(rgb2d[y][x][0], rgb2d[y][x][1], rgb2d[y][x][2]);
                var i3 = (y * width + x) * 3;
                map[i3] = HSVUtil.clamp01(hsv.h);
                map[i3 + 1] = HSVUtil.clamp01(hsv.s);
                map[i3 + 2] = HSVUtil.clamp01(hsv.v);
            }
        }
        return map;
    }

    function renderArtistic(map, width, height, audio) {
        if (lastW !== width || lastH !== height) initBuffers(width, height);

        var dtSec = (isFinite(audio.dt) && isFinite(audio.bpm) && audio.bpm > 0)
            ? Math.max(0, audio.dt * 60 / audio.bpm)
            : HSVUtil.audioSeconds(audio);
        var power = HSVUtil.clamp01(isFinite(powerFor(audio)) ? powerFor(audio) : 0);
        var impulse = power * 6.0;
        var audioSpeed = (algo.intensity === 0)
            ? algo.speed
            : (algo.speed * impulse * algo.intensity);
        var move = audioSpeed * audioSpeed * 0.5 * dtSec;
        phaseX += move;
        phaseY += move;

        genNoiseField(width, height);

        var gradient = gradientStops();
        var palette = new Array(height);
        for (var y = 0; y < height; y++) {
            palette[y] = new Array(width);
            for (var x = 0; x < width; x++) {
                var palIdx = ((1 - noiseField[y][x]) * 3.0) % 1;
                if (palIdx < 0) palIdx += 1;
                palette[y][x] = HSVUtil.gradientAt(gradient, palIdx);
            }
        }

        if (needSeed) {
            for (var sy = 0; sy < height; sy++)
                for (var sx = 0; sx < width; sx++)
                    prevPixels[sy][sx] = {
                        h: palette[sy][sx].h,
                        s: palette[sy][sx].s,
                        v: palette[sy][sx].v
                    };
            needSeed = false;
        }

        var ampX = Math.max(1, (width - 8) / 8) * (1 + 7 * algo.density);
        var ampY = Math.max(1, (height - 8) / 8) * (1 + 7 * algo.density);

        var amtRows = new Array(height);
        for (var yRow = 0; yRow < height; yRow++)
            amtRows[yRow] = (noiseField[yRow][0] - 0.5) * ampX;
        var amtCols = new Array(width);
        for (var xCol = 0; xCol < width; xCol++)
            amtCols[xCol] = (noiseField[0][xCol] - 0.5) * ampY;

        var afterRow = new Array(height);
        for (var yMix = 0; yMix < height; yMix++) {
            afterRow[yMix] = new Array(width);
            var amt = amtRows[yMix];
            var sgn = amt > 0 ? 1 : (amt < 0 ? -1 : 0);
            var mag = Math.abs(amt);
            var di = Math.floor(mag);
            var frac = mag - di;
            var wB = frac * frac * (3 - 2 * frac);
            var wA = 1 - wB;
            for (var xMix = 0; xMix < width; xMix++) {
                var zD = xMix + sgn * di;
                var zF = zD + sgn;
                var a, b;
                if (zD >= 0 && zD < width) a = prevPixels[yMix][zD];
                else { var cx = Math.max(0, Math.min(width - 1, zD)); a = palette[yMix][cx]; }
                if (zF >= 0 && zF < width) b = prevPixels[yMix][zF];
                else { var cx2 = Math.max(0, Math.min(width - 1, zF)); b = palette[yMix][cx2]; }
                var dh = b.h - a.h;
                if (dh > 0.5) dh -= 1;
                else if (dh < -0.5) dh += 1;
                var rh = a.h + wB * dh;
                rh = rh - Math.floor(rh);
                afterRow[yMix][xMix] = {
                    h: rh,
                    s: a.s * wA + b.s * wB,
                    v: a.v * wA + b.v * wB
                };
            }
        }

        for (var xOut = 0; xOut < width; xOut++) {
            var amtY = amtCols[xOut];
            var sgnY = amtY > 0 ? 1 : (amtY < 0 ? -1 : 0);
            var magY = Math.abs(amtY);
            var diY = Math.floor(magY);
            var fracY = magY - diY;
            var wBY = fracY * fracY * (3 - 2 * fracY);
            var wAY = 1 - wBY;
            for (var yOut = 0; yOut < height; yOut++) {
                var zDY = yOut + sgnY * diY;
                var zFY = zDY + sgnY;
                var aY, bY;
                if (zDY >= 0 && zDY < height) aY = afterRow[zDY][xOut];
                else { var cyY = Math.max(0, Math.min(height - 1, zDY)); aY = palette[cyY][xOut]; }
                if (zFY >= 0 && zFY < height) bY = afterRow[zFY][xOut];
                else { var cy2Y = Math.max(0, Math.min(height - 1, zFY)); bY = palette[cy2Y][xOut]; }
                var dhY = bY.h - aY.h;
                if (dhY > 0.5) dhY -= 1;
                else if (dhY < -0.5) dhY += 1;
                var rhY = aY.h + wBY * dhY;
                rhY = rhY - Math.floor(rhY);
                var rsY = aY.s * wAY + bY.s * wBY;
                var rvY = aY.v * wAY + bY.v * wBY;
                prevPixels[yOut][xOut] = {h: rhY, s: rsY, v: rvY};
                var i3 = (yOut * width + xOut) * 3;
                map[i3] = rhY;
                map[i3 + 1] = Math.max(0, Math.min(1, rsY));
                map[i3 + 2] = Math.max(0, Math.min(1, rvY));
            }
        }

        return map;
    }

    function renderLedFx(map, width, height, audio) {
        syncLedStateIdentity(width, height, audio);

        var dtSec = HSVUtil.audioSeconds(audio);
        var power = HSVUtil.clamp01(isFinite(powerFor(audio)) ? powerFor(audio) : 0);
        var impulse = power * 6.0;
        var audioSpeed = (algo.intensity === 0)
            ? algo.speed
            : (algo.speed * impulse * algo.intensity);

        if (dtSec > 0) {
            var move = audioSpeed * audioSpeed * 0.5 * dtSec;
            ledPhaseX += move;
            ledPhaseY += move;
            updateLedNoise(width, height, dtSec);
        }

        var gradient = gradientStops();
        var palette = new Array(height);
        for (var y = 0; y < height; y++) {
            palette[y] = new Array(width);
            for (var x = 0; x < width; x++) {
                var palIdx = ((1 - ledNoise[y][x]) * 3.0) % 1;
                if (palIdx < 0) palIdx += 1;
                palette[y][x] = HSVUtil.gradientRgbAt(gradient, palIdx);
            }
        }

        if (ledNeedSeed) {
            for (var sy = 0; sy < height; sy++) {
                for (var sx = 0; sx < width; sx++) {
                    var src = palette[sy][sx];
                    ledPrevRgb[sy][sx] = [src[0], src[1], src[2]];
                }
            }
            ledNeedSeed = false;
        }

        if (dtSec <= 0)
            return writeMapFromRgb(map, ledPrevRgb, width, height);

        var ampFactor = 1 + 7 * algo.density;
        var ampX = Math.max(1, (width - 8) / 8) * ampFactor;
        var ampY = Math.max(1, (height - 8) / 8) * ampFactor;
        var amtRows = new Array(height);
        for (var row = 0; row < height; row++)
            amtRows[row] = (ledNoise[row][0] - 0.5) * ampX;
        var amtCols = new Array(width);
        for (var col = 0; col < width; col++)
            amtCols[col] = (ledNoise[0][col] - 0.5) * ampY;

        var afterRows = smearRgb(ledPrevRgb, palette, amtRows, 1, width, height);
        var outFrame = smearRgb(afterRows, palette, amtCols, 0, width, height);
        ledPrevRgb = outFrame;
        return writeMapFromRgb(map, outFrame, width, height);
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
