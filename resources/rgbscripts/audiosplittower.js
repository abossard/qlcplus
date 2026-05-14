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
    algo.acceptColors = 3;
    algo.usesAudio = true;
    algo.properties = new Array();

    algo.presetBands = 3;
    algo.properties.push(
      "name:presetBands|type:range|display:Bands|" +
      "values:2,5|write:setBands|read:getBands");

    algo.presetPeakHold = 10;
    algo.properties.push(
      "name:presetPeakHold|type:range|display:PeakHold|" +
      "values:1,20|write:setPeakHold|read:getPeakHold");

    algo.presetDecay = 0.05;
    algo.properties.push(
      "name:presetDecay|type:float|display:Decay|" +
      "write:setDecay|read:getDecay");

    algo.setBands = function(_v) { algo.presetBands = parseInt(_v); };
    algo.getBands = function() { return algo.presetBands; };
    algo.setPeakHold = function(_v) { algo.presetPeakHold = parseInt(_v); };
    algo.getPeakHold = function() { return algo.presetPeakHold; };
    algo.setDecay = function(_v) { algo.presetDecay = parseFloat(_v); };
    algo.getDecay = function() { return algo.presetDecay; };

    var DEFAULT_BAND_COLORS = [
        {h: 0.958, s: 1.0, v: 1.0},
        {h: 0.167, s: 1.0, v: 1.0},
        {h: 0.611, s: 0.749, v: 1.0}
    ];
    var BEAT_PULSE_AMOUNT = 0.25;
    var TOP_DARKEN = 0.3;

    algo.peakValues = [];
    algo.peakHolds = [];
    algo.smoothBands = [];

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };

    function ensureState() {
        var n = algo.presetBands;
        if (algo.peakValues.length !== n) {
            algo.peakValues = new Array(n);
            algo.peakHolds = new Array(n);
            algo.smoothBands = new Array(n);
            for (var i = 0; i < n; i++) {
                algo.peakValues[i] = 0;
                algo.peakHolds[i] = 0;
                algo.smoothBands[i] = 0;
            }
        }
    }

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        ensureState();
        var map = HSVUtil.createMap(width, height);
        if (!audio) return map;

        var numBands = algo.presetBands;
        var sourceBands = [audio.low, audio.mid, audio.high];
        var bands = (numBands === 3) ? sourceBands : HSVUtil.interpolate(sourceBands, numBands);
        var bandColors = algo.colors || DEFAULT_BAND_COLORS;
        var fallStep = algo.presetDecay;
        var peakStep = Math.max(1, Math.round(algo.presetDecay * 50));

        var beatBoost = 1.0 + BEAT_PULSE_AMOUNT * audio.cosPulse;

        for (var section = 0; section < numBands; section++) {
            var magnitude = Math.max(0, bands[section]);
            if (magnitude > algo.smoothBands[section])
                algo.smoothBands[section] = magnitude;
            else
                algo.smoothBands[section] = Math.max(0, algo.smoothBands[section] - fallStep);

            var smoothMagnitude = Math.max(0, algo.smoothBands[section]);
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
            var color = bandColors[section % 3];

            for (var x = sectionStart; x < sectionEnd; x++) {
                for (var y = 0; y < height; y++) {
                    var fromBottom = height - 1 - y;
                    var i3 = (y * width + x) * 3;
                    if (fromBottom < barHeight) {
                        var baseBrightness = smoothMagnitude * (1 - y / height * TOP_DARKEN);
                        var brightness = baseBrightness * beatBoost;
                        map[i3] = color.h;
                        map[i3 + 1] = color.s;
                        map[i3 + 2] = Math.min(1, color.v * brightness);
                    } else if (fromBottom === peakPosition && peakPosition < height) {
                        map[i3] = color.h;
                        map[i3 + 1] = color.s;
                        map[i3 + 2] = color.v;
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
