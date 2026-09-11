/*
  Q Light Controller Plus
  audiostrobe.js

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
    algo.name = "Audio Strobe";
    algo.author = "Ported from LedFx";
    algo.acceptColors = 3;
    algo.usesAudio = true;
    algo.properties = new Array();

    algo.presetMode = "Artistic";
    algo.properties.push("name:mode|type:list|display:Response|values:Artistic,Reference|write:setMode|read:getMode");
    algo.setMode = function(v) { algo.presetMode = v === "Reference" ? v : "Artistic"; };
    algo.getMode = function() { return algo.presetMode; };
    algo.referenceClock = "Detected";
    algo.properties.push("name:referenceClock|type:list|display:Reference Clock|values:Detected,Extrapolated|write:setReferenceClock|read:getReferenceClock");
    algo.setReferenceClock = function(v) { algo.referenceClock = v === "Extrapolated" ? v : "Detected"; };
    algo.getReferenceClock = function() { return algo.referenceClock; };
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
        var n = width * height;
        var phase = audio && audio.tempo ? audio.tempo.beatPhase : 0;
        var bar = audio && audio.tempo ? audio.tempo.barPhase : 0;
        var colors = algo.referencePalette.split(",").map(function(hex) {
            return [1, 3, 5].map(function(offset) { return parseInt(hex.substr(offset, 2), 16) / 255; });
        });
        var length = Math.max(n, 256), split = Math.floor(length / 2);
        var sample = Math.floor((length - 1) * HSVUtil.clamp01(bar));
        var band = sample < split ? 0 : 1;
        var start = band ? split : 0, count = band ? length - split : split;
        var t = (sample - start) / Math.max(1, count - 1);
        var a = Math.pow(t, 1.5), b = Math.pow(1 - t, 1.5), mix = a / (a + b);
        // Two phase pulses per beat. The reference applies the pulse envelope twice.
        var amplitude = Math.pow(HSVUtil.mod1(-phase * 2), 3) * Math.pow(1 - phase, 4);
        if (!audio || !audio.tempo || (algo.referenceClock === "Detected" && !audio.tempo.valid)) amplitude = 0;
        var color = colors[band].map(function(channel, c) {
            return (channel + (colors[band + 1][c] - channel) * mix) * amplitude;
        });
        return referenceOutput(new Array(n).fill(color), width, height);
    }

    var DOMINANT_TINT = 0.5;
    var DEFAULT_HSV_STOPS = [
        { h: 0.000, s: 1.0, v: 1.0 },
        { h: 0.884, s: 1.0, v: 1.0 },
        { h: 0.667, s: 1.0, v: 1.0 }
    ];
    var BASS_REFRACTORY_MS = 200;

    algo.color_step = 0.0625;
    algo.bass_strobe_decay_rate = 0.5;
    algo.strobe_width = 10;
    algo.strobe_decay_rate = 0.5;
    algo.color_shift_delay = 1.0;

    algo.properties.push("name:color_step|type:float|display:Color Step|write:setColorStep|read:getColorStep");
    algo.properties.push("name:bass_strobe_decay_rate|type:float|display:Bass Strobe Decay Rate|write:setBassStrobeDecayRate|read:getBassStrobeDecayRate");
    algo.properties.push("name:strobe_width|type:range|display:Strobe Width|values:0,1000|write:setStrobeWidth|read:getStrobeWidth");
    algo.properties.push("name:strobe_decay_rate|type:float|display:Strobe Decay Rate|write:setStrobeDecayRate|read:getStrobeDecayRate");
    algo.properties.push("name:color_shift_delay|type:float|display:Color Shift Delay|write:setColorShiftDelay|read:getColorShiftDelay");

    algo.presetBassTrigger = "Volume Beat";
    algo.properties.push("name:bass_trigger|type:list|display:Bass Trigger|values:Volume Beat,Tempo Beat,Onset,Kick|write:setBassTrigger|read:getBassTrigger");
    algo.setBassTrigger = function(v) {
      var valid = ["Volume Beat", "Tempo Beat", "Onset", "Kick"];
      algo.presetBassTrigger = valid.indexOf(v) >= 0 ? v : "Volume Beat";
    };
    algo.getBassTrigger = function() { return algo.presetBassTrigger; };

    algo.setColorStep = function(v) { algo.color_step = parseFloat(v); };
    algo.getColorStep = function() { return algo.color_step; };
    algo.setBassStrobeDecayRate = function(v) { algo.bass_strobe_decay_rate = parseFloat(v); };
    algo.getBassStrobeDecayRate = function() { return algo.bass_strobe_decay_rate; };
    algo.setStrobeWidth = function(v) { algo.strobe_width = parseFloat(v); };
    algo.getStrobeWidth = function() { return algo.strobe_width; };
    algo.setStrobeDecayRate = function(v) { algo.strobe_decay_rate = parseFloat(v); };
    algo.getStrobeDecayRate = function() { return algo.strobe_decay_rate; };
    algo.setColorShiftDelay = function(v) { algo.color_shift_delay = parseFloat(v); };
    algo.getColorShiftDelay = function() { return algo.color_shift_delay; };

    var strobeOverlay = [];
    var bassStrobeOverlay = [];
    var onsetsQueued = 0;
    var elapsedMs = 0;
    var lastColorShiftMs = 0;
    var lastStrobeMs = 0;
    var lastBassStrobeMs = 0;
    var colorIdx = 0;
    var bassStrobeColor = {h: 0, s: 0, v: 0};
    var lastWidth = 0;

    function zeroStrip(n) {
        var out = new Array(n);
        for (var i = 0; i < n; i++) out[i] = {h: 0, s: 0, v: 0};
        return out;
    }

    function gradientStops() {
        return (algo.colors && algo.colors.length > 0)
            ? algo.colors : DEFAULT_HSV_STOPS;
    }

    function ensure(width) {
        if (lastWidth === width && strobeOverlay.length === width) return;
        strobeOverlay = zeroStrip(width);
        bassStrobeOverlay = zeroStrip(width);
        onsetsQueued = 0;
        colorIdx = 0;
        bassStrobeColor = HSVUtil.gradientAt(gradientStops(), colorIdx);
        lastWidth = width;
    }

    function scaleInPlace(strip, factor) {
        for (var i = 0; i < strip.length; i++)
            strip[i].v *= factor;
    }

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step, audio) {
        if (algo.presetMode === "Reference") return referenceMap(width, height, audio);
        ensure(width);
        var map = HSVUtil.createMap(width, height);
        if (!audio) return map;

        var seconds = audio.timing ? audio.timing.deltaSeconds : audio.dt * 60 / audio.bpm;
        var frameScale = audio.timing ? seconds * 50 : 1;
        elapsedMs += seconds * 1000;

        if (elapsedMs - lastColorShiftMs > Math.max(0, Math.min(1, parseFloat(algo.color_shift_delay))) * 1000.0) {
            colorIdx = (colorIdx + Math.max(0, Math.min(0.25, parseFloat(algo.color_step)))) % 1.0;
            bassStrobeColor = HSVUtil.gradientAt(gradientStops(), colorIdx);
            lastColorShiftMs = elapsedMs;
        }

        var bassDecay = 1.0 - Math.max(0, Math.min(1, parseFloat(algo.bass_strobe_decay_rate)));
        var bassTriggerFired = false;
        if (algo.presetBassTrigger === "Tempo Beat") bassTriggerFired = audio.beatFired;
        else if (algo.presetBassTrigger === "Onset") bassTriggerFired = audio.onset;
        else if (algo.presetBassTrigger === "Kick") bassTriggerFired = audio.version >= 6 ? audio.kickFired : audio.beatFired;
        else bassTriggerFired = audio.version >= 6 ? audio.kickFired : audio.onset;
        if (bassTriggerFired && elapsedMs - lastBassStrobeMs > BASS_REFRACTORY_MS && bassDecay) {
            for (var b = 0; b < width; b++)
                bassStrobeOverlay[b] = {h: bassStrobeColor.h, s: bassStrobeColor.s, v: bassStrobeColor.v};
            lastBassStrobeMs = elapsedMs;
        }

        if (audio.events || (audio.onset && elapsedMs - lastStrobeMs > 0)) {
            onsetsQueued += audio.events ? audio.events.delta.onset : 1;
            lastStrobeMs = elapsedMs;
        }

        while (onsetsQueued > 0) {
            onsetsQueued--;
            var strobeWidth = Math.min(Math.max(0, Math.min(1000, algo.strobe_width)), width);
            var lengthDiff = width - strobeWidth;
            var position = lengthDiff === 0 ? 0 : Math.floor(Math.random() * (width - strobeWidth));
            var bandColor = (algo.colors && algo.colors.length >= 3) ? algo.colors[0] : {h: 0, s: 0, v: 1};
            var domIdx = (audio.mid > audio.low && audio.mid >= audio.high) ? 1 : (audio.high > audio.low) ? 2 : 0;
            var domPower = Math.max(audio.low, audio.mid, audio.high);
            var domHsv = (domPower >= 0.05 && algo.colors && algo.colors.length >= 3) ? algo.colors[domIdx] : bandColor;
            var t2 = DOMINANT_TINT;
            var scol = {h: bandColor.h + (domHsv.h - bandColor.h) * t2, s: bandColor.s + (domHsv.s - bandColor.s) * t2, v: bandColor.v + (domHsv.v - bandColor.v) * t2};
            for (var s = position; s < position + strobeWidth; s++)
                strobeOverlay[s] = {h: scol.h, s: scol.s, v: scol.v};
        }

        // Combine overlays (additive in value)
        for (var x = 0; x < width; x++) {
            var bv = bassStrobeOverlay[x].v;
            var sv = strobeOverlay[x].v;
            var totalV = bv + sv;
            if (totalV > 0.001) {
                var h, sat;
                if (sv > bv) {
                    h = strobeOverlay[x].h; sat = strobeOverlay[x].s;
                    var t = bv / totalV;
                    var dh = bassStrobeOverlay[x].h - h;
                    if (dh > 0.5) dh -= 1; else if (dh < -0.5) dh += 1;
                    h += t * dh;
                    h = h - Math.floor(h);
                    sat = sat * (1 - t) + bassStrobeOverlay[x].s * t;
                } else {
                    h = bassStrobeOverlay[x].h; sat = bassStrobeOverlay[x].s;
                    if (sv > 0.001) {
                        var t = sv / totalV;
                        var dh = strobeOverlay[x].h - h;
                        if (dh > 0.5) dh -= 1; else if (dh < -0.5) dh += 1;
                        h += t * dh;
                        h = h - Math.floor(h);
                        sat = sat * (1 - t) + strobeOverlay[x].s * t;
                    }
                }
                var v = Math.min(1, totalV);
                for (var y = 0; y < height; y++) {
                    var i3 = (y * width + x) * 3;
                    map[i3] = h; map[i3 + 1] = sat; map[i3 + 2] = v;
                }
            }
        }

        scaleInPlace(strobeOverlay, Math.pow(1.0 - Math.max(0, Math.min(1, parseFloat(algo.strobe_decay_rate))), frameScale));
        scaleInPlace(bassStrobeOverlay, Math.pow(bassDecay, frameScale));

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
