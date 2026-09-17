/*
  Q Light Controller Plus
  audioflame.js

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
    algo.name = "Audio Flame";
    algo.author = "QLC+ contributors";
    algo.acceptColors = 0;
    algo.usesAudio = true;
    algo.properties = [];

    algo.presetSpawnRate = 16;
    algo.presetTopTrips = 2;
    algo.presetIntensity = 1.0;
    algo.presetBlur = 0.3;
    algo.presetColorLow = "#ff5500";
    algo.presetColorMid = "#ffaa00";
    algo.presetColorHigh = "#ffffff";
    algo.properties.push("name:spawnRate|type:float|values:0,60|display:Spawn/s|write:setSpawnRate|read:getSpawnRate");
    algo.properties.push("name:topTrips|type:float|values:0,10|display:Top Trips/s|write:setTopTrips|read:getTopTrips");
    algo.properties.push("name:intensity|type:float|values:0,2|display:Intensity|write:setIntensity|read:getIntensity");
    algo.properties.push("name:blur|type:float|values:0,1|display:Blur|write:setBlur|read:getBlur");
    algo.properties.push("name:colorLow|type:string|display:Low Color|write:setColorLow|read:getColorLow");
    algo.properties.push("name:colorMid|type:string|display:Mid Color|write:setColorMid|read:getColorMid");
    algo.properties.push("name:colorHigh|type:string|display:High Color|write:setColorHigh|read:getColorHigh");
    algo.setSpawnRate = function(v) { algo.presetSpawnRate = Math.max(0, Math.min(60, parseFloat(v) || 0)); };
    algo.getSpawnRate = function() { return algo.presetSpawnRate; };
    algo.setTopTrips = function(v) { algo.presetTopTrips = Math.max(0, Math.min(10, parseFloat(v) || 0)); };
    algo.getTopTrips = function() { return algo.presetTopTrips; };
    algo.setIntensity = function(v) { algo.presetIntensity = Math.max(0, Math.min(2, parseFloat(v) || 0)); };
    algo.getIntensity = function() { return algo.presetIntensity; };
    algo.setBlur = function(v) { algo.presetBlur = HSVUtil.clamp01(parseFloat(v) || 0); };
    algo.getBlur = function() { return algo.presetBlur; };
    algo.setColorLow = function(v) { algo.presetColorLow = /^#[0-9a-f]{6}$/i.test(String(v)) ? String(v) : "#ff5500"; };
    algo.getColorLow = function() { return algo.presetColorLow; };
    algo.setColorMid = function(v) { algo.presetColorMid = /^#[0-9a-f]{6}$/i.test(String(v)) ? String(v) : "#ffaa00"; };
    algo.getColorMid = function() { return algo.presetColorMid; };
    algo.setColorHigh = function(v) { algo.presetColorHigh = /^#[0-9a-f]{6}$/i.test(String(v)) ? String(v) : "#ffffff"; };
    algo.getColorHigh = function() { return algo.presetColorHigh; };
    // Compatibility aliases.
    algo.setSpawn = function(v) { algo.setSpawnRate(v); };
    algo.getSpawn = function() { return algo.getSpawnRate(); };
    algo.setFade = function(v) {};
    algo.getFade = function() { return 0; };

    var state = { audioKey: "", geometryKey: "", particles: [], spawnCarry: 0, topCarry: 0 };

    function geometryIdentity(width, height) {
        return [width, height].join("|");
    }

    function hexEnabled(hex) {
        return /^#[0-9a-f]{6}$/i.test(hex) && hex.toLowerCase() !== "#000000";
    }

    function spawnPopulation(width, pop, amount) {
        if (amount <= 0 || width <= 0) return;
        var births = Math.min(128, Math.floor(amount));
        for (var i = 0; i < births; i++) {
            if (state.particles.length >= 1024) break;
            state.particles.push({
                pop: pop,
                x: Math.random() * Math.max(1, width - 1),
                y: 1.0,
                vx: (Math.random() - 0.5) * 0.2,
                vy: 0.5 + Math.random() * 0.8,
                age: 0,
                maxAge: 1.2 + Math.random() * 0.8
            });
        }
    }

    function populationColor(pop) {
        if (pop === 0) return HSVUtil.parseHexRgb(algo.presetColorLow);
        if (pop === 1) return HSVUtil.parseHexRgb(algo.presetColorMid);
        return HSVUtil.parseHexRgb(algo.presetColorHigh);
    }

    function blurMap(map, width, height, amount) {
        if (amount <= 0) return map;
        var out = map.slice();
        var alpha = HSVUtil.clamp01(amount);
        for (var y = 0; y < height; y++) {
            for (var x = 0; x < width; x++) {
                var base = (y * width + x) * 3;
                var sum = [0, 0, 0], c = 0;
                for (var dy = -1; dy <= 1; dy++) {
                    for (var dx = -1; dx <= 1; dx++) {
                        var xx = x + dx, yy = y + dy;
                        if (xx < 0 || yy < 0 || xx >= width || yy >= height) continue;
                        var o = (yy * width + xx) * 3;
                        sum[0] += map[o];
                        sum[1] += map[o + 1];
                        sum[2] += map[o + 2];
                        c++;
                    }
                }
                out[base] = map[base] * (1 - alpha) + (sum[0] / c) * alpha;
                out[base + 1] = map[base + 1] * (1 - alpha) + (sum[1] / c) * alpha;
                out[base + 2] = map[base + 2] * (1 - alpha) + (sum[2] / c) * alpha;
            }
        }
        return out;
    }

    algo.rgbMapStepCount = function() { return 1; };
    algo.rgbMapSetColors = function() {};
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step, audio) {
        var map = HSVUtil.createMap(width, height);
        var audioKey = HSVUtil.audioIdentityKey(audio, state.audioKey);
        var geometryKey = geometryIdentity(width, height);
        if (state.audioKey !== audioKey || state.geometryKey !== geometryKey) {
            state.audioKey = audioKey;
            state.geometryKey = geometryKey;
            state.particles = [];
            state.spawnCarry = 0;
            state.topCarry = 0;
        }
        var dt = HSVUtil.audioSeconds(audio);
        var low = HSVUtil.clamp01(audio && audio.low || 0);
        var mid = HSVUtil.clamp01(audio && audio.mid || 0);
        var high = HSVUtil.clamp01(audio && audio.high || 0);
        var powers = [low, mid, high];
        var colors = [algo.presetColorLow, algo.presetColorMid, algo.presetColorHigh];

        state.spawnCarry += dt * algo.presetSpawnRate;
        var baseBirths = Math.min(256, Math.floor(state.spawnCarry));
        state.spawnCarry -= baseBirths;
        for (var p = 0; p < 3; p++) {
            if (!hexEnabled(colors[p])) continue;
            var births = baseBirths * (0.25 + powers[p] * algo.presetIntensity);
            spawnPopulation(width, p, births);
        }

        var alive = [];
        for (var i = 0; i < state.particles.length; i++) {
            var part = state.particles[i];
            part.age += dt;
            part.x += part.vx * dt * (1 + powers[part.pop] * algo.presetIntensity);
            part.y -= part.vy * dt * (1 + powers[part.pop] * algo.presetIntensity);
            if (part.x < 0) part.x += width;
            if (part.x >= width) part.x -= width;
            if (part.age >= part.maxAge) continue;
            if (part.y < -0.2) continue;
            alive.push(part);
            var life = HSVUtil.clamp01(1 - (part.age / part.maxAge));
            var v = Math.pow(life, 1.5);
            var sat = HSVUtil.clamp01(0.4 + 0.6 * life);
            var baseRgb = populationColor(part.pop);
            var rgbOut = [
                baseRgb[0] * v,
                baseRgb[1] * v,
                baseRgb[2] * v
            ];
            var hsv = HSVUtil.rgbToHsv(rgbOut[0], rgbOut[1], rgbOut[2]);
            var hueDrift = (1 - life) * (part.pop === 0 ? 0.09 : (part.pop === 1 ? 0.05 : 0.02));
            hsv.h = HSVUtil.mod1(hsv.h + hueDrift);
            hsv.s = sat;
            var x = Math.max(0, Math.min(width - 1, Math.round(part.x)));
            var y = Math.max(0, Math.min(height - 1, Math.round(part.y * (height - 1))));
            var o = (y * width + x) * 3;
            if (hsv.v > map[o + 2]) {
                map[o] = hsv.h;
                map[o + 1] = hsv.s;
                map[o + 2] = hsv.v;
            }
        }
        state.particles = alive;

        state.topCarry += dt * algo.presetTopTrips;
        if (state.topCarry >= 1) {
            state.topCarry = state.topCarry % 1;
            for (var x2 = 0; x2 < width; x2++) {
                var top = (0 * width + x2) * 3;
                map[top] *= 0.4;
                map[top + 1] *= 0.4;
                map[top + 2] *= 0.4;
            }
        }

        map = blurMap(map, width, height, algo.presetBlur);
        return map;
    };

    testAlgo = algo;
    return algo;
})();
