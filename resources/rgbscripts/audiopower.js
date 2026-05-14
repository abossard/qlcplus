/*
  Q Light Controller Plus
  audiopower.js — 5-segment power meter (Audio API v2)

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
    algo.name = "Audio Power";
    algo.author = "QLC+ contributors";
    algo.acceptColors = 5;
    algo.usesAudio = true;
    algo.properties = new Array();

    var DEFAULT_GRADIENT = [
        {h: 0.0,   s: 1.0, v: 1.0},
        {h: 0.078, s: 1.0, v: 1.0},
        {h: 0.131, s: 1.0, v: 1.0},
        {h: 0.333, s: 1.0, v: 1.0},
        {h: 0.611, s: 0.75, v: 1.0}
    ];

    algo.blur = 0.0;
    algo.bass_decay_rate = 0.05;
    algo.sparks_decay_rate = 0.15;

    algo.properties.push("name:blur|type:float|display:Blur|write:setBlur|read:getBlur");
    algo.properties.push("name:bass_decay_rate|type:float|display:Bass Decay Rate|write:setBassDecayRate|read:getBassDecayRate");
    algo.properties.push("name:sparks_decay_rate|type:float|display:Sparks Decay Rate|write:setSparksDecayRate|read:getSparksDecayRate");

    algo.setBlur = function(v) { algo.blur = parseFloat(v); };
    algo.getBlur = function() { return algo.blur; };
    algo.setBassDecayRate = function(v) { algo.bass_decay_rate = parseFloat(v); };
    algo.getBassDecayRate = function() { return algo.bass_decay_rate; };
    algo.setSparksDecayRate = function(v) { algo.sparks_decay_rate = parseFloat(v); };
    algo.getSparksDecayRate = function() { return algo.sparks_decay_rate; };

    algo.presetSmoothing = 5;
    algo.properties.push("name:presetSmoothing|type:range|display:Smoothing|values:1,10|write:setSmoothing|read:getSmoothing");
    algo.setSmoothing = function(v) { algo.presetSmoothing = parseInt(v); };
    algo.getSmoothing = function() { return algo.presetSmoothing; };

    var smoothBands = [0, 0, 0, 0, 0];

    // Bar-level build-up / release state
    var barEnergy = 0;
    var peakEnergy = 0;
    var releaseFlash = 0;

    var sparksV = null;
    var bassV = null;
    var sparkColor = {h: 0.884, s: 1.0, v: 1.0};
    var lastW = 0;


    function ensure(width) {
        if (lastW === width && sparksV) return;
        lastW = width;
        sparksV = new Array(width);
        bassV = new Array(width);
        for (var i = 0; i < width; i++) { sparksV[i] = 0; bassV[i] = 0; }
    }

    function scaleInPlace(arr, factor) {
        for (var i = 0; i < arr.length; i++) arr[i] *= factor;
    }

    function gradientStops() {
        return (algo.colors && algo.colors.length >= 3) ? algo.colors : DEFAULT_GRADIENT;
    }

    function boxBlur(strip, radius) {
        if (radius < 0.5) return strip;
        var n = strip.length;
        var r = Math.max(1, Math.round(radius));
        var out = new Array(n);
        for (var i = 0; i < n; i++) {
            var bh = 0, bs = 0, bv = 0, c = 0;
            for (var k = -r; k <= r; k++) {
                var idx = i + k;
                if (idx < 0 || idx >= n) continue;
                bh += strip[idx].h; bs += strip[idx].s; bv += strip[idx].v; c++;
            }
            out[i] = {h: bh / c, s: bs / c, v: bv / c};
        }
        return out;
    }

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step, audio) {
        ensure(width);
        var map = HSVUtil.createMap(width, height);
        if (!audio) return map;

        scaleInPlace(sparksV, 1.0 - Math.max(0, Math.min(1, algo.sparks_decay_rate)));
        scaleInPlace(bassV, 1.0 - Math.max(0, Math.min(1, algo.bass_decay_rate)));

        // Sparks on onset
        if (audio.onset) {
            var numSparks = Math.floor(width / 20);
            var stops = gradientStops();
            sparkColor = stops[stops.length - 1];
            for (var s = 0; s < numSparks; s++) {
                var sx = Math.floor(Math.random() * width);
                sparksV[sx] = sparkColor.v;
            }
        }

        // Bass fill from left
        var bass = Math.max(0, Math.min(1, audio.low));
        var bassIdx = Math.floor(bass * width);
        var bassHsv = HSVUtil.gradientAt(gradientStops(), bass);
        for (var i = 0; i < bassIdx && i < width; i++)
            bassV[i] = bassHsv.v;

        // Build 5-band interpolated power strip
        var rawBands = [audio.beat, audio.bass, audio.low, audio.mid, audio.high];
        // Asymmetric EMA per bin (fast attack, slow decay)
        var smoothing = algo.presetSmoothing / 10.0;
        var riseAlpha = 0.5 * (1 - smoothing) + 0.05;
        var decayAlpha = 0.02 + 0.03 * (1 - smoothing);
        for (var sb = 0; sb < 5; sb++) {
            var sa = rawBands[sb] > smoothBands[sb] ? riseAlpha : decayAlpha;
            smoothBands[sb] += sa * (rawBands[sb] - smoothBands[sb]);
        }
        var bands = smoothBands;

        // --- Bar-level build-up ---
        var rawEnergy = (audio.low + audio.mid * 0.5 + audio.high * 0.3) / 1.8;
        barEnergy += rawEnergy * audio.dt;
        if (barEnergy > peakEnergy) peakEnergy = barEnergy;
        if (audio.downbeat) {
            releaseFlash = Math.min(1, peakEnergy * 0.5);
            barEnergy = 0;
            peakEnergy = 0;
        }
        releaseFlash *= 0.85;

        var strip = new Array(width);
        for (var x = 0; x < width; x++) {
            var spatial = width <= 1 ? 0 : x / (width - 1);
            // Interpolate band power at this position
            var bandPos = spatial * (bands.length - 1);
            var lo = Math.floor(bandPos);
            var hi = Math.min(bands.length - 1, lo + 1);
            var frac = bandPos - lo;
            var melVal = bands[lo] * (1 - frac) + bands[hi] * frac;

            var gradHsv = HSVUtil.gradientAt(gradientStops(), spatial);
            // Release briefly expands all bars toward peak
            var baseV = gradHsv.v * Math.min(1, melVal + releaseFlash * 0.3);
            var bV = bassV[x];
            var spV = sparksV[x];
            var totalV = Math.max(0, Math.min(1, baseV + bV + spV));
            var ph = gradHsv.h, ps = gradHsv.s;
            if (spV > baseV && spV > bV) {
                ph = sparkColor.h; ps = sparkColor.s;
            } else if (bV > baseV) {
                ph = bassHsv.h; ps = bassHsv.s;
            }
            strip[x] = {h: ph, s: ps, v: totalV};
        }

        strip = boxBlur(strip, algo.blur);
        for (var y = 0; y < height; y++)
            for (var px = 0; px < width; px++)
                HSVUtil.setPixel(map, width, px, y, strip[px].h, strip[px].s, strip[px].v);
        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
