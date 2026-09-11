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
    algo.properties.push("name:mode|type:list|display:Response|values:Artistic,Reference|write:setMode|read:getMode");
    algo.setMode = function(v) { algo.presetMode = v === "Reference" ? v : "Artistic"; };
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
        var values = HSVUtil.interpolate(bank && bank.count ? bank.novelty : [], n);
        var colors = algo.referencePalette.split(",").map(function(hex) {
            return [1, 3, 5].map(function(offset) { return parseInt(hex.substr(offset, 2), 16) / 255; });
        });
        var length = Math.max(n, 256), split = Math.floor(length / 2);
        var pixels = values.map(function(value, i) {
            var sample = Math.floor((length - 1) * (n > 1 ? i / (n - 1) : 0));
            var band = sample < split ? 0 : 1;
            var start = band ? split : 0, count = band ? length - split : split;
            var t = (sample - start) / Math.max(1, count - 1);
            var a = Math.pow(t, 1.5), b = Math.pow(1 - t, 1.5), mix = a / (a + b);
            return colors[band].map(function(channel, c) {
                return (channel + (colors[band + 1][c] - channel) * mix) * value;
            });
        });
        return referenceOutput(pixels, width, height);
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

    var REACTIVITY_SCALE = 5.0;
    var SAT_BASE = 0.9;
    var SAT_REACT_ADD = 0.3;
    var OFFSET_AMP = 2.0;

    algo.phase = 0;
    var energyState = { phase: 0 };

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        if (algo.presetMode === "Reference") return referenceMap(width, height, audio);
        var map = HSVUtil.createMap(width, height);
        if (!audio) return map;

        var dt = audio.dt;
        var speed = algo.presetSpeed;
        var reactivity01 = algo.presetReactivity;
        var reactivity = reactivity01 * REACTIVITY_SCALE;
        var rawLow = audio.low;
        // Asymmetric EMA smoothing (fast attack, slow decay)
        var smoothing = algo.presetSmoothing / 10.0;
        var riseAlpha = 0.5 * (1 - smoothing) + 0.05;
        var decayAlpha = 0.02 + 0.03 * (1 - smoothing);
        smoothLow += (rawLow > smoothLow ? riseAlpha : decayAlpha) * (rawLow - smoothLow);
        var lowPower = smoothLow;

        algo.phase = (energyState.phase = (energyState.phase + audio.dt * speed) % 1.0);

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
