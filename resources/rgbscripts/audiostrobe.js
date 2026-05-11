/*
  Q Light Controller Plus
  audiostrobe.js

  Copyright (c) QLC+ contributors
  Ported from LedFx "Real Strobe" effect (MIT License)

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

    function clamp(v, lo, hi) { if (isNaN(v)) return lo; return Math.max(lo, Math.min(hi, v)); }
    algo.setColorStep = function(v) { algo.color_step = clamp(parseFloat(v), 0, 0.25); };
    algo.getColorStep = function() { return algo.color_step; };
    algo.setBassStrobeDecayRate = function(v) { algo.bass_strobe_decay_rate = clamp(parseFloat(v), 0, 1); };
    algo.getBassStrobeDecayRate = function() { return algo.bass_strobe_decay_rate; };
    algo.setStrobeWidth = function(v) { algo.strobe_width = clamp(parseInt(v), 0, 1000); };
    algo.getStrobeWidth = function() { return algo.strobe_width; };
    algo.setStrobeDecayRate = function(v) { algo.strobe_decay_rate = clamp(parseFloat(v), 0, 1); };
    algo.getStrobeDecayRate = function() { return algo.strobe_decay_rate; };
    algo.setColorShiftDelay = function(v) { algo.color_shift_delay = clamp(parseFloat(v), 0, 1); };
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
        return (algo.gradientColors && algo.gradientColors.length > 0)
            ? algo.gradientColors : DEFAULT_HSV_STOPS;
    }

    function ensure(width) {
        if (lastWidth === width && strobeOverlay.length === width) return;
        strobeOverlay = zeroStrip(width);
        bassStrobeOverlay = zeroStrip(width);
        onsetsQueued = 0;
        colorIdx = 0;
        bassStrobeColor = RGBUtil.gradientAt(gradientStops(), colorIdx);
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
        ensure(width);
        var map = RGBUtil.createMap(width, height);
        if (!audio) return map;

        var dtMs = audio.timing.consumerDtMs > 0 ? audio.timing.consumerDtMs : 40;
        elapsedMs += dtMs;

        if (elapsedMs - lastColorShiftMs > clamp(parseFloat(algo.color_shift_delay), 0, 1) * 1000.0) {
            colorIdx = (colorIdx + clamp(parseFloat(algo.color_step), 0, 0.25)) % 1.0;
            bassStrobeColor = RGBUtil.gradientAt(gradientStops(), colorIdx);
            lastColorShiftMs = elapsedMs;
        }

        var bassDecay = 1.0 - clamp(parseFloat(algo.bass_strobe_decay_rate), 0, 1);
        var bassTriggerFired = false;
        if (algo.presetBassTrigger === "Tempo Beat") bassTriggerFired = audio.beat.fired;
        else if (algo.presetBassTrigger === "Onset") bassTriggerFired = audio.onset.fired;
        else if (algo.presetBassTrigger === "Kick") bassTriggerFired = audio.beat.kick;
        else bassTriggerFired = audio.volume.fired;
        if (bassTriggerFired && elapsedMs - lastBassStrobeMs > BASS_REFRACTORY_MS && bassDecay) {
            for (var b = 0; b < width; b++)
                bassStrobeOverlay[b] = {h: bassStrobeColor.h, s: bassStrobeColor.s, v: bassStrobeColor.v};
            lastBassStrobeMs = elapsedMs;
        }

        if (audio.onset.fired && elapsedMs - lastStrobeMs > 0) {
            onsetsQueued++;
            lastStrobeMs = elapsedMs;
        }

        if (onsetsQueued > 0) {
            onsetsQueued--;
            var strobeWidth = Math.min(clamp(parseInt(algo.strobe_width), 0, 1000), width);
            var lengthDiff = width - strobeWidth;
            var position = lengthDiff === 0 ? 0 : Math.floor(Math.random() * (width - strobeWidth));
            var bandColor = AudioColors.bands(algo)[0];
            var domHsv = AudioColors.dominantColor(algo, audio, bandColor, 0.05);
            var scol = AudioColors.blend(bandColor, domHsv, DOMINANT_TINT);
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

        scaleInPlace(strobeOverlay, 1.0 - clamp(parseFloat(algo.strobe_decay_rate), 0, 1));
        scaleInPlace(bassStrobeOverlay, bassDecay);

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
