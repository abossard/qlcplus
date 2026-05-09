/*
  Q Light Controller Plus
  audiotunnel.js

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
    algo.name = "Audio Tunnel";
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
    algo.presetRings = 5;
    algo.properties.push(
      "name:presetRings|type:range|display:Ring Count|" +
      "values:2,10|write:setRings|read:getRings");
    algo.presetShape = 0;
    algo.properties.push(
      "name:presetShape|type:list|display:Shape|" +
      "values:Circle,Diamond,Square|write:setShape|read:getShape");

    algo.setSpeed = function(_v) { algo.presetSpeed = parseInt(_v); };
    algo.getSpeed = function() { return algo.presetSpeed; };
    algo.setRings = function(_v) { algo.presetRings = parseInt(_v); };
    algo.getRings = function() { return algo.presetRings; };
    algo.setShape = function(_v) {
        if (_v === "Diamond") algo.presetShape = 1;
        else if (_v === "Square") algo.presetShape = 2;
        else algo.presetShape = 0;
    };
    algo.getShape = function() {
        return ["Circle", "Diamond", "Square"][algo.presetShape];
    };

    var DEFAULT_BAND_COLORS = [0x0080FF, 0x8040D0, 0xFF0080];
    var lowsFilter = null;
    var phase = 0;
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
        phase += dt * speed * (1 + power * algo.presetReactivity / 3.0);

        var cx = width / 2;
        var cy = height / 2;
        var maxDist = Math.sqrt(cx * cx + cy * cy);
        var ringCount = algo.presetRings;
        var blended = AudioParams.colorChannels(AudioParams.blendBandColors(algo, audio, DEFAULT_BAND_COLORS));
        var beatBoost = 1.0 + 0.20 * AudioParams.beatPulse(audio);
        var noveltyBoost = 1.0 + 0.30 * AudioParams.melNoveltyAvg(audio);

        for (var y = 0; y < height; y++) {
            for (var x = 0; x < width; x++) {
                var dx = x - cx + 0.5;
                var dy = y - cy + 0.5;

                // Distance based on shape
                var dist;
                if (algo.presetShape === 1) // Diamond
                    dist = Math.abs(dx) + Math.abs(dy);
                else if (algo.presetShape === 2) // Square
                    dist = Math.max(Math.abs(dx), Math.abs(dy));
                else // Circle
                    dist = Math.sqrt(dx * dx + dy * dy);

                // Normalize distance and add expanding phase
                var normDist = dist / maxDist;
                var ringPhase = (normDist * ringCount - phase) % 1;
                ringPhase = ((ringPhase % 1) + 1) % 1; // wrap to 0-1

                // Create ring pattern with smooth falloff
                var ringVal = Math.sin(ringPhase * Math.PI * 2) * 0.5 + 0.5;

                // Audio modulates ring brightness
                var bright = AudioParams.applyPunch(AudioParams.applyFloor(algo, Math.min(1, ringVal * power)), audio) * beatBoost * noveltyBoost;

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
