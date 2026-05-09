/*
  Q Light Controller Plus
  audioshot.js

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
    algo.name = "Audio Shot";
    algo.author = "QLC+ contributors";
    algo.acceptColors = 3; // low/mid/high mel-bank gradient
    algo.usesAudio = true;
    algo.properties = new Array();

    AudioParams.installTrigger(algo, {gain: 5, reactivity: 5, sensitivity: 5});
    AudioParams.installBandPowerControls(algo);

    algo.presetDecay = 5;
    algo.properties.push(
      "name:presetDecay|type:range|display:Decay Speed|" +
      "values:1,10|write:setDecay|read:getDecay");
    algo.presetSize = 3;
    algo.properties.push(
      "name:presetSize|type:range|display:Shot Size|" +
      "values:1,10|write:setSize|read:getSize");
    algo.presetTrigger = 0;
    algo.properties.push(
      "name:presetTrigger|type:list|display:Trigger|" +
      "values:Beat,Bass,Mids,Highs|write:setTrigger|read:getTrigger");
    algo.presetMaxShots = 5;
    algo.properties.push(
      "name:presetMaxShots|type:range|display:Max Shots|" +
      "values:1,20|write:setMaxShots|read:getMaxShots");
    algo.setDecay = function(_v) { algo.presetDecay = parseInt(_v); };
    algo.getDecay = function() { return algo.presetDecay; };
    algo.setSize = function(_v) { algo.presetSize = parseInt(_v); };
    algo.getSize = function() { return algo.presetSize; };
    algo.setTrigger = function(_v) {
        if (_v === "Bass") algo.presetTrigger = 1;
        else if (_v === "Mids") algo.presetTrigger = 2;
        else if (_v === "Highs") algo.presetTrigger = 3;
        else algo.presetTrigger = 0;
    };
    algo.getTrigger = function() {
        return ["Beat", "Bass", "Mids", "Highs"][algo.presetTrigger];
    };
    algo.setMaxShots = function(_v) { algo.presetMaxShots = parseInt(_v); };
    algo.getMaxShots = function() { return algo.presetMaxShots; };
    var DEFAULT_BAND_COLORS = [0xFF0000, 0xFFFF00, 0xFFFFFF];

    // Active shots: [{x, y, r, g, b, brightness, size}]
    var shots = [];
    var initialized = false;

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() {
        return AudioParams.bandColors(algo, DEFAULT_BAND_COLORS).slice();
    };


    function spawnShot(width, height, audio) {
        var color = AudioParams.colorChannels(
            AudioParams.dominantBandColor(algo, audio, DEFAULT_BAND_COLORS));
        var r = color[0], g = color[1], b = color[2];
        var hitScale = Math.min(1.0, 0.4 + 0.6 * AudioParams.maxOnsetIntensity(audio));

        shots.push({
            x: Math.floor(Math.random() * width),
            y: Math.floor(Math.random() * height),
            r: r, g: g, b: b,
            brightness: hitScale,
            size: algo.presetSize
        });

        // Cap shots
        while (shots.length > algo.presetMaxShots)
            shots.shift();
    }

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var map = RGBUtil.createMap(width, height);
        if (!audio || !audio.mel || audio.mel.length === 0) return map;

        // Check trigger
        var trigger = false;
        if (algo.presetTrigger === 0) trigger = audio.triggers.beat.firedThisFrame;
        else if (algo.presetTrigger === 1) trigger = audio.triggers.low.firedThisFrame || AudioParams.kickFired(audio);
        else if (algo.presetTrigger === 2) trigger = audio.triggers.mid.firedThisFrame;
        else trigger = audio.triggers.high.firedThisFrame;
        trigger = trigger || AudioParams.anyOnsetFired(audio);

        if (trigger) spawnShot(width, height, audio);

        // Decay rate
        var decayRate = algo.presetDecay / 200.0;

        // Render and decay shots
        for (var si = shots.length - 1; si >= 0; si--) {
            var shot = shots[si];
            shot.brightness -= decayRate;

            if (shot.brightness <= 0) {
                shots.splice(si, 1);
                continue;
            }

            var b = shot.brightness;
            var sz = Math.ceil(shot.size * (0.5 + b * 0.5)); // shrinks as it fades

            for (var dy = -sz; dy <= sz; dy++) {
                for (var dx = -sz; dx <= sz; dx++) {
                    var px = shot.x + dx;
                    var py = shot.y + dy;
                    if (px < 0 || px >= width || py < 0 || py >= height) continue;

                    // Distance falloff
                    var dist = Math.sqrt(dx * dx + dy * dy);
                    if (dist > sz) continue;
                    var falloff = b * (1 - dist / (sz + 0.5));

                    // Additive blend
                    var existing = map[py][px];
                    var er = (existing >> 16) & 0xFF;
                    var eg = (existing >> 8) & 0xFF;
                    var eb = existing & 0xFF;

                    map[py][px] = RGBUtil.rgb(
                        Math.min(255, er + shot.r * falloff),
                        Math.min(255, eg + shot.g * falloff),
                        Math.min(255, eb + shot.b * falloff)
                    );
                }
            }
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
