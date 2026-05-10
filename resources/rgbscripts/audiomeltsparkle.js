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

    algo.presetSpeed = 30;
    algo.properties.push(
      "name:presetSpeed|type:range|display:Speed|" +
      "values:1,100|write:setSpeed|read:getSpeed");

    algo.presetReactivity = 40;
    algo.properties.push(
      "name:presetReactivity|type:range|display:Reactivity|" +
      "values:1,100|write:setReactivity|read:getReactivity");

    algo.presetBgBright = 30;
    algo.properties.push(
      "name:presetBgBright|type:range|display:Background Brightness|" +
      "values:1,100|write:setBgBright|read:getBgBright");

    algo.presetLavaWidth = 50;
    algo.properties.push(
      "name:presetLavaWidth|type:range|display:Lava Width|" +
      "values:1,100|write:setLavaWidth|read:getLavaWidth");

    algo.presetStrobeThreshold = 40;
    algo.properties.push(
      "name:presetStrobeThreshold|type:range|display:Strobe Threshold|" +
      "values:1,100|write:setStrobeThreshold|read:getStrobeThreshold");

    algo.presetStrobeRate = 50;
    algo.properties.push(
      "name:presetStrobeRate|type:range|display:Strobe Rate|" +
      "values:1,100|write:setStrobeRate|read:getStrobeRate");

    algo.presetStrobeWidth = 30;
    algo.properties.push(
      "name:presetStrobeWidth|type:range|display:Strobe Width|" +
      "values:1,100|write:setStrobeWidth|read:getStrobeWidth");

    algo.presetStrobeDecay = 50;
    algo.properties.push(
      "name:presetStrobeDecay|type:range|display:Strobe Decay|" +
      "values:1,100|write:setStrobeDecay|read:getStrobeDecay");

    algo.presetStrobeBlur = 2;
    algo.properties.push(
      "name:presetStrobeBlur|type:range|display:Strobe Blur|" +
      "values:0,10|write:setStrobeBlur|read:getStrobeBlur");

    algo.presetAxis = "Horizontal";
    algo.properties.push(
      "name:presetAxis|type:list|display:Axis|" +
      "values:Horizontal,Vertical|write:setAxis|read:getAxis");

    algo.setSpeed = function(_v) { algo.presetSpeed = parseInt(_v); };
    algo.getSpeed = function() { return algo.presetSpeed; };
    algo.setReactivity = function(_v) { algo.presetReactivity = parseInt(_v); };
    algo.getReactivity = function() { return algo.presetReactivity; };
    algo.setBgBright = function(_v) { algo.presetBgBright = parseInt(_v); };
    algo.getBgBright = function() { return algo.presetBgBright; };
    algo.setLavaWidth = function(_v) { algo.presetLavaWidth = parseInt(_v); };
    algo.getLavaWidth = function() { return algo.presetLavaWidth; };
    algo.setStrobeThreshold = function(_v) { algo.presetStrobeThreshold = parseInt(_v); };
    algo.getStrobeThreshold = function() { return algo.presetStrobeThreshold; };
    algo.setStrobeRate = function(_v) { algo.presetStrobeRate = parseInt(_v); };
    algo.getStrobeRate = function() { return algo.presetStrobeRate; };
    algo.setStrobeWidth = function(_v) { algo.presetStrobeWidth = parseInt(_v); };
    algo.getStrobeWidth = function() { return algo.presetStrobeWidth; };
    algo.setStrobeDecay = function(_v) { algo.presetStrobeDecay = parseInt(_v); };
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
    var DEFAULT_DT_MS = 40;            // fallback frame interval when audio timing is missing

    algo.timestep = 0;
    algo.direction = 1;
    algo.strobeOverlay = [];
    algo.lastStrobeMs = 0;
    algo.lastMs = 0;
    algo.lastSize = 0;

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };

    function resetState(n) {
        algo.strobeOverlay = new Array(n);
        for (var i = 0; i < n; i++) algo.strobeOverlay[i] = 0;
        algo.lastSize = n;
        algo.timestep = 0;
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

        var nowMs = Date.now();
        var dtMs = audio.timing.consumerDtMs > 0 ? audio.timing.consumerDtMs : DEFAULT_DT_MS;
        var dtSec = dtMs / 1000.0;
        algo.lastMs = nowMs;

        var lowPower = audio.power.low;
        var midPower = audio.power.mid;
        var highMax = audio.spectrum.high.max;
        var onsetFired = audio.onset.fired;

        var speed01 = algo.presetSpeed / 100.0;
        var reactivity01 = algo.presetReactivity / 100.0;
        var bgBright01 = algo.presetBgBright / 100.0;
        var lavaWidth01 = algo.presetLavaWidth / 100.0;
        var strobeCutoff = algo.presetStrobeThreshold / 100.0;
        var strobeRate01 = algo.presetStrobeRate / 100.0;
        var strobeWidth01 = algo.presetStrobeWidth / 100.0;
        var strobeDecay01 = algo.presetStrobeDecay / 100.0;
        var strobeBlur = algo.presetStrobeBlur;

        algo.timestep += dtSec * algo.direction;
        algo.timestep += lowPower * reactivity01 * speed01 * dtSec * algo.direction;

        if (lowPower > strobeCutoff) {
            var flipProb = DIRECTION_FLIP_CHANCE * dtSec;
            if (Math.random() < flipProb) algo.direction = -algo.direction;
        }

        var bassFactor = lowPower * reactivity01 * 0.5;
        var widthFactor = (1 - lavaWidth01) * (1 - lavaWidth01);
        var lavaPower = LAVA_POWER_BASE * widthFactor - midPower * widthFactor;
        if (lavaPower < 1) lavaPower = 1;

        var cooldownMs = MIN_STROBE_COOLDOWN_MS + (1 - strobeRate01) * MAX_STROBE_COOLDOWN_MS;
        if (onsetFired && highMax > strobeCutoff && (nowMs - algo.lastStrobeMs) >= cooldownMs) {
            var widthFrac = strobeWidth01 * strobeWidth01 * strobeWidth01;
            if (widthFrac > MAX_STROBE_WIDTH_FRAC) widthFrac = MAX_STROBE_WIDTH_FRAC;
            var sw = Math.max(1, Math.round(widthFrac * n));
            if (sw > n) sw = n;
            var maxStart = n - sw;
            var startIdx = maxStart > 0 ? Math.floor(Math.random() * (maxStart + 1)) : 0;
            for (var s = 0; s < sw; s++) algo.strobeOverlay[startIdx + s] = 1.0;
            algo.lastStrobeMs = nowMs;
        }

        var decayMul = 1 - strobeDecay01;
        if (decayMul < 0) decayMul = 0;
        for (var d = 0; d < n; d++) algo.strobeOverlay[d] *= decayMul;
        if (strobeBlur > 0) algo.strobeOverlay = boxBlur(algo.strobeOverlay, strobeBlur);

        var strip = new Array(n);
        var denom = Math.max(1, n - 1);
        for (var i = 0; i < n; i++) {
            var u = 1 - i / denom;

            var h0 = RGBUtil.sin01(u);
            var h1 = RGBUtil.triangle(h0 + bassFactor + algo.timestep * 0.3);
            var hue = RGBUtil.mod1(RGBUtil.triangle(h1) + algo.timestep * 0.1);

            var v0 = RGBUtil.sin01(u * 3 + algo.timestep);
            var v1 = RGBUtil.triangle(v0 + bassFactor);
            var v = RGBUtil.triangle(v1);
            v = Math.pow(v, lavaPower);
            v *= bgBright01;

            var sat = 1.0;
            var overlay = algo.strobeOverlay[i];
            if (overlay > 0) {
                sat *= 1 - RGBUtil.clamp01(overlay);
                v += overlay;
                if (v > 1) v = 1;
            }

            strip[i] = RGBUtil.hsvToRgb(hue, sat, RGBUtil.clamp01(v));
        }

        if (algo.presetAxis === "Vertical") {
            for (var y = 0; y < height; y++) {
                var cy = strip[y];
                for (var x = 0; x < width; x++) map[y][x] = cy;
            }
        } else {
            for (var y2 = 0; y2 < height; y2++) {
                for (var x2 = 0; x2 < width; x2++) map[y2][x2] = strip[x2];
            }
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
