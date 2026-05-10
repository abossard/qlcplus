/*
  Q Light Controller Plus
  audiovortex.js

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
    algo.name = "Audio Vortex";
    algo.author = "QLC+ contributors";
    algo.acceptColors = 3; // low/mid/high mel-bank gradient
    algo.usesAudio = true;
    algo.properties = new Array();

    algo.presetReactivity = 1.67;
    algo.properties.push(
      "name:presetReactivity|type:float|display:Reactivity|" +
      "write:setReactivity|read:getReactivity");
    algo.presetSpeed = 0.5;
    algo.properties.push(
      "name:presetSpeed|type:float|display:Speed (cyc/beat)|" +
      "write:setSpeed|read:getSpeed");
    algo.presetArms = 3;
    algo.properties.push(
      "name:presetArms|type:range|display:Spiral Arms|" +
      "values:1,8|write:setArms|read:getArms");
    algo.presetTightness = 1.67;
    algo.properties.push(
      "name:presetTightness|type:float|display:Tightness|" +
      "write:setTightness|read:getTightness");

    algo.setSpeed = function(_v) { algo.presetSpeed = parseFloat(_v); };
    algo.getSpeed = function() { return algo.presetSpeed; };
    algo.setArms = function(_v) { algo.presetArms = parseInt(_v); };
    algo.getArms = function() { return algo.presetArms; };
    algo.setTightness = function(_v) { algo.presetTightness = parseFloat(_v); };
    algo.getTightness = function() { return algo.presetTightness; };

    algo.setReactivity = function(_v) { algo.presetReactivity = parseFloat(_v); };
    algo.getReactivity = function() { return algo.presetReactivity; };

    var BEAT_PULSE_AMP = 0.20;
    var SPIRAL_FREQ = 0.3;
    var DIST_FADE = 0.5;
    var vortexState = { phase: 0 };

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() {
        return AudioColors.bands(algo).slice();
    };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var map = RGBUtil.createMap(width, height);
        if (!audio) return map;

        var dtMs = audio.timing.consumerDtMs;
        var bpm = (audio && audio.beat) ? audio.beat.bpm : 0;

        var power = audio.power.low;
        var speed = algo.presetSpeed;
        var angle = RGBUtil.beatTime(speed * (1 + power * algo.presetReactivity), vortexState, bpm, dtMs) * Math.PI * 2;

        var cx = width / 2;
        var cy = height / 2;
        var maxDist = Math.sqrt(cx * cx + cy * cy);
        var arms = algo.presetArms;
        var tightness = algo.presetTightness;
        var blendedPacked = AudioColors.blendByPower(algo, audio);
        var blended = [(blendedPacked >> 16) & 0xFF, (blendedPacked >> 8) & 0xFF, blendedPacked & 0xFF];
        var beatBoost = 1.0 + BEAT_PULSE_AMP * audio.beat.cosPulse;
        var noveltyBoost = AudioColors.noveltyBoost(audio);
        var fluxPunch = AudioColors.fluxPunch(audio);

        for (var y = 0; y < height; y++) {
            for (var x = 0; x < width; x++) {
                var dx = x - cx + 0.5;
                var dy = y - cy + 0.5;
                var dist = Math.sqrt(dx * dx + dy * dy);
                var normDist = dist / maxDist;

                // Angle of pixel from center
                var pixAngle = Math.atan2(dy, dx);

                // Spiral: angle offset increases with distance
                var spiral = pixAngle + dist * tightness * SPIRAL_FREQ - angle;

                // Create arm pattern
                var armVal = Math.sin(spiral * arms) * 0.5 + 0.5;

                // Brightness: arms visible, fades toward edge
                var baseBright = Math.min(1, armVal * power * (1 - normDist * DIST_FADE));
                var floored = baseBright;
                var bright = Math.min(1, floored * fluxPunch) * beatBoost * noveltyBoost;

                map[y][x] = RGBUtil.rgb(
                    blended[0] * bright,
                    blended[1] * bright,
                    blended[2] * bright);
            }
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
