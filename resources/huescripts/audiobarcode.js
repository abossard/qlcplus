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
    var referenceState = null;
    var scrollPlusState = null;
    var algo = new Object;
    algo.apiVersion = 3;
    algo.name = "Audio Barcode";
    algo.author = "QLC+ contributors";
    algo.acceptColors = 5;
    algo.usesAudio = true;
    algo.properties = new Array();

    algo.presetMode = "Artistic";
    algo.properties.push("name:mode|type:list|display:Response|values:Artistic,Reference,Scroll+|write:setMode|read:getMode");
    algo.setMode = function(v) {
      algo.presetMode = (v === "Reference" || v === "Scroll+") ? v : "Artistic";
      referenceState = null;
      scrollPlusState = null;
    };
    algo.getMode = function() { return algo.presetMode; };
    algo.referenceBlur = 1.5;
    algo.referenceMirror = "On";
    algo.referenceBrightness = 0.7;
    algo.referencePalette = "#ff0000,#00ff00,#0000ff";
    algo.referenceSpeed = 3;
    algo.referenceDecay = 0.97;
    algo.referenceThreshold = 0;
    algo.referenceRangeStart = 0;
    algo.referenceRangeEnd = 1;
    algo.referenceFlip = "Off";
    algo.referenceBackgroundMode = "Off";
    algo.referenceBackgroundColor = "#000000";
    algo.referenceBackgroundBrightness = 1;
    algo.scrollPlusSpeed = 0.5;
    algo.scrollPlusDecay = 0.5;
    algo.scrollPlusThreshold = 0.1;
    algo.properties.push("name:referenceBlur|type:float|display:Reference Blur|write:setReferenceBlur|read:getReferenceBlur");
    algo.properties.push("name:referenceMirror|type:list|display:Reference Mirror|values:Off,On|write:setReferenceMirror|read:getReferenceMirror");
    algo.properties.push("name:referenceBrightness|type:float|display:Reference Brightness|write:setReferenceBrightness|read:getReferenceBrightness");
    algo.properties.push("name:referencePalette|type:string|display:Reference RGB Palette|write:setReferencePalette|read:getReferencePalette");
    algo.properties.push("name:referenceSpeed|type:range|display:Reference Speed (px/tick)|values:1,10|write:setReferenceSpeed|read:getReferenceSpeed");
    algo.properties.push("name:referenceDecay|type:float|values:0.8,1|display:Reference Decay (0.8..1)|write:setReferenceDecay|read:getReferenceDecay");
    algo.properties.push("name:referenceThreshold|type:float|values:0,1|display:Reference Threshold (0..1)|write:setReferenceThreshold|read:getReferenceThreshold");
    algo.properties.push("name:referenceRangeStart|type:float|values:0,1|display:Reference Range Start (0..1)|write:setReferenceRangeStart|read:getReferenceRangeStart");
    algo.properties.push("name:referenceRangeEnd|type:float|values:0,1|display:Reference Range End (0..1)|write:setReferenceRangeEnd|read:getReferenceRangeEnd");
    algo.properties.push("name:referenceFlip|type:list|display:Reference Flip|values:Off,On|write:setReferenceFlip|read:getReferenceFlip");
    algo.properties.push("name:referenceBackgroundMode|type:list|display:Reference Background Mode|values:Off,Additive|write:setReferenceBackgroundMode|read:getReferenceBackgroundMode");
    algo.properties.push("name:referenceBackgroundColor|type:string|display:Reference Background Color (#rrggbb)|write:setReferenceBackgroundColor|read:getReferenceBackgroundColor");
    algo.properties.push("name:referenceBackgroundBrightness|type:float|values:0,1|display:Reference Background Brightness (0..1)|write:setReferenceBackgroundBrightness|read:getReferenceBackgroundBrightness");
    algo.properties.push("name:scrollPlusSpeed|type:float|values:0.01,2|display:Scroll+ Speed (strips/s)|write:setScrollPlusSpeed|read:getScrollPlusSpeed");
    algo.properties.push("name:scrollPlusDecay|type:float|values:0,2|display:Scroll+ Decay (linear/s)|write:setScrollPlusDecay|read:getScrollPlusDecay");
    algo.properties.push("name:scrollPlusThreshold|type:float|values:0,1|display:Scroll+ Threshold|write:setScrollPlusThreshold|read:getScrollPlusThreshold");
    algo.setReferenceBlur = function(v) { algo.referenceBlur = Math.max(0, Math.min(10, parseFloat(v) || 0)); };
    algo.getReferenceBlur = function() { return algo.referenceBlur; };
    algo.setReferenceMirror = function(v) { algo.referenceMirror = v === "On" ? "On" : "Off"; };
    algo.getReferenceMirror = function() { return algo.referenceMirror; };
    algo.setReferenceBrightness = function(v) { algo.referenceBrightness = HSVUtil.clamp01(parseFloat(v) || 0); };
    algo.getReferenceBrightness = function() { return algo.referenceBrightness; };
    algo.setReferencePalette = function(v) { if (/^#[0-9a-f]{6},#[0-9a-f]{6},#[0-9a-f]{6}$/i.test(v)) algo.referencePalette = v; };
    algo.getReferencePalette = function() { return algo.referencePalette; };
    algo.setReferenceSpeed = function(v) {
      var value = parseInt(v, 10);
      if (!isFinite(value)) value = 3;
      algo.referenceSpeed = Math.max(1, Math.min(10, value));
    };
    algo.getReferenceSpeed = function() { return algo.referenceSpeed; };
    algo.setReferenceDecay = function(v) {
      var value = parseFloat(v);
      if (!isFinite(value)) value = 0.97;
      algo.referenceDecay = Math.max(0.8, Math.min(1, value));
    };
    algo.getReferenceDecay = function() { return algo.referenceDecay; };
    algo.setReferenceThreshold = function(v) {
      algo.referenceThreshold = HSVUtil.clamp01(parseFloat(v) || 0);
    };
    algo.getReferenceThreshold = function() { return algo.referenceThreshold; };
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
    algo.setScrollPlusSpeed = function(v) { algo.scrollPlusSpeed = Math.max(0.01, Math.min(2, parseFloat(v) || 0.5)); };
    algo.getScrollPlusSpeed = function() { return algo.scrollPlusSpeed; };
    algo.setScrollPlusDecay = function(v) { algo.scrollPlusDecay = Math.max(0, Math.min(2, parseFloat(v) || 0)); };
    algo.getScrollPlusDecay = function() { return algo.scrollPlusDecay; };
    algo.setScrollPlusThreshold = function(v) { algo.scrollPlusThreshold = HSVUtil.clamp01(parseFloat(v) || 0); };
    algo.getScrollPlusThreshold = function() { return algo.scrollPlusThreshold; };

    function parseReferenceColors() {
      return algo.referencePalette.split(",").map(function(hex) {
        return HSVUtil.parseHexRgb(hex);
      });
    }

    function rangedProcessed(audio) {
      var values = audio && audio.banks && audio.banks.full && audio.banks.full.processed
        ? audio.banks.full.processed.slice() : [];
      if (!values.length) return values;
      var start = Math.min(algo.referenceRangeStart, algo.referenceRangeEnd);
      var end = Math.max(algo.referenceRangeStart, algo.referenceRangeEnd);
      var from = Math.max(0, Math.floor(start * values.length));
      var to = Math.min(values.length, Math.ceil(end * values.length));
      return to > from ? values.slice(from, to) : [];
    }

    function bandLevels(values, threshold) {
      var bounds = [0, Math.floor(values.length * 0.2), Math.floor(values.length * 0.5), values.length];
      var levels = [0, 0, 0];
      for (var band = 0; band < 3; band++) {
        var peak = 0;
        for (var i = bounds[band]; i < bounds[band + 1]; i++) peak = Math.max(peak, values[i] || 0);
        levels[band] = HSVUtil.clamp01(peak * peak);
      }
      if (levels[0] < threshold / 10) levels[0] = 0;
      if (levels[1] < threshold / 8) levels[1] = 0;
      if (levels[2] < threshold / 7) levels[2] = 0;
      return levels;
    }

    function mixColor(levels, colors) {
      return [
        colors[0][0] * levels[0] + colors[1][0] * levels[1] + colors[2][0] * levels[2],
        colors[0][1] * levels[0] + colors[1][1] * levels[1] + colors[2][1] * levels[2],
        colors[0][2] * levels[0] + colors[1][2] * levels[1] + colors[2][2] * levels[2]
      ];
    }

    function modeColor(audio, threshold) {
      var colors = parseReferenceColors();
      var values = rangedProcessed(audio || {});
      var levels = bandLevels(values, threshold);
      return { levels: levels, rgb: mixColor(levels, colors) };
    }

    function rgbToHsv(r, g, b) {
      r = HSVUtil.clamp01(r);
      g = HSVUtil.clamp01(g);
      b = HSVUtil.clamp01(b);
      var max = Math.max(r, g, b);
      var min = Math.min(r, g, b);
      var delta = max - min;
      var h = 0;
      if (delta) h = (max === r ? (g - b) / delta : max === g ? 2 + (b - r) / delta : 4 + (r - g) / delta) / 6;
      return { h: HSVUtil.mod1(h), s: max ? delta / max : 0, v: max };
    }

    function scrollPlusMap(width, height, audio) {
      var map = HSVUtil.createMap(width, height);
      var n = width * height;
      var previousIdentity = scrollPlusState ? scrollPlusState.identityKey : "";
      var identityKey = HSVUtil.audioIdentityKey(audio, previousIdentity);
      if (!scrollPlusState || scrollPlusState.n !== n || scrollPlusState.identityKey !== identityKey)
        scrollPlusState = {
          n: n,
          identityKey: identityKey,
          offset: 0,
          strip: new Array(n).fill(0).map(function() { return [0, 0, 0]; })
        };
      var state = scrollPlusState;
      var dt = HSVUtil.audioSeconds(audio);
      var mode = modeColor(audio || {}, algo.scrollPlusThreshold);
      state.offset += algo.scrollPlusSpeed * n * Math.max(0, dt);
      var shift = Math.floor(state.offset);
      state.offset -= shift;
      var decay = dt > 0 ? Math.max(0, 1 - algo.scrollPlusDecay * dt * 3) : 1;
      for (var i = 0; i < n; i++) {
        var p = state.strip[i];
        p[0] *= decay;
        p[1] *= decay;
        p[2] *= decay;
      }
      shift = Math.min(shift, n);
      for (var i = 0; i < shift; i++) {
        state.strip.pop();
        state.strip.unshift(mode.rgb.slice());
      }
      for (var i = 0; i < n; i++) {
        var p = state.strip[i];
        var hsv = rgbToHsv(p[0], p[1], p[2]);
        map[i * 3] = hsv.h;
        map[i * 3 + 1] = hsv.s;
        map[i * 3 + 2] = hsv.v;
      }
      return HSVUtil.applyStripTransforms(map, width, height, {
        flip: algo.referenceFlip,
        mirror: algo.referenceMirror,
        backgroundMode: algo.referenceBackgroundMode,
        backgroundColor: algo.referenceBackgroundColor,
        backgroundBrightness: algo.referenceBackgroundBrightness,
        brightness: algo.referenceBrightness,
        blur: algo.referenceBlur
      });
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
      var previousIdentity = referenceState ? referenceState.identityKey : "";
      var identityKey = HSVUtil.audioIdentityKey(audio, previousIdentity);
      if (!referenceState || referenceState.n !== n || referenceState.identityKey !== identityKey)
        referenceState = {
          n: n,
          identityKey: identityKey,
          offset: 0,
          seeded: false,
          strip: new Array(n).fill(0).map(function() { return [0, 0, 0]; })
        };
      var state = referenceState;
      var dt = HSVUtil.audioSeconds(audio);
      var ticks = Math.max(0, dt * 60);
      var mode = modeColor(audio || {}, algo.referenceThreshold);
      if (!state.seeded) {
        var initialSpan = Math.min(n, Math.max(1, Math.floor(algo.referenceSpeed)));
        for (var s = 0; s < initialSpan; s++) state.strip[s] = mode.rgb.slice();
        state.seeded = true;
      }
      state.offset += algo.referenceSpeed * ticks;
      var shift = Math.min(n, Math.max(0, Math.floor(state.offset)));
      state.offset -= shift;
      if (shift > 0) {
        for (var i = n - 1; i >= shift; i--) state.strip[i] = state.strip[i - shift].slice();
      }
      if (ticks > 0) {
        var decay = Math.pow(algo.referenceDecay, ticks);
        for (var i = 0; i < n; i++) {
          state.strip[i][0] *= decay;
          state.strip[i][1] *= decay;
          state.strip[i][2] *= decay;
        }
      }
      if (shift > 0) {
        for (var i = 0; i < shift; i++) state.strip[i] = mode.rgb.slice();
      }
      return referenceOutput(state.strip, width, height);
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
      if (algo.presetMode === "Scroll+")
        return scrollPlusMap(width, height, audio);
      if (algo.presetMode === "Reference")
        return referenceMap(width, height, audio);
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
