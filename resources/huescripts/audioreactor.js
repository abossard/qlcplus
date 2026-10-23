/*
  Q Light Controller Plus
  audioreactor.js

  Kitchen-sink audio demo: dominant-band scenes, onset flash, beat-driven
  sparkles, optional pitch-hue palette. Each audio feature drives a single
  visual behavior (no stacking).

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
    algo.name = "Audio Reactor";
    algo.author = "QLC+ contributors";
    algo.acceptColors = 3; // low/mid/high band palette
    algo.usesAudio = true;
    algo.properties = new Array();

    algo.presetPalette = 0;
    algo.properties.push(
      "name:presetPalette|type:list|display:Palette|" +
      "values:Band Colors,Pitch Hue|write:setPalette|read:getPalette");

    algo.presetSensitivity = 1.0;
    algo.properties.push(
      "name:presetSensitivity|type:float|display:Sensitivity|" +
      "write:setSensitivity|read:getSensitivity");

    algo.presetFlash = 0.8;
    algo.properties.push(
      "name:presetFlash|type:float|display:Onset Flash|" +
      "write:setFlash|read:getFlash");

    algo.presetSparkles = 1;
    algo.properties.push(
      "name:presetSparkles|type:list|display:High Sparkles|" +
      "values:Off,On|write:setSparkles|read:getSparkles");

    algo.setPalette = function(_v) { algo.presetPalette = (_v === "Pitch Hue") ? 1 : 0; };
    algo.getPalette = function() { return algo.presetPalette ? "Pitch Hue" : "Band Colors"; };
    algo.setSensitivity = function(_v) { algo.presetSensitivity = parseFloat(_v); };
    algo.getSensitivity = function() { return algo.presetSensitivity; };
    algo.setFlash = function(_v) { algo.presetFlash = parseFloat(_v); };
    algo.getFlash = function() { return algo.presetFlash; };
    algo.setSparkles = function(_v) { algo.presetSparkles = (_v === "On") ? 1 : 0; };
    algo.getSparkles = function() { return algo.presetSparkles ? "On" : "Off"; };

    algo.presetSmoothing = 5;
    algo.properties.push(
      "name:presetSmoothing|type:range|display:Smoothing|" +
      "values:1,10|write:setSmoothing|read:getSmoothing");
    algo.setSmoothing = function(_v) { algo.presetSmoothing = parseInt(_v); };
    algo.getSmoothing = function() { return algo.presetSmoothing; };

    var smoothPow = [0, 0, 0];

    // Bar-level build-up / release state
    var barState = {bar: 0, flash: 0};

    // Default band palette, pre-expressed in HSV: red-ish, green-ish, blue-ish.
    var DEFAULT_BAND_HSV = [
        { h: 0.972, s: 0.875, v: 1.0 }, // ~0xFF2040
        { h: 0.389, s: 0.875, v: 1.0 }, // ~0x20FF80
        { h: 0.583, s: 0.498, v: 1.0 }  // ~0x80C0FF
    ];
    var FLASH_DECAY = 0.82;
    var SPARKLE_DECAY = 0.70;
    var SPARKLE_DENSITY = 0.10;
    var BASE_OVERALL = 0.35;
    // White sparkle in HSV: saturation 0 means hue is irrelevant.
    var SPARKLE_HSV = { h: 0, s: 0, v: 1 };

    // BPM-scaled wall clock (one unit per beat). No audio in time scale.
    var time = 0;
    var flash = 0;
    var sparkleLevel = 0;

    // Reusable sparkle bitmap (Array of 0/1) — preallocated and reused across
    // frames to avoid GC and the QV4 numeric-key→string conversion that an
    // object literal `{}` would trigger when indexed with `y * width + x`.
    var sparkleBitmap = null;

    // Resolve a band color as {h,s,v}. Uses power-band proportions for palette selection.
    function colorFor(audio, bandIndex) {
        if (algo.presetPalette) {
            // Use band proportions for hue generation
            var powers = [audio.low, audio.mid, audio.high];
            var total = powers[0] + powers[1] + powers[2] + 0.001;
            var hue = HSVUtil.mod1(powers[bandIndex] / total + bandIndex / 3.0);
            return { h: hue, s: 0.85, v: 1.0 };
        }
        return (algo.hasUserColors ? algo.colors : DEFAULT_BAND_HSV)[bandIndex];
    }

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    // HSV-only contract: scripts no longer return packed RGB defaults.
    // The engine seeds m_rgbColors from acceptColors and the user picks them.
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var map = HSVUtil.createMap(width, height);
        if (!audio) return map;

        // BPM-scaled free-running time: one unit per beat (matches seconds at 60 BPM).
        time += audio.dt;

        var sensitivity = algo.presetSensitivity;
        var rawPows = [audio.low, audio.mid, audio.high];
        for (var sb = 0; sb < 3; sb++)
            smoothPow[sb] = HSVUtil.smoothFollow(smoothPow[sb], rawPows[sb], algo.presetSmoothing);
        var lowVis = Math.min(1, smoothPow[0] * sensitivity);
        var midVis = Math.min(1, smoothPow[1] * sensitivity);
        var highVis = Math.min(1, smoothPow[2] * sensitivity);

        // --- Bar-level build-up ---
        var rawEnergy = (rawPows[0] + rawPows[1] * 0.5 + rawPows[2] * 0.3) / 1.8;
        HSVUtil.barEnvelope(barState, rawEnergy, audio.dt, audio.downbeat, 0.85);

        var dominant = HSVUtil.dominantBand(audio);
        var dominantIndex = Math.max(0, dominant);
        var dominantValue = [lowVis, midVis, highVis][dominantIndex];
        var lowColor = colorFor(audio, 0);
        var midColor = colorFor(audio, 1);
        var highColor = colorFor(audio, 2);
        var dominantColor = [lowColor, midColor, highColor][dominantIndex];

        // Onset → flash overlay (single trigger source, no double-dipping).
        if (audio.onset)
            flash = Math.max(flash, audio.onsetIntensity * algo.presetFlash);
        flash *= FLASH_DECAY;

        // Beat → sparkle intensity envelope (single behavior).
        sparkleLevel *= SPARKLE_DECAY;
        if (algo.presetSparkles)
            sparkleLevel = Math.max(sparkleLevel, audio.cosPulse * highVis);

        // Distribute sparkle pixels for this frame in the upper half.
        var sparkleActive = false;
        if (algo.presetSparkles && sparkleLevel > 0.02) {
            var pixelCount = width * height;
            if (sparkleBitmap === null || sparkleBitmap.length !== pixelCount)
                sparkleBitmap = new Array(pixelCount);
            for (var b = 0; b < pixelCount; b++) sparkleBitmap[b] = 0;
            sparkleActive = true;
            var sparkRows = Math.max(1, Math.floor(height / 2));
            var sparkCount = Math.max(1, Math.floor(width * sparkRows * SPARKLE_DENSITY * sparkleLevel));
            for (var s = 0; s < sparkCount; s++) {
                var sx = Math.floor(Math.random() * width);
                var sy = Math.floor(Math.random() * sparkRows);
                sparkleBitmap[sy * width + sx] = 1;
            }
        }

        var barPhase = audio.barPhase;
        // Build-up swells overall brightness, downbeat release adds a burst
        var overall = BASE_OVERALL + dominantValue + barState.bar * 0.08 + barState.flash * 0.35;
        var flashActive = flash > 0.01;
        var flashAmount = Math.min(1, flash);

        for (var y = 0; y < height; y++) {
            var y01 = height <= 1 ? 0 : y / (height - 1);
            var bottom = 1 - y01;
            var top = y01;

            for (var x = 0; x < width; x++) {
                var x01 = width <= 1 ? 0 : x / (width - 1);
                var color, level;

                if (dominant === 0) {
                    var wave = HSVUtil.sin01(x01 * 2.5 + time + barPhase);
                    var pulseHeight = Math.min(1, 0.15 + lowVis * 1.15);
                    level = Math.max(0, (pulseHeight - bottom) / Math.max(0.001, pulseHeight));
                    level *= 0.55 + 0.45 * wave;
                    color = lowColor;
                } else if (dominant === 1) {
                    var dx = Math.abs(x01 - barPhase);
                    dx = Math.min(dx, 1 - dx);
                    var ripple = Math.max(0, 1 - dx * (4 + midVis * 8));
                    var rowWave = HSVUtil.sin01(y01 * 3 + time * 0.65);
                    level = Math.max(ripple, rowWave * midVis * 0.65);
                    color = midColor;
                } else {
                    var shimmer = HSVUtil.sin01(x01 * 9 + y01 * 5 + time * 1.8);
                    level = highVis * (0.25 + 0.75 * top) * (0.45 + 0.55 * shimmer);
                    color = highColor;
                }

                // Sparkle override (single white flicker, no per-pixel state).
                if (sparkleActive && sparkleBitmap[y * width + x]) {
                    color = SPARKLE_HSV;
                    level = Math.max(level, sparkleLevel);
                }

                // Flash override (full-frame tint, single trigger source).
                if (flashActive) {
                    color = dominantColor;
                    level = Math.max(level, flashAmount);
                }

                var brightness = HSVUtil.clamp01(Math.min(1, level) * overall);
                var pi = (y * width + x) * 3;
                map[pi]     = color.h;
                map[pi + 1] = color.s;
                map[pi + 2] = color.v * brightness;
            }
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
