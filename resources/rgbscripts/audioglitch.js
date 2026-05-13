/*
  Q Light Controller Plus
  audioglitch.js

  Copyright (c) QLC+ contributors
  Ported from LedFX "Glitch" effect (MIT License)

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
    algo.name = "Audio Glitch";
    algo.author = "Ported from LedFx";
    algo.acceptColors = 3; // low/mid/high mel-bank gradient
    algo.usesAudio = true;
    algo.properties = new Array();

    algo.presetReactivity = 0.3;
    algo.properties.push(
      "name:presetReactivity|type:float|display:Reactivity|" +
      "write:setReactivity|read:getReactivity");
    algo.presetSpeed = 0.125;
    algo.properties.push(
      "name:presetSpeed|type:float|display:Speed (cyc/beat)|" +
      "write:setSpeed|read:getSpeed");
    algo.presetSaturation = 1.0;
    algo.properties.push(
      "name:presetSaturation|type:float|display:Saturation|" +
      "write:setSaturation|read:getSaturation");
    algo.presetComplexity = 5;
    algo.properties.push(
      "name:presetComplexity|type:range|display:Complexity|" +
      "values:1,10|write:setComplexity|read:getComplexity");

    algo.setSpeed = function(_v) { algo.presetSpeed = parseFloat(_v); };
    algo.getSpeed = function() { return algo.presetSpeed; };
    algo.setSaturation = function(_v) { algo.presetSaturation = parseFloat(_v); };
    algo.getSaturation = function() { return algo.presetSaturation; };
    algo.setComplexity = function(_v) { algo.presetComplexity = parseInt(_v); };
    algo.getComplexity = function() { return algo.presetComplexity; };

    algo.setReactivity = function(_v) { algo.presetReactivity = parseFloat(_v); };
    algo.getReactivity = function() { return algo.presetReactivity; };

    // 0xFF0080=pink, 0xFFFF00=yellow, 0xFFFFFF=white
    var DEFAULT_BAND_COLORS = [
        {h: 0.917, s: 1.0, v: 1.0},
        {h: 0.167, s: 1.0, v: 1.0},
        {h: 0.0,   s: 0.0, v: 1.0}
    ];
    // Per-track ratios relative to presetSpeed (preserves old PHASE_SPEED_* proportions).
    var MED_RATIO  = 5.0;
    var T4_RATIO   = 2.0;
    var T5_RATIO   = 0.5;
    var FAST_RATIO = 20.0;
    var AUDIO_TIME_BOOST_PER_FRAME_MS = 50;
    var HIT_FLOOR = 0.4;
    var HIT_RANGE = 0.6;
    var STRIPE_MID = 0.3;
    var STRIPE_AMP = 0.2;
    var FLASH_DECAY = 0.15;
    var COLOR_FLOOR = 0.35;
    var BRIGHT_FLOOR = 0.4;
    var phaseSlow = { phase: 0 };
    var phaseMed  = { phase: 0 };
    var phaseT4   = { phase: 0 };
    var phaseT5   = { phase: 0 };
    var phaseFast = { phase: 0 };
    var flashColor = null;
    var flashLevel = 0;

    function triangle(x) { return Math.abs(((x % 1) + 1) % 1 * 2 - 1); }

    function bandColors() {
        if (algo.colors && algo.colors.length >= 3)
            return algo.colors;
        return DEFAULT_BAND_COLORS;
    }

    function dominantBandHsv(audio) {
        var dom = audio.power.dominant;
        var bc = bandColors();
        if (dom === "mid")  return bc[1];
        if (dom === "high") return bc[2];
        return bc[0];
    }

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() {
        return bandColors().slice();
    };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var map = HSVUtil.createMap(width, height);
        if (!audio) return map;

        var dt = audio.timing.consumerDtMs;
        var bpm = (audio && audio.beat) ? audio.beat.bpm : 0;

        var lowPower = audio.power.low;

        var speed = algo.presetSpeed;
        var reactivity = algo.presetReactivity;
        var satThreshold = algo.presetSaturation;
        var complexity = algo.presetComplexity;

        var boost = lowPower * reactivity / Math.max(0.001, speed) * AUDIO_TIME_BOOST_PER_FRAME_MS;

        // t1 and t2 share the slow accumulator (read once, reuse value).
        var slow01 = HSVUtil.beatTime(speed, phaseSlow, bpm, dt + boost);
        var t1 = slow01 * Math.PI * 2;
        var t2 = slow01;
        var t3 = HSVUtil.beatTime(speed * MED_RATIO,  phaseMed,  bpm, dt + boost);
        var t4 = HSVUtil.beatTime(speed * T4_RATIO,   phaseT4,   bpm, dt + boost) * Math.PI * 2;
        var t5 = HSVUtil.beatTime(speed * T5_RATIO,   phaseT5,   bpm, dt + boost);
        var t6 = HSVUtil.beatTime(speed * FAST_RATIO, phaseFast, bpm, dt + boost);

        if (audio.onset.fired || audio.beat.kick) {
            flashColor = dominantBandHsv(audio);
            var hitScale = Math.min(1.0, HIT_FLOOR + HIT_RANGE * audio.onset.intensity);
            flashLevel = hitScale;
        }
        var dominant = (flashLevel > 0.01 && flashColor) ? flashColor : dominantBandHsv(audio);

        for (var x = 0; x < width; x++) {
            var il = (x - width / 2) / width;

            // Glitch: modular arithmetic creates digital artifacts
            var m = STRIPE_MID + triangle(t2) * STRIPE_AMP;
            var c = triangle(t3) * complexity * 2 + 4 * Math.sin(t4);

            var h = ((il * c) % m + m) % m;
            h = h + Math.sin(t1);

            // Saturation from layered triangle waves
            var s1 = triangle((t5 + x / width * 5) % 1);
            s1 = s1 * s1;
            var s2 = triangle((t6 - x / width) % 1);
            s2 = s2 * s2 * s2 * s2;
            var sat = 1 - triangle(s1 * s2);
            sat = Math.max(satThreshold, Math.min(1, sat));

            var hNorm = ((h % 1) + 1) % 1;
            var tAbs = Math.abs(il * 2);
            var glitchMix = 0.55 + (1 - tAbs * 0.3) * 0.45;

            var baseBrightness = Math.max(BRIGHT_FLOOR, flashLevel);
            var brightness = baseBrightness;

            // HSV output: dominant hue shifted by glitch pattern
            var hOut = HSVUtil.mod1(dominant.h + hNorm * (1 - dominant.s) * 0.4);
            var sOut = HSVUtil.clamp01(dominant.s + (1 - dominant.s) * sat * 0.5);
            var vOut = HSVUtil.clamp01((COLOR_FLOOR + glitchMix) * brightness * dominant.v);

            for (var y = 0; y < height; y++)
                HSVUtil.setPixel(map, width, x, y, hOut, sOut, vOut);
        }
        flashLevel = Math.max(0, flashLevel - FLASH_DECAY);

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
