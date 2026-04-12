/*
  Q Light Controller Plus
  audiotunnel.js

  Copyright (c) QLC+ contributors

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
    algo.name = "Audio Tunnel";
    algo.author = "QLC+ contributors";
    algo.acceptColors = 2;
    algo.usesAudio = true;
    algo.properties = new Array();

    algo.presetSpeed = 5;
    algo.properties.push(
      "name:presetSpeed|type:range|display:Speed|" +
      "values:1,10|write:setSpeed|read:getSpeed");
    algo.presetRings = 5;
    algo.properties.push(
      "name:presetRings|type:range|display:Ring Count|" +
      "values:2,10|write:setRings|read:getRings");
    algo.presetReactivity = 5;
    algo.properties.push(
      "name:presetReactivity|type:range|display:Reactivity|" +
      "values:1,10|write:setReactivity|read:getReactivity");
    algo.presetShape = 0;
    algo.properties.push(
      "name:presetShape|type:list|display:Shape|" +
      "values:Circle,Diamond,Square|write:setShape|read:getShape");

    algo.setSpeed = function(_v) { algo.presetSpeed = parseInt(_v); };
    algo.getSpeed = function() { return algo.presetSpeed; };
    algo.setRings = function(_v) { algo.presetRings = parseInt(_v); };
    algo.getRings = function() { return algo.presetRings; };
    algo.setReactivity = function(_v) { algo.presetReactivity = parseInt(_v); };
    algo.getReactivity = function() { return algo.presetReactivity; };
    algo.setShape = function(_v) {
        if (_v === "Diamond") algo.presetShape = 1;
        else if (_v === "Square") algo.presetShape = 2;
        else algo.presetShape = 0;
    };
    algo.getShape = function() {
        return ["Circle", "Diamond", "Square"][algo.presetShape];
    };

    var startColor = [0, 128, 255];
    var endColor = [255, 0, 128];
    var lowsFilter = null;
    var phase = 0;
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
            lowsFilter = new LedFx.ExpFilter(0.05, 0.4);
            lastTime = Date.now();
            initialized = true;
        }

        var map = LedFx.createMap(width, height);
        if (!audio || !audio.spectrum || audio.spectrum.length === 0) return map;

        var now = Date.now();
        var dt = (now - lastTime) / 1000.0;
        lastTime = now;
        if (dt <= 0 || dt > 0.2) dt = 0.02;

        var power = lowsFilter.update(LedFx.lows_power(audio));
        var speed = algo.presetSpeed / 5.0;
        phase += dt * speed * (1 + power * algo.presetReactivity / 3.0);

        var cx = width / 2;
        var cy = height / 2;
        var maxDist = Math.sqrt(cx * cx + cy * cy);
        var ringCount = algo.presetRings;

        for (var y = 0; y < height; y++) {
            for (var x = 0; x < width; x++) {
                var dx = x - cx + 0.5;
                var dy = y - cy + 0.5;

                // Distance based on shape
                var dist;
                if (algo.presetShape === 1) // Diamond
                    dist = Math.abs(dx) + Math.abs(dy);
                else if (algo.presetShape === 2) // Square
                    dist = Math.max(Math.abs(dx), Math.abs(dy));
                else // Circle
                    dist = Math.sqrt(dx * dx + dy * dy);

                // Normalize distance and add expanding phase
                var normDist = dist / maxDist;
                var ringPhase = (normDist * ringCount - phase) % 1;
                ringPhase = ((ringPhase % 1) + 1) % 1; // wrap to 0-1

                // Create ring pattern with smooth falloff
                var ringVal = Math.sin(ringPhase * Math.PI * 2) * 0.5 + 0.5;

                // Audio modulates ring brightness
                var bright = ringVal * (0.3 + power * 0.7);

                // Color gradient from center to edge
                var t = normDist;
                var r = startColor[0] + (endColor[0] - startColor[0]) * t;
                var g = startColor[1] + (endColor[1] - startColor[1]) * t;
                var b = startColor[2] + (endColor[2] - startColor[2]) * t;

                map[y][x] = LedFx.rgb(r * bright, g * bright, b * bright);
            }
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
