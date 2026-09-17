/*
  Q Light Controller Plus
  audiodigitalrain.js

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
    algo.name = "Audio Digital Rain";
    algo.author = "QLC+ contributors";
    algo.acceptColors = 3;
    algo.usesAudio = true;
    algo.properties = [];

    algo.presetCount = 1.9;
    algo.presetAddSpeed = 30.0;
    algo.presetWidth = 1;
    algo.presetRunSeconds = 2.0;
    algo.presetTail = 67;
    algo.presetTailSegments = 10;
    algo.presetImpulseDecay = 0.01;
    algo.presetMultiplier = 10.0;
    algo.presetSpeedScalar = 1.0;
    algo.presetMaxStreams = 0;
    algo.properties.push("name:count|type:float|values:0.01,4|display:Count|write:setCount|read:getCount");
    algo.properties.push("name:addSpeed|type:float|values:0.1,30|display:Add Speed|write:setAddSpeed|read:getAddSpeed");
    algo.properties.push("name:lineWidth|type:range|display:Line Width (%)|values:1,30|write:setLineWidth|read:getLineWidth");
    algo.properties.push("name:runSeconds|type:float|values:1,10|display:Run Seconds|write:setRunSeconds|read:getRunSeconds");
    algo.properties.push("name:tail|type:range|display:Tail (%)|values:1,100|write:setTail|read:getTail");
    algo.properties.push("name:tailSegments|type:range|display:Tail Segments|values:2,30|write:setTailSegments|read:getTailSegments");
    algo.properties.push("name:impulseDecay|type:float|values:0.01,0.3|display:Impulse Decay|write:setImpulseDecay|read:getImpulseDecay");
    algo.properties.push("name:multiplier|type:float|values:0,10|display:Multiplier|write:setMultiplier|read:getMultiplier");
    algo.setCount = function(v) { algo.presetCount = Math.max(0.01, Math.min(4, parseFloat(v) || 0.01)); };
    algo.getCount = function() { return algo.presetCount; };
    algo.setAddSpeed = function(v) { algo.presetAddSpeed = Math.max(0.1, Math.min(30, parseFloat(v) || 0.1)); };
    algo.getAddSpeed = function() { return algo.presetAddSpeed; };
    algo.setLineWidth = function(v) { algo.presetWidth = Math.max(1, Math.min(30, parseInt(v) || 1)); };
    algo.getLineWidth = function() { return algo.presetWidth; };
    algo.setRunSeconds = function(v) { algo.presetRunSeconds = Math.max(1, Math.min(10, parseFloat(v) || 1)); };
    algo.getRunSeconds = function() { return algo.presetRunSeconds; };
    algo.setTail = function(v) { algo.presetTail = Math.max(1, Math.min(100, parseInt(v) || 1)); };
    algo.getTail = function() { return algo.presetTail; };
    algo.setTailSegments = function(v) { algo.presetTailSegments = Math.max(2, Math.min(30, parseInt(v) || 2)); };
    algo.getTailSegments = function() { return algo.presetTailSegments; };
    algo.setImpulseDecay = function(v) { algo.presetImpulseDecay = Math.max(0.01, Math.min(0.3, parseFloat(v) || 0.01)); };
    algo.getImpulseDecay = function() { return algo.presetImpulseDecay; };
    algo.setMultiplier = function(v) { algo.presetMultiplier = Math.max(0, Math.min(10, parseFloat(v) || 0)); };
    algo.getMultiplier = function() { return algo.presetMultiplier; };
    algo.setSpeed = function(v) {
        var f = parseFloat(v);
        if (!isFinite(f)) f = 1;
        algo.presetSpeedScalar = Math.max(0, f);
    };
    algo.getSpeed = function() { return algo.presetSpeedScalar; };
    algo.setMaxStreams = function(v) {
        var i = parseInt(v);
        if (!isFinite(i) || i < 0) i = 0;
        algo.presetMaxStreams = i;
    };
    algo.getMaxStreams = function() { return algo.presetMaxStreams; };
    algo.setBirthsPerSecond = function(v) { algo.setAddSpeed(v); };
    algo.getBirthsPerSecond = function() { return algo.getAddSpeed(); };
    var _setTail = algo.setTail;
    algo.setTail = function(v) {
        var f = parseFloat(v);
        if (isFinite(f) && f > 0 && f < 1) _setTail(f * 100);
        else _setTail(v);
    };
    var readOutputControls = HSVUtil.attachStripTransformControls(algo);

    var state = {
        key: "",
        audioKey: "",
        lines: [],
        lastAdded: 0,
        low: 0, mid: 0, high: 0
    };

    function makeFadeMultipliers() {
        var n = Math.max(2, algo.presetTailSegments);
        var out = new Array(n);
        for (var i = 0; i < n; i++) out[i] = 1 - i / (n - 1);
        return out;
    }

    function updateImpulse(current, target) {
        var alpha = target > current ? 0.99 : algo.presetImpulseDecay;
        return current + (target - current) * alpha;
    }

    function rawPower(audio, name) {
        var raw = audio && audio.powers && audio.powers.raw ? audio.powers.raw : {};
        var value = raw[name];
        if (!isFinite(value)) return 0;
        return HSVUtil.clamp01(value);
    }

    function createLine(width, fadeMultipliers) {
        var r = Math.random();
        var color = HSVUtil.gradientLedfxAt(algo.colors || [{h: 0.39, s: 1, v: 1}], r);
        return {
            nx: Math.random(),
            ny: 0,
            color: color,
            offset: r,
            speed: 0.1 + r * 0.9,
            impulseIndex: Math.floor(r * 3),
            fadeMultipliers: fadeMultipliers
        };
    }

    algo.rgbMapStepCount = function() { return 1; };
    algo.rgbMapSetColors = function() {};
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step, audio) {
        var map = HSVUtil.createMap(width, height);
        var audioKey = HSVUtil.audioIdentityKey(audio, state.audioKey);
        var key = [width, height, audioKey].join("|");
        if (key !== state.key) {
            state.key = key;
            state.audioKey = audioKey;
            state.lines = [];
            state.lastAdded = 0;
            state.low = 0;
            state.mid = 0;
            state.high = 0;
        }
        var dt = HSVUtil.audioSeconds(audio);
        var beatOsc = audio && audio.tempo && isFinite(audio.tempo.beatPhase) ? HSVUtil.clamp01(audio.tempo.beatPhase) : 0;
        var impulses = [state.low, state.mid, state.high];
        var lowTarget = rawPower(audio, "low") * algo.presetMultiplier;
        var midTarget = rawPower(audio, "mid") * algo.presetMultiplier;
        var highTarget = rawPower(audio, "high") * algo.presetMultiplier;
        state.low = updateImpulse(state.low, lowTarget);
        state.mid = updateImpulse(state.mid, midTarget);
        state.high = updateImpulse(state.high, highTarget);
        impulses[0] = state.low;
        impulses[1] = state.mid;
        impulses[2] = state.high;

        var lineCap = algo.presetMaxStreams > 0 ? algo.presetMaxStreams : Math.max(1, Math.floor(algo.presetCount * width));
        var fadeMultipliers = makeFadeMultipliers();
        var lineWidth = Math.max(1, Math.floor(width * (algo.presetWidth / 100)));
        var tailPixels = Math.floor(height * (algo.presetTail / 100)) - lineWidth;
        var segment = tailPixels > 0 ? tailPixels / fadeMultipliers.length : 0;

        state.lastAdded += dt;
        var spawnPeriod = 1 / Math.max(0.1, algo.presetAddSpeed);
        var attempts = Math.floor(state.lastAdded / spawnPeriod);
        if (attempts > 0) state.lastAdded -= attempts * spawnPeriod;
        var maxAttempts = Math.max(4, lineCap * 4);
        if (attempts > maxAttempts) attempts = maxAttempts;
        while (attempts-- > 0) {
            if (state.lines.length < lineCap) state.lines.push(createLine(width, fadeMultipliers));
        }

        var alive = [];
        for (var li = 0; li < state.lines.length; li++) {
            var line = state.lines[li];
            var movement = dt / algo.presetRunSeconds * algo.presetSpeedScalar * (1 + impulses[line.impulseIndex]);
            line.ny += movement * line.speed;
            var tailNorm = algo.presetTail / 100;
            if (line.ny > (1 + tailNorm)) continue;
            alive.push(line);
            var x = Math.floor(line.nx * width);
            var y = Math.floor(line.ny * height);
            for (var s = 0; s < line.fadeMultipliers.length; s++) {
                if (segment <= 0) break;
                var yStart = Math.floor(y - (lineWidth - 1) - segment * s);
                var yEnd = Math.floor(yStart - segment);
                var mult = line.fadeMultipliers[s];
                var tailV = HSVUtil.clamp01(line.color.v * mult);
                for (var yy = yEnd; yy <= yStart; yy++) {
                    if (yy < 0 || yy >= height) continue;
                    for (var wx = 0; wx < lineWidth; wx++) {
                        var xx = x + wx;
                        if (xx < 0 || xx >= width) continue;
                        var o = (yy * width + xx) * 3;
                        if (tailV > map[o + 2]) {
                            map[o] = line.color.h;
                            map[o + 1] = line.color.s;
                            map[o + 2] = tailV;
                        }
                    }
                }
            }
            var beatRoll = HSVUtil.mod1(beatOsc + line.offset);
            var headV = HSVUtil.clamp01(0.5 + beatRoll * 0.5);
            for (var hw = 0; hw < lineWidth; hw++) {
                var hx = x + hw;
                if (hx < 0 || hx >= width || y < 0 || y >= height) continue;
                var ho = (y * width + hx) * 3;
                if (headV > map[ho + 2]) {
                    map[ho] = 0;
                    map[ho + 1] = 0;
                    map[ho + 2] = headV;
                }
            }
        }
        state.lines = alive;
        return HSVUtil.applyStripTransforms(map, width, height, readOutputControls());
    };

    testAlgo = algo;
    return algo;
})();
