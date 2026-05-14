/*
  Q Light Controller Plus
  audioflowfield.js

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
    algo.name = "Audio Flow Field";
    algo.author = "QLC+ contributors";
    algo.acceptColors = 5;
    algo.usesAudio = true;
    algo.properties = new Array();

    var DEFAULT_GRADIENT = [
        {h: 0.958, s: 1.0, v: 1.0},
        {h: 0.167, s: 1.0, v: 1.0},
        {h: 0.611, s: 0.749, v: 1.0}
    ];

    algo.presetParticleCount = 60;
    algo.properties.push(
      "name:presetParticleCount|type:range|display:Particle Count|" +
      "values:5,400|write:setParticleCount|read:getParticleCount");
    algo.setParticleCount = function(_v) { algo.presetParticleCount = parseInt(_v); };
    algo.getParticleCount = function() { return algo.presetParticleCount; };

    algo.presetMaxParticles = 200;
    algo.properties.push(
      "name:presetMaxParticles|type:range|display:Max Particles|" +
      "values:20,400|write:setMaxParticles|read:getMaxParticles");
    algo.setMaxParticles = function(_v) { algo.presetMaxParticles = parseInt(_v); };
    algo.getMaxParticles = function() { return algo.presetMaxParticles; };

    algo.presetTrailMs = 300;
    algo.properties.push(
      "name:presetTrailMs|type:range|display:Trail Half-life (ms)|" +
      "values:50,2000|write:setTrailMs|read:getTrailMs");
    algo.setTrailMs = function(_v) { algo.presetTrailMs = parseFloat(_v); };
    algo.getTrailMs = function() { return algo.presetTrailMs; };

    algo.presetBaseSpeed = 30;
    algo.properties.push(
      "name:presetBaseSpeed|type:range|display:Base Speed|" +
      "values:0,200|write:setBaseSpeed|read:getBaseSpeed");
    algo.setBaseSpeed = function(_v) { algo.presetBaseSpeed = parseFloat(_v); };
    algo.getBaseSpeed = function() { return algo.presetBaseSpeed; };

    algo.presetHighSpeed = 70;
    algo.properties.push(
      "name:presetHighSpeed|type:range|display:High-Power Boost|" +
      "values:0,300|write:setHighSpeed|read:getHighSpeed");
    algo.setHighSpeed = function(_v) { algo.presetHighSpeed = parseFloat(_v); };
    algo.getHighSpeed = function() { return algo.presetHighSpeed; };

    algo.presetFieldSpeed = 50;
    algo.properties.push(
      "name:presetFieldSpeed|type:range|display:Field Animation Speed|" +
      "values:0,200|write:setFieldSpeed|read:getFieldSpeed");
    algo.setFieldSpeed = function(_v) { algo.presetFieldSpeed = parseFloat(_v); };
    algo.getFieldSpeed = function() { return algo.presetFieldSpeed; };

    algo.presetMorphSpeed = 30;
    algo.properties.push(
      "name:presetMorphSpeed|type:range|display:Field Morph Speed|" +
      "values:0,200|write:setMorphSpeed|read:getMorphSpeed");
    algo.setMorphSpeed = function(_v) { algo.presetMorphSpeed = parseFloat(_v); };
    algo.getMorphSpeed = function() { return algo.presetMorphSpeed; };

    algo.presetTurbulence = 100;
    algo.properties.push(
      "name:presetTurbulence|type:range|display:Max Turbulence|" +
      "values:0,300|write:setTurbulence|read:getTurbulence");
    algo.setTurbulence = function(_v) { algo.presetTurbulence = parseFloat(_v); };
    algo.getTurbulence = function() { return algo.presetTurbulence; };

    algo.presetSpawnBurst = 10;
    algo.properties.push(
      "name:presetSpawnBurst|type:range|display:Beat Spawn Burst|" +
      "values:0,50|write:setSpawnBurst|read:getSpawnBurst");
    algo.setSpawnBurst = function(_v) { algo.presetSpawnBurst = parseInt(_v); };
    algo.getSpawnBurst = function() { return algo.presetSpawnBurst; };

    algo.presetLifeMs = 4000;
    algo.properties.push(
      "name:presetLifeMs|type:range|display:Particle Lifetime (ms)|" +
      "values:200,15000|write:setLifeMs|read:getLifeMs");
    algo.setLifeMs = function(_v) { algo.presetLifeMs = parseFloat(_v); };
    algo.getLifeMs = function() { return algo.presetLifeMs; };

    algo.presetWrap = "Wrap";
    algo.properties.push(
      "name:presetWrap|type:list|display:Edge Behavior|" +
      "values:Wrap,Kill|write:setWrap|read:getWrap");
    algo.setWrap = function(_v) { algo.presetWrap = _v; };
    algo.getWrap = function() { return algo.presetWrap; };

    algo.presetSmoothing = 5;
    algo.properties.push(
      "name:presetSmoothing|type:range|display:Smoothing|" +
      "values:1,10|write:setSmoothing|read:getSmoothing");
    algo.setSmoothing = function(_v) { algo.presetSmoothing = parseInt(_v); };
    algo.getSmoothing = function() { return algo.presetSmoothing; };

    var smoothLow = 0;
    var smoothHigh = 0;

    algo.particles = [];
    algo.fieldRot = 0;
    algo.noiseT = 0;
    algo.fb = null;
    algo.lastW = -1;
    algo.lastH = -1;

    function ensureFb(width, height) {
      if (algo.fb && algo.lastW === width && algo.lastH === height) return;
      algo.fb = new Float32Array(width * height * 3);
      algo.particles = [];
      algo.lastW = width;
      algo.lastH = height;
    }

    function spawnParticle(width, height, audio) {
      var gradient = (algo.colors && algo.colors.length > 0)
        ? algo.colors : DEFAULT_GRADIENT;
      return {
        x: Math.random() * width,
        y: Math.random() * height,
        prevX: 0,
        prevY: 0,
        vx: 0,
        vy: 0,
        ageMs: 0,
        lifeMs: algo.presetLifeMs,
        color: HSVUtil.gradientAt(gradient, Math.random())
      };
    }

    function stampPixel(width, height, x, y, color) {
      if (x < 0 || x >= width || y < 0 || y >= height) return;
      var i = (y * width + x) * 3;
      var ev = algo.fb[i + 2];
      var nv = Math.min(1, ev + color.v);
      if (color.v > ev) { algo.fb[i] = color.h; algo.fb[i + 1] = color.s; }
      algo.fb[i + 2] = nv;
    }

    function stampLine(width, height, x0, y0, x1, y1, color) {
      var ix0 = Math.round(x0), iy0 = Math.round(y0);
      var ix1 = Math.round(x1), iy1 = Math.round(y1);
      var dx = Math.abs(ix1 - ix0), sx = ix0 < ix1 ? 1 : -1;
      var dy = -Math.abs(iy1 - iy0), sy = iy0 < iy1 ? 1 : -1;
      var err = dx + dy;
      var guard = dx - dy + 2;
      while (true) {
        stampPixel(width, height, ix0, iy0, color);
        if (ix0 === ix1 && iy0 === iy1) break;
        if (--guard < 0) break;
        var e2 = 2 * err;
        if (e2 >= dy) { err += dy; ix0 += sx; }
        if (e2 <= dx) { err += dx; iy0 += sy; }
      }
    }

    algo.rgbMapStepCount = function(_w, _h) { return 1; };
    algo.rgbMapSetColors = function(_raw) { };
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, _rgb, _step, audio) {
      var dt = audio.dt * 60.0 / audio.bpm;
      ensureFb(width, height);

      var fieldSpeed01 = algo.presetFieldSpeed / 100.0;
      var morphSpeed01 = algo.presetMorphSpeed / 100.0;
      // Asymmetric EMA on low and high bands (fast attack, slow decay)
      var smoothing = algo.presetSmoothing / 10.0;
      var riseAlpha = 0.5 * (1 - smoothing) + 0.05;
      var decayAlpha = 0.02 + 0.03 * (1 - smoothing);
      smoothLow += (audio.low > smoothLow ? riseAlpha : decayAlpha) * (audio.low - smoothLow);
      smoothHigh += (audio.high > smoothHigh ? riseAlpha : decayAlpha) * (audio.high - smoothHigh);
      var fieldScale = fieldSpeed01 * (0.5 + 2.0 * smoothLow);
      algo.fieldRot += dt * audio.mid * fieldSpeed01;
      algo.noiseT += dt * morphSpeed01;

      // Decay framebuffer: only decay value, preserve hue/saturation
      var fade = Math.exp(-dt / (algo.presetTrailMs / 1000.0));
      var pixelCount = width * height;
      for (var di = 0; di < pixelCount; di++) {
        algo.fb[di * 3 + 2] *= fade;
      }

      var maxP = algo.presetMaxParticles;
      var nominal = algo.presetParticleCount;
      if (nominal > maxP) nominal = maxP;

      while (algo.particles.length < nominal) {
        algo.particles.push(spawnParticle(width, height, audio));
      }

      if (audio.beatFired) {
        for (var k = 0; k < algo.presetSpawnBurst; k++) {
          if (algo.particles.length >= maxP) break;
          algo.particles.push(spawnParticle(width, height, audio));
        }
      }

      var turbulence = (smoothHigh / (smoothLow + 0.001)) * algo.presetTurbulence / 100.0;
      var wrap = (algo.presetWrap === "Wrap");
      var baseSpeedNorm = (algo.presetBaseSpeed + audio.high * algo.presetHighSpeed) / 100.0;
      var rotation = algo.fieldRot;
      var noiseT = algo.noiseT;

      for (var p = algo.particles.length - 1; p >= 0; p--) {
        var part = algo.particles[p];
        part.prevX = part.x;
        part.prevY = part.y;
        var nx = part.x * fieldScale * 0.05 + noiseT;
        var ny = part.y * fieldScale * 0.05 + noiseT;
        var noiseVal = HSVUtil.simplex2d(nx, ny);
        var angle = noiseVal * Math.PI * (1.0 + turbulence) + rotation;
        part.vx = Math.cos(angle) * baseSpeedNorm * width;
        part.vy = Math.sin(angle) * baseSpeedNorm * height;
        part.x += part.vx * dt;
        part.y += part.vy * dt;
        part.ageMs += (audio.dt * 60000 / audio.bpm);

        var wrapped = false;
        if (wrap) {
          if (part.x < 0 || part.x >= width) {
            part.x = ((part.x % width) + width) % width;
            wrapped = true;
          }
          if (part.y < 0 || part.y >= height) {
            part.y = ((part.y % height) + height) % height;
            wrapped = true;
          }
        } else {
          if (part.x < 0 || part.x >= width || part.y < 0 || part.y >= height) {
            algo.particles.splice(p, 1);
            continue;
          }
        }

        // Cull aged particles in BOTH wrap and kill modes.
        if (part.ageMs > part.lifeMs) {
          algo.particles.splice(p, 1);
          continue;
        }

        if (wrapped) {
          // Don't draw a line crossing the matrix; just stamp the new point.
          stampPixel(width, height, Math.round(part.x), Math.round(part.y), part.color);
        } else {
          stampLine(width, height, part.prevX, part.prevY, part.x, part.y, part.color);
        }
      }

      var map = HSVUtil.createMap(width, height);
      var total = width * height * 3;
      for (var ci = 0; ci < total; ci++) {
        map[ci] = algo.fb[ci];
      }
      return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
