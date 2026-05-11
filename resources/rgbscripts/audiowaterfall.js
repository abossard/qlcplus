/*
  Q Light Controller Plus
  audiowaterfall.js

  Copyright (c) QLC+ contributors

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
    algo.name = "Audio Waterfall";
    algo.author = "QLC+ contributors";
    algo.acceptColors = 5;
    algo.usesAudio = true;
    algo.properties = new Array();

    var DEFAULT_GRADIENT = [0x000020, 0x0000FF, 0x00FF00, 0xFFFF00, 0xFF0000];

    algo.presetSpeedHz = 25;
    algo.properties.push(
      "name:presetSpeedHz|type:range|display:Scroll Speed (cols/s)|" +
      "values:1,60|write:setSpeedHz|read:getSpeedHz");
    algo.setSpeedHz = function(v) { algo.presetSpeedHz = parseInt(v); };
    algo.getSpeedHz = function() { return algo.presetSpeedHz; };

    algo.presetBank = "Full";
    algo.properties.push(
      "name:presetBank|type:list|display:Spectrum Source|" +
      "values:Full,Low,Mid,High|write:setBank|read:getBank");
    algo.setBank = function(v) { algo.presetBank = String(v); };
    algo.getBank = function() { return algo.presetBank; };

    algo.presetColorMode = "Gradient";
    algo.properties.push(
      "name:presetColorMode|type:list|display:Color Mode|" +
      "values:Gradient,PowerScaled|write:setColorMode|read:getColorMode");
    algo.setColorMode = function(v) { algo.presetColorMode = String(v); };
    algo.getColorMode = function() { return algo.presetColorMode; };

    algo.presetGain = 100;
    algo.properties.push(
      "name:presetGain|type:range|display:Magnitude Gain (%)|" +
      "values:25,400|write:setGain|read:getGain");
    algo.setGain = function(v) { algo.presetGain = parseInt(v); };
    algo.getGain = function() { return algo.presetGain; };

    algo.history = null;
    algo.histW = 0;
    algo.histH = 0;
    algo.col = 0;
    algo.scrollAccum = 0;
    algo.column = null;

    algo.ensureHistory = function(w, h) {
        if (algo.histW === w && algo.histH === h && algo.history !== null) return;
        algo.histW = w;
        algo.histH = h;
        algo.history = new Array(w * h);
        for (var i = 0; i < w * h; i++) algo.history[i] = 0;
        algo.col = 0;
        algo.scrollAccum = 0;
        algo.column = new Array(h);
    };

    algo.pickSource = function(audio) {
        if (algo.presetBank === "Low")  return audio.spectrum.low.values;
        if (algo.presetBank === "Mid")  return audio.spectrum.mid.values;
        if (algo.presetBank === "High") return audio.spectrum.high.values;
        return audio.spectrum.full;
    };

    algo.rgbMapStepCount = function(_w, _h) { return 1; };
    algo.rgbMapSetColors = function(_raw) { };
    algo.rgbMapGetColors = function() {
        return algo.gradientBandColors
            ? algo.gradientBandColors.slice()
            : DEFAULT_GRADIENT.slice();
    };

    algo.rgbMap = function(width, height, rgb, step, audio) {
        var map = RGBUtil.createFlatMap(width, height);
        var dt = audio.timing.consumerDtMs / 1000.0;
        algo.ensureHistory(width, height);

        var gradient = (audio.colors && audio.colors.gradient && audio.colors.gradient.length > 0)
            ? audio.colors.gradient : DEFAULT_GRADIENT;
        var gain = algo.presetGain / 100.0;
        var denom = Math.max(1, height - 1);

        var src = algo.pickSource(audio);
        var resampled = RGBUtil.interpolate(src, height);
        for (var y = 0; y < height; y++) {
            var renderVal = resampled[y] * gain;
            // Clamp render-side magnitude only to [0,1] for color sampling.
            var mag = renderVal < 0 ? 0 : (renderVal > 1 ? 1 : renderVal);
            var c;
            if (algo.presetColorMode === "PowerScaled") {
                c = RGBUtil.scaleColor(RGBUtil.gradientColorAt(gradient, y / denom), mag);
            } else {
                c = RGBUtil.gradientColorAt(gradient, mag);
            }
            algo.column[y] = c;
            algo.history[algo.col * height + y] = c;
        }

        algo.scrollAccum += dt * algo.presetSpeedHz;
        while (algo.scrollAccum >= 1.0) {
            algo.col = (algo.col + 1) % width;
            algo.scrollAccum -= 1.0;
            // Re-stamp the new write column with the latest data so it isn't blank
            // until the next sample (keeps continuity at high scroll rates).
            for (var yy = 0; yy < height; yy++)
                algo.history[algo.col * height + yy] = algo.column[yy];
        }

        for (var x = 0; x < width; x++) {
            var srcCol = (algo.col - (width - 1 - x) + width) % width;
            for (var yi = 0; yi < height; yi++) {
                var pix = algo.history[srcCol * height + yi];
                map[(yi) * width + (x)] = pix;
            }
        }
        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
