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
    algo.acceptColors = 3; // low/mid/high mel-bank gradient
    algo.usesAudio = true;
    algo.properties = new Array();

    algo.presetReactivity = 5;
    algo.properties.push(
      "name:presetReactivity|type:range|display:Reactivity|" +
      "values:1,10|write:setReactivity|read:getReactivity");
    algo.presetSpeed = 5;
    algo.properties.push(
      "name:presetSpeed|type:range|display:Speed|" +
      "values:1,10|write:setSpeed|read:getSpeed");
    algo.presetDensity = 5;
    algo.properties.push(
      "name:presetDensity|type:range|display:Smear|" +
      "values:1,10|write:setDensity|read:getDensity");
    algo.presetSmooth = 5;
    algo.properties.push(
      "name:presetSmooth|type:range|display:Smoothing|" +
      "values:1,10|write:setSmooth|read:getSmooth");

    algo.setSpeed = function(_v) { algo.presetSpeed = parseInt(_v); };
    algo.getSpeed = function() { return algo.presetSpeed; };
    algo.setDensity = function(_v) { algo.presetDensity = parseInt(_v); };
    algo.getDensity = function() { return algo.presetDensity; };
    algo.setSmooth = function(_v) { algo.presetSmooth = parseInt(_v); };
    algo.getSmooth = function() { return algo.presetSmooth; };

    algo.setReactivity = function(_v) { algo.presetReactivity = parseInt(_v); };
    algo.getReactivity = function() { return algo.presetReactivity; };
    var NOISE_FREQ = 3.0;
    var MAX_NOISE_GRID = 8;
    var MAX_SOAP_PIXELS = 2048;
    var phaseX = Math.random() * 100;
    var phaseY = Math.random() * 100;
    var lastW = 0, lastH = 0;
    algo.noiseField = null;
    algo.coarseNoise = null;
    algo.prevPixels = null; // persistent pixel buffer [y][x] = [r,g,b]
    algo.newPixels = null;

    function initBuffers(w, h) {
        algo.prevPixels = new Array(h);
        algo.newPixels = new Array(h);
        algo.noiseField = new Array(h);
        for (var y = 0; y < h; y++) {
            algo.prevPixels[y] = new Array(w);
            algo.newPixels[y] = new Array(w);
            algo.noiseField[y] = new Array(w);
            for (var x = 0; x < w; x++) {
                algo.prevPixels[y][x] = [0, 0, 0];
                algo.newPixels[y][x] = [0, 0, 0];
                algo.noiseField[y][x] = 0.5;
            }
        }
        var gridW = Math.min(MAX_NOISE_GRID, w);
        var gridH = Math.min(MAX_NOISE_GRID, h);
        algo.coarseNoise = new Array(gridH);
        for (var gy = 0; gy < gridH; gy++) {
            algo.coarseNoise[gy] = new Array(gridW);
            for (var gx = 0; gx < gridW; gx++)
                algo.coarseNoise[gy][gx] = 0.5;
        }
        algo.coarseNoiseWidth = gridW;
        algo.coarseNoiseHeight = gridH;
        lastW = w;
        lastH = h;
        var seedPacked = AudioColors.bands(algo)[0];
        var seedR = (seedPacked >> 16) & 0xFF;
        var seedG = (seedPacked >> 8) & 0xFF;
        var seedB = seedPacked & 0xFF;
        for (var y = 0; y < h; y++) {
            for (var x = 0; x < w; x++) {
                var t = ((x / Math.max(1, w - 1)) + (y / Math.max(1, h - 1))) / 2;
                algo.prevPixels[y][x][0] = seedR * t;
                algo.prevPixels[y][x][1] = seedG * t;
                algo.prevPixels[y][x][2] = seedB * t;
            }
        }
    }

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() {
        return AudioColors.bands(algo).slice();
    };

    function fillNoiseField(w, h, smooth) {
        var coarseNoise = algo.coarseNoise;
        var noiseField = algo.noiseField;
        var gridW = algo.coarseNoiseWidth;
        var gridH = algo.coarseNoiseHeight;
        var invGridW = 1 / Math.max(1, gridW - 1);
        var invGridH = 1 / Math.max(1, gridH - 1);

        for (var gy = 0; gy < gridH; gy++) {
            for (var gx = 0; gx < gridW; gx++) {
                var n = RGBUtil.simplex2d(
                    gx * invGridW * NOISE_FREQ + phaseX,
                    gy * invGridH * NOISE_FREQ + phaseY
                );
                coarseNoise[gy][gx] = (n + 1) * 0.5;
            }
        }

        var fullScaleX = Math.max(1, gridW - 1) / Math.max(1, w - 1);
        var fullScaleY = Math.max(1, gridH - 1) / Math.max(1, h - 1);
        var newMix = 1 - smooth;
        for (var y = 0; y < h; y++) {
            var gyf = y * fullScaleY;
            var gy0 = Math.floor(gyf);
            var gy1 = Math.min(gridH - 1, gy0 + 1);
            var fy = gyf - gy0;
            var wy = fy * fy * (3 - 2 * fy);
            var row0 = coarseNoise[gy0];
            var row1 = coarseNoise[gy1];
            var dstRow = noiseField[y];
            for (var x = 0; x < w; x++) {
                var gxf = x * fullScaleX;
                var gx0 = Math.floor(gxf);
                var gx1 = Math.min(gridW - 1, gx0 + 1);
                var fx = gxf - gx0;
                var wx = fx * fx * (3 - 2 * fx);
                var top = row0[gx0] * (1 - wx) + row0[gx1] * wx;
                var bottom = row1[gx0] * (1 - wx) + row1[gx1] * wx;
                var n = top * (1 - wy) + bottom * wy;
                dstRow[x] = dstRow[x] * smooth + n * newMix;
            }
        }
    }

    function colorScaleForNoise(noise) {
        var t = ((1 - noise) * 3) % 1;
        return 0.45 + t * 0.55;
    }

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var map = RGBUtil.createMap(width, height);
        if (!audio) return map;
        if (width * height > MAX_SOAP_PIXELS) {
            var fallbackColor = AudioColors.blendByPower(algo, audio);
            for (var fy = 0; fy < height; fy++)
                for (var fx = 0; fx < width; fx++)
                    map[fy][fx] = fallbackColor;
            return map;
        }

        if (lastW !== width || lastH !== height) initBuffers(width, height);

        var dt = audio.timing.consumerDtMs / 1000.0;

        var power = audio.power.low;
        var speed = algo.presetSpeed / 10.0;
        var reactivity = algo.presetReactivity / 10.0;
        var smooth = algo.presetSmooth / 10.0;
        var density = algo.presetDensity / 10.0;

        // Audio-modulated speed
        var audioSpeed = (reactivity === 0)
            ? speed
            : speed * (1 + power * reactivity * 6);
        var move = audioSpeed * audioSpeed * 0.5 * dt;
        phaseX += move;
        phaseY += move * 0.7;

        fillNoiseField(width, height, smooth);

        // Compute palette color inline from noise (wrap 3x like WLED)
        var blendedPacked = AudioColors.blendByPower(algo, audio);
        var blendedR = (blendedPacked >> 16) & 0xFF;
        var blendedG = (blendedPacked >> 8) & 0xFF;
        var blendedB = blendedPacked & 0xFF;
        var beatBoost = 1.0 + 0.20 * audio.beat.cosPulse;
        var noveltyBoost = AudioColors.noveltyBoost(audio);
        var fluxPunch = AudioColors.fluxPunch(audio);
        var noiseField = algo.noiseField;
        var prevPixels = algo.prevPixels;
        var newPixels = algo.newPixels;

        // Smear: shift pixels based on noise, blend with palette for OOB
        var ampX = Math.max(1, (width - 2) / 4) * (1 + 7 * density);
        var ampY = Math.max(1, (height - 2) / 4) * (1 + 7 * density);

        for (var y = 0; y < height; y++) {
            var rowShift = (noiseField[y][0] - 0.5) * ampX;

            for (var x = 0; x < width; x++) {
                var colShift = (noiseField[0][x] - 0.5) * ampY;

                var srcX = x + rowShift;
                var srcY = y + colShift;
                var sx0 = Math.floor(srcX);
                var sy0 = Math.floor(srcY);
                var fx = srcX - sx0;
                var fy = srcY - sy0;

                var ar, ag, ab, br, bg, bb, cr, cg, cb, dr, dg, db;
                var ax = sx0, ay = sy0;
                if (ax >= 0 && ax < width && ay >= 0 && ay < height) {
                    var a = prevPixels[ay][ax];
                    ar = a[0]; ag = a[1]; ab = a[2];
                } else {
                    ax = Math.max(0, Math.min(width - 1, ax));
                    ay = Math.max(0, Math.min(height - 1, ay));
                    var as = colorScaleForNoise(noiseField[ay][ax]);
                    ar = blendedR * as; ag = blendedG * as; ab = blendedB * as;
                }
                var bx = sx0 + 1, by = sy0;
                if (bx >= 0 && bx < width && by >= 0 && by < height) {
                    var b = prevPixels[by][bx];
                    br = b[0]; bg = b[1]; bb = b[2];
                } else {
                    bx = Math.max(0, Math.min(width - 1, bx));
                    by = Math.max(0, Math.min(height - 1, by));
                    var bs = colorScaleForNoise(noiseField[by][bx]);
                    br = blendedR * bs; bg = blendedG * bs; bb = blendedB * bs;
                }
                var cx = sx0, cy = sy0 + 1;
                if (cx >= 0 && cx < width && cy >= 0 && cy < height) {
                    var c = prevPixels[cy][cx];
                    cr = c[0]; cg = c[1]; cb = c[2];
                } else {
                    cx = Math.max(0, Math.min(width - 1, cx));
                    cy = Math.max(0, Math.min(height - 1, cy));
                    var cs = colorScaleForNoise(noiseField[cy][cx]);
                    cr = blendedR * cs; cg = blendedG * cs; cb = blendedB * cs;
                }
                var dx = sx0 + 1, dy = sy0 + 1;
                if (dx >= 0 && dx < width && dy >= 0 && dy < height) {
                    var d = prevPixels[dy][dx];
                    dr = d[0]; dg = d[1]; db = d[2];
                } else {
                    dx = Math.max(0, Math.min(width - 1, dx));
                    dy = Math.max(0, Math.min(height - 1, dy));
                    var ds = colorScaleForNoise(noiseField[dy][dx]);
                    dr = blendedR * ds; dg = blendedG * ds; db = blendedB * ds;
                }

                // Smoothstep for easing
                var wx = fx * fx * (3 - 2 * fx);
                var wy = fy * fy * (3 - 2 * fy);

                var invWx = 1 - wx;
                var invWy = 1 - wy;
                var r = ar * invWx * invWy + br * wx * invWy + cr * invWx * wy + dr * wx * wy;
                var g = ag * invWx * invWy + bg * wx * invWy + cg * invWx * wy + dg * wx * wy;
                var bl = ab * invWx * invWy + bb * wx * invWy + cb * invWx * wy + db * wx * wy;

                var maxChannel = Math.max(r, g, bl) / 255.0;
                if (maxChannel > 0) {
                    var baseFloor = Math.min(1, maxChannel);
                    var floored = Math.min(1, baseFloor * fluxPunch) * beatBoost * noveltyBoost;
                    var floorScale = floored / maxChannel;
                    r *= floorScale;
                    g *= floorScale;
                    bl *= floorScale;
                }
                var out = newPixels[y][x];
                out[0] = r;
                out[1] = g;
                out[2] = bl;
                map[y][x] = RGBUtil.rgb(r, g, bl);
            }
        }

        // Store for next frame (persistence)
        algo.prevPixels = newPixels;
        algo.newPixels = prevPixels;

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
