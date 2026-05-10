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

    algo.presetReactivity = 7;
    algo.presetFloor = 0;

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

    var currentHue = 0;

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

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var map = RGBUtil.createMap(width, height);
        if (!audio) return map;
        var bass = audio.power.low;
        var mids = audio.power.mid;
        var highs = audio.power.high;

        // Pitch-driven hue: map pitch to hue offset (one octave = full hue cycle)
        var pitch = audio.pitch.confidence < 0.5 ? 0 : audio.pitch.hz;
        var pitchHueOffset = 0;
        if (pitch > 0) {
            pitchHueOffset = ((Math.log2(pitch / 110) % 1) + 1) % 1;
        }

        var totalPower = bass + mids + highs + 0.001;
        var targetHue = (bass * 0.0 + mids * 0.33 + highs * 0.66) / totalPower;
        targetHue = wrapHue(targetHue + Math.max(0, Math.min(1, (audio.features.centroidHz - 200) / 3800)) * 0.33 + pitchHueOffset * 0.4);
        var speedRate = 0.05 + (algo.presetSpeed / 10.0) * 0.45;
        currentHue = lerpHue(currentHue, targetHue, speedRate);

        var minBrightness = algo.presetMinBrightness / 100.0;
        var volume = (bass + mids + highs) / 3.0;
        // Steady brightness with subtle audio + beat pulse modulation
        var beatMod = audio.beat.cosPulse * 0.12;
        var brightness = clamp(minBrightness + volume * 0.15 + beatMod, minBrightness, 1.0);
        var waveScale = algo.presetWaveScale / 20.0;
        var saturation = algo.presetSaturation / 100.0;

        for (var y = 0; y < height; y++) {
            for (var x = 0; x < width; x++) {
                var wave = Math.sin(x * 0.3 + y * 0.2 + step * 0.05) * waveScale;
                var pixelHue = wrapHue(currentHue + wave);
                var baseBri = clamp(brightness + wave * 0.2, minBrightness, 1.0);
                var pixelBri = algo.presetFloor/100 + (1 - algo.presetFloor/100) * baseBri;
                var color = RGBUtil.hsv2rgb(pixelHue, saturation, pixelBri);
                map[y][x] = RGBUtil.rgb(color[0], color[1], color[2]);
            }
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
