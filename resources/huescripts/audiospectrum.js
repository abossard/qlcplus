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

    function rangedValues(values) {
      if (!values || !values.length) return [];
      var start = Math.min(algo.referenceRangeStart, algo.referenceRangeEnd);
      var end = Math.max(algo.referenceRangeStart, algo.referenceRangeEnd);
      var from = Math.max(0, Math.floor(start * values.length));
      var to = Math.min(values.length, Math.ceil(end * values.length));
      return to > from ? values.slice(from, to) : [];
    }

    // Linear RGB stays unclipped until the final HSV wire conversion.
    function referenceOutput(pixels, width, height) {
      var n = pixels.length;
      var map = HSVUtil.createMap(width, height);
      for (var i = 0; i < n; i++) {
        var hsv = HSVUtil.rgbToHsvUnclipped(pixels[i][0], pixels[i][1], pixels[i][2]);
        map[i * 3] = hsv.h;
        map[i * 3 + 1] = hsv.s;
        map[i * 3 + 2] = hsv.v;
      }
      HSVUtil.applyStripTransforms(map, width, height, {
        flip: algo.referenceFlip,
        mirror: algo.referenceMirror,
        backgroundMode: algo.referenceBackgroundMode,
        backgroundColor: algo.referenceBackgroundColor,
        backgroundBrightness: algo.referenceBackgroundBrightness,
        brightness: algo.referenceBrightness,
        blur: algo.referenceBlur
      });
      if (algo.referenceDiagnostics) {
        var transformed = new Array(n);
        for (var j = 0; j < n; j++) {
          var o = j * 3;
          transformed[j] = HSVUtil.hsvToRgb(map[o], map[o + 1], map[o + 2]);
        }
        algo.referenceFrame = {pre: pixels, transformed: transformed};
      }
      return map;
    }

    function referenceMap(width, height, audio) {
      var n = width * height, bank = audio && audio.banks && audio.banks.full;
      var epoch = audio ? [
        audio.sourceId, audio.profileId, audio.sourceEpoch, audio.configRevision,
        algo.referenceRangeStart, algo.referenceRangeEnd
      ].join(":") : "";
      if (!referenceState || referenceState.n !== n || referenceState.epoch !== epoch)
        referenceState = {n: n, epoch: epoch, previous: new Array(n).fill(0), envelope: null};
      var state = referenceState;
      var processed = bank && bank.count ? rangedValues(bank.processed || []) : [];
      var noveltyBank = bank && bank.count ? rangedValues(bank.novelty || []) : [];
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
      return referenceOutput(state.output, width, height);
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
    var lastPixelCount = -1;

    function ensureState(pixelCount) {
      if (lastPixelCount === pixelCount && prevY) return;
      prevY = new Array(pixelCount);
      bFilter = null;
      for (var i = 0; i < pixelCount; i++) prevY[i] = 0;
      lastPixelCount = pixelCount;
    }

    function updateFilter(values, frameScale) {
      if (!bFilter || bFilter.length !== values.length) {
        bFilter = values.slice();
        return bFilter.slice();
      }
      var out = new Array(values.length);
      for (var i = 0; i < values.length; i++) {
        var alpha = values[i] > bFilter[i] ? 0.5 : 0.1;
        alpha = 1 - Math.pow(1 - alpha, frameScale);
        bFilter[i] = alpha * values[i] + (1.0 - alpha) * bFilter[i];
        out[i] = bFilter[i];
      }
      return out;
    }

    // Local RGB→HSV (the effect inherently creates colors from 3 independent
    // channels, so we convert the conceptual RGB to HSV for output).
    function toHsv(r, g, b) {
        r = Math.max(0, Math.min(1, r / 255));
        g = Math.max(0, Math.min(1, g / 255));
        b = Math.max(0, Math.min(1, b / 255));
        var mx = Math.max(r, g, b), mn = Math.min(r, g, b);
        var d = mx - mn;
        var h = 0, s = (mx === 0) ? 0 : d / mx, v = mx;
        if (d > 0) {
            if (mx === r) h = ((g - b) / d) % 6;
            else if (mx === g) h = (b - r) / d + 2;
            else h = (r - g) / d + 4;
            h /= 6;
            if (h < 0) h += 1;
        }
        return {h: h, s: s, v: v};
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
      var filtered = bank && bank.count ? HSVUtil.interpolate(bank.novelty, pixelCount) : y.slice();
      var frameScale = audio.timing ? audio.timing.deltaSeconds * 60 : 1;
      var filt = updateFilter(y, frameScale);
      var mix = rgbMixes[algo.presetRgbMix];
      var nextPrev = y.slice();

      for (var i = 0; i < pixelCount; i++) {
        var channels = [0, 0, 0];
        channels[mix[0]] = filtered[i] * 255.0;
        channels[mix[1]] = Math.abs(y[i] - prevY[i]) * 255.0;
        channels[mix[2]] = filt[i] * 255.0;

        var hsv = toHsv(channels[0], channels[1], channels[2]);
        var px = i % width;
        var row = Math.floor(i / width);
        var i3 = (row * width + px) * 3;
        map[i3] = hsv.h;
        map[i3 + 1] = hsv.s;
        map[i3 + 2] = hsv.v;
      }

      prevY = nextPrev;
      return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
