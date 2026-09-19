/*
  Q Light Controller Plus
  audiospotlight.js

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
    algo.name = "Audio Spotlight";
    algo.author = "QLC+ contributors";
    algo.acceptColors = 5;
    algo.usesAudio = true;
    algo.properties = [];

    algo.presetWidth = 0.18;
    algo.presetFadeSeconds = 0.8;
    algo.presetMaxSpots = 8;
    algo.presetSource = "Lows (beat+bass)";
    algo.presetGradient = "Yes";
    algo.presetGradientSpeed = 0.5;
    algo.presetColorSpan = 1.0;
    algo.presetCenterEdge = 0.0;
    algo.presetBirthGain = 10.0;
    algo.presetBrightness = 1.0;
    algo.properties.push("name:width|type:float|values:0.02,0.6|display:Width|write:setWidth|read:getWidth");
    algo.properties.push("name:fadeSeconds|type:float|values:0.1,5|display:Fade Seconds|write:setFadeSeconds|read:getFadeSeconds");
    algo.properties.push("name:maxSpots|type:range|display:Max Spots|values:1,24|write:setMaxSpots|read:getMaxSpots");
    algo.properties.push("name:source|type:list|display:Source|values:Beat,Bass,Lows (beat+bass),Mids,High|write:setSource|read:getSource");
    algo.properties.push("name:gradient|type:list|display:Gradient|values:Yes,No|write:setGradient|read:getGradient");
    algo.properties.push("name:gradientSpeed|type:float|values:0,2|display:Gradient Speed|write:setGradientSpeed|read:getGradientSpeed");
    algo.properties.push("name:colorSpan|type:float|values:0,2|display:Color Span|write:setColorSpan|read:getColorSpan");
    algo.properties.push("name:centerEdge|type:float|values:-1,1|display:Center/Edge|write:setCenterEdge|read:getCenterEdge");
    algo.properties.push("name:birthGain|type:float|values:0,30|display:Birth Gain|write:setBirthGain|read:getBirthGain");
    algo.properties.push("name:brightness|type:float|values:0,1|display:Brightness|write:setBrightness|read:getBrightness");
    algo.setWidth = function(v) { algo.presetWidth = Math.max(0.02, Math.min(0.6, parseFloat(v) || 0.02)); };
    algo.getWidth = function() { return algo.presetWidth; };
    algo.setFadeSeconds = function(v) { algo.presetFadeSeconds = Math.max(0.1, Math.min(5, parseFloat(v) || 0.1)); };
    algo.getFadeSeconds = function() { return algo.presetFadeSeconds; };
    algo.setMaxSpots = function(v) { algo.presetMaxSpots = Math.max(1, Math.min(24, Math.round(parseFloat(v) || 1))); };
    algo.getMaxSpots = function() { return algo.presetMaxSpots; };
    algo.setSource = function(v) {
        var values = ["Beat", "Bass", "Lows (beat+bass)", "Mids", "High"];
        algo.presetSource = values.indexOf(v) >= 0 ? v : "Lows (beat+bass)";
    };
    algo.getSource = function() { return algo.presetSource; };
    algo.setGradient = function(v) { algo.presetGradient = v === "No" ? "No" : "Yes"; };
    algo.getGradient = function() { return algo.presetGradient; };
    algo.setGradientSpeed = function(v) { algo.presetGradientSpeed = Math.max(0, Math.min(2, parseFloat(v) || 0)); };
    algo.getGradientSpeed = function() { return algo.presetGradientSpeed; };
    algo.setColorSpan = function(v) { algo.presetColorSpan = Math.max(0, Math.min(2, parseFloat(v) || 0)); };
    algo.getColorSpan = function() { return algo.presetColorSpan; };
    algo.setCenterEdge = function(v) { algo.presetCenterEdge = Math.max(-1, Math.min(1, parseFloat(v) || 0)); };
    algo.getCenterEdge = function() { return algo.presetCenterEdge; };
    algo.setBirthGain = function(v) { algo.presetBirthGain = Math.max(0, Math.min(30, parseFloat(v) || 0)); };
    algo.getBirthGain = function() { return algo.presetBirthGain; };
    algo.setBrightness = function(v) { algo.presetBrightness = HSVUtil.clamp01(parseFloat(v) || 0); };
    algo.getBrightness = function() { return algo.presetBrightness; };
    var outputOptions = HSVUtil.attachStripTransformControls(algo);

    var state = { audioKey: "", spots: [], carry: 0, prevActivity: 0, gradientPhase: 0, activity: 0 };

    function sourcePower(audio) {
        return HSVUtil.powerForRange(audio, algo.presetSource, false);
    }

    function weightedActivity(audio) {
        var low = HSVUtil.clamp01(audio && audio.low || 0);
        var mid = HSVUtil.clamp01(audio && audio.mid || 0);
        var high = HSVUtil.clamp01(audio && audio.high || 0);
        return HSVUtil.clamp01(low * 0.5 + mid * 0.3 + high * 0.2);
    }

    function spawnX() {
        var r = Math.random();
        if (algo.presetCenterEdge > 0) {
            var edgeBias = Math.pow(r, 1 + algo.presetCenterEdge * 3);
            return Math.random() < 0.5 ? edgeBias * 0.5 : 1 - edgeBias * 0.5;
        }
        if (algo.presetCenterEdge < 0) {
            var centerBias = 1 - Math.pow(r, 1 + (-algo.presetCenterEdge) * 3);
            return 0.5 + (Math.random() < 0.5 ? -0.5 : 0.5) * centerBias;
        }
        return Math.random();
    }

    function colorAt(t) {
        var stops = algo.colors && algo.colors.length ? algo.colors : [{ h: 0.6, s: 1, v: 1 }];
        if (algo.presetGradient === "No") return stops[0];
        return HSVUtil.gradientLedfxAt(stops, HSVUtil.mod1(t));
    }

    function toRgb(color) {
        return HSVUtil.hsvToRgb(color.h, color.s, color.v);
    }

    function eventCount(deltaEvents, key, legacyFired) {
        if (deltaEvents && isFinite(deltaEvents[key])) {
            var count = Math.floor(deltaEvents[key]);
            return count > 0 ? count : 0;
        }
        return legacyFired ? 1 : 0;
    }

    algo.rgbMapStepCount = function() { return 1; };
    algo.rgbMapSetColors = function() {};
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step, audio) {
        var map = HSVUtil.createMap(width, height);
        var audioKey = HSVUtil.audioIdentityKey(audio, state.audioKey);
        if (state.audioKey !== audioKey) {
            state.audioKey = audioKey;
            state.spots = [];
            state.carry = 0;
            state.prevActivity = 0;
            state.gradientPhase = 0;
            state.activity = 0;
        }
        var dt = HSVUtil.audioSeconds(audio);
        var deltaEvents = audio && audio.events && audio.events.delta ? audio.events.delta : null;
        for (var si = state.spots.length - 1; si >= 0; si--) {
            state.spots[si].life -= dt;
            if (state.spots[si].life <= 0) state.spots.splice(si, 1);
        }
        var activity = weightedActivity(audio);
        var delta = Math.max(0, activity - state.prevActivity);
        state.prevActivity = activity;
        state.gradientPhase = HSVUtil.mod1(state.gradientPhase + dt * algo.presetGradientSpeed);
        var activityInput = HSVUtil.clamp01(activity + delta * 1.2);
        var alpha = activityInput > state.activity ? 0.6 : 0.25;
        state.activity += (activityInput - state.activity) * alpha;
        state.activity = HSVUtil.clamp01(state.activity);
        state.carry += dt * (2.5 + 14.0 * state.activity);
        state.carry += delta * algo.presetBirthGain;
        state.carry += sourcePower(audio) * dt * 1.5;
        var onsets = eventCount(deltaEvents, "onset", !!(audio && audio.onset));
        var kicks = eventCount(deltaEvents, "kick", !!(audio && audio.kickFired));
        if (onsets > 0 || kicks > 0) {
            state.carry += (onsets + kicks) * (1.8 + delta * 1.2);
        }
        var minCap = Math.min(3, algo.presetMaxSpots);
        var dynamicCap = minCap + Math.floor((algo.presetMaxSpots - minCap) * state.activity);
        while (state.carry >= 1 && state.spots.length < dynamicCap) {
            state.carry -= 1;
            var x = spawnX();
            var anchor = HSVUtil.mod1(state.gradientPhase + (Math.random() * algo.presetColorSpan));
            state.spots.push({
                x: x,
                life: algo.presetFadeSeconds,
                maxLife: algo.presetFadeSeconds,
                anchor: anchor,
                color: colorAt(anchor)
            });
        }
        if (state.spots.length >= dynamicCap && state.carry > 0.999)
            state.carry = 0.999;

        // sigma and the per-spot gradient endpoint colors depend only on
        // frame-constant presets and each spot's anchor, never on the pixel
        // position, so compute them once per frame instead of per pixel.
        var sigma = Math.max(0.02, algo.presetWidth);
        var sigmaSq = sigma * sigma;
        var gradientOn = algo.presetGradient === "Yes";
        var spotCenterRgb = null, spotEdgeRgb = null;
        if (gradientOn) {
            spotCenterRgb = new Array(state.spots.length);
            spotEdgeRgb = new Array(state.spots.length);
            for (var sp = 0; sp < state.spots.length; sp++) {
                var spotSp = state.spots[sp];
                spotCenterRgb[sp] = toRgb(colorAt(spotSp.anchor));
                spotEdgeRgb[sp] = toRgb(colorAt(spotSp.anchor + algo.presetColorSpan));
            }
        }

        for (var y = 0; y < height; y++) {
            for (var x2 = 0; x2 < width; x2++) {
                var pos = width <= 1 ? 0 : x2 / (width - 1);
                var r = 0, g = 0, b = 0;
                for (var s = 0; s < state.spots.length; s++) {
                    var spot = state.spots[s];
                    var d = Math.abs(pos - spot.x);
                    d = Math.min(d, 1 - d);
                    var spatial = Math.exp(-(d * d) / sigmaSq);
                    var life = Math.pow(HSVUtil.clamp01(spot.life / Math.max(1e-6, spot.maxLife)), 1.4);
                    var w = spatial * life;
                    var srgb = null;
                    if (gradientOn) {
                        var centerRgb = spotCenterRgb[s];
                        var edgeRgb = spotEdgeRgb[s];
                        var mix = HSVUtil.clamp01(d / sigma);
                        srgb = [
                            centerRgb[0] * (1 - mix) + edgeRgb[0] * mix,
                            centerRgb[1] * (1 - mix) + edgeRgb[1] * mix,
                            centerRgb[2] * (1 - mix) + edgeRgb[2] * mix
                        ];
                    } else {
                        srgb = toRgb(spot.color);
                    }
                    r += srgb[0] * w;
                    g += srgb[1] * w;
                    b += srgb[2] * w;
                }
                var hsv = HSVUtil.rgbToHsvUnclipped(r, g, b);
                var i = (y * width + x2) * 3;
                map[i] = hsv.h;
                map[i + 1] = hsv.s;
                map[i + 2] = hsv.v;
            }
        }
        HSVUtil.applyStripTransforms(map, width, height, outputOptions());
        return map;
    };

    testAlgo = algo;
    return algo;
})();
