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
    algo.acceptColors = 3;
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
    algo.presetRings = 5;
    algo.properties.push(
      "name:presetRings|type:range|display:Ring Count|" +
      "values:2,10|write:setRings|read:getRings");
    algo.presetShape = 0;
    algo.properties.push(
      "name:presetShape|type:list|display:Shape|" +
      "values:Circle,Diamond,Square|write:setShape|read:getShape");

    algo.setSpeed = function(_v) { algo.presetSpeed = parseFloat(_v); };
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

    algo.setReactivity = function(_v) { algo.presetReactivity = parseFloat(_v); };
    algo.getReactivity = function() { return algo.presetReactivity; };

    var BEAT_PULSE_AMP = 0.20;
    var tunnelState = { phase: 0 };

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var map = RGBUtil.createMap(width, height);
        if (!audio) return map;

        var dtMs = audio.timing.consumerDtMs;
        var bpm = (audio && audio.beat) ? audio.beat.bpm : 0;

        var power = audio.power.low;
        var speed = algo.presetSpeed;
        var phase = RGBUtil.beatTime(speed * (1 + power * algo.presetReactivity), tunnelState, bpm, dtMs);

        var cx = width / 2;
        var cy = height / 2;
        var maxDist = Math.sqrt(cx * cx + cy * cy);
        var ringCount = algo.presetRings;
        var blended = AudioColors.blendByPower(algo, audio);
        var beatBoost = 1.0 + BEAT_PULSE_AMP * audio.beat.cosPulse;
        var noveltyBoost = AudioColors.noveltyBoost(audio);
        var fluxPunch = AudioColors.fluxPunch(audio);

        for (var y = 0; y < height; y++) {
            for (var x = 0; x < width; x++) {
                var dx = x - cx + 0.5;
                var dy = y - cy + 0.5;

                var dist;
                if (algo.presetShape === 1)
                    dist = Math.abs(dx) + Math.abs(dy);
                else if (algo.presetShape === 2)
                    dist = Math.max(Math.abs(dx), Math.abs(dy));
                else
                    dist = Math.sqrt(dx * dx + dy * dy);

                var normDist = dist / maxDist;
                var ringPhase = (normDist * ringCount - phase) % 1;
                ringPhase = ((ringPhase % 1) + 1) % 1;

                var ringVal = Math.sin(ringPhase * Math.PI * 2) * 0.5 + 0.5;

                var baseBright = Math.min(1, ringVal * power);
                var bright = Math.min(1, baseBright * fluxPunch) * beatBoost * noveltyBoost;

                var i3 = (y * width + x) * 3;
                map[i3] = blended.h;
                map[i3 + 1] = blended.s;
                map[i3 + 2] = Math.min(1, blended.v * bright);
            }
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
