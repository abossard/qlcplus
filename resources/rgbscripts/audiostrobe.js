/*
  Q Light Controller Plus
  audiostrobe.js

  Copyright (c) QLC+ contributors
  Ported from LedFX "BPM Strobe" effect (MIT License)

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
    algo.name = "Audio Strobe";
    algo.author = "Ported from LedFx";
    algo.acceptColors = 2;
    algo.usesAudio = true;
    algo.properties = new Array();

    AudioParams.installTrigger(algo, {gain: 5, reactivity: 5, sensitivity: 5});

    algo.presetDecay = 5;
    algo.properties.push(
      "name:presetDecay|type:range|display:Decay Speed|" +
      "values:1,10|write:setDecay|read:getDecay");

    algo.presetMode = 0;
    algo.properties.push(
      "name:presetMode|type:list|display:Trigger|" +
      "values:Beat,Bass,Mids,Highs,Volume|write:setMode|read:getMode");

    algo.presetRandomColor = 0;
    algo.properties.push(
      "name:presetRandomColor|type:list|display:Random Color|" +
      "values:Off,On|write:setRandomColor|read:getRandomColor");

    algo.setDecay = function(_v) { algo.presetDecay = parseInt(_v); };
    algo.getDecay = function() { return algo.presetDecay; };
    algo.setMode = function(_v) {
        if (_v === "Bass") algo.presetMode = 1;
        else if (_v === "Mids") algo.presetMode = 2;
        else if (_v === "Highs") algo.presetMode = 3;
        else if (_v === "Volume") algo.presetMode = 4;
        else algo.presetMode = 0;
    };
    algo.getMode = function() {
        if (algo.presetMode === 1) return "Bass";
        if (algo.presetMode === 2) return "Mids";
        if (algo.presetMode === 3) return "Highs";
        if (algo.presetMode === 4) return "Volume";
        return "Beat";
    };
    algo.setRandomColor = function(_v) { algo.presetRandomColor = (_v === "On") ? 1 : 0; };
    algo.getRandomColor = function() { return algo.presetRandomColor ? "On" : "Off"; };

    var strobeColor = [255, 255, 255];
    var bgColor = [0, 0, 0];
    var activeColor = [255, 255, 255];
    var brightness = 0;

    algo.rgbMapStepCount = function(width, height) { return 1; };

    algo.rgbMapSetColors = function(rawColors) {
        if (rawColors && rawColors.length >= 1)
            strobeColor = [(rawColors[0] >> 16) & 0xFF, (rawColors[0] >> 8) & 0xFF, rawColors[0] & 0xFF];
        if (rawColors && rawColors.length >= 2)
            bgColor = [(rawColors[1] >> 16) & 0xFF, (rawColors[1] >> 8) & 0xFF, rawColors[1] & 0xFF];
    };

    algo.rgbMapGetColors = function() {
        return [RGBUtil.rgb(strobeColor[0], strobeColor[1], strobeColor[2]),
                RGBUtil.rgb(bgColor[0], bgColor[1], bgColor[2])];
    };



    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var map = RGBUtil.createMap(width, height);
        if (!audio || !audio.mel || audio.mel.length === 0) return map;

        // Determine if we should flash
        var trigger = false;
        if (algo.presetMode === 0) {
            trigger = audio.triggers.beat.firedThisFrame;
        } else if (algo.presetMode === 1) {
            trigger = audio.triggers.bass.firedThisFrame;
        } else if (algo.presetMode === 2) {
            trigger = audio.triggers.mid.firedThisFrame;
        } else if (algo.presetMode === 3) {
            trigger = audio.triggers.high.firedThisFrame;
        } else {
            trigger = audio.triggers.volume.active;
        }

        if (trigger) {
            brightness = 1.0;
            if (algo.presetRandomColor) {
                var c = RGBUtil.hsv2rgb(Math.random(), 1, 1);
                activeColor = [c[0], c[1], c[2]];
            } else {
                activeColor = strobeColor;
            }
        }

        // Decay
        var decayRate = algo.presetDecay / 50.0;
        brightness = Math.max(0, brightness - decayRate);

        // Render
        var r = bgColor[0] + (activeColor[0] - bgColor[0]) * brightness;
        var g = bgColor[1] + (activeColor[1] - bgColor[1]) * brightness;
        var b = bgColor[2] + (activeColor[2] - bgColor[2]) * brightness;
        var packed = RGBUtil.rgb(r, g, b);

        for (var y = 0; y < height; y++)
            for (var x = 0; x < width; x++)
                map[y][x] = packed;

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
