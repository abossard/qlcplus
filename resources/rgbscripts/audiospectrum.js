/*
  Q Light Controller Plus
  audiospectrum.js

  Copyright (c) QLC+ contributors
  Ported from LedFx "Spectrum" effect (MIT License)

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
    algo.acceptColors = 0;
    algo.usesAudio = true;
    algo.properties = new Array();

    algo.presetRgbMix = 0;
    algo.properties.push(
      "name:rgb_mix|type:range|display:RGB Mix|" +
      "values:0,5|write:setRgbMix|read:getRgbMix");
    algo.setRgbMix = function(_v) { algo.presetRgbMix = parseFloat(_v); };
    algo.getRgbMix = function() { return algo.presetRgbMix; };

    var rgbMixes = [
      [0, 1, 2],
      [0, 2, 1],
      [1, 0, 2],
      [1, 2, 0],
      [2, 0, 1],
      [2, 1, 0]
    ];
    algo.presetDecay = 0.05;
    algo.properties.push(
      "name:decay|type:float|display:Decay|" +
      "write:setDecay|read:getDecay");
    algo.setDecay = function(_v) { algo.presetDecay = parseFloat(_v); };
    algo.getDecay = function() { return algo.presetDecay; };

    var prevY = null;
    var bFilter = null;
    var lastPixelCount = -1;

    function ensureState(pixelCount) {
      if (lastPixelCount === pixelCount && prevY) return;
      prevY = new Array(pixelCount);
      bFilter = null;
      for (var i = 0; i < pixelCount; i++) prevY[i] = 0;
      lastPixelCount = pixelCount;
    }

    function updateFilter(values) {
      if (!bFilter || bFilter.length !== values.length) {
        bFilter = values.slice();
        return bFilter.slice();
      }
      var out = new Array(values.length);
      for (var i = 0; i < values.length; i++) {
        var alpha = values[i] > bFilter[i] ? 0.5 : 0.1;
        bFilter[i] = alpha * values[i] + (1.0 - alpha) * bFilter[i];
        out[i] = bFilter[i];
      }
      return out;
    }

    // Local RGB→HSV (the effect inherently creates colors from 3 independent
    // channels, so we convert the conceptual RGB to HSV for output).
    function toHsv(r, g, b) {
        r /= 255; g /= 255; b /= 255;
        var mx = Math.max(r, g, b), mn = Math.min(r, g, b);
        var d = mx - mn;
        var h = 0, s = (mx === 0) ? 0 : d / mx, v = mx;
        if (d > 0) {
            if (mx === r) h = ((g - b) / d) % 6;
            else if (mx === g) h = (b - r) / d + 2;
            else h = (r - g) / d + 4;
            h /= 6;
            if (h < 0) h += 1;
        }
        return {h: h, s: s, v: v};
    }

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
      var pixelCount = width * height;
      ensureState(pixelCount);
      var map = HSVUtil.createMap(width, height);
      if (!audio) return map;

      var y = HSVUtil.interpolate([audio.low, audio.mid, audio.high], pixelCount);
      var filtered = y.slice();
      var filt = updateFilter(y);
      var mix = rgbMixes[algo.presetRgbMix];
      var nextPrev = y.slice();

      for (var i = 0; i < pixelCount; i++) {
        var channels = [0, 0, 0];
        channels[mix[0]] = filtered[i] * 255.0;
        channels[mix[1]] = Math.abs(y[i] - prevY[i]) * 255.0;
        channels[mix[2]] = filt[i] * 255.0;

        var hsv = toHsv(channels[0], channels[1], channels[2]);
        var px = i % width;
        var row = Math.floor(i / width);
        var i3 = (row * width + px) * 3;
        map[i3] = hsv.h;
        map[i3 + 1] = hsv.s;
        map[i3 + 2] = hsv.v;
      }

      prevY = nextPrev;
      return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
