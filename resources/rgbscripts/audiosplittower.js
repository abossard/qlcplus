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
    algo.acceptColors = 3; // low/mid/high mel-bank gradient
    algo.usesAudio = true;
    algo.properties = new Array();

    algo.presetReactivity = 7;
    algo.presetFloor = 15;

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

    var DEFAULT_BAND_COLORS = [0xFF0040, 0xFFFF00, 0x4080FF];
    var BEAT_PULSE_AMOUNT = 0.25;
    var TOP_DARKEN = 0.3; // top of bar fades to (1 - TOP_DARKEN) brightness

    algo.peakValues = [];
    algo.peakHolds = [];
    algo.smoothBands = [];

    function unpackColor(packed) { return [(packed >> 16) & 0xFF, (packed >> 8) & 0xFF, packed & 0xFF]; }

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() {
        return algo.gradientBandColors ? algo.gradientBandColors.slice() : DEFAULT_BAND_COLORS.slice();
    };

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
        var map = RGBUtil.createMap(width, height);
        if (!audio) return map;

        var numBands = algo.presetBands;
        var sourceBands = audio.power.bands;
        var bands = (numBands === 3) ? sourceBands : RGBUtil.interpolate(sourceBands, numBands);
        var colorStops = algo.gradientBandColors || DEFAULT_BAND_COLORS;
        var sectionColors = [];
        for (var ci = 0; ci < 3; ci++)
            sectionColors.push(unpackColor(colorStops[ci]));
        var floorBrightness = algo.presetFloor/100;
        var fallStep = algo.presetDecay / 100.0;
        var peakStep = Math.max(1, Math.round(algo.presetDecay / 2));

        // Beat-pulse brightness boost
        var beatBoost = 1.0 + BEAT_PULSE_AMOUNT * audio.beat.cosPulse;

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
            var color = sectionColors[section % 3];

            for (var x = sectionStart; x < sectionEnd; x++) {
                for (var y = 0; y < height; y++) {
                    var fromBottom = height - 1 - y;
                    if (fromBottom < barHeight) {
                        var baseBrightness = smoothMagnitude * (1 - y / height * TOP_DARKEN);
                        var brightness = (algo.presetFloor/100 + (1 - algo.presetFloor/100) * baseBrightness) * beatBoost;
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
