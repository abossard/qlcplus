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

    var PITCH_CONF_THRESH = 0.5;
    var CENTROID_MIN_HZ = 200;
    var CENTROID_RANGE_HZ = 3800;
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

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var map = HSVUtil.createMap(width, height);
        if (!audio) return map;
        var bass = audio.power.low;
        var mids = audio.power.mid;
        var highs = audio.power.high;

        // Pitch-driven hue: map pitch to hue offset (one octave = full hue cycle)
        var pitch = audio.pitch.confidence < PITCH_CONF_THRESH ? 0 : audio.pitch.hz;
        var pitchHueOffset = 0;
        if (pitch > 0) {
            pitchHueOffset = ((Math.log2(pitch / 110) % 1) + 1) % 1;
        }

        var totalPower = bass + mids + highs + 0.001;
        var targetHue = (bass * 0.0 + mids * 0.33 + highs * 0.66) / totalPower;
        targetHue = wrapHue(targetHue + Math.max(0, Math.min(1, (audio.features.centroidHz - CENTROID_MIN_HZ) / CENTROID_RANGE_HZ)) * 0.33 + pitchHueOffset * 0.4);
        var speedRate = algo.presetSpeed;
        currentHue = lerpHue(currentHue, targetHue, speedRate);

        var minBrightness = algo.presetMinBrightness;
        var volume = (bass + mids + highs) / 3.0;
        // Steady brightness with subtle audio + beat pulse modulation
        var beatMod = audio.beat.cosPulse * BEAT_MOD;
        var brightness = clamp(minBrightness + volume * VOL_BRI_SCALE + beatMod, minBrightness, 1.0);
        var waveScale = algo.presetWaveScale;
        var saturation = algo.presetSaturation;

        for (var y = 0; y < height; y++) {
            for (var x = 0; x < width; x++) {
                var wave = Math.sin(x * WAVE_FREQ_X + y * WAVE_FREQ_Y + step * WAVE_TIME_FREQ) * waveScale;
                var pixelHue = wrapHue(currentHue + wave);
                var baseBri = clamp(brightness + wave * WAVE_BRI_AMP, minBrightness, 1.0);
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
