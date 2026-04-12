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
    algo.acceptColors = 2;
    algo.usesAudio = true;
    algo.properties = new Array();

    algo.presetSpeed = 5;
    algo.properties.push(
      "name:presetSpeed|type:range|display:Speed|" +
      "values:1,10|write:setSpeed|read:getSpeed");
    algo.presetDensity = 5;
    algo.properties.push(
      "name:presetDensity|type:range|display:Smear|" +
      "values:1,10|write:setDensity|read:getDensity");
    algo.presetReactivity = 5;
    algo.properties.push(
      "name:presetReactivity|type:range|display:Reactivity|" +
      "values:0,10|write:setReactivity|read:getReactivity");
    algo.presetSmooth = 5;
    algo.properties.push(
      "name:presetSmooth|type:range|display:Smoothing|" +
      "values:1,10|write:setSmooth|read:getSmooth");

    algo.setSpeed = function(_v) { algo.presetSpeed = parseInt(_v); };
    algo.getSpeed = function() { return algo.presetSpeed; };
    algo.setDensity = function(_v) { algo.presetDensity = parseInt(_v); };
    algo.getDensity = function() { return algo.presetDensity; };
    algo.setReactivity = function(_v) { algo.presetReactivity = parseInt(_v); };
    algo.getReactivity = function() { return algo.presetReactivity; };
    algo.setSmooth = function(_v) { algo.presetSmooth = parseInt(_v); };
    algo.getSmooth = function() { return algo.presetSmooth; };

    var startColor = [128, 0, 255];
    var endColor = [0, 255, 200];
    var lowsFilter = null;
    var noiseField = null;
    var prevPixels = null; // persistent pixel buffer [y][x] = {r,g,b}
    var phaseX = 0, phaseY = 0;
    var lastTime = 0;
    var initialized = false;
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
        // Seed with gradient colors
        for (var y = 0; y < h; y++) {
            for (var x = 0; x < w; x++) {
                var t = ((x / Math.max(1, w - 1)) + (y / Math.max(1, h - 1))) / 2;
                prevPixels[y][x] = [
                    startColor[0] + (endColor[0] - startColor[0]) * t,
                    startColor[1] + (endColor[1] - startColor[1]) * t,
                    startColor[2] + (endColor[2] - startColor[2]) * t
                ];
            }
        }
    }

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) {
        if (rawColors && rawColors.length >= 1)
            startColor = [(rawColors[0] >> 16) & 0xFF, (rawColors[0] >> 8) & 0xFF, rawColors[0] & 0xFF];
        if (rawColors && rawColors.length >= 2)
            endColor = [(rawColors[1] >> 16) & 0xFF, (rawColors[1] >> 8) & 0xFF, rawColors[1] & 0xFF];
    };
    algo.rgbMapGetColors = function() {
        return [LedFx.rgb(startColor[0], startColor[1], startColor[2]),
                LedFx.rgb(endColor[0], endColor[1], endColor[2])];
    };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        if (!initialized) {
            lowsFilter = new LedFx.ExpFilter(0.05, 0.3);
            phaseX = Math.random() * 100;
            phaseY = Math.random() * 100;
            lastTime = Date.now();
            initialized = true;
        }
        if (lastW !== width || lastH !== height) initBuffers(width, height);

        var map = LedFx.createMap(width, height);
        if (!audio || !audio.spectrum || audio.spectrum.length === 0) return map;

        var now = Date.now();
        var dt = (now - lastTime) / 1000.0;
        lastTime = now;
        if (dt <= 0 || dt > 0.2) dt = 0.02;

        var power = lowsFilter.update(LedFx.lows_power(audio));
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

        // Generate new noise field
        var freq = 3.0;
        var newField = LedFx.noiseField2d(width, height, freq, phaseX, phaseY);

        // EMA smooth noise field
        for (var y = 0; y < height; y++)
            for (var x = 0; x < width; x++)
                noiseField[y][x] = noiseField[y][x] * smooth + newField[y][x] * (1 - smooth);

        // Compute palette colors from noise (wrap 3x like WLED)
        var palette = new Array(height);
        for (var y = 0; y < height; y++) {
            palette[y] = new Array(width);
            for (var x = 0; x < width; x++) {
                var t = ((1 - noiseField[y][x]) * 3) % 1;
                palette[y][x] = [
                    startColor[0] + (endColor[0] - startColor[0]) * t,
                    startColor[1] + (endColor[1] - startColor[1]) * t,
                    startColor[2] + (endColor[2] - startColor[2]) * t
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

                // Source position (shifted)
                var srcX = x + rowShift;
                var srcY = y + colShift;

                // Bilinear sample from prevPixels or palette for OOB
                var sx0 = Math.floor(srcX);
                var sy0 = Math.floor(srcY);
                var fx = srcX - sx0;
                var fy = srcY - sy0;

                function sample(sy, sx) {
                    if (sx >= 0 && sx < width && sy >= 0 && sy < height)
                        return prevPixels[sy][sx];
                    // OOB: use palette color
                    var cx = Math.max(0, Math.min(width - 1, sx));
                    var cy = Math.max(0, Math.min(height - 1, sy));
                    return palette[cy][cx];
                }

                var a = sample(sy0, sx0);
                var b = sample(sy0, sx0 + 1);
                var c = sample(sy0 + 1, sx0);
                var d = sample(sy0 + 1, sx0 + 1);

                // Smoothstep for easing
                var wx = fx * fx * (3 - 2 * fx);
                var wy = fy * fy * (3 - 2 * fy);

                var r = a[0] * (1-wx) * (1-wy) + b[0] * wx * (1-wy) + c[0] * (1-wx) * wy + d[0] * wx * wy;
                var g = a[1] * (1-wx) * (1-wy) + b[1] * wx * (1-wy) + c[1] * (1-wx) * wy + d[1] * wx * wy;
                var bl = a[2] * (1-wx) * (1-wy) + b[2] * wx * (1-wy) + c[2] * (1-wx) * wy + d[2] * wx * wy;

                newPixels[y][x] = [r, g, bl];
                map[y][x] = LedFx.rgb(r, g, bl);
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
