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
    algo.acceptColors = 2;
    algo.usesAudio = true;
    algo.properties = new Array();

    AudioParams.installContinuous(algo, {gain: 5, reactivity: 5});

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

    var startColor = [0, 255, 128];
    var endColor = [128, 0, 255];
    var lowsFilter = null;
    var timestep = 0;
    var lastTime = 0;
    var initialized = false;

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) {
        if (rawColors && rawColors.length >= 1)
            startColor = [(rawColors[0] >> 16) & 0xFF, (rawColors[0] >> 8) & 0xFF, rawColors[0] & 0xFF];
        if (rawColors && rawColors.length >= 2)
            endColor = [(rawColors[1] >> 16) & 0xFF, (rawColors[1] >> 8) & 0xFF, rawColors[1] & 0xFF];
    };
    algo.rgbMapGetColors = function() {
        return [LedFx.rgb(startColor[0], startColor[1], startColor[2]),
                LedFx.rgb(endColor[0], endColor[1], endColor[2])];
    };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        if (!initialized) {
            lowsFilter = AudioParams.createFilter(algo, 0.1);
            lastTime = Date.now();
            initialized = true;
        }

        var map = LedFx.createMap(width, height);
        if (!audio || !audio.spectrum || audio.spectrum.length === 0) return map;

        var now = Date.now();
        var dt = now - lastTime;
        lastTime = now;
        if (dt <= 0 || dt > 200) dt = 20;

        var lowsPower = lowsFilter.update(LedFx.lows_power(audio)) * AudioParams.gainFactor(algo);

        var speed = algo.presetSpeed / 10.0;
        var sway = algo.presetSway / 5.0;
        var chop = algo.presetChop / 5.0;
        var reactivity = algo.presetReactivity / 10.0;

        // Accumulate time with audio modulation
        timestep += dt;
        timestep += lowsPower * reactivity * 50;

        // Three time phases at different rates (smooth undulation)
        var t1 = (timestep * speed * sway * 0.0003) % (Math.PI * 20);
        var t2 = (timestep * speed * chop * 0.0005) % (Math.PI * 20);
        var t3 = (timestep * speed * (chop + reactivity * lowsPower) * 0.0004) % (Math.PI * 20);

        for (var x = 0; x < width; x++) {
            var il = (x - width / 2) / width; // -0.5 to 0.5

            // Crawling hue: modular arithmetic + sine interference
            var stretch = 2.0 + Math.sin(t1) * sway;
            var h = ((il * stretch) % 0.3 + 0.3) % 0.3;
            h = h + Math.sin(t2 + il * 5) * 0.1;

            // Smooth brightness modulation
            var v = Math.sin(t2 + il * chop * 3);
            v = v * v; // Square for contrast

            // Color from hue position in gradient
            var hNorm = ((h * 3 + 0.5) % 1 + 1) % 1;
            var r = startColor[0] + (endColor[0] - startColor[0]) * hNorm;
            var g = startColor[1] + (endColor[1] - startColor[1]) * hNorm;
            var b = startColor[2] + (endColor[2] - startColor[2]) * hNorm;

            // Apply brightness
            var bright = AudioParams.applyFloor(algo, Math.max(0, Math.min(1, v)));

            var packed = LedFx.rgb(r * bright, g * bright, b * bright);
            for (var y = 0; y < height; y++)
                map[y][x] = packed;
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
