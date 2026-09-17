/*
  Q Light Controller Plus
  audiomulticolorbar.js

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
    algo.name = "Audio Multicolor Bar";
    algo.author = "QLC+ contributors";
    algo.acceptColors = 5;
    algo.usesAudio = true;
    algo.properties = [];

    algo.presetMode = "wipe";
    algo.presetEaseMethod = "linear";
    algo.presetColorStep = 0.125;
    algo.properties.push("name:mode|type:list|display:Mode|values:cascade,wipe|write:setMode|read:getMode");
    algo.properties.push("name:easeMethod|type:list|display:Ease Method|values:ease_in_out,ease_in,ease_out,linear|write:setEaseMethod|read:getEaseMethod");
    algo.properties.push("name:colorStep|type:float|values:0.0625,0.5|display:Color Step|write:setColorStep|read:getColorStep");
    algo.setMode = function(v) { algo.presetMode = ["cascade", "wipe"].indexOf(String(v)) >= 0 ? String(v) : "wipe"; };
    algo.getMode = function() { return algo.presetMode; };
    algo.setEaseMethod = function(v) { algo.presetEaseMethod = ["ease_in_out", "ease_in", "ease_out", "linear"].indexOf(String(v)) >= 0 ? String(v) : "linear"; };
    algo.getEaseMethod = function() { return algo.presetEaseMethod; };
    algo.setColorStep = function(v) { algo.presetColorStep = Math.max(0.0625, Math.min(0.5, parseFloat(v) || 0.125)); };
    algo.getColorStep = function() { return algo.presetColorStep; };
    var outputOptions = HSVUtil.attachStripTransformControls(algo);

    var state = { phase: 0, colorIdx: 0, audioKey: "", geometryKey: "" };

    function beatDelta(audio) {
        var d = audio && audio.events && audio.events.delta;
        var beats = d && isFinite(d.beat) ? Math.floor(d.beat) : 0;
        if (beats <= 0 && audio && audio.beatFired) beats = 1;
        return beats > 0 ? beats : 0;
    }

    function beatOsc(audio) {
        return audio && audio.tempo && audio.tempo.valid && isFinite(audio.tempo.beatPhase)
            ? HSVUtil.clamp01(audio.tempo.beatPhase)
            : (audio && isFinite(audio.phase) ? HSVUtil.clamp01(audio.phase) : 0);
    }

    function ease(x) {
        if (algo.presetEaseMethod === "ease_in_out") return 0.5 * Math.sin(Math.PI * (x - 0.5)) + 0.5;
        if (algo.presetEaseMethod === "ease_in") return x * x;
        if (algo.presetEaseMethod === "ease_out") return -Math.pow(x - 1, 2) + 1;
        return x;
    }

    function resetState() {
        state.phase = 0;
        state.colorIdx = 0;
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
        var colors = algo.colors && algo.colors.length ? algo.colors : [{h: 0, s: 1, v: 1}];
        var beats = beatDelta(audio);
        for (var i = 0; i < beats; i++) {
            state.phase = 1 - state.phase;
            state.colorIdx = HSVUtil.mod1(state.colorIdx + algo.presetColorStep);
        }
        var beatValue = ease(beatOsc(audio));
        var fg = HSVUtil.gradientLedfxAt(colors, state.colorIdx);
        var bg = HSVUtil.gradientLedfxAt(colors, HSVUtil.mod1(state.colorIdx + algo.presetColorStep));
        var idx = beatValue;
        if (algo.presetMode === "wipe" && state.phase === 1) {
            idx = 1 - beatValue;
            var tmp = fg;
            fg = bg;
            bg = tmp;
        }
        var n = width * height;
        var split = Math.floor(n * HSVUtil.clamp01(idx));
        for (var p = 0; p < n; p++) {
            var color = p < split ? bg : fg;
            var pi = p * 3;
            map[pi] = color.h;
            map[pi + 1] = color.s;
            map[pi + 2] = color.v;
        }
        HSVUtil.applyStripTransforms(map, width, height, outputOptions());
        return map;
    };

    testAlgo = algo;
    return algo;
})();
