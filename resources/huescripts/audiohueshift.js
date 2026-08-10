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

    algo.presetSpeed = 0.275;
    algo.properties.push(
      "name:presetSpeed|type:float|display:Speed|" +
      "write:setSpeed|read:getSpeed");
    algo.presetWaveScale = 0.2;
    algo.properties.push(
      "name:presetWaveScale|type:float|display:WaveScale|" +
      "write:setWaveScale|read:getWaveScale");
    algo.presetSaturation = 0.9;
    algo.properties.push(
      "name:presetSaturation|type:float|display:Saturation|" +
      "write:setSaturation|read:getSaturation");
    algo.presetMinBrightness = 0.4;
    algo.properties.push(
      "name:presetMinBrightness|type:float|display:MinBrightness|" +
      "write:setMinBrightness|read:getMinBrightness");

    var BEAT_MOD = 0.12;
    var VOL_BRI_SCALE = 0.15;
    var WAVE_FREQ_X = 0.3;
    var WAVE_FREQ_Y = 0.2;
    var WAVE_TIME_FREQ = 0.05;
    var WAVE_BRI_AMP = 0.2;

    var currentHue = 0;

    algo.setSpeed = function(_v) { algo.presetSpeed = parseFloat(_v); };
    algo.getSpeed = function() { return algo.presetSpeed; };
    algo.setWaveScale = function(_v) { algo.presetWaveScale = parseFloat(_v); };
    algo.getWaveScale = function() { return algo.presetWaveScale; };
    algo.setSaturation = function(_v) { algo.presetSaturation = parseFloat(_v); };
    algo.getSaturation = function() { return algo.presetSaturation; };
    algo.setMinBrightness = function(_v) { algo.presetMinBrightness = parseFloat(_v); };
    algo.getMinBrightness = function() { return algo.presetMinBrightness; };

    algo.presetSmoothing = 5;
    algo.properties.push(
      "name:presetSmoothing|type:range|display:Smoothing|" +
      "values:1,10|write:setSmoothing|read:getSmoothing");
    algo.setSmoothing = function(_v) { algo.presetSmoothing = parseInt(_v); };
    algo.getSmoothing = function() { return algo.presetSmoothing; };

    var smoothVolume = 0;

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };



    function wrapHue(hue) {
        return ((hue % 1.0) + 1.0) % 1.0;
    }

    function lerpHue(current, target, rate) {
        var delta = target - current;
        while (delta > 0.5) delta -= 1.0;
        while (delta < -0.5) delta += 1.0;
        return wrapHue(current + delta * rate);
    }

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var map = HSVUtil.createMap(width, height);
        if (!audio) return map;
        var bass = audio.low;
        var mids = audio.mid;
        var highs = audio.high;

        // Drive hue from low/mid/high proportions
        var totalPower = bass + mids + highs + 0.001;
        var targetHue = (bass * 0.0 + mids * 0.33 + highs * 0.66) / totalPower;
        // High-frequency content shifts hue further
        var highRatio = highs / totalPower;
        targetHue = wrapHue(targetHue + highRatio * 0.33);
        var speedRate = algo.presetSpeed;
        currentHue = lerpHue(currentHue, targetHue, speedRate);

        var minBrightness = algo.presetMinBrightness;
        var rawVolume = (bass + mids + highs) / 3.0;
        // Asymmetric EMA smoothing (fast attack, slow decay)
        var smoothing = algo.presetSmoothing / 10.0;
        var riseAlpha = 0.5 * (1 - smoothing) + 0.05;
        var decayAlpha = 0.02 + 0.03 * (1 - smoothing);
        smoothVolume += (rawVolume > smoothVolume ? riseAlpha : decayAlpha) * (rawVolume - smoothVolume);
        var volume = smoothVolume;
        // Steady brightness with subtle audio + beat pulse modulation
        var beatMod = audio.cosPulse * BEAT_MOD;
        var brightness = Math.max(minBrightness, Math.min(1.0, minBrightness + volume * VOL_BRI_SCALE + beatMod));
        var waveScale = algo.presetWaveScale;
        var saturation = algo.presetSaturation;

        for (var y = 0; y < height; y++) {
            for (var x = 0; x < width; x++) {
                var wave = Math.sin(x * WAVE_FREQ_X + y * WAVE_FREQ_Y + step * WAVE_TIME_FREQ) * waveScale;
                var pixelHue = wrapHue(currentHue + wave);
                var baseBri = Math.max(minBrightness, Math.min(1.0, brightness + wave * WAVE_BRI_AMP));
                var pixelBri = baseBri;
                HSVUtil.setPixel(map, width, x, y, pixelHue, saturation, pixelBri);
            }
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
