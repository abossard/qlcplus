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
    algo.acceptColors = 2;
    algo.usesAudio = true;
    algo.properties = new Array();

    algo.presetSpeed = 5;
    algo.properties.push(
      "name:presetSpeed|type:range|display:Speed|" +
      "values:1,10|write:setSpeed|read:getSpeed");
    algo.presetReactivity = 3;
    algo.properties.push(
      "name:presetReactivity|type:range|display:Reactivity|" +
      "values:1,10|write:setReactivity|read:getReactivity");
    algo.presetSaturation = 10;
    algo.properties.push(
      "name:presetSaturation|type:range|display:Saturation|" +
      "values:0,10|write:setSaturation|read:getSaturation");
    algo.presetComplexity = 5;
    algo.properties.push(
      "name:presetComplexity|type:range|display:Complexity|" +
      "values:1,10|write:setComplexity|read:getComplexity");

    algo.setSpeed = function(_v) { algo.presetSpeed = parseInt(_v); };
    algo.getSpeed = function() { return algo.presetSpeed; };
    algo.setReactivity = function(_v) { algo.presetReactivity = parseInt(_v); };
    algo.getReactivity = function() { return algo.presetReactivity; };
    algo.setSaturation = function(_v) { algo.presetSaturation = parseInt(_v); };
    algo.getSaturation = function() { return algo.presetSaturation; };
    algo.setComplexity = function(_v) { algo.presetComplexity = parseInt(_v); };
    algo.getComplexity = function() { return algo.presetComplexity; };

    var startColor = [255, 0, 128];
    var endColor = [0, 255, 255];
    var lowsPower = 0;
    var lowsFilter = null;
    var timestep = 0;
    var lastTime = 0;
    var initialized = false;

    function triangle(x) { return Math.abs(((x % 1) + 1) % 1 * 2 - 1); }

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
        if (!initialized) { lowsFilter = new LedFx.ExpFilter(0.05, 0.95); lastTime = Date.now(); initialized = true; }

        var map = LedFx.createMap(width, height);
        if (!audio || !audio.spectrum || audio.spectrum.length === 0) return map;

        var now = Date.now();
        var dt = now - lastTime;
        lastTime = now;
        if (dt <= 0 || dt > 200) dt = 20;

        lowsPower = lowsFilter.update(LedFx.lows_power(audio));

        var speed = algo.presetSpeed / 10.0;
        var reactivity = algo.presetReactivity / 10.0;
        var satThreshold = algo.presetSaturation / 10.0;
        var complexity = algo.presetComplexity;

        timestep += dt;
        timestep += lowsPower * reactivity / speed * 50;

        var t1 = (timestep * speed * 0.0005) * Math.PI * 2;
        var t2 = (timestep * speed * 0.0005) % 1;
        var t3 = (timestep * speed * 0.0025) % 1;
        var t4 = (timestep * speed * 0.001) * Math.PI * 2;
        var t5 = (timestep * speed * 0.00025) % 1;
        var t6 = (timestep * speed * 0.01) % 1;

        for (var x = 0; x < width; x++) {
            var il = (x - width / 2) / width;

            // Glitch: modular arithmetic creates digital artifacts
            var m = 0.3 + triangle(t2) * 0.2;
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
            var c1 = LedFx.hsv2rgb(hNorm, sat, 1);

            // Blend with user colors based on position
            var t = Math.abs(il * 2);
            var r = c1[0] * (1 - t * 0.3) + startColor[0] * t * 0.3;
            var g = c1[1] * (1 - t * 0.3) + endColor[1] * t * 0.3;
            var b = c1[2] * (1 - t * 0.3) + endColor[2] * t * 0.3;

            var packed = LedFx.rgb(r, g, b);
            for (var y = 0; y < height; y++)
                map[y][x] = packed;
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
