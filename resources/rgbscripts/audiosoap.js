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

    AudioParams.installContinuous(algo, {gain: 5, reactivity: 5});

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

    var DEFAULT_BAND_COLORS = [0x8000FF, 0x4066E0, 0x00FFC8];
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
        var seed = AudioParams.colorChannels(AudioParams.bandColors(algo, DEFAULT_BAND_COLORS)[0]);
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
        return AudioParams.bandColors(algo, DEFAULT_BAND_COLORS).slice();
    };


    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        if (!initialized) {
            lowsFilter = new AudioDSP.Filter(0.05, AudioParams.filterRise(algo));
            phaseX = Math.random() * 100;
            phaseY = Math.random() * 100;
            lastTime = Date.now();
            initialized = true;
        }
        if (lastW !== width || lastH !== height) initBuffers(width, height);

        var map = RGBUtil.createMap(width, height);
        if (!audio) return map;

        var now = Date.now();
        var dt = (now - lastTime) / 1000.0;
        lastTime = now;
        if (dt <= 0 || dt > 0.2) dt = 0.02;

        var power = lowsFilter.update(audio.lows);
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
        var newField = RGBUtil.noiseField2d(width, height, freq, phaseX, phaseY);

        // EMA smooth noise field
        for (var y = 0; y < height; y++)
            for (var x = 0; x < width; x++)
                noiseField[y][x] = noiseField[y][x] * smooth + newField[y][x] * (1 - smooth);

        // Compute palette colors from noise (wrap 3x like WLED)
        var blended = AudioParams.colorChannels(AudioParams.blendBandColors(algo, audio, DEFAULT_BAND_COLORS));
        var beatBoost = 1.0 + 0.20 * AudioParams.beatPulse(audio);
        var noveltyBoost = 1.0 + 0.30 * AudioParams.melNoveltyAvg(audio);
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

                var maxChannel = Math.max(r, g, bl) / 255.0;
                if (maxChannel > 0) {
                    var floored = AudioParams.applyPunch(AudioParams.applyFloor(algo, Math.min(1, maxChannel)), audio) * beatBoost * noveltyBoost;
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
