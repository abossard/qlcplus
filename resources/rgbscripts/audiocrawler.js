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

    algo.presetReactivity = 5;
    algo.presetFloor = 0;

    algo.presetSpeed = 5;
    algo.properties.push(
      "name:presetSpeed|type:range|display:Speed|" +
      "values:1,10|write:setSpeed|read:getSpeed");
    algo.presetSway = 5;
    algo.properties.push(
      "name:presetSway|type:range|display:Sway|" +
      "values:1,10|write:setSway|read:getSway");
    algo.presetChop = 5;
    algo.properties.push(
      "name:presetChop|type:range|display:Chop|" +
      "values:1,10|write:setChop|read:getChop");

    algo.setSpeed = function(_v) { algo.presetSpeed = parseInt(_v); };
    algo.getSpeed = function() { return algo.presetSpeed; };
    algo.setSway = function(_v) { algo.presetSway = parseInt(_v); };
    algo.getSway = function() { return algo.presetSway; };
    algo.setChop = function(_v) { algo.presetChop = parseInt(_v); };
    algo.getChop = function() { return algo.presetChop; };

    var DEFAULT_BAND_COLORS = [0x00FF80, 0x80A0FF, 0xFFFFFF];
    var timestep = 0;
    var lastTime = 0;
    var initialized = false;
    var activeColor = null;

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() {
        return AudioColors.bands(algo).slice();
    };


    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        if (!initialized) {
            lastTime = Date.now();
            initialized = true;
        }

        var map = RGBUtil.createMap(width, height);
        if (!audio) return map;

        var now = Date.now();
        var dt = now - lastTime;
        lastTime = now;
        if (dt <= 0 || dt > 200) dt = 20;

        var lowPower = audio.power.low;

        var speed = algo.presetSpeed / 10.0;
        var sway = algo.presetSway / 5.0;
        var chop = algo.presetChop / 5.0;
        var reactivity = algo.presetReactivity / 10.0;

        // Accumulate time with audio modulation
        timestep += dt;
        timestep += lowPower * reactivity * 50;

        // Three time phases at different rates (smooth undulation)
        var t1 = (timestep * speed * sway * 0.0003) % (Math.PI * 20);
        var t2 = (timestep * speed * chop * 0.0005) % (Math.PI * 20);
        var t3 = (timestep * speed * (chop + reactivity * lowPower) * 0.0004) % (Math.PI * 20);
        if (audio.onset.fired || audio.beat.kick || !activeColor) {
            var dominantColor = AudioColors.dominant(algo, audio);
            activeColor = [(dominantColor >> 16) & 0xFF, (dominantColor >> 8) & 0xFF, dominantColor & 0xFF];
        }
        var dominant = activeColor;

        for (var x = 0; x < width; x++) {
            var il = (x - width / 2) / width; // -0.5 to 0.5

            // Crawling hue: modular arithmetic + sine interference
            var stretch = 2.0 + Math.sin(t1) * sway;
            var h = ((il * stretch) % 0.3 + 0.3) % 0.3;
            h = h + Math.sin(t2 + il * 5) * 0.1;

            // Smooth brightness modulation
            var v = Math.sin(t2 + il * chop * 3);
            v = v * v; // Square for contrast

            var hNorm = ((h * 3 + 0.5) % 1 + 1) % 1;
            var colorScale = 0.6 + hNorm * 0.4;
            var r = dominant[0] * colorScale;
            var g = dominant[1] * colorScale;
            var b = dominant[2] * colorScale;

            // Apply brightness
            var baseBright = Math.max(0, Math.min(1, v));
            var bright = algo.presetFloor/100 + (1 - algo.presetFloor/100) * baseBright;

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
