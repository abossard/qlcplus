/*
  Q Light Controller Plus
  audioenergy2.js

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
    algo.name = "Audio Energy 2";
    algo.author = "Ported from LedFx";
    algo.acceptColors = 0;
    algo.usesAudio = true;
    algo.properties = new Array();

    algo.presetMode = "Artistic";
    algo.properties.push("name:mode|type:list|display:Response|values:Artistic,Reference,LedFx Energy 2|write:setMode|read:getMode");
    algo.setMode = function(v) {
        algo.presetMode = (v === "Reference" || v === "LedFx Energy 2") ? v : "Artistic";
        referenceState = null;
    };
    algo.getMode = function() { return algo.presetMode; };
    algo.referenceBlur = 1.5;
    algo.referenceMirror = "On";
    algo.referenceBrightness = 0.7;
    algo.referencePalette = "#ff0000,#00ff00,#0000ff";
    algo.referencePositions = "";
    algo.referenceRoll = 0;
    algo.referenceRangeStart = 0;
    algo.referenceRangeEnd = 1;
    algo.referenceFlip = "Off";
    algo.referenceBackgroundMode = "Off";
    algo.referenceBackgroundColor = "#000000";
    algo.referenceBackgroundBrightness = 1;
    algo.properties.push("name:referenceBlur|type:float|display:Reference Blur|write:setReferenceBlur|read:getReferenceBlur");
    algo.properties.push("name:referenceMirror|type:list|display:Reference Mirror|values:Off,On|write:setReferenceMirror|read:getReferenceMirror");
    algo.properties.push("name:referenceBrightness|type:float|display:Reference Brightness|write:setReferenceBrightness|read:getReferenceBrightness");
    algo.properties.push("name:referencePalette|type:string|display:Reference RGB Palette|write:setReferencePalette|read:getReferencePalette");
    algo.properties.push("name:referencePositions|type:string|display:Reference Positions (0..1 CSV)|write:setReferencePositions|read:getReferencePositions");
    algo.properties.push("name:referenceRoll|type:float|values:0,1|display:Reference Palette Roll (0..1)|write:setReferenceRoll|read:getReferenceRoll");
    algo.properties.push("name:referenceRangeStart|type:float|values:0,1|display:Reference Range Start (0..1)|write:setReferenceRangeStart|read:getReferenceRangeStart");
    algo.properties.push("name:referenceRangeEnd|type:float|values:0,1|display:Reference Range End (0..1)|write:setReferenceRangeEnd|read:getReferenceRangeEnd");
    algo.properties.push("name:referenceFlip|type:list|display:Reference Flip|values:Off,On|write:setReferenceFlip|read:getReferenceFlip");
    algo.properties.push("name:referenceBackgroundMode|type:list|display:Reference Background Mode|values:Off,Additive|write:setReferenceBackgroundMode|read:getReferenceBackgroundMode");
    algo.properties.push("name:referenceBackgroundColor|type:string|display:Reference Background Color (#rrggbb)|write:setReferenceBackgroundColor|read:getReferenceBackgroundColor");
    algo.properties.push("name:referenceBackgroundBrightness|type:float|values:0,1|display:Reference Background Brightness (0..1)|write:setReferenceBackgroundBrightness|read:getReferenceBackgroundBrightness");
    algo.setReferenceBlur = function(v) { algo.referenceBlur = Math.max(0, Math.min(10, parseFloat(v) || 0)); };
    algo.getReferenceBlur = function() { return algo.referenceBlur; };
    algo.setReferenceMirror = function(v) { algo.referenceMirror = v === "On" ? "On" : "Off"; };
    algo.getReferenceMirror = function() { return algo.referenceMirror; };
    algo.setReferenceBrightness = function(v) { algo.referenceBrightness = HSVUtil.clamp01(parseFloat(v) || 0); };
    algo.getReferenceBrightness = function() { return algo.referenceBrightness; };
    algo.setReferencePalette = function(v) {
        if (typeof v !== "string") return;
        var parsed = parseReferenceGradient(v);
        if (parsed) {
            algo.referencePalette = parsed.palette;
            algo.referencePositions = parsed.positions;
        }
    };
    algo.getReferencePalette = function() { return algo.referencePalette; };
    algo.setReferencePositions = function(v) {
        if (typeof v === "string") algo.referencePositions = v;
    };
    algo.getReferencePositions = function() { return algo.referencePositions; };
    algo.setReferenceRoll = function(v) {
        var value = parseFloat(v);
        if (!isFinite(value)) value = 0;
        algo.referenceRoll = HSVUtil.clamp01(value);
    };
    algo.getReferenceRoll = function() { return algo.referenceRoll; };
    algo.setReferenceRangeStart = function(v) {
        var value = parseFloat(v);
        if (!isFinite(value)) value = 0;
        algo.referenceRangeStart = HSVUtil.clamp01(value);
    };
    algo.getReferenceRangeStart = function() { return algo.referenceRangeStart; };
    algo.setReferenceRangeEnd = function(v) {
        var value = parseFloat(v);
        if (!isFinite(value)) value = 1;
        algo.referenceRangeEnd = HSVUtil.clamp01(value);
    };
    algo.getReferenceRangeEnd = function() { return algo.referenceRangeEnd; };
    algo.setReferenceFlip = function(v) { algo.referenceFlip = v === "On" ? "On" : "Off"; };
    algo.getReferenceFlip = function() { return algo.referenceFlip; };
    algo.setReferenceBackgroundMode = function(v) {
        algo.referenceBackgroundMode = v === "Additive" ? "Additive" : "Off";
    };
    algo.getReferenceBackgroundMode = function() { return algo.referenceBackgroundMode; };
    algo.setReferenceBackgroundColor = function(v) {
        if (typeof v === "string" && /^#[0-9a-f]{6}$/i.test(v))
            algo.referenceBackgroundColor = v;
    };
    algo.getReferenceBackgroundColor = function() { return algo.referenceBackgroundColor; };
    algo.setReferenceBackgroundBrightness = function(v) {
        var value = parseFloat(v);
        if (!isFinite(value)) value = 1;
        algo.referenceBackgroundBrightness = HSVUtil.clamp01(value);
    };
    algo.getReferenceBackgroundBrightness = function() { return algo.referenceBackgroundBrightness; };

    function parseReferenceGradient(value) {
        var raw = String(value).trim();
        if (!raw) return null;
        var parts = raw.split("|");
        var palettePart = parts[0];
        var positionPart = parts.length > 1 ? parts[1] : "";
        var tokens = palettePart.split(",");
        var stops = [];
        for (var i = 0; i < tokens.length; i++) {
            var token = String(tokens[i]).trim();
            if (!/^#[0-9a-f]{6}$/i.test(token)) return null;
            stops.push(token.toLowerCase());
        }
        if (!stops.length) return null;
        if (positionPart) {
            var parsed = HSVUtil.parsePositions(positionPart, stops.length);
            if (!parsed) return null;
            positionPart = parsed.join(",");
        }
        return { palette: stops.join(","), positions: positionPart };
    }

    function parseReferenceStops() {
        return algo.referencePalette.split(",").map(function(hex) {
            return HSVUtil.hexToHsv(hex);
        });
    }

    function referenceMap(width, height, audio) {
        var n = width * height;
        var bank = audio && audio.banks && audio.banks.full;
        var epoch = audio ? [
            audio.sourceId, audio.profileId, audio.sourceEpoch, audio.configRevision,
            algo.referencePalette, algo.referencePositions, algo.referenceRangeStart, algo.referenceRangeEnd
        ].join(":") : "";
        if (!referenceState || referenceState.n !== n || referenceState.epoch !== epoch)
            referenceState = {n: n, epoch: epoch, rollPhase: 0};
        var values = bank && bank.count
            ? HSVUtil.rangeSlice(bank.novelty, algo.referenceRangeStart, algo.referenceRangeEnd) : [];
        values = HSVUtil.interpolate(values, n);
        var seconds = HSVUtil.audioSeconds(audio);
        if (seconds > 0)
            referenceState.rollPhase = HSVUtil.mod1(referenceState.rollPhase + seconds * algo.referenceRoll);
        var stops = parseReferenceStops();
        var pixels = new Array(n);
        for (var i = 0; i < n; i++) {
            var u = n <= 1 ? 0 : i / (n - 1);
            var t = u + referenceState.rollPhase;
            if (t > 1) t -= Math.floor(t);
            var rgb = HSVUtil.gradientRgbAt(stops, t, algo.referencePositions);
            pixels[i] = [rgb[0] * values[i], rgb[1] * values[i], rgb[2] * values[i]];
        }
        return HSVUtil.referenceOutput(algo, pixels, width, height);
    }

    algo.presetSpeed = 0.075;
    algo.properties.push(
      "name:presetSpeed|type:float|display:Speed (cyc/beat)|" +
      "write:setSpeed|read:getSpeed");

    algo.presetReactivity = 0.4;
    algo.properties.push(
      "name:presetReactivity|type:float|display:Reactivity|" +
      "write:setReactivity|read:getReactivity");

    algo.setSpeed = function(_v) { algo.presetSpeed = parseFloat(_v); };
    algo.getSpeed = function() { return algo.presetSpeed; };
    algo.setReactivity = function(_v) { algo.presetReactivity = parseFloat(_v); };
    algo.getReactivity = function() { return algo.presetReactivity; };

    algo.presetSmoothing = 5;
    algo.properties.push(
      "name:presetSmoothing|type:range|display:Smoothing|" +
      "values:1,10|write:setSmoothing|read:getSmoothing");
    algo.setSmoothing = function(_v) { algo.presetSmoothing = parseInt(_v); };
    algo.getSmoothing = function() { return algo.presetSmoothing; };

    var smoothLow = 0;

    var SAT_BASE = 0.9;
    var SAT_REACT_ADD = 0.3;
    var OFFSET_AMP = 2.0;

    algo.phase = 0;

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        if (algo.presetMode === "Reference") return referenceMap(width, height, audio);
        var map = HSVUtil.createMap(width, height);
        if (!audio) return map;
        var ledFxMode = algo.presetMode === "LedFx Energy 2";

        var dt = HSVUtil.audioSeconds(audio);
        var speed = algo.presetSpeed;
        var reactivity01 = algo.presetReactivity;
        var frameScale = dt * 60;
        if (ledFxMode) {
            var rawLow = HSVUtil.powerForRange(audio, "Lows", true);
            smoothLow = HSVUtil.attackRelease(smoothLow, rawLow, 0.1, 0.1, frameScale);
        } else {
            // Asymmetric EMA smoothing (fast attack, slow decay)
            smoothLow = HSVUtil.smoothFollow(smoothLow, audio.low, algo.presetSmoothing, frameScale);
        }
        var lowPower = smoothLow;

        algo.phase = HSVUtil.mod1(algo.phase + dt * speed);

        var hue = HSVUtil.mod1(lowPower + algo.phase);
        var satThreshold = SAT_BASE - (reactivity01 + SAT_REACT_ADD) * lowPower;
        var offset = OFFSET_AMP * HSVUtil.sin01(algo.phase + reactivity01 * lowPower);

        var n = Math.max(2, width);
        for (var x = 0; x < width; x++) {
            var u = x / (n - 1);
            var v0 = HSVUtil.mod1(u + offset);
            var tri = HSVUtil.triangle(v0);
            var v = tri * tri;
            var sat = v < satThreshold ? 1 : 0;
            for (var y = 0; y < height; y++)
                HSVUtil.setPixel(map, width, x, y, hue, sat, v);
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
