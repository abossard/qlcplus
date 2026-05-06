/*
  Q Light Controller Plus
  audiomelt.js

  Copyright (c) QLC+ contributors
  Ported from LedFX "Melt" effect (MIT License)

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
    algo.name = "Audio Melt";
    algo.author = "Ported from LedFx";
    algo.acceptColors = 2;
    algo.usesAudio = true;
    algo.properties = new Array();

    AudioParams.installContinuous(algo, {gain: 5, reactivity: 5});

    algo.presetSpeed = 5;
    algo.properties.push(
      "name:presetSpeed|type:range|display:Speed|" +
      "values:1,10|write:setSpeed|read:getSpeed");
    algo.presetColorSpeed = 5;
    algo.properties.push(
      "name:presetColorSpeed|type:range|display:Color Speed|" +
      "values:1,10|write:setColorSpeed|read:getColorSpeed");

    algo.setSpeed = function(_v) { algo.presetSpeed = parseInt(_v); };
    algo.getSpeed = function() { return algo.presetSpeed; };
    algo.setColorSpeed = function(_v) { algo.presetColorSpeed = parseInt(_v); };
    algo.getColorSpeed = function() { return algo.presetColorSpeed; };

    var startColor = [128, 0, 255];
    var endColor = [0, 255, 128];
    var lowsFilter = null;
    var lowsPower = 0;
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
        return [RGBUtil.rgb(startColor[0], startColor[1], startColor[2]),
                RGBUtil.rgb(endColor[0], endColor[1], endColor[2])];
    };


    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        if (!initialized) {
            lowsFilter = new AudioDSP.Filter(0.1, AudioParams.filterRise(algo));
            lastTime = Date.now();
            initialized = true;
        }

        var map = RGBUtil.createMap(width, height);
        if (!audio || !audio.mel || audio.mel.length === 0) return map;

        var now = Date.now();
        var dt = now - lastTime;
        lastTime = now;
        if (dt <= 0 || dt > 200) dt = 20;

        lowsPower = lowsFilter.update(audio.bands.low);

        // Accumulate time with audio reactivity
        var speed = algo.presetSpeed / 10.0;
        var reactivity = algo.presetReactivity / 10.0;
        timestep += dt;
        timestep += lowsPower * reactivity / speed * 50;

        var t1 = (timestep * speed * 0.0005) % 1;
        var t2 = (timestep * speed * 0.00065) % 1;
        var colorT = (timestep * algo.presetColorSpeed * 0.0001) % 1;

        for (var x = 0; x < width; x++) {
            var il = 1 - x / Math.max(1, width - 1);

            // Melt: layered sine waves creating organic patterns
            var v = Math.sin((il + t1) * Math.PI * 2);
            v = Math.sin((v + t1) * Math.PI * 2);
            v = Math.sin((v + t1) * Math.PI * 2);
            v = v * v; // Square for contrast

            // Color cycling: blend between colors based on position + time
            var huePos = (il + t2 + colorT) % 1;
            var r = startColor[0] + (endColor[0] - startColor[0]) * huePos;
            var g = startColor[1] + (endColor[1] - startColor[1]) * huePos;
            var b = startColor[2] + (endColor[2] - startColor[2]) * huePos;

            var bright = AudioParams.applyFloor(algo, v);
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
