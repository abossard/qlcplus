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
    algo.properties.push("name:referenceBlur|type:float|display:Reference Blur|write:setReferenceBlur|read:getReferenceBlur");
    algo.properties.push("name:referenceMirror|type:list|display:Reference Mirror|values:Off,On|write:setReferenceMirror|read:getReferenceMirror");
    algo.properties.push("name:referenceBrightness|type:float|display:Reference Brightness|write:setReferenceBrightness|read:getReferenceBrightness");
    algo.properties.push("name:referencePalette|type:string|display:Reference RGB Palette|write:setReferencePalette|read:getReferencePalette");
    algo.setReferenceBlur = function(v) { algo.referenceBlur = Math.max(0, Math.min(10, parseFloat(v) || 0)); };
    algo.getReferenceBlur = function() { return algo.referenceBlur; };
    algo.setReferenceMirror = function(v) { algo.referenceMirror = v === "On" ? "On" : "Off"; };
    algo.getReferenceMirror = function() { return algo.referenceMirror; };
    algo.setReferenceBrightness = function(v) { algo.referenceBrightness = HSVUtil.clamp01(parseFloat(v) || 0); };
    algo.getReferenceBrightness = function() { return algo.referenceBrightness; };
    algo.setReferencePalette = function(v) { if (/^#[0-9a-f]{6},#[0-9a-f]{6},#[0-9a-f]{6}$/i.test(v)) algo.referencePalette = v; };
    algo.getReferencePalette = function() { return algo.referencePalette; };

    function referenceOutput(pixels, width, height) {
        var n = pixels.length, values = pixels;
        if (algo.referenceMirror === "On") {
          values = new Array(n);
          for (var i = 0; i < n; i++) {
            var a = 2 * i, b = a + 1;
            a = a < n ? n - 1 - a : a - n;
            b = b < n ? n - 1 - b : b - n;
            values[i] = [Math.max(pixels[a][0], pixels[b][0]),
                         Math.max(pixels[a][1], pixels[b][1]), Math.max(pixels[a][2], pixels[b][2])];
          }
        }
        var sigma = algo.referenceBlur;
        var radius = sigma > 0 && n > 3 ? Math.max(1, Math.min(Math.floor((n - 1) / 2), Math.round(4 * sigma))) : 0;
        var weights = [], sum = 0;
        for (var d = -radius; d <= radius; d++) {
            var weight = radius ? Math.exp(-d * d / (2 * sigma * sigma)) : 1;
            weights.push(weight); sum += weight;
        }
        var transformed = new Array(n);
        for (var i = 0; i < n; i++) {
            var r = 0, g = 0, b = 0;
            for (var d = Math.max(-radius, -i); d <= radius && i + d < n; d++) {
                var pixel = values[i + d], weight = weights[d + radius];
                r += pixel[0] * weight; g += pixel[1] * weight; b += pixel[2] * weight;
            }
            transformed[i] = [r / sum * algo.referenceBrightness,
                              g / sum * algo.referenceBrightness, b / sum * algo.referenceBrightness];
        }
        if (algo.referenceDiagnostics) algo.referenceFrame = {pre: pixels, transformed: transformed};
        var map = HSVUtil.createMap(width, height);
        for (var i = 0; i < n; i++) {
            var pixel = transformed[i];
            var r = HSVUtil.clamp01(pixel[0]), g = HSVUtil.clamp01(pixel[1]), b = HSVUtil.clamp01(pixel[2]);
            var max = Math.max(r, g, b), delta = max - Math.min(r, g, b), h = 0;
            if (delta) h = (max === r ? (g - b) / delta : max === g ? 2 + (b - r) / delta : 4 + (r - g) / delta) / 6;
            map[i * 3] = HSVUtil.mod1(h); map[i * 3 + 1] = max ? delta / max : 0; map[i * 3 + 2] = max;
        }
        return map;
    }

    function referenceMap(width, height, audio) {
        var n = width * height, bank = audio && audio.banks && audio.banks.full;
        var epoch = audio ? [audio.sourceId, audio.profileId, audio.sourceEpoch, audio.configRevision].join(":") : "";
        if (!referenceState || referenceState.n !== n || referenceState.epoch !== epoch)
            referenceState = {n: n, epoch: epoch, pixels: null};
        var values = bank && bank.count ? bank.processed : [];
        var bounds = [0, Math.floor(values.length * 0.2), Math.floor(values.length * 0.5), values.length];
        var fills = [0, 1, 2].map(function(band) {
            var total = 0;
            for (var i = bounds[band]; i < bounds[band + 1]; i++) total += values[i];
            return Math.floor(n * (1.6 - algo.referenceBlur / 17) * total / Math.max(1, bounds[band + 1] - bounds[band]));
        });
        var colors = algo.referencePalette.split(",").map(function(hex) {
            return [1, 3, 5].map(function(offset) { return parseInt(hex.substr(offset, 2), 16) / 255; });
        });
        var scale = audio && audio.timing ? audio.timing.deltaSeconds * 60 : 1;
        var old = referenceState.pixels;
        referenceState.pixels = new Array(n).fill(0).map(function(_, i) {
            return [0, 1, 2].map(function(c) {
                var target = 0;
                for (var band = 0; band < 3; band++) if (i < fills[band]) target += colors[band][c];
                if (!old) return target;
                var retention = Math.pow(target > old[i][c] ? 0.4 : 0.65, scale);
                return target + (old[i][c] - target) * retention;
            });
        });
        return referenceOutput(referenceState.pixels, width, height);
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
