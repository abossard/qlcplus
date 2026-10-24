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
    algo.acceptColors = 3;
    algo.usesAudio = true;
    algo.properties = new Array();
    algo.presetMode = "Artistic";
    algo.properties.push("name:mode|type:list|display:Response|values:Artistic,LedFx Melt and Sparkle|write:setMode|read:getMode");
    algo.setMode = function(v) { algo.presetMode = v === "LedFx Melt and Sparkle" ? v : "Artistic"; };
    algo.getMode = function() { return algo.presetMode; };

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

    algo.setSpeed = function(_v) { algo.presetSpeed = HSVUtil.parseNumber(_v, algo.presetSpeed, 0, 10); };
    algo.getSpeed = function() { return algo.presetSpeed; };
    algo.setReactivity = function(_v) { algo.presetReactivity = HSVUtil.parseNumber(_v, algo.presetReactivity, 0, 10); };
    algo.getReactivity = function() { return algo.presetReactivity; };
    algo.setBgBright = function(_v) { algo.presetBgBright = HSVUtil.parseNumber(_v, algo.presetBgBright, 0, 1); };
    algo.getBgBright = function() { return algo.presetBgBright; };
    algo.setLavaWidth = function(_v) { algo.presetLavaWidth = HSVUtil.parseNumber(_v, algo.presetLavaWidth, 0, 1); };
    algo.getLavaWidth = function() { return algo.presetLavaWidth; };
    algo.setStrobeThreshold = function(_v) { algo.presetStrobeThreshold = HSVUtil.parseNumber(_v, algo.presetStrobeThreshold, 0, 1); };
    algo.getStrobeThreshold = function() { return algo.presetStrobeThreshold; };
    algo.setStrobeRate = function(_v) { algo.presetStrobeRate = HSVUtil.parseNumber(_v, algo.presetStrobeRate, 0, 1); };
    algo.getStrobeRate = function() { return algo.presetStrobeRate; };
    algo.setStrobeWidth = function(_v) { algo.presetStrobeWidth = HSVUtil.parseNumber(_v, algo.presetStrobeWidth, 0, 1); };
    algo.getStrobeWidth = function() { return algo.presetStrobeWidth; };
    algo.setStrobeDecay = function(_v) { algo.presetStrobeDecay = HSVUtil.parseNumber(_v, algo.presetStrobeDecay, 0, 1); };
    algo.getStrobeDecay = function() { return algo.presetStrobeDecay; };
    algo.setStrobeBlur = function(_v) { algo.presetStrobeBlur = Math.round(HSVUtil.parseNumber(_v, algo.presetStrobeBlur, 0, 10)); };
    algo.getStrobeBlur = function() { return algo.presetStrobeBlur; };
    algo.setAxis = function(_v) { algo.presetAxis = _v; };
    algo.getAxis = function() { return algo.presetAxis; };

    algo.presetSmoothing = 5;
    algo.properties.push(
      "name:presetSmoothing|type:range|display:Smoothing|" +
      "values:1,10|write:setSmoothing|read:getSmoothing");
    algo.setSmoothing = function(_v) { algo.presetSmoothing = Math.round(HSVUtil.parseNumber(_v, algo.presetSmoothing, 1, 10)); };
    algo.getSmoothing = function() { return algo.presetSmoothing; };

    var smoothLow = 0;
    var smoothLowLedFx = 0;
    var smoothMidLedFx = 0;
    var ledFxTimestepMs = 0;
    var NOMINAL_HZ = 60;
    var getLedFxOutputOptions = HSVUtil.attachStripTransformControls(algo, {
        prefix: "ledFx",
        display: "LedFx "
    });

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
    var t1 = 0;
    var hueFast = 0;
    var hueSlow = 0;
    algo.strobeMask = [];
    algo.lastStrobeMs = -MAX_STROBE_COOLDOWN_MS;
    algo.elapsedMs = 0;
    algo.lastSize = 0;
    algo.lastStateKey = "";
    algo.lastAudioIdentityKey = "";

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };

    function resetState(n) {
        algo.strobeMask = new Array(n);
        for (var i = 0; i < n; i++) algo.strobeMask[i] = 0;
        algo.lastSize = n;
        t1 = 0;
        hueFast = 0;
        hueSlow = 0;
        algo.direction = 1;
        algo.lastStrobeMs = -MAX_STROBE_COOLDOWN_MS;
        algo.elapsedMs = 0;
        smoothLow = 0;
        smoothLowLedFx = 0;
        smoothMidLedFx = 0;
        ledFxTimestepMs = 0;
    }

    function upperProcessedPeak(audio) {
        var bank = audio && audio.banks && audio.banks.full;
        var values = bank && Array.isArray(bank.processed) ? bank.processed : null;
        if (!values || values.length === 0)
            return HSVUtil.clamp01(audio && isFinite(audio.high) ? audio.high : 0);
        var count = bank.count && bank.count > 0 ? Math.min(bank.count, values.length) : values.length;
        var start = Math.max(0, Math.floor((2 * count) / 3));
        var peak = 0;
        for (var i = start; i < count; i++) {
            var v = isFinite(values[i]) ? values[i] : 0;
            if (v > peak) peak = v;
        }
        return HSVUtil.clamp01(peak);
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

    function strobeOverlay(blur) {
        var overlay = blur > 0 ? boxBlur(algo.strobeMask, blur) : algo.strobeMask.slice();
        for (var i = 0; i < overlay.length; i++) overlay[i] = Math.max(overlay[i], algo.strobeMask[i]);
        return overlay;
    }

    algo.rgbMap = function(width, height, rgb, step, audio) {
        var map = HSVUtil.createMap(width, height);
        if (!audio) return map;
        var ledFxMode = algo.presetMode === "LedFx Melt and Sparkle";
        var vertical = algo.presetAxis === "Vertical";
        var n = ledFxMode ? Math.max(1, width * height) : (vertical ? height : width);
        if (n <= 0) return map;

        var audioIdentityKey = HSVUtil.audioIdentityKey(audio, algo.lastAudioIdentityKey);
        var stateKey = width + "x" + height + "|" + (ledFxMode ? "LedFx" : algo.presetAxis);
        if (algo.lastSize !== n ||
            algo.lastStateKey !== stateKey ||
            algo.lastAudioIdentityKey !== audioIdentityKey)
        {
            resetState(n);
            algo.lastStateKey = stateKey;
        }
        algo.lastAudioIdentityKey = audioIdentityKey;

        var dtSec = HSVUtil.audioSeconds(audio);
        var dtMs = dtSec * 1000.0;
        algo.elapsedMs += dtMs;

        if (ledFxMode) {
            var lowPowerLed = HSVUtil.powerForRange(audio, "Lows", true);
            smoothLowLedFx = HSVUtil.attackRelease(smoothLowLedFx, lowPowerLed, 0.1, 0.1, dtSec * 50);
            var midPowerLed = HSVUtil.clamp01(isFinite(audio.mid) ? audio.mid : 0);
            smoothMidLedFx = HSVUtil.attackRelease(smoothMidLedFx, midPowerLed, 0.1, 0.1, dtSec * 50);

            var speedLed = algo.presetSpeed;
            var reactivityLed = algo.presetReactivity;
            var bgBrightLed = algo.presetBgBright;
            var lavaWidthLed = algo.presetLavaWidth;
            var strobeCutoffLed = algo.presetStrobeThreshold / 10.0;
            var strobeRateLed = algo.presetStrobeRate;
            var strobeWidthLed = algo.presetStrobeWidth;
            var strobeDecayLed = algo.presetStrobeDecay;
            var strobeBlurLed = algo.presetStrobeBlur;

            if (smoothLowLedFx > strobeCutoffLed && Math.random() < DIRECTION_FLIP_CHANCE * dtSec)
                algo.direction = -algo.direction;

            ledFxTimestepMs += dtMs * algo.direction;
            ledFxTimestepMs += smoothLowLedFx * reactivityLed * speedLed * 500.0 * dtSec * NOMINAL_HZ * algo.direction;
            var t1Led = HSVUtil.time01(speedLed * 20.0, ledFxTimestepMs);
            var bassFactorLed = smoothLowLedFx * reactivityLed * 0.5;

            var decayBaseLed = 1 - strobeDecayLed;
            if (decayBaseLed < 0) decayBaseLed = 0;
            var decayLed = strobeDecayLed >= 1.0 ? 0 : Math.pow(decayBaseLed, dtSec * NOMINAL_HZ);

            var onsetCountLed = audio.events && audio.events.delta ? audio.events.delta.onset : 0;
            var triggerValueLed = Math.pow(upperProcessedPeak(audio), 2);
            var cooldownMsLed = MIN_STROBE_COOLDOWN_MS + (1 - strobeRateLed) * MAX_STROBE_COOLDOWN_MS;
            while (onsetCountLed > 0 &&
                   triggerValueLed > strobeCutoffLed &&
                   (algo.elapsedMs - algo.lastStrobeMs) >= cooldownMsLed) {
                var widthFracLed = strobeWidthLed * strobeWidthLed * strobeWidthLed;
                if (widthFracLed > MAX_STROBE_WIDTH_FRAC) widthFracLed = MAX_STROBE_WIDTH_FRAC;
                var maxWidthLed = Math.max(1, n - 1);
                var spanLed = Math.max(1, Math.min(maxWidthLed, Math.floor(widthFracLed * n)));
                var posRangeLed = Math.max(1, n - spanLed);
                var posLed = Math.floor(Math.random() * posRangeLed);
                for (var w = 0; w < spanLed; w++) algo.strobeMask[posLed + w] = 1.0;
                algo.lastStrobeMs = algo.elapsedMs;
                onsetCountLed--;
            }

            var overlayLed = strobeOverlay(strobeBlurLed);

            var widthFactorLed = Math.pow(1 - lavaWidthLed, 2);
            var lavaPowerLed = LAVA_POWER_BASE * widthFactorLed - (smoothMidLedFx * widthFactorLed);
            var stopsLed = HSVUtil.ledFxGradientStops(algo);
            for (var iLed = 0; iLed < n; iLed++) {
                var ledX = iLed % width;
                var ledY = Math.floor(iLed / width);
                var uLed = 1 - iLed / Math.max(1, n - 1);

                var hLed = HSVUtil.sin01(uLed);
                hLed += bassFactorLed * speedLed * 5;
                hLed = HSVUtil.triangle(hLed);
                hLed = HSVUtil.triangle(hLed);

                var vLed = HSVUtil.sin01(uLed);
                vLed = HSVUtil.sin01(vLed);
                vLed = HSVUtil.sin01(vLed + t1Led);
                vLed = HSVUtil.triangle(vLed + (1 - t1Led));
                vLed = HSVUtil.triangle(vLed + bassFactorLed);
                vLed = Math.pow(vLed, lavaPowerLed);
                vLed *= bgBrightLed;

                var satLed = 1.0;
                var sparkleLed = HSVUtil.clamp01(overlayLed[iLed]);
                if (sparkleLed > 0) {
                    satLed *= (1 - sparkleLed);
                    vLed = Math.max(vLed, sparkleLed);
                }

                var outHsv = HSVUtil.ledFxColor(stopsLed, HSVUtil.mod1(hLed), satLed, vLed);
                HSVUtil.setPixel(map, width, ledX, ledY, outHsv.h, outHsv.s, outHsv.v);
            }

            if (dtSec > 0) {
                for (var dLed = 0; dLed < n; dLed++) algo.strobeMask[dLed] *= decayLed;
                if (strobeBlurLed > 0) algo.strobeMask = boxBlur(algo.strobeMask, strobeBlurLed);
            }

            HSVUtil.applyStripTransforms(map, width, height, getLedFxOutputOptions());
            return map;
        }

        var lowPower = audio.low;
        smoothLow = HSVUtil.smoothFollow(smoothLow, lowPower, algo.presetSmoothing, dtSec * 50);

        var speed01 = algo.presetSpeed;
        var reactivity01 = algo.presetReactivity;
        var bgBright01 = algo.presetBgBright;
        var lavaWidth01 = algo.presetLavaWidth;
        var strobeCutoff = algo.presetStrobeThreshold;
        var strobeRate01 = algo.presetStrobeRate;
        var strobeWidth01 = algo.presetStrobeWidth;
        var strobeDecay01 = algo.presetStrobeDecay;
        var strobeBlur = algo.presetStrobeBlur;

        if (lowPower > strobeCutoff) {
            var flipProb = DIRECTION_FLIP_CHANCE * dtSec;
            if (Math.random() < flipProb) algo.direction = -algo.direction;
        }

        var bassFactor = smoothLow * reactivity01 * 0.5;
        var widthFactor = (1 - lavaWidth01) * (1 - lavaWidth01);
        var lavaPower = LAVA_POWER_BASE * widthFactor - audio.mid * widthFactor;
        if (lavaPower < 1) lavaPower = 1;

        if (dtSec > 0) {
            var decayMul = 1 - strobeDecay01;
            if (decayMul < 0) decayMul = 0;
            var decay = strobeDecay01 >= 1.0 ? 0 : Math.pow(decayMul, dtSec * 50);
            for (var d = 0; d < n; d++) algo.strobeMask[d] *= decay;
        }
        var cooldownMs = MIN_STROBE_COOLDOWN_MS + (1 - strobeRate01) * MAX_STROBE_COOLDOWN_MS;
        var onsetCount = audio.events && audio.events.delta
            ? audio.events.delta.onset
            : (audio.onset ? 1 : 0);
        while (onsetCount > 0 &&
               audio.high > strobeCutoff &&
               (algo.elapsedMs - algo.lastStrobeMs) >= cooldownMs) {
            var widthFrac = strobeWidth01 * strobeWidth01 * strobeWidth01;
            if (widthFrac > MAX_STROBE_WIDTH_FRAC) widthFrac = MAX_STROBE_WIDTH_FRAC;
            var sw = Math.max(1, Math.round(widthFrac * n));
            if (sw > n) sw = n;
            var maxStart = n - sw;
            var startIdx = maxStart > 0 ? Math.floor(Math.random() * (maxStart + 1)) : 0;
            for (var s = 0; s < sw; s++) algo.strobeMask[startIdx + s] = 1.0;
            algo.lastStrobeMs = algo.elapsedMs;
            onsetCount--;
        }
        var overlay = strobeOverlay(strobeBlur);

        var strip = new Array(n);
        var denom = Math.max(1, n - 1);

        // Beat-locked replacements for the LedFx HSVEffect.time() outputs.
        t1 = (t1 + audio.dt * speed01 * T1_RATIO) % 1.0;
        hueFast = (hueFast + audio.dt * speed01 * HUE_FAST_RATIO) % 1.0;
        hueSlow = (hueSlow + audio.dt * speed01 * HUE_SLOW_RATIO) % 1.0;

        for (var i = 0; i < n; i++) {
            var u = 1 - i / denom;

            var h0 = HSVUtil.sin01(u);
            var h1 = HSVUtil.triangle(h0 + bassFactor + hueFast);
            var hue = HSVUtil.mod1(HSVUtil.triangle(h1) + hueSlow);

            // LedFx parity: 4-pass value pipeline (render_hsv lines 156-172).
            var vInit = HSVUtil.sin01(u);
            var vA = HSVUtil.sin01(vInit);
            var vB = HSVUtil.sin01(vA + t1);
            var vC = HSVUtil.triangle(vB + (1.0 - t1));
            var v = HSVUtil.triangle(vC + bassFactor * algo.direction);
            v = Math.pow(v, lavaPower);
            v *= bgBright01;

            var sparkle = overlay[i] > 0 ? HSVUtil.clamp01(overlay[i]) : 0;
            strip[i] = {h: hue, s: 1 - sparkle, v: HSVUtil.clamp01(Math.max(v, sparkle))};
        }

        for (var y = 0; y < height; y++) {
            for (var x = 0; x < width; x++) {
                var px = strip[vertical ? y : x];
                HSVUtil.setPixel(map, width, x, y, px.h, px.s, px.v);
            }
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
