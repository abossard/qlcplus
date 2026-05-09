/*
  Q Light Controller Plus
  audioscroll.js

  Copyright (c) QLC+ contributors
  Ported from LedFX "Scroll" effect (MIT License)

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
    algo.name = "Audio Scroll";
    algo.author = "Ported from LedFx";
    algo.acceptColors = 3; // low/mid/high mel-bank gradient
    algo.usesAudio = true;
    algo.properties = new Array();

    AudioParams.installContinuous(algo, {gain: 5, reactivity: 5});

    algo.presetDecay = 5;
    algo.properties.push(
      "name:presetDecay|type:range|display:Decay|" +
      "values:1,10|write:setDecay|read:getDecay");
    algo.presetDirection = 0;
    algo.properties.push(
      "name:presetDirection|type:list|display:Direction|" +
      "values:Down,Up,Right,Left|write:setDirection|read:getDirection");
    algo.presetColorMode = 0;
    algo.properties.push(
      "name:presetColorMode|type:list|display:Color Mode|" +
      "values:Gradient,Rainbow|write:setColorMode|read:getColorMode");
    AudioParams.installBandPowerControls(algo);

    algo.setDecay = function(_v) { algo.presetDecay = parseInt(_v); };
    algo.getDecay = function() { return algo.presetDecay; };
    algo.setDirection = function(_v) {
        if (_v === "Up") algo.presetDirection = 1;
        else if (_v === "Right") algo.presetDirection = 2;
        else if (_v === "Left") algo.presetDirection = 3;
        else algo.presetDirection = 0;
    };
    algo.getDirection = function() {
        return ["Down", "Up", "Right", "Left"][algo.presetDirection];
    };
    algo.setColorMode = function(_v) {
        if (_v === "Rainbow") algo.presetColorMode = 1;
        else algo.presetColorMode = 0;
    };
    algo.getColorMode = function() {
        return ["Gradient", "Rainbow"][algo.presetColorMode];
    };

    var DEFAULT_GRADIENT = [0xFF0000, 0xFF8000, 0xFFFF00, 0x00FF80, 0x4080FF];
    var gradientLut = null;
    var lutWidth = -1;
    var lutSig = "";
    var history = null; // 2D buffer of packed colors
    var initialized = false;
    function bandScaleForColumn(x, width) { return AudioParams.bandScaleForColumn(algo, x, width); }
    function unpackColor(packed) { return AudioParams.colorChannels(packed); }

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() {
        return algo.gradientColors ? algo.gradientColors.slice() : DEFAULT_GRADIENT.slice();
    };


    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var isVertical = (algo.presetDirection <= 1); // Down or Up
        var effectiveWidth = (typeof algo.displayWidth !== 'undefined') ? algo.displayWidth : width;
        var scrollLen = isVertical ? height : width;
        var bandLen = isVertical ? effectiveWidth : height;

        if (!initialized || !history || history.length !== scrollLen || !history[0] || history[0].length !== bandLen) {
            history = new Array(scrollLen);
            for (var i = 0; i < scrollLen; i++) {
                history[i] = new Array(bandLen);
                for (var j = 0; j < bandLen; j++)
                    history[i][j] = 0;
            }
            initialized = true;
        }

        var map = RGBUtil.createMap(width, height);
        
        var melSrc = AudioParams.fullMel(audio);
        if (!melSrc || melSrc.length === 0) return map;

        // Decay rate
        var decay = 1 - algo.presetDecay / 15.0;

        // Get current spectrum for new row
        var bands = RGBUtil.interpolate(melSrc, bandLen);
        for (var bi = 0; bi < bands.length; bi++)
            bands[bi] = Math.min(1, bands[bi]) * bandScaleForColumn(bi, bandLen);

        var stops = (algo.gradientColors && algo.gradientColors.length > 0) ? algo.gradientColors : DEFAULT_GRADIENT;
        var sig = stops.length + ":" + stops.join(",");
        if (gradientLut === null || lutWidth !== bandLen || lutSig !== sig) {
            gradientLut = RGBUtil.gradientLut(stops, bandLen);
            lutWidth = bandLen;
            lutSig = sig;
        }

        // Build new row of colors
        var beatMod = 1 + AudioParams.beatPulse(audio) * 0.15;
        var newRow = new Array(bandLen);
        for (var i = 0; i < bandLen; i++) {
            var val = AudioParams.applyFloor(algo, Math.min(1, bands[i])) * beatMod;
            var t = i / Math.max(1, bandLen - 1);
            var r, g, b;

            if (algo.presetColorMode === 1) {
                // Rainbow
                var c = RGBUtil.hsv2rgb(t, 1, val);
                r = c[0]; g = c[1]; b = c[2];
            } else {
                // N-stop gradient sampled per column
                var packed = gradientLut[i];
                r = ((packed >> 16) & 0xFF) * val;
                g = ((packed >> 8) & 0xFF) * val;
                b = (packed & 0xFF) * val;
            }
            newRow[i] = RGBUtil.rgb(r, g, b);
        }

        // Scroll: shift history, add new row
        var reverse = (algo.presetDirection === 1 || algo.presetDirection === 3);
        if (reverse) {
            // Shift toward end, new row at start
            for (var s = scrollLen - 1; s > 0; s--)
                history[s] = history[s - 1];
            history[0] = newRow;
        } else {
            // Shift toward start, new row at end
            for (var s = 0; s < scrollLen - 1; s++)
                history[s] = history[s + 1];
            history[scrollLen - 1] = newRow;
        }

        // Apply decay to older rows
        for (var s = 0; s < scrollLen; s++) {
            var age = reverse ? s : (scrollLen - 1 - s);
            var fadeFactor = Math.pow(decay, age);
            for (var j = 0; j < bandLen; j++) {
                var px = history[s][j];
                var pr = ((px >> 16) & 0xFF) * fadeFactor;
                var pg = ((px >> 8) & 0xFF) * fadeFactor;
                var pb = (px & 0xFF) * fadeFactor;
                history[s][j] = RGBUtil.rgb(pr, pg, pb);
            }
        }

        // Map to grid
        if (isVertical) {
            for (var y = 0; y < height; y++)
                for (var x = 0; x < width; x++)
                    map[y][x] = (y < scrollLen && x < bandLen) ? history[y][x] : 0;
        } else {
            for (var y = 0; y < height; y++)
                for (var x = 0; x < width; x++)
                    map[y][x] = (x < scrollLen && y < bandLen) ? history[x][y] : 0;
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
