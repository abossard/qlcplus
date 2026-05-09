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

    AudioParams.installContinuous(algo, {gain: 5, reactivity: 5});
    AudioParams.installBandPowerControls(algo);

    algo.presetSpeed = 5;
    algo.properties.push(
      "name:presetSpeed|type:range|display:Speed|" +
      "values:1,10|write:setSpeed|read:getSpeed");
    algo.presetArms = 3;
    algo.properties.push(
      "name:presetArms|type:range|display:Spiral Arms|" +
      "values:1,8|write:setArms|read:getArms");
    algo.presetTightness = 5;
    algo.properties.push(
      "name:presetTightness|type:range|display:Tightness|" +
      "values:1,10|write:setTightness|read:getTightness");

    algo.setSpeed = function(_v) { algo.presetSpeed = parseInt(_v); };
    algo.getSpeed = function() { return algo.presetSpeed; };
    algo.setArms = function(_v) { algo.presetArms = parseInt(_v); };
    algo.getArms = function() { return algo.presetArms; };
    algo.setTightness = function(_v) { algo.presetTightness = parseInt(_v); };
    algo.getTightness = function() { return algo.presetTightness; };

    var DEFAULT_BAND_COLORS = [0xFF0080, 0x8064D8, 0x00C8FF];
    var lowsFilter = null;
    var angle = 0;
    var lastTime = 0;
    var initialized = false;

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() {
        return AudioParams.bandColors(algo, DEFAULT_BAND_COLORS).slice();
    };


    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        if (!initialized) {
            lowsFilter = new AudioDSP.Filter(0.05, AudioParams.filterRise(algo));
            lastTime = Date.now();
            initialized = true;
        }

        var map = RGBUtil.createMap(width, height);
        if (!audio || !audio.mel || audio.mel.length === 0) return map;

        var now = Date.now();
        var dt = (now - lastTime) / 1000.0;
        lastTime = now;
        if (dt <= 0 || dt > 0.2) dt = 0.02;

        var power = lowsFilter.update(audio.lows);
        var speed = algo.presetSpeed / 5.0;
        angle += dt * speed * (1 + power * algo.presetReactivity / 3.0);

        var cx = width / 2;
        var cy = height / 2;
        var maxDist = Math.sqrt(cx * cx + cy * cy);
        var arms = algo.presetArms;
        var tightness = algo.presetTightness / 3.0;
        var blended = AudioParams.colorChannels(AudioParams.blendBandColors(algo, audio, DEFAULT_BAND_COLORS));
        var beatBoost = 1.0 + 0.20 * AudioParams.beatPulse(audio);
        var noveltyBoost = 1.0 + 0.30 * AudioParams.melNoveltyAvg(audio);

        for (var y = 0; y < height; y++) {
            for (var x = 0; x < width; x++) {
                var dx = x - cx + 0.5;
                var dy = y - cy + 0.5;
                var dist = Math.sqrt(dx * dx + dy * dy);
                var normDist = dist / maxDist;

                // Angle of pixel from center
                var pixAngle = Math.atan2(dy, dx);

                // Spiral: angle offset increases with distance
                var spiral = pixAngle + dist * tightness * 0.3 - angle;

                // Create arm pattern
                var armVal = Math.sin(spiral * arms) * 0.5 + 0.5;

                // Brightness: arms visible, fades toward edge
                var bright = AudioParams.applyPunch(AudioParams.applyFloor(algo, Math.min(1, armVal * power * (1 - normDist * 0.5))), audio) * beatBoost * noveltyBoost;

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
