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

    algo.presetReactivity = 0.1;
    algo.properties.push(
      "name:presetReactivity|type:float|display:Reactivity|" +
      "write:setReactivity|read:getReactivity");
    algo.presetSpeed = 0.8;
    algo.properties.push(
      "name:presetSpeed|type:float|display:Speed|" +
      "write:setSpeed|read:getSpeed");
    algo.presetLayers = 3;
    algo.properties.push(
      "name:presetLayers|type:range|display:Layers|" +
      "values:1,5|write:setLayers|read:getLayers");
    algo.presetWaveSize = 1.0;
    algo.properties.push(
      "name:presetWaveSize|type:float|display:Wave Size|" +
      "write:setWaveSize|read:getWaveSize");

    algo.setSpeed = function(_v) { algo.presetSpeed = parseFloat(_v); };
    algo.getSpeed = function() { return algo.presetSpeed; };
    algo.setLayers = function(_v) { algo.presetLayers = parseInt(_v); };
    algo.getLayers = function() { return algo.presetLayers; };
    algo.setWaveSize = function(_v) { algo.presetWaveSize = parseFloat(_v); };
    algo.getWaveSize = function() { return algo.presetWaveSize; };

    algo.setReactivity = function(_v) { algo.presetReactivity = parseFloat(_v); };
    algo.getReactivity = function() { return algo.presetReactivity; };

    var BEAT_PULSE_AMP = 0.25;
    var LAYER_SPEED_STEP = 0.7;
    var LAYER_PHASE_OFFSET = 2.1;
    var VERT_WAVE_FREQ = 1.5;
    var elapsedSec = 0;

    // Default 3-bank aurora palette (low, mid, high). Replaced per-frame by
    // algo.gradientBandColors when the matrix supplies color stops.
    var DEFAULT_BAND_COLORS = [0x6400FF, 0x00FF64, 0xFF8000];

    algo.rgbMapStepCount = function(width, height) { return 1; };

    // Required by apiVersion 3 loader; ignored — colors come from the
    // auto-injected algo.gradientBandColors instead.
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() {
        return algo.gradientBandColors
            ? algo.gradientBandColors.slice()
            : DEFAULT_BAND_COLORS.slice();
    };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var map = RGBUtil.createMap(width, height);
        if (!audio) return map;

        var dt = audio.timing.consumerDtMs / 1000.0;

        // 3 mel banks, each driving its own gradient color.
        var bandPowers = audio.power.bands;
        var bandColors = algo.gradientBandColors || DEFAULT_BAND_COLORS;

        var cols = new Array(3);
        for (var k = 0; k < 3; k++) {
            var packed = bandColors[k] | 0;
            cols[k] = [(packed >> 16) & 0xFF, (packed >> 8) & 0xFF, packed & 0xFF];
        }

        var speed = algo.presetSpeed;
        var reactivity = algo.presetReactivity;
        elapsedSec += dt * speed;

        var waveFreq = algo.presetWaveSize;
        var layers = algo.presetLayers;

        // Beat-pulse brightness boost
        var beatBoost = 1.0 + BEAT_PULSE_AMP * audio.beat.cosPulse;

        // True 2D: multiple sine wave layers drifting across the grid
        for (var y = 0; y < height; y++) {
            var yNorm = y / Math.max(1, height - 1);

            for (var x = 0; x < width; x++) {
                var xNorm = x / Math.max(1, width - 1);
                var r = 0, g = 0, b2 = 0;

                for (var l = 0; l < layers; l++) {
                    var layerSpeed = (l + 1) * LAYER_SPEED_STEP;
                    var layerPhase = l * LAYER_PHASE_OFFSET;
                    var bandIdx = l % 3;
                    var power = bandPowers[bandIdx];
                    var col = cols[bandIdx];

                    // Horizontal drifting wave with vertical offset
                    var wave = Math.sin(
                        xNorm * waveFreq * Math.PI * 2 +
                        yNorm * (l + 1) * VERT_WAVE_FREQ +
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
                    var floored = Math.min(1, maxChannel);
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
