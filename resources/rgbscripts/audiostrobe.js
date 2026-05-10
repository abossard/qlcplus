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
    var DANCEFLOOR_GRADIENT = [0xFF0000, 0xFF00B2, 0x0000FF];

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
    var bassStrobeColor = [0, 0, 0];
    var lastWidth = 0;

    function colorArray(packed) {
        packed = packed & 0xFFFFFF;
        return [(packed >> 16) & 0xFF, (packed >> 8) & 0xFF, packed & 0xFF];
    }

    function zeroStrip(n) {
        var out = new Array(n);
        for (var i = 0; i < n; i++) out[i] = [0, 0, 0];
        return out;
    }

    function gradientStops() {
        return (algo.gradientColors && algo.gradientColors.length > 0) ? algo.gradientColors : DANCEFLOOR_GRADIENT;
    }

    function ensure(width) {
        if (lastWidth === width && strobeOverlay.length === width) return;
        strobeOverlay = zeroStrip(width);
        bassStrobeOverlay = zeroStrip(width);
        onsetsQueued = 0;
        colorIdx = 0;
        bassStrobeColor = colorArray(RGBUtil.gradientColorAt(gradientStops(), colorIdx));
        lastWidth = width;
    }

    function scaleInPlace(strip, factor) {
        for (var i = 0; i < strip.length; i++) {
            strip[i][0] *= factor;
            strip[i][1] *= factor;
            strip[i][2] *= factor;
        }
    }

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { algo.gradientColors = RGBUtil.buildGradientColors(rawColors); };
    algo.rgbMapGetColors = function() { return gradientStops().slice(); };

    algo.rgbMap = function(width, height, rgb, step, audio) {
        ensure(width);
        var map = RGBUtil.createMap(width, height);
        if (!audio) return map;

        var dtMs = audio.timing.consumerDtMs > 0 ? audio.timing.consumerDtMs : 40;
        elapsedMs += dtMs;

        if (elapsedMs - lastColorShiftMs > clamp(parseFloat(algo.color_shift_delay), 0, 1) * 1000.0) {
            colorIdx = (colorIdx + clamp(parseFloat(algo.color_step), 0, 0.25)) % 1.0;
            bassStrobeColor = colorArray(RGBUtil.gradientColorAt(gradientStops(), colorIdx));
            lastColorShiftMs = elapsedMs;
        }

        var bassDecay = 1.0 - clamp(parseFloat(algo.bass_strobe_decay_rate), 0, 1);
        var bassTriggerFired = false;
        if (algo.presetBassTrigger === "Tempo Beat") bassTriggerFired = audio.beat.fired;
        else if (algo.presetBassTrigger === "Onset") bassTriggerFired = audio.onset.fired;
        else if (algo.presetBassTrigger === "Kick") bassTriggerFired = audio.beat.kick;
        else bassTriggerFired = audio.volume.fired;
        if (bassTriggerFired && elapsedMs - lastBassStrobeMs > 200 && bassDecay) {
            for (var b = 0; b < width; b++)
                bassStrobeOverlay[b] = [bassStrobeColor[0], bassStrobeColor[1], bassStrobeColor[2]];
            lastBassStrobeMs = elapsedMs;
        }

        if (audio.onset.fired && elapsedMs - lastStrobeMs > 0) {
            onsetsQueued++;
            lastStrobeMs = elapsedMs;
        }

        var pixels = new Array(width);
        for (var i = 0; i < width; i++)
            pixels[i] = [bassStrobeOverlay[i][0], bassStrobeOverlay[i][1], bassStrobeOverlay[i][2]];

        if (onsetsQueued > 0) {
            onsetsQueued--;
            var strobeWidth = Math.min(clamp(parseInt(algo.strobe_width), 0, 1000), width);
            var lengthDiff = width - strobeWidth;
            var position = lengthDiff === 0 ? 0 : Math.floor(Math.random() * (width - strobeWidth));
            var strobeColorPacked = AudioColors.bands(algo)[0] | 0;
            var dominantPacked = AudioColors.dominantColor(algo, audio, strobeColorPacked, 0.05);
            var blended = AudioColors.blendPacked(strobeColorPacked, dominantPacked, DOMINANT_TINT);
            var scol = colorArray(blended);
            for (var s = position; s < position + strobeWidth; s++)
                strobeOverlay[s] = [scol[0], scol[1], scol[2]];
        }

        for (var x = 0; x < width; x++) {
            pixels[x][0] += strobeOverlay[x][0];
            pixels[x][1] += strobeOverlay[x][1];
            pixels[x][2] += strobeOverlay[x][2];
        }

        scaleInPlace(strobeOverlay, 1.0 - clamp(parseFloat(algo.strobe_decay_rate), 0, 1));
        scaleInPlace(bassStrobeOverlay, bassDecay);

        for (var y = 0; y < height; y++)
            for (var px = 0; px < width; px++)
                map[y][px] = RGBUtil.rgb(pixels[px][0], pixels[px][1], pixels[px][2]);
        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
