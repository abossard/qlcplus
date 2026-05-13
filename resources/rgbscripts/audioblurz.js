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

    var DEFAULT_BANDS = [
        {h: 0.042, s: 1.0, v: 1.0},
        {h: 0.398, s: 1.0, v: 1.0},
        {h: 0.611, s: 0.75, v: 1.0}
    ];

    algo.fb = null;
    algo.scratch = null;
    algo.fbW = 0;
    algo.fbH = 0;

    algo.ensureBuffers = function(w, h) {
        if (algo.fbW === w && algo.fbH === h && algo.fb !== null) return;
        algo.fbW = w;
        algo.fbH = h;
        algo.fb = new Float32Array(w * h * 3);
        algo.scratch = new Float32Array(w * h * 3);
    };

    algo.rgbMapStepCount = function(_w, _h) { return 1; };
    algo.rgbMapSetColors = function(_raw) { };
    algo.rgbMapGetColors = function() {
        return algo.colors ? algo.colors.slice() : DEFAULT_BANDS.slice();
    };

    var TWO_PI = Math.PI * 2;

    var blendAddHsv = function(buf, idx, nh, ns, nv) {
        var ev = buf[idx + 2];
        if (ev < 0.001) {
            buf[idx] = nh; buf[idx + 1] = ns; buf[idx + 2] = nv;
            return;
        }
        var eh = buf[idx];
        var totalV = ev + nv;
        var cx = Math.cos(eh * TWO_PI) * ev + Math.cos(nh * TWO_PI) * nv;
        var cy = Math.sin(eh * TWO_PI) * ev + Math.sin(nh * TWO_PI) * nv;
        var bh = Math.atan2(cy, cx) / TWO_PI;
        if (bh < 0) bh += 1;
        buf[idx] = bh;
        buf[idx + 1] = (buf[idx + 1] * ev + ns * nv) / totalV;
        buf[idx + 2] = Math.min(1, totalV);
    };

    var boxBlurH = function(srcBuf, dstBuf, w, h, radius) {
        var diam = 2 * radius + 1;
        for (var y = 0; y < h; y++) {
            for (var x = 0; x < w; x++) {
                var sumH = 0, sumS = 0, sumV = 0;
                for (var k = -radius; k <= radius; k++) {
                    var sx = x + k;
                    if (sx < 0) sx = 0;
                    else if (sx > w - 1) sx = w - 1;
                    var si = (y * w + sx) * 3;
                    sumH += srcBuf[si];
                    sumS += srcBuf[si + 1];
                    sumV += srcBuf[si + 2];
                }
                var di = (y * w + x) * 3;
                dstBuf[di] = sumH / diam;
                dstBuf[di + 1] = sumS / diam;
                dstBuf[di + 2] = sumV / diam;
            }
        }
    };

    var boxBlurV = function(srcBuf, dstBuf, w, h, radius) {
        var diam = 2 * radius + 1;
        for (var x = 0; x < w; x++) {
            for (var y = 0; y < h; y++) {
                var sumH = 0, sumS = 0, sumV = 0;
                for (var k = -radius; k <= radius; k++) {
                    var sy = y + k;
                    if (sy < 0) sy = 0;
                    else if (sy > h - 1) sy = h - 1;
                    var si = (sy * w + x) * 3;
                    sumH += srcBuf[si];
                    sumS += srcBuf[si + 1];
                    sumV += srcBuf[si + 2];
                }
                var di = (y * w + x) * 3;
                dstBuf[di] = sumH / diam;
                dstBuf[di + 1] = sumS / diam;
                dstBuf[di + 2] = sumV / diam;
            }
        }
    };

    algo.bandBlend = function(audio) {
        var bandColors = algo.colors || DEFAULT_BANDS;
        var powers = audio.power.bands;
        var totalP = powers[0] + powers[1] + powers[2];
        if (totalP < 0.001) return {h: 0, s: 0, v: 0};
        var cx = 0, cy = 0, ws = 0, wv = 0;
        for (var i = 0; i < 3; i++) {
            var p = powers[i];
            cx += Math.cos(bandColors[i].h * TWO_PI) * p;
            cy += Math.sin(bandColors[i].h * TWO_PI) * p;
            ws += bandColors[i].s * p;
            wv += bandColors[i].v * p;
        }
        var h = Math.atan2(cy, cx) / TWO_PI;
        if (h < 0) h += 1;
        return {h: h, s: ws / totalP, v: wv / totalP};
    };

    algo.rgbMap = function(width, height, rgb, step, audio) {
        var map = HSVUtil.createMap(width, height);
        var dt = audio.timing.consumerDtMs / 1000.0;
        algo.ensureBuffers(width, height);

        var decayPerFrame = Math.exp(-dt / (algo.presetDecayMs / 1000.0));
        var N = width * height * 3;
        for (var i = 0; i < N; i += 3) {
            if (algo.fb[i + 2] < 0.001) continue;
            algo.fb[i + 2] *= decayPerFrame;
            if (algo.fb[i + 2] < 0.001) {
                algo.fb[i] = 0;
                algo.fb[i + 1] = 0;
                algo.fb[i + 2] = 0;
            }
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
                if (pos < 0) pos = 0;
                if (pos > dimLong - 1) pos = dimLong - 1;
                if (horizontal) {
                    for (var y = 0; y < height; y++) {
                        blendAddHsv(algo.fb, (y * width + pos) * 3, color.h, color.s, color.v);
                    }
                } else {
                    for (var xx = 0; xx < width; xx++) {
                        blendAddHsv(algo.fb, (pos * width + xx) * 3, color.h, color.s, color.v);
                    }
                }
            }
        }

        map.set(algo.fb);
        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
