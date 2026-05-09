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
    algo.acceptColors = 3; // low/mid/high mel-bank gradient
    algo.usesAudio = true;
    algo.properties = new Array();

    AudioParams.installTrigger(algo, {gain: 5, reactivity: 5, sensitivity: 5});
    AudioParams.installBandPowerControls(algo);

    algo.presetDecay = 5;
    algo.properties.push(
      "name:presetDecay|type:range|display:Decay Speed|" +
      "values:1,10|write:setDecay|read:getDecay");

    algo.presetMode = 0;
    algo.properties.push(
      "name:presetMode|type:list|display:Trigger|" +
      "values:Beat,Bass,Mids,Highs,Volume,Kick|write:setMode|read:getMode");
    algo.presetTriggerMode = 0;
    algo.properties.push(
      "name:triggerMode|type:list|display:Trigger Mode|" +
      "values:Beat,Onset,Note|write:setTriggerMode|read:getTriggerMode");

    algo.setDecay = function(_v) { algo.presetDecay = parseInt(_v); };
    algo.getDecay = function() { return algo.presetDecay; };
    algo.setMode = function(_v) {
        if (_v === "Bass") algo.presetMode = 1;
        else if (_v === "Mids") algo.presetMode = 2;
        else if (_v === "Highs") algo.presetMode = 3;
        else if (_v === "Volume") algo.presetMode = 4;
        else if (_v === "Kick") algo.presetMode = 5;
        else algo.presetMode = 0;
    };
    algo.getMode = function() {
        if (algo.presetMode === 1) return "Bass";
        if (algo.presetMode === 2) return "Mids";
        if (algo.presetMode === 3) return "Highs";
        if (algo.presetMode === 4) return "Volume";
        if (algo.presetMode === 5) return "Kick";
        return "Beat";
    };
    algo.setTriggerMode = function(_v) {
        if (_v === "Onset") algo.presetTriggerMode = 1;
        else if (_v === "Note") algo.presetTriggerMode = 2;
        else algo.presetTriggerMode = 0;
    };
    algo.getTriggerMode = function() {
        return ["Beat", "Onset", "Note"][algo.presetTriggerMode];
    };
    var DEFAULT_BAND_COLORS = [0xFFFFFF, 0xFF8000, 0xFFFFFF];
    var activeColor = [255, 255, 255];
    var brightness = 0;

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() {
        return AudioParams.bandColors(algo, DEFAULT_BAND_COLORS).slice();
    };



    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var map = RGBUtil.createMap(width, height);
        if (!audio || !audio.mel || audio.mel.length === 0) return map;

        // Determine if we should flash
        var trigger = false;
        if (algo.presetTriggerMode === 1) {
            trigger = AudioParams.anyOnsetFired(audio);
        } else if (algo.presetTriggerMode === 2) {
            trigger = !!(audio.note && audio.note.noteOn);
        } else if (algo.presetMode === 0) {
            trigger = audio.triggers.beat.firedThisFrame;
        } else if (algo.presetMode === 1) {
            trigger = audio.triggers.low.firedThisFrame || AudioParams.kickFired(audio);
        } else if (algo.presetMode === 2) {
            trigger = audio.triggers.mid.firedThisFrame;
        } else if (algo.presetMode === 3) {
            trigger = audio.triggers.high.firedThisFrame;
        } else if (algo.presetMode === 4) {
            trigger = audio.triggers.volume.firedThisFrame;
        } else {
            trigger = AudioParams.kickFired(audio);
        }

        if (trigger) {
            var hitScale = Math.min(1.0, 0.4 + 0.6 * AudioParams.maxOnsetIntensity(audio));
            brightness = hitScale;
            activeColor = AudioParams.colorChannels(
                AudioParams.dominantBandColor(algo, audio, DEFAULT_BAND_COLORS));
        }

        // Decay
        var decayRate = algo.presetDecay / 50.0;
        brightness = Math.max(0, brightness - decayRate);

        // Render
        var r = activeColor[0] * brightness;
        var g = activeColor[1] * brightness;
        var b = activeColor[2] * brightness;
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
