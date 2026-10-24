/*
  Q Light Controller Plus
  audiovortex.js

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
    algo.name = "Audio Vortex";
    algo.author = "QLC+ contributors";
    algo.acceptColors = 3;
    algo.usesAudio = true;
    algo.properties = new Array();

    algo.presetReactivity = 1.67;
    algo.properties.push(
      "name:presetReactivity|type:float|display:Reactivity|" +
      "write:setReactivity|read:getReactivity");
    algo.presetSpeed = 0.5;
    algo.properties.push(
      "name:presetSpeed|type:float|display:Speed (cyc/beat)|" +
      "write:setSpeed|read:getSpeed");
    algo.presetArms = 3;
    algo.properties.push(
      "name:presetArms|type:range|display:Spiral Arms|" +
      "values:1,8|write:setArms|read:getArms");
    algo.presetTightness = 1.67;
    algo.properties.push(
      "name:presetTightness|type:float|display:Tightness|" +
      "write:setTightness|read:getTightness");

    algo.setSpeed = function(_v) { algo.presetSpeed = HSVUtil.parseNumber(_v, algo.presetSpeed, -10, 10); };
    algo.getSpeed = function() { return algo.presetSpeed; };
    algo.setArms = function(_v) { algo.presetArms = Math.round(HSVUtil.parseNumber(_v, algo.presetArms, 1, 8)); };
    algo.getArms = function() { return algo.presetArms; };
    algo.setTightness = function(_v) { algo.presetTightness = HSVUtil.parseNumber(_v, algo.presetTightness, -10, 10); };
    algo.getTightness = function() { return algo.presetTightness; };

    algo.setReactivity = function(_v) { algo.presetReactivity = HSVUtil.parseNumber(_v, algo.presetReactivity, 0, 10); };
    algo.getReactivity = function() { return algo.presetReactivity; };

    algo.presetSmoothing = 5;
    algo.properties.push(
      "name:presetSmoothing|type:range|display:Smoothing|" +
      "values:1,10|write:setSmoothing|read:getSmoothing");
    algo.setSmoothing = function(_v) { algo.presetSmoothing = Math.round(HSVUtil.parseNumber(_v, algo.presetSmoothing, 1, 10)); };
    algo.getSmoothing = function() { return algo.presetSmoothing; };

    var BEAT_PULSE_AMP = 0.15;
    var SPIRAL_FREQ = 0.3;

    // --- State (audiotunnel-style envelope) ---
    var phase = 0;           // spiral rotation phase (0-1)
    var energy = 0;          // smoothed overall energy
    var envelope = {bar: 0, flash: 0};  // bar build-up, released as a flash on the downbeat

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var map = HSVUtil.createMap(width, height);
        if (!audio) return map;

        var dt = audio.dt;
        var frames = HSVUtil.elapsedFrames(audio);
        var reactivity = algo.presetReactivity;

        // --- Envelope followers (asymmetric EMA, fast attack / slow decay) ---
        var rawEnergy = (audio.low + audio.mid * 0.5 + audio.high * 0.3) / 1.8;
        energy = HSVUtil.smoothFollow(energy, rawEnergy, algo.presetSmoothing, frames);

        // --- Bar-level build-up, released as a burst on the downbeat ---
        HSVUtil.barEnvelope(envelope, rawEnergy, dt, audio.downbeat, Math.pow(0.85, frames));

        // --- Phase accumulation ---
        var speedMod = 1 + energy * reactivity;
        phase = (phase + dt * algo.presetSpeed * speedMod) % 1.0;
        var angle = phase * Math.PI * 2;

        var cx = width / 2;
        var cy = height / 2;
        var maxDist = Math.sqrt(cx * cx + cy * cy);
        var arms = algo.presetArms;
        var tightness = algo.presetTightness;

        // --- Colors: user's 3-color gradient, sampled by distance ---
        var colors = algo.colors;

        // Gentle beat pulse rides the smoothed envelope
        var beatGlow = audio.cosPulse * BEAT_PULSE_AMP * energy;

        for (var y = 0; y < height; y++) {
            for (var x = 0; x < width; x++) {
                var dx = x - cx + 0.5;
                var dy = y - cy + 0.5;
                var dist = Math.sqrt(dx * dx + dy * dy);
                var normDist = dist / maxDist;

                var pixAngle = Math.atan2(dy, dx);
                var spiral = pixAngle + dist * tightness * SPIRAL_FREQ - angle;
                var armVal = Math.sin(spiral * arms) * 0.5 + 0.5;

                // Per-pixel color from user's stops by radial position
                var color = HSVUtil.gradientAt(colors, normDist);

                // Brightness: smooth energy + gentle beat + release burst
                var bright = armVal * energy * (1 + beatGlow) + envelope.flash * (1 - normDist);
                bright = Math.min(1, bright);

                var i3 = (y * width + x) * 3;
                map[i3] = color.h;
                map[i3 + 1] = color.s;
                map[i3 + 2] = color.v * bright;
            }
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
