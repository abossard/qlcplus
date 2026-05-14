/*
  Q Light Controller Plus
  audiotunnel.js

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
    algo.name = "Audio Tunnel";
    algo.author = "QLC+ contributors";
    algo.acceptColors = 3;
    algo.usesAudio = true;
    algo.properties = new Array();

    algo.presetSpeed = 0.5;
    algo.properties.push(
      "name:presetSpeed|type:float|display:Speed (cyc/beat)|" +
      "write:setSpeed|read:getSpeed");
    algo.presetRings = 5;
    algo.properties.push(
      "name:presetRings|type:range|display:Ring Count|" +
      "values:2,10|write:setRings|read:getRings");
    algo.presetShape = 0;
    algo.properties.push(
      "name:presetShape|type:list|display:Shape|" +
      "values:Circle,Diamond,Square|write:setShape|read:getShape");
    algo.presetReactivity = 1.5;
    algo.properties.push(
      "name:presetReactivity|type:float|display:Reactivity|" +
      "write:setReactivity|read:getReactivity");

    algo.setSpeed = function(_v) { algo.presetSpeed = parseFloat(_v); };
    algo.getSpeed = function() { return algo.presetSpeed; };
    algo.setRings = function(_v) { algo.presetRings = parseInt(_v); };
    algo.getRings = function() { return algo.presetRings; };
    algo.setShape = function(_v) {
        if (_v === "Diamond") algo.presetShape = 1;
        else if (_v === "Square") algo.presetShape = 2;
        else algo.presetShape = 0;
    };
    algo.getShape = function() {
        return ["Circle", "Diamond", "Square"][algo.presetShape];
    };
    algo.setReactivity = function(_v) { algo.presetReactivity = parseFloat(_v); };
    algo.getReactivity = function() { return algo.presetReactivity; };
    algo.presetSmoothing = 5;
    algo.properties.push(
      "name:presetSmoothing|type:range|display:Smoothing|" +
      "values:1,10|write:setSmoothing|read:getSmoothing");
    algo.setSmoothing = function(_v) { algo.presetSmoothing = parseInt(_v); };
    algo.getSmoothing = function() { return algo.presetSmoothing; };

    // --- State ---
    var phase = 0;           // ring scroll phase (0-1, beat-locked)
    var energy = 0;          // smoothed overall energy (slow rise, slow decay)
    var barEnergy = 0;       // energy accumulated within the current bar
    var peakEnergy = 0;      // peak reached during build-up, released on downbeat
    var releaseFlash = 0;    // burst brightness on downbeat, decays quickly

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var map = HSVUtil.createMap(width, height);
        if (!audio) return map;

        var dt = audio.dt;
        var reactivity = algo.presetReactivity;

        // --- Envelope followers (smooth, stateful) ---
        var rawEnergy = (audio.low + audio.mid * 0.5 + audio.high * 0.3) / 1.8;
        var smoothing = algo.presetSmoothing / 10.0;
        var riseAlpha = 0.5 * (1 - smoothing) + 0.05;
        var decayAlpha = 0.02 + 0.03 * (1 - smoothing);
        energy += (rawEnergy > energy ? riseAlpha : decayAlpha) * (rawEnergy - energy);

        // --- Bar-level build-up ---
        barEnergy += rawEnergy * dt;
        if (barEnergy > peakEnergy) peakEnergy = barEnergy;

        // --- Downbeat: release + reset ---
        if (audio.downbeat)
        {
            releaseFlash = Math.min(1, peakEnergy * 0.5);
            barEnergy = 0;
            peakEnergy = 0;
        }

        // Release flash decays over ~8 frames
        releaseFlash *= 0.85;

        // --- Phase accumulation (flow) ---
        var speedMod = 1 + energy * reactivity;
        phase = (phase + dt * algo.presetSpeed * speedMod) % 1.0;

        // --- Colors: user's 3-color gradient, sampled by ring distance ---
        var colors = (algo.colors && algo.colors.length >= 3) ? algo.colors
            : [{h:0.042,s:1,v:1},{h:0.399,s:1,v:1},{h:0.611,s:0.749,v:1}];

        // Beat pulse: gentle, rides the envelope
        var beatGlow = audio.cosPulse * 0.15 * energy;

        // --- Render rings ---
        var cx = width / 2;
        var cy = height / 2;
        var maxDist = Math.sqrt(cx * cx + cy * cy);
        var ringCount = algo.presetRings;

        for (var y = 0; y < height; y++)
        {
            for (var x = 0; x < width; x++)
            {
                var dx = x - cx + 0.5;
                var dy = y - cy + 0.5;

                var dist;
                if (algo.presetShape === 1)
                    dist = Math.abs(dx) + Math.abs(dy);
                else if (algo.presetShape === 2)
                    dist = Math.max(Math.abs(dx), Math.abs(dy));
                else
                    dist = Math.sqrt(dx * dx + dy * dy);

                var normDist = dist / maxDist;
                var ringPhase = (normDist * ringCount - phase) % 1;
                ringPhase = ((ringPhase % 1) + 1) % 1;
                var ringVal = Math.sin(ringPhase * Math.PI * 2) * 0.5 + 0.5;

                // Color: walk through user's 3 colors by distance from center
                var t = normDist * 2; // 0→2 across the 3 stops
                var ci = Math.min(1, Math.floor(t));
                var cf = t - ci;
                var ca = colors[ci];
                var cb = colors[ci + 1] || colors[ci];
                var h = ca.h + (cb.h - ca.h) * cf;
                var s = ca.s + (cb.s - ca.s) * cf;
                var baseV = ca.v + (cb.v - ca.v) * cf;

                // Brightness: smooth energy envelope + gentle beat + release burst
                var bright = ringVal * energy * (1 + beatGlow) + releaseFlash * (1 - normDist);
                bright = Math.min(1, bright);

                var i3 = (y * width + x) * 3;
                map[i3] = h;
                map[i3 + 1] = s;
                map[i3 + 2] = baseV * bright;
            }
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
