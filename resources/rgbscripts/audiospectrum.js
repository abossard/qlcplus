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
    algo.acceptColors = 3; // low/mid/high mel-bank gradient
    algo.usesAudio = true;
    algo.properties = new Array();

    algo.presetReactivity = 7;
    algo.presetFloor = 0;

    algo.presetMode = 0;
    algo.properties.push(
      "name:presetMode|type:list|display:Color Mode|" +
      "values:Gradient,Rainbow,RGB Mix|write:setMode|read:getMode");

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

    var DEFAULT_GRADIENT = [0xFF0000, 0x0000FF];
    var PEAK_HOLD_FRAMES = 5;
    var PEAK_DECAY = 0.95;
    var DOWNBEAT_PEAK_DROP = 0.5;
    var prevBands = null;
    var peakValues = null;
    var peakHolds = null;
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
        for (var i = 0; i < bands.length; i++)
            bands[i] = Math.min(1, bands[i]);

        if (!prevBands || prevBands.length !== bands.length) prevBands = bands.slice();
        if (!peakValues || peakValues.length !== bands.length) {
            peakValues = new Array(bands.length);
            peakHolds = new Array(bands.length);
            for (var pi = 0; pi < bands.length; pi++) {
                peakValues[pi] = 0;
                peakHolds[pi] = 0;
            }
        }

        var onset = audio.onset.fired;

        if (algo.presetMode === 0) {
            var stops = (algo.gradientColors && algo.gradientColors.length > 0)
                ? algo.gradientColors
                : DEFAULT_GRADIENT;
            var sig = stops.length + ":" + stops.join(",");
            if (gradientLut === null || lutWidth !== width || lutSig !== sig) {
                gradientLut = RGBUtil.gradientLut(stops, width);
                lutWidth = width;
                lutSig = sig;
            }
        }

        for (var x = 0; x < Math.min(width, bands.length); x++) {
            var val = bands[x];
            var diff = Math.abs(bands[x] - prevBands[x]);
            var barHeight = Math.round(val * height);
            if (val > 0.01)
                barHeight = Math.max(1, barHeight);
            if (audio.bar.downbeat) {
                peakValues[x] *= DOWNBEAT_PEAK_DROP;
            }
            if (val >= peakValues[x]) {
                peakValues[x] = val;
                peakHolds[x] = PEAK_HOLD_FRAMES;
            } else if (onset && bands[x] > 0.05) {
                peakValues[x] = Math.max(peakValues[x], val);
                peakHolds[x] = PEAK_HOLD_FRAMES;
            } else if (peakHolds[x] > 0) {
                peakHolds[x]--;
            } else {
                peakValues[x] *= PEAK_DECAY;
            }

            for (var dy = 0; dy < barHeight; dy++) {
                var y = height - 1 - dy;
                if (y < 0) break;

                var r, g, b;
                if (algo.presetMode === 1) {
                    var t = x / Math.max(1, width - 1);
                    var c = RGBUtil.hsv2rgb(t, 1, 1);
                    r = c[0]; g = c[1]; b = c[2];
                } else if (algo.presetMode === 2) {
                    // RGB Mix: R=value, G=frame-to-frame diff, B=value (lower gain)
                    r = Math.min(255, val * 1000);
                    g = Math.min(255, diff * 2000);
                    b = Math.min(255, val * 800);
                } else {
                    var packed = gradientLut[x];
                    r = (packed >> 16) & 0xFF;
                    g = (packed >> 8) & 0xFF;
                    b = packed & 0xFF;
                }
                var baseBright = (dy / height) * 0.5 + 0.5;
                var bright = algo.presetFloor/100 + (1 - algo.presetFloor/100) * baseBright;
                map[y][x] = RGBUtil.rgb(r * bright, g * bright, b * bright);
            }
            if (peakValues[x] > 0.01) {
                var peakY = height - 1 - Math.floor(peakValues[x] * height);
                if (peakY >= 0 && peakY < height)
                    map[peakY][x] = RGBUtil.rgb(255, 255, 255);
            }
        }
        prevBands = bands.slice();
        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
