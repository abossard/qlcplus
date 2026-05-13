/*
  Q Light Controller Plus
  audioscroll.js

  Copyright (c) QLC+ contributors
  Ported from LedFx "Scroll" effect (MIT License)

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
    algo.name = "Audio Scroll";
    algo.author = "Ported from LedFx";
    algo.acceptColors = 3;
    algo.usesAudio = true;
    algo.properties = new Array();

    algo.presetBlur = 3;
    algo.properties.push(
      "name:blur|type:range|display:Blur|" +
      "values:0,10|write:setBlur|read:getBlur");
    algo.setBlur = function(_v) { algo.presetBlur = clampFloat(_v, 0, 10, 3); };
    algo.getBlur = function() { return algo.presetBlur; };

    algo.presetSpeed = 3;
    algo.properties.push(
      "name:speed|type:range|display:Speed|" +
      "values:1,10|write:setSpeed|read:getSpeed");
    algo.setSpeed = function(_v) { algo.presetSpeed = clampInt(_v, 1, 10); };
    algo.getSpeed = function() { return algo.presetSpeed; };

    algo.presetDecay = 97;
    algo.properties.push(
      "name:decay|type:range|display:Decay (%)|" +
      "values:80,100|write:setDecay|read:getDecay");
    algo.setDecay = function(_v) { algo.presetDecay = clampInt(_v, 80, 100); };
    algo.getDecay = function() { return algo.presetDecay; };

    algo.presetThreshold = 0;
    algo.properties.push(
      "name:threshold|type:range|display:Threshold (%)|" +
      "values:0,100|write:setThreshold|read:getThreshold");
    algo.setThreshold = function(_v) { algo.presetThreshold = clampInt(_v, 0, 100); };
    algo.getThreshold = function() { return algo.presetThreshold; };

    var pixels = null;
    var lastPixelCount = -1;

    function clampInt(v, lo, hi) {
      v = parseInt(v);
      if (isNaN(v)) v = lo;
      return Math.max(lo, Math.min(hi, v));
    }

    function clampFloat(v, lo, hi, fallback) {
      v = parseFloat(v);
      if (isNaN(v)) v = fallback;
      return Math.max(lo, Math.min(hi, v));
    }

    function ensurePixels(pixelCount) {
      if (pixels && lastPixelCount === pixelCount) return;
      pixels = new Array(pixelCount);
      for (var i = 0; i < pixelCount; i++) pixels[i] = {h: 0, s: 0, v: 0};
      lastPixelCount = pixelCount;
    }

    function maxInRange(arr, start, end) {
      var m = 0;
      for (var i = start; i < end; i++) {
        var v = arr[i];
        if (v > m) m = v;
      }
      return m;
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

    function renderPixelsForOutput() {
      return boxBlurHsv(pixels, algo.presetBlur);
    }

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
      var pixelCount = width * height;
      ensurePixels(pixelCount);

      if (audio) {
        var bandColors = AudioColors.bands(algo);

        var mel = (audio.spectrum && audio.spectrum.full) || [];
        var split1 = Math.floor(0.2 * mel.length);
        var split2 = Math.floor(0.5 * mel.length);
        var intensities = [
          Math.pow(maxInRange(mel, 0, split1), 2),
          Math.pow(maxInRange(mel, split1, split2), 2),
          Math.pow(maxInRange(mel, split2, mel.length), 2)
        ];
        var threshold = algo.presetThreshold / 100.0;
        var cutoffs = [threshold / 10.0, threshold / 8.0, threshold / 7.0];
        for (var i = 0; i < 3; i++) {
          intensities[i] = HSVUtil.clamp01(intensities[i]);
          if (intensities[i] < cutoffs[i]) intensities[i] = 0;
        }

        var speed = Math.min(algo.presetSpeed, pixelCount);
        for (var dst = pixelCount - 1; dst >= speed; dst--) {
          pixels[dst] = {h: pixels[dst - speed].h, s: pixels[dst - speed].s, v: pixels[dst - speed].v};
        }

        var decay = algo.presetDecay / 100.0;
        for (var p = 0; p < pixelCount; p++) {
          pixels[p].v *= decay;
        }

        // Weighted blend of band colors by intensity
        var totalI = intensities[0] + intensities[1] + intensities[2];
        if (totalI > 0.001) {
          var domIdx = 0;
          for (var k = 1; k < 3; k++) if (intensities[k] > intensities[domIdx]) domIdx = k;
          var h = bandColors[domIdx].h;
          var s = bandColors[domIdx].s;
          for (var k = 0; k < 3; k++) {
            if (k === domIdx) continue;
            var t = intensities[k] / totalI;
            if (t > 0.01) {
              var dh = bandColors[k].h - h;
              if (dh > 0.5) dh -= 1;
              else if (dh < -0.5) dh += 1;
              h += t * dh;
              s = s * (1 - t) + bandColors[k].s * t;
            }
          }
          h = h - Math.floor(h);
          var v = Math.min(1, totalI);
          for (var n = 0; n < speed; n++)
            pixels[n] = {h: h, s: s, v: v};
        }
      }

      var outPixels = renderPixelsForOutput();
      var map = HSVUtil.createMap(width, height);
      for (var y = 0; y < height; y++) {
        for (var x = 0; x < width; x++) {
          var px = outPixels[y * width + x];
          var i3 = (y * width + x) * 3;
          map[i3] = px.h;
          map[i3 + 1] = px.s;
          map[i3 + 2] = px.v;
        }
      }
      return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
