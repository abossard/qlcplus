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
    algo.acceptColors = 3;
    algo.usesAudio = true;
    algo.properties = new Array();

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

    algo.presetPeakHold = 5;
    algo.properties.push(
      "name:presetPeakHold|type:range|display:Peak Hold|" +
      "values:1,20|write:setPeakHold|read:getPeakHold");

    algo.presetPeakDecay = 0.95;
    algo.properties.push(
      "name:presetPeakDecay|type:float|display:Peak Decay|" +
      "write:setPeakDecay|read:getPeakDecay");

    algo.presetDownbeatDrop = 0.5;
    algo.properties.push(
      "name:presetDownbeatDrop|type:float|display:Downbeat Drop|" +
      "write:setDownbeatDrop|read:getDownbeatDrop");

    algo.setDecay = function(_v) { algo.presetDecay = parseInt(_v); };
    algo.getDecay = function()  { return algo.presetDecay; };
    algo.setPeaks = function(_v) { algo.presetPeaks = (_v === "On") ? 1 : 0; };
    algo.getPeaks = function()  { return algo.presetPeaks ? "On" : "Off"; };
    algo.setCenter = function(_v) { algo.presetCenter = (_v === "On") ? 1 : 0; };
    algo.getCenter = function()  { return algo.presetCenter ? "On" : "Off"; };
    algo.setGap = function(_v) { algo.presetGap = (_v === "On") ? 1 : 0; };
    algo.getGap = function() { return algo.presetGap ? "On" : "Off"; };
    algo.setPeakHold = function(_v) { algo.presetPeakHold = parseInt(_v); };
    algo.getPeakHold = function() { return algo.presetPeakHold; };
    algo.setPeakDecay = function(_v) { algo.presetPeakDecay = Math.min(0.999, parseFloat(_v)); };
    algo.getPeakDecay = function() { return algo.presetPeakDecay; };
    algo.setDownbeatDrop = function(_v) { algo.presetDownbeatDrop = parseFloat(_v); };
    algo.getDownbeatDrop = function() { return algo.presetDownbeatDrop; };

    // --- Internal state ---
    var MIN_ONSET_BAND = 0.05;
    var MIN_PEAK_RENDER = 0.01;

    var peakValues = null;
    var peakHolds = null;
    var smoothedBands = null;
    var DEFAULT_GRADIENT = [
      {h: 0.0,   s: 1.0, v: 1.0},
      {h: 0.083, s: 1.0, v: 1.0},
      {h: 0.167, s: 1.0, v: 1.0},
      {h: 0.398, s: 1.0, v: 1.0},
      {h: 0.611, s: 0.75, v: 1.0}
    ];

    function init(bandCount)
    {
        peakValues = new Array(bandCount);
        peakHolds = new Array(bandCount);
        smoothedBands = new Array(bandCount);
        for (var i = 0; i < bandCount; i++) {
            peakValues[i] = 0;
            peakHolds[i] = 0;
            smoothedBands[i] = 0;
        }
    }

    algo.rgbMapStepCount = function(width, height) { return 1; };

    algo.rgbMapSetColors = function(rawColors) { };

    algo.rgbMapGetColors = function()
    {
        return algo.gradientColors ? algo.gradientColors.slice() : DEFAULT_GRADIENT.slice();
    };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var bandCount = algo.displayWidth;
        if (peakValues === null || peakValues.length !== bandCount)
            init(bandCount);

        var map = RGBUtil.createMap(width, height);

        if (!audio) return map;
        var melSrc = audio.spectrum.full;
        if (!melSrc || melSrc.length === 0)
            return map;

        var rawBands = RGBUtil.interpolate(melSrc, bandCount);

        var stops = (algo.gradientColors && algo.gradientColors.length > 0) ? algo.gradientColors : DEFAULT_GRADIENT;

        // Asymmetric EMA: instant attack, exponential decay.
        // presetDecay 1..10 → alpha 0.05..0.5
        var decayAlpha = algo.presetDecay / 20.0;
        var bands = new Array(bandCount);
        for (var b = 0; b < bandCount; b++)
        {
            var raw = Math.max(0, rawBands[b]);
            if (raw >= smoothedBands[b])
                smoothedBands[b] = raw;
            else
                smoothedBands[b] += (raw - smoothedBands[b]) * decayAlpha;
            bands[b] = smoothedBands[b];
        }
        var onset = audio.onset.fired;

        for (var x = 0; x < Math.min(bandCount, width); x++)
        {
            if (algo.presetGap && (x % 2 === 1)) continue;

            var magnitude = bands[x];
            var barHeight = Math.round(magnitude * height);

            // Update peak marker
            if (algo.presetPeaks) {
                if (audio.bar.downbeat) {
                    peakValues[x] *= algo.presetDownbeatDrop;
                }
                if (magnitude >= peakValues[x]) {
                    peakValues[x] = magnitude;
                    peakHolds[x] = algo.presetPeakHold;
                } else if (onset && rawBands[x] > MIN_ONSET_BAND) {
                    peakValues[x] = Math.max(peakValues[x], magnitude);
                    peakHolds[x] = algo.presetPeakHold;
                } else if (peakHolds[x] > 0) {
                    peakHolds[x]--;
                } else {
                    peakValues[x] *= algo.presetPeakDecay;
                }
            }

            // Gradient color for this band position
            var t = (bandCount > 1) ? x / (bandCount - 1) : 0;
            var c = RGBUtil.gradientAt(stops, t);
            var brightness = magnitude;

            if (algo.presetCenter)
            {
                var halfBar = Math.floor(barHeight / 2);
                var mid = Math.floor(height / 2);
                for (var y = mid - halfBar; y <= mid + halfBar; y++)
                {
                    if (y < 0 || y >= height) continue;
                    RGBUtil.setPixel(map, width, x, y, c.h, c.s, c.v * brightness);
                }
            }
            else
            {
                for (var dy = 0; dy < barHeight; dy++)
                {
                    var y = height - 1 - dy;
                    if (y < 0) break;
                    RGBUtil.setPixel(map, width, x, y, c.h, c.s, c.v * brightness);
                }
            }

            // Peak marker (white dot)
            if (algo.presetPeaks && peakValues[x] > MIN_PEAK_RENDER)
            {
                var peakY;
                if (algo.presetCenter) {
                    var peakHalf = Math.floor(peakValues[x] * height / 2);
                    peakY = Math.floor(height / 2) - peakHalf;
                } else {
                    peakY = height - 1 - Math.min(height - 1, Math.floor(peakValues[x] * height));
                }
                if (peakY >= 0 && peakY < height) {
                    var peakBrightness = peakValues[x];
                    RGBUtil.setPixel(map, width, x, peakY, 0, 0, peakBrightness);
                }
            }
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
