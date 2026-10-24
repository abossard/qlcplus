/*
  Q Light Controller Plus
  audioreaction.js

  Copyright (c) QLC+ contributors

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

// Gray-Scott reaction-diffusion. Best on 10x10 to 30x30 panels. On 4x80
// (or other very thin matrices) the pattern degenerates to bands but is
// still visually interesting. presetStepsPerFrame controls how many PDE
// sub-steps run per render frame: more steps = faster pattern evolution.

var testAlgo;

(
  function () {
    var algo = new Object;
    algo.apiVersion = 3;
    algo.name = "Audio Reaction-Diffusion";
    algo.author = "QLC+ contributors";
    algo.acceptColors = 5;
    algo.usesAudio = true;
    algo.properties = new Array();

    // Gray-Scott safe parameter ranges.
    var F_MIN = 0.01, F_MAX = 0.10;
    var K_MIN = 0.04, K_MAX = 0.07;
    var DU_MIN = 0.10, DU_MAX = 0.30;
    var DV_MIN = 0.05, DV_MAX = 0.15;
    // Simulated time per render is capped, so a stalled render does not run thousands of steps.
    var MAX_SECONDS_PER_RENDER = 0.25;

    algo.presetFeedBase = 30;
    algo.properties.push(
      "name:presetFeedBase|type:range|display:Feed Base (x1e-3)|" +
      "values:10,80|write:setFeedBase|read:getFeedBase");
    algo.setFeedBase = function(_v) { algo.presetFeedBase = HSVUtil.parseNumber(_v, algo.presetFeedBase, 10, 80); };
    algo.getFeedBase = function() { return algo.presetFeedBase; };

    algo.presetKillBase = 55;
    algo.properties.push(
      "name:presetKillBase|type:range|display:Kill Base (x1e-3)|" +
      "values:40,70|write:setKillBase|read:getKillBase");
    algo.setKillBase = function(_v) { algo.presetKillBase = HSVUtil.parseNumber(_v, algo.presetKillBase, 40, 70); };
    algo.getKillBase = function() { return algo.presetKillBase; };

    algo.presetDu = 16;
    algo.properties.push(
      "name:presetDu|type:range|display:Diffusion U (x1e-2)|" +
      "values:10,30|write:setDu|read:getDu");
    algo.setDu = function(_v) { algo.presetDu = HSVUtil.parseNumber(_v, algo.presetDu, 10, 30); };
    algo.getDu = function() { return algo.presetDu; };

    algo.presetDv = 8;
    algo.properties.push(
      "name:presetDv|type:range|display:Diffusion V (x1e-2)|" +
      "values:5,15|write:setDv|read:getDv");
    algo.setDv = function(_v) { algo.presetDv = HSVUtil.parseNumber(_v, algo.presetDv, 5, 15); };
    algo.getDv = function() { return algo.presetDv; };

    algo.presetSimDt = 90;
    algo.properties.push(
      "name:presetSimDt|type:range|display:Sim dt (x1e-2)|" +
      "values:10,100|write:setSimDt|read:getSimDt");
    algo.setSimDt = function(_v) { algo.presetSimDt = HSVUtil.parseNumber(_v, algo.presetSimDt, 10, 100); };
    algo.getSimDt = function() { return algo.presetSimDt; };

    algo.presetStepsPerFrame = 4;
    algo.properties.push(
      "name:presetStepsPerFrame|type:range|display:Sim Steps per Frame|" +
      "values:1,12|write:setStepsPerFrame|read:getStepsPerFrame");
    algo.setStepsPerFrame = function(_v) { algo.presetStepsPerFrame = Math.round(HSVUtil.parseNumber(_v, algo.presetStepsPerFrame, 1, 12)); };
    algo.getStepsPerFrame = function() { return algo.presetStepsPerFrame; };

    algo.presetSeedSize = 2;
    algo.properties.push(
      "name:presetSeedSize|type:range|display:Kick Seed Size (px)|" +
      "values:1,6|write:setSeedSize|read:getSeedSize");
    algo.setSeedSize = function(_v) { algo.presetSeedSize = HSVUtil.parseNumber(_v, algo.presetSeedSize, 1, 6); };
    algo.getSeedSize = function() { return algo.presetSeedSize; };

    algo.presetFlatnessModulation = 40;
    algo.properties.push(
      "name:presetFlatnessModulation|type:range|display:Flatness->F Mod (x1e-3)|" +
      "values:0,90|write:setFlatnessMod|read:getFlatnessMod");
    algo.setFlatnessMod = function(_v) { algo.presetFlatnessModulation = HSVUtil.parseNumber(_v, algo.presetFlatnessModulation, 0, 90); };
    algo.getFlatnessMod = function() { return algo.presetFlatnessModulation; };

    algo.presetMidModulation = 20;
    algo.properties.push(
      "name:presetMidModulation|type:range|display:Mid->k Mod (x1e-3)|" +
      "values:0,30|write:setMidMod|read:getMidMod");
    algo.setMidMod = function(_v) { algo.presetMidModulation = HSVUtil.parseNumber(_v, algo.presetMidModulation, 0, 30); };
    algo.getMidMod = function() { return algo.presetMidModulation; };

    algo.presetHighModulation = 50;
    algo.properties.push(
      "name:presetHighModulation|type:range|display:High->Dv Mod (%)|" +
      "values:0,200|write:setHighMod|read:getHighMod");
    algo.setHighMod = function(_v) { algo.presetHighModulation = HSVUtil.parseNumber(_v, algo.presetHighModulation, 0, 200); };
    algo.getHighMod = function() { return algo.presetHighModulation; };

    algo.presetGain = 200;
    algo.properties.push(
      "name:presetGain|type:range|display:Render Gain (%)|" +
      "values:50,800|write:setGain|read:getGain");
    algo.setGain = function(_v) { algo.presetGain = HSVUtil.parseNumber(_v, algo.presetGain, 50, 800); };
    algo.getGain = function() { return algo.presetGain; };

    algo.U = null;
    algo.V = null;
    algo.Unext = null;
    algo.Vnext = null;
    algo.lastW = -1;
    algo.lastH = -1;

    function ensureGrids(width, height) {
      if (algo.U && algo.lastW === width && algo.lastH === height) return;
      var n = width * height;
      algo.U = new Float32Array(n);
      algo.V = new Float32Array(n);
      algo.Unext = new Float32Array(n);
      algo.Vnext = new Float32Array(n);
      for (var i = 0; i < n; i++) { algo.U[i] = 1.0; algo.V[i] = 0.0; }
      // Central seed of V.
      var cx = Math.floor(width / 2);
      var cy = Math.floor(height / 2);
      var seed = Math.max(1, algo.presetSeedSize);
      seedCluster(width, height, cx, cy, seed);
      algo.lastW = width;
      algo.lastH = height;
    }

    function seedCluster(width, height, cx, cy, size) {
      for (var dy = -size; dy <= size; dy++) {
        var yy = ((cy + dy) % height + height) % height;
        for (var dx = -size; dx <= size; dx++) {
          var xx = ((cx + dx) % width + width) % width;
          var idx = yy * width + xx;
          algo.U[idx] = 0.5;
          algo.V[idx] = 0.25;
        }
      }
    }

    algo.rgbMapStepCount = function(_w, _h) { return 1; };
    algo.rgbMapSetColors = function(_raw) { };
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, _rgb, _step, audio) {
      ensureGrids(width, height);

      // Audio-driven parameters, hard-clamped to Gray-Scott safe regimes.
      var F = Math.max(F_MIN, Math.min(F_MAX, algo.presetFeedBase / 1000.0
          + (audio.high / (audio.low + 0.001)) * (algo.presetFlatnessModulation / 1000.0)));
      var k = Math.max(K_MIN, Math.min(K_MAX, algo.presetKillBase / 1000.0
          + audio.mid * (algo.presetMidModulation / 1000.0)));
      var Du = Math.max(DU_MIN, Math.min(DU_MAX, algo.presetDu / 100.0));
      var DvBase = algo.presetDv / 100.0;
      var DvMod = audio.high * (algo.presetHighModulation / 100.0);
      var Dv = Math.max(DV_MIN, Math.min(DV_MAX, DvBase * (1.0 + DvMod)));

      // Canonical timing: derive simulation budget from engine dt.
      var dt = Math.min(MAX_SECONDS_PER_RENDER, audio.dt * 60.0 / audio.bpm);
      var simDt = algo.presetSimDt / 100.0;
      var steps = Math.max(1, Math.round(algo.presetStepsPerFrame * dt * 25));

      if (audio.beatFired) {
        var sx = Math.floor(Math.random() * width);
        var sy = Math.floor(Math.random() * height);
        seedCluster(width, height, sx, sy, Math.max(1, algo.presetSeedSize));
      }

      var W = width;
      var H = height;
      for (var s = 0; s < steps; s++) {
        var U = algo.U, V = algo.V;
        var Un = algo.Unext, Vn = algo.Vnext;
        for (var y = 0; y < H; y++) {
          var ym = (y - 1 + H) % H;
          var yp = (y + 1) % H;
          for (var x = 0; x < W; x++) {
            var xm = (x - 1 + W) % W;
            var xp = (x + 1) % W;
            var i = y * W + x;
            var u = U[i], v = V[i];
            var lapU = U[y * W + xm] + U[y * W + xp]
                     + U[ym * W + x] + U[yp * W + x] - 4 * u;
            var lapV = V[y * W + xm] + V[y * W + xp]
                     + V[ym * W + x] + V[yp * W + x] - 4 * v;
            var uvv = u * v * v;
            // Simulation-state bounding (NOT audio clamping): prevents PDE
            // numerical blowup, as in standard Gray-Scott implementations.
            var nU = u + simDt * (Du * lapU - uvv + F * (1 - u));
            var nV = v + simDt * (Dv * lapV + uvv - (F + k) * v);
            if (nU < 0) nU = 0; else if (nU > 1) nU = 1;
            if (nV < 0) nV = 0; else if (nV > 1) nV = 1;
            Un[i] = nU;
            Vn[i] = nV;
          }
        }
        algo.U = Un; algo.V = Vn;
        algo.Unext = U; algo.Vnext = V;
      }

      var gain = algo.presetGain / 100.0;

      var map = HSVUtil.createMap(width, height);
      for (var p = 0; p < W * H; p++) {
        var c = HSVUtil.gradientAt(algo.colors, HSVUtil.clamp01(algo.V[p] * gain));
        map[p * 3] = c.h;
        map[p * 3 + 1] = c.s;
        map[p * 3 + 2] = c.v;
      }
      return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
