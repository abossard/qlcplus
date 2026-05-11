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

    var DEFAULT_HSV_STOPS = [
        { h: 0.667, s: 1.0, v: 0.125 },
        { h: 0.667, s: 1.0, v: 1.0 },
        { h: 0.333, s: 1.0, v: 1.0 },
        { h: 0.167, s: 1.0, v: 1.0 },
        { h: 0.000, s: 1.0, v: 1.0 }
    ];

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
        // Store HSV triplets: history[col * height + y] = {h, s, v}
        algo.history = new Array(w * h);
        for (var i = 0; i < w * h; i++) algo.history[i] = {h: 0, s: 0, v: 0};
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
    algo.rgbMapGetColors = function() { return []; };

    function gradientStops() {
        return (algo.gradientColors && algo.gradientColors.length > 0)
            ? algo.gradientColors : DEFAULT_HSV_STOPS;
    }

    algo.rgbMap = function(width, height, rgb, step, audio) {
        var map = RGBUtil.createMap(width, height);
        var dt = audio.timing.consumerDtMs / 1000.0;
        algo.ensureHistory(width, height);

        var gradient = gradientStops();
        var gain = algo.presetGain / 100.0;
        var denom = Math.max(1, height - 1);

        var src = algo.pickSource(audio);
        var resampled = RGBUtil.interpolate(src, height);
        for (var y = 0; y < height; y++) {
            var renderVal = resampled[y] * gain;
            var mag = renderVal < 0 ? 0 : (renderVal > 1 ? 1 : renderVal);
            var c;
            if (algo.presetColorMode === "PowerScaled") {
                c = RGBUtil.gradientAt(gradient, y / denom);
                c = {h: c.h, s: c.s, v: c.v * mag};
            } else {
                c = RGBUtil.gradientAt(gradient, mag);
            }
            algo.column[y] = c;
            algo.history[algo.col * height + y] = c;
        }

        algo.scrollAccum += dt * algo.presetSpeedHz;
        while (algo.scrollAccum >= 1.0) {
            algo.col = (algo.col + 1) % width;
            algo.scrollAccum -= 1.0;
            for (var yy = 0; yy < height; yy++)
                algo.history[algo.col * height + yy] = algo.column[yy];
        }

        for (var x = 0; x < width; x++) {
            var srcCol = (algo.col - (width - 1 - x) + width) % width;
            for (var yi = 0; yi < height; yi++) {
                var pix = algo.history[srcCol * height + yi];
                var i3 = (yi * width + x) * 3;
                map[i3] = pix.h;
                map[i3 + 1] = pix.s;
                map[i3 + 2] = pix.v;
            }
        }
        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
