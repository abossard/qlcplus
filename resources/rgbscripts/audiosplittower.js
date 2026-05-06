/*
  Q Light Controller Plus
  audiosplittower.js

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
    algo.name = "Audio Split Tower";
    algo.author = "QLC+ contributors";
    algo.acceptColors = 5;
    algo.usesAudio = true;
    algo.properties = new Array();

    AudioParams.installContinuous(algo, {gain: 7, reactivity: 7, floor: 15});

    algo.presetBands = 3;
    algo.properties.push(
      "name:presetBands|type:range|display:Bands|" +
      "values:2,5|write:setBands|read:getBands");


    algo.presetPeakHold = 10;
    algo.properties.push(
      "name:presetPeakHold|type:range|display:PeakHold|" +
      "values:1,20|write:setPeakHold|read:getPeakHold");


    algo.presetDecay = 5;
    algo.properties.push(
      "name:presetDecay|type:range|display:Decay|" +
      "values:1,10|write:setDecay|read:getDecay");

    algo.setBands = function(_v) { algo.presetBands = Math.max(2, Math.min(5, parseInt(_v))); };
    algo.getBands = function() { return algo.presetBands; };
    algo.setPeakHold = function(_v) { algo.presetPeakHold = parseInt(_v); };
    algo.getPeakHold = function() { return algo.presetPeakHold; };
    algo.setDecay = function(_v) { algo.presetDecay = parseInt(_v); };
    algo.getDecay = function() { return algo.presetDecay; };

    var sectionColors = [
        [255, 0, 0],
        [0, 255, 0],
        [0, 0, 255],
        [255, 255, 0],
        [0, 255, 255]
    ];

    algo.peakValues = [];
    algo.peakHolds = [];
    algo.smoothBands = [];

    algo.rgbMapStepCount = function(width, height) { return 1; };

    algo.rgbMapSetColors = function(rawColors) {
        if (!rawColors) return;
        for (var i = 0; i < Math.min(rawColors.length, 5); i++)
            sectionColors[i] = [(rawColors[i] >> 16) & 0xFF, (rawColors[i] >> 8) & 0xFF, rawColors[i] & 0xFF];
    };

    algo.rgbMapGetColors = function() {
        var result = [];
        for (var i = 0; i < algo.presetBands; i++)
            result.push(RGBUtil.rgb(sectionColors[i][0], sectionColors[i][1], sectionColors[i][2]));
        return result;
    };

    function ensureState() {
        var n = algo.presetBands;
        if (!algo.peakValues || algo.peakValues.length !== n) {
            algo.peakValues = []; for (var i = 0; i < n; i++) algo.peakValues.push(0);
        }
        if (!algo.peakHolds || algo.peakHolds.length !== n) {
            algo.peakHolds = []; for (var i = 0; i < n; i++) algo.peakHolds.push(0);
        }
        if (!algo.smoothBands || algo.smoothBands.length !== n) {
            algo.smoothBands = []; for (var i = 0; i < n; i++) algo.smoothBands.push(0);
        }
    }

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        ensureState();
        var map = RGBUtil.createMap(width, height);
        if (!audio || !audio.mel || audio.mel.length === 0) return map;

        var numBands = algo.presetBands;
        var bands = RGBUtil.interpolate(audio.mel, numBands);
        var floorBrightness = AudioParams.applyFloor(algo, 0);
        var fallStep = algo.presetDecay / 100.0;
        var peakStep = Math.max(1, Math.round(algo.presetDecay / 2));

        for (var section = 0; section < numBands; section++) {
            var magnitude = Math.max(0, Math.min(1, bands[section]));
            if (magnitude > algo.smoothBands[section])
                algo.smoothBands[section] = magnitude;
            else
                algo.smoothBands[section] = Math.max(0, algo.smoothBands[section] - fallStep);

            var smoothMagnitude = Math.max(0, Math.min(1, algo.smoothBands[section]));
            var barHeight = Math.round(smoothMagnitude * height);
            if (magnitude > 0.01)
                barHeight = Math.max(1, barHeight);

            if (barHeight >= algo.peakValues[section]) {
                algo.peakValues[section] = barHeight;
                algo.peakHolds[section] = algo.presetPeakHold;
            } else if (algo.peakHolds[section] > 0) {
                algo.peakHolds[section]--;
            } else {
                algo.peakValues[section] = Math.max(0, algo.peakValues[section] - peakStep);
            }

            var peakPosition = Math.round(algo.peakValues[section]);
            var sectionStart = Math.floor(section * width / numBands);
            var sectionEnd = Math.floor((section + 1) * width / numBands);
            sectionStart = Math.max(0, Math.min(width, sectionStart));
            sectionEnd = Math.max(sectionStart, Math.min(width, sectionEnd));
            var color = sectionColors[section];

            for (var x = sectionStart; x < sectionEnd; x++) {
                for (var y = 0; y < height; y++) {
                    var fromBottom = height - 1 - y;
                    if (fromBottom < barHeight) {
                        var brightness = AudioParams.applyFloor(algo, smoothMagnitude * (1 - y / height * 0.3));
                        map[y][x] = RGBUtil.rgb(
                            color[0] * brightness,
                            color[1] * brightness,
                            color[2] * brightness);
                    } else if (fromBottom === peakPosition && peakPosition < height) {
                        map[y][x] = RGBUtil.rgb(color[0], color[1], color[2]);
                    } else if (floorBrightness > 0 && magnitude > 0.01) {
                        map[y][x] = RGBUtil.rgb(
                            color[0] * floorBrightness,
                            color[1] * floorBrightness,
                            color[2] * floorBrightness);
                    }
                }
            }
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
