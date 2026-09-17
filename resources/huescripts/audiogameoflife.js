/*
  Q Light Controller Plus
  audiogameoflife.js

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
    algo.name = "Audio Game of Life";
    algo.author = "QLC+ contributors";
    algo.acceptColors = 5;
    algo.usesAudio = true;
    algo.properties = [];

    algo.presetRate = 2.0;
    algo.presetSource = "Lows";
    algo.presetHealthCheck = "Oscillating";
    algo.presetHealthInterval = 1.0;
    algo.presetImpulseDecay = 0.15;
    algo.presetFreeRun = "Yes";
    algo.properties.push("name:rate|type:float|values:0.1,20|display:Rate|write:setRate|read:getRate");
    algo.properties.push("name:source|type:list|display:Source|values:Beat,Bass,Lows,Mids,High|write:setSource|read:getSource");
    algo.properties.push("name:healthCheck|type:list|display:Health Check|values:All,Dead,Oscillating,None|write:setHealthCheck|read:getHealthCheck");
    algo.properties.push("name:healthInterval|type:float|values:0.1,10|display:Health Interval|write:setHealthInterval|read:getHealthInterval");
    algo.properties.push("name:impulseDecay|type:float|values:0.01,0.5|display:Impulse Decay|write:setImpulseDecay|read:getImpulseDecay");
    algo.properties.push("name:freeRun|type:list|display:Free Run|values:Yes,No|write:setFreeRun|read:getFreeRun");
    algo.setRate = function(v) { algo.presetRate = Math.max(0.1, Math.min(20, parseFloat(v) || 0.1)); };
    algo.getRate = function() { return algo.presetRate; };
    algo.setSource = function(v) {
        algo.presetSource = ["Beat", "Bass", "Lows", "Mids", "High"].indexOf(v) >= 0 ? v : "Lows";
    };
    algo.getSource = function() { return algo.presetSource; };
    algo.setHealthCheck = function(v) {
        algo.presetHealthCheck = ["All", "Dead", "Oscillating", "None"].indexOf(v) >= 0 ? v : "Oscillating";
    };
    algo.getHealthCheck = function() { return algo.presetHealthCheck; };
    algo.setHealthInterval = function(v) { algo.presetHealthInterval = Math.max(0.1, Math.min(10, parseFloat(v) || 0.1)); };
    algo.getHealthInterval = function() { return algo.presetHealthInterval; };
    algo.setImpulseDecay = function(v) { algo.presetImpulseDecay = Math.max(0.01, Math.min(0.5, parseFloat(v) || 0.01)); };
    algo.getImpulseDecay = function() { return algo.presetImpulseDecay; };
    algo.setFreeRun = function(v) { algo.presetFreeRun = v === "No" ? "No" : "Yes"; };
    algo.getFreeRun = function() { return algo.presetFreeRun; };
    // Compatibility aliases.
    algo.setInjection = function(v) { algo.setSource(v); };
    algo.getInjection = function() { return algo.getSource(); };

    var state = {
        audioKey: "",
        geometryKey: "",
        board: [],
        carry: 0,
        healthTimer: 0,
        impulse: 0,
        recent: [],
        patternIndex: 0,
        lastEventSequence: null
    };

    function geometryIdentity(width, height) {
        return [width, height].join("|");
    }

    function idx(width, height, x, y) {
        var xx = (x + width) % width;
        var yy = (y + height) % height;
        return yy * width + xx;
    }

    function sourcePower(audio) {
        if (algo.presetSource === "Beat") return HSVUtil.clamp01(audio && audio.beat || 0);
        if (algo.presetSource === "Bass") return HSVUtil.clamp01(audio && audio.bass || 0);
        if (algo.presetSource === "Lows") return HSVUtil.clamp01(audio && audio.low || 0);
        if (algo.presetSource === "Mids") return HSVUtil.clamp01(audio && audio.mid || 0);
        return HSVUtil.clamp01(audio && audio.high || 0);
    }

    function boardSignature(board) {
        return board.join("");
    }

    function randomize(width, height) {
        state.board = new Array(width * height);
        for (var i = 0; i < state.board.length; i++) state.board[i] = Math.random() < 0.28 ? 1 : 0;
        state.recent = [boardSignature(state.board)];
    }

    function ensure(audio, width, height) {
        var audioKey = HSVUtil.audioIdentityKey(audio, state.audioKey);
        var geometryKey = geometryIdentity(width, height);
        if (state.audioKey === audioKey && state.geometryKey === geometryKey &&
            state.board.length === width * height)
            return;
        state.audioKey = audioKey;
        state.geometryKey = geometryKey;
        state.carry = 0;
        state.healthTimer = 0;
        state.impulse = 0;
        state.recent = [];
        state.patternIndex = 0;
        state.lastEventSequence = null;
        randomize(width, height);
    }

    function step(width, height) {
        var next = new Array(width * height).fill(0);
        for (var y = 0; y < height; y++) {
            for (var x = 0; x < width; x++) {
                var n = 0;
                for (var dy = -1; dy <= 1; dy++) {
                    for (var dx = -1; dx <= 1; dx++) {
                        if (dx === 0 && dy === 0) continue;
                        n += state.board[idx(width, height, x + dx, y + dy)];
                    }
                }
                var alive = state.board[idx(width, height, x, y)] === 1;
                next[idx(width, height, x, y)] = (alive && (n === 2 || n === 3)) || (!alive && n === 3) ? 1 : 0;
            }
        }
        state.board = next;
    }

    function stamp(width, height, points) {
        if (width <= 4 || height <= 4) return;
        var x = Math.floor(Math.random() * width);
        var y = Math.floor(Math.random() * height);
        for (var i = 0; i < points.length; i++) {
            var p = points[i];
            state.board[idx(width, height, x + p[0], y + p[1])] = 1;
        }
    }

    function injectPattern(width, height) {
        var patterns = [
            [[0, 0], [1, 0], [2, 0], [2, -1], [1, -2]], // glider
            [[-1, 0], [0, 0], [1, 0]],                   // blinker
            [[-1, 0], [0, 0], [1, 0], [0, -1], [1, -1], [2, -1]], // toad-ish
            [[0, 0], [1, 0], [0, 1], [3, 2], [2, 3], [3, 3]] // beacon
        ];
        stamp(width, height, patterns[state.patternIndex % patterns.length]);
        state.patternIndex++;
    }

    function shouldReset(width, height) {
        if (algo.presetHealthCheck === "None") return false;
        var alive = 0;
        for (var i = 0; i < state.board.length; i++) alive += state.board[i];
        if (algo.presetHealthCheck === "All") return alive === state.board.length;
        if (algo.presetHealthCheck === "Dead") return alive === 0;
        var sig = boardSignature(state.board);
        var matches = 0;
        for (var j = 0; j < state.recent.length; j++) {
            if (state.recent[j] === sig) matches++;
            if (matches > 1) return true;
        }
        return false;
    }

    algo.rgbMapStepCount = function() { return 1; };
    algo.rgbMapSetColors = function() {};
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, stepValue, audio) {
        var map = HSVUtil.createMap(width, height);
        ensure(audio, width, height);
        var dt = HSVUtil.audioSeconds(audio);
        var p = sourcePower(audio);
        var targetImpulse = p;
        var alpha = targetImpulse > state.impulse ? 0.99 : algo.presetImpulseDecay;
        state.impulse += (targetImpulse - state.impulse) * alpha;
        var speedFactor = algo.presetFreeRun === "Yes"
            ? Math.max(0.01, state.impulse)
            : state.impulse;
        state.carry += dt * algo.presetRate * speedFactor;
        var maxSteps = 256;
        var steps = 0;
        while (state.carry >= 1 && steps < maxSteps) {
            step(width, height);
            state.carry -= 1;
            steps++;
            var sig = boardSignature(state.board);
            state.recent.push(sig);
            if (state.recent.length > 5) state.recent.shift();
        }
        if (steps === maxSteps && state.carry >= 1)
            state.carry = state.carry % 1;

        var deltaEvents = audio && audio.events && audio.events.delta ? audio.events.delta : null;
        var eventSequence = audio && isFinite(audio.frameSequence) ? Number(audio.frameSequence) : null;
        var kickCount = 0;
        if (eventSequence === null || state.lastEventSequence !== eventSequence) {
            if (eventSequence !== null) state.lastEventSequence = eventSequence;
            var eventKicks = deltaEvents ? Math.max(0, (deltaEvents.kick || 0) + (deltaEvents.beat || 0)) : 0;
            kickCount = eventKicks;
            if (audio && audio.beatFired && eventKicks === 0) kickCount += 1;
        }
        while (kickCount-- > 0) injectPattern(width, height);

        state.healthTimer += dt;
        if (state.healthTimer >= algo.presetHealthInterval) {
            state.healthTimer = state.healthTimer % algo.presetHealthInterval;
            if (shouldReset(width, height)) randomize(width, height);
        }

        var occupancy = new Array(width * height).fill(0);
        for (var r = 0; r < state.recent.length; r++) {
            var sig2 = state.recent[r];
            for (var i2 = 0; i2 < sig2.length; i2++) occupancy[i2] += sig2.charCodeAt(i2) === 49 ? 1 : 0;
        }
        var denom = Math.max(1, state.recent.length);
        for (var y2 = 0; y2 < height; y2++) {
            for (var x2 = 0; x2 < width; x2++) {
                var id2 = idx(width, height, x2, y2);
                var occ = occupancy[id2] / denom;
                var c = HSVUtil.gradientLedfxAt(algo.colors && algo.colors.length ? algo.colors : [{ h: 0.3, s: 1, v: 1 }], occ);
                var o = (y2 * width + x2) * 3;
                map[o] = c.h;
                map[o + 1] = c.s;
                map[o + 2] = state.board[id2] ? c.v : c.v * occ * 0.35;
            }
        }
        return map;
    };

    testAlgo = algo;
    return algo;
})();
