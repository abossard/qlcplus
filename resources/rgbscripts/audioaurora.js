/*
  Q Light Controller Plus
  audioaurora.js

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
    algo.name = "Audio Aurora";
    algo.author = "QLC+ contributors";
    algo.acceptColors = 3;
    algo.usesAudio = true;
    algo.properties = new Array();

    algo.presetSpeed = 4;
    algo.properties.push(
      "name:presetSpeed|type:range|display:Speed|" +
      "values:1,10|write:setSpeed|read:getSpeed");
    algo.presetLayers = 3;
    algo.properties.push(
      "name:presetLayers|type:range|display:Layers|" +
      "values:1,5|write:setLayers|read:getLayers");
    algo.presetReactivity = 5;
    algo.properties.push(
      "name:presetReactivity|type:range|display:Reactivity|" +
      "values:1,10|write:setReactivity|read:getReactivity");
    algo.presetWaveSize = 5;
    algo.properties.push(
      "name:presetWaveSize|type:range|display:Wave Size|" +
      "values:1,10|write:setWaveSize|read:getWaveSize");

    algo.setSpeed = function(_v) { algo.presetSpeed = parseInt(_v); };
    algo.getSpeed = function() { return algo.presetSpeed; };
    algo.setLayers = function(_v) { algo.presetLayers = parseInt(_v); };
    algo.getLayers = function() { return algo.presetLayers; };
    algo.setReactivity = function(_v) { algo.presetReactivity = parseInt(_v); };
    algo.getReactivity = function() { return algo.presetReactivity; };
    algo.setWaveSize = function(_v) { algo.presetWaveSize = parseInt(_v); };
    algo.getWaveSize = function() { return algo.presetWaveSize; };

    var color1 = [0, 255, 100];
    var color2 = [0, 100, 255];
    var color3 = [128, 0, 255];
    var lowsFilter = null;
    var midsFilter = null;
    var highsFilter = null;
    var elapsedSec = 0;
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
        return [LedFx.rgb(color1[0], color1[1], color1[2]),
                LedFx.rgb(color2[0], color2[1], color2[2]),
                LedFx.rgb(color3[0], color3[1], color3[2])];
    };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        if (!initialized) {
            lowsFilter = new LedFx.ExpFilter(0.03, 0.2);
            midsFilter = new LedFx.ExpFilter(0.03, 0.3);
            highsFilter = new LedFx.ExpFilter(0.03, 0.4);
            lastTime = Date.now();
            initialized = true;
        }

        var map = LedFx.createMap(width, height);
        if (!audio || !audio.spectrum || audio.spectrum.length === 0) return map;

        var now = Date.now();
        var dt = (now - lastTime) / 1000.0;
        lastTime = now;
        if (dt <= 0 || dt > 0.2) dt = 0.02;

        var lows = lowsFilter.update(LedFx.lows_power(audio));
        var mids = midsFilter.update(LedFx.mids_power(audio));
        var highs = highsFilter.update(LedFx.high_power(audio));

        var speed = algo.presetSpeed / 5.0;
        var reactivity = algo.presetReactivity / 10.0;
        elapsedSec += dt * speed;

        var waveFreq = algo.presetWaveSize / 5.0;
        var layers = algo.presetLayers;
        var colors = [color1, color2, color3];

        // True 2D: multiple sine wave layers drifting across the grid
        for (var y = 0; y < height; y++) {
            var yNorm = y / Math.max(1, height - 1);

            for (var x = 0; x < width; x++) {
                var xNorm = x / Math.max(1, width - 1);
                var r = 0, g = 0, b = 0;

                for (var l = 0; l < layers; l++) {
                    var layerSpeed = (l + 1) * 0.7;
                    var layerPhase = l * 2.1;
                    var power = [lows, mids, highs][l % 3];
                    var col = colors[l % 3];

                    // Horizontal drifting wave with vertical offset
                    var wave = Math.sin(
                        xNorm * waveFreq * Math.PI * 2 +
                        yNorm * (l + 1) * 1.5 +
                        elapsedSec * layerSpeed +
                        layerPhase +
                        power * reactivity * 3
                    );

                    // Vertical undulation
                    var vertWave = Math.sin(
                        yNorm * Math.PI * 2 * (l + 1) +
                        elapsedSec * layerSpeed * 0.5 -
                        xNorm * 2
                    );

                    var intensity = (wave * 0.5 + 0.5) * (vertWave * 0.3 + 0.7);
                    intensity *= power; // band power directly controls visibility

                    r += col[0] * intensity / layers;
                    g += col[1] * intensity / layers;
                    b += col[2] * intensity / layers;
                }

                map[y][x] = LedFx.rgb(
                    Math.min(255, r),
                    Math.min(255, g),
                    Math.min(255, b)
                );
            }
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
