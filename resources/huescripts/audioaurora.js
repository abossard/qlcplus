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

    algo.setSpeed = function(_v) { algo.presetSpeed = HSVUtil.parseNumber(_v, algo.presetSpeed, 0, 10); };
    algo.getSpeed = function() { return algo.presetSpeed; };
    algo.setLayers = function(_v) { algo.presetLayers = Math.round(HSVUtil.parseNumber(_v, algo.presetLayers, 1, 5)); };
    algo.getLayers = function() { return algo.presetLayers; };
    algo.setWaveSize = function(_v) { algo.presetWaveSize = HSVUtil.parseNumber(_v, algo.presetWaveSize, 0, 10); };
    algo.getWaveSize = function() { return algo.presetWaveSize; };

    algo.setReactivity = function(_v) { algo.presetReactivity = HSVUtil.parseNumber(_v, algo.presetReactivity, 0, 1); };
    algo.getReactivity = function() { return algo.presetReactivity; };

    algo.presetSmoothing = 5;
    algo.properties.push(
      "name:presetSmoothing|type:range|display:Smoothing|" +
      "values:1,10|write:setSmoothing|read:getSmoothing");
    algo.setSmoothing = function(_v) { algo.presetSmoothing = Math.round(HSVUtil.parseNumber(_v, algo.presetSmoothing, 1, 10)); };
    algo.getSmoothing = function() { return algo.presetSmoothing; };

    var smoothPow = [0, 0, 0];

    var BEAT_PULSE_AMP = 0.25;
    var LAYER_SPEED_STEP = 0.7;
    var LAYER_PHASE_OFFSET = 2.1;
    var VERT_WAVE_FREQ = 1.5;
    var auroraState = { phase: 0 };

    algo.rgbMapStepCount = function(width, height) { return 1; };

    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var map = HSVUtil.createMap(width, height);
        if (!audio) return map;

        var rawPows = [audio.low, audio.mid, audio.high];
        var frames = HSVUtil.elapsedFrames(audio);
        for (var sb = 0; sb < 3; sb++)
            smoothPow[sb] = HSVUtil.smoothFollow(smoothPow[sb], rawPows[sb], algo.presetSmoothing, frames);

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
                    var power = smoothPow[bandIdx];
                    var col = algo.colors[bandIdx];

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
                    ph = HSVUtil.hueMean(layerHues, layerWeights);
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
