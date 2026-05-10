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

    algo.presetMirror = "Yes";
    algo.properties.push(
      "name:mirror|type:list|display:Mirror|" +
      "values:No,Yes|write:setMirror|read:getMirror");
    algo.setMirror = function(_v) { algo.presetMirror = (_v === "No") ? "No" : "Yes"; };
    algo.getMirror = function() { return algo.presetMirror; };

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

    var lowsColor = [255, 0, 0];
    var midsColor = [0, 255, 0];
    var highColor = [0, 0, 255];
    algo.presetColorLows = "#FF0000";
    algo.presetColorMids = "#00FF00";
    algo.presetColorHigh = "#0000FF";
    algo.properties.push(
      "name:color_lows|type:string|display:Color Lows|" +
      "write:setColorLows|read:getColorLows");
    algo.properties.push(
      "name:color_mids|type:string|display:Color Mids|" +
      "write:setColorMids|read:getColorMids");
    algo.properties.push(
      "name:color_high|type:string|display:Color High|" +
      "write:setColorHigh|read:getColorHigh");

    algo.setColorLows = function(_v) {
      algo.presetColorLows = normalizeColorString(_v, algo.presetColorLows);
      lowsColor = unpack(parseColorString(algo.presetColorLows));
    };
    algo.getColorLows = function() { return algo.presetColorLows; };
    algo.setColorMids = function(_v) {
      algo.presetColorMids = normalizeColorString(_v, algo.presetColorMids);
      midsColor = unpack(parseColorString(algo.presetColorMids));
    };
    algo.getColorMids = function() { return algo.presetColorMids; };
    algo.setColorHigh = function(_v) {
      algo.presetColorHigh = normalizeColorString(_v, algo.presetColorHigh);
      highColor = unpack(parseColorString(algo.presetColorHigh));
    };
    algo.getColorHigh = function() { return algo.presetColorHigh; };

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
      for (var i = 0; i < pixelCount; i++) pixels[i] = [0, 0, 0];
      lastPixelCount = pixelCount;
    }

    function unpack(c) {
      c = c & 0xFFFFFF;
      return [(c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF];
    }

    function toHex(c) {
      c = c & 0xFFFFFF;
      var s = c.toString(16).toUpperCase();
      while (s.length < 6) s = "0" + s;
      return "#" + s;
    }

    function parseColorString(s) {
      if (typeof s !== "string") return 0;
      var m = s.match(/#?([0-9a-fA-F]{6})/);
      return m ? parseInt(m[1], 16) : 0;
    }

    function normalizeColorString(s, fallback) {
      var c = parseColorString(s);
      return c === 0 && !String(s).match(/#?0{6}/) ? fallback : toHex(c);
    }

    function maxInRange(arr, start, end) {
      var m = 0;
      for (var i = start; i < end; i++) {
        var v = arr[i];
        if (v > m) m = v;
      }
      return m;
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

    function mirrorPixels(src) {
      var n = src.length;
      var out = new Array(n);
      for (var i = 0; i < n; i++) {
        var a = src[n - 1 - (2 * i)];
        var b = src[n - 2 - (2 * i)];
        if (i >= Math.ceil(n / 2)) {
          var k = 2 * i - n;
          a = src[k];
          b = src[Math.min(k + 1, n - 1)];
        } else if (!b) {
          b = src[0];
        }
        out[i] = [Math.max(a[0], b[0]), Math.max(a[1], b[1]), Math.max(a[2], b[2])];
      }
      return out;
    }

    function renderPixelsForOutput() {
      var out = pixels;
      if (algo.presetMirror === "Yes") out = mirrorPixels(out);
      out = boxBlur(out, algo.presetBlur);
      return out;
    }

    algo.rgbMapStepCount = function(width, height) { return 1; };

    algo.rgbMapSetColors = function(rawColors) {
      var colors = RGBUtil.buildGradientColors(rawColors);
      if (colors.length > 0) algo.setColorLows(toHex(colors[0]));
      if (colors.length > 1) algo.setColorMids(toHex(colors[1]));
      if (colors.length > 2) algo.setColorHigh(toHex(colors[2]));
    };

    algo.rgbMapGetColors = function() {
      return [RGBUtil.rgb(lowsColor[0], lowsColor[1], lowsColor[2]),
              RGBUtil.rgb(midsColor[0], midsColor[1], midsColor[2]),
              RGBUtil.rgb(highColor[0], highColor[1], highColor[2])];
    };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
      var pixelCount = width * height;
      ensurePixels(pixelCount);

      if (audio) {
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
          intensities[i] = RGBUtil.clamp01(intensities[i]);
          if (intensities[i] < cutoffs[i]) intensities[i] = 0;
        }

        var speed = Math.min(algo.presetSpeed, pixelCount);
        for (var dst = pixelCount - 1; dst >= speed; dst--) {
          pixels[dst][0] = pixels[dst - speed][0];
          pixels[dst][1] = pixels[dst - speed][1];
          pixels[dst][2] = pixels[dst - speed][2];
        }

        var decay = algo.presetDecay / 100.0;
        for (var p = 0; p < pixelCount; p++) {
          pixels[p][0] *= decay;
          pixels[p][1] *= decay;
          pixels[p][2] *= decay;
        }

        var r = lowsColor[0] * intensities[0] + midsColor[0] * intensities[1] + highColor[0] * intensities[2];
        var g = lowsColor[1] * intensities[0] + midsColor[1] * intensities[1] + highColor[1] * intensities[2];
        var b = lowsColor[2] * intensities[0] + midsColor[2] * intensities[1] + highColor[2] * intensities[2];
        for (var n = 0; n < speed; n++) pixels[n] = [r, g, b];
      }

      var outPixels = renderPixelsForOutput();
      var map = RGBUtil.createMap(width, height);
      for (var y = 0; y < height; y++) {
        for (var x = 0; x < width; x++) {
          var px = outPixels[y * width + x];
          map[y][x] = RGBUtil.rgb(px[0], px[1], px[2]);
        }
      }
      return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
