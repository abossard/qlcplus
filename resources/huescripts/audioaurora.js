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
    algo.presetSpeed = 0.4;
    algo.properties.push(
      "name:presetSpeed|type:float|display:Speed (cyc/beat)|" +
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

    algo.presetSmoothing = 5;
    algo.properties.push(
      "name:presetSmoothing|type:range|display:Smoothing|" +
      "values:1,10|write:setSmoothing|read:getSmoothing");
    algo.setSmoothing = function(_v) { algo.presetSmoothing = parseInt(_v); };
    algo.getSmoothing = function() { return algo.presetSmoothing; };

    var smoothPow = [0, 0, 0];

    var BEAT_PULSE_AMP = 0.25;
    var LAYER_SPEED_STEP = 0.7;
    var LAYER_PHASE_OFFSET = 2.1;
    var VERT_WAVE_FREQ = 1.5;
    var auroraState = { phase: 0 };

    // Default 3-bank aurora palette (low, mid, high) as HSV.
    var DEFAULT_BAND_COLORS = [
        { h: 0.731, s: 1.0, v: 1.0 },  // purple  (0x6400FF)
        { h: 0.398, s: 1.0, v: 1.0 },  // green   (0x00FF64)
        { h: 0.083, s: 1.0, v: 1.0 }   // orange  (0xFF8000)
    ];

    algo.rgbMapStepCount = function(width, height) { return 1; };

    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };

    // Weighted circular hue average: sums sin/cos of hue angles weighted by
    // intensity, then recovers the mean hue via atan2.
    function blendHueWeighted(hues, weights) {
        var sx = 0, sy = 0, tw = 0;
        for (var i = 0; i < hues.length; i++) {
            var w = weights[i];
            if (w <= 0) continue;
            var a = hues[i] * Math.PI * 2;
            sx += Math.cos(a) * w;
            sy += Math.sin(a) * w;
            tw += w;
        }
        if (tw <= 0) return 0;
        var angle = Math.atan2(sy, sx);
        if (angle < 0) angle += Math.PI * 2;
        return angle / (Math.PI * 2);
    }

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var map = HSVUtil.createMap(width, height);
        if (!audio) return map;

        var dt = audio.dt;

        // Asymmetric EMA smoothing of band powers (fast attack, slow decay)
        var smoothing = algo.presetSmoothing / 10.0;
        var riseAlpha = 0.5 * (1 - smoothing) + 0.05;
        var decayAlpha = 0.02 + 0.03 * (1 - smoothing);
        var rawPows = [audio.low, audio.mid, audio.high];
        for (var sb = 0; sb < 3; sb++) {
            var sa = rawPows[sb] > smoothPow[sb] ? riseAlpha : decayAlpha;
            smoothPow[sb] += sa * (rawPows[sb] - smoothPow[sb]);
        }
        var bandPowers = smoothPow;
        var bandColors = algo.colors || DEFAULT_BAND_COLORS;

        var speed = algo.presetSpeed;
        var reactivity = algo.presetReactivity;
        auroraState.phase = (auroraState.phase + audio.dt * speed) % 1.0;
        var theta = auroraState.phase * Math.PI * 2;

        var waveFreq = algo.presetWaveSize;
        var layers = algo.presetLayers;

        var beatBoost = 1.0 + BEAT_PULSE_AMP * audio.cosPulse;

        var layerHues = [];
        var layerWeights = [];

        for (var y = 0; y < height; y++) {
            var yNorm = y / Math.max(1, height - 1);

            for (var x = 0; x < width; x++) {
                var xNorm = x / Math.max(1, width - 1);
                var totalIntensity = 0;
                layerHues.length = 0;
                layerWeights.length = 0;
                var satAccum = 0;

                for (var l = 0; l < layers; l++) {
                    var layerSpeed = (l + 1) * LAYER_SPEED_STEP;
                    var layerPhase = l * LAYER_PHASE_OFFSET;
                    var bandIdx = l % 3;
                    var power = bandPowers[bandIdx];
                    var col = bandColors[bandIdx];

                    var wave = Math.sin(
                        xNorm * waveFreq * Math.PI * 2 +
                        yNorm * (l + 1) * VERT_WAVE_FREQ +
                        theta * layerSpeed +
                        layerPhase +
                        power * reactivity * 3
                    );

                    var vertWave = Math.sin(
                        yNorm * Math.PI * 2 * (l + 1) +
                        theta * layerSpeed * 0.5 -
                        xNorm * 2
                    );

                    var intensity = (wave * 0.5 + 0.5) * (vertWave * 0.3 + 0.7);
                    intensity *= power;
                    var contribution = intensity * col.v / layers;

                    layerHues.push(col.h);
                    layerWeights.push(contribution);
                    satAccum += col.s * contribution;
                    totalIntensity += contribution;
                }

                var ph = 0, ps = 0, pv = 0;
                if (totalIntensity > 0) {
                    ph = blendHueWeighted(layerHues, layerWeights);
                    ps = satAccum / totalIntensity;
                    pv = Math.min(1, totalIntensity * beatBoost);
                }

                HSVUtil.setPixel(map, width, x, y, ph, ps, pv);
            }
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
