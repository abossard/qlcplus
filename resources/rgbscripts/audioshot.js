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
    algo.acceptColors = 5;
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
    algo.presetColorMode = 0;
    algo.properties.push(
      "name:presetColorMode|type:list|display:Color Mode|" +
      "values:Random,Palette,Fixed|write:setColorMode|read:getColorMode");

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
    algo.setColorMode = function(_v) {
        if (_v === "Palette") algo.presetColorMode = 1;
        else if (_v === "Fixed") algo.presetColorMode = 2;
        else algo.presetColorMode = 0;
    };
    algo.getColorMode = function() {
        return ["Random", "Palette", "Fixed"][algo.presetColorMode];
    };

    var util = new Object;
    util.colorArray = new Array(5);
    for (var i = 0; i < 5; i++) util.colorArray[i] = 0;

    // Active shots: [{x, y, r, g, b, brightness, size}]
    var shots = [];
    var initialized = false;

    algo.rgbMapStepCount = function(width, height) { return 1; };

    algo.rgbMapSetColors = function(rawColors) {
        for (var i = 0; i < Math.min(rawColors.length, 5); i++)
            util.colorArray[i] = rawColors[i];
    };
    algo.rgbMapGetColors = function() {
        var arr = [];
        for (var i = 0; i < 5; i++) arr.push(util.colorArray[i]);
        return arr;
    };

    function getColor(index) {
        var c = util.colorArray[index % 5] || 0xFF0000;
        return [(c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF];
    }

    function spawnShot(width, height) {
        var r, g, b;
        if (algo.presetColorMode === 0) {
            // Random vibrant color via HSV
            var c = LedFx.hsv2rgb(Math.random(), 0.8 + Math.random() * 0.2, 1);
            r = c[0]; g = c[1]; b = c[2];
        } else if (algo.presetColorMode === 1) {
            // Cycle through palette colors
            var ci = Math.floor(Math.random() * 5);
            var c = getColor(ci);
            r = c[0]; g = c[1]; b = c[2];
        } else {
            var c = getColor(0);
            r = c[0]; g = c[1]; b = c[2];
        }

        shots.push({
            x: Math.floor(Math.random() * width),
            y: Math.floor(Math.random() * height),
            r: r, g: g, b: b,
            brightness: 1.0,
            size: algo.presetSize
        });

        // Cap shots
        while (shots.length > algo.presetMaxShots)
            shots.shift();
    }

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var map = LedFx.createMap(width, height);
        if (!audio || !audio.spectrum || audio.spectrum.length === 0) return map;

        // Check trigger
        var trigger = false;
        var thresh = 0.5;
        if (algo.presetTrigger === 0) trigger = audio.beat;
        else if (algo.presetTrigger === 1) trigger = LedFx.lows_power(audio) > thresh;
        else if (algo.presetTrigger === 2) trigger = LedFx.mids_power(audio) > thresh;
        else trigger = LedFx.high_power(audio) > thresh;

        if (trigger) spawnShot(width, height);

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

                    map[py][px] = LedFx.rgb(
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
