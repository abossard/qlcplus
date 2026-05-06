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
    algo.acceptColors = 2; // start + end gradient
    algo.usesAudio = true;
    algo.properties = new Array();

    AudioParams.installContinuous(algo, {gain: 5, reactivity: 5});

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
    var barFilter = null;
    var peakValues = null;
    var startColor = [255, 0, 0];
    var endColor = [0, 0, 255];
    var initialized = false;

    function init(bandCount)
    {
        var decay = algo.presetDecay / 20.0;
        barFilter = new AudioDSP.Filter(decay, AudioParams.filterRise(algo));
        peakValues = new Array(bandCount);
        for (var i = 0; i < bandCount; i++) peakValues[i] = 0;
        initialized = true;
    }

    /** Compute gradient color between start and end */
    function gradientColor(t)
    {
        t = Math.max(0, Math.min(1, t));
        return [
            Math.round(startColor[0] + (endColor[0] - startColor[0]) * t),
            Math.round(startColor[1] + (endColor[1] - startColor[1]) * t),
            Math.round(startColor[2] + (endColor[2] - startColor[2]) * t)
        ];
    }

    algo.rgbMapStepCount = function(width, height) { return 1; };

    algo.rgbMapSetColors = function(rawColors)
    {
        if (rawColors && rawColors.length >= 1) {
            startColor = [(rawColors[0] >> 16) & 0xFF, (rawColors[0] >> 8) & 0xFF, rawColors[0] & 0xFF];
        }
        if (rawColors && rawColors.length >= 2) {
            endColor = [(rawColors[1] >> 16) & 0xFF, (rawColors[1] >> 8) & 0xFF, rawColors[1] & 0xFF];
        }
    };

    algo.rgbMapGetColors = function()
    {
        return [
            RGBUtil.rgb(startColor[0], startColor[1], startColor[2]),
            RGBUtil.rgb(endColor[0], endColor[1], endColor[2])
        ];
    };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var effectiveWidth = (typeof algo.displayWidth !== 'undefined') ? algo.displayWidth : width;
        var bandCount = effectiveWidth;
        if (!initialized || (peakValues && peakValues.length !== bandCount))
            init(bandCount);

        var map = RGBUtil.createMap(width, height);

        if (!audio || !audio.mel || audio.mel.length === 0)
            return map;

        // Get spectrum interpolated to match grid width
        var rawBands = RGBUtil.interpolate(audio.mel, bandCount);
        for (var i = 0; i < rawBands.length; i++)
            rawBands[i] = Math.min(1, rawBands[i]);

        // Apply smoothing
        var bands = barFilter.updateArray(rawBands);

        var peakDecay = 0.01 + (10 - algo.presetDecay) * 0.005;

        for (var x = 0; x < Math.min(bandCount, width); x++)
        {
            // Gap mode: leave every other column dark
            if (algo.presetGap && (x % 2 === 1)) continue;

            var magnitude = Math.max(0, Math.min(1, bands[x]));
            var barHeight = Math.round(magnitude * height);

            // Update peak marker
            if (algo.presetPeaks) {
                if (magnitude > peakValues[x])
                    peakValues[x] = magnitude;
                else
                    peakValues[x] = Math.max(0, peakValues[x] - peakDecay);
            }

            if (algo.presetCenter)
            {
                // Centered: bars grow from middle
                var halfBar = Math.floor(barHeight / 2);
                var mid = Math.floor(height / 2);
                for (var y = mid - halfBar; y <= mid + halfBar; y++)
                {
                    if (y < 0 || y >= height) continue;
                    var t = Math.abs(y - mid) / (height / 2);
                    var c = gradientColor(t);
                    var brightness = AudioParams.applyFloor(algo, 1.0);
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
                    var t = dy / height;
                    var c = gradientColor(t);
                    var brightness = AudioParams.applyFloor(algo, 1.0);
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
                    var peakBrightness = AudioParams.applyFloor(algo, 1.0);
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
