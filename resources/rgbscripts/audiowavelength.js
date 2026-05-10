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
    algo.acceptColors = 3; // low/mid/high mel-bank gradient
    algo.usesAudio = true;
    algo.properties = new Array();

    algo.presetReactivity = 5;
    algo.presetFloor = 0;

    var DEFAULT_GRADIENT = [0x00FF00, 0x80FF00, 0xFFFF00, 0x00FFFF, 0x0000FF];
    var gradientLut = null;
    var lutWidth = -1;
    var lutSig = "";

    algo.rgbMapStepCount = function(width, height) { return 1; };

    algo.rgbMapSetColors = function(rawColors) { };

    algo.rgbMapGetColors = function() {
        return algo.gradientColors ? algo.gradientColors.slice() : DEFAULT_GRADIENT.slice();
    };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var map = RGBUtil.createMap(width, height);
        if (!audio) return map;
        var melSrc = audio.spectrum.full;
        if (!melSrc || melSrc.length === 0) return map;

        var effectiveWidth = (typeof algo.displayWidth !== 'undefined') ? algo.displayWidth : width;
        var bands = RGBUtil.interpolate(melSrc, effectiveWidth);
        for (var bi = 0; bi < bands.length; bi++)
            bands[bi] = Math.min(1, bands[bi]);

        var stops = (algo.gradientColors && algo.gradientColors.length > 0) ? algo.gradientColors : DEFAULT_GRADIENT;
        var sig = stops.length + ":" + stops.join(",");
        if (gradientLut === null || lutWidth !== width || lutSig !== sig) {
            gradientLut = RGBUtil.gradientLut(stops, width);
            lutWidth = width;
            lutSig = sig;
        }

        // Each column: spectrum magnitude → gradient color × brightness
        for (var x = 0; x < width; x++) {
            var magnitude = bands[x];
            var packed = gradientLut[x];
            var r = (packed >> 16) & 0xFF;
            var g = (packed >> 8) & 0xFF;
            var b = packed & 0xFF;

            var barHeight = Math.round(magnitude * height);

            for (var dy = 0; dy < barHeight; dy++) {
                var y = height - 1 - dy;
                if (y < 0) break;
                var baseBright = magnitude * (0.5 + 0.5 * dy / Math.max(1, barHeight));
                var bright = algo.presetFloor/100 + (1 - algo.presetFloor/100) * baseBright;
                map[y][x] = RGBUtil.rgb(r * bright, g * bright, b * bright);
            }
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
