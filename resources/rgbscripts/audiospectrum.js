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
    algo.setRgbMix = function(_v) {
      _v = parseInt(_v);
      if (isNaN(_v)) _v = 0;
      algo.presetRgbMix = Math.max(0, Math.min(5, _v));
    };
    algo.getRgbMix = function() { return algo.presetRgbMix; };

    var rgbMixes = [
      [0, 1, 2],
      [0, 2, 1],
      [1, 0, 2],
      [1, 2, 0],
      [2, 0, 1],
      [2, 1, 0]
    ];
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

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
      var pixelCount = width * height;
      ensureState(pixelCount);
      var map = RGBUtil.createMap(width, height);
      if (!audio) return map;

      // y = unfiltered channel (same source — QLC+ has one melbank flavor)
      var y = RGBUtil.interpolate((audio.spectrum && audio.spectrum.full) || [], pixelCount);
      var filtered = y.slice();
      var filt = updateFilter(y);
      var mix = rgbMixes[algo.presetRgbMix];
      var nextPrev = y.slice();

      for (var i = 0; i < pixelCount; i++) {
        var channels = [0, 0, 0];
        channels[mix[0]] = filtered[i] * 255.0;
        channels[mix[1]] = Math.abs(y[i] - prevY[i]) * 255.0;
        channels[mix[2]] = filt[i] * 255.0;
        var x = i % width;
        var row = Math.floor(i / width);
        map[row][x] = RGBUtil.rgb(channels[0], channels[1], channels[2]);
      }

      prevY = nextPrev;
      return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
