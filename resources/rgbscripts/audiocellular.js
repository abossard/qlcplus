/*
  Q Light Controller Plus
  audiocellular.js

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
    algo.name = "Audio Cellular";
    algo.author = "QLC+ contributors";
    algo.acceptColors = 5;
    algo.usesAudio = true;
    algo.properties = new Array();

    var DEFAULT_GRADIENT = [0xFF0040, 0xFFFF00, 0x4080FF];

    algo.presetRules = "30,90,110,150";
    algo.properties.push(
      "name:presetRules|type:list|display:Rule Pool|" +
      "values:30,90,110,150;30,30,30,30;All|write:setRules|read:getRules");
    algo.setRules = function(_v) {
      algo.presetRules = _v;
      algo.rulePoolCache = null;
      algo.rulePoolKey = null;
    };
    algo.getRules = function() { return algo.presetRules; };

    algo.presetSeedMode = "Center";
    algo.properties.push(
      "name:presetSeedMode|type:list|display:Seed Mode|" +
      "values:Center,Bass,Random|write:setSeedMode|read:getSeedMode");
    algo.setSeedMode = function(_v) { algo.presetSeedMode = _v; };
    algo.getSeedMode = function() { return algo.presetSeedMode; };

    algo.presetSeedThreshold = 60;
    algo.properties.push(
      "name:presetSeedThreshold|type:range|display:Seed Threshold (%)|" +
      "values:0,100|write:setSeedThreshold|read:getSeedThreshold");
    algo.setSeedThreshold = function(_v) { algo.presetSeedThreshold = parseInt(_v); };
    algo.getSeedThreshold = function() { return algo.presetSeedThreshold; };

    algo.presetScrollHz = 4.0;
    algo.properties.push(
      "name:presetScrollHz|type:float|display:Scroll Rate (rows/beat)|" +
      "write:setScrollHz|read:getScrollHz");
    algo.setScrollHz = function(_v) { algo.presetScrollHz = parseFloat(_v); };
    algo.getScrollHz = function() { return algo.presetScrollHz; };

    algo.history = null;
    algo.scratch = null;
    algo.row = 0;
    algo.rule = 30;
    algo.ruleIdx = 0;
    algo.scrollAccum = 0;
    algo.lastW = -1;
    algo.lastH = -1;
    algo.rulePoolCache = null;
    algo.rulePoolKey = null;

    function parseRulePool(spec) {
      if (spec === "All") {
        var arr = new Array(256);
        for (var i = 0; i < 256; i++) arr[i] = i;
        return arr;
      }
      var parts = spec.split(",");
      var out = [];
      for (var p = 0; p < parts.length; p++) {
        var n = parseInt(parts[p]);
        if (!isNaN(n)) out.push(((n % 256) + 256) % 256);
      }
      if (out.length === 0) out.push(30);
      return out;
    }

    function getRulePool() {
      if (algo.rulePoolCache && algo.rulePoolKey === algo.presetRules)
        return algo.rulePoolCache;
      algo.rulePoolCache = parseRulePool(algo.presetRules);
      algo.rulePoolKey = algo.presetRules;
      return algo.rulePoolCache;
    }

    function ensureHistory(width, height) {
      if (algo.history && algo.lastW === width && algo.lastH === height) return;
      algo.history = new Uint8Array(width * height);
      algo.scratch = new Uint8Array(width);
      algo.row = 0;
      algo.scrollAccum = 0;
      algo.lastW = width;
      algo.lastH = height;
      seedRow(width);
    }

    function seedRow(N) {
      var base = algo.row * N;
      for (var i = 0; i < N; i++) algo.history[base + i] = 0;
      algo.history[base + Math.floor(N / 2)] = 1;
    }

    algo.rgbMapStepCount = function(_w, _h) { return 1; };
    algo.rgbMapSetColors = function(_raw) { };
    algo.rgbMapGetColors = function() {
      return (algo.gradientBandColors && algo.gradientBandColors.length >= 3)
        ? algo.gradientBandColors.slice() : DEFAULT_GRADIENT.slice();
    };

    algo.rgbMap = function(width, height, _rgb, _step, audio) {
      var dt = audio.timing.consumerDtMs / 1000.0;
      ensureHistory(width, height);
      var N = width;
      var H = height;
      var rulePool = getRulePool();

      if (audio.beat.fired) {
        algo.ruleIdx = (algo.ruleIdx + 1) % rulePool.length;
        algo.rule = rulePool[algo.ruleIdx];
      }

      if (audio.bar.downbeatFired) {
        for (var i = 0; i < algo.history.length; i++) algo.history[i] = 0;
        algo.row = 0;
        seedRow(N);
      }

      if (audio.power.low > algo.presetSeedThreshold / 100.0) {
        var seedX;
        if (algo.presetSeedMode === "Center") {
          seedX = Math.floor(N / 2);
        } else if (algo.presetSeedMode === "Bass") {
          seedX = Math.min(N - 1, Math.floor(N * audio.power.low));
        } else {
          seedX = Math.floor(Math.random() * N);
        }
        if (seedX < 0) seedX = 0; else if (seedX > N - 1) seedX = N - 1;
        algo.history[algo.row * N + seedX] = 1;
      }

      var bpm = (audio.beat && audio.beat.bpm > 0) ? audio.beat.bpm : 120;
      var beatsPerSec = bpm / 60.0;
      algo.scrollAccum += dt * algo.presetScrollHz * beatsPerSec;
      var rule = algo.rule;
      while (algo.scrollAccum >= 1.0) {
        algo.scrollAccum -= 1.0;
        var srcRow = algo.row;
        var srcBase = srcRow * N;
        if (H === 1) {
          var scratch = algo.scratch;
          for (var i2 = 0; i2 < N; i2++) {
            var l = algo.history[srcBase + ((i2 - 1 + N) % N)];
            var c = algo.history[srcBase + i2];
            var r = algo.history[srcBase + ((i2 + 1) % N)];
            var pat = (l << 2) | (c << 1) | r;
            scratch[i2] = (rule >> pat) & 1;
          }
          for (var i3 = 0; i3 < N; i3++) algo.history[i3] = scratch[i3];
        } else {
          var nextRow = (srcRow + 1) % H;
          var nextBase = nextRow * N;
          for (var i4 = 0; i4 < N; i4++) {
            var l2 = algo.history[srcBase + ((i4 - 1 + N) % N)];
            var c2 = algo.history[srcBase + i4];
            var r2 = algo.history[srcBase + ((i4 + 1) % N)];
            var pat2 = (l2 << 2) | (c2 << 1) | r2;
            algo.history[nextBase + i4] = (rule >> pat2) & 1;
          }
          algo.row = nextRow;
        }
      }

      var gradient = (audio.colors && audio.colors.gradient && audio.colors.gradient.length > 0)
        ? audio.colors.gradient : DEFAULT_GRADIENT;

      var map = RGBUtil.createMap(width, height);
      for (var y = 0; y < H; y++) {
        var src = (algo.row - (H - 1 - y) + H) % H;
        var srcBase2 = src * N;
        var age = (H - 1) > 0 ? (H - 1 - y) / (H - 1) : 0;
        var rowColor = RGBUtil.gradientColorAt(gradient, age);
        for (var x = 0; x < N; x++) {
          map[y][x] = algo.history[srcBase2 + x] ? rowColor : 0;
        }
      }

      return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
