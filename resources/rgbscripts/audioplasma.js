/*
  Q Light Controller Plus
  audioplasma.js

  Copyright (c) QLC+ contributors
  Ported from LedFX "Plasma2d" effect (MIT License)

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
    algo.name = "Audio Plasma";
    algo.author = "Ported from LedFx";
    algo.acceptColors = 3; // low/mid/high mel-bank gradient
    algo.usesAudio = true;
    algo.properties = new Array();

    algo.presetReactivity = 2;
    algo.properties.push(
      "name:presetReactivity|type:range|display:Reactivity|" +
      "values:1,10|write:setReactivity|read:getReactivity");
    algo.presetSpeed = 5;
    algo.properties.push(
      "name:presetSpeed|type:range|display:Speed|" +
      "values:1,10|write:setSpeed|read:getSpeed");
    algo.presetDensity = 5;
    algo.properties.push(
      "name:presetDensity|type:range|display:Density|" +
      "values:1,10|write:setDensity|read:getDensity");
    algo.presetTwist = 4;
    algo.properties.push(
      "name:presetTwist|type:range|display:Twist|" +
      "values:1,10|write:setTwist|read:getTwist");

    algo.setSpeed = function(_v) { algo.presetSpeed = parseInt(_v); };
    algo.getSpeed = function() { return algo.presetSpeed; };
    algo.setDensity = function(_v) { algo.presetDensity = parseInt(_v); };
    algo.getDensity = function() { return algo.presetDensity; };
    algo.setTwist = function(_v) { algo.presetTwist = parseInt(_v); };
    algo.getTwist = function() { return algo.presetTwist; };

    algo.setReactivity = function(_v) { algo.presetReactivity = parseInt(_v); };
    algo.getReactivity = function() { return algo.presetReactivity; };
    var DEFAULT_BAND_COLORS = [0xFF0080, 0x8040E0, 0x0080FF];
    var elapsedSec = 0;

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() {
        return AudioColors.bands(algo).slice();
    };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var map = RGBUtil.createMap(width, height);
        if (!audio) return map;

        var dt = audio.timing.consumerDtMs / 1000.0;

        var power = audio.power.low;
        var speed = algo.presetSpeed / 5.0;
        var noveltyMax = audio.spectrum.novelty.max;
        elapsedSec += dt * speed * (1 + power * algo.presetReactivity / 5.0) * (1 + 0.5 * noveltyMax);

        var density = 0.01 + (power * algo.presetDensity / 10.0);
        var twist = algo.presetTwist / 100.0;
        var radius = 0.2;
        var blendedPacked = AudioColors.blendByPower(algo, audio);
        var blended = [(blendedPacked >> 16) & 0xFF, (blendedPacked >> 8) & 0xFF, blendedPacked & 0xFF];
        var beatBoost = 1.0 + 0.20 * audio.beat.cosPulse;
        var noveltyBoost = AudioColors.noveltyBoost(audio);
        var fluxPunch = AudioColors.fluxPunch(audio);

        // True 2D plasma: different value at every (x,y)
        for (var y = 0; y < height; y++) {
            var py = y * density;
            for (var x = 0; x < width; x++) {
                var px = x * density;

                // Three overlapping sine waves for plasma pattern
                var v1 = Math.sin(px * 0.1 + elapsedSec) * Math.cos(py * 0.1 - elapsedSec);
                var v2 = Math.sin((px * 0.1 + py * twist + elapsedSec) * 2.5);
                var v3 = Math.sin(Math.sqrt(px * px + py * py) * radius - elapsedSec);

                // Combine and normalize to 0-1
                var plasma = (v1 + v2 + v3 + 3) / 6.0;

                var floored = plasma;
                var brightness = Math.min(1, floored * fluxPunch) * beatBoost * noveltyBoost;
                map[y][x] = RGBUtil.rgb(
                    blended[0] * brightness,
                    blended[1] * brightness,
                    blended[2] * brightness);
            }
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
