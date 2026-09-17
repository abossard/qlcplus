/*
  Q Light Controller Plus
  audiobpmbar.js

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
    algo.name = "Audio BPM Bar";
    algo.author = "QLC+ contributors";
    algo.acceptColors = 5;
    algo.usesAudio = true;
    algo.properties = [];

    algo.presetMode = "wipe";
    algo.presetEaseMethod = "ease_out";
    algo.presetColorStep = 0.125;
    algo.presetBeatSkip = "none";
    algo.presetBeatOffset = 0;
    algo.presetSkipEvery = 1;
    algo.properties.push("name:mode|type:list|display:Mode|values:wipe,bounce,in-out|write:setMode|read:getMode");
    algo.properties.push("name:easeMethod|type:list|display:Ease Method|values:ease_in_out,ease_in,ease_out,linear|write:setEaseMethod|read:getEaseMethod");
    algo.properties.push("name:colorStep|type:float|values:0.0625,0.5|display:Color Step|write:setColorStep|read:getColorStep");
    algo.properties.push("name:beatSkip|type:list|display:Beat Skip|values:none,odds,even|write:setBeatSkip|read:getBeatSkip");
    algo.properties.push("name:beatOffset|type:float|values:0,1|display:Beat Offset|write:setBeatOffset|read:getBeatOffset");
    algo.properties.push("name:skipEvery|type:list|display:Skip Every|values:1,2|write:setSkipEvery|read:getSkipEvery");
    algo.setMode = function(v) { algo.presetMode = ["wipe", "bounce", "in-out"].indexOf(String(v)) >= 0 ? String(v) : "wipe"; };
    algo.getMode = function() { return algo.presetMode; };
    algo.setEaseMethod = function(v) { algo.presetEaseMethod = ["ease_in_out", "ease_in", "ease_out", "linear"].indexOf(String(v)) >= 0 ? String(v) : "ease_out"; };
    algo.getEaseMethod = function() { return algo.presetEaseMethod; };
    algo.setColorStep = function(v) { algo.presetColorStep = Math.max(0.0625, Math.min(0.5, parseFloat(v) || 0.125)); };
    algo.getColorStep = function() { return algo.presetColorStep; };
    algo.setBeatSkip = function(v) { algo.presetBeatSkip = ["none", "odds", "even"].indexOf(String(v)) >= 0 ? String(v) : "none"; };
    algo.getBeatSkip = function() { return algo.presetBeatSkip; };
    algo.setBeatOffset = function(v) { algo.presetBeatOffset = HSVUtil.clamp01(parseFloat(v) || 0); };
    algo.getBeatOffset = function() { return algo.presetBeatOffset; };
    algo.setSkipEvery = function(v) { algo.presetSkipEvery = String(v) === "2" ? 2 : 1; };
    algo.getSkipEvery = function() { return algo.presetSkipEvery; };
    var outputOptions = HSVUtil.attachStripTransformControls(algo);

    var state = {
        phase: 0,
        colorIdx: 0,
        lastBeatOsc: 0,
        beatCount: 0,
        hasValidBeat: false,
        audioKey: "",
        geometryKey: ""
    };

    function beatOscillator(audio) {
        var p = audio && audio.tempo && isFinite(audio.tempo.beatPhase) ? audio.tempo.beatPhase : (audio && isFinite(audio.phase) ? audio.phase : 0);
        return HSVUtil.mod1(p + algo.presetBeatOffset);
    }

    function eased(x) {
        if (algo.presetEaseMethod === "ease_in_out") return 0.5 * Math.sin(Math.PI * (x - 0.5)) + 0.5;
        if (algo.presetEaseMethod === "ease_in") return x * x;
        if (algo.presetEaseMethod === "ease_out") return -Math.pow(x - 1, 2) + 1;
        return x;
    }

    function countedBeat(audio) {
        var counters = audio && audio.events && audio.events.counters;
        var raw = null;
        if (counters && isFinite(counters.beat)) raw = counters.beat;
        else if (audio && audio.tempo && isFinite(audio.tempo.beatCount)) raw = audio.tempo.beatCount;
        if (!isFinite(raw)) return null;
        var wrapped = Math.floor(raw) % 4;
        return wrapped < 0 ? wrapped + 4 : wrapped;
    }

    function advanceStateOnWrap() {
        state.phase = 1 - state.phase;
        if (state.phase === 0) state.colorIdx = HSVUtil.mod1(state.colorIdx + algo.presetColorStep);
        state.beatCount = (state.beatCount + 1) % 4;
    }

    function resetState() {
        state.phase = 0;
        state.colorIdx = 0;
        state.lastBeatOsc = 0;
        state.beatCount = 0;
        state.hasValidBeat = false;
    }

    algo.rgbMapStepCount = function() { return 1; };
    algo.rgbMapSetColors = function() {};
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step, audio) {
        var map = HSVUtil.createMap(width, height);
        var audioKey = HSVUtil.audioIdentityKey(audio, state.audioKey);
        var geometryKey = width + "x" + height;
        if (audioKey !== state.audioKey || geometryKey !== state.geometryKey) {
            resetState();
            state.audioKey = audioKey;
            state.geometryKey = geometryKey;
        }
        var beatCount = countedBeat(audio);
        if (beatCount !== null) state.beatCount = beatCount;
        var validBeat = !!(audio && audio.tempo && audio.tempo.valid && isFinite(audio.tempo.beatPhase));
        var beatOsc = validBeat ? beatOscillator(audio) : state.lastBeatOsc;
        if (validBeat && state.hasValidBeat && state.lastBeatOsc > 0.2 && beatOsc < 0.1) {
            advanceStateOnWrap();
        }
        if (validBeat) state.lastBeatOsc = beatOsc;
        state.hasValidBeat = validBeat;
        var easedX = eased(beatOsc);
        var barStart = 0;
        var barEnd = 0;
        var barLen = 0.3;
        if (algo.presetMode === "wipe") {
            if (state.phase === 0) { barEnd = easedX; barStart = 0; }
            else { barEnd = 1; barStart = easedX; }
        } else if (algo.presetMode === "bounce") {
            easedX = easedX * (1 - barLen);
            if (state.phase === 0) { barEnd = easedX + barLen; barStart = easedX; }
            else { barEnd = 1 - easedX; barStart = 1 - (easedX + barLen); }
        } else {
            if (state.phase === 0) { barEnd = easedX; barStart = 0; }
            else { barEnd = 1 - easedX; barStart = 0; }
        }
        var color = HSVUtil.gradientLedfxAt(algo.colors || [{h: 0.0, s: 1, v: 1}], state.colorIdx);
        if (algo.presetBeatSkip !== "none") {
            var skipped = (((algo.presetSkipEvery === 2 ? (state.beatCount === 0 || state.beatCount === 1) : ((state.beatCount % 2) === 0))) === (algo.presetBeatSkip === "even"));
            if (skipped) color = {h: 0, s: 0, v: 0};
        }
        var n = width * height;
        var startPx = Math.floor(n * HSVUtil.clamp01(barStart));
        var endPx = Math.floor(n * HSVUtil.clamp01(barEnd));
        for (var p = 0; p < n; p++) {
            var i = p * 3;
            if (p >= startPx && p < endPx) {
                map[i] = color.h;
                map[i + 1] = color.s;
                map[i + 2] = color.v;
            } else {
                map[i] = 0;
                map[i + 1] = 0;
                map[i + 2] = 0;
            }
        }
        HSVUtil.applyStripTransforms(map, width, height, outputOptions());
        return map;
    };

    testAlgo = algo;
    return algo;
})();
