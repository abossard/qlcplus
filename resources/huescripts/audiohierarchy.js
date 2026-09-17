/*
  Q Light Controller Plus
  audiohierarchy.js

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
    algo.name = "Audio Hierarchy";
    algo.author = "QLC+ contributors";
    algo.acceptColors = 0;
    algo.usesAudio = true;
    algo.properties = [];

    algo.presetColorLows = "#ff0000";
    algo.presetColorMids = "#00ff00";
    algo.presetColorHigh = "#0000ff";
    algo.presetBrightnessBoost = 0.0;
    algo.presetThresholdLows = 0.05;
    algo.presetThresholdMids = 0.05;
    algo.presetSwitchTime = 0.1;
    algo.properties.push("name:colorLows|type:string|display:Color Lows|write:setColorLows|read:getColorLows");
    algo.properties.push("name:colorMids|type:string|display:Color Mids|write:setColorMids|read:getColorMids");
    algo.properties.push("name:colorHigh|type:string|display:Color High|write:setColorHigh|read:getColorHigh");
    algo.properties.push("name:brightnessBoost|type:float|values:0,1|display:Brightness Boost (0..1)|write:setBrightnessBoost|read:getBrightnessBoost");
    algo.properties.push("name:thresholdLows|type:float|values:0,1|display:Threshold Lows (0..1)|write:setThresholdLows|read:getThresholdLows");
    algo.properties.push("name:thresholdMids|type:float|values:0,1|display:Threshold Mids (0..1)|write:setThresholdMids|read:getThresholdMids");
    algo.properties.push("name:switchTime|type:float|values:0,1|display:Switch Time (0..1)|write:setSwitchTime|read:getSwitchTime");
    algo.setColorLows = function(v) { algo.presetColorLows = /^#[0-9a-f]{6}$/i.test(String(v)) ? String(v) : "#ff0000"; };
    algo.getColorLows = function() { return algo.presetColorLows; };
    algo.setColorMids = function(v) { algo.presetColorMids = /^#[0-9a-f]{6}$/i.test(String(v)) ? String(v) : "#00ff00"; };
    algo.getColorMids = function() { return algo.presetColorMids; };
    algo.setColorHigh = function(v) { algo.presetColorHigh = /^#[0-9a-f]{6}$/i.test(String(v)) ? String(v) : "#0000ff"; };
    algo.getColorHigh = function() { return algo.presetColorHigh; };
    algo.setBrightnessBoost = function(v) { algo.presetBrightnessBoost = HSVUtil.clamp01(parseFloat(v) || 0); };
    algo.getBrightnessBoost = function() { return algo.presetBrightnessBoost; };
    algo.setThresholdLows = function(v) { algo.presetThresholdLows = HSVUtil.clamp01(parseFloat(v) || 0); };
    algo.getThresholdLows = function() { return algo.presetThresholdLows; };
    algo.setThresholdMids = function(v) { algo.presetThresholdMids = HSVUtil.clamp01(parseFloat(v) || 0); };
    algo.getThresholdMids = function() { return algo.presetThresholdMids; };
    algo.setSwitchTime = function(v) { algo.presetSwitchTime = HSVUtil.clamp01(parseFloat(v) || 0); };
    algo.getSwitchTime = function() { return algo.presetSwitchTime; };

    var state = {
        audioKey: "",
        geometryKey: "",
        selected: "low",
        lowBelowSec: 0,
        midBelowSec: 0,
        filteredPower: 0
    };

    function hexToHsv(hex) {
        var rgb = HSVUtil.parseHexRgb(hex);
        return HSVUtil.rgbToHsv(rgb[0], rgb[1], rgb[2]);
    }

    function ensureState(width, height, audio) {
        var nextAudioKey = HSVUtil.audioIdentityKey(audio, state.audioKey);
        var nextGeometryKey = width + "|" + height;
        if (state.audioKey !== nextAudioKey || state.geometryKey !== nextGeometryKey) {
            state.audioKey = nextAudioKey;
            state.geometryKey = nextGeometryKey;
            state.selected = "low";
            state.lowBelowSec = 0;
            state.midBelowSec = 0;
            state.filteredPower = 0;
        }
    }

    algo.rgbMapStepCount = function() { return 1; };
    algo.rgbMapSetColors = function() {};
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step, audio) {
        var map = HSVUtil.createMap(width, height);
        ensureState(width, height, audio);
        var dt = HSVUtil.audioSeconds(audio);
        var low = HSVUtil.powerForRange(audio, "Lows (beat+bass)", false);
        var mid = HSVUtil.powerForRange(audio, "Mids", false);
        var high = HSVUtil.powerForRange(audio, "High", false);
        if (low > algo.presetThresholdLows) {
            state.selected = "low";
            state.filteredPower = low;
            state.lowBelowSec = 0;
            state.midBelowSec = 0;
        } else {
            state.lowBelowSec += dt;
            if (state.selected === "low") state.filteredPower = low;
            if (state.lowBelowSec > algo.presetSwitchTime) {
                if (mid > algo.presetThresholdMids) {
                    state.selected = "mid";
                    state.filteredPower = mid;
                    state.midBelowSec = 0;
                } else {
                    state.midBelowSec += dt;
                    if (state.selected === "mid") state.filteredPower = mid;
                    if (state.midBelowSec > algo.presetSwitchTime) {
                        state.selected = "high";
                        state.filteredPower = high;
                    } else if (state.selected === "high") {
                        state.filteredPower = high;
                    }
                }
            }
        }
        var color = state.selected === "low"
            ? hexToHsv(algo.presetColorLows)
            : (state.selected === "mid" ? hexToHsv(algo.presetColorMids) : hexToHsv(algo.presetColorHigh));
        var brightness = HSVUtil.aggressiveTopEndBias(state.filteredPower, algo.presetBrightnessBoost);

        for (var y = 0; y < height; y++) {
            for (var x = 0; x < width; x++) {
                var i = (y * width + x) * 3;
                map[i] = color.h;
                map[i + 1] = color.s;
                map[i + 2] = HSVUtil.clamp01(brightness * color.v);
            }
        }
        return map;
    };

    testAlgo = algo;
    return algo;
})();
