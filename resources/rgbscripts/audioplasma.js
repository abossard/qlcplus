/*
  Q Light Controller Plus
  audioplasma.js

  Copyright (c) QLC+ contributors
  Ported from LedFX "Plasma2d" effect (MIT License)

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
    algo.name = "Audio Plasma";
    algo.author = "Ported from LedFx";
    algo.acceptColors = 2;
    algo.usesAudio = true;
    algo.properties = new Array();

    AudioParams.installContinuous(algo, {gain: 3, reactivity: 2});

    algo.presetSpeed = 5;
    algo.properties.push(
      "name:presetSpeed|type:range|display:Speed|" +
      "values:1,10|write:setSpeed|read:getSpeed");
    algo.presetDensity = 5;
    algo.properties.push(
      "name:presetDensity|type:range|display:Density|" +
      "values:1,10|write:setDensity|read:getDensity");
    algo.presetTwist = 4;
    algo.properties.push(
      "name:presetTwist|type:range|display:Twist|" +
      "values:1,10|write:setTwist|read:getTwist");

    algo.setSpeed = function(_v) { algo.presetSpeed = parseInt(_v); };
    algo.getSpeed = function() { return algo.presetSpeed; };
    algo.setDensity = function(_v) { algo.presetDensity = parseInt(_v); };
    algo.getDensity = function() { return algo.presetDensity; };
    algo.setTwist = function(_v) { algo.presetTwist = parseInt(_v); };
    algo.getTwist = function() { return algo.presetTwist; };

    var startColor = [255, 0, 128];
    var endColor = [0, 128, 255];
    var lowsFilter = null;
    var elapsedSec = 0;
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
            lowsFilter = new AudioDSP.Filter(0.05, AudioParams.filterRise(algo));
            lastTime = Date.now();
            initialized = true;
        }

        var map = RGBUtil.createMap(width, height);
        if (!audio || !audio.mel || audio.mel.length === 0) return map;

        var now = Date.now();
        var dt = (now - lastTime) / 1000.0;
        lastTime = now;
        if (dt <= 0 || dt > 0.2) dt = 0.02;

        var power = lowsFilter.update(audio.bands.low);
        var speed = algo.presetSpeed / 5.0;
        elapsedSec += dt * speed * (1 + power * algo.presetReactivity / 5.0);

        var density = 0.01 + (power * algo.presetDensity / 10.0);
        var twist = algo.presetTwist / 100.0;
        var radius = 0.2;
        var t = elapsedSec;

        // True 2D plasma: different value at every (x,y)
        for (var y = 0; y < height; y++) {
            var py = y * density;
            for (var x = 0; x < width; x++) {
                var px = x * density;

                // Three overlapping sine waves for plasma pattern
                var v1 = Math.sin(px * 0.1 + t) * Math.cos(py * 0.1 - t);
                var v2 = Math.sin((px * 0.1 + py * twist + t) * 2.5);
                var v3 = Math.sin(Math.sqrt(px * px + py * py) * radius - t);

                // Combine and normalize to 0-1
                var plasma = (v1 + v2 + v3 + 3) / 6.0;

                // Map plasma value to gradient color
                var r = startColor[0] + (endColor[0] - startColor[0]) * plasma;
                var g = startColor[1] + (endColor[1] - startColor[1]) * plasma;
                var b = startColor[2] + (endColor[2] - startColor[2]) * plasma;

                var brightness = AudioParams.applyFloor(algo, 1.0);
                map[y][x] = RGBUtil.rgb(r * brightness, g * brightness, b * brightness);
            }
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
