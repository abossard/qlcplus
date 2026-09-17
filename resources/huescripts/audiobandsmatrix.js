/*
  Q Light Controller Plus
  audiobandsmatrix.js

  Copyright (c) QLC+ contributors

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

var testAlgo;

(function() {
    var algo = {};
    algo.apiVersion = 3;
    algo.name = "Audio Bands Matrix";
    algo.author = "QLC+ contributors";
    algo.acceptColors = 5;
    algo.usesAudio = true;
    algo.properties = [];

    algo.presetBandCount = 6;
    algo.presetMirror = "No";
    algo.presetFlipGradient = "No";
    algo.presetFlipBandOrder = "No";
    algo.properties.push("name:bandCount|type:range|display:Band Count|values:1,16|write:setBandCount|read:getBandCount");
    algo.properties.push("name:mirror|type:list|display:Mirror|values:Yes,No|write:setMirror|read:getMirror");
    algo.properties.push("name:flipGradient|type:list|display:Flip Gradient|values:Yes,No|write:setFlipGradient|read:getFlipGradient");
    algo.properties.push("name:flipBandOrder|type:list|display:Flip Band Order|values:Yes,No|write:setFlipBandOrder|read:getFlipBandOrder");
    algo.setBandCount = function(v) { algo.presetBandCount = Math.max(1, Math.min(16, parseInt(v) || 1)); };
    algo.getBandCount = function() { return algo.presetBandCount; };
    algo.setMirror = function(v) { algo.presetMirror = v === "Yes" ? "Yes" : "No"; };
    algo.getMirror = function() { return algo.presetMirror; };
    algo.setFlipGradient = function(v) { algo.presetFlipGradient = v === "Yes" ? "Yes" : "No"; };
    algo.getFlipGradient = function() { return algo.presetFlipGradient; };
    algo.setFlipBandOrder = function(v) { algo.presetFlipBandOrder = v === "Yes" ? "Yes" : "No"; };
    algo.getFlipBandOrder = function() { return algo.presetFlipBandOrder; };
    var readOutputControls = HSVUtil.attachStripTransformControls(algo, { display: "" });

    algo.rgbMapStepCount = function() { return 1; };
    algo.rgbMapSetColors = function() {};
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step, audio) {
        var map = HSVUtil.createMap(width, height);
        var pixelCount = width * height;
        if (pixelCount <= 0)
            return map;
        var bank = audio && audio.banks && audio.banks.full;
        var fallback = [HSVUtil.clamp01(audio && audio.low || 0), HSVUtil.clamp01(audio && audio.mid || 0), HSVUtil.clamp01(audio && audio.high || 0)];
        var source = bank && bank.count ? (bank.novelty || []) : fallback;
        var r = HSVUtil.interpolate(source, pixelCount);
        if (width > 1 && height > 1) {
            var visibleBands = Math.min(algo.presetBandCount, width);
            var samplesPerBand = Math.floor(pixelCount / visibleBands);
            var sampleExtra = pixelCount - samplesPerBand * visibleBands;
            var sampleStart = 0;
            var bands = new Array(visibleBands);
            for (var b = 0; b < visibleBands; b++) {
                var sampleCount = samplesPerBand + (b < sampleExtra ? 1 : 0);
                var peak = 0;
                for (var s = sampleStart; s < sampleStart + sampleCount; s++)
                    peak = Math.max(peak, HSVUtil.clamp01(r[s]));
                bands[b] = { peak: peak, odd: (b & 1) !== 0 };
                sampleStart += sampleCount;
            }
            if (algo.presetFlipBandOrder === "Yes")
                bands.reverse();
            var gradSign = algo.presetFlipGradient === "Yes" ? -1 : 1;
            var gradOffset = algo.presetFlipGradient === "Yes" ? 1 : 0;
            for (var bi = 0; bi < bands.length; bi++) {
                var xStart = Math.floor(bi * width / bands.length);
                var xEnd = Math.floor((bi + 1) * width / bands.length);
                var band = bands[bi];
                for (var y = 0; y < height; y++) {
                    var logicalY = band.odd ? (height - 1 - y) : y;
                    var coverage = HSVUtil.clamp01(band.peak * height - (height - 1 - logicalY));
                    var g = gradOffset + gradSign * (logicalY / Math.max(1, height));
                    var color = HSVUtil.gradientLedfxAt(algo.colors || [{h: 0.6, s: 1, v: 1}], HSVUtil.clamp01(g));
                    for (var x = xStart; x < xEnd; x++) {
                        var out3 = (y * width + x) * 3;
                        if (coverage > 0) {
                            map[out3] = color.h;
                            map[out3 + 1] = color.s;
                            map[out3 + 2] = color.v * coverage;
                        } else {
                            map[out3] = 0;
                            map[out3 + 1] = 0;
                            map[out3 + 2] = 0;
                        }
                    }
                }
            }
        } else {
            var bandsActive = Math.min(algo.presetBandCount, pixelCount);
            var baseWidth = Math.floor(pixelCount / bandsActive);
            var extra = pixelCount - baseWidth * bandsActive;
            var bandStart = 0;
            var bandOutputs = new Array(bandsActive);
            for (var b = 0; b < bandsActive; b++) {
                var bandWidth = baseWidth + (b < extra ? 1 : 0);
                var peak = 0;
                for (var i = bandStart; i < bandStart + bandWidth; i++)
                    peak = Math.max(peak, HSVUtil.clamp01(r[i]));
                var vol = Math.floor(peak * bandWidth);
                var out = new Array(bandWidth);
                var gradSign1 = algo.presetFlipGradient === "Yes" ? -1 : 1;
                var gradOffset1 = algo.presetFlipGradient === "Yes" ? 1 : 0;
                for (var j = 0; j < bandWidth; j++) {
                    if (j < vol) {
                        var g1 = gradOffset1 + gradSign1 * (j / Math.max(1, bandWidth));
                        var color1 = HSVUtil.gradientLedfxAt(algo.colors || [{h: 0.6, s: 1, v: 1}], HSVUtil.clamp01(g1));
                        out[j] = color1;
                    } else out[j] = {h: 0, s: 0, v: 0};
                }
                if (b & 1) out.reverse();
                bandOutputs[b] = out;
                bandStart += bandWidth;
            }
            if (algo.presetFlipBandOrder === "Yes") bandOutputs.reverse();
            var idx = 0;
            for (var bj = 0; bj < bandOutputs.length; bj++) {
                for (var bk = 0; bk < bandOutputs[bj].length; bk++) {
                    if (idx >= pixelCount) break;
                    var px = bandOutputs[bj][bk];
                    var out = idx * 3;
                    map[out] = px.h;
                    map[out + 1] = px.s;
                    map[out + 2] = px.v;
                    idx++;
                }
            }
        }
        return HSVUtil.applyStripTransforms(map, width, height, readOutputControls());
    };

    testAlgo = algo;
    return algo;
})();
