/*
  Q Light Controller Plus
  audiomarching.js

  Copyright (c) QLC+ contributors

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

var testAlgo;

(function() {
    var algo = {};
    algo.apiVersion = 3;
    algo.name = "Audio Marching";
    algo.author = "QLC+ contributors";
    algo.acceptColors = 5;
    algo.usesAudio = true;
    algo.properties = [];

    algo.presetSpeed = 0.1;
    algo.presetReactivity = 0.2;
    algo.properties.push("name:speed|type:float|values:0.00001,1|display:Speed|write:setSpeed|read:getSpeed");
    algo.properties.push("name:reactivity|type:float|values:0.00001,1|display:Reactivity|write:setReactivity|read:getReactivity");
    algo.setSpeed = function(v) { algo.presetSpeed = Math.max(0.00001, Math.min(1, parseFloat(v) || 0.00001)); };
    algo.getSpeed = function() { return algo.presetSpeed; };
    algo.setReactivity = function(v) { algo.presetReactivity = Math.max(0.00001, Math.min(1, parseFloat(v) || 0.00001)); };
    algo.getReactivity = function() { return algo.presetReactivity; };
    var outputOptions = HSVUtil.attachStripTransformControls(algo);

    var state = { t1: 0, t2: 0, lowsPower: 0, audioKey: "", geometryKey: "" };

    function resetState() {
        state.t1 = 0;
        state.t2 = 0;
        state.lowsPower = 0;
    }

    algo.rgbMapStepCount = function() { return 1; };
    algo.rgbMapSetColors = function() {};
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step, audio) {
        var map = HSVUtil.createMap(width, height);
        var audioKey = HSVUtil.audioIdentityKey(audio, state.audioKey);
        var geometryKey = width + "x" + height;
        if (audioKey !== state.audioKey || geometryKey !== state.geometryKey) {
            resetState();
            state.audioKey = audioKey;
            state.geometryKey = geometryKey;
        }
        var dt = HSVUtil.audioSeconds(audio);
        var rawLow = HSVUtil.powerForRange(audio, "Lows", true);
        var alpha = rawLow > state.lowsPower ? 0.2 : 0.05;
        state.lowsPower += (rawLow - state.lowsPower) * alpha;
        state.t1 = HSVUtil.mod1(state.t1 + dt * algo.presetSpeed * 20);
        state.t2 = HSVUtil.mod1(state.t2 + dt * algo.presetSpeed);
        var reactiveOffset = algo.presetReactivity * state.lowsPower * 20;
        for (var y = 0; y < height; y++) {
            for (var x = 0; x < width; x++) {
                var w = (width * height) <= 1 ? 0 : ((y * width + x) / (width * height - 1));
                var w1 = HSVUtil.sin01(state.t1 + w);
                var hWave = HSVUtil.sin01(w1 - w);
                hWave = HSVUtil.sin01(hWave);
                var w2 = HSVUtil.sin01(state.t2 - w * 10 + 2 + reactiveOffset);
                var v = w1 - w2;
                var intensity = HSVUtil.clamp01(v);
                var hue = HSVUtil.mod1(0.5 + 0.5 * hWave);
                var color = HSVUtil.gradientLedfxAt(algo.colors || [{h: 0.58, s: 1, v: 1}], hue);
                var i = (y * width + x) * 3;
                map[i] = color.h;
                map[i + 1] = color.s;
                map[i + 2] = intensity;
            }
        }
        HSVUtil.applyStripTransforms(map, width, height, outputOptions());
        return map;
    };

    testAlgo = algo;
    return algo;
})();
