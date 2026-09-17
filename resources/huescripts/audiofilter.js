/*
  Q Light Controller Plus
  audiofilter.js

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
    algo.name = "Audio Filter";
    algo.author = "QLC+ contributors";
    algo.acceptColors = 5;
    algo.usesAudio = true;
    algo.properties = [];

    algo.presetColor = "#ff0000";
    algo.presetFrequencyRange = "Lows (beat+bass)";
    algo.presetUseGradient = "No";
    algo.presetRollSpeed = 0.0;
    algo.presetBoost = 0.0;
    algo.properties.push("name:color|type:string|display:Color (hex)|write:setColor|read:getColor");
    algo.properties.push("name:frequencyRange|type:list|display:Frequency Range|values:Beat,Bass,Lows (beat+bass),Mids,High|write:setFrequencyRange|read:getFrequencyRange");
    algo.properties.push("name:useGradient|type:list|display:Use Gradient|values:Yes,No|write:setUseGradient|read:getUseGradient");
    algo.properties.push("name:rollSpeed|type:float|values:0,1|display:Roll Speed (0..1)|write:setRollSpeed|read:getRollSpeed");
    algo.properties.push("name:boost|type:float|values:0,1|display:Boost (0..1)|write:setBoost|read:getBoost");
    algo.setColor = function(v) { algo.presetColor = /^#[0-9a-f]{6}$/i.test(String(v)) ? String(v) : "#ff0000"; };
    algo.getColor = function() { return algo.presetColor; };
    algo.setFrequencyRange = function(v) {
        algo.presetFrequencyRange = ["Beat", "Bass", "Lows (beat+bass)", "Mids", "High"].indexOf(v) >= 0
            ? v : "Lows (beat+bass)";
    };
    algo.getFrequencyRange = function() { return algo.presetFrequencyRange; };
    algo.setUseGradient = function(v) { algo.presetUseGradient = v === "Yes" ? "Yes" : "No"; };
    algo.getUseGradient = function() { return algo.presetUseGradient; };
    algo.setRollSpeed = function(v) { algo.presetRollSpeed = HSVUtil.clamp01(parseFloat(v) || 0); };
    algo.getRollSpeed = function() { return algo.presetRollSpeed; };
    algo.setBoost = function(v) { algo.presetBoost = HSVUtil.clamp01(parseFloat(v) || 0); };
    algo.getBoost = function() { return algo.presetBoost; };
    var readOutputControls = HSVUtil.attachStripTransformControls(algo);

    var state = { audioKey: "", geometryKey: "", roll: 0 };

    function ensureState(width, height, audio) {
        var nextAudioKey = HSVUtil.audioIdentityKey(audio, state.audioKey);
        var nextGeometryKey = width + "|" + height;
        if (state.audioKey !== nextAudioKey || state.geometryKey !== nextGeometryKey) {
            state.audioKey = nextAudioKey;
            state.geometryKey = nextGeometryKey;
            state.roll = 0;
        }
    }

    algo.rgbMapStepCount = function() { return 1; };
    algo.rgbMapSetColors = function() {};
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step, audio) {
        var map = HSVUtil.createMap(width, height);
        ensureState(width, height, audio);
        var power = HSVUtil.powerForRange(audio, algo.presetFrequencyRange, false);
        var boosted = HSVUtil.aggressiveTopEndBias(power, algo.presetBoost);
        var staticRgb = HSVUtil.parseHexRgb(algo.presetColor);
        var staticColor = HSVUtil.rgbToHsv(staticRgb[0], staticRgb[1], staticRgb[2]);
        var roll = 0;
        if (algo.presetRollSpeed > 0) {
            var rollTime = (1 - algo.presetRollSpeed) * 59 + 1;
            state.roll = HSVUtil.mod1(state.roll + HSVUtil.audioSeconds(audio) / rollTime);
            roll = state.roll;
        }
        var gradColor = HSVUtil.gradientLedfxAt(algo.colors || [staticColor], roll);
        var color = algo.presetUseGradient === "Yes" ? gradColor : staticColor;
        for (var y = 0; y < height; y++) {
            for (var x = 0; x < width; x++) {
                var i = (y * width + x) * 3;
                map[i] = color.h;
                map[i + 1] = color.s;
                map[i + 2] = color.v * boosted;
            }
        }
        HSVUtil.applyStripTransforms(map, width, height, readOutputControls());
        return map;
    };

    testAlgo = algo;
    return algo;
})();
