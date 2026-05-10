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
    algo.acceptColors = 3; // low/mid/high mel-bank gradient
    algo.usesAudio = true;
    algo.properties = new Array();

    algo.presetReactivity = 8;
    algo.presetFloor = 0;

    algo.presetSparks = 0;
    algo.properties.push(
      "name:presetSparks|type:list|display:Beat Sparks|" +
      "values:Off,On|write:setSparks|read:getSparks");

    algo.setSparks = function(_v) { algo.presetSparks = (_v === "On") ? 1 : 0; };
    algo.getSparks = function() { return algo.presetSparks ? "On" : "Off"; };

    var SPARK_DECAY = 0.85;
    var SPARK_THRESHOLD = 0.1;
    var SPARK_DENSITY = 15; // 1 spark per N pixels of width
    var sparksPixels = null;
    var sparkColors = null;

    function initSparks(width) {
        sparksPixels = new Array(width);
        sparkColors = new Array(width);
        for (var i = 0; i < width; i++) {
            sparksPixels[i] = 0;
            sparkColors[i] = [255, 255, 255];
        }
    }

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() {
        return AudioColors.bands(algo).slice();
    };


    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        if (!sparksPixels || sparksPixels.length !== width) initSparks(width);
        var map = RGBUtil.createMap(width, height);
        if (!audio) return map;

        var effectiveWidth = (typeof algo.displayWidth !== 'undefined') ? algo.displayWidth : width;
        var bands = RGBUtil.interpolate(audio.spectrum.full, effectiveWidth);
        for (var bi = 0; bi < bands.length; bi++)
            bands[bi] = Math.min(1, bands[bi]);
        var bass = audio.power.low;
        var dominantPacked = AudioColors.dominant(algo, audio);
        var dominant = [(dominantPacked >> 16) & 0xFF, (dominantPacked >> 8) & 0xFF, dominantPacked & 0xFF];

        // Bass overlay: fill from edge based on bass power
        var bassIdx = Math.min(width, Math.floor(bass * width * 1.5));

        var beat = audio.beat.fired || audio.beat.kick || audio.onset.fired;
        if (algo.presetSparks && beat) {
            var hitScale = Math.min(1.0, 0.4 + 0.6 * audio.onset.intensity);
            var sparkCount = Math.max(1, Math.floor(width / SPARK_DENSITY));
            for (var s = 0; s < sparkCount; s++) {
                var sx = Math.floor(Math.random() * width);
                sparksPixels[sx] = hitScale;
                sparkColors[sx] = dominant;
            }
        }
        for (var i = 0; i < width; i++)
            sparksPixels[i] *= SPARK_DECAY;

        for (var y = 0; y < height; y++) {
            for (var x = 0; x < width; x++) {
                var edgeGlow = 0.75 + 0.25 * x / Math.max(1, width - 1);
                var r = dominant[0] * edgeGlow;
                var g = dominant[1] * edgeGlow;
                var b = dominant[2] * edgeGlow;

                // Brightness from spectrum
                var specBright = Math.min(1, bands[x % bands.length]);

                // Bass overlay brightness
                var bassBright = (x < bassIdx) ? bass : 0;

                // Combine
                var baseBright = Math.max(specBright, bassBright);
                var bright = algo.presetFloor/100 + (1 - algo.presetFloor/100) * baseBright;

                if (sparksPixels[x] > SPARK_THRESHOLD) {
                    var sparkVal = sparksPixels[x];
                    var sc = sparkColors[x] || [255, 255, 255];
                    r = r * (1 - sparkVal) + sc[0] * sparkVal;
                    g = g * (1 - sparkVal) + sc[1] * sparkVal;
                    b = b * (1 - sparkVal) + sc[2] * sparkVal;
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
