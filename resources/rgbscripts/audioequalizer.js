/*
  Q Light Controller Plus
  audioequalizer.js

  Copyright (c) QLC+ contributors
  Inspired by LedFX "Equalizer2d" effect (MIT License)
  Original by LedFX contributors: https://github.com/LedFx/LedFx

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
    algo.name = "Audio Equalizer";
    algo.author = "Ported from LedFx";
    algo.acceptColors = 3; // low/mid/high mel-bank gradient
    algo.usesAudio = true;
    algo.properties = new Array();

    algo.presetReactivity = 5;
    algo.presetFloor = 0;

    // --- Properties ---
    algo.presetDecay = 5;
    algo.properties.push(
      "name:presetDecay|type:range|display:Decay|" +
      "values:1,10|write:setDecay|read:getDecay");

    algo.presetPeaks = 0;
    algo.properties.push(
      "name:presetPeaks|type:list|display:Peak Markers|" +
      "values:Off,On|write:setPeaks|read:getPeaks");

    algo.presetCenter = 0;
    algo.properties.push(
      "name:presetCenter|type:list|display:Centered|" +
      "values:Off,On|write:setCenter|read:getCenter");

    algo.presetGap = 0;
    algo.properties.push(
      "name:presetGap|type:list|display:Bar Gap|" +
      "values:Off,On|write:setGap|read:getGap");


    algo.setDecay = function(_v) { algo.presetDecay = parseInt(_v); };
    algo.getDecay = function()  { return algo.presetDecay; };
    algo.setPeaks = function(_v) { algo.presetPeaks = (_v === "On") ? 1 : 0; };
    algo.getPeaks = function()  { return algo.presetPeaks ? "On" : "Off"; };
    algo.setCenter = function(_v) { algo.presetCenter = (_v === "On") ? 1 : 0; };
    algo.getCenter = function()  { return algo.presetCenter ? "On" : "Off"; };
    algo.setGap = function(_v) { algo.presetGap = (_v === "On") ? 1 : 0; };
    algo.getGap = function() { return algo.presetGap ? "On" : "Off"; };

    // --- Internal state ---
    var peakValues = null;
    var peakHolds = null;
    var DEFAULT_GRADIENT = [0xFF0000, 0xFF8000, 0xFFFF00, 0x00FF80, 0x4080FF];
    var gradientLut = null;
    var lutWidth = -1;
    var lutSig = "";
    var initialized = false;

    function init(bandCount)
    {
        peakValues = new Array(bandCount);
        peakHolds = new Array(bandCount);
        for (var i = 0; i < bandCount; i++) {
            peakValues[i] = 0;
            peakHolds[i] = 0;
        }
        initialized = true;
    }
    function unpackColor(packed) { return [(packed >> 16) & 0xFF, (packed >> 8) & 0xFF, packed & 0xFF]; }

    algo.rgbMapStepCount = function(width, height) { return 1; };

    algo.rgbMapSetColors = function(rawColors) { };

    algo.rgbMapGetColors = function()
    {
        return algo.gradientColors ? algo.gradientColors.slice() : DEFAULT_GRADIENT.slice();
    };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var effectiveWidth = (typeof algo.displayWidth !== 'undefined') ? algo.displayWidth : width;
        var bandCount = effectiveWidth;
        if (!initialized || (peakValues && peakValues.length !== bandCount))
            init(bandCount);

        var map = RGBUtil.createMap(width, height);

        if (!audio) return map;
        var melSrc = audio.spectrum.full;
        if (!melSrc || melSrc.length === 0)
            return map;

        // Get spectrum interpolated to match grid width
        var rawBands = RGBUtil.interpolate(melSrc, bandCount);
        for (var i = 0; i < rawBands.length; i++)
            rawBands[i] = Math.min(1, rawBands[i]);

        var stops = (algo.gradientColors && algo.gradientColors.length > 0) ? algo.gradientColors : DEFAULT_GRADIENT;
        var sig = stops.length + ":" + stops.join(",");
        if (gradientLut === null || lutWidth !== width || lutSig !== sig) {
            gradientLut = RGBUtil.gradientLut(stops, width);
            lutWidth = width;
            lutSig = sig;
        }

        var bands = rawBands;
        var onset = audio.onset.fired;

        for (var x = 0; x < Math.min(bandCount, width); x++)
        {
            // Gap mode: leave every other column dark
            if (algo.presetGap && (x % 2 === 1)) continue;

            var magnitude = Math.max(0, Math.min(1, bands[x]));
            var barHeight = Math.round(magnitude * height);

            // Update peak marker
            if (algo.presetPeaks) {
                if (audio.bar.downbeat) {
                    peakValues[x] *= 0.5;
                }
                if (magnitude >= peakValues[x]) {
                    peakValues[x] = magnitude;
                    peakHolds[x] = 5;
                } else if (onset && rawBands[x] > 0.05) {
                    peakValues[x] = Math.max(peakValues[x], magnitude);
                    peakHolds[x] = 5;
                } else if (peakHolds[x] > 0) {
                    peakHolds[x]--;
                } else {
                    peakValues[x] *= 0.95;
                }
            }

            if (algo.presetCenter)
            {
                // Centered: bars grow from middle
                var halfBar = Math.floor(barHeight / 2);
                var mid = Math.floor(height / 2);
                for (var y = mid - halfBar; y <= mid + halfBar; y++)
                {
                    if (y < 0 || y >= height) continue;
                    var c = unpackColor(gradientLut[x]);
                    var brightness = algo.presetFloor/100 + (1 - algo.presetFloor/100) * magnitude;
                    map[y][x] = RGBUtil.rgb(c[0] * brightness, c[1] * brightness, c[2] * brightness);
                }
            }
            else
            {
                // Bottom-up bars
                for (var dy = 0; dy < barHeight; dy++)
                {
                    var y = height - 1 - dy;
                    if (y < 0) break;
                    var c = unpackColor(gradientLut[x]);
                    var brightness = algo.presetFloor/100 + (1 - algo.presetFloor/100) * magnitude;
                    map[y][x] = RGBUtil.rgb(c[0] * brightness, c[1] * brightness, c[2] * brightness);
                }
            }

            // Peak marker (white dot)
            if (algo.presetPeaks && peakValues[x] > 0.01)
            {
                var peakY;
                if (algo.presetCenter) {
                    var peakHalf = Math.floor(peakValues[x] * height / 2);
                    peakY = Math.floor(height / 2) - peakHalf;
                } else {
                    peakY = height - 1 - Math.floor(peakValues[x] * height);
                }
                if (peakY >= 0 && peakY < height) {
                    var peakBrightness = algo.presetFloor/100 + (1 - algo.presetFloor/100) * peakValues[x];
                    map[peakY][x] = RGBUtil.rgb(255 * peakBrightness, 255 * peakBrightness, 255 * peakBrightness);
                }
            }
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
