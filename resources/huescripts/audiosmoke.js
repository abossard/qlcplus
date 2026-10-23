/*
  Q Light Controller Plus
  audiosmoke.js

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
    algo.name = "Audio Smoke";
    algo.author = "QLC+ contributors";
    algo.acceptColors = 5;
    algo.usesAudio = true;
    algo.properties = [];

    algo.presetSpeed = 1.0;
    algo.presetStretch = 1.5;
    algo.presetZoom = 2.0;
    algo.presetImpulseDecay = 0.06;
    algo.presetMultiplier = 2.0;
    algo.properties.push("name:speed|type:float|values:0,4|display:Speed|write:setSpeed|read:getSpeed");
    algo.properties.push("name:stretch|type:float|values:0.5,3|display:Stretch|write:setStretch|read:getStretch");
    algo.properties.push("name:zoom|type:float|values:0.1,15|display:Zoom|write:setZoom|read:getZoom");
    algo.properties.push("name:impulseDecay|type:float|values:0.01,0.3|display:Impulse Decay|write:setImpulseDecay|read:getImpulseDecay");
    algo.properties.push("name:multiplier|type:float|values:0,4|display:Multiplier|write:setMultiplier|read:getMultiplier");
    algo.setSpeed = function(v) { algo.presetSpeed = Math.max(0, Math.min(4, parseFloat(v) || 0)); };
    algo.getSpeed = function() { return algo.presetSpeed; };
    algo.setStretch = function(v) { algo.presetStretch = Math.max(0.5, Math.min(3, parseFloat(v) || 0.5)); };
    algo.getStretch = function() { return algo.presetStretch; };
    algo.setZoom = function(v) { algo.presetZoom = Math.max(0.1, Math.min(15, parseFloat(v) || 0.1)); };
    algo.getZoom = function() { return algo.presetZoom; };
    algo.setImpulseDecay = function(v) { algo.presetImpulseDecay = Math.max(0.01, Math.min(0.3, parseFloat(v) || 0.01)); };
    algo.getImpulseDecay = function() { return algo.presetImpulseDecay; };
    algo.setMultiplier = function(v) { algo.presetMultiplier = Math.max(0, Math.min(4, parseFloat(v) || 0)); };
    algo.getMultiplier = function() { return algo.presetMultiplier; };

    function fbm(x, y, z) {
        var total = 0;
        var amp = 0.5;
        var freq = 1;
        for (var octave = 0; octave < 4; octave++) {
            total += amp * HSVUtil.simplex3d(x * freq, y * freq, z * freq);
            freq *= 2;
            amp *= 0.5;
        }
        return total;
    }

    algo.rgbMapStepCount = function() { return 1; };
    algo.rgbMapSetColors = function() {};
    algo.rgbMapGetColors = function() { return []; };
    algo.rgbMap = HSVUtil.noiseFieldRenderer(algo, fbm);

    testAlgo = algo;
    return algo;
})();
