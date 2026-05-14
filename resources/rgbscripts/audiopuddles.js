/*
  Q Light Controller Plus
  audiopuddles.js

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
    algo.name = "Audio Puddles";
    algo.author = "QLC+ contributors";
    algo.acceptColors = 5;
    algo.usesAudio = true;
    algo.properties = new Array();

    var DEFAULT_GRADIENT = [
        {h: 0.708, s: 1.0, v: 1.0},
        {h: 0.500, s: 1.0, v: 1.0},
        {h: 0.417, s: 1.0, v: 1.0},
        {h: 0.167, s: 1.0, v: 1.0},
        {h: 0.958, s: 1.0, v: 1.0}
    ];
    var DEFAULT_BANDS = [
        {h: 0.042, s: 1.0, v: 1.0},
        {h: 0.399, s: 1.0, v: 1.0},
        {h: 0.611, s: 0.749, v: 1.0}
    ];

    algo.presetMaxRipples = 8;
    algo.properties.push(
      "name:presetMaxRipples|type:range|display:Max Concurrent Ripples|" +
      "values:1,32|write:setMaxRipples|read:getMaxRipples");
    algo.setMaxRipples = function(v) { algo.presetMaxRipples = parseInt(v); };
    algo.getMaxRipples = function() { return algo.presetMaxRipples; };

    algo.presetExpansionSpeed = 30;
    algo.properties.push(
      "name:presetExpansionSpeed|type:range|display:Expansion Speed (px/s)|" +
      "values:5,200|write:setExpansionSpeed|read:getExpansionSpeed");
    algo.setExpansionSpeed = function(v) { algo.presetExpansionSpeed = parseFloat(v); };
    algo.getExpansionSpeed = function() { return algo.presetExpansionSpeed; };

    algo.presetMaxRadius = 30;
    algo.properties.push(
      "name:presetMaxRadius|type:range|display:Max Radius (px)|" +
      "values:4,120|write:setMaxRadius|read:getMaxRadius");
    algo.setMaxRadius = function(v) { algo.presetMaxRadius = parseFloat(v); };
    algo.getMaxRadius = function() { return algo.presetMaxRadius; };

    algo.presetLifeMs = 1500;
    algo.properties.push(
      "name:presetLifeMs|type:range|display:Ripple Lifetime (ms)|" +
      "values:200,5000|write:setLifeMs|read:getLifeMs");
    algo.setLifeMs = function(v) { algo.presetLifeMs = parseFloat(v); };
    algo.getLifeMs = function() { return algo.presetLifeMs; };

    algo.presetRingWidth = 2;
    algo.properties.push(
      "name:presetRingWidth|type:range|display:Ring Width (px)|" +
      "values:1,8|write:setRingWidth|read:getRingWidth");
    algo.setRingWidth = function(v) { algo.presetRingWidth = parseFloat(v); };
    algo.getRingWidth = function() { return algo.presetRingWidth; };

    algo.presetTrigger = "Onset";
    algo.properties.push(
      "name:presetTrigger|type:list|display:Trigger|" +
      "values:Onset,Kick,Beat|write:setTrigger|read:getTrigger");
    algo.setTrigger = function(v) { algo.presetTrigger = String(v); };
    algo.getTrigger = function() { return algo.presetTrigger; };

    algo.presetMinSpawnMs = 80;
    algo.properties.push(
      "name:presetMinSpawnMs|type:range|display:Min Spawn Interval (ms)|" +
      "values:0,500|write:setMinSpawnMs|read:getMinSpawnMs");
    algo.setMinSpawnMs = function(v) { algo.presetMinSpawnMs = parseFloat(v); };
    algo.getMinSpawnMs = function() { return algo.presetMinSpawnMs; };

    algo.ripples = [];
    algo.spawnAccumMs = 1e9; // start ready

    algo.dominantColor = function(audio) {
        var bands = (algo.colors && algo.colors.length >= 3)
            ? algo.colors : DEFAULT_BANDS;
        var dom = (function(){var b=[audio.low,audio.mid,audio.high];return ["low","mid","high"][b.indexOf(Math.max.apply(null,b))]})();
        return bands[dom === "high" ? 2 : (dom === "mid" ? 1 : 0)];
    };

    algo.rgbMapStepCount = function(_w, _h) { return 1; };
    algo.rgbMapSetColors = function(_raw) { };
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step, audio) {
        var map = HSVUtil.createMap(width, height);
        var dt = audio.dt * 60.0 / audio.bpm;

        var trigger = false;
        var intensity = 1.0;
        if (algo.presetTrigger === "Kick") {
            trigger = audio.beatFired;
            intensity = audio.onsetIntensity;
        } else if (algo.presetTrigger === "Beat") {
            trigger = audio.beatFired;
            intensity = 1.0;
        } else {
            trigger = audio.onset;
            intensity = audio.onsetIntensity;
        }

        algo.spawnAccumMs += (audio.dt * 60000 / audio.bpm);
        var canSpawn = algo.spawnAccumMs >= algo.presetMinSpawnMs;

        if (trigger && canSpawn) {
            var gradient = (algo.colors && algo.colors.length > 0)
                ? algo.colors : DEFAULT_GRADIENT;
            // Hue cycles by ripple index
            var rippleIdx = algo.ripples.length;
            var color = HSVUtil.gradientAt(gradient, (rippleIdx % 12) / 11.0);
            var maxR = algo.presetMaxRadius * (0.5 + 0.5 * intensity);
            var ripple = {
                cx: Math.floor(Math.random() * width),
                cy: Math.floor(Math.random() * height),
                radius: 0,
                maxRadius: maxR,
                age: 0,
                lifeMs: algo.presetLifeMs,
                color: color
            };
            if (algo.ripples.length >= algo.presetMaxRipples) {
                // Replace oldest/weakest: pick the ripple with the largest age/lifeMs ratio.
                var worstIdx = 0;
                var worstScore = -1;
                for (var k = 0; k < algo.ripples.length; k++) {
                    var rk = algo.ripples[k];
                    var score = rk.age / rk.lifeMs;
                    if (score > worstScore) { worstScore = score; worstIdx = k; }
                }
                algo.ripples[worstIdx] = ripple;
            } else {
                algo.ripples.push(ripple);
            }
            algo.spawnAccumMs = 0;
        }

        for (var i = algo.ripples.length - 1; i >= 0; i--) {
            var r = algo.ripples[i];
            r.radius += algo.presetExpansionSpeed * dt;
            r.age += (audio.dt * 60000 / audio.bpm);
            if (r.age >= r.lifeMs || r.radius > r.maxRadius) {
                algo.ripples.splice(i, 1);
            }
        }

        var ringW = algo.presetRingWidth;
        for (var ri = 0; ri < algo.ripples.length; ri++) {
            var rp = algo.ripples[ri];
            var alpha = 1.0 - (rp.age / rp.lifeMs);
            if (alpha <= 0) continue;
            var x0 = Math.floor(rp.cx - rp.radius - ringW);
            var x1 = Math.ceil(rp.cx + rp.radius + ringW);
            var y0 = Math.floor(rp.cy - rp.radius - ringW);
            var y1 = Math.ceil(rp.cy + rp.radius + ringW);
            // Clamp render-side bounds.
            if (x0 < 0) x0 = 0;
            if (y0 < 0) y0 = 0;
            if (x1 > width - 1)  x1 = width - 1;
            if (y1 > height - 1) y1 = height - 1;
            for (var y = y0; y <= y1; y++) {
                for (var x = x0; x <= x1; x++) {
                    var dx = x - rp.cx;
                    var dy = y - rp.cy;
                    var d = Math.sqrt(dx * dx + dy * dy);
                    var edge = 1 - Math.abs(d - rp.radius) / ringW;
                    if (edge > 0) {
                        var contrib_v = rp.color.v * edge * alpha;
                        var idx = (y * width + x) * 3;
                        var ev = map[idx + 2];
                        if (contrib_v > ev) { map[idx] = rp.color.h; map[idx + 1] = rp.color.s; }
                        map[idx + 2] = Math.min(1, ev + contrib_v);
                    }
                }
            }
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
