/*
  Q Light Controller Plus
  audiospectrum.js

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
    algo.name = "Audio Spectrum Bars";
    algo.author = "Ported from LedFx";
    algo.acceptColors = 0;
    algo.usesAudio = true;
    algo.properties = new Array();

    var referenceState = null;
    var rgbMixes = [
      [0, 1, 2], [0, 2, 1], [1, 0, 2],
      [1, 2, 0], [2, 0, 1], [2, 1, 0]
    ];
    algo.presetMode = "Artistic";
    algo.properties.push("name:mode|type:list|display:Response|values:Artistic,Reference|write:setMode|read:getMode");
    algo.setMode = function(v) { algo.presetMode = v === "Reference" ? v : "Artistic"; referenceState = null; };
    algo.getMode = function() { return algo.presetMode; };
    algo.referenceBlur = 1.5;
    algo.referenceMirror = "On";
    algo.referenceBrightness = 0.7;
    algo.referenceRangeStart = 0;
    algo.referenceRangeEnd = 1;
    algo.referenceFlip = "Off";
    algo.referenceBackgroundMode = "Off";
    algo.referenceBackgroundColor = "#000000";
    algo.referenceBackgroundBrightness = 1;
    algo.properties.push("name:referenceBlur|type:float|display:Reference Blur|write:setReferenceBlur|read:getReferenceBlur");
    algo.properties.push("name:referenceMirror|type:list|display:Reference Mirror|values:Off,On|write:setReferenceMirror|read:getReferenceMirror");
    algo.properties.push("name:referenceBrightness|type:float|display:Reference Brightness|write:setReferenceBrightness|read:getReferenceBrightness");
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

    function referenceMap(width, height, audio) {
      var n = width * height, bank = audio && audio.banks && audio.banks.full;
      var epoch = audio ? [
        audio.sourceId, audio.profileId, audio.sourceEpoch, audio.configRevision,
        algo.referenceRangeStart, algo.referenceRangeEnd
      ].join(":") : "";
      if (!referenceState || referenceState.n !== n || referenceState.epoch !== epoch)
        referenceState = {n: n, epoch: epoch, previous: new Array(n).fill(0), envelope: null};
      var state = referenceState;
      var start = algo.referenceRangeStart, end = algo.referenceRangeEnd;
      var processed = bank && bank.count ? HSVUtil.rangeSlice(bank.processed, start, end) : [];
      var noveltyBank = bank && bank.count ? HSVUtil.rangeSlice(bank.novelty, start, end) : [];
      var y = HSVUtil.interpolate(processed, n);
      var novelty = HSVUtil.interpolate(noveltyBank, n);
      var scale = audio && audio.timing ? audio.timing.deltaSeconds * 60 : 1;
      if (!state.output || scale > 0) {
        if (!state.envelope) state.envelope = y.slice();
        var mix = rgbMixes[algo.presetRgbMix] || rgbMixes[0];
        state.output = new Array(n);
        for (var i = 0; i < n; i++) {
          var value = y[i];
          var retention = Math.pow(value > state.envelope[i] ? 0.5 : 0.9, scale);
          state.envelope[i] = value + (state.envelope[i] - value) * retention;
          var channels = [0, 0, 0];
          channels[mix[0]] = novelty[i] * 1000 / 255;
          channels[mix[1]] = Math.abs(value - state.previous[i]) * 1000 / 255;
          channels[mix[2]] = state.envelope[i] * 1000 / 255;
          state.output[i] = channels;
        }
        state.previous = y.slice();
      }
      return HSVUtil.referenceOutput(algo, state.output, width, height);
    }

    algo.presetRgbMix = 0;
    algo.properties.push(
      "name:rgb_mix|type:range|display:RGB Mix|" +
      "values:0,5|write:setRgbMix|read:getRgbMix");
    algo.setRgbMix = function(_v) { algo.presetRgbMix = parseFloat(_v); };
    algo.getRgbMix = function() { return algo.presetRgbMix; };

    algo.presetDecay = 0.05;
    algo.properties.push(
      "name:decay|type:float|display:Decay|" +
      "write:setDecay|read:getDecay");
    algo.setDecay = function(_v) { algo.presetDecay = parseFloat(_v); };
    algo.getDecay = function() { return algo.presetDecay; };

    var prevY = null;
    var bFilter = null;

    function ensureState(pixelCount) {
      if (prevY && prevY.length === pixelCount) return;
      prevY = new Array(pixelCount).fill(0);
      bFilter = null;
    }

    function updateFilter(values, frameScale) {
      if (!bFilter || bFilter.length !== values.length) {
        bFilter = values.slice();
        return bFilter;
      }
      for (var i = 0; i < values.length; i++) {
        var alpha = values[i] > bFilter[i] ? 0.5 : 0.1;
        alpha = 1 - Math.pow(1 - alpha, frameScale);
        bFilter[i] = alpha * values[i] + (1.0 - alpha) * bFilter[i];
      }
      return bFilter;
    }

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
      if (algo.presetMode === "Reference") return referenceMap(width, height, audio);
      var pixelCount = width * height;
      ensureState(pixelCount);
      var map = HSVUtil.createMap(width, height);
      if (!audio) return map;

      var bank = audio.banks && audio.banks.full;
      var y = HSVUtil.interpolate(bank && bank.count ? bank.processed :
          [audio.low, audio.mid, audio.high], pixelCount);
      var filtered = bank && bank.count ? HSVUtil.interpolate(bank.novelty, pixelCount) : y;
      var frameScale = audio.timing ? audio.timing.deltaSeconds * 60 : 1;
      var filt = updateFilter(y, frameScale);
      var mix = rgbMixes[algo.presetRgbMix];

      for (var i = 0; i < pixelCount; i++) {
        var channels = [0, 0, 0];
        channels[mix[0]] = filtered[i];
        channels[mix[1]] = Math.abs(y[i] - prevY[i]);
        channels[mix[2]] = filt[i];

        var hsv = HSVUtil.rgbToHsv(channels[0], channels[1], channels[2]);
        map[i * 3] = hsv.h;
        map[i * 3 + 1] = hsv.s;
        map[i * 3 + 2] = hsv.v;
      }

      prevY = y;
      return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
