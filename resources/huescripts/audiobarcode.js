/*
  Q Light Controller Plus
  audiobarcode.js

  Copyright (c) QLC+ contributors

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

// Note: when Trigger=Flux, spawning is level-triggered (a new line is spawned
// every frame the flux value remains above the threshold, rate-limited by
// presetMinSpawnMs). Use Trigger=Onset for one-shot per articulation.

var testAlgo;

(
  function () {
    var algo = new Object;
    algo.apiVersion = 3;
    algo.name = "Audio Barcode";
    algo.author = "QLC+ contributors";
    algo.acceptColors = 5;
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
      var initial = !referenceState || referenceState.n !== n || referenceState.epoch !== epoch;
      if (initial) referenceState = {n: n, epoch: epoch, age: 0, history: []};
      var values = bank && bank.count ? bank.processed : [];
      var bounds = [0, Math.floor(values.length * 0.2), Math.floor(values.length * 0.5), values.length];
      var levels = [0, 1, 2].map(function(band) {
        var peak = 0;
        for (var i = bounds[band]; i < bounds[band + 1]; i++) peak = Math.max(peak, values[i]);
        return Math.min(1, peak * peak);
      });
      var colors = algo.referencePalette.split(",").map(function(hex) {
        return [1, 3, 5].map(function(offset) { return parseInt(hex.substr(offset, 2), 16) / 255; });
      });
      var color = [0, 1, 2].map(function(c) {
        return colors[0][c] * levels[0] + colors[1][c] * levels[1] + colors[2][c] * levels[2];
      });
      var scale = audio && audio.timing ? audio.timing.deltaSeconds * 60 : 1;
      var state = referenceState;
      if (initial || scale > 0) {
        state.age += initial ? 0 : scale;
        state.history.unshift({age: state.age, color: color});
        while (state.history.length > 1 && (state.age - state.history[state.history.length - 2].age) * 3 > n)
          state.history.pop();
      }
      // Sample an elapsed-time trail. At 60 Hz each sample occupies exactly three pixels.
      var pixels = new Array(n).fill(0).map(function(_, i) {
        var age = i / 3, entry = null;
        for (var j = 0; j < state.history.length; j++)
          if (state.age - state.history[j].age <= age + 1e-7) entry = state.history[j]; else break;
        if (!entry) return [0, 0, 0];
        var elapsed = state.age - entry.age;
        var nextAge = state.history.indexOf(entry) + 1;
        var end = nextAge < state.history.length ? state.age - state.history[nextAge].age : elapsed + 1;
        if (age >= end - 1e-7) return [0, 0, 0];
        return entry.color.map(function(c) { return c * Math.pow(0.97, elapsed); });
      });
      return referenceOutput(pixels, width, height);
    }

    var DEFAULT_GRADIENT = [
        { h: 0.042, s: 1.0, v: 1.0 },   // orange-red (0xFF4000)
        { h: 0, s: 0, v: 1.0 },          // white      (0xFFFFFF)
        { h: 0.542, s: 1.0, v: 1.0 }     // cyan-blue  (0x00C0FF)
    ];

    algo.presetLineWidth = 2;
    algo.properties.push(
      "name:presetLineWidth|type:range|display:Line Width (px)|" +
      "values:1,8|write:setLineWidth|read:getLineWidth");
    algo.setLineWidth = function(_v) { algo.presetLineWidth = parseInt(_v); };
    algo.getLineWidth = function() { return algo.presetLineWidth; };

    algo.presetScrollSpeed = 25;
    algo.properties.push(
      "name:presetScrollSpeed|type:range|display:Scroll Speed (px/s)|" +
      "values:1,100|write:setScrollSpeed|read:getScrollSpeed");
    algo.setScrollSpeed = function(_v) { algo.presetScrollSpeed = parseFloat(_v); };
    algo.getScrollSpeed = function() { return algo.presetScrollSpeed; };

    algo.presetDecayMs = 1500;
    algo.properties.push(
      "name:presetDecayMs|type:range|display:Brightness Decay (ms)|" +
      "values:100,8000|write:setDecayMs|read:getDecayMs");
    algo.setDecayMs = function(_v) { algo.presetDecayMs = parseFloat(_v); };
    algo.getDecayMs = function() { return algo.presetDecayMs; };

    algo.presetMinIntensity = 15;
    algo.properties.push(
      "name:presetMinIntensity|type:range|display:Min Intensity (%)|" +
      "values:0,100|write:setMinIntensity|read:getMinIntensity");
    algo.setMinIntensity = function(_v) { algo.presetMinIntensity = parseFloat(_v); };
    algo.getMinIntensity = function() { return algo.presetMinIntensity; };

    algo.presetTrigger = "Onset";
    algo.properties.push(
      "name:presetTrigger|type:list|display:Trigger Source|" +
      "values:Onset,Flux|write:setTrigger|read:getTrigger");
    algo.setTrigger = function(_v) { algo.presetTrigger = _v; };
    algo.getTrigger = function() { return algo.presetTrigger; };

    algo.presetFluxThreshold = 30;
    algo.properties.push(
      "name:presetFluxThreshold|type:range|display:Flux Threshold (%)|" +
      "values:0,100|write:setFluxThreshold|read:getFluxThreshold");
    algo.setFluxThreshold = function(_v) { algo.presetFluxThreshold = parseFloat(_v); };
    algo.getFluxThreshold = function() { return algo.presetFluxThreshold; };

    algo.presetHfcScale = 100;
    algo.properties.push(
      "name:presetHfcScale|type:range|display:HFC Scale (%=1.0)|" +
      "values:10,400|write:setHfcScale|read:getHfcScale");
    algo.setHfcScale = function(_v) { algo.presetHfcScale = parseFloat(_v); };
    algo.getHfcScale = function() { return algo.presetHfcScale; };

    algo.presetMaxLines = 40;
    algo.properties.push(
      "name:presetMaxLines|type:range|display:Max Active Lines|" +
      "values:4,200|write:setMaxLines|read:getMaxLines");
    algo.setMaxLines = function(_v) { algo.presetMaxLines = parseInt(_v); };
    algo.getMaxLines = function() { return algo.presetMaxLines; };

    algo.presetMinSpawnMs = 50;
    algo.properties.push(
      "name:presetMinSpawnMs|type:range|display:Min Spawn Interval (ms)|" +
      "values:0,500|write:setMinSpawnMs|read:getMinSpawnMs");
    algo.setMinSpawnMs = function(_v) { algo.presetMinSpawnMs = parseFloat(_v); };
    algo.getMinSpawnMs = function() { return algo.presetMinSpawnMs; };

    algo.presetAxis = "Horizontal";
    algo.properties.push(
      "name:presetAxis|type:list|display:Axis|" +
      "values:Horizontal,Vertical|write:setAxis|read:getAxis");
    algo.setAxis = function(_v) { algo.presetAxis = _v; };
    algo.getAxis = function() { return algo.presetAxis; };

    algo.lines = [];
    algo.lastW = -1;
    algo.lastH = -1;
    algo.spawnAccumMs = 0;

    function ensureState(width, height) {
      if (algo.lastW === width && algo.lastH === height) return;
      algo.lines = [];
      algo.spawnAccumMs = 0;
      algo.lastW = width;
      algo.lastH = height;
    }

    algo.rgbMapStepCount = function(_w, _h) { return 1; };
    algo.rgbMapSetColors = function(_raw) { };
    algo.rgbMapGetColors = function() { return []; };

    // Additive blend for HSV pixels in the Float32Array: read existing pixel,
    // keep the hue of the brighter contributor, add brightness values.
    function blendAddPixel(map, width, px, py, ch, cs, cv) {
        var idx = (py * width + px) * 3;
        var ev = map[idx + 2];
        if (ev <= 0) {
            map[idx] = ch;
            map[idx + 1] = cs;
            map[idx + 2] = Math.min(1, cv);
        } else {
            // Keep hue/sat of brighter contributor
            if (cv > ev) {
                map[idx] = ch;
                map[idx + 1] = cs;
            }
            map[idx + 2] = Math.min(1, ev + cv);
        }
    }

    algo.rgbMap = function(width, height, _rgb, _step, audio) {
      if (algo.presetMode === "Reference") return referenceMap(width, height, audio);
      var dt = audio.timing ? audio.timing.deltaSeconds : audio.dt * 60.0 / audio.bpm;
      ensureState(width, height);

      var horizontal = (algo.presetAxis === "Horizontal");
      var N = horizontal ? width : height;

      var triggered;
      var intensity;
      if (algo.presetTrigger === "Onset") {
        triggered = audio.onset;
        intensity = audio.onsetIntensity;
      } else {
        triggered = (audio.onsetIntensity > algo.presetFluxThreshold / 100.0);
        intensity = audio.onsetIntensity;
      }
      var threshold = algo.presetMinIntensity / 100.0;

      var count = algo.presetTrigger === "Onset" && audio.events ?
          audio.events.delta.onset : (triggered ? 1 : 0);
      algo.spawnAccumMs += dt * 1000;
      var canSpawn = algo.spawnAccumMs >= algo.presetMinSpawnMs;

      for (var spawn = 0; intensity >= threshold && canSpawn && spawn < Math.min(count, algo.presetMaxLines); spawn++) {
        var hfcScale = algo.presetHfcScale / 100.0;
        if (hfcScale < 0.001) hfcScale = 0.001;
        var t = audio.high / hfcScale;
        var gradient = (algo.colors && algo.colors.length > 0)
          ? algo.colors : DEFAULT_GRADIENT;
        var color = HSVUtil.gradientAt(gradient, t);
        algo.lines.push({
          position: N - 1,
          color: { h: color.h, s: color.s, v: color.v },
          brightness: intensity
        });
        algo.spawnAccumMs = 0;

        while (algo.lines.length > algo.presetMaxLines) {
          algo.lines.shift();
        }
      }

      var scrollPxPerSec = algo.presetScrollSpeed;
      var decayPerSec = 1.0 / (algo.presetDecayMs / 1000.0);
      for (var i = algo.lines.length - 1; i >= 0; i--) {
        var ln = algo.lines[i];
        ln.position -= scrollPxPerSec * dt;
        ln.brightness -= decayPerSec * dt;
        if (ln.brightness < 0) ln.brightness = 0;
        if (ln.position < -algo.presetLineWidth || ln.brightness <= 0.005) {
          algo.lines.splice(i, 1);
        }
      }

      var map = HSVUtil.createMap(width, height);
      var halfW = algo.presetLineWidth / 2;
      for (var li = 0; li < algo.lines.length; li++) {
        var lln = algo.lines[li];
        var x0 = Math.max(0, Math.floor(lln.position - halfW));
        var x1 = Math.min(N - 1, Math.ceil(lln.position + halfW));
        var bright = Math.max(0, Math.min(1, lln.brightness));
        var sv = lln.color.v * bright;
        var sh = lln.color.h;
        var ss = lln.color.s;
        for (var p = x0; p <= x1; p++) {
          if (horizontal) {
            for (var y = 0; y < height; y++) blendAddPixel(map, width, p, y, sh, ss, sv);
          } else {
            for (var x = 0; x < width; x++) blendAddPixel(map, width, x, p, sh, ss, sv);
          }
        }
      }

      return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
