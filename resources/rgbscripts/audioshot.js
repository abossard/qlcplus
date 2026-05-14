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
    algo.acceptColors = 3;
    algo.usesAudio = true;
    algo.properties = new Array();

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
    algo.setDecay = function(_v) { algo.presetDecay = parseFloat(_v); };
    algo.getDecay = function() { return algo.presetDecay; };
    algo.setSize = function(_v) { algo.presetSize = parseFloat(_v); };
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
    var DECAY_DIVISOR = 200.0;

    // Active shots: [{x, y, h, s, brightness, size}]
    var shots = [];

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };

    function spawnShot(width, height, audio) {
        var band = (audio.mid > audio.low && audio.mid >= audio.high) ? 1 : (audio.high > audio.low) ? 2 : 0;
        var color = (algo.colors && algo.colors.length >= 3) ? algo.colors[band] : {h: 0, s: 0, v: 1};
        var hitScale = Math.min(1.0, 0.4 + 0.6 * audio.onsetIntensity);
        var y;
        if (band === 0)
            y = Math.floor(height * 0.5 + Math.random() * height * 0.5);
        else if (band === 2)
            y = Math.floor(Math.random() * height * 0.5);
        else
            y = Math.floor(Math.random() * height);

        shots.push({
            x: Math.floor(Math.random() * width),
            y: y,
            h: color.h,
            s: color.s,
            brightness: hitScale,
            size: algo.presetSize
        });

        while (shots.length > algo.presetMaxShots)
            shots.shift();
    }

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var map = HSVUtil.createMap(width, height);
        if (!audio) return map;

        var trigger;
        if (algo.presetTrigger === 0) trigger = audio.beatFired;
        else if (algo.presetTrigger === 1) trigger = audio.beatFired || audio.beatFired;
        else if (algo.presetTrigger === 2) trigger = audio.onset;
        else trigger = audio.onset;
        trigger = trigger || audio.onset;

        if (trigger) spawnShot(width, height, audio);

        var decayRate = algo.presetDecay / DECAY_DIVISOR;

        // Accumulate per-pixel brightness and track dominant shot hue
        // (simulates additive blending by picking brightest contributor's hue)
        for (var si = shots.length - 1; si >= 0; si--) {
            var shot = shots[si];
            shot.brightness -= decayRate;

            if (shot.brightness <= 0) {
                shots.splice(si, 1);
                continue;
            }

            var b = shot.brightness;
            var sz = Math.ceil(shot.size * (0.5 + b * 0.5));

            for (var dy = -sz; dy <= sz; dy++) {
                for (var dx = -sz; dx <= sz; dx++) {
                    var px = shot.x + dx;
                    var py = shot.y + dy;
                    if (px < 0 || px >= width || py < 0 || py >= height) continue;

                    var dist = Math.sqrt(dx * dx + dy * dy);
                    if (dist > sz) continue;
                    var falloff = b * (1 - dist / (sz + 0.5));

                    var i3 = (py * width + px) * 3;
                    var existingV = map[i3 + 2];
                    var newV = Math.min(1, existingV + falloff);
                    if (existingV < 0.001) {
                        map[i3] = shot.h;
                        map[i3 + 1] = shot.s;
                        map[i3 + 2] = newV;
                    } else {
                        // Blend hue toward brighter contributor
                        var total = existingV + falloff;
                        var t = falloff / Math.max(0.001, total);
                        var dh = shot.h - map[i3];
                        if (dh > 0.5) dh -= 1;
                        else if (dh < -0.5) dh += 1;
                        map[i3] = (map[i3] + t * dh) - Math.floor(map[i3] + t * dh);
                        map[i3 + 1] = map[i3 + 1] * (1 - t) + shot.s * t;
                        map[i3 + 2] = newV;
                    }
                }
            }
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
