/*
  Q Light Controller Plus
  audiobladepower.js

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
    algo.name = "Audio Blade Power";
    algo.author = "QLC+ contributors";
    algo.acceptColors = 5;
    algo.usesAudio = true;
    algo.properties = [];

    algo.presetMirror = "No";
    algo.presetBlur = 2.0;
    algo.presetDecay = 0.7;
    algo.presetMultiplier = 0.5;
    algo.presetBrightness = 1.0;
    algo.presetFrequencyRange = "Lows (beat+bass)";
    algo.presetFixHues = "No";
    algo.properties.push("name:mirror|type:list|display:Mirror|values:Yes,No|write:setMirror|read:getMirror");
    algo.properties.push("name:blur|type:float|values:0,10|display:Blur|write:setBlur|read:getBlur");
    algo.properties.push("name:decay|type:float|values:0,1|display:Decay|write:setDecay|read:getDecay");
    algo.properties.push("name:multiplier|type:float|values:0,1|display:Multiplier|write:setMultiplier|read:getMultiplier");
    algo.properties.push("name:brightness|type:float|values:0,1|display:Brightness|write:setBrightness|read:getBrightness");
    algo.properties.push("name:frequencyRange|type:list|display:Frequency Range|values:Beat,Bass,Lows (beat+bass),Mids,High|write:setFrequencyRange|read:getFrequencyRange");
    algo.properties.push("name:fixHues|type:list|display:Fix Hues|values:Yes,No|write:setFixHues|read:getFixHues");
    algo.setMirror = function(v) { algo.presetMirror = v === "Yes" ? "Yes" : "No"; };
    algo.getMirror = function() { return algo.presetMirror; };
    algo.setBlur = function(v) { algo.presetBlur = Math.max(0, Math.min(10, parseFloat(v) || 0)); };
    algo.getBlur = function() { return algo.presetBlur; };
    algo.setDecay = function(v) { algo.presetDecay = HSVUtil.clamp01(parseFloat(v) || 0); };
    algo.getDecay = function() { return algo.presetDecay; };
    algo.setMultiplier = function(v) { algo.presetMultiplier = HSVUtil.clamp01(parseFloat(v) || 0); };
    algo.getMultiplier = function() { return algo.presetMultiplier; };
    algo.setBrightness = function(v) { algo.presetBrightness = HSVUtil.clamp01(parseFloat(v) || 0); };
    algo.getBrightness = function() { return algo.presetBrightness; };
    algo.setFrequencyRange = function(v) {
        algo.presetFrequencyRange = ["Beat", "Bass", "Lows (beat+bass)", "Mids", "High"].indexOf(v) >= 0
            ? v : "Lows (beat+bass)";
    };
    algo.getFrequencyRange = function() { return algo.presetFrequencyRange; };
    algo.setFixHues = function(v) { algo.presetFixHues = v === "Yes" ? "Yes" : "No"; };
    algo.getFixHues = function() { return algo.presetFixHues; };
    var readOutputControls = HSVUtil.attachStripTransformControls(algo, { display: "" });

    var state = { identityKey: "", geometryKey: "", values: [] };

    function ensure(width, height, audio) {
        var identityKey = HSVUtil.audioIdentityKey(audio, state.identityKey);
        var geometryKey = width + "|" + height;
        var n = width * height;
        if (state.identityKey !== identityKey || state.geometryKey !== geometryKey || state.values.length !== n) {
            state.identityKey = identityKey;
            state.geometryKey = geometryKey;
            state.values = new Array(n).fill(0);
        }
    }

    function sourcePower(audio) {
        if (!audio) return 0;
        var beat = HSVUtil.clamp01(audio.beat || 0);
        var bass = HSVUtil.clamp01(audio.bass || 0);
        if (algo.presetFrequencyRange === "Lows (beat+bass)")
            return HSVUtil.clamp01((beat + bass) * 0.5);
        return HSVUtil.powerForRange(audio, algo.presetFrequencyRange, false);
    }

    algo.rgbMapStepCount = function() { return 1; };
    algo.rgbMapSetColors = function() {};
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step, audio) {
        var map = HSVUtil.createMap(width, height);
        ensure(width, height, audio);
        var n = width * height;
        var bar = sourcePower(audio) * algo.presetMultiplier * 2;
        var level = HSVUtil.clamp01(bar);
        var barIdx = Math.floor(level * n);
        var seconds = HSVUtil.audioSeconds(audio);
        var keep = algo.presetDecay / 2 + 0.45;
        if (seconds > 0)
            keep = Math.pow(keep, seconds * 60);
        else
            keep = 1;
        for (var i = 0; i < n; i++) {
            state.values[i] = HSVUtil.clamp01(state.values[i] * keep);
            if (i < barIdx) state.values[i] = algo.presetBrightness;
            var hueCoord = algo.presetFixHues === "Yes"
                ? (Math.floor(i * 12 / Math.max(1, n - 1)) / 12)
                : (n <= 1 ? 0 : i / (n - 1));
            var color = HSVUtil.gradientLedfxAt(algo.colors || [{h: 0.0, s: 1, v: 1}, {h: 0.66, s: 1, v: 1}], hueCoord);
            var o = i * 3;
            map[o] = color.h;
            map[o + 1] = color.s;
            map[o + 2] = state.values[i];
        }
        return HSVUtil.applyStripTransforms(map, width, height, readOutputControls());
    };

    testAlgo = algo;
    return algo;
})();
