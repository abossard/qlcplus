/*
  Q Light Controller Plus
  audiocrawler.js

  Copyright (c) QLC+ contributors
  Ported from LedFX "Crawler" effect (MIT License)

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
    algo.name = "Audio Crawler";
    algo.author = "Ported from LedFx";
    algo.acceptColors = 3; // low/mid/high mel-bank gradient
    algo.usesAudio = true;
    algo.properties = new Array();

    algo.presetReactivity = 0.5;
    algo.properties.push(
      "name:presetReactivity|type:float|display:Reactivity|" +
      "write:setReactivity|read:getReactivity");
    algo.presetSpeed = 0.075;
    algo.properties.push(
      "name:presetSpeed|type:float|display:Speed (cyc/beat)|" +
      "write:setSpeed|read:getSpeed");
    algo.presetSway = 1.0;
    algo.properties.push(
      "name:presetSway|type:float|display:Sway|" +
      "write:setSway|read:getSway");
    algo.presetChop = 1.0;
    algo.properties.push(
      "name:presetChop|type:float|display:Chop|" +
      "write:setChop|read:getChop");

    algo.setSpeed = function(_v) { algo.presetSpeed = parseFloat(_v); };
    algo.getSpeed = function() { return algo.presetSpeed; };
    algo.setSway = function(_v) { algo.presetSway = parseFloat(_v); };
    algo.getSway = function() { return algo.presetSway; };
    algo.setChop = function(_v) { algo.presetChop = parseFloat(_v); };
    algo.getChop = function() { return algo.presetChop; };

    algo.setReactivity = function(_v) { algo.presetReactivity = parseFloat(_v); };
    algo.getReactivity = function() { return algo.presetReactivity; };

    var DEFAULT_BAND_COLORS = [0x00FF80, 0x80A0FF, 0xFFFFFF];
    var CHOP_RATIO = 1.67;   // chop is 1.67× the base sway rate
    var AUDIO_BOOST_MS_PER_FRAME = 50;
    var HUE_BAND = 0.3;
    var HUE_PERTURB = 0.1;
    var COLOR_FLOOR = 0.6;
    var BASE_STRETCH = 2.0;
    var swayState = { phase: 0 };
    var chopState = { phase: 0 };
    var activeColor = null;

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() {
        return AudioColors.bands(algo).slice();
    };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var map = RGBUtil.createMap(width, height);
        if (!audio) return map;

        var dtMs = audio.timing.consumerDtMs;
        var bpm = (audio && audio.beat) ? audio.beat.bpm : 0;

        var lowPower = audio.power.low;

        var speed = algo.presetSpeed;
        var sway = algo.presetSway;
        var chop = algo.presetChop;
        var reactivity = algo.presetReactivity;

        // Audio modulation extends the effective dt for this frame
        var boost = lowPower * reactivity * AUDIO_BOOST_MS_PER_FRAME;

        // Two BPM-locked phases at different rates (smooth undulation)
        var t1 = RGBUtil.beatAngle(speed * sway, swayState, bpm, dtMs + boost);
        var t2 = RGBUtil.beatAngle(speed * CHOP_RATIO * chop, chopState, bpm, dtMs + boost);
        if (audio.onset.fired || audio.beat.kick || !activeColor) {
            var dominantColor = AudioColors.dominant(algo, audio);
            activeColor = [(dominantColor >> 16) & 0xFF, (dominantColor >> 8) & 0xFF, dominantColor & 0xFF];
        }
        var dominant = activeColor;

        for (var x = 0; x < width; x++) {
            var il = (x - width / 2) / width; // -0.5 to 0.5

            // Crawling hue: modular arithmetic + sine interference
            var stretch = BASE_STRETCH + Math.sin(t1) * sway;
            var h = ((il * stretch) % HUE_BAND + HUE_BAND) % HUE_BAND;
            h = h + Math.sin(t2 + il * 5) * HUE_PERTURB;

            // Smooth brightness modulation
            var v = Math.sin(t2 + il * chop * 3);
            v = v * v; // Square for contrast

            var hNorm = ((h * 3 + 0.5) % 1 + 1) % 1;
            var colorScale = COLOR_FLOOR + hNorm * 0.4;
            var r = dominant[0] * colorScale;
            var g = dominant[1] * colorScale;
            var b = dominant[2] * colorScale;

            // Apply brightness
            var baseBright = Math.max(0, Math.min(1, v));
            var bright = baseBright;

            var packed = RGBUtil.rgb(r * bright, g * bright, b * bright);
            for (var y = 0; y < height; y++)
                map[y][x] = packed;
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
