/*
  Q Light Controller Plus
  audiobands.js

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
    algo.name = "Audio Bands";
    algo.author = "QLC+ contributors";
    algo.acceptColors = 5;
    algo.usesAudio = true;
    algo.properties = [];

    algo.presetBandCount = 6;
    algo.presetAlign = "left";
    algo.properties.push("name:bandCount|type:range|display:Band Count|values:1,16|write:setBandCount|read:getBandCount");
    algo.properties.push("name:align|type:list|display:Align|values:left,right,invert,center|write:setAlign|read:getAlign");
    algo.setBandCount = function(v) { algo.presetBandCount = Math.max(1, Math.min(16, parseInt(v) || 1)); };
    algo.getBandCount = function() { return algo.presetBandCount; };
    algo.setAlign = function(v) { algo.presetAlign = ["left", "right", "invert", "center"].indexOf(String(v)) >= 0 ? String(v) : "left"; };
    algo.getAlign = function() { return algo.presetAlign; };
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
        var bandsActive = Math.min(algo.presetBandCount, pixelCount);
        var baseWidth = Math.floor(pixelCount / bandsActive);
        var extra = pixelCount - baseWidth * bandsActive;
        var start = 0;
        for (var b = 0; b < bandsActive; b++) {
            var bandWidth = baseWidth + (b < extra ? 1 : 0);
            var peak = 0;
            for (var i = start; i < start + bandWidth; i++)
                peak = Math.max(peak, HSVUtil.clamp01(r[i]));
            var vol = Math.floor(peak * bandWidth);
            var coord = b / bandsActive;
            var color = HSVUtil.gradientLedfxAt(algo.colors || [{h: 0.6, s: 1, v: 1}], coord);
            for (var j = 0; j < bandWidth; j++) {
                var idx = start + j;
                var on = j < vol;
                if (algo.presetAlign === "right") on = j >= (bandWidth - vol);
                else if (algo.presetAlign === "center") {
                    var s = Math.floor((bandWidth - vol) / 2);
                    on = j >= s && j < s + vol;
                } else if (algo.presetAlign === "invert") {
                    var shift = Math.floor(vol / 2);
                    var rolled = (j + shift) % bandWidth;
                    on = rolled < vol;
                }
                var out = idx * 3;
                if (on) {
                    map[out] = color.h;
                    map[out + 1] = color.s;
                    map[out + 2] = color.v;
                }
            }
            start += bandWidth;
        }
        return HSVUtil.applyStripTransforms(map, width, height, readOutputControls());
    };

    testAlgo = algo;
    return algo;
})();
