/*
  Q Light Controller Plus
  audiolava.js

  Copyright (c) QLC+ contributors
  Ported from LedFX "Lava lamp" effect (MIT License)

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
    algo.name = "Audio Lava Lamp";
    algo.author = "Ported from LedFx";
    algo.acceptColors = 3; // bass, mids, highs
    algo.usesAudio = true;
    algo.properties = new Array();

    AudioParams.installContinuous(algo, {gain: 5, reactivity: 5});

    algo.presetSpeed = 7;
    algo.properties.push(
      "name:presetSpeed|type:range|display:Speed|" +
      "values:1,15|write:setSpeed|read:getSpeed");
    algo.presetContrast = 6;
    algo.properties.push(
      "name:presetContrast|type:range|display:Contrast|" +
      "values:0,10|write:setContrast|read:getContrast");

    algo.setSpeed = function(_v) { algo.presetSpeed = parseInt(_v); };
    algo.getSpeed = function() { return algo.presetSpeed; };
    algo.setContrast = function(_v) { algo.presetContrast = parseInt(_v); };
    algo.getContrast = function() { return algo.presetContrast; };

    var color1 = [255, 0, 128];
    var color2 = [0, 128, 255];
    var color3 = [128, 255, 0];
    var lowsFilter = null;
    var midsFilter = null;
    var highsFilter = null;
    var lowsPower = 0;
    var elapsedMs = 0;
    var lastTime = 0;
    var initialized = false;

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) {
        if (rawColors && rawColors.length >= 1)
            color1 = [(rawColors[0] >> 16) & 0xFF, (rawColors[0] >> 8) & 0xFF, rawColors[0] & 0xFF];
        if (rawColors && rawColors.length >= 2)
            color2 = [(rawColors[1] >> 16) & 0xFF, (rawColors[1] >> 8) & 0xFF, rawColors[1] & 0xFF];
        if (rawColors && rawColors.length >= 3)
            color3 = [(rawColors[2] >> 16) & 0xFF, (rawColors[2] >> 8) & 0xFF, rawColors[2] & 0xFF];
    };
    algo.rgbMapGetColors = function() {
        return [RGBUtil.rgb(color1[0], color1[1], color1[2]),
                RGBUtil.rgb(color2[0], color2[1], color2[2]),
                RGBUtil.rgb(color3[0], color3[1], color3[2])];
    };


    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        if (!initialized) {
            lastTime = Date.now();
            initialized = true;
        }

        var map = RGBUtil.createMap(width, height);
        if (!audio || !audio.mel || audio.mel.length === 0) return map;

        var now = Date.now();
        var dt = (now - lastTime) / 1000.0;
        lastTime = now;
        if (dt <= 0 || dt > 0.2) dt = 0.02;
        elapsedMs += dt * 1000;

        lowsPower = audio.bands.low;
        var midsPower = audio.bands.mid;
        var highsPower = audio.bands.high;
        var speed = algo.presetSpeed;
        var contrast = 1 - algo.presetContrast / 10.0;

        // Time phases — bass accelerates significantly
        var t1 = (elapsedMs * speed * 0.0001 * Math.max(1, 1 + lowsPower * 2)) % 1;
        var t2 = (elapsedMs * speed * 0.0002 * Math.max(1, 1 + lowsPower * 3)) % 1;

        // True 2D: each pixel gets unique wave value
        for (var y = 0; y < height; y++) {
            var yNorm = y / Math.max(1, height - 1);

            for (var x = 0; x < width; x++) {
                var xNorm = x / Math.max(1, width - 1);

                // Three wave layers for lava pattern
                var w1 = Math.sin((t1 + xNorm + yNorm * 0.5) * Math.PI * 2);
                var w2 = Math.sin((t2 - xNorm + yNorm * 0.7) * Math.PI * 2);
                var w3 = Math.sin((xNorm + yNorm + w1 + w2) * Math.PI * 2);

                // Combine waves for pattern
                var pattern = (w1 + 0.1) * (w2 + lowsPower * 2) * (w3 + midsPower * 1.5);
                pattern = Math.pow(Math.max(0, pattern + contrast), 2);
                pattern = AudioParams.applyFloor(algo, Math.min(1, pattern));

                // Mix 3 colors weighted by frequency band powers
                var r = color1[0] * lowsPower + color2[0] * midsPower + color3[0] * highsPower;
                var g = color1[1] * lowsPower + color2[1] * midsPower + color3[1] * highsPower;
                var b = color1[2] * lowsPower + color2[2] * midsPower + color3[2] * highsPower;

                // Normalize and apply pattern
                var total = Math.max(0.01, lowsPower + midsPower + highsPower);
                map[y][x] = RGBUtil.rgb(
                    r / total * pattern,
                    g / total * pattern,
                    b / total * pattern
                );
            }
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
