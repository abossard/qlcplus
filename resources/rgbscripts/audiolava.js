/*
  Q Light Controller Plus
  audiolava.js

  Copyright (c) QLC+ contributors
  Ported from LedFX "Lava lamp" effect (MIT License)

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
    algo.name = "Audio Lava Lamp";
    algo.author = "Ported from LedFx";
    algo.acceptColors = 2;
    algo.usesAudio = true;
    algo.properties = new Array();

    algo.presetSpeed = 7;
    algo.properties.push(
      "name:presetSpeed|type:range|display:Speed|" +
      "values:1,15|write:setSpeed|read:getSpeed");
    algo.presetContrast = 6;
    algo.properties.push(
      "name:presetContrast|type:range|display:Contrast|" +
      "values:0,10|write:setContrast|read:getContrast");
    algo.presetReactivity = 3;
    algo.properties.push(
      "name:presetReactivity|type:range|display:Reactivity|" +
      "values:1,9|write:setReactivity|read:getReactivity");

    algo.setSpeed = function(_v) { algo.presetSpeed = parseInt(_v); };
    algo.getSpeed = function() { return algo.presetSpeed; };
    algo.setContrast = function(_v) { algo.presetContrast = parseInt(_v); };
    algo.getContrast = function() { return algo.presetContrast; };
    algo.setReactivity = function(_v) { algo.presetReactivity = parseInt(_v); };
    algo.getReactivity = function() { return algo.presetReactivity; };

    var startColor = [255, 0, 128];
    var endColor = [0, 128, 255];
    var lowsFilter = null;
    var lowsPower = 0;
    var elapsedMs = 0;
    var lastTime = 0;
    var initialized = false;

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
            lowsFilter = new LedFx.ExpFilter(0.05, algo.presetReactivity / 10.0);
            lastTime = Date.now();
            initialized = true;
        }

        var map = LedFx.createMap(width, height);
        if (!audio || !audio.spectrum || audio.spectrum.length === 0) return map;

        var now = Date.now();
        var dt = (now - lastTime) / 1000.0;
        lastTime = now;
        if (dt <= 0 || dt > 0.2) dt = 0.02;
        elapsedMs += dt * 1000;

        lowsPower = lowsFilter.update(LedFx.lows_power(audio));
        var speed = algo.presetSpeed;
        var contrast = 1 - algo.presetContrast / 10.0;

        // Time phases
        var t1 = (elapsedMs * speed * 0.0001 * Math.max(1, 1 + lowsPower * 0.004)) % 1;
        var t2 = (elapsedMs * speed * 0.0002 * Math.max(1, 1 + lowsPower * 0.007)) % 1;

        var pixelCount = width;

        for (var x = 0; x < width; x++) {
            var il = x / Math.max(1, pixelCount - 1);

            // Lava lamp sine wave blending
            var w1 = Math.sin((t1 + il) * Math.PI * 2);
            var w2 = Math.sin((t2 - il) * Math.PI * 2);
            var w3 = Math.sin((il + w1 + w2) * Math.PI * 2);

            // Hue from position + time
            var hue = (t1 + il) % 1;

            // Combine waves for brightness
            var bright = (w1 + 0.1) * (w2 + lowsPower * 0.7) * (w3 + lowsPower * 0.9);
            bright = Math.pow(Math.max(0, bright + contrast), 2);
            bright = Math.min(1, bright);

            // Interpolate between start and end color based on hue
            var r = startColor[0] + (endColor[0] - startColor[0]) * hue;
            var g = startColor[1] + (endColor[1] - startColor[1]) * hue;
            var b = startColor[2] + (endColor[2] - startColor[2]) * hue;

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
