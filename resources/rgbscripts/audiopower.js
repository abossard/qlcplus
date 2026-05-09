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

    AudioParams.installContinuous(algo, {gain: 7, reactivity: 8});
    AudioParams.installBandPowerControls(algo);

    algo.presetSparks = 0;
    algo.properties.push(
      "name:presetSparks|type:list|display:Beat Sparks|" +
      "values:Off,On|write:setSparks|read:getSparks");

    algo.setSparks = function(_v) { algo.presetSparks = (_v === "On") ? 1 : 0; };
    algo.getSparks = function() { return algo.presetSparks ? "On" : "Off"; };

    var DEFAULT_BAND_COLORS = [0xFF0000, 0xFFFF00, 0xFFFFFF];
    var bassFilter = null;
    var sparksPixels = null;
    var sparkColors = null;
    var initialized = false;

    function init(width) {
        bassFilter = new AudioDSP.Filter(0.1, AudioParams.filterRise(algo));
        sparksPixels = new Array(width);
        sparkColors = new Array(width);
        for (var i = 0; i < width; i++) {
            sparksPixels[i] = 0;
            sparkColors[i] = [255, 255, 255];
        }
        initialized = true;
    }

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() {
        return AudioParams.bandColors(algo, DEFAULT_BAND_COLORS).slice();
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
        var bass = bassFilter.update(audio.lows);
        var dominant = AudioParams.colorChannels(
            AudioParams.dominantBandColor(algo, audio, DEFAULT_BAND_COLORS));

        // Bass overlay: fill from edge based on bass power
        var bassIdx = Math.min(width, Math.floor(bass * width * 1.5));

        // Sparks: random pixels on beat
        var beat = audio.triggers.beat.firedThisFrame || AudioParams.kickFired(audio) || AudioParams.anyOnsetFired(audio);
        if (algo.presetSparks && beat) {
            var sparkColor = AudioParams.colorChannels(
                AudioParams.dominantBandColor(algo, audio, DEFAULT_BAND_COLORS));
            var hitScale = Math.min(1.0, 0.4 + 0.6 * AudioParams.maxOnsetIntensity(audio));
            var sparkCount = Math.max(1, Math.floor(width / 15));
            for (var s = 0; s < sparkCount; s++) {
                var sx = Math.floor(Math.random() * width);
                sparksPixels[sx] = hitScale;
                sparkColors[sx] = sparkColor;
            }
        }
        // Decay sparks
        for (var i = 0; i < width; i++)
            sparksPixels[i] *= 0.85;

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
                var bright = AudioParams.applyFloor(algo, Math.max(specBright, bassBright));

                // Add sparks (white flash)
                if (sparksPixels[x] > 0.1) {
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
