/*
  Q Light Controller Plus
  audiopower.js

  Copyright (c) QLC+ contributors
  Ported from LedFX "Power" effect (MIT License)

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
    algo.name = "Audio Power";
    algo.author = "Ported from LedFx";
    algo.acceptColors = 2;
    algo.usesAudio = true;
    algo.properties = new Array();

    algo.presetSparks = 0;
    algo.properties.push(
      "name:presetSparks|type:list|display:Beat Sparks|" +
      "values:Off,On|write:setSparks|read:getSparks");

    algo.presetMirror = 0;
    algo.properties.push(
      "name:presetMirror|type:list|display:Mirror|" +
      "values:Off,On|write:setMirror|read:getMirror");

    algo.setSparks = function(_v) { algo.presetSparks = (_v === "On") ? 1 : 0; };
    algo.getSparks = function() { return algo.presetSparks ? "On" : "Off"; };
    algo.setMirror = function(_v) { algo.presetMirror = (_v === "On") ? 1 : 0; };
    algo.getMirror = function() { return algo.presetMirror ? "On" : "Off"; };

    var startColor = [255, 0, 0];
    var endColor = [0, 0, 255];
    var bassFilter = null;
    var sparksPixels = null;
    var initialized = false;

    function init(width) {
        bassFilter = new LedFx.ExpFilter(0.1, 0.8);
        sparksPixels = new Array(width);
        for (var i = 0; i < width; i++) sparksPixels[i] = 0;
        initialized = true;
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
        if (!initialized || !sparksPixels || sparksPixels.length !== width) init(width);
        var map = LedFx.createMap(width, height);
        if (!audio || !audio.spectrum || audio.spectrum.length === 0) return map;

        // Get spectrum and bass power
        var bands = LedFx.melbank(audio, width);
        var bass = bassFilter.update(LedFx.lows_power(audio));

        // Bass overlay: fill from edge based on bass power
        var bassIdx = Math.min(width, Math.floor(bass * width * 1.5));

        // Sparks: random pixels on beat
        if (algo.presetSparks && audio.beat) {
            var sparkCount = Math.max(1, Math.floor(width / 15));
            for (var s = 0; s < sparkCount; s++) {
                var sx = Math.floor(Math.random() * width);
                sparksPixels[sx] = 1.0;
            }
        }
        // Decay sparks
        for (var i = 0; i < width; i++)
            sparksPixels[i] *= 0.85;

        for (var y = 0; y < height; y++) {
            for (var x = 0; x < width; x++) {
                var pos = algo.presetMirror
                    ? Math.abs(x - Math.floor(width / 2)) * 2
                    : x;

                // Gradient color based on position
                var t = pos / Math.max(1, width - 1);
                var r = startColor[0] + (endColor[0] - startColor[0]) * t;
                var g = startColor[1] + (endColor[1] - startColor[1]) * t;
                var b = startColor[2] + (endColor[2] - startColor[2]) * t;

                // Brightness from spectrum
                var specBright = Math.min(1, bands[x % bands.length] * 3);

                // Bass overlay brightness
                var bassBright = (pos < bassIdx) ? bass : 0;

                // Combine
                var bright = Math.max(specBright, bassBright);

                // Add sparks (white flash)
                if (sparksPixels[x] > 0.1) {
                    var sparkVal = sparksPixels[x];
                    r = r * (1 - sparkVal) + 255 * sparkVal;
                    g = g * (1 - sparkVal) + 255 * sparkVal;
                    b = b * (1 - sparkVal) + 255 * sparkVal;
                    bright = Math.max(bright, sparkVal);
                }

                map[y][x] = LedFx.rgb(r * bright, g * bright, b * bright);
            }
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
