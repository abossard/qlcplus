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
    algo.presetSpeed = 0.5;
    algo.properties.push(
      "name:presetSpeed|type:float|display:Speed|" +
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

    var DEFAULT_BAND_COLORS = [0xFF0080, 0xFFFF00, 0xFFFFFF];
    var PHASE_RATE_SLOW = 0.0005;
    var PHASE_RATE_MED = 0.0025;
    var PHASE_RATE_T4 = 0.001;
    var PHASE_RATE_T5 = 0.00025;
    var PHASE_RATE_FAST = 0.01;
    var HIT_FLOOR = 0.4;
    var HIT_RANGE = 0.6;
    var STRIPE_MID = 0.3;
    var STRIPE_AMP = 0.2;
    var FLASH_DECAY = 0.15;
    var COLOR_FLOOR = 0.35;
    var BRIGHT_FLOOR = 0.4;
    var timestep = 0;
    var flashColor = null;
    var flashLevel = 0;

    function triangle(x) { return Math.abs(((x % 1) + 1) % 1 * 2 - 1); }

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() {
        return AudioColors.bands(algo).slice();
    };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var map = RGBUtil.createMap(width, height);
        if (!audio) return map;

        var dt = audio.timing.consumerDtMs;

        var lowPower = audio.power.low;

        var speed = algo.presetSpeed;
        var reactivity = algo.presetReactivity;
        var satThreshold = algo.presetSaturation;
        var complexity = algo.presetComplexity;

        timestep += dt;
        timestep += lowPower * reactivity / speed * 50;

        var t1 = (timestep * speed * PHASE_RATE_SLOW) * Math.PI * 2;
        var t2 = (timestep * speed * PHASE_RATE_SLOW) % 1;
        var t3 = (timestep * speed * PHASE_RATE_MED) % 1;
        var t4 = (timestep * speed * PHASE_RATE_T4) * Math.PI * 2;
        var t5 = (timestep * speed * PHASE_RATE_T5) % 1;
        var t6 = (timestep * speed * PHASE_RATE_FAST) % 1;
        if (audio.onset.fired || audio.beat.kick) {
            var flashPacked = AudioColors.dominant(algo, audio);
            flashColor = [(flashPacked >> 16) & 0xFF, (flashPacked >> 8) & 0xFF, flashPacked & 0xFF];
            var hitScale = Math.min(1.0, HIT_FLOOR + HIT_RANGE * audio.onset.intensity);
            flashLevel = hitScale;
        }
        var dominantPacked = AudioColors.dominant(algo, audio);
        var dominant = (flashLevel > 0.01 && flashColor) ? flashColor :
            [(dominantPacked >> 16) & 0xFF, (dominantPacked >> 8) & 0xFF, dominantPacked & 0xFF];

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

            // Map to colors using HSV-like approach
            var hNorm = ((h % 1) + 1) % 1;
            var c1 = RGBUtil.hsv2rgb(hNorm, sat, 1);

            var t = Math.abs(il * 2);
            var glitchMix = 0.55 + (1 - t * 0.3) * 0.45;
            var r = dominant[0] * (COLOR_FLOOR + c1[0] / 255.0 * glitchMix);
            var g = dominant[1] * (COLOR_FLOOR + c1[1] / 255.0 * glitchMix);
            var b = dominant[2] * (COLOR_FLOOR + c1[2] / 255.0 * glitchMix);

            var baseBrightness = Math.max(BRIGHT_FLOOR, flashLevel);
            var brightness = baseBrightness;
            var packed = RGBUtil.rgb(r * brightness, g * brightness, b * brightness);
            for (var y = 0; y < height; y++)
                map[y][x] = packed;
        }
        flashLevel = Math.max(0, flashLevel - FLASH_DECAY);

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
