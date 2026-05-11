/*
  Q Light Controller Plus
  audiopower.js

  Copyright (c) QLC+ contributors
  Ported from LedFx "Power" effect (MIT License)

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
    algo.author = "Ported from LedFx";
    algo.acceptColors = 3;
    algo.usesAudio = true;
    algo.properties = new Array();

    var DEFAULT_GRADIENT = [
        {h: 0.0,   s: 1.0, v: 1.0},
        {h: 0.078, s: 1.0, v: 1.0},
        {h: 0.131, s: 1.0, v: 1.0},
        {h: 0.333, s: 1.0, v: 1.0},
        {h: 0.453, s: 1.0, v: 0.780},
        {h: 0.667, s: 1.0, v: 1.0},
        {h: 0.833, s: 1.0, v: 0.502},
        {h: 0.884, s: 1.0, v: 1.0}
    ];

    var DEFAULT_HIGH_BAND = {h: 0.884, s: 1.0, v: 1.0};

    algo.blur = 0.0;
    algo.bass_decay_rate = 0.05;
    algo.sparks_decay_rate = 0.15;

    algo.properties.push("name:blur|type:float|display:Blur|write:setBlur|read:getBlur");
    algo.properties.push("name:bass_decay_rate|type:float|display:Bass Decay Rate|write:setBassDecayRate|read:getBassDecayRate");
    algo.properties.push("name:sparks_decay_rate|type:float|display:Sparks Decay Rate|write:setSparksDecayRate|read:getSparksDecayRate");

    algo.setBlur = function(v) { algo.blur = clamp(parseFloat(v), 0, 10); };
    algo.getBlur = function() { return algo.blur; };
    algo.setBassDecayRate = function(v) { algo.bass_decay_rate = clamp(parseFloat(v), 0, 1); };
    algo.getBassDecayRate = function() { return algo.bass_decay_rate; };
    algo.setSparksDecayRate = function(v) { algo.sparks_decay_rate = clamp(parseFloat(v), 0, 1); };
    algo.getSparksDecayRate = function() { return algo.sparks_decay_rate; };

    var sparksV = null;
    var bassV = null;
    var sparkColor = DEFAULT_HIGH_BAND;
    var bassFilter = null;
    var lastWidth = 0;

    function clamp(v, lo, hi) {
        if (isNaN(v)) return lo;
        return Math.max(lo, Math.min(hi, v));
    }

    function gradientStops() {
        return (algo.gradientColors && algo.gradientColors.length > 0) ? algo.gradientColors : DEFAULT_GRADIENT;
    }

    function ensure(width) {
        if (lastWidth === width && sparksV !== null && sparksV.length === width) return;
        sparksV = new Float32Array(width);
        bassV = new Float32Array(width);
        bassFilter = null;
        lastWidth = width;
    }

    function scaleInPlace(arr, factor) {
        for (var i = 0; i < arr.length; i++) arr[i] *= factor;
    }

    function expFilter(value) {
        if (bassFilter === null) {
            bassFilter = value;
            return bassFilter;
        }
        var alpha = value > bassFilter ? 0.8 : 0.1;
        bassFilter = alpha * value + (1.0 - alpha) * bassFilter;
        return bassFilter;
    }

    function boxBlur(strip, amount) {
        var radius = Math.round(amount);
        if (radius <= 0 || strip.length <= 3) return strip;
        var n = strip.length;
        var out = new Array(n);
        for (var i = 0; i < n; i++) {
            var bh = 0, bs = 0, bv = 0, c = 0;
            for (var k = -radius; k <= radius; k++) {
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
    algo.rgbMapGetColors = function() { return DEFAULT_GRADIENT.slice(); };

    algo.rgbMap = function(width, height, rgb, step, audio) {
        ensure(width);
        var map = RGBUtil.createMap(width, height);
        if (!audio) return map;

        scaleInPlace(sparksV, 1.0 - clamp(algo.sparks_decay_rate, 0, 1));
        scaleInPlace(bassV, 1.0 - clamp(algo.bass_decay_rate, 0, 1));

        if (audio.onset.fired) {
            var numSparks = Math.floor(width / 20);
            var bands = (algo.gradientBandColors && algo.gradientBandColors.length >= 3)
                ? algo.gradientBandColors : null;
            sparkColor = bands ? bands[2] : DEFAULT_HIGH_BAND;
            for (var s = 0; s < numSparks; s++) {
                var sx = Math.floor(Math.random() * width);
                sparksV[sx] = sparkColor.v;
            }
        }

        var bass = expFilter(Math.max(0, audio.power.low));
        var bassIdx = Math.floor(bass * width);
        var bassHsv = RGBUtil.gradientAt(gradientStops(), bass);
        for (var i = 0; i < bassIdx && i < width; i++)
            bassV[i] = bassHsv.v;

        var mel = RGBUtil.interpolate(audio.spectrum.full, width);
        var strip = new Array(width);
        for (var x = 0; x < width; x++) {
            var spatial = width <= 1 ? 0 : x / (width - 1);
            var gradHsv = RGBUtil.gradientAt(gradientStops(), spatial);
            var baseV = gradHsv.v * mel[x];
            var bV = bassV[x];
            var spV = sparksV[x];
            var totalV = RGBUtil.clamp01(baseV + bV + spV);
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
                RGBUtil.setPixel(map, width, px, y, strip[px].h, strip[px].s, strip[px].v);
        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
