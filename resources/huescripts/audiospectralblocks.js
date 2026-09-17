/*
  Q Light Controller Plus
  audiospectralblocks.js

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
    algo.name = "Audio Spectral Blocks";
    algo.author = "QLC+ contributors";
    algo.acceptColors = 5;
    algo.usesAudio = true;
    algo.properties = [];

    algo.presetBlockCount = 4;
    algo.properties.push("name:blockCount|type:range|display:Block Count|values:1,10|write:setBlockCount|read:getBlockCount");
    algo.setBlockCount = function(v) { algo.presetBlockCount = Math.max(1, Math.min(10, parseInt(v) || 1)); };
    algo.getBlockCount = function() { return algo.presetBlockCount; };
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
        var blocks = Math.min(algo.presetBlockCount, pixelCount);
        var baseWidth = Math.floor(pixelCount / blocks);
        var extra = pixelCount - baseWidth * blocks;
        var start = 0;
        for (var b = 0; b < blocks; b++) {
            var blockWidth = baseWidth + (b < extra ? 1 : 0);
            var end = start + blockWidth;
            var peak = 0;
            for (var i = start; i < end; i++) peak = Math.max(peak, r[i]);
            var color = HSVUtil.gradientLedfxAt(algo.colors || [{h: 0.6, s: 1, v: 1}], b / blocks);
            var rgbColor = HSVUtil.hsvToRgb(color.h, color.s, color.v);
            for (var j = start; j < end; j++) {
                var power = r[j] * peak;
                var mixed = HSVUtil.rgbToHsvUnclipped(
                    rgbColor[0] * power,
                    rgbColor[1] * power,
                    rgbColor[2] * power
                );
                var o = j * 3;
                map[o] = mixed.h;
                map[o + 1] = mixed.s;
                map[o + 2] = mixed.v;
            }
            start += blockWidth;
        }
        return HSVUtil.applyStripTransforms(map, width, height, readOutputControls());
    };

    testAlgo = algo;
    return algo;
})();
