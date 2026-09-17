/*
  Q Light Controller Plus
  audioenergy.js

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
    algo.name = "Audio Energy";
    algo.author = "Ported from LedFx";
    algo.acceptColors = 3;
    algo.usesAudio = true;
    algo.properties = new Array();

    var referenceState = null;
    algo.presetMode = "Artistic";
    algo.properties.push("name:mode|type:list|display:Response|values:Artistic,Reference|write:setMode|read:getMode");
    algo.setMode = function(v) { algo.presetMode = v === "Reference" ? v : "Artistic"; referenceState = null; };
    algo.getMode = function() { return algo.presetMode; };
    algo.referenceBlur = 1.5;
    algo.referenceMirror = "On";
    algo.referenceBrightness = 0.7;
    algo.referencePalette = "#ff0000,#00ff00,#0000ff";
    algo.referenceSensitivity = 0.6;
    algo.referenceMixingMode = "Additive";
    algo.referenceColorCycler = "Off";
    algo.referenceCyclePalette = "";
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
    algo.properties.push("name:referenceSensitivity|type:float|values:0.3,0.99|display:Reference Sensitivity (0.3..0.99)|write:setReferenceSensitivity|read:getReferenceSensitivity");
    algo.properties.push("name:referenceMixingMode|type:list|display:Reference Mixing|values:Additive,Overlap|write:setReferenceMixingMode|read:getReferenceMixingMode");
    algo.properties.push("name:referenceColorCycler|type:list|display:Reference Color Cycler|values:Off,On|write:setReferenceColorCycler|read:getReferenceColorCycler");
    algo.properties.push("name:referenceCyclePalette|type:string|display:Reference Cycle Palette|write:setReferenceCyclePalette|read:getReferenceCyclePalette");
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
    algo.setReferencePalette = function(v) { if (/^#[0-9a-f]{6},#[0-9a-f]{6},#[0-9a-f]{6}$/i.test(v)) algo.referencePalette = v; };
    algo.getReferencePalette = function() { return algo.referencePalette; };
    algo.setReferenceSensitivity = function(v) {
      var value = parseFloat(v);
      if (!isFinite(value)) value = 0.6;
      algo.referenceSensitivity = Math.max(0.3, Math.min(0.99, value));
    };
    algo.getReferenceSensitivity = function() { return algo.referenceSensitivity; };
    algo.setReferenceMixingMode = function(v) { algo.referenceMixingMode = v === "Overlap" ? "Overlap" : "Additive"; };
    algo.getReferenceMixingMode = function() { return algo.referenceMixingMode; };
    algo.setReferenceColorCycler = function(v) { algo.referenceColorCycler = v === "On" ? "On" : "Off"; };
    algo.getReferenceColorCycler = function() { return algo.referenceColorCycler; };
    algo.setReferenceCyclePalette = function(v) {
      if (typeof v === "string") algo.referenceCyclePalette = v;
    };
    algo.getReferenceCyclePalette = function() { return algo.referenceCyclePalette; };
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

    function parseReferencePalette() {
      return algo.referencePalette.split(",").map(function(hex) {
        return HSVUtil.parseHexRgb(hex);
      });
    }

    function parseCyclePalette(base) {
      var tokens = typeof algo.referenceCyclePalette === "string"
        ? algo.referenceCyclePalette.split(",") : [];
      var parsed = [];
      for (var i = 0; i < tokens.length; i++) {
        var token = String(tokens[i]).trim();
        if (/^#[0-9a-f]{6}$/i.test(token)) parsed.push(HSVUtil.parseHexRgb(token));
      }
      if (parsed.length) return parsed;
      if (algo.colors && algo.colors.length) {
        return algo.colors.map(function(stop) {
          return HSVUtil.hsvToRgb(stop.h, stop.s, stop.v);
        });
      }
      return base.map(function(color) { return color.slice(); });
    }

    function rangedThirdMeans(audio) {
      var bank = audio && audio.banks && audio.banks.full;
      var values = bank && bank.count ? bank.processed : [];
      if (!values || values.length === 0) return [0, 0, 0];
      var start = Math.min(algo.referenceRangeStart, algo.referenceRangeEnd);
      var end = Math.max(algo.referenceRangeStart, algo.referenceRangeEnd);
      var from = Math.max(0, Math.floor(start * values.length));
      var to = Math.min(values.length, Math.ceil(end * values.length));
      values = to > from ? values.slice(from, to) : [];
      if (!values.length) return [0, 0, 0];
      var bounds = [0, Math.floor(values.length * 0.2), Math.floor(values.length * 0.5), values.length];
      var means = [0, 0, 0];
      for (var band = 0; band < 3; band++) {
        var total = 0;
        for (var i = bounds[band]; i < bounds[band + 1]; i++) total += values[i] || 0;
        var count = Math.max(1, bounds[band + 1] - bounds[band]);
        means[band] = total / count;
      }
      return means;
    }

    function consumeKickDelta(audio, state) {
      var delta = audio && audio.events && audio.events.delta && isFinite(audio.events.delta.kick)
        ? Math.max(0, Math.floor(audio.events.delta.kick)) : 0;
      if (!audio || !isFinite(audio.frameSequence)) return delta;
      var signature = [
        audio.sourceId, audio.profileId, audio.sourceEpoch, audio.configRevision, audio.frameSequence
      ].join(":");
      if (state.lastKickSignature === signature) return 0;
      state.lastKickSignature = signature;
      return delta;
    }

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
        var n = width * height;
        var epoch = audio ? [
            audio.sourceId, audio.profileId, audio.sourceEpoch, audio.configRevision,
            algo.referenceMixingMode, algo.referenceRangeStart, algo.referenceRangeEnd
        ].join(":") : "";
        if (!referenceState || referenceState.n !== n || referenceState.epoch !== epoch) {
            var baseColors = parseReferencePalette();
            referenceState = {
                n: n,
                epoch: epoch,
                pixels: null,
                initialized: false,
                colorCycler: 0,
                lowsColor: baseColors[0].slice(),
                midsColor: baseColors[1].slice(),
                highColor: baseColors[2].slice(),
                lastKickSignature: ""
            };
        }
        var state = referenceState;
        var means = rangedThirdMeans(audio);
        var multiplier = 1.6 - algo.referenceBlur / 17;
        var fills = [0, 1, 2].map(function(band) {
            return Math.max(0, Math.floor(n * multiplier * means[band]));
        });
        if (algo.referenceColorCycler === "On") {
            var kicks = consumeKickDelta(audio, state);
            if (kicks > 0) {
                var pool = parseCyclePalette(parseReferencePalette());
                for (var k = 0; k < kicks; k++) {
                    state.colorCycler = (state.colorCycler + 1) % 3;
                    var nextColor = pool[Math.floor(Math.random() * pool.length)];
                    if (state.colorCycler === 0) state.lowsColor = nextColor.slice();
                    else if (state.colorCycler === 1) state.midsColor = nextColor.slice();
                    else state.highColor = nextColor.slice();
                }
            }
        }
        var colors = [state.lowsColor, state.midsColor, state.highColor];
        var target = new Array(n);
        for (var i = 0; i < n; i++) {
            var rgb = [0, 0, 0];
            if (algo.referenceMixingMode === "Overlap") {
                if (i < fills[0]) rgb = colors[0].slice();
                if (i < fills[1]) rgb = colors[1].slice();
                if (i < fills[2]) rgb = colors[2].slice();
            } else {
                for (var band = 0; band < 3; band++) {
                    if (i < fills[band]) {
                        rgb[0] += colors[band][0];
                        rgb[1] += colors[band][1];
                        rgb[2] += colors[band][2];
                    }
                }
            }
            target[i] = rgb;
        }
        if (!state.pixels || state.pixels.length !== n) {
            state.pixels = target.map(function(pixel) { return pixel.slice(); });
            state.initialized = true;
        } else {
            var scale = HSVUtil.audioSeconds(audio) * 60;
            if (scale > 0) {
                var sensitivity = Math.max(0.3, Math.min(0.99, algo.referenceSensitivity));
                var alphaRise = sensitivity;
                var alphaDecay = (sensitivity - 0.1) * 0.7;
                for (var p = 0; p < n; p++) {
                    for (var c = 0; c < 3; c++) {
                        var current = state.pixels[p][c];
                        var next = target[p][c];
                        var alpha = next > current ? alphaRise : alphaDecay;
                        alpha = 1 - Math.pow(1 - alpha, scale);
                        state.pixels[p][c] = current + alpha * (next - current);
                    }
                }
            }
        }
        return referenceOutput(state.pixels, width, height);
    }

    algo.presetMultiplier = 1.6;
    algo.properties.push(
      "name:presetMultiplier|type:float|display:Fill Amount|" +
      "write:setMultiplier|read:getMultiplier");

    algo.setMultiplier = function(_v) { algo.presetMultiplier = parseFloat(_v); };
    algo.getMultiplier = function() { return algo.presetMultiplier; };

    algo.presetSmoothing = 5;
    algo.properties.push(
      "name:presetSmoothing|type:range|display:Smoothing|" +
      "values:1,10|write:setSmoothing|read:getSmoothing");
    algo.setSmoothing = function(_v) { algo.presetSmoothing = parseInt(_v); };
    algo.getSmoothing = function() { return algo.presetSmoothing; };

    var smoothPow = [0, 0, 0];

    // Bar-level build-up / release state
    var barEnergy = 0;
    var peakEnergy = 0;
    var releaseFlash = 0;

    var DEFAULT_BAND_COLORS = [
      {h: 0.958, s: 1.0, v: 1.0},
      {h: 0.167, s: 1.0, v: 1.0},
      {h: 0.611, s: 0.75, v: 1.0}
    ];
    var BEAT_PULSE_AMP = 0.25;
    var idx = new Array(3);

    algo.rgbMapStepCount = function(width, height)
    {
        return 1;
    };

    algo.rgbMapSetColors = function(rawColors) { };

    algo.rgbMapGetColors = function()
    {
        return algo.colors
            ? algo.colors.slice()
            : DEFAULT_BAND_COLORS.slice();
    };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        if (algo.presetMode === "Reference") return referenceMap(width, height, audio);
        var map = HSVUtil.createMap(width, height);

        if (!audio)
            return map;

        var rawPows = [audio.low, audio.mid, audio.high];
        var frameScale = audio.timing ? audio.timing.deltaSeconds * 50 : 1;
        // Asymmetric EMA: fast attack, slow decay on bar-fill power
        var smoothing = algo.presetSmoothing / 10.0;
        var riseAlpha = 0.5 * (1 - smoothing) + 0.05;
        var decayAlpha = 0.02 + 0.03 * (1 - smoothing);
        for (var sb = 0; sb < 3; sb++) {
            var sa = rawPows[sb] > smoothPow[sb] ? riseAlpha : decayAlpha;
            sa = 1 - Math.pow(1 - sa, frameScale);
            smoothPow[sb] += sa * (rawPows[sb] - smoothPow[sb]);
        }
        var bandPowers = smoothPow;
        var bandColors = algo.colors || DEFAULT_BAND_COLORS;

        // --- Bar-level build-up ---
        var rawEnergy = (rawPows[0] + rawPows[1] * 0.5 + rawPows[2] * 0.3) / 1.8;
        barEnergy += rawEnergy * audio.dt;
        if (barEnergy > peakEnergy) peakEnergy = barEnergy;
        if (audio.downbeat) {
            releaseFlash = Math.min(1, peakEnergy * 0.5);
            barEnergy = 0;
            peakEnergy = 0;
        }
        releaseFlash *= Math.pow(0.85, frameScale);

        var beatBoost = 1.0 + BEAT_PULSE_AMP * audio.cosPulse;

        var multiplier = algo.presetMultiplier;
        // Release momentarily extends bar fill so bars "breathe" with the phrase
        var fillBoost = 1.0 + releaseFlash * 0.35;
        for (var k = 0; k < 3; k++)
            idx[k] = Math.min(width, Math.floor(multiplier * fillBoost * width * bandPowers[k]));

        for (var y = 0; y < height; y++)
        {
            for (var x = 0; x < width; x++)
            {
                // Widest-reaching band's color wins (last band covering x)
                var col = null;
                for (var k = 0; k < 3; k++) {
                    if (x < idx[k]) col = bandColors[k];
                }

                if (col) {
                    var brightness = Math.min(1.0, beatBoost + releaseFlash * 0.4);
                    HSVUtil.setPixel(map, width, x, y, col.h, col.s, col.v * brightness);
                }
            }
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
