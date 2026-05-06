/*
  Q Light Controller Plus
  audiopower.js

  Copyright (c) QLC+ contributors
  Ported from LedFX "Power" effect (MIT License)

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
    algo.name = "Audio Power";
    algo.author = "Ported from LedFx";
    algo.acceptColors = 2;
    algo.usesAudio = true;
    algo.properties = new Array();

    AudioParams.installContinuous(algo, {gain: 7, reactivity: 8});

    algo.presetSparks = 0;
    algo.properties.push(
      "name:presetSparks|type:list|display:Beat Sparks|" +
      "values:Off,On|write:setSparks|read:getSparks");

    algo.setSparks = function(_v) { algo.presetSparks = (_v === "On") ? 1 : 0; };
    algo.getSparks = function() { return algo.presetSparks ? "On" : "Off"; };

    var startColor = [255, 0, 0];
    var endColor = [0, 0, 255];
    var bassFilter = null;
    var sparksPixels = null;
    var initialized = false;

    function init(width) {
        bassFilter = new AudioDSP.Filter(0.1, AudioParams.filterRise(algo));
        sparksPixels = new Array(width);
        for (var i = 0; i < width; i++) sparksPixels[i] = 0;
        initialized = true;
    }

    algo.rgbMapStepCount = function(width, height) { return 1; };

    algo.rgbMapSetColors = function(rawColors) {
        if (rawColors && rawColors.length >= 1)
            startColor = [(rawColors[0] >> 16) & 0xFF, (rawColors[0] >> 8) & 0xFF, rawColors[0] & 0xFF];
        if (rawColors && rawColors.length >= 2)
            endColor = [(rawColors[1] >> 16) & 0xFF, (rawColors[1] >> 8) & 0xFF, rawColors[1] & 0xFF];
    };

    algo.rgbMapGetColors = function() {
        return [RGBUtil.rgb(startColor[0], startColor[1], startColor[2]),
                RGBUtil.rgb(endColor[0], endColor[1], endColor[2])];
    };


    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        if (!initialized || !sparksPixels || sparksPixels.length !== width) init(width);
        var map = RGBUtil.createMap(width, height);
        if (!audio || !audio.mel || audio.mel.length === 0) return map;

        // Get spectrum and bass power
        var effectiveWidth = (typeof algo.displayWidth !== 'undefined') ? algo.displayWidth : width;
        var bands = RGBUtil.interpolate(audio.mel, effectiveWidth);
        for (var bi = 0; bi < bands.length; bi++)
            bands[bi] = Math.min(1, bands[bi]);
        var bass = bassFilter.update(audio.bands.low);

        // Bass overlay: fill from edge based on bass power
        var bassIdx = Math.min(width, Math.floor(bass * width * 1.5));

        // Sparks: random pixels on beat
        var beat = audio.triggers.beat.firedThisFrame;
        if (algo.presetSparks && beat) {
            var sparkCount = Math.max(1, Math.floor(width / 15));
            for (var s = 0; s < sparkCount; s++) {
                var sx = Math.floor(Math.random() * width);
                sparksPixels[sx] = 1.0;
            }
        }
        // Decay sparks
        for (var i = 0; i < width; i++)
            sparksPixels[i] *= 0.85;

        for (var y = 0; y < height; y++) {
            for (var x = 0; x < width; x++) {
                // Gradient color based on position
                var t = x / Math.max(1, width - 1);
                var r = startColor[0] + (endColor[0] - startColor[0]) * t;
                var g = startColor[1] + (endColor[1] - startColor[1]) * t;
                var b = startColor[2] + (endColor[2] - startColor[2]) * t;

                // Brightness from spectrum
                var specBright = Math.min(1, bands[x % bands.length]);

                // Bass overlay brightness
                var bassBright = (x < bassIdx) ? bass : 0;

                // Combine
                var bright = AudioParams.applyFloor(algo, Math.max(specBright, bassBright));

                // Add sparks (white flash)
                if (sparksPixels[x] > 0.1) {
                    var sparkVal = sparksPixels[x];
                    r = r * (1 - sparkVal) + 255 * sparkVal;
                    g = g * (1 - sparkVal) + 255 * sparkVal;
                    b = b * (1 - sparkVal) + 255 * sparkVal;
                    bright = Math.max(bright, sparkVal);
                }

                map[y][x] = RGBUtil.rgb(r * bright, g * bright, b * bright);
            }
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
