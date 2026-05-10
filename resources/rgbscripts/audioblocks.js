/*
  Q Light Controller Plus
  audioblocks.js

  Copyright (c) QLC+ contributors
  Ported from LedFX "Block Reflections" effect (MIT License)

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
    algo.name = "Audio Blocks";
    algo.author = "Ported from LedFx";
    algo.acceptColors = 2;
    algo.usesAudio = true;
    algo.properties = new Array();

    algo.presetReactivity = 7;
    algo.presetFloor = 0;

    algo.presetBlockSize = 5;
    algo.properties.push(
      "name:presetBlockSize|type:range|display:Block Size|" +
      "values:1,10|write:setBlockSize|read:getBlockSize");
    algo.presetDecay = 5;
    algo.properties.push(
      "name:presetDecay|type:range|display:Decay|" +
      "values:1,10|write:setDecay|read:getDecay");
    algo.presetReactTo = 0;
    algo.properties.push(
      "name:presetReactTo|type:list|display:React To|" +
      "values:Bass,Mids,Highs,All|write:setReactTo|read:getReactTo");
    algo.presetFill = 0;
    algo.properties.push(
      "name:presetFill|type:list|display:Fill Mode|" +
      "values:Solid,Gradient|write:setFill|read:getFill");

    algo.setBlockSize = function(_v) { algo.presetBlockSize = parseInt(_v); };
    algo.getBlockSize = function() { return algo.presetBlockSize; };
    algo.setDecay = function(_v) { algo.presetDecay = parseInt(_v); };
    algo.getDecay = function() { return algo.presetDecay; };
    algo.setReactTo = function(_v) {
        if (_v === "Mids") algo.presetReactTo = 1;
        else if (_v === "Highs") algo.presetReactTo = 2;
        else if (_v === "All") algo.presetReactTo = 3;
        else algo.presetReactTo = 0;
    };
    algo.getReactTo = function() {
        if (algo.presetReactTo === 1) return "Mids";
        if (algo.presetReactTo === 2) return "Highs";
        if (algo.presetReactTo === 3) return "All";
        return "Bass";
    };
    algo.setFill = function(_v) { algo.presetFill = (_v === "Gradient") ? 1 : 0; };
    algo.getFill = function() { return algo.presetFill ? "Gradient" : "Solid"; };

    var startColor = [255, 0, 64];
    var endColor = [0, 64, 255];
    var blockBrightness = null;

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
        var blockW = Math.max(1, algo.presetBlockSize);
        var numBlocks = Math.ceil(algo.displayWidth / blockW);

        if (!blockBrightness || blockBrightness.length !== numBlocks) {
            blockBrightness = new Array(numBlocks);
            for (var i = 0; i < numBlocks; i++) blockBrightness[i] = 0;
        }

        var map = RGBUtil.createMap(width, height);
        if (!audio) return map;

        // Get audio power based on selected range
        var power;
        if (algo.presetReactTo === 1) power = audio.power.mid;
        else if (algo.presetReactTo === 2) power = audio.power.high;
        else if (algo.presetReactTo === 3) power = audio.volume.normalized;
        else power = audio.power.low;

        // Get spectrum for per-block variation
        var bands = RGBUtil.interpolate(audio.spectrum.full, numBlocks);
        for (var i = 0; i < bands.length; i++)
            bands[i] = Math.min(1, bands[i]);

        // Decay and update blocks
        var decayRate = 1 - algo.presetDecay / 50.0;
        var kickBoost = audio.beat.kick ? 0.3 : 0;
        for (var bi = 0; bi < numBlocks; bi++) {
            blockBrightness[bi] *= decayRate;
            // Trigger blocks based on spectrum + overall power
            var trigger = bands[bi] * power + kickBoost;
            if (trigger > blockBrightness[bi])
                blockBrightness[bi] = Math.min(1, trigger);
        }

        // Render blocks
        var beatPulse = audio.beat.cosPulse;
        for (var bi = 0; bi < numBlocks; bi++) {
            var bright = algo.presetFloor/100 + (1 - algo.presetFloor/100) * blockBrightness[bi];
            if (bright < 0.01) continue;
            
            // Add subtle beat pulse modulation (0-20%)
            bright *= (1.0 + beatPulse * 0.2);

            var t = bi / Math.max(1, numBlocks - 1);
            var r, g, b;
            if (algo.presetFill) {
                // Gradient across blocks
                r = startColor[0] + (endColor[0] - startColor[0]) * t;
                g = startColor[1] + (endColor[1] - startColor[1]) * t;
                b = startColor[2] + (endColor[2] - startColor[2]) * t;
            } else {
                r = startColor[0]; g = startColor[1]; b = startColor[2];
            }

            var packed = RGBUtil.rgb(r * bright, g * bright, b * bright);
            var xStart = bi * blockW;
            var xEnd = Math.min(xStart + blockW, width);

            for (var y = 0; y < height; y++)
                for (var x = xStart; x < xEnd; x++)
                    map[y][x] = packed;
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
