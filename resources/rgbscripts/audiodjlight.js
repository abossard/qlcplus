/*
  Q Light Controller Plus
  audiodjlight.js

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
    algo.name = "Audio DJ Light";
    algo.author = "QLC+ contributors";
    algo.acceptColors = 3;
    algo.usesAudio = true;
    algo.properties = new Array();

    var DEFAULT_BAND_COLORS = [0xFF0040, 0xFFFF00, 0x4080FF];

    algo.presetBlobWidth = 6;
    algo.properties.push(
      "name:presetBlobWidth|type:range|display:Blob Width (sigma px)|" +
      "values:1,40|write:setBlobWidth|read:getBlobWidth");
    algo.setBlobWidth = function(_v) { algo.presetBlobWidth = parseInt(_v); };
    algo.getBlobWidth = function() { return algo.presetBlobWidth; };

    algo.presetSpeed = 30;
    algo.properties.push(
      "name:presetSpeed|type:range|display:Drift Speed|" +
      "values:0,200|write:setSpeed|read:getSpeed");
    algo.setSpeed = function(_v) { algo.presetSpeed = parseInt(_v); };
    algo.getSpeed = function() { return algo.presetSpeed; };

    algo.presetAxis = "Horizontal";
    algo.properties.push(
      "name:presetAxis|type:list|display:Axis|" +
      "values:Horizontal,Vertical|write:setAxis|read:getAxis");
    algo.setAxis = function(_v) { algo.presetAxis = _v; };
    algo.getAxis = function() { return algo.presetAxis; };

    algo.pos = null;
    algo.vel = null;
    algo.lastN = -1;
    algo.accR = null;
    algo.accG = null;
    algo.accB = null;
    algo.accLen = 0;

    function ensureBlobs(N) {
      if (algo.pos && algo.lastN === N) return;
      algo.pos = [0.2, 0.5, 0.8];
      algo.vel = [+1, -1, +1];
      algo.lastN = N;
    }

    algo.rgbMapStepCount = function(_w, _h) { return 1; };
    algo.rgbMapSetColors = function(_raw) { };
    algo.rgbMapGetColors = function() {
      return (algo.gradientBandColors && algo.gradientBandColors.length >= 3)
        ? algo.gradientBandColors.slice() : DEFAULT_BAND_COLORS.slice();
    };

    algo.rgbMap = function(width, height, _rgb, _step, audio) {
      var dt = audio.timing.consumerDtMs / 1000.0;
      var horizontal = (algo.presetAxis === "Horizontal");
      var N = horizontal ? width : height;
      ensureBlobs(N);

      var speedScale = algo.presetSpeed / 100.0;
      for (var i = 0; i < 3; i++) {
        algo.pos[i] += algo.vel[i] * speedScale * dt;
        if (algo.pos[i] < 0) { algo.pos[i] = -algo.pos[i]; algo.vel[i] = -algo.vel[i]; }
        if (algo.pos[i] > 1) { algo.pos[i] = 2 - algo.pos[i]; algo.vel[i] = -algo.vel[i]; }
      }

      var map = RGBUtil.createMap(width, height);
      var colors = AudioColors.bands(algo);
      var powers = audio.power.bands;
      var sigma = Math.max(1, algo.presetBlobWidth);
      var twoSigSq = 2 * sigma * sigma;

      // Accumulate raw RGB per long-axis position.
      if (algo.accLen !== N) {
        algo.accR = new Array(N);
        algo.accG = new Array(N);
        algo.accB = new Array(N);
        algo.accLen = N;
      }
      for (var p = 0; p < N; p++) { algo.accR[p] = 0; algo.accG[p] = 0; algo.accB[p] = 0; }

      for (var b = 0; b < 3; b++) {
        var c = colors[b] | 0;
        var cr = (c >> 16) & 0xFF;
        var cg = (c >> 8) & 0xFF;
        var cb = c & 0xFF;
        var cx = algo.pos[b] * (N - 1);
        var amp = powers[b];
        for (var q = 0; q < N; q++) {
          var d = q - cx;
          var falloff = Math.exp(-(d * d) / twoSigSq);
          var k = amp * falloff;
          algo.accR[q] += cr * k;
          algo.accG[q] += cg * k;
          algo.accB[q] += cb * k;
        }
      }

      for (var pos = 0; pos < N; pos++) {
        var r = algo.accR[pos];
        var g = algo.accG[pos];
        var bl = algo.accB[pos];
        var packed = RGBUtil.rgb(r, g, bl);
        if (horizontal) {
          for (var y = 0; y < height; y++) map[y][pos] = packed;
        } else {
          for (var x = 0; x < width; x++) map[pos][x] = packed;
        }
      }

      return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
