/*
  Q Light Controller Plus
  audiochromatic.js

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
    algo.name = "Audio Chromatic Keyboard";
    algo.author = "QLC+ contributors";
    algo.acceptColors = 5;
    algo.usesAudio = true;
    algo.properties = new Array();

    var DEFAULT_GRADIENT = [
        {h: 0.708, s: 1.0, v: 1.0},
        {h: 0.5, s: 1.0, v: 1.0},
        {h: 0.333, s: 1.0, v: 1.0},
        {h: 0.167, s: 1.0, v: 1.0},
        {h: 0.0, s: 1.0, v: 1.0}
    ];

    algo.presetGlowWidth = 100;
    algo.properties.push(
      "name:presetGlowWidth|type:range|display:Glow Width (%)|" +
      "values:0,600|write:setGlowWidth|read:getGlowWidth");
    algo.setGlowWidth = function(v) { algo.presetGlowWidth = parseInt(v); };
    algo.getGlowWidth = function() { return algo.presetGlowWidth; };

    algo.presetGlowDecayMs = 250;
    algo.properties.push(
      "name:presetGlowDecayMs|type:range|display:Glow Decay (ms)|" +
      "values:50,2000|write:setGlowDecayMs|read:getGlowDecayMs");
    algo.setGlowDecayMs = function(v) { algo.presetGlowDecayMs = parseInt(v); };
    algo.getGlowDecayMs = function() { return algo.presetGlowDecayMs; };

    algo.presetMinConfidence = 30;
    algo.properties.push(
      "name:presetMinConfidence|type:range|display:Min Confidence (%)|" +
      "values:0,100|write:setMinConfidence|read:getMinConfidence");
    algo.setMinConfidence = function(v) { algo.presetMinConfidence = parseInt(v); };
    algo.getMinConfidence = function() { return algo.presetMinConfidence; };

    algo.presetAxis = "Horizontal";
    algo.properties.push(
      "name:presetAxis|type:list|display:Axis|" +
      "values:Horizontal,Vertical|write:setAxis|read:getAxis");
    algo.setAxis = function(v) { algo.presetAxis = String(v); };
    algo.getAxis = function() { return algo.presetAxis; };

    algo.zone = [0,0,0,0,0,0,0,0,0,0,0,0];

    algo.rgbMapStepCount = function(_w, _h) { return 1; };
    algo.rgbMapSetColors = function(_raw) { };
    algo.rgbMapGetColors = function() {
        return algo.colors
            ? algo.colors.slice()
            : DEFAULT_GRADIENT.slice();
    };

    algo.rgbMap = function(width, height, rgb, step, audio) {
        var map = HSVUtil.createMap(width, height);
        var dt = audio.timing.consumerDtMs / 1000.0;
        var glowDecayPerSec = 1.0 / (algo.presetGlowDecayMs / 1000.0);
        var glowWidth = algo.presetGlowWidth / 100.0;
        var midSpread = audio.power.mid * glowWidth;
        var minConf = algo.presetMinConfidence / 100.0;

        var pc = -1;
        var excite = 0;
        if (audio.pitch.confidence >= minConf) {
            var rounded = Math.round(audio.pitch.midi);
            pc = ((rounded % 12) + 12) % 12;
            excite = audio.pitch.confidence;
        }

        for (var i = 0; i < 12; i++) {
            var nz = algo.zone[i] - glowDecayPerSec * dt;
            algo.zone[i] = nz < 0 ? 0 : nz;
        }

        if (pc >= 0) {
            if (excite > algo.zone[pc]) algo.zone[pc] = excite;
            var sigma = 0.5 + midSpread;
            var maxD = Math.ceil(sigma * 3);
            if (maxD < 1) maxD = 1;
            for (var d = 1; d <= maxD; d++) {
                var w = Math.exp(-(d * d) / (2 * sigma * sigma));
                var contribution = excite * w;
                var idxL = ((pc - d) % 12 + 12) % 12;
                var idxR = (pc + d) % 12;
                if (contribution > algo.zone[idxL]) algo.zone[idxL] = contribution;
                if (contribution > algo.zone[idxR]) algo.zone[idxR] = contribution;
            }
        }

        var horizontal = (algo.presetAxis === "Horizontal");
        var N = horizontal ? width : height;
        var gradient = (algo.colors && algo.colors.length > 0)
            ? algo.colors : DEFAULT_GRADIENT;
        for (var p = 0; p < N; p++) {
            var zoneIdx;
            if (N >= 12) {
                zoneIdx = Math.floor(p * 12 / N);
                if (zoneIdx > 11) zoneIdx = 11;
            } else {
                var f0 = p * 12 / N;
                var f1 = (p + 1) * 12 / N;
                var lo = Math.floor(f0);
                var hi = Math.floor(f1 - 1e-9);
                if (hi < lo) hi = lo;
                if (hi > 11) hi = 11;
                zoneIdx = lo;
                var bestVal = -1;
                for (var z = lo; z <= hi; z++) {
                    var zi = ((z % 12) + 12) % 12;
                    if (algo.zone[zi] > bestVal) {
                        bestVal = algo.zone[zi];
                        zoneIdx = zi;
                    }
                }
            }
            var t = zoneIdx / 11.0;
            var base = HSVUtil.gradientAt(gradient, t);
            var bright = algo.zone[zoneIdx];
            var ch = base.h;
            var cs = base.s;
            var cv = base.v * bright;
            if (horizontal) {
                for (var y = 0; y < height; y++)
                    HSVUtil.setPixel(map, width, p, y, ch, cs, cv);
            } else {
                for (var x = 0; x < width; x++)
                    HSVUtil.setPixel(map, width, x, p, ch, cs, cv);
            }
        }
        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
