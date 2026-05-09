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
    algo.acceptColors = 3; // low/mid/high mel-bank gradient
    algo.usesAudio = true;
    algo.properties = new Array();

    AudioParams.installContinuous(algo, {gain: 3, reactivity: 1});

    algo.presetSpeed = 4;
    algo.properties.push(
      "name:presetSpeed|type:range|display:Speed|" +
      "values:1,10|write:setSpeed|read:getSpeed");
    algo.presetLayers = 3;
    algo.properties.push(
      "name:presetLayers|type:range|display:Layers|" +
      "values:1,5|write:setLayers|read:getLayers");
    algo.presetWaveSize = 5;
    algo.properties.push(
      "name:presetWaveSize|type:range|display:Wave Size|" +
      "values:1,10|write:setWaveSize|read:getWaveSize");
    AudioParams.installBandPowerControls(algo);

    algo.setSpeed = function(_v) { algo.presetSpeed = parseInt(_v); };
    algo.getSpeed = function() { return algo.presetSpeed; };
    algo.setLayers = function(_v) { algo.presetLayers = parseInt(_v); };
    algo.getLayers = function() { return algo.presetLayers; };
    algo.setWaveSize = function(_v) { algo.presetWaveSize = parseInt(_v); };
    algo.getWaveSize = function() { return algo.presetWaveSize; };

    var elapsedSec = 0;
    var lastTime = 0;
    var initialized = false;

    // Default 3-bank aurora palette (low, mid, high). Replaced per-frame by
    // algo.gradientBandColors when the matrix supplies color stops.
    var DEFAULT_BAND_COLORS = [0x6400FF, 0x00FF64, 0xFF8000];
    function bandScaleForColumn(x, width) { return AudioParams.bandScaleForColumn(algo, x, width); }
    function unpackColor(packed) { return AudioParams.colorChannels(packed); }

    algo.rgbMapStepCount = function(width, height) { return 1; };

    // Required by apiVersion 3 loader; ignored — colors come from the
    // auto-injected algo.gradientBandColors / algo.gradientColors instead.
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() {
        return algo.gradientBandColors
            ? algo.gradientBandColors.slice()
            : DEFAULT_BAND_COLORS.slice();
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

        // 3 mel banks, each driving its own gradient color.
        var bandPowers = AudioParams.bandWeights(algo, audio);
        var bandColors = algo.gradientBandColors || DEFAULT_BAND_COLORS;

        var cols = new Array(3);
        for (var k = 0; k < 3; k++) {
            var packed = bandColors[k] | 0;
            cols[k] = [(packed >> 16) & 0xFF, (packed >> 8) & 0xFF, packed & 0xFF];
        }

        var speed = algo.presetSpeed / 5.0;
        var reactivity = algo.presetReactivity / 10.0;
        elapsedSec += dt * speed;

        var waveFreq = algo.presetWaveSize / 5.0;
        var layers = algo.presetLayers;

        // Beat-pulse brightness boost
        var beatBoost = 1.0 + 0.25 * AudioParams.beatPulse(audio);

        // True 2D: multiple sine wave layers drifting across the grid
        for (var y = 0; y < height; y++) {
            var yNorm = y / Math.max(1, height - 1);

            for (var x = 0; x < width; x++) {
                var xNorm = x / Math.max(1, width - 1);
                var r = 0, g = 0, b2 = 0;

                for (var l = 0; l < layers; l++) {
                    var layerSpeed = (l + 1) * 0.7;
                    var layerPhase = l * 2.1;
                    var bandIdx = l % 3;
                    var power = bandPowers[bandIdx];
                    var col = cols[bandIdx];

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
                    b2 += col[2] * intensity / layers;
                }

                var maxChannel = Math.max(r, g, b2) / 255.0;
                if (maxChannel > 0) {
                    var floored = AudioParams.applyFloor(algo, Math.min(1, maxChannel));
                    var floorScale = floored / maxChannel * beatBoost;
                    r *= floorScale;
                    g *= floorScale;
                    b2 *= floorScale;
                }
                map[y][x] = RGBUtil.rgb(
                    Math.min(255, r),
                    Math.min(255, g),
                    Math.min(255, b2)
                );
            }
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
