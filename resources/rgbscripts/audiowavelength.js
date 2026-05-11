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

    var DEFAULT_GRADIENT = [
      0xFF0000, 0xFF7800, 0xFFC800, 0x00FF00,
      0x00C78C, 0x0000FF, 0x800080, 0xFF00B2
    ];
    var gradientRollCounter = 0;

    function clampFloat(v, lo, hi, fallback) {
      v = parseFloat(v);
      if (isNaN(v)) v = fallback;
      return Math.max(lo, Math.min(hi, v));
    }

    function gradientStops() {
      return (algo.gradientColors && algo.gradientColors.length > 0)
        ? algo.gradientColors : DEFAULT_GRADIENT;
    }

    function boxBlur(src, amount) {
      var radius = Math.round(amount);
      if (radius <= 0 || src.length <= 3) return src;
      var out = new Array(src.length);
      for (var i = 0; i < src.length; i++) {
        var r = 0, g = 0, b = 0, count = 0;
        for (var j = i - radius; j <= i + radius; j++) {
          if (j < 0 || j >= src.length) continue;
          r += src[j][0]; g += src[j][1]; b += src[j][2]; count++;
        }
        out[i] = [r / count, g / count, b / count];
      }
      return out;
    }

    function rolledGradientColor(t, pixelCount) {
      if (algo.presetGradientRoll !== 0 && pixelCount > 0) {
        t = RGBUtil.mod1(t - gradientRollCounter / pixelCount);
      }
      return RGBUtil.gradientColorAt(gradientStops(), t);
    }

    algo.rgbMapStepCount = function(width, height) { return 1; };

    algo.rgbMapGetColors = function() { return gradientStops().slice(); };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
      var pixelCount = width * height;
      var map = RGBUtil.createFlatMap(width, height);
      if (!audio || pixelCount <= 0) return map;

      var rValues = RGBUtil.interpolate((audio.spectrum && audio.spectrum.full) || [], pixelCount);
      var pixels = new Array(pixelCount);
      var denom = Math.max(1, pixelCount - 1);

      for (var i = 0; i < pixelCount; i++) {
        var packed = rolledGradientColor(i / denom, pixelCount);
        var scale = RGBUtil.clamp01(rValues[i]);
        pixels[i] = [
          ((packed >> 16) & 0xFF) * scale,
          ((packed >> 8) & 0xFF) * scale,
          (packed & 0xFF) * scale
        ];
      }

      pixels = boxBlur(pixels, algo.presetBlur);

      for (var y = 0; y < height; y++) {
        for (var x = 0; x < width; x++) {
          var px = pixels[y * width + x];
          map[(y) * width + (x)] = RGBUtil.rgb(px[0], px[1], px[2]);
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
