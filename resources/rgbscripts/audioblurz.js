/*
  Q Light Controller Plus
  audioblurz.js

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
    algo.name = "Audio Blurz";
    algo.author = "QLC+ contributors";
    algo.acceptColors = 3; // low/mid/high mel-bank gradient
    algo.usesAudio = true;
    algo.properties = new Array();

    algo.presetBlurRadius = 1;
    algo.properties.push(
      "name:presetBlurRadius|type:range|display:Blur Radius (px)|" +
      "values:1,5|write:setBlurRadius|read:getBlurRadius");
    algo.setBlurRadius = function(v) { algo.presetBlurRadius = parseInt(v); };
    algo.getBlurRadius = function() { return algo.presetBlurRadius; };

    algo.presetDecayMs = 800;
    algo.properties.push(
      "name:presetDecayMs|type:range|display:Decay Time (ms)|" +
      "values:100,5000|write:setDecayMs|read:getDecayMs");
    algo.setDecayMs = function(v) { algo.presetDecayMs = parseInt(v); };
    algo.getDecayMs = function() { return algo.presetDecayMs; };

    algo.presetFluxThreshold = 25;
    algo.properties.push(
      "name:presetFluxThreshold|type:range|display:Flux Threshold (%)|" +
      "values:0,100|write:setFluxThreshold|read:getFluxThreshold");
    algo.setFluxThreshold = function(v) { algo.presetFluxThreshold = parseInt(v); };
    algo.getFluxThreshold = function() { return algo.presetFluxThreshold; };

    algo.presetBurstDensity = 6;
    algo.properties.push(
      "name:presetBurstDensity|type:range|display:Burst Density|" +
      "values:1,16|write:setBurstDensity|read:getBurstDensity");
    algo.setBurstDensity = function(v) { algo.presetBurstDensity = parseInt(v); };
    algo.getBurstDensity = function() { return algo.presetBurstDensity; };

    algo.presetBurstSpread = 4;
    algo.properties.push(
      "name:presetBurstSpread|type:range|display:Burst Spread (px)|" +
      "values:0,32|write:setBurstSpread|read:getBurstSpread");
    algo.setBurstSpread = function(v) { algo.presetBurstSpread = parseInt(v); };
    algo.getBurstSpread = function() { return algo.presetBurstSpread; };

    algo.presetMaxBursts = 16;
    algo.properties.push(
      "name:presetMaxBursts|type:range|display:Max Bursts per Frame|" +
      "values:1,64|write:setMaxBursts|read:getMaxBursts");
    algo.setMaxBursts = function(v) { algo.presetMaxBursts = parseInt(v); };
    algo.getMaxBursts = function() { return algo.presetMaxBursts; };

    algo.presetAxis = "Horizontal";
    algo.properties.push(
      "name:presetAxis|type:list|display:Burst Axis|" +
      "values:Horizontal,Vertical|write:setAxis|read:getAxis");
    algo.setAxis = function(v) { algo.presetAxis = String(v); };
    algo.getAxis = function() { return algo.presetAxis; };

    var DEFAULT_BANDS = [0xFF4000, 0x00FF64, 0x4080FF];

    algo.fb = null;
    algo.scratch = null;
    algo.fbW = 0;
    algo.fbH = 0;

    algo.ensureBuffers = function(w, h) {
        if (algo.fbW === w && algo.fbH === h && algo.fb !== null) return;
        algo.fbW = w;
        algo.fbH = h;
        algo.fb = new Array(w * h);
        algo.scratch = new Array(w * h);
        for (var i = 0; i < w * h; i++) { algo.fb[i] = 0; algo.scratch[i] = 0; }
    };

    algo.rgbMapStepCount = function(_w, _h) { return 1; };
    algo.rgbMapSetColors = function(_raw) { };
    algo.rgbMapGetColors = function() {
        return algo.gradientBandColors ? algo.gradientBandColors.slice() : DEFAULT_BANDS.slice();
    };

    var blendAddPacked = function(a, b) {
        var r = ((a >> 16) & 0xFF) + ((b >> 16) & 0xFF);
        var g = ((a >> 8) & 0xFF) + ((b >> 8) & 0xFF);
        var bl = (a & 0xFF) + (b & 0xFF);
        if (r > 255) r = 255;
        if (g > 255) g = 255;
        if (bl > 255) bl = 255;
        return (r << 16) | (g << 8) | bl;
    };

    var boxBlurH = function(srcBuf, dstBuf, w, h, radius) {
        var diam = 2 * radius + 1;
        for (var y = 0; y < h; y++) {
            var row = y * w;
            for (var x = 0; x < w; x++) {
                var r = 0, g = 0, b = 0;
                for (var k = -radius; k <= radius; k++) {
                    var sx = x + k;
                    if (sx < 0) sx = 0;
                    else if (sx > w - 1) sx = w - 1;
                    var c = srcBuf[row + sx];
                    r += (c >> 16) & 0xFF;
                    g += (c >> 8) & 0xFF;
                    b += c & 0xFF;
                }
                dstBuf[row + x] = ((r / diam) << 16) | ((g / diam) << 8) | (b / diam) | 0;
            }
        }
    };

    var boxBlurV = function(srcBuf, dstBuf, w, h, radius) {
        var diam = 2 * radius + 1;
        for (var x = 0; x < w; x++) {
            for (var y = 0; y < h; y++) {
                var r = 0, g = 0, b = 0;
                for (var k = -radius; k <= radius; k++) {
                    var sy = y + k;
                    if (sy < 0) sy = 0;
                    else if (sy > h - 1) sy = h - 1;
                    var c = srcBuf[sy * w + x];
                    r += (c >> 16) & 0xFF;
                    g += (c >> 8) & 0xFF;
                    b += c & 0xFF;
                }
                dstBuf[y * w + x] = ((r / diam) << 16) | ((g / diam) << 8) | (b / diam) | 0;
            }
        }
    };

    algo.bandBlend = function(audio) {
        return AudioColors.blendByPower(algo, audio);
    };

    algo.rgbMap = function(width, height, rgb, step, audio) {
        var map = RGBUtil.createFlatMap(width, height);
        var dt = audio.timing.consumerDtMs / 1000.0;
        algo.ensureBuffers(width, height);

        var decayPerFrame = Math.exp(-dt / (algo.presetDecayMs / 1000.0));
        var N = width * height;
        for (var i = 0; i < N; i++) {
            var c = algo.fb[i];
            if (c === 0) continue;
            var r = Math.floor(((c >> 16) & 0xFF) * decayPerFrame);
            var g = Math.floor(((c >> 8) & 0xFF) * decayPerFrame);
            var b = Math.floor((c & 0xFF) * decayPerFrame);
            algo.fb[i] = (r << 16) | (g << 8) | b;
        }

        var radius = algo.presetBlurRadius;
        if (radius > 0) {
            boxBlurH(algo.fb, algo.scratch, width, height, radius);
            boxBlurV(algo.scratch, algo.fb, width, height, radius);
        }

        var horizontal = (algo.presetAxis === "Horizontal");
        var dimLong  = horizontal ? width  : height;

        var flux = audio.features.flux;
        var threshold = algo.presetFluxThreshold / 100.0;
        if (flux > threshold) {
            var headroom = 1 - threshold;
            if (headroom < 0.001) headroom = 0.001;
            var nBursts = Math.floor(algo.presetBurstDensity * (flux - threshold) / headroom);
            if (nBursts < 1) nBursts = 1;
            if (nBursts > algo.presetMaxBursts) nBursts = algo.presetMaxBursts;

            var color = algo.bandBlend(audio);

            var full = audio.spectrum.full;
            var peakBin = 0;
            var peakVal = -1;
            for (var fi = 0; fi < full.length; fi++) {
                if (full[fi] > peakVal) { peakVal = full[fi]; peakBin = fi; }
            }
            var lenF = full.length > 0 ? full.length : 1;
            var peakLong = Math.floor(peakBin * dimLong / lenF);

            for (var k = 0; k < nBursts; k++) {
                var off = Math.floor((Math.random() - 0.5) * algo.presetBurstSpread);
                var pos = peakLong + off;
                // Clamp render-side burst position to pixel range.
                if (pos < 0) pos = 0;
                if (pos > dimLong - 1) pos = dimLong - 1;
                if (horizontal) {
                    for (var y = 0; y < height; y++) {
                        var idxH = y * width + pos;
                        algo.fb[idxH] = blendAddPacked(algo.fb[idxH], color);
                    }
                } else {
                    for (var xx = 0; xx < width; xx++) {
                        var idxV = pos * width + xx;
                        algo.fb[idxV] = blendAddPacked(algo.fb[idxV], color);
                    }
                }
            }
        }

        for (var y2 = 0; y2 < height; y2++) {
            for (var x2 = 0; x2 < width; x2++) {
                map[(y2) * width + (x2)] = algo.fb[y2 * width + x2];
            }
        }
        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
