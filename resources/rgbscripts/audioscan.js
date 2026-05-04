/*
  Q Light Controller Plus
  audioscan.js

  Copyright (c) QLC+ contributors
  Ported from LedFX "Scan" effect (MIT License)

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
    algo.name = "Audio Scan";
    algo.author = "Ported from LedFx";
    algo.acceptColors = 2;
    algo.usesAudio = true;
    algo.properties = new Array();

    AudioParams.installContinuous(algo, {gain: 5, reactivity: 5});

    algo.presetSpeed = 5;
    algo.properties.push(
      "name:presetSpeed|type:range|display:Speed|" +
      "values:1,10|write:setSpeed|read:getSpeed");

    algo.presetWidth = 3;
    algo.properties.push(
      "name:presetWidth|type:range|display:Scan Width|" +
      "values:1,10|write:setWidth|read:getWidth");

    algo.presetBounce = 0;
    algo.properties.push(
      "name:presetBounce|type:list|display:Bounce|" +
      "values:Yes,No|write:setBounce|read:getBounce");

    algo.setSpeed = function(_v) { algo.presetSpeed = parseInt(_v); };
    algo.getSpeed = function() { return algo.presetSpeed; };
    algo.setWidth = function(_v) { algo.presetWidth = parseInt(_v); };
    algo.getWidth = function() { return algo.presetWidth; };
    algo.setBounce = function(_v) { algo.presetBounce = (_v === "Yes") ? 1 : 0; };
    algo.getBounce = function() { return algo.presetBounce ? "Yes" : "No"; };

    var scanColor = [255, 0, 0];
    var bgColor = [0, 0, 0];
    var scanPos = 0;
    var returning = false;
    var lastTime = 0;
    var powerFilter = null;
    var initialized = false;

    algo.rgbMapStepCount = function(width, height) { return 1; };

    algo.rgbMapSetColors = function(rawColors) {
        if (rawColors && rawColors.length >= 1)
            scanColor = [(rawColors[0] >> 16) & 0xFF, (rawColors[0] >> 8) & 0xFF, rawColors[0] & 0xFF];
        if (rawColors && rawColors.length >= 2)
            bgColor = [(rawColors[1] >> 16) & 0xFF, (rawColors[1] >> 8) & 0xFF, rawColors[1] & 0xFF];
    };

    algo.rgbMapGetColors = function() {
        return [LedFx.rgb(scanColor[0], scanColor[1], scanColor[2]),
                LedFx.rgb(bgColor[0], bgColor[1], bgColor[2])];
    };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        if (!initialized) {
            powerFilter = AudioParams.createFilter(algo, 0.1);
            lastTime = Date.now();
            initialized = true;
        }

        var map = LedFx.createMap(width, height);
        if (!audio || !audio.spectrum || audio.spectrum.length === 0) return map;

        // Time delta
        var now = Date.now();
        var deltaSec = (now - lastTime) / 1000.0;
        lastTime = now;
        if (deltaSec <= 0 || deltaSec > 0.2) deltaSec = 0.02;

        // Audio drives speed
        var power = powerFilter.update(LedFx.lows_power(audio)) * AudioParams.gainFactor(algo);
        var speedMult = 1 + power * 8;
        var baseSpeed = algo.presetSpeed * 5;
        var stepSize = deltaSec * baseSpeed * speedMult;

        // Scan width in pixels — widens with bass
        var scanW = Math.max(1, Math.round(width * algo.presetWidth / 20));
        scanW = Math.min(width, scanW + Math.round(power * 4));

        // Move scan position
        if (algo.presetBounce) {
            if (returning) scanPos -= stepSize;
            else scanPos += stepSize;

            if (scanPos > width - scanW) { returning = true; scanPos = width - scanW; }
            if (scanPos < 0) { returning = false; scanPos = 0; }
        } else {
            scanPos += stepSize;
            if (scanPos >= width) scanPos -= width;
        }

        // Brightness varies with audio power
        var bright = AudioParams.applyFloor(algo, Math.min(1, 0.3 + power * 2.0));

        // Fill background
        var bgPacked = LedFx.rgb(bgColor[0], bgColor[1], bgColor[2]);
        var scanPacked = LedFx.rgb(
            scanColor[0] * bright, scanColor[1] * bright, scanColor[2] * bright);

        for (var y = 0; y < height; y++) {
            for (var x = 0; x < width; x++) {
                // Check if pixel is within scan beam
                var inScan = false;
                var sp = Math.floor(scanPos);
                for (var s = 0; s < scanW; s++) {
                    var sx = (sp + s) % width;
                    if (x === sx) { inScan = true; break; }
                }
                map[y][x] = inScan ? scanPacked : bgPacked;
            }
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
