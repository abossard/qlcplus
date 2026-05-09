/*
  Q Light Controller Plus
  audiospectrum.js

  Copyright (c) QLC+ contributors
  Ported from LedFX "Spectrum" effect (MIT License)

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
    algo.name = "Audio Spectrum";
    algo.author = "Ported from LedFx";
    algo.acceptColors = 3; // low/mid/high mel-bank gradient
    algo.usesAudio = true;
    algo.properties = new Array();

    AudioParams.installContinuous(algo, {gain: 7, reactivity: 7});

    algo.presetMode = 0;
    algo.properties.push(
      "name:presetMode|type:list|display:Color Mode|" +
      "values:Gradient,Rainbow,RGB Mix|write:setMode|read:getMode");


    algo.presetSmoothing = 5;
    algo.properties.push(
      "name:presetSmoothing|type:range|display:Smoothing|" +
      "values:1,10|write:setSmoothing|read:getSmoothing");

    algo.setMode = function(_v) {
        if (_v === "Rainbow") algo.presetMode = 1;
        else if (_v === "RGB Mix") algo.presetMode = 2;
        else algo.presetMode = 0;
    };
    algo.getMode = function() {
        if (algo.presetMode === 1) return "Rainbow";
        if (algo.presetMode === 2) return "RGB Mix";
        return "Gradient";
    };
    algo.setSmoothing = function(_v) { algo.presetSmoothing = parseInt(_v); filterDirty = true; };
    algo.getSmoothing = function() { return algo.presetSmoothing; };

    // Default gradient stops if neither user palette nor algo.gradientColors
    // provide any (red -> blue). algo.gradientColors is auto-injected by C++
    // every frame from the matrix's color stops.
    var DEFAULT_GRADIENT = [0xFF0000, 0x0000FF];
    var filter = null;
    var prevBands = null;
    var peakValues = null;
    var peakHolds = null;
    var initialized = false;
    var filterDirty = false;
    var gradientLut = null;
    var lutWidth = -1;
    var lutSig = "";

    function init() {
        var decay = algo.presetSmoothing / 15.0;
        var rise = 0.1 + algo.presetReactivity * 0.09;
        filter = new AudioDSP.Filter(decay, rise);
        peakValues = null;
        peakHolds = null;
        initialized = true;
        filterDirty = false;
    }
    function unpackColor(packed) { return AudioParams.colorChannels(packed); }

    algo.rgbMapStepCount = function(width, height) { return 1; };

    // Required by apiVersion 3 loader; we ignore the array because
    // algo.gradientColors is auto-injected by C++ before every rgbMap() call.
    algo.rgbMapSetColors = function(rawColors) { };

    algo.rgbMapGetColors = function() {
        return algo.gradientColors ? algo.gradientColors.slice() : DEFAULT_GRADIENT.slice();
    };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        if (!initialized || filterDirty) init();
        var map = RGBUtil.createMap(width, height);
        
        var melSrc = AudioParams.fullMel(audio);
        if (!melSrc || melSrc.length === 0) return map;

        var effectiveWidth = (typeof algo.displayWidth !== 'undefined') ? algo.displayWidth : width;
        var bands = RGBUtil.interpolate(melSrc, effectiveWidth);
        for (var i = 0; i < bands.length; i++)
            bands[i] = Math.min(1, bands[i]);

        var filtered = filter.updateArray(bands);
        if (!prevBands || prevBands.length !== bands.length) prevBands = bands.slice();
        if (!peakValues || peakValues.length !== bands.length) {
            peakValues = new Array(bands.length);
            peakHolds = new Array(bands.length);
            for (var pi = 0; pi < bands.length; pi++) {
                peakValues[pi] = 0;
                peakHolds[pi] = 0;
            }
        }
        var onset = AudioParams.anyOnsetFired(audio);

        // Build/refresh per-column gradient LUT lazily for Gradient mode.
        // Rebuild also when the auto-injected gradient changes (signature check).
        var stops = (algo.gradientColors && algo.gradientColors.length > 0)
            ? algo.gradientColors
            : DEFAULT_GRADIENT;
        if (algo.presetMode === 0) {
            var sig = stops.length + ":" + stops.join(",");
            if (gradientLut === null || lutWidth !== width || lutSig !== sig) {
                gradientLut = RGBUtil.gradientLut(stops, width);
                lutWidth = width;
                lutSig = sig;
            }
        }

        for (var x = 0; x < Math.min(width, filtered.length); x++) {
            var val = Math.min(1, filtered[x]);
            var diff = Math.abs(bands[x] - prevBands[x]);
            var barHeight = Math.round(val * height);
            if (val > 0.01)
                barHeight = Math.max(1, barHeight);
            var t = x / Math.max(1, width - 1);
            if (AudioParams.isDownbeat(audio)) {
                peakValues[x] *= 0.5;
            }
            if (val >= peakValues[x]) {
                peakValues[x] = val;
                peakHolds[x] = 5;
            } else if (onset && bands[x] > 0.05) {
                peakValues[x] = Math.max(peakValues[x], val);
                peakHolds[x] = 5;
            } else if (peakHolds[x] > 0) {
                peakHolds[x]--;
            } else {
                peakValues[x] *= 0.95;
            }

            for (var dy = 0; dy < barHeight; dy++) {
                var y = height - 1 - dy;
                if (y < 0) break;

                var r, g, b;
                if (algo.presetMode === 1) {
                    // Rainbow: hue based on column position
                    var c = RGBUtil.hsv2rgb(t, 1, 1);
                    r = c[0]; g = c[1]; b = c[2];
                } else if (algo.presetMode === 2) {
                    // RGB Mix: R=filtered, G=diff, B=smoothed
                    r = Math.min(255, val * 1000);
                    g = Math.min(255, diff * 2000);
                    b = Math.min(255, filtered[x] * 800);
                } else {
                    // N-stop gradient sampled per column from the user's colors
                    var packed = gradientLut[x];
                    r = (packed >> 16) & 0xFF;
                    g = (packed >> 8) & 0xFF;
                    b = packed & 0xFF;
                }
                // Brightness from height position
                var bright = AudioParams.applyFloor(algo, (dy / height) * 0.5 + 0.5);
                map[y][x] = RGBUtil.rgb(r * bright, g * bright, b * bright);
            }
            if (peakValues[x] > 0.01) {
                var peakY = height - 1 - Math.floor(peakValues[x] * height);
                if (peakY >= 0 && peakY < height)
                    map[peakY][x] = RGBUtil.rgb(255, 255, 255);
            }
        }
        prevBands = bands.slice();
        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
