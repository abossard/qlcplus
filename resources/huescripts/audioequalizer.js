/*
  Q Light Controller Plus
  audioequalizer.js — 5-band equalizer (Audio API v2)

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
    algo.name = "Audio Equalizer";
    algo.author = "QLC+ contributors";
    algo.acceptColors = 5;
    algo.usesAudio = true;
    algo.properties = new Array();
    algo.presetMode = "Artistic";
    algo.properties.push("name:mode|type:list|display:Response|values:Artistic,Segment Equalizer|write:setMode|read:getMode");
    algo.setMode = function(v) { algo.presetMode = v === "Segment Equalizer" ? v : "Artistic"; };
    algo.getMode = function() { return algo.presetMode; };

    algo.presetDecay = 5;
    algo.properties.push(
      "name:presetDecay|type:range|display:Decay|" +
      "values:1,10|write:setDecay|read:getDecay");

    algo.presetPeaks = 0;
    algo.properties.push(
      "name:presetPeaks|type:list|display:Peak Markers|" +
      "values:Off,On|write:setPeaks|read:getPeaks");

    algo.presetCenter = 0;
    algo.properties.push(
      "name:presetCenter|type:list|display:Centered|" +
      "values:Off,On|write:setCenter|read:getCenter");

    algo.presetDownbeatDrop = 0.5;
    algo.properties.push(
      "name:presetDownbeatDrop|type:float|display:Downbeat Drop|" +
      "write:setDownbeatDrop|read:getDownbeatDrop");

    algo.presetPeakHold = 5;
    algo.properties.push(
      "name:presetPeakHold|type:range|display:Peak Hold|" +
      "values:1,20|write:setPeakHold|read:getPeakHold");

    algo.presetPeakDecay = 0.95;
    algo.properties.push(
      "name:presetPeakDecay|type:float|display:Peak Decay|" +
      "write:setPeakDecay|read:getPeakDecay");

    algo.setDecay = function(_v) { algo.presetDecay = parseFloat(_v); };
    algo.getDecay = function() { return algo.presetDecay; };
    algo.setPeaks = function(_v) { algo.presetPeaks = (_v === "On") ? 1 : 0; };
    algo.getPeaks = function() { return algo.presetPeaks ? "On" : "Off"; };
    algo.setCenter = function(_v) { algo.presetCenter = (_v === "On") ? 1 : 0; };
    algo.getCenter = function() { return algo.presetCenter ? "On" : "Off"; };
    algo.setDownbeatDrop = function(_v) { algo.presetDownbeatDrop = parseFloat(_v); };
    algo.getDownbeatDrop = function() { return algo.presetDownbeatDrop; };
    algo.setPeakHold = function(_v) { algo.presetPeakHold = parseInt(_v); };
    algo.getPeakHold = function() { return algo.presetPeakHold; };
    algo.setPeakDecay = function(_v) { algo.presetPeakDecay = parseFloat(_v); };
    algo.getPeakDecay = function() { return algo.presetPeakDecay; };

    algo.presetSegmentCount = 5;
    algo.properties.push(
      "name:presetSegmentCount|type:range|display:Segment Count|" +
      "values:1,16|write:setSegmentCount|read:getSegmentCount");
    algo.presetSegmentAlign = "Left";
    algo.properties.push(
      "name:presetSegmentAlign|type:list|display:Segment Alignment|" +
      "values:Left,Right,Center,Invert|write:setSegmentAlign|read:getSegmentAlign");
    algo.setSegmentCount = function(_v) {
        var n = parseInt(_v, 10);
        if (!isFinite(n)) n = 5;
        algo.presetSegmentCount = Math.max(1, Math.min(16, n));
    };
    algo.getSegmentCount = function() { return algo.presetSegmentCount; };
    algo.setSegmentAlign = function(_v) {
        algo.presetSegmentAlign = (_v === "Right" || _v === "Center" || _v === "Invert") ? _v : "Left";
    };
    algo.getSegmentAlign = function() { return algo.presetSegmentAlign; };
    var readSegmentTransforms = HSVUtil.attachStripTransformControls(algo, {
        prefix: "presetSegment",
        display: "Segment ",
        defaults: {
            flip: "Off",
            mirror: "Off",
            backgroundMode: "Off",
            backgroundColor: "#000000",
            backgroundBrightness: 1,
            brightness: 1,
            blur: 0
        }
    });

    var BAND_COUNT = 5;
    var DEFAULT_COLORS = [
        {h: 0.0,   s: 1.0, v: 1.0},  // red (beat)
        {h: 0.083, s: 1.0, v: 1.0},  // orange (bass)
        {h: 0.167, s: 1.0, v: 1.0},  // yellow (low)
        {h: 0.398, s: 1.0, v: 1.0},  // green (mid)
        {h: 0.611, s: 0.75, v: 1.0}  // blue (high)
    ];

    var smoothed = [0, 0, 0, 0, 0];
    var peakValues = [0, 0, 0, 0, 0];
    var peakHolds = [0, 0, 0, 0, 0];

    function splitSizes(total, count) {
        var active = Math.max(1, Math.min(count, Math.max(1, total)));
        var base = Math.floor(total / active);
        var extra = total - base * active;
        var sizes = new Array(active);
        for (var i = 0; i < active; i++)
            sizes[i] = base + (i < extra ? 1 : 0);
        return sizes;
    }

    function paintAlignedMask(mask, start, size, volume, align) {
        if (size <= 0 || volume <= 0) return;
        var lit = Math.max(0, Math.min(size, volume));
        if (lit <= 0) return;
        if (align === "Right") {
            for (var i = size - lit; i < size; i++)
                mask[start + i] = 1;
            return;
        }
        var shift = 0;
        if (align === "Center")
            shift = Math.floor((size - lit) / 2);
        else if (align === "Invert")
            shift = -Math.floor(lit / 2);
        for (var j = 0; j < lit; j++) {
            var pos = (j + shift) % size;
            if (pos < 0) pos += size;
            mask[start + pos] = 1;
        }
    }

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var map = HSVUtil.createMap(width, height);
        if (!audio) return map;
        if (algo.presetMode === "Segment Equalizer") {
            var pixelCount = width * height;
            if (pixelCount <= 0) return map;
            var count = Math.max(1, Math.min(16, algo.presetSegmentCount | 0, pixelCount));
            var bank = audio && audio.banks && audio.banks.full;
            var novelty = bank && bank.count ? bank.novelty : [];
            var bins = HSVUtil.interpolate(novelty || [], pixelCount);
            var stopsSeg = (algo.colors && algo.colors.length > 0) ? algo.colors : DEFAULT_COLORS;
            var clipped = new Array(pixelCount);
            for (var i = 0; i < pixelCount; i++)
                clipped[i] = HSVUtil.clamp01(isFinite(bins[i]) ? bins[i] : 0);
            var sizes = splitSizes(pixelCount, count);
            var mask = new Array(pixelCount);
            for (var m = 0; m < pixelCount; m++) mask[m] = 0;
            var start = 0;
            for (var seg = 0; seg < sizes.length; seg++) {
                var segWidth = sizes[seg];
                var sum = 0;
                for (var s = 0; s < segWidth; s++)
                    sum += clipped[start + s];
                var volume = Math.floor((sum / segWidth) * segWidth);
                paintAlignedMask(mask, start, segWidth, volume, algo.presetSegmentAlign);
                start += segWidth;
            }
            for (var p = 0; p < pixelCount; p++) {
                if (mask[p]) {
                    var tSeg = pixelCount <= 1 ? 0 : p / (pixelCount - 1);
                    var hsvSeg = HSVUtil.gradientLedfxAt(stopsSeg, tSeg);
                    var oSeg = p * 3;
                    map[oSeg] = hsvSeg.h;
                    map[oSeg + 1] = hsvSeg.s;
                    map[oSeg + 2] = hsvSeg.v;
                }
            }
            return HSVUtil.applyStripTransforms(map, width, height, readSegmentTransforms());
        }

        var rawBands = [audio.beat, audio.bass, audio.low, audio.mid, audio.high];
        var stops = (algo.colors && algo.colors.length >= 5) ? algo.colors : DEFAULT_COLORS;

        // EMA smoothing
        var decayAlpha = algo.presetDecay / 20.0;
        for (var b = 0; b < BAND_COUNT; b++) {
            var raw = Math.max(0, rawBands[b]);
            if (raw >= smoothed[b])
                smoothed[b] = raw;
            else
                smoothed[b] += (raw - smoothed[b]) * decayAlpha;
        }

        // Columns per band
        var colsPerBand = Math.floor(width / BAND_COUNT);
        var remainder = width - colsPerBand * BAND_COUNT;

        var xOffset = 0;
        for (var band = 0; band < BAND_COUNT; band++) {
            var bandWidth = colsPerBand + (band < remainder ? 1 : 0);
            var magnitude = smoothed[band];
            var barHeight = Math.round(magnitude * height);
            var c = stops[band];

            // Peak markers
            if (algo.presetPeaks) {
                if (audio.downbeat)
                    peakValues[band] *= algo.presetDownbeatDrop;
                if (magnitude >= peakValues[band]) {
                    peakValues[band] = magnitude;
                    peakHolds[band] = algo.presetPeakHold;
                } else if (peakHolds[band] > 0) {
                    peakHolds[band]--;
                } else {
                    peakValues[band] *= algo.presetPeakDecay;
                }
            }

            for (var bx = 0; bx < bandWidth; bx++) {
                var x = xOffset + bx;
                if (x >= width) break;

                if (algo.presetCenter) {
                    var halfBar = Math.floor(barHeight / 2);
                    var mid = Math.floor(height / 2);
                    for (var y = mid - halfBar; y <= mid + halfBar; y++) {
                        if (y < 0 || y >= height) continue;
                        HSVUtil.setPixel(map, width, x, y, c.h, c.s, c.v * magnitude);
                    }
                } else {
                    for (var dy = 0; dy < barHeight; dy++) {
                        var y = height - 1 - dy;
                        if (y < 0) break;
                        HSVUtil.setPixel(map, width, x, y, c.h, c.s, c.v * magnitude);
                    }
                }

                // Peak marker (white dot)
                if (algo.presetPeaks && peakValues[band] > 0.01) {
                    var peakY;
                    if (algo.presetCenter)
                        peakY = Math.floor(height / 2) - Math.floor(peakValues[band] * height / 2);
                    else
                        peakY = height - 1 - Math.min(height - 1, Math.floor(peakValues[band] * height));
                    if (peakY >= 0 && peakY < height)
                        HSVUtil.setPixel(map, width, x, peakY, 0, 0, peakValues[band]);
                }
            }
            xOffset += bandWidth;
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
