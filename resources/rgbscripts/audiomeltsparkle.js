/*
  Q Light Controller Plus
  audiomeltsparkle.js

  Copyright (c) QLC+ contributors
  Ported from LedFX "Melt and Sparkle" effect (MIT License)

  Flowing HSV lava field driven by bass (hue roll, speed, direction
  flips) and mid (lava chunk width). Onset-triggered white sparkle
  spans blur and decay across the strip.

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
    algo.name = "Audio Melt and Sparkle";
    algo.author = "Ported from LedFx";
    algo.acceptColors = 0; // pure HSV procedural
    algo.usesAudio = true;
    algo.properties = new Array();

    // LedFx defaults: speed=0.5, reactivity=0.5
    algo.presetSpeed = 0.5;
    algo.properties.push(
      "name:presetSpeed|type:float|display:Speed (cyc/beat)|" +
      "write:setSpeed|read:getSpeed");

    algo.presetReactivity = 0.5;
    algo.properties.push(
      "name:presetReactivity|type:float|display:Reactivity|" +
      "write:setReactivity|read:getReactivity");

    // LedFx parity: bg_bright default = 0.4 (was 0.30 in earlier QLC+ port)
    algo.presetBgBright = 0.4;
    algo.properties.push(
      "name:presetBgBright|type:float|display:Background Brightness|" +
      "write:setBgBright|read:getBgBright");

    algo.presetLavaWidth = 0.5;
    algo.properties.push(
      "name:presetLavaWidth|type:float|display:Lava Width|" +
      "write:setLavaWidth|read:getLavaWidth");

    // LedFx parity: strobe_threshold default = 0.75 (was 0.40 in earlier QLC+ port)
    algo.presetStrobeThreshold = 0.75;
    algo.properties.push(
      "name:presetStrobeThreshold|type:float|display:Strobe Threshold|" +
      "write:setStrobeThreshold|read:getStrobeThreshold");

    // LedFx default: strobe_rate=0.75
    algo.presetStrobeRate = 0.75;
    algo.properties.push(
      "name:presetStrobeRate|type:float|display:Strobe Rate|" +
      "write:setStrobeRate|read:getStrobeRate");

    algo.presetStrobeWidth = 0.3;
    algo.properties.push(
      "name:presetStrobeWidth|type:float|display:Strobe Width|" +
      "write:setStrobeWidth|read:getStrobeWidth");

    // LedFx default: strobe_decay_rate=0.25
    algo.presetStrobeDecay = 0.25;
    algo.properties.push(
      "name:presetStrobeDecay|type:float|display:Strobe Decay|" +
      "write:setStrobeDecay|read:getStrobeDecay");

    // LedFx default: strobe_blur=3.5 (rounded to 4 for integer range)
    algo.presetStrobeBlur = 4;
    algo.properties.push(
      "name:presetStrobeBlur|type:range|display:Strobe Blur|" +
      "values:0,10|write:setStrobeBlur|read:getStrobeBlur");

    algo.presetAxis = "Horizontal";
    algo.properties.push(
      "name:presetAxis|type:list|display:Axis|" +
      "values:Horizontal,Vertical|write:setAxis|read:getAxis");

    algo.setSpeed = function(_v) { algo.presetSpeed = parseFloat(_v); };
    algo.getSpeed = function() { return algo.presetSpeed; };
    algo.setReactivity = function(_v) { algo.presetReactivity = parseFloat(_v); };
    algo.getReactivity = function() { return algo.presetReactivity; };
    algo.setBgBright = function(_v) { algo.presetBgBright = parseFloat(_v); };
    algo.getBgBright = function() { return algo.presetBgBright; };
    algo.setLavaWidth = function(_v) { algo.presetLavaWidth = parseFloat(_v); };
    algo.getLavaWidth = function() { return algo.presetLavaWidth; };
    algo.setStrobeThreshold = function(_v) { algo.presetStrobeThreshold = parseFloat(_v); };
    algo.getStrobeThreshold = function() { return algo.presetStrobeThreshold; };
    algo.setStrobeRate = function(_v) { algo.presetStrobeRate = parseFloat(_v); };
    algo.getStrobeRate = function() { return algo.presetStrobeRate; };
    algo.setStrobeWidth = function(_v) { algo.presetStrobeWidth = parseFloat(_v); };
    algo.getStrobeWidth = function() { return algo.presetStrobeWidth; };
    algo.setStrobeDecay = function(_v) { algo.presetStrobeDecay = parseFloat(_v); };
    algo.getStrobeDecay = function() { return algo.presetStrobeDecay; };
    algo.setStrobeBlur = function(_v) { algo.presetStrobeBlur = parseInt(_v); };
    algo.getStrobeBlur = function() { return algo.presetStrobeBlur; };
    algo.setAxis = function(_v) { algo.presetAxis = _v; };
    algo.getAxis = function() { return algo.presetAxis; };

    var DIRECTION_FLIP_CHANCE = 5.0;   // expected flips per second when bass > strobe cutoff
    var MIN_STROBE_COOLDOWN_MS = 100;  // floor for strobe cooldown regardless of rate
    var MAX_STROBE_WIDTH_FRAC = 0.5;   // a single strobe spans at most this fraction of the strip
    var MAX_STROBE_COOLDOWN_MS = 1000; // span of the cooldown range mapped from rate
    var LAVA_POWER_BASE = 30;          // base exponent for lava chunk shaping
    // Per-track ratios relative to presetSpeed. T1 runs ~4× faster (inner lava),
    // hueFast/hueSlow trail behind the base rate.
    var T1_RATIO = 4.0;
    var HUE_FAST_RATIO = 0.3;
    var HUE_SLOW_RATIO = 0.1;

    algo.direction = 1;
    var t1State = { phase: 0 };
    var hueFastState = { phase: 0 };
    var hueSlowState = { phase: 0 };
    algo.strobeOverlay = [];
    algo.lastStrobeMs = 0;
    algo.elapsedMs = 0;
    algo.lastSize = 0;

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };

    function resetState(n) {
        algo.strobeOverlay = new Array(n);
        for (var i = 0; i < n; i++) algo.strobeOverlay[i] = 0;
        algo.lastSize = n;
        t1State.phase = 0;
        hueFastState.phase = 0;
        hueSlowState.phase = 0;
        algo.direction = 1;
        algo.lastStrobeMs = 0;
    }

    function boxBlur(arr, radius) {
        var n = arr.length;
        if (radius <= 0 || n === 0) return arr;
        var out = new Array(n);
        var window = radius * 2 + 1;
        for (var i = 0; i < n; i++) {
            var sum = 0;
            for (var k = -radius; k <= radius; k++) {
                var idx = i + k;
                if (idx < 0) idx = 0;
                else if (idx >= n) idx = n - 1;
                sum += arr[idx];
            }
            out[i] = sum / window;
        }
        return out;
    }

    algo.rgbMap = function(width, height, rgb, step, audio) {
        var map = RGBUtil.createMap(width, height);
        if (!audio) return map;

        var n = (algo.presetAxis === "Vertical") ? height : width;
        if (n <= 0) return map;

        if (algo.lastSize !== n) resetState(n);

        var dtMs = audio.timing.consumerDtMs > 0 ? audio.timing.consumerDtMs : 40;
        var dt = dtMs / 1000.0;
        var bpm = audio.beat ? audio.beat.bpm : 0;
        algo.elapsedMs += dtMs;

        var lowPower = audio.power.low;
        var midPower = audio.power.mid;
        var highMax = audio.spectrum.high.max;
        var onsetFired = audio.onset.fired;
        var lastDominant = AudioColors.dominantIndex(audio);

        var speed01 = algo.presetSpeed;
        var reactivity01 = algo.presetReactivity;
        var bgBright01 = algo.presetBgBright;
        var lavaWidth01 = algo.presetLavaWidth;
        var strobeCutoff = algo.presetStrobeThreshold;
        var strobeRate01 = algo.presetStrobeRate;
        var strobeWidth01 = algo.presetStrobeWidth;
        var strobeDecay01 = algo.presetStrobeDecay;
        var strobeBlur = algo.presetStrobeBlur;

        // Direction-flipped, audio-boosted dt fed to all beat helpers.
        var boostMs = lowPower * reactivity01 * speed01 * dtMs;
        var dirDtMs = (dtMs + boostMs) * algo.direction;

        if (lowPower > strobeCutoff) {
            var flipProb = DIRECTION_FLIP_CHANCE * dt;
            if (Math.random() < flipProb) algo.direction = -algo.direction;
        }

        var bassFactor = lowPower * reactivity01 * 0.5;
        var widthFactor = (1 - lavaWidth01) * (1 - lavaWidth01);
        var lavaPower = LAVA_POWER_BASE * widthFactor - midPower * widthFactor;
        if (lavaPower < 1) lavaPower = 1;

        var cooldownMs = MIN_STROBE_COOLDOWN_MS + (1 - strobeRate01) * MAX_STROBE_COOLDOWN_MS;
        if (onsetFired && highMax > strobeCutoff && (algo.elapsedMs - algo.lastStrobeMs) >= cooldownMs) {
            var widthFrac = strobeWidth01 * strobeWidth01 * strobeWidth01;
            if (widthFrac > MAX_STROBE_WIDTH_FRAC) widthFrac = MAX_STROBE_WIDTH_FRAC;
            var sw = Math.max(1, Math.round(widthFrac * n));
            if (sw > n) sw = n;
            var maxStart = n - sw;
            var startIdx = maxStart > 0 ? Math.floor(Math.random() * (maxStart + 1)) : 0;
            for (var s = 0; s < sw; s++) algo.strobeOverlay[startIdx + s] = 1.0;
            algo.lastStrobeMs = algo.elapsedMs;
        }

        var decayMul = 1 - strobeDecay01;
        if (decayMul < 0) decayMul = 0;
        for (var d = 0; d < n; d++) algo.strobeOverlay[d] *= decayMul;
        if (strobeBlur > 0) algo.strobeOverlay = boxBlur(algo.strobeOverlay, strobeBlur);

        var strip = new Array(n);
        var denom = Math.max(1, n - 1);

        // Beat-locked replacements for the LedFx HSVEffect.time() outputs.
        var t1 = RGBUtil.beatTime(speed01 * T1_RATIO, t1State, bpm, dirDtMs);
        var hueFast = RGBUtil.beatTime(speed01 * HUE_FAST_RATIO, hueFastState, bpm, dirDtMs);
        var hueSlow = RGBUtil.beatTime(speed01 * HUE_SLOW_RATIO, hueSlowState, bpm, dirDtMs);

        var bandShift = 0;
        if (lastDominant === 0) bandShift = 0.05;
        else if (lastDominant === 2) bandShift = -0.1;

        for (var i = 0; i < n; i++) {
            var u = 1 - i / denom;

            var h0 = RGBUtil.sin01(u);
            var h1 = RGBUtil.triangle(h0 + bassFactor + hueFast);
            var hue = RGBUtil.mod1(RGBUtil.triangle(h1) + hueSlow);

            // LedFx parity: 4-pass value pipeline (render_hsv lines 156-172).
            var vInit = RGBUtil.sin01(u);
            var vA = RGBUtil.sin01(vInit);
            var vB = RGBUtil.sin01(vA + t1);
            var vC = RGBUtil.triangle(vB + (1.0 - t1));
            var v = RGBUtil.triangle(vC + bassFactor * algo.direction);
            v = Math.pow(v, lavaPower);
            v *= bgBright01;

            var sat = 1.0;
            var overlay = algo.strobeOverlay[i];
            if (overlay > 0) {
                var tintedHue = RGBUtil.mod1(hue + bandShift);
                sat *= 1 - RGBUtil.clamp01(overlay) * 0.7;
                v += overlay;
                strip[i] = {h: tintedHue, s: sat, v: RGBUtil.clamp01(v)};
            } else {
                strip[i] = {h: hue, s: sat, v: RGBUtil.clamp01(v)};
            }
        }

        if (algo.presetAxis === "Vertical") {
            for (var y = 0; y < height; y++) {
                var cy = strip[y];
                for (var x = 0; x < width; x++) RGBUtil.setPixel(map, width, x, y, cy.h, cy.s, cy.v);
            }
        } else {
            for (var y2 = 0; y2 < height; y2++) {
                for (var x2 = 0; x2 < width; x2++) {
                    var px = strip[x2];
                    RGBUtil.setPixel(map, width, x2, y2, px.h, px.s, px.v);
                }
            }
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
