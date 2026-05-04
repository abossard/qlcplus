/*
  Q Light Controller Plus
  audio_common.js

  Shared helpers for audio-reactive RGB scripts.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

var AudioParams = {
    installContinuous: function(algo, defaults) {
        var d = defaults || {};
        algo.presetGain = d.gain || 5;
        algo.presetReactivity = d.reactivity || 5;
        algo.presetFloor = d.floor || 0;

        algo.properties.push("name:presetGain|type:range|display:Gain|values:1,10|write:setGain|read:getGain");
        algo.properties.push("name:presetReactivity|type:range|display:Reactivity|values:1,10|write:setReactivity|read:getReactivity");
        algo.properties.push("name:presetFloor|type:range|display:Floor|values:0,100|write:setFloor|read:getFloor");

        algo.setGain = function(v) { algo.presetGain = Math.max(1, Math.min(10, parseInt(v))); };
        algo.getGain = function() { return algo.presetGain; };
        algo.setReactivity = function(v) { algo.presetReactivity = Math.max(1, Math.min(10, parseInt(v))); };
        algo.getReactivity = function() { return algo.presetReactivity; };
        algo.setFloor = function(v) { algo.presetFloor = Math.max(0, Math.min(100, parseInt(v))); };
        algo.getFloor = function() { return algo.presetFloor; };
    },

    installTrigger: function(algo, defaults) {
        var d = defaults || {};
        algo.presetGain = d.gain || 5;
        algo.presetReactivity = d.reactivity || 5;
        algo.presetSensitivity = d.sensitivity || 5;

        algo.properties.push("name:presetGain|type:range|display:Gain|values:1,10|write:setGain|read:getGain");
        algo.properties.push("name:presetReactivity|type:range|display:Reactivity|values:1,10|write:setReactivity|read:getReactivity");
        algo.properties.push("name:presetSensitivity|type:range|display:Sensitivity|values:1,10|write:setSensitivity|read:getSensitivity");

        algo.setGain = function(v) { algo.presetGain = Math.max(1, Math.min(10, parseInt(v))); };
        algo.getGain = function() { return algo.presetGain; };
        algo.setReactivity = function(v) { algo.presetReactivity = Math.max(1, Math.min(10, parseInt(v))); };
        algo.getReactivity = function() { return algo.presetReactivity; };
        algo.setSensitivity = function(v) { algo.presetSensitivity = Math.max(1, Math.min(10, parseInt(v))); };
        algo.getSensitivity = function() { return algo.presetSensitivity; };
    },

    gainFactor: function(algo) { return 0.6 + algo.presetGain * 0.2; },
    filterRise: function(algo) { return 0.1 + algo.presetReactivity * 0.09; },
    applyFloor: function(algo, brightness) {
        var f = algo.presetFloor / 100.0;
        return f + (1 - f) * brightness;
    },
    triggerThreshold: function(algo) { return 0.45 - algo.presetSensitivity * 0.04; },

    createFilter: function(algo, baseDecay) {
        return new LedFx.ExpFilter(baseDecay, AudioParams.filterRise(algo));
    }
};
