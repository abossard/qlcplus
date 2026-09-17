/*
  Q Light Controller Plus
  audiovumeter.js

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
    algo.name = "Audio VuMeter";
    algo.author = "QLC+ contributors";
    algo.acceptColors = 0;
    algo.usesAudio = true;
    algo.properties = [];

    algo.presetPeakDecay = 0.1;
    algo.presetMinVolume = 0.2;
    algo.presetColorMin = "#0000ff";
    algo.presetColorMax = "#ff0000";
    algo.presetColorMid = "#00ff00";
    algo.presetColorPeak = "#ffffff";
    algo.presetPeakPercent = 1;
    algo.presetMaxVolume = 0.8;
    algo.properties.push("name:peakDecay|type:float|values:0.01,0.3|display:Peak Decay (0.01..0.3)|write:setPeakDecay|read:getPeakDecay");
    algo.properties.push("name:minVolume|type:float|values:0,1|display:Min Volume (0..1)|write:setMinVolume|read:getMinVolume");
    algo.properties.push("name:colorMin|type:string|display:Color Min|write:setColorMin|read:getColorMin");
    algo.properties.push("name:colorMax|type:string|display:Color Max|write:setColorMax|read:getColorMax");
    algo.properties.push("name:colorMid|type:string|display:Color Mid|write:setColorMid|read:getColorMid");
    algo.properties.push("name:colorPeak|type:string|display:Color Peak|write:setColorPeak|read:getColorPeak");
    algo.properties.push("name:peakPercent|type:range|display:Peak Percent|values:0,5|write:setPeakPercent|read:getPeakPercent");
    algo.properties.push("name:maxVolume|type:float|values:0,1|display:Max Volume (0..1)|write:setMaxVolume|read:getMaxVolume");
    algo.setPeakDecay = function(v) { algo.presetPeakDecay = Math.max(0.01, Math.min(0.3, parseFloat(v) || 0.01)); };
    algo.getPeakDecay = function() { return algo.presetPeakDecay; };
    algo.setMinVolume = function(v) { algo.presetMinVolume = HSVUtil.clamp01(parseFloat(v) || 0); };
    algo.getMinVolume = function() { return algo.presetMinVolume; };
    algo.setColorMin = function(v) { algo.presetColorMin = /^#[0-9a-f]{6}$/i.test(String(v)) ? String(v) : "#0000ff"; };
    algo.getColorMin = function() { return algo.presetColorMin; };
    algo.setColorMax = function(v) { algo.presetColorMax = /^#[0-9a-f]{6}$/i.test(String(v)) ? String(v) : "#ff0000"; };
    algo.getColorMax = function() { return algo.presetColorMax; };
    algo.setColorMid = function(v) { algo.presetColorMid = /^#[0-9a-f]{6}$/i.test(String(v)) ? String(v) : "#00ff00"; };
    algo.getColorMid = function() { return algo.presetColorMid; };
    algo.setColorPeak = function(v) { algo.presetColorPeak = /^#[0-9a-f]{6}$/i.test(String(v)) ? String(v) : "#ffffff"; };
    algo.getColorPeak = function() { return algo.presetColorPeak; };
    algo.setPeakPercent = function(v) { algo.presetPeakPercent = Math.max(0, Math.min(5, parseInt(v) || 0)); };
    algo.getPeakPercent = function() { return algo.presetPeakPercent; };
    algo.setMaxVolume = function(v) { algo.presetMaxVolume = HSVUtil.clamp01(parseFloat(v) || 0); };
    algo.getMaxVolume = function() { return algo.presetMaxVolume; };
    var readOutputControls = HSVUtil.attachStripTransformControls(algo);

    var state = { audioKey: "", geometryKey: "", peak: 0, trough: 1, initialized: false };

    function rmsToMeter(rawRms) {
        if (!isFinite(rawRms) || rawRms <= 0) return 0;
        return HSVUtil.clamp01(1 + (20 * Math.log10(rawRms)) / 100);
    }

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
            state.peak = 0;
            state.trough = 1;
            state.initialized = false;
        }
    }

    algo.rgbMapStepCount = function() { return 1; };
    algo.rgbMapSetColors = function() {};
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step, audio) {
        var map = HSVUtil.createMap(width, height);
        ensureState(width, height, audio);
        var rawRms = audio && audio.volume && isFinite(audio.volume.rawRms) ? audio.volume.rawRms : 0;
        var volume = rmsToMeter(rawRms);
        var alphaDecay = algo.presetPeakDecay;
        if (!state.initialized) {
            state.peak = volume;
            state.trough = volume;
            state.initialized = true;
        } else {
            state.peak += (volume - state.peak) * (volume > state.peak ? 0.99 : alphaDecay);
            if (volume < state.trough) state.trough = volume;
            else state.trough += (volume - state.trough) * alphaDecay;
        }
        var volumeMin = algo.presetMinVolume;
        var volumeMax = algo.presetMaxVolume;
        var volumePixels = Math.floor(width * volume);
        var minPixels = Math.max(0, Math.min(width, Math.floor(width * volumeMin)));
        var maxPixels = Math.max(0, Math.min(width, Math.floor(width * volumeMax)));
        var peakSize = Math.floor(algo.presetPeakPercent * (width / 100));
        if (peakSize < 0) peakSize = 0;
        var peakStart = Math.min(Math.floor(width * state.peak), width);
        var peakEnd = Math.min(peakStart + peakSize, width);
        var troughEnd = Math.max(0, Math.min(Math.floor(width * state.trough), width));
        var troughStart = Math.max(0, troughEnd - peakSize);
        var colorMin = hexToHsv(algo.presetColorMin);
        var colorMid = hexToHsv(algo.presetColorMid);
        var colorMax = hexToHsv(algo.presetColorMax);
        var colorPeak = hexToHsv(algo.presetColorPeak);
        for (var y = 0; y < height; y++) {
            for (var x = 0; x < width; x++) {
                var c = {h: 0, s: 0, v: 0};
                if (x >= peakStart && x < peakEnd) c = colorPeak;
                else if (x >= troughStart && x < troughEnd) c = colorPeak;
                else if (x < volumePixels && x < minPixels) c = colorMin;
                else if (x < volumePixels && x < maxPixels) c = colorMid;
                else if (x < volumePixels) c = colorMax;
                var i = (y * width + x) * 3;
                map[i] = c.h;
                map[i + 1] = c.s;
                map[i + 2] = c.v;
            }
        }
        HSVUtil.applyStripTransforms(map, width, height, readOutputControls());
        return map;
    };

    testAlgo = algo;
    return algo;
})();
