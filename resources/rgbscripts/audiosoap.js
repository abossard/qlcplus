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
    algo.presetFloor = 0;

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

    var NOISE_FREQ = 3.0;
    var noiseField = null;
    var prevPixels = null; // persistent pixel buffer [y][x] = {r,g,b}
    var phaseX = Math.random() * 100;
    var phaseY = Math.random() * 100;
    var lastW = 0, lastH = 0;

    function initBuffers(w, h) {
        prevPixels = new Array(h);
        for (var y = 0; y < h; y++) {
            prevPixels[y] = new Array(w);
            for (var x = 0; x < w; x++)
                prevPixels[y][x] = [0, 0, 0];
        }
        noiseField = new Array(h);
        for (var y = 0; y < h; y++) {
            noiseField[y] = new Array(w);
            for (var x = 0; x < w; x++)
                noiseField[y][x] = 0.5;
        }
        lastW = w;
        lastH = h;
        var seedPacked = AudioColors.bands(algo)[0];
        var seed = [(seedPacked >> 16) & 0xFF, (seedPacked >> 8) & 0xFF, seedPacked & 0xFF];
        for (var y = 0; y < h; y++) {
            for (var x = 0; x < w; x++) {
                var t = ((x / Math.max(1, w - 1)) + (y / Math.max(1, h - 1))) / 2;
                prevPixels[y][x] = [seed[0] * t, seed[1] * t, seed[2] * t];
            }
        }
    }

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() {
        return AudioColors.bands(algo).slice();
    };


    function sampleBilinear(buf, w, h, fallback, sy, sx) {
        if (sx >= 0 && sx < w && sy >= 0 && sy < h)
            return buf[sy][sx];
        var cx = Math.max(0, Math.min(w - 1, sx));
        var cy = Math.max(0, Math.min(h - 1, sy));
        return fallback[cy][cx];
    }

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        if (lastW !== width || lastH !== height) initBuffers(width, height);

        var map = RGBUtil.createMap(width, height);
        if (!audio) return map;

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

        var newField = RGBUtil.noiseField2d(width, height, NOISE_FREQ, phaseX, phaseY);

        // EMA smooth noise field
        for (var y = 0; y < height; y++)
            for (var x = 0; x < width; x++)
                noiseField[y][x] = noiseField[y][x] * smooth + newField[y][x] * (1 - smooth);

        // Compute palette colors from noise (wrap 3x like WLED)
        var blendedPacked = AudioColors.blendByPower(algo, audio);
        var blended = [(blendedPacked >> 16) & 0xFF, (blendedPacked >> 8) & 0xFF, blendedPacked & 0xFF];
        var beatBoost = 1.0 + 0.20 * audio.beat.cosPulse;
        var noveltyBoost = AudioColors.noveltyBoost(audio);
        var fluxPunch = AudioColors.fluxPunch(audio);
        var palette = new Array(height);
        for (var y = 0; y < height; y++) {
            palette[y] = new Array(width);
            for (var x = 0; x < width; x++) {
                var t = ((1 - noiseField[y][x]) * 3) % 1;
                var colorScale = 0.45 + t * 0.55;
                palette[y][x] = [
                    blended[0] * colorScale,
                    blended[1] * colorScale,
                    blended[2] * colorScale
                ];
            }
        }

        // Smear: shift pixels based on noise, blend with palette for OOB
        var ampX = Math.max(1, (width - 2) / 4) * (1 + 7 * density);
        var ampY = Math.max(1, (height - 2) / 4) * (1 + 7 * density);
        var newPixels = new Array(height);

        for (var y = 0; y < height; y++) {
            newPixels[y] = new Array(width);
            var rowShift = (noiseField[y][0] - 0.5) * ampX;

            for (var x = 0; x < width; x++) {
                var colShift = (noiseField[0][x] - 0.5) * ampY;

                var srcX = x + rowShift;
                var srcY = y + colShift;
                var sx0 = Math.floor(srcX);
                var sy0 = Math.floor(srcY);
                var fx = srcX - sx0;
                var fy = srcY - sy0;

                var a = sampleBilinear(prevPixels, width, height, palette, sy0,     sx0);
                var b = sampleBilinear(prevPixels, width, height, palette, sy0,     sx0 + 1);
                var c = sampleBilinear(prevPixels, width, height, palette, sy0 + 1, sx0);
                var d = sampleBilinear(prevPixels, width, height, palette, sy0 + 1, sx0 + 1);

                // Smoothstep for easing
                var wx = fx * fx * (3 - 2 * fx);
                var wy = fy * fy * (3 - 2 * fy);

                var r = a[0] * (1-wx) * (1-wy) + b[0] * wx * (1-wy) + c[0] * (1-wx) * wy + d[0] * wx * wy;
                var g = a[1] * (1-wx) * (1-wy) + b[1] * wx * (1-wy) + c[1] * (1-wx) * wy + d[1] * wx * wy;
                var bl = a[2] * (1-wx) * (1-wy) + b[2] * wx * (1-wy) + c[2] * (1-wx) * wy + d[2] * wx * wy;

                var maxChannel = Math.max(r, g, bl) / 255.0;
                if (maxChannel > 0) {
                    var baseFloor = algo.presetFloor/100 + (1 - algo.presetFloor/100) * Math.min(1, maxChannel);
                    var floored = Math.min(1, baseFloor * fluxPunch) * beatBoost * noveltyBoost;
                    var floorScale = floored / maxChannel;
                    r *= floorScale;
                    g *= floorScale;
                    bl *= floorScale;
                }
                newPixels[y][x] = [r, g, bl];
                map[y][x] = RGBUtil.rgb(r, g, bl);
            }
        }

        // Store for next frame (persistence)
        prevPixels = newPixels;

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
