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

    algo.presetReactivity = 5;
    algo.presetFloor = 0;

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

    var SCAN_SPEED_SCALE = 5;
    var SCAN_WIDTH_DIVISOR = 20;
    var scanPos = 0;
    var returning = false;
    var activeColor = null;
    var lastWidth = 0;

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() {
        return AudioColors.bands(algo).slice();
    };


    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        if (lastWidth !== width) {
            scanPos = 0;
            returning = false;
            lastWidth = width;
        }

        var map = RGBUtil.createMap(width, height);
        if (!audio) return map;

        var deltaSec = audio.timing.consumerDtMs / 1000.0;

        var power = audio.power.low;
        var speedMult = 1 + power * 8;
        var stepSize = deltaSec * algo.presetSpeed * SCAN_SPEED_SCALE * speedMult;

        var scanW = Math.max(1, Math.round(width * algo.presetWidth / SCAN_WIDTH_DIVISOR));
        scanW = Math.min(width, scanW + Math.round(power * 4));

        // Move scan position
        if (algo.presetBounce) {
            scanPos += returning ? -stepSize : stepSize;
            if (scanPos > width - scanW) { returning = true; scanPos = width - scanW; }
            if (scanPos < 0) { returning = false; scanPos = 0; }
        } else {
            scanPos += stepSize;
            if (scanPos >= width) scanPos -= width;
        }

        var baseBright = Math.min(1, 0.3 + power * 2.0);
        var bright = algo.presetFloor/100 + (1 - algo.presetFloor/100) * baseBright;

        if (audio.onset.fired || audio.beat.kick || !activeColor) {
            var dominantColor = AudioColors.dominant(algo, audio);
            activeColor = [(dominantColor >> 16) & 0xFF, (dominantColor >> 8) & 0xFF, dominantColor & 0xFF];
        }
        var bgPacked = 0;
        var scanPacked = RGBUtil.rgb(
            activeColor[0] * bright, activeColor[1] * bright, activeColor[2] * bright);

        var sp = Math.floor(scanPos);
        for (var y = 0; y < height; y++) {
            for (var x = 0; x < width; x++) {
                var d = (x - sp + width) % width;
                map[y][x] = (d < scanW) ? scanPacked : bgPacked;
            }
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
