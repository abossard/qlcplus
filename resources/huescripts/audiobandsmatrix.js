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
    algo.presetMatrixLayout = "Matrix";
    algo.properties.push("name:matrixLayout|type:list|display:Layout|values:Matrix,Repeated rows|write:setMatrixLayout|read:getMatrixLayout");
    algo.setBandCount = function(v) { algo.presetBandCount = Math.max(1, Math.min(16, parseInt(v) || 1)); };
    algo.getBandCount = function() { return algo.presetBandCount; };
    algo.setMirror = function(v) { algo.presetMirror = v === "Yes" ? "Yes" : "No"; };
    algo.getMirror = function() { return algo.presetMirror; };
    algo.setFlipGradient = function(v) { algo.presetFlipGradient = v === "Yes" ? "Yes" : "No"; };
    algo.getFlipGradient = function() { return algo.presetFlipGradient; };
    algo.setFlipBandOrder = function(v) { algo.presetFlipBandOrder = v === "Yes" ? "Yes" : "No"; };
    algo.getFlipBandOrder = function() { return algo.presetFlipBandOrder; };
    algo.setMatrixLayout = function(v) { algo.presetMatrixLayout = v === "Repeated rows" ? "Repeated rows" : "Matrix"; };
    algo.getMatrixLayout = function() { return algo.presetMatrixLayout; };
    var readOutputControls = HSVUtil.attachStripTransformControls(algo, { display: "" });

    algo.rgbMapStepCount = function() { return 1; };
    algo.rgbMapSetColors = function() {};
    algo.rgbMapGetColors = function() { return []; };

    function bandPeaks(r, count) {
        var baseWidth = Math.floor(r.length / count);
        var extra = r.length - baseWidth * count;
        var start = 0;
        var bands = new Array(count);
        for (var b = 0; b < count; b++) {
            var bandWidth = baseWidth + (b < extra ? 1 : 0);
            var peak = 0;
            for (var i = start; i < start + bandWidth; i++)
                peak = Math.max(peak, HSVUtil.clamp01(r[i]));
            bands[b] = { width: bandWidth, peak: peak, odd: (b & 1) !== 0 };
            start += bandWidth;
        }
        return bands;
    }

    function renderInternal(width, height, audio) {
        var map = HSVUtil.createMap(width, height);
        var pixelCount = width * height;
        if (pixelCount <= 0)
            return map;
        var bank = audio && audio.banks && audio.banks.full;
        var fallback = [HSVUtil.clamp01(audio && audio.low || 0), HSVUtil.clamp01(audio && audio.mid || 0), HSVUtil.clamp01(audio && audio.high || 0)];
        var source = bank && bank.count ? (bank.novelty || []) : fallback;
        var r = HSVUtil.interpolate(source, pixelCount);
        var gradSign = algo.presetFlipGradient === "Yes" ? -1 : 1;
        var gradOffset = algo.presetFlipGradient === "Yes" ? 1 : 0;
        if (width > 1 && height > 1) {
            var bands = bandPeaks(r, Math.min(algo.presetBandCount, width));
            if (algo.presetFlipBandOrder === "Yes")
                bands.reverse();
            for (var bi = 0; bi < bands.length; bi++) {
                var xStart = Math.floor(bi * width / bands.length);
                var xEnd = Math.floor((bi + 1) * width / bands.length);
                var band = bands[bi];
                for (var y = 0; y < height; y++) {
                    var logicalY = band.odd ? (height - 1 - y) : y;
                    var coverage = HSVUtil.clamp01(band.peak * height - (height - 1 - logicalY));
                    if (coverage > 0) {
                        var g = gradOffset + gradSign * (logicalY / Math.max(1, height));
                        var color = HSVUtil.gradientLedfxAt(algo.colors, HSVUtil.clamp01(g));
                        for (var x = xStart; x < xEnd; x++) {
                            var out3 = (y * width + x) * 3;
                            map[out3] = color.h;
                            map[out3 + 1] = color.s;
                            map[out3 + 2] = color.v * coverage;
                        }
                    }
                }
            }
        } else {
            var bands = bandPeaks(r, Math.min(algo.presetBandCount, pixelCount));
            var bandOutputs = new Array(bands.length);
            for (var b = 0; b < bands.length; b++) {
                var bandWidth = bands[b].width;
                var vol = Math.floor(bands[b].peak * bandWidth);
                var pixels = new Array(bandWidth);
                for (var j = 0; j < bandWidth; j++) {
                    if (j < vol) {
                        var g1 = gradOffset + gradSign * (j / Math.max(1, bandWidth));
                        pixels[j] = HSVUtil.gradientLedfxAt(algo.colors, HSVUtil.clamp01(g1));
                    } else pixels[j] = {h: 0, s: 0, v: 0};
                }
                if (bands[b].odd) pixels.reverse();
                bandOutputs[b] = pixels;
            }
            if (algo.presetFlipBandOrder === "Yes") bandOutputs.reverse();
            var idx = 0;
            for (var bj = 0; bj < bandOutputs.length; bj++) {
                for (var bk = 0; bk < bandOutputs[bj].length; bk++) {
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

    algo.rgbMap = function(width, height, rgb, step, audio) {
        var repeatedRows = algo.presetMatrixLayout === "Repeated rows" &&
            width > 1 && height > 1;
        if (!repeatedRows)
            return renderInternal(width, height, audio);

        var row = renderInternal(width, 1, audio);
        var map = HSVUtil.createMap(width, height);
        var rowSize = width * 3;
        for (var y = 0; y < height; y++)
            map.set(row, y * rowSize);
        return map;
    };

    testAlgo = algo;
    return algo;
})();
