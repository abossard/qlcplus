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

    algo.presetParticleCount = 60;
    algo.properties.push(
      "name:presetParticleCount|type:range|display:Particle Count|" +
      "values:5,400|write:setParticleCount|read:getParticleCount");
    algo.setParticleCount = function(_v) { algo.presetParticleCount = Math.round(HSVUtil.parseNumber(_v, algo.presetParticleCount, 5, 400)); };
    algo.getParticleCount = function() { return algo.presetParticleCount; };

    algo.presetMaxParticles = 200;
    algo.properties.push(
      "name:presetMaxParticles|type:range|display:Max Particles|" +
      "values:20,400|write:setMaxParticles|read:getMaxParticles");
    algo.setMaxParticles = function(_v) { algo.presetMaxParticles = Math.round(HSVUtil.parseNumber(_v, algo.presetMaxParticles, 20, 400)); };
    algo.getMaxParticles = function() { return algo.presetMaxParticles; };

    algo.presetTrailMs = 300;
    algo.properties.push(
      "name:presetTrailMs|type:range|display:Trail Half-life (ms)|" +
      "values:50,2000|write:setTrailMs|read:getTrailMs");
    algo.setTrailMs = function(_v) { algo.presetTrailMs = HSVUtil.parseNumber(_v, algo.presetTrailMs, 50, 2000); };
    algo.getTrailMs = function() { return algo.presetTrailMs; };

    algo.presetBaseSpeed = 30;
    algo.properties.push(
      "name:presetBaseSpeed|type:range|display:Base Speed|" +
      "values:0,200|write:setBaseSpeed|read:getBaseSpeed");
    algo.setBaseSpeed = function(_v) { algo.presetBaseSpeed = HSVUtil.parseNumber(_v, algo.presetBaseSpeed, 0, 200); };
    algo.getBaseSpeed = function() { return algo.presetBaseSpeed; };

    algo.presetHighSpeed = 70;
    algo.properties.push(
      "name:presetHighSpeed|type:range|display:High-Power Boost|" +
      "values:0,300|write:setHighSpeed|read:getHighSpeed");
    algo.setHighSpeed = function(_v) { algo.presetHighSpeed = HSVUtil.parseNumber(_v, algo.presetHighSpeed, 0, 300); };
    algo.getHighSpeed = function() { return algo.presetHighSpeed; };

    algo.presetFieldSpeed = 50;
    algo.properties.push(
      "name:presetFieldSpeed|type:range|display:Field Animation Speed|" +
      "values:0,200|write:setFieldSpeed|read:getFieldSpeed");
    algo.setFieldSpeed = function(_v) { algo.presetFieldSpeed = HSVUtil.parseNumber(_v, algo.presetFieldSpeed, 0, 200); };
    algo.getFieldSpeed = function() { return algo.presetFieldSpeed; };

    algo.presetMorphSpeed = 30;
    algo.properties.push(
      "name:presetMorphSpeed|type:range|display:Field Morph Speed|" +
      "values:0,200|write:setMorphSpeed|read:getMorphSpeed");
    algo.setMorphSpeed = function(_v) { algo.presetMorphSpeed = HSVUtil.parseNumber(_v, algo.presetMorphSpeed, 0, 200); };
    algo.getMorphSpeed = function() { return algo.presetMorphSpeed; };

    algo.presetTurbulence = 100;
    algo.properties.push(
      "name:presetTurbulence|type:range|display:Max Turbulence|" +
      "values:0,300|write:setTurbulence|read:getTurbulence");
    algo.setTurbulence = function(_v) { algo.presetTurbulence = HSVUtil.parseNumber(_v, algo.presetTurbulence, 0, 300); };
    algo.getTurbulence = function() { return algo.presetTurbulence; };

    algo.presetSpawnBurst = 10;
    algo.properties.push(
      "name:presetSpawnBurst|type:range|display:Beat Spawn Burst|" +
      "values:0,50|write:setSpawnBurst|read:getSpawnBurst");
    algo.setSpawnBurst = function(_v) { algo.presetSpawnBurst = Math.round(HSVUtil.parseNumber(_v, algo.presetSpawnBurst, 0, 50)); };
    algo.getSpawnBurst = function() { return algo.presetSpawnBurst; };

    algo.presetLifeMs = 4000;
    algo.properties.push(
      "name:presetLifeMs|type:range|display:Particle Lifetime (ms)|" +
      "values:200,15000|write:setLifeMs|read:getLifeMs");
    algo.setLifeMs = function(_v) { algo.presetLifeMs = HSVUtil.parseNumber(_v, algo.presetLifeMs, 200, 15000); };
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
    algo.setSmoothing = function(_v) { algo.presetSmoothing = Math.round(HSVUtil.parseNumber(_v, algo.presetSmoothing, 1, 10)); };
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

    function spawnParticle(width, height) {
      return {
        x: Math.random() * width,
        y: Math.random() * height,
        ageMs: 0,
        lifeMs: algo.presetLifeMs,
        color: HSVUtil.gradientAt(algo.colors, Math.random())
      };
    }

    function stampLine(width, height, x0, y0, x1, y1, color) {
      HSVUtil.walkLine(Math.floor(x0), Math.floor(y0), Math.floor(x1), Math.floor(y1), function(x, y) {
        HSVUtil.addPixel(algo.fb, width, height, x, y, color.h, color.s, color.v);
      });
    }

    algo.rgbMapStepCount = function(_w, _h) { return 1; };
    algo.rgbMapSetColors = function(_raw) { };
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, _rgb, _step, audio) {
      var dt = HSVUtil.audioSeconds(audio);
      var frames = HSVUtil.elapsedFrames(audio);
      ensureFb(width, height);

      var fieldSpeed01 = algo.presetFieldSpeed / 100.0;
      var morphSpeed01 = algo.presetMorphSpeed / 100.0;
      smoothLow = HSVUtil.smoothFollow(smoothLow, audio.low, algo.presetSmoothing, frames);
      smoothHigh = HSVUtil.smoothFollow(smoothHigh, audio.high, algo.presetSmoothing, frames);
      var fieldScale = fieldSpeed01 * (0.5 + 2.0 * smoothLow);
      algo.fieldRot += dt * audio.mid * fieldSpeed01;
      algo.noiseT += dt * morphSpeed01;

      // Decay framebuffer: only decay value, preserve hue/saturation
      var fade = Math.pow(0.5, dt / (algo.presetTrailMs / 1000.0));
      var pixelCount = width * height;
      for (var di = 0; di < pixelCount; di++) {
        algo.fb[di * 3 + 2] *= fade;
      }

      var maxP = algo.presetMaxParticles;
      var nominal = algo.presetParticleCount;
      if (nominal > maxP) nominal = maxP;

      while (algo.particles.length < nominal) {
        algo.particles.push(spawnParticle(width, height));
      }

      if (audio.beatFired) {
        for (var k = 0; k < algo.presetSpawnBurst; k++) {
          if (algo.particles.length >= maxP) break;
          algo.particles.push(spawnParticle(width, height));
        }
      }

      var turbulence = (smoothHigh / (smoothLow + 0.001)) * algo.presetTurbulence / 100.0;
      var wrap = (algo.presetWrap === "Wrap");
      var baseSpeedNorm = (algo.presetBaseSpeed + audio.high * algo.presetHighSpeed) / 100.0;
      var rotation = algo.fieldRot;
      var noiseT = algo.noiseT;

      for (var p = algo.particles.length - 1; p >= 0; p--) {
        var part = algo.particles[p];
        var prevX = part.x;
        var prevY = part.y;
        var nx = part.x * fieldScale * 0.05 + noiseT;
        var ny = part.y * fieldScale * 0.05 + noiseT;
        var noiseVal = HSVUtil.simplex2d(nx, ny);
        var angle = noiseVal * Math.PI * (1.0 + turbulence) + rotation;
        part.x += Math.cos(angle) * baseSpeedNorm * width * dt;
        part.y += Math.sin(angle) * baseSpeedNorm * height * dt;
        part.ageMs += dt * 1000;

        var wrapped = false;
        if (wrap) {
          if (part.x < 0 || part.x >= width) {
            part.x = HSVUtil.wrap(part.x, width);
            wrapped = true;
          }
          if (part.y < 0 || part.y >= height) {
            part.y = HSVUtil.wrap(part.y, height);
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
          HSVUtil.addPixel(algo.fb, width, height, Math.floor(part.x), Math.floor(part.y),
            part.color.h, part.color.s, part.color.v);
        } else {
          stampLine(width, height, prevX, prevY, part.x, part.y, part.color);
        }
      }

      return new Float32Array(algo.fb);
    };

    testAlgo = algo;
    return algo;
  }
)();
