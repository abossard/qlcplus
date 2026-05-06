/*
  Q Light Controller Plus
  audiowavelength.js

  Copyright (c) QLC+ contributors
  Ported from LedFX "Wavelength" effect (MIT License)

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
    algo.name = "Audio Wavelength";
    algo.author = "Ported from LedFx";
    algo.acceptColors = 2;
    algo.usesAudio = true;
    algo.properties = new Array();

    AudioParams.installContinuous(algo, {gain: 5, reactivity: 5});

    algo.presetSmoothing = 7;
    algo.properties.push(
      "name:presetSmoothing|type:range|display:Smoothing|" +
      "values:1,10|write:setSmoothing|read:getSmoothing");

    algo.setSmoothing = function(_v) { algo.presetSmoothing = parseInt(_v); };
    algo.getSmoothing = function() { return algo.presetSmoothing; };

    var startColor = [0, 255, 0];
    var endColor = [0, 0, 255];
    var filter = null;
    var initialized = false;

    algo.rgbMapStepCount = function(width, height) { return 1; };

    algo.rgbMapSetColors = function(rawColors) {
        if (rawColors && rawColors.length >= 1)
            startColor = [(rawColors[0] >> 16) & 0xFF, (rawColors[0] >> 8) & 0xFF, rawColors[0] & 0xFF];
        if (rawColors && rawColors.length >= 2)
            endColor = [(rawColors[1] >> 16) & 0xFF, (rawColors[1] >> 8) & 0xFF, rawColors[1] & 0xFF];
    };

    algo.rgbMapGetColors = function() {
        return [RGBUtil.rgb(startColor[0], startColor[1], startColor[2]),
                RGBUtil.rgb(endColor[0], endColor[1], endColor[2])];
    };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var pixelCount = width * height;
        if (!initialized || !filter) {
            var decay = algo.presetSmoothing / 15.0;
            filter = new AudioDSP.Filter(decay, AudioParams.filterRise(algo));
            initialized = true;
        }

        var map = RGBUtil.createMap(width, height);
        if (!audio || !audio.mel || audio.mel.length === 0) return map;

        // Get spectrum interpolated to physical display width
        var effectiveWidth = (typeof algo.displayWidth !== 'undefined') ? algo.displayWidth : width;
        var bands = RGBUtil.interpolate(audio.mel, effectiveWidth);
        for (var bi = 0; bi < bands.length; bi++)
            bands[bi] = Math.min(1, bands[bi]);
        var smoothed = filter.updateArray(bands);

        // Map each column: spectrum magnitude → gradient color × brightness
        for (var x = 0; x < width; x++) {
            var magnitude = Math.max(0, Math.min(1, smoothed[x]));
            var t = x / Math.max(1, width - 1);

            // Gradient color based on position
            var r = startColor[0] + (endColor[0] - startColor[0]) * t;
            var g = startColor[1] + (endColor[1] - startColor[1]) * t;
            var b = startColor[2] + (endColor[2] - startColor[2]) * t;

            // Height of the bar based on magnitude
            var barHeight = Math.round(magnitude * height);

            for (var dy = 0; dy < barHeight; dy++) {
                var y = height - 1 - dy;
                if (y < 0) break;
                // Fade brightness toward top
                var bright = AudioParams.applyFloor(algo, magnitude * (0.5 + 0.5 * dy / Math.max(1, barHeight)));
                map[y][x] = RGBUtil.rgb(r * bright, g * bright, b * bright);
            }
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
