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
    algo.acceptColors = 3; // low/mid/high mel-bank gradient
    algo.usesAudio = true;
    algo.properties = new Array();

    AudioParams.installContinuous(algo, {gain: 5, reactivity: 5});
    AudioParams.installBandPowerControls(algo);

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

    var DEFAULT_BAND_COLORS = [0xFF0000, 0xFFFF00, 0xFFFFFF];
    var scanPos = 0;
    var returning = false;
    var lastTime = 0;
    var powerFilter = null;
    var initialized = false;
    var activeColor = null;

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() {
        return AudioParams.bandColors(algo, DEFAULT_BAND_COLORS).slice();
    };


    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        if (!initialized) {
            powerFilter = new AudioDSP.Filter(0.1, AudioParams.filterRise(algo));
            lastTime = Date.now();
            initialized = true;
        }

        var map = RGBUtil.createMap(width, height);
        if (!audio || !audio.mel || audio.mel.length === 0) return map;

        // Time delta
        var now = Date.now();
        var deltaSec = (now - lastTime) / 1000.0;
        lastTime = now;
        if (deltaSec <= 0 || deltaSec > 0.2) deltaSec = 0.02;

        // Audio drives speed
        var power = powerFilter.update(audio.lows);
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
        if (AudioParams.anyOnsetFired(audio) || AudioParams.kickFired(audio) || !activeColor) {
            activeColor = AudioParams.colorChannels(
                AudioParams.dominantBandColor(algo, audio, DEFAULT_BAND_COLORS));
        }
        var dominant = activeColor;
        var bgPacked = RGBUtil.rgb(0, 0, 0);
        var scanPacked = RGBUtil.rgb(
            dominant[0] * bright, dominant[1] * bright, dominant[2] * bright);

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
