/*
  Q Light Controller Plus
  audiowavelength.js

  Copyright (c) QLC+ contributors
  Ported from LedFx "Wavelength" effect (MIT License)

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
    algo.acceptColors = 5;
    algo.usesAudio = true;
    algo.properties = new Array();

    algo.presetBlur = 3;
    algo.properties.push(
      "name:blur|type:range|display:Blur|" +
      "values:0,10|write:setBlur|read:getBlur");
    algo.setBlur = function(_v) { algo.presetBlur = clampFloat(_v, 0, 10, 3); };
    algo.getBlur = function() { return algo.presetBlur; };

    algo.presetGradientRoll = 0;
    algo.properties.push(
      "name:gradient_roll|type:range|display:Gradient Roll|" +
      "values:0,10|write:setGradientRoll|read:getGradientRoll");
    algo.setGradientRoll = function(_v) { algo.presetGradientRoll = clampFloat(_v, 0, 10, 0); };
    algo.getGradientRoll = function() { return algo.presetGradientRoll; };

    var DEFAULT_HSV_STOPS = [
        { h: 0.000, s: 1.0, v: 1.0 },
        { h: 0.078, s: 1.0, v: 1.0 },
        { h: 0.131, s: 1.0, v: 1.0 },
        { h: 0.333, s: 1.0, v: 1.0 },
        { h: 0.446, s: 1.0, v: 0.78 },
        { h: 0.667, s: 1.0, v: 1.0 },
        { h: 0.833, s: 1.0, v: 0.50 },
        { h: 0.884, s: 1.0, v: 1.0 }
    ];
    var gradientRollCounter = 0;

    function clampFloat(v, lo, hi, fallback) {
      v = parseFloat(v);
      if (isNaN(v)) v = fallback;
      return Math.max(lo, Math.min(hi, v));
    }

    function gradientStops() {
      return (algo.colors && algo.colors.length > 0)
        ? algo.colors : DEFAULT_HSV_STOPS;
    }

    function boxBlurHsv(src, amount) {
      var radius = Math.round(amount);
      if (radius <= 0 || src.length <= 3) return src;
      var out = new Array(src.length);
      for (var i = 0; i < src.length; i++) {
        var refH = src[i].h;
        var sumH = 0, sumS = 0, sumV = 0, count = 0;
        for (var j = i - radius; j <= i + radius; j++) {
          if (j < 0 || j >= src.length) continue;
          var dh = src[j].h - refH;
          if (dh > 0.5) dh -= 1;
          else if (dh < -0.5) dh += 1;
          sumH += dh;
          sumS += src[j].s;
          sumV += src[j].v;
          count++;
        }
        var h = refH + sumH / count;
        h = h - Math.floor(h);
        out[i] = {h: h, s: sumS / count, v: sumV / count};
      }
      return out;
    }

    function rolledGradientColor(t, pixelCount) {
      if (algo.presetGradientRoll !== 0 && pixelCount > 0) {
        t = HSVUtil.mod1(t - gradientRollCounter / pixelCount);
      }
      return HSVUtil.gradientAt(gradientStops(), t);
    }

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
      var pixelCount = width * height;
      var map = HSVUtil.createMap(width, height);
      if (!audio || pixelCount <= 0) return map;

      var rValues = HSVUtil.interpolate((audio.spectrum && audio.spectrum.full) || [], pixelCount);
      var pixels = new Array(pixelCount);
      var denom = Math.max(1, pixelCount - 1);

      for (var i = 0; i < pixelCount; i++) {
        var hsv = rolledGradientColor(i / denom, pixelCount);
        var scale = HSVUtil.clamp01(rValues[i]);
        pixels[i] = {h: hsv.h, s: hsv.s, v: hsv.v * scale};
      }

      pixels = boxBlurHsv(pixels, algo.presetBlur);

      for (var y = 0; y < height; y++) {
        for (var x = 0; x < width; x++) {
          var px = pixels[y * width + x];
          var i3 = (y * width + x) * 3;
          map[i3] = px.h;
          map[i3 + 1] = px.s;
          map[i3 + 2] = px.v;
        }
      }

      if (algo.presetGradientRoll !== 0) {
        var dtFrames = (audio.timing.consumerDtMs > 0 ? audio.timing.consumerDtMs : 40) / 40;
        gradientRollCounter += algo.presetGradientRoll * dtFrames;
      }
      return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
