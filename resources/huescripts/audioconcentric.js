/*
  Q Light Controller Plus
  audioconcentric.js

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
    algo.name = "Audio Concentric";
    algo.author = "QLC+ contributors";
    algo.acceptColors = 5;
    algo.usesAudio = true;
    algo.properties = [];

    algo.presetFrequencyRange = "Lows (beat+bass)";
    algo.presetInvert = "No";
    algo.presetPowerMultiplier = 0.5;
    algo.presetGradientScale = 1;
    algo.presetStretchHeight = 1;
    algo.presetCenterSmoothing = 0.5;
    algo.presetIdleSpeed = 1;
    algo.properties.push("name:frequencyRange|type:list|display:Frequency Range|values:Beat,Bass,Lows (beat+bass),Mids,High|write:setFrequencyRange|read:getFrequencyRange");
    algo.properties.push("name:invert|type:list|display:Invert|values:Yes,No|write:setInvert|read:getInvert");
    algo.properties.push("name:powerMultiplier|type:float|values:0,1|display:Power Multiplier (0..1)|write:setPowerMultiplier|read:getPowerMultiplier");
    algo.properties.push("name:gradientScale|type:float|values:0.1,10|display:Gradient Scale (0.1..10)|write:setGradientScale|read:getGradientScale");
    algo.properties.push("name:stretchHeight|type:float|values:0.1,5|display:Stretch Height (0.1..5)|write:setStretchHeight|read:getStretchHeight");
    algo.properties.push("name:centerSmoothing|type:float|values:0,5|display:Center Smoothing (0..5)|write:setCenterSmoothing|read:getCenterSmoothing");
    algo.properties.push("name:idleSpeed|type:float|values:0,1|display:Idle Speed (0..1)|write:setIdleSpeed|read:getIdleSpeed");
    algo.setFrequencyRange = function(v) {
        algo.presetFrequencyRange = ["Beat", "Bass", "Lows (beat+bass)", "Mids", "High"].indexOf(v) >= 0
            ? v : "Lows (beat+bass)";
    };
    algo.getFrequencyRange = function() { return algo.presetFrequencyRange; };
    algo.setInvert = function(v) { algo.presetInvert = v === "Yes" ? "Yes" : "No"; };
    algo.getInvert = function() { return algo.presetInvert; };
    algo.setPowerMultiplier = function(v) { algo.presetPowerMultiplier = HSVUtil.clamp01(parseFloat(v) || 0); };
    algo.getPowerMultiplier = function() { return algo.presetPowerMultiplier; };
    algo.setGradientScale = function(v) { algo.presetGradientScale = Math.max(0.1, Math.min(10, parseFloat(v) || 1)); };
    algo.getGradientScale = function() { return algo.presetGradientScale; };
    algo.setStretchHeight = function(v) { algo.presetStretchHeight = Math.max(0.1, Math.min(5, parseFloat(v) || 1)); };
    algo.getStretchHeight = function() { return algo.presetStretchHeight; };
    algo.setCenterSmoothing = function(v) { algo.presetCenterSmoothing = Math.max(0, Math.min(5, parseFloat(v) || 0)); };
    algo.getCenterSmoothing = function() { return algo.presetCenterSmoothing; };
    algo.setIdleSpeed = function(v) { algo.presetIdleSpeed = Math.max(0, Math.min(1, parseFloat(v) || 0)); };
    algo.getIdleSpeed = function() { return algo.presetIdleSpeed; };

    var state = { audioKey: "", geometryKey: "", offset: 0, distCache: null, cacheKey: "" };

    function buildDist(width, height) {
        var key = [width, height, algo.presetGradientScale, algo.presetStretchHeight, algo.presetCenterSmoothing].join("|");
        if (state.cacheKey === key && state.distCache) return state.distCache;
        state.cacheKey = key;
        var dist = new Array(width * height);
        var cx = (width - 1) * 0.5;
        var cy = (height - 1) * 0.5;
        var maxRadius = Math.hypot(cx, cy / algo.presetStretchHeight);
        for (var y = 0; y < height; y++) {
            for (var x = 0; x < width; x++) {
                var dx = (x - cx) / algo.presetGradientScale;
                var dy = (y - cy) / (algo.presetGradientScale * algo.presetStretchHeight);
                var d = Math.sqrt(dx * dx + dy * dy);
                if (maxRadius > 0) d /= maxRadius;
                dist[y * width + x] = HSVUtil.clamp01(Math.pow(d, 0.9));
            }
        }
        if (algo.presetCenterSmoothing > 0) {
            var blur = Math.max(1, Math.round(algo.presetCenterSmoothing));
            for (var pass = 0; pass < blur; pass++) {
                var smoothed = dist.slice();
                for (var yb = 0; yb < height; yb++) {
                    for (var xb = 0; xb < width; xb++) {
                        var sum = 0, c = 0;
                        for (var dyb = -1; dyb <= 1; dyb++) {
                            for (var dxb = -1; dxb <= 1; dxb++) {
                                var xx = xb + dxb, yy = yb + dyb;
                                if (xx < 0 || yy < 0 || xx >= width || yy >= height) continue;
                                sum += dist[yy * width + xx];
                                c++;
                            }
                        }
                        smoothed[yb * width + xb] = sum / c;
                    }
                }
                dist = smoothed;
            }
        }
        state.distCache = dist;
        return dist;
    }

    function ensureState(width, height, audio) {
        var nextAudioKey = HSVUtil.audioIdentityKey(audio, state.audioKey);
        var nextGeometryKey = width + "|" + height;
        if (state.audioKey !== nextAudioKey || state.geometryKey !== nextGeometryKey) {
            state.audioKey = nextAudioKey;
            state.geometryKey = nextGeometryKey;
            state.offset = 0;
        }
    }

    algo.rgbMapStepCount = function() { return 1; };
    algo.rgbMapSetColors = function() {};
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step, audio) {
        var map = HSVUtil.createMap(width, height);
        ensureState(width, height, audio);
        var dt = HSVUtil.audioSeconds(audio);
        var power = HSVUtil.powerForRange(audio, algo.presetFrequencyRange, false) * algo.presetPowerMultiplier * 12;
        state.offset = HSVUtil.mod1(state.offset + (power + algo.presetIdleSpeed) * dt);
        var dist = buildDist(width, height);
        for (var y = 0; y < height; y++) {
            for (var x = 0; x < width; x++) {
                var d = dist[y * width + x];
                var t = algo.presetInvert === "Yes" ? HSVUtil.mod1(d + state.offset) : HSVUtil.mod1(d - state.offset);
                var color = HSVUtil.gradientLedfxAt(algo.colors || [{h: 0.65, s: 1, v: 1}], t);
                var i = (y * width + x) * 3;
                map[i] = color.h;
                map[i + 1] = color.s;
                map[i + 2] = HSVUtil.clamp01(color.v);
            }
        }
        return map;
    };

    testAlgo = algo;
    return algo;
})();
