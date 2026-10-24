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

    algo.presetBlobWidth = 6;
    algo.properties.push(
      "name:presetBlobWidth|type:range|display:Blob Width (sigma px)|" +
      "values:1,40|write:setBlobWidth|read:getBlobWidth");
    algo.setBlobWidth = function(_v) { algo.presetBlobWidth = HSVUtil.parseNumber(_v, algo.presetBlobWidth, 1, 40); };
    algo.getBlobWidth = function() { return algo.presetBlobWidth; };

    algo.presetSpeed = 0.25;
    algo.properties.push(
      "name:presetSpeed|type:float|display:Speed (cyc/beat)|" +
      "write:setSpeed|read:getSpeed");
    algo.setSpeed = function(_v) { algo.presetSpeed = HSVUtil.parseNumber(_v, algo.presetSpeed, -100, 100); };
    algo.getSpeed = function() { return algo.presetSpeed; };

    algo.presetAxis = "Horizontal";
    algo.properties.push(
      "name:presetAxis|type:list|display:Axis|" +
      "values:Horizontal,Vertical|write:setAxis|read:getAxis");
    algo.setAxis = function(_v) { algo.presetAxis = _v; };
    algo.getAxis = function() { return algo.presetAxis; };

    algo.presetSmoothing = 5;
    algo.properties.push(
      "name:presetSmoothing|type:range|display:Smoothing|" +
      "values:1,10|write:setSmoothing|read:getSmoothing");
    algo.setSmoothing = function(_v) {
      algo.presetSmoothing = Math.round(HSVUtil.parseNumber(_v, algo.presetSmoothing, 1, 10));
    };
    algo.getSmoothing = function() { return algo.presetSmoothing; };

    var smoothPow = [0, 0, 0];

    algo.pos = null;
    algo.vel = null;
    algo.lastN = -1;

    function ensureBlobs(N) {
      if (algo.pos && algo.lastN === N) return;
      algo.pos = [0.2, 0.5, 0.8];
      algo.vel = [+1, -1, +1];
      algo.lastN = N;
    }

    algo.rgbMapStepCount = function(_w, _h) { return 1; };
    algo.rgbMapSetColors = function(_raw) { };
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, _rgb, _step, audio) {
      var horizontal = (algo.presetAxis === "Horizontal");
      var N = horizontal ? width : height;
      ensureBlobs(N);

      // Bounce off both ends: fold the travelled position into [0, 1], period 2
      for (var i = 0; i < 3; i++) {
        var p = algo.pos[i] + algo.vel[i] * algo.presetSpeed * audio.dt;
        p -= 2 * Math.floor(p / 2);
        if (p > 1) { p = 2 - p; algo.vel[i] = -algo.vel[i]; }
        algo.pos[i] = p;
      }

      var map = HSVUtil.createMap(width, height);

      // Asymmetric EMA smoothing of band powers (fast attack, slow decay)
      var rawPows = [audio.low, audio.mid, audio.high];
      var frames = HSVUtil.elapsedFrames(audio);
      for (var sb = 0; sb < 3; sb++)
        smoothPow[sb] = HSVUtil.smoothFollow(smoothPow[sb], rawPows[sb], algo.presetSmoothing, frames);
      var sigma = Math.max(1, algo.presetBlobWidth);
      var twoSigSq = 2 * sigma * sigma;
      var centers = [algo.pos[0] * (N - 1), algo.pos[1] * (N - 1), algo.pos[2] * (N - 1)];
      var intensity = [0, 0, 0];

      for (var pos = 0; pos < N; pos++) {
        // Per-band gaussian intensity at this long-axis position
        for (var b = 0; b < 3; b++) {
          var d = pos - centers[b];
          intensity[b] = smoothPow[b] * Math.exp(-(d * d) / twoSigSq);
        }
        var i0 = intensity[0];
        var i1 = intensity[1];
        var i2 = intensity[2];
        var totalV = i0 + i1 + i2;

        if (totalV < 0.001) continue;

        // Pick dominant band for hue/sat, use total intensity for value
        var domBand = 0;
        if (i1 > i0 && i1 > i2) domBand = 1;
        else if (i2 > i0) domBand = 2;

        var col = algo.colors[domBand];
        var v = Math.min(1.0, totalV);

        if (horizontal) {
          for (var y = 0; y < height; y++)
            HSVUtil.setPixel(map, width, pos, y, col.h, col.s, v);
        } else {
          for (var x = 0; x < width; x++)
            HSVUtil.setPixel(map, width, x, pos, col.h, col.s, v);
        }
      }

      return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
