/*
  Q Light Controller Plus
  audiomagnitude.js

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
    algo.name = "Audio Magnitude";
    algo.author = "QLC+ contributors";
    algo.acceptColors = 5;
    algo.usesAudio = true;
    algo.properties = [];

    algo.presetFrequencyRange = "Lows (beat+bass)";
    algo.properties.push("name:frequencyRange|type:list|display:Frequency Range|values:Beat,Bass,Lows (beat+bass),Mids,High|write:setFrequencyRange|read:getFrequencyRange");
    algo.setFrequencyRange = function(v) {
        algo.presetFrequencyRange = ["Beat", "Bass", "Lows (beat+bass)", "Mids", "High"].indexOf(v) >= 0
            ? v : "Lows (beat+bass)";
    };
    algo.getFrequencyRange = function() { return algo.presetFrequencyRange; };
    var readOutputControls = HSVUtil.attachStripTransformControls(algo);

    algo.rgbMapStepCount = function() { return 1; };
    algo.rgbMapSetColors = function() {};
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step, audio) {
        var map = HSVUtil.createMap(width, height);
        var magnitude = HSVUtil.powerForRange(audio, algo.presetFrequencyRange, false);
        var total = width * height;
        for (var p = 0; p < total; p++) {
            var color = HSVUtil.gradientLedfxAt(algo.colors, p / Math.max(1, total - 1));
            map[p * 3] = color.h;
            map[p * 3 + 1] = color.s;
            map[p * 3 + 2] = color.v * magnitude;
        }
        HSVUtil.applyStripTransforms(map, width, height, readOutputControls());
        return map;
    };

    testAlgo = algo;
    return algo;
})();
