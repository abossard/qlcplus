/*
  Q Light Controller Plus
  audiowater.js

  Copyright (c) QLC+ contributors
  Ported from LedFX "Water" effect (MIT License)

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
    algo.acceptColors = 2;
    algo.usesAudio = true;
    algo.properties = new Array();

    AudioParams.installContinuous(algo, {gain: 5, reactivity: 5});

    algo.presetSpeed = 5;
    algo.properties.push(
      "name:presetSpeed|type:range|display:Speed|" +
      "values:1,10|write:setSpeed|read:getSpeed");
    algo.presetViscosity = 6;
    algo.properties.push(
      "name:presetViscosity|type:range|display:Viscosity|" +
      "values:2,12|write:setViscosity|read:getViscosity");
    algo.presetBassSize = 8;
    algo.properties.push(
      "name:presetBassSize|type:range|display:Bass Ripple Size|" +
      "values:1,15|write:setBassSize|read:getBassSize");
    algo.presetHighSize = 3;
    algo.properties.push(
      "name:presetHighSize|type:range|display:High Ripple Size|" +
      "values:1,15|write:setHighSize|read:getHighSize");

    algo.setSpeed = function(_v) { algo.presetSpeed = parseInt(_v); };
    algo.getSpeed = function() { return algo.presetSpeed; };
    algo.setViscosity = function(_v) { algo.presetViscosity = parseInt(_v); };
    algo.getViscosity = function() { return algo.presetViscosity; };
    algo.setBassSize = function(_v) { algo.presetBassSize = parseInt(_v); };
    algo.getBassSize = function() { return algo.presetBassSize; };
    algo.setHighSize = function(_v) { algo.presetHighSize = parseInt(_v); };
    algo.getHighSize = function() { return algo.presetHighSize; };

    var startColor = [0, 64, 255];
    var endColor = [0, 255, 200];
    var buf0 = null;
    var buf1 = null;
    var curBuf = 0;
    var initialized = false;

    function init(w) {
        buf0 = new Array(w); buf1 = new Array(w);
        for (var i = 0; i < w; i++) { buf0[i] = 0; buf1[i] = 0; }
        curBuf = 0;
        initialized = true;
    }

    function createDrop(pos, h, w) {
        if (pos < 1 || pos >= w - 1) return;
        buf0[pos] = buf0[pos-1] = buf0[pos+1] = h;
        buf1[pos] = buf1[pos-1] = buf1[pos+1] = h;
    }

    function doRipple(dampFactor, w) {
        var src = (curBuf === 0) ? buf1 : buf0;
        var dst = (curBuf === 0) ? buf0 : buf1;
        for (var i = 1; i < w - 1; i++) {
            dst[i] = ((src[i-1] + src[i+1] + src[i] * 2) / 2) - dst[i];
            dst[i] -= dst[i] / dampFactor;
        }
        curBuf = 1 - curBuf;
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
        if (!initialized || !buf0 || buf0.length !== width) init(width);
        var map = LedFx.createMap(width, height);
        if (!audio || !audio.spectrum || audio.spectrum.length === 0) return map;

        var dampFactor = Math.pow(2, algo.presetViscosity);
        var gain = AudioParams.gainFactor(algo);
        var thirds = LedFx.melbank_thirds(audio);
        var bassIntensity = Math.min(1, Math.pow(LedFx.avg(thirds.lows) * gain, 2));
        var midsIntensity = Math.min(1, Math.pow(LedFx.avg(thirds.mids) * gain, 2));
        var highIntensity = Math.min(1, Math.pow(LedFx.avg(thirds.highs) * gain, 2));

        // Create drops based on audio
        createDrop(1, bassIntensity * algo.presetBassSize, width);
        createDrop(Math.floor(width / 2), bassIntensity * algo.presetBassSize, width);
        createDrop(width - 2, bassIntensity * algo.presetBassSize, width);

        // Mids drops at moving positions
        var midPos = Math.floor((Date.now() * 0.0002 * algo.presetSpeed) % 1 * (width - 2)) + 1;
        createDrop(midPos, midsIntensity * 6, width);

        // Highs drops at multiple positions
        var highPos1 = Math.floor((Date.now() * 0.0003 * algo.presetSpeed) % 1 * (width - 2)) + 1;
        var highPos2 = Math.floor((Date.now() * -0.00025 * algo.presetSpeed + 0.5) % 1 * (width - 2)) + 1;
        if (highPos2 < 1) highPos2 = 1;
        createDrop(highPos1, highIntensity * algo.presetHighSize, width);
        createDrop(highPos2, highIntensity * algo.presetHighSize, width);

        // Run ripple simulation
        var speedSteps = Math.max(1, Math.floor(algo.presetSpeed / 3));
        for (var s = 0; s < speedSteps; s++)
            doRipple(dampFactor, width);

        // Render: map water height to colors
        var current = (curBuf === 0) ? buf0 : buf1;
        for (var x = 0; x < width; x++) {
            var val = current[x];
            // Triangle wave for hue variation
            var hue = Math.abs((val * 2) % 2 - 1);
            // Brightness from water height
            var bright = AudioParams.applyFloor(algo, Math.min(1, Math.max(0, val * 0.8 + 0.12)));

            var r = startColor[0] + (endColor[0] - startColor[0]) * hue;
            var g = startColor[1] + (endColor[1] - startColor[1]) * hue;
            var b = startColor[2] + (endColor[2] - startColor[2]) * hue;

            // Saturation reduction for bright peaks (whitewash effect)
            if (bright > 0.8) {
                var whiteMix = (bright - 0.8) * 5;
                r = r + (255 - r) * whiteMix;
                g = g + (255 - g) * whiteMix;
                b = b + (255 - b) * whiteMix;
            }

            var packed = LedFx.rgb(r * bright, g * bright, b * bright);
            for (var y = 0; y < height; y++)
                map[y][x] = packed;
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
