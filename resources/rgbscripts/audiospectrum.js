/*
  Q Light Controller Plus
  audiospectrum.js

  Copyright (c) QLC+ contributors
  Ported from LedFX "Spectrum" effect (MIT License)

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
    algo.name = "Audio Spectrum";
    algo.author = "Ported from LedFx";
    algo.acceptColors = 2;
    algo.usesAudio = true;
    algo.properties = new Array();

    algo.presetMode = 0;
    algo.properties.push(
      "name:presetMode|type:list|display:Color Mode|" +
      "values:Gradient,Rainbow,RGB Mix|write:setMode|read:getMode");

    algo.presetSensitivity = 5;
    algo.properties.push(
      "name:presetSensitivity|type:range|display:Sensitivity|" +
      "values:1,10|write:setSensitivity|read:getSensitivity");

    algo.presetSmoothing = 5;
    algo.properties.push(
      "name:presetSmoothing|type:range|display:Smoothing|" +
      "values:1,10|write:setSmoothing|read:getSmoothing");

    algo.setMode = function(_v) {
        if (_v === "Rainbow") algo.presetMode = 1;
        else if (_v === "RGB Mix") algo.presetMode = 2;
        else algo.presetMode = 0;
    };
    algo.getMode = function() {
        if (algo.presetMode === 1) return "Rainbow";
        if (algo.presetMode === 2) return "RGB Mix";
        return "Gradient";
    };
    algo.setSensitivity = function(_v) { algo.presetSensitivity = parseInt(_v); };
    algo.getSensitivity = function() { return algo.presetSensitivity; };
    algo.setSmoothing = function(_v) { algo.presetSmoothing = parseInt(_v); filterDirty = true; };
    algo.getSmoothing = function() { return algo.presetSmoothing; };

    var startColor = [255, 0, 0];
    var endColor = [0, 0, 255];
    var filter = null;
    var prevBands = null;
    var initialized = false;
    var filterDirty = false;

    function init() {
        var decay = algo.presetSmoothing / 15.0;
        filter = new LedFx.ExpFilter(decay, 0.5);
        initialized = true;
        filterDirty = false;
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
        if (!initialized || filterDirty) init();
        var map = LedFx.createMap(width, height);
        if (!audio || !audio.spectrum || audio.spectrum.length === 0) return map;

        var bands = LedFx.melbank(audio, width);
        var scale = algo.presetSensitivity / 5.0;
        for (var i = 0; i < bands.length; i++)
            bands[i] = Math.min(1, bands[i] * scale);

        var filtered = filter.updateArray(bands);
        if (!prevBands) prevBands = bands.slice();

        for (var x = 0; x < width; x++) {
            var val = Math.min(1, filtered[x]);
            var diff = Math.abs(bands[x] - prevBands[x]);
            var barHeight = Math.round(val * height);
            var t = x / Math.max(1, width - 1);

            for (var dy = 0; dy < barHeight; dy++) {
                var y = height - 1 - dy;
                if (y < 0) break;

                var r, g, b;
                if (algo.presetMode === 1) {
                    // Rainbow: hue based on column position
                    var c = LedFx.hsv2rgb(t, 1, 1);
                    r = c[0]; g = c[1]; b = c[2];
                } else if (algo.presetMode === 2) {
                    // RGB Mix: R=filtered, G=diff, B=smoothed
                    r = Math.min(255, val * 1000);
                    g = Math.min(255, diff * 2000);
                    b = Math.min(255, filtered[x] * 800);
                } else {
                    // Gradient between start and end color
                    r = startColor[0] + (endColor[0] - startColor[0]) * t;
                    g = startColor[1] + (endColor[1] - startColor[1]) * t;
                    b = startColor[2] + (endColor[2] - startColor[2]) * t;
                }
                // Brightness from height position
                var bright = (dy / height) * 0.5 + 0.5;
                map[y][x] = LedFx.rgb(r * bright, g * bright, b * bright);
            }
        }
        prevBands = bands.slice();
        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
