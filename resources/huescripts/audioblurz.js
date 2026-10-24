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
    algo.setBlurRadius = function(v) { algo.presetBlurRadius = Math.round(HSVUtil.parseNumber(v, algo.presetBlurRadius, 1, 5)); };
    algo.getBlurRadius = function() { return algo.presetBlurRadius; };

    algo.presetDecayMs = 800;
    algo.properties.push(
      "name:presetDecayMs|type:range|display:Decay Time (ms)|" +
      "values:100,5000|write:setDecayMs|read:getDecayMs");
    algo.setDecayMs = function(v) { algo.presetDecayMs = Math.round(HSVUtil.parseNumber(v, algo.presetDecayMs, 100, 5000)); };
    algo.getDecayMs = function() { return algo.presetDecayMs; };

    algo.presetFluxThreshold = 25;
    algo.properties.push(
      "name:presetFluxThreshold|type:range|display:Flux Threshold (%)|" +
      "values:0,100|write:setFluxThreshold|read:getFluxThreshold");
    algo.setFluxThreshold = function(v) { algo.presetFluxThreshold = Math.round(HSVUtil.parseNumber(v, algo.presetFluxThreshold, 0, 100)); };
    algo.getFluxThreshold = function() { return algo.presetFluxThreshold; };

    algo.presetBurstDensity = 6;
    algo.properties.push(
      "name:presetBurstDensity|type:range|display:Burst Density|" +
      "values:1,16|write:setBurstDensity|read:getBurstDensity");
    algo.setBurstDensity = function(v) { algo.presetBurstDensity = Math.round(HSVUtil.parseNumber(v, algo.presetBurstDensity, 1, 16)); };
    algo.getBurstDensity = function() { return algo.presetBurstDensity; };

    algo.presetBurstSpread = 4;
    algo.properties.push(
      "name:presetBurstSpread|type:range|display:Burst Spread (px)|" +
      "values:0,32|write:setBurstSpread|read:getBurstSpread");
    algo.setBurstSpread = function(v) { algo.presetBurstSpread = Math.round(HSVUtil.parseNumber(v, algo.presetBurstSpread, 0, 32)); };
    algo.getBurstSpread = function() { return algo.presetBurstSpread; };

    algo.presetMaxBursts = 16;
    algo.properties.push(
      "name:presetMaxBursts|type:range|display:Max Bursts per Frame|" +
      "values:1,64|write:setMaxBursts|read:getMaxBursts");
    algo.setMaxBursts = function(v) { algo.presetMaxBursts = Math.round(HSVUtil.parseNumber(v, algo.presetMaxBursts, 1, 64)); };
    algo.getMaxBursts = function() { return algo.presetMaxBursts; };

    algo.presetAxis = "Horizontal";
    algo.properties.push(
      "name:presetAxis|type:list|display:Burst Axis|" +
      "values:Horizontal,Vertical|write:setAxis|read:getAxis");
    algo.setAxis = function(v) { algo.presetAxis = String(v); };
    algo.getAxis = function() { return algo.presetAxis; };

    algo.fb = null;
    algo.scratch = null;
    algo.fbW = 0;
    algo.fbH = 0;
    // Blur passes and bursts are counted per 20 ms reference frame; the fractions carry over.
    algo.blurCredit = 0;
    algo.burstCredit = 0;
    var MAX_CATCHUP_FRAMES = 50; // a stall longer than 1 s is not caught up

    algo.ensureBuffers = function(w, h) {
        if (algo.fbW === w && algo.fbH === h && algo.fb !== null) return;
        algo.fbW = w;
        algo.fbH = h;
        algo.fb = new Float32Array(w * h * 3);
        algo.scratch = new Float32Array(w * h * 3);
    };

    algo.rgbMapStepCount = function(_w, _h) { return 1; };
    algo.rgbMapSetColors = function(_raw) { };
    algo.rgbMapGetColors = function() { return []; };

    var blendAddHsv = function(buf, idx, nh, ns, nv) {
        var ev = buf[idx + 2];
        if (ev < 0.001) {
            buf[idx] = nh; buf[idx + 1] = ns; buf[idx + 2] = nv;
            return;
        }
        var totalV = ev + nv;
        buf[idx] = HSVUtil.hueMean([buf[idx], nh], [ev, nv]);
        buf[idx + 1] = (buf[idx + 1] * ev + ns * nv) / totalV;
        buf[idx + 2] = Math.min(1, totalV);
    };

    // Box blur along rows, or along columns when vertical, clamping at the edges.
    // Colors mix in RGB, so hues wrap and black adds no hue.
    var boxBlur = function(srcBuf, dstBuf, w, h, radius, vertical) {
        var diam = 2 * radius + 1;
        var len = vertical ? h : w;
        var stride = vertical ? w : 1;
        var rgb = new Array(w * h);
        for (var p = 0; p < w * h; p++)
            rgb[p] = HSVUtil.hsvToRgb(srcBuf[p * 3], srcBuf[p * 3 + 1], srcBuf[p * 3 + 2]);
        for (var y = 0; y < h; y++) {
            for (var x = 0; x < w; x++) {
                var along = vertical ? y : x;
                var lineStart = vertical ? x : y * w;
                var sumR = 0, sumG = 0, sumB = 0;
                for (var k = -radius; k <= radius; k++) {
                    var sk = along + k;
                    if (sk < 0) sk = 0;
                    else if (sk > len - 1) sk = len - 1;
                    var c = rgb[lineStart + sk * stride];
                    sumR += c[0];
                    sumG += c[1];
                    sumB += c[2];
                }
                var hsv = HSVUtil.rgbToHsv(sumR / diam, sumG / diam, sumB / diam);
                var di = (y * w + x) * 3;
                dstBuf[di] = hsv.h;
                dstBuf[di + 1] = hsv.s;
                dstBuf[di + 2] = hsv.v;
            }
        }
    };

    algo.bandBlend = function(audio) {
        var bandColors = algo.colors;
        var powers = [audio.low, audio.mid, audio.high];
        var totalP = powers[0] + powers[1] + powers[2];
        if (totalP < 0.001) return {h: 0, s: 0, v: 0};
        var ws = 0, wv = 0;
        for (var i = 0; i < 3; i++) {
            ws += bandColors[i].s * powers[i];
            wv += bandColors[i].v * powers[i];
        }
        var hues = [bandColors[0].h, bandColors[1].h, bandColors[2].h];
        return {h: HSVUtil.hueMean(hues, powers), s: ws / totalP, v: wv / totalP};
    };

    algo.rgbMap = function(width, height, rgb, step, audio) {
        var map = HSVUtil.createMap(width, height);
        var dt = HSVUtil.audioSeconds(audio);
        var frames = Math.min(MAX_CATCHUP_FRAMES, HSVUtil.elapsedFrames(audio));
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
        algo.blurCredit += frames;
        while (algo.blurCredit >= 1) {
            boxBlur(algo.fb, algo.scratch, width, height, radius, false);
            boxBlur(algo.scratch, algo.fb, width, height, radius, true);
            algo.blurCredit--;
        }

        var horizontal = (algo.presetAxis === "Horizontal");
        var dimLong  = horizontal ? width  : height;

        var flux = audio.onsetIntensity;
        var threshold = algo.presetFluxThreshold / 100.0;
        if (flux > threshold) {
            var headroom = 1 - threshold;
            if (headroom < 0.001) headroom = 0.001;
            var nBursts = Math.floor(algo.presetBurstDensity * (flux - threshold) / headroom);
            if (nBursts < 1) nBursts = 1;
            if (nBursts > algo.presetMaxBursts) nBursts = algo.presetMaxBursts;
            algo.burstCredit += nBursts * frames;
            nBursts = Math.floor(algo.burstCredit);
            algo.burstCredit -= nBursts;

            var color = algo.bandBlend(audio);

            // Use 5 bands to determine peak position
            var bands = [audio.beat, audio.bass, audio.low, audio.mid, audio.high];
            var peakBin = 0;
            var peakVal = -1;
            // low is the mean of beat and bass, so it only reaches their peak when they are equal;
            // it then takes the slot between them.
            for (var fi = 0; fi < bands.length; fi++) {
                if (bands[fi] > peakVal || (fi === 2 && bands[fi] === peakVal)) { peakVal = bands[fi]; peakBin = fi; }
            }
            var peakLong = Math.floor(peakBin * dimLong / bands.length);

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
