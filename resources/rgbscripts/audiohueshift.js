/*
  Q Light Controller Plus
  audiohueshift.js

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
    algo.name = "Audio Hue Shift";
    algo.author = "QLC+ contributors";
    algo.acceptColors = 0;  // hue is audio-driven
    algo.usesAudio = true;
    algo.properties = new Array();

    AudioParams.installContinuous(algo, {gain: 7, reactivity: 7});

    algo.presetSpeed = 5;
    algo.properties.push(
      "name:presetSpeed|type:range|display:Speed|" +
      "values:1,10|write:setSpeed|read:getSpeed");
    algo.presetWaveScale = 4;
    algo.properties.push(
      "name:presetWaveScale|type:range|display:WaveScale|" +
      "values:1,10|write:setWaveScale|read:getWaveScale");
    algo.presetSaturation = 90;
    algo.properties.push(
      "name:presetSaturation|type:range|display:Saturation|" +
      "values:50,100|write:setSaturation|read:getSaturation");
    algo.presetMinBrightness = 40;
    algo.properties.push(
      "name:presetMinBrightness|type:range|display:MinBrightness|" +
      "values:10,100|write:setMinBrightness|read:getMinBrightness");

    algo.currentHue = null;
    algo.bassFilter = null;
    algo.midsFilter = null;
    algo.highsFilter = null;

    algo.setSpeed = function(_v) { algo.presetSpeed = clampInt(_v, 1, 10); };
    algo.getSpeed = function() { return algo.presetSpeed; };
    algo.setWaveScale = function(_v) { algo.presetWaveScale = clampInt(_v, 1, 10); };
    algo.getWaveScale = function() { return algo.presetWaveScale; };
    algo.setSaturation = function(_v) { algo.presetSaturation = clampInt(_v, 50, 100); };
    algo.getSaturation = function() { return algo.presetSaturation; };
    algo.setMinBrightness = function(_v) { algo.presetMinBrightness = clampInt(_v, 10, 100); };
    algo.getMinBrightness = function() { return algo.presetMinBrightness; };

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };

    function clamp(v, min, max) {
        return Math.max(min, Math.min(max, v));
    }

    function clampInt(v, min, max) {
        var parsed = parseInt(v);
        if (isNaN(parsed)) parsed = min;
        return clamp(parsed, min, max);
    }

    function wrapHue(hue) {
        return ((hue % 1.0) + 1.0) % 1.0;
    }

    function lerpHue(current, target, rate) {
        var delta = target - current;
        while (delta > 0.5) delta -= 1.0;
        while (delta < -0.5) delta += 1.0;
        return wrapHue(current + delta * rate);
    }

    function initState() {
        if (!algo.bassFilter) algo.bassFilter = AudioParams.createFilter(algo, 0.3);
        if (!algo.midsFilter) algo.midsFilter = AudioParams.createFilter(algo, 0.3);
        if (!algo.highsFilter) algo.highsFilter = AudioParams.createFilter(algo, 0.3);
        if (algo.currentHue === null) algo.currentHue = 0.0;
    }

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        initState();

        var map = LedFx.createMap(width, height);
        var gain = AudioParams.gainFactor(algo);
        var bass = 0;
        var mids = 0;
        var highs = 0;

        if (audio && audio.spectrum && audio.spectrum.length > 0) {
            bass = algo.bassFilter.update(LedFx.lows_power(audio)) * gain;
            mids = algo.midsFilter.update(LedFx.mids_power(audio)) * gain;
            highs = algo.highsFilter.update(LedFx.high_power(audio)) * gain;
        } else {
            bass = algo.bassFilter.update(0) * gain;
            mids = algo.midsFilter.update(0) * gain;
            highs = algo.highsFilter.update(0) * gain;
        }

        var totalPower = bass + mids + highs + 0.001;
        var targetHue = (bass * 0.0 + mids * 0.33 + highs * 0.66) / totalPower;
        var speedRate = 0.05 + (algo.presetSpeed / 10.0) * 0.45;
        algo.currentHue = lerpHue(algo.currentHue, targetHue, speedRate);

        var minBrightness = algo.presetMinBrightness / 100.0;
        var volume = (bass + mids + highs) / 3.0;
        // Steady brightness with subtle audio modulation — hue is the main show
        var brightness = clamp(minBrightness + volume * 0.15, minBrightness, 1.0);
        var waveScale = algo.presetWaveScale / 20.0;
        var saturation = algo.presetSaturation / 100.0;

        for (var y = 0; y < height; y++) {
            for (var x = 0; x < width; x++) {
                var wave = Math.sin(x * 0.3 + y * 0.2 + step * 0.05) * waveScale;
                var pixelHue = wrapHue(algo.currentHue + wave);
                var pixelBri = AudioParams.applyFloor(algo, clamp(brightness + wave * 0.2, minBrightness, 1.0));
                var color = LedFx.hsv2rgb(pixelHue, saturation, pixelBri);
                map[y][x] = LedFx.rgb(color[0], color[1], color[2]);
            }
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
