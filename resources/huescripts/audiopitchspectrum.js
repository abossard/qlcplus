/*
  Q Light Controller Plus
  audiopitchspectrum.js

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
    algo.name = "Audio Pitch Spectrum";
    algo.author = "QLC+ contributors";
    algo.acceptColors = 5;
    algo.usesAudio = true;
    algo.properties = [];

    algo.presetBlur = 1.0;
    algo.presetMirror = "Yes";
    algo.presetFadeRate = 0.15;
    algo.presetResponsiveness = 0.15;
    algo.properties.push("name:blur|type:float|values:0,10|display:Blur (0..10)|write:setBlur|read:getBlur");
    algo.properties.push("name:mirror|type:list|display:Mirror|values:Yes,No|write:setMirror|read:getMirror");
    algo.properties.push("name:fadeRate|type:float|values:0,1|display:Fade Rate (0..1)|write:setFadeRate|read:getFadeRate");
    algo.properties.push("name:responsiveness|type:float|values:0,1|display:Responsiveness (0..1)|write:setResponsiveness|read:getResponsiveness");
    algo.setBlur = function(v) { algo.presetBlur = Math.max(0, Math.min(10, parseFloat(v) || 0)); };
    algo.getBlur = function() { return algo.presetBlur; };
    algo.setMirror = function(v) { algo.presetMirror = v === "No" ? "No" : "Yes"; };
    algo.getMirror = function() { return algo.presetMirror; };
    algo.setFadeRate = function(v) { algo.presetFadeRate = HSVUtil.clamp01(parseFloat(v) || 0); };
    algo.getFadeRate = function() { return algo.presetFadeRate; };
    algo.setResponsiveness = function(v) { algo.presetResponsiveness = HSVUtil.clamp01(parseFloat(v) || 0); };
    algo.getResponsiveness = function() { return algo.presetResponsiveness; };
    algo.setFade = function(v) { algo.setFadeRate(v); };
    algo.getFade = function() { return algo.getFadeRate(); };
    var readOutputControls = HSVUtil.attachStripTransformControls(algo);

    var state = {
        audioKey: "",
        geometryKey: "",
        avgMidi: 0,
        hasValidPitch: false,
        hasCapturedFrame: false,
        pixels: []
    };

    function ensure(width, height, audio) {
        var nextAudioKey = HSVUtil.audioIdentityKey(audio, state.audioKey);
        var nextGeometryKey = width + "|" + height;
        var n = width * height;
        if (state.audioKey !== nextAudioKey || state.geometryKey !== nextGeometryKey || state.pixels.length !== n) {
            state.audioKey = nextAudioKey;
            state.geometryKey = nextGeometryKey;
            state.avgMidi = 0;
            state.hasValidPitch = false;
            state.hasCapturedFrame = false;
            state.pixels = new Array(n);
            for (var i = 0; i < n; i++) state.pixels[i] = [0, 0, 0];
        }
    }

    algo.rgbMapStepCount = function() { return 1; };
    algo.rgbMapSetColors = function() {};
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step, audio) {
        var map = HSVUtil.createMap(width, height);
        ensure(width, height, audio);
        var n = width * height;

        var nominalSteps = HSVUtil.audioSeconds(audio) * 60;
        if (!isFinite(nominalSteps) || nominalSteps < 0) nominalSteps = 0;

        var bank = audio && audio.banks && audio.banks.full;
        var mel = bank && bank.count ? HSVUtil.interpolate(bank.processed || [], n) : new Array(n).fill(0);
        var pitch = audio && audio.pitch ? audio.pitch : {};
        var hasPitch = !!(pitch.valid && isFinite(pitch.midi) && pitch.midi >= 21);
        var midiValue = hasPitch ? pitch.midi : 0;
        if (hasPitch && !state.hasValidPitch) {
            state.avgMidi = midiValue;
            state.hasValidPitch = true;
        } else if (hasPitch && nominalSteps > 0 && algo.presetResponsiveness > 0) {
            var avgRetain = Math.pow(1 - algo.presetResponsiveness, nominalSteps);
            state.avgMidi = state.avgMidi * avgRetain + midiValue * (1 - avgRetain);
        }

        var effectiveSteps = nominalSteps;
        if (effectiveSteps <= 0 && hasPitch && !state.hasCapturedFrame) effectiveSteps = 1;

        var noteColor = [0, 0, 0];
        if (state.hasValidPitch && state.avgMidi >= 21) {
            var scaled = HSVUtil.clamp01((state.avgMidi - 21) / (108 - 21));
            var hsv = HSVUtil.gradientLedfxAt(algo.colors || [{h: 0.0, s: 1, v: 1}, {h: 0.66, s: 1, v: 1}], scaled);
            noteColor = HSVUtil.hsvToRgb(hsv.h, hsv.s, hsv.v);
        }

        if (effectiveSteps > 0) {
            var retain = 1 - algo.presetFadeRate;
            for (var i = 0; i < n; i++) {
                var w = HSVUtil.clamp01(isFinite(mel[i]) ? mel[i] : 0);
                var a = retain * (1 - w);
                var b = retain * w;
                var decay = Math.pow(a, effectiveSteps);
                var injectGain = 0;
                if (b > 0) {
                    var denom = 1 - a;
                    injectGain = Math.abs(denom) < 1e-12
                        ? b * effectiveSteps
                        : b * (1 - decay) / denom;
                }
                var prev = state.pixels[i];
                state.pixels[i] = [
                    prev[0] * decay + noteColor[0] * injectGain,
                    prev[1] * decay + noteColor[1] * injectGain,
                    prev[2] * decay + noteColor[2] * injectGain
                ];
            }
            state.hasCapturedFrame = true;
        }

        var out = new Array(n);
        for (var p = 0; p < n; p++) {
            var hsvOut = HSVUtil.rgbToHsvUnclipped(state.pixels[p][0], state.pixels[p][1], state.pixels[p][2]);
            out[p] = hsvOut;
        }
        for (var j = 0; j < n; j++) {
            var o = j * 3;
            map[o] = out[j].h;
            map[o + 1] = out[j].s;
            map[o + 2] = out[j].v;
        }
        HSVUtil.applyStripTransforms(map, width, height, readOutputControls());
        return map;
    };

    testAlgo = algo;
    return algo;
})();
