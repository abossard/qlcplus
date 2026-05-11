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

    var DEFAULT_BAND_COLORS = [
      {h: 0.958, s: 1.0, v: 1.0},
      {h: 0.167, s: 1.0, v: 1.0},
      {h: 0.611, s: 0.75, v: 1.0}
    ];

    algo.presetBlobWidth = 6;
    algo.properties.push(
      "name:presetBlobWidth|type:range|display:Blob Width (sigma px)|" +
      "values:1,40|write:setBlobWidth|read:getBlobWidth");
    algo.setBlobWidth = function(_v) { algo.presetBlobWidth = parseInt(_v); };
    algo.getBlobWidth = function() { return algo.presetBlobWidth; };

    algo.presetSpeed = 0.25;
    algo.properties.push(
      "name:presetSpeed|type:float|display:Speed (cyc/beat)|" +
      "write:setSpeed|read:getSpeed");
    algo.setSpeed = function(_v) { algo.presetSpeed = parseFloat(_v); };
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
    algo.accIntensity = null;
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
      var dtMs = audio.timing.consumerDtMs;
      var bpm = (audio && audio.beat) ? audio.beat.bpm : 0;
      var bpmEff = (bpm > 0) ? bpm : 120;
      var dtBeats = dtMs * bpmEff / 60000.0;
      var horizontal = (algo.presetAxis === "Horizontal");
      var N = horizontal ? width : height;
      ensureBlobs(N);

      var speedScale = algo.presetSpeed;
      for (var i = 0; i < 3; i++) {
        algo.pos[i] += algo.vel[i] * speedScale * dtBeats;
        if (algo.pos[i] < 0) { algo.pos[i] = -algo.pos[i]; algo.vel[i] = -algo.vel[i]; }
        if (algo.pos[i] > 1) { algo.pos[i] = 2 - algo.pos[i]; algo.vel[i] = -algo.vel[i]; }
      }

      var map = RGBUtil.createMap(width, height);
      var bandColors = (algo.gradientBandColors && algo.gradientBandColors.length >= 3)
        ? algo.gradientBandColors : DEFAULT_BAND_COLORS;
      var powers = audio.power.bands;
      var sigma = Math.max(1, algo.presetBlobWidth);
      var twoSigSq = 2 * sigma * sigma;

      // Per-band intensity accumulator for each long-axis position
      if (algo.accLen !== N) {
        algo.accIntensity = new Array(N);
        for (var a = 0; a < N; a++) algo.accIntensity[a] = [0, 0, 0];
        algo.accLen = N;
      }
      for (var p = 0; p < N; p++) {
        algo.accIntensity[p][0] = 0;
        algo.accIntensity[p][1] = 0;
        algo.accIntensity[p][2] = 0;
      }

      // Accumulate per-band intensity at each position
      for (var b = 0; b < 3; b++) {
        var cx = algo.pos[b] * (N - 1);
        var amp = powers[b];
        for (var q = 0; q < N; q++) {
          var d = q - cx;
          var falloff = Math.exp(-(d * d) / twoSigSq);
          algo.accIntensity[q][b] = amp * falloff;
        }
      }

      for (var pos = 0; pos < N; pos++) {
        var i0 = algo.accIntensity[pos][0];
        var i1 = algo.accIntensity[pos][1];
        var i2 = algo.accIntensity[pos][2];
        var totalV = i0 + i1 + i2;

        if (totalV < 0.001) continue;

        // Pick dominant band for hue/sat, use total intensity for value
        var domBand = 0;
        if (i1 > i0 && i1 > i2) domBand = 1;
        else if (i2 > i0) domBand = 2;

        var col = bandColors[domBand];
        var v = Math.min(1.0, totalV);

        if (horizontal) {
          for (var y = 0; y < height; y++)
            RGBUtil.setPixel(map, width, pos, y, col.h, col.s, v);
        } else {
          for (var x = 0; x < width; x++)
            RGBUtil.setPixel(map, width, x, pos, col.h, col.s, v);
        }
      }

      return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
