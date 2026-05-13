/*
  Q Light Controller Plus
  audiobasslaser.js

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
    algo.name = "Audio Bass Laser";
    algo.author = "QLC+ contributors";
    algo.acceptColors = 2;  // beam color + trail color
    algo.usesAudio = true;
    algo.properties = new Array();

    algo.presetTrailLength = 8;
    algo.properties.push(
      "name:presetTrailLength|type:range|display:Trail Length|" +
      "values:2,20|write:setTrailLength|read:getTrailLength");
    algo.presetMaxBeams = 6;
    algo.properties.push(
      "name:presetMaxBeams|type:range|display:Max Beams|" +
      "values:1,16|write:setMaxBeams|read:getMaxBeams");
    algo.presetDirection = 2;
    algo.properties.push(
      "name:presetDirection|type:list|display:Direction|" +
      "values:Horizontal,Vertical,Both|write:setDirection|read:getDirection");
    algo.presetSpeed = 6;
    algo.properties.push(
      "name:presetSpeed|type:range|display:Speed|" +
      "values:1,10|write:setSpeed|read:getSpeed");

    algo.setTrailLength = function(_v) { algo.presetTrailLength = clampInt(_v, 2, 20, 8); };
    algo.getTrailLength = function() { return algo.presetTrailLength; };
    algo.setMaxBeams = function(_v) { algo.presetMaxBeams = clampInt(_v, 1, 16, 6); };
    algo.getMaxBeams = function() { return algo.presetMaxBeams; };
    algo.setDirection = function(_v) {
        if (_v === "Vertical" || parseInt(_v) === 1) algo.presetDirection = 1;
        else if (_v === "Both" || parseInt(_v) === 2) algo.presetDirection = 2;
        else algo.presetDirection = 0;
    };
    algo.getDirection = function() {
        return ["Horizontal", "Vertical", "Both"][algo.presetDirection];
    };
    algo.setSpeed = function(_v) { algo.presetSpeed = clampInt(_v, 1, 10, 6); };
    algo.getSpeed = function() { return algo.presetSpeed; };

    // Colors stored as HSV
    var beamColor = { h: 0, s: 0, v: 1.0 };        // white
    var trailColor = { h: 0.611, s: 0.875, v: 1.0 }; // blue (32, 128, 255)

    algo.beams = [];
    var lastWidth = 0;
    var lastHeight = 0;

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) {
        if (rawColors && rawColors.length >= 1)
            beamColor = rawColors[0];
        if (rawColors && rawColors.length >= 2)
            trailColor = rawColors[1];
    };
    algo.rgbMapGetColors = function() { return []; };

    function clampInt(value, minValue, maxValue, defaultValue) {
        var parsed = parseInt(value);
        if (isNaN(parsed)) parsed = defaultValue;
        return Math.max(minValue, Math.min(maxValue, parsed));
    }

    // Lerp hue along the shortest arc on the hue circle
    function lerpHue(h1, h2, t) {
        var d = h2 - h1;
        if (d > 0.5) d -= 1.0;
        else if (d < -0.5) d += 1.0;
        var h = h1 + d * t;
        return h < 0 ? h + 1.0 : (h >= 1.0 ? h - 1.0 : h);
    }

    // Returns {h, s, v} interpolated between beamColor and trailColor,
    // scaled by brightness.
    function colorAtTrail(t, brightness) {
        var mix = t / Math.max(1, algo.presetTrailLength);
        return {
            h: lerpHue(beamColor.h, trailColor.h, mix),
            s: beamColor.s * (1.0 - mix) + trailColor.s * mix,
            v: (beamColor.v * (1.0 - mix) + trailColor.v * mix) * brightness
        };
    }

    // Additive blend: keep hue of brighter contributor, accumulate brightness
    function addPixel(map, width, height, x, y, color) {
        x = Math.round(x);
        y = Math.round(y);
        if (x < 0 || x >= width || y < 0 || y >= height) return;
        var idx = (y * width + x) * 3;
        var ev = map[idx + 2];
        if (ev <= 0) {
            map[idx] = color.h;
            map[idx + 1] = color.s;
            map[idx + 2] = Math.min(1, color.v);
        } else {
            if (color.v > ev) {
                map[idx] = color.h;
                map[idx + 1] = color.s;
            }
            map[idx + 2] = Math.min(1, ev + color.v);
        }
    }

    function chooseDirection() {
        if (algo.presetDirection === 0) return 0;
        if (algo.presetDirection === 1) return 1;
        return Math.random() < 0.5 ? 0 : 1;
    }

    function spawnBeam(width, height, strength) {
        var direction = chooseDirection();
        var dir = Math.random() < 0.5 ? 1 : -1;
        var presetSpeed = 0.35 + algo.presetSpeed * 0.12;
        var strengthSpeed = 0.7 + Math.min(1.5, strength) * 0.9;
        var speed = presetSpeed * strengthSpeed;
        var beam;

        if (direction === 0) {
            beam = {
                x: dir > 0 ? 0 : width - 1,
                y: Math.floor(Math.random() * height),
                dx: dir * speed,
                dy: 0,
                life: algo.presetTrailLength * 3,
                maxLife: algo.presetTrailLength * 3,
                hue: Math.random()
            };
        } else {
            beam = {
                x: Math.floor(Math.random() * width),
                y: dir > 0 ? 0 : height - 1,
                dx: 0,
                dy: dir * speed,
                life: algo.presetTrailLength * 3,
                maxLife: algo.presetTrailLength * 3,
                hue: Math.random()
            };
        }

        algo.beams.push(beam);
        while (algo.beams.length > algo.presetMaxBeams)
            algo.beams.shift();
    }

    function fullyOffScreen(beam, width, height, margin) {
        return beam.x < -margin || beam.x >= width + margin ||
               beam.y < -margin || beam.y >= height + margin ||
               beam.life <= 0;
    }

    function renderAmbient(map, width, height, bass, highs) {
        var count = Math.max(1, Math.floor(width * height / 40));
        var ambient = 0.06 + bass * 0.08 + highs * 0.06;
        for (var i = 0; i < count; i++) {
            var x = Math.floor(Math.random() * width);
            var y = Math.floor(Math.random() * height);
            var twinkle = ambient * (0.4 + Math.random() * 0.6);
            addPixel(map, width, height, x, y, {
                h: trailColor.h,
                s: trailColor.s,
                v: trailColor.v * twinkle
            });
        }
    }

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var map = HSVUtil.createMap(width, height);
        if (!audio) return map;

        if (width !== lastWidth || height !== lastHeight) {
            algo.beams = [];
            lastWidth = width;
            lastHeight = height;
        }

        var bass = audio.power.low;
        var highs = audio.power.high;
        var onsetIntensity = Math.max(0.4, audio.onset.intensity);
        var glowMul = (0.8 + highs * 0.4) * onsetIntensity;

        if (audio.bands.low.fired || audio.beat.kick || (algo.beams.length < 3 && bass > 0.15))
            spawnBeam(width, height, Math.max(0.3, bass));

        for (var bi = algo.beams.length - 1; bi >= 0; bi--) {
            var beam = algo.beams[bi];
            beam.x += beam.dx;
            beam.y += beam.dy;
            beam.life--;

            if (fullyOffScreen(beam, width, height, algo.presetTrailLength)) {
                algo.beams.splice(bi, 1);
                continue;
            }

            for (var t = 0; t <= algo.presetTrailLength; t++) {
                var tx = beam.x - beam.dx * t;
                var ty = beam.y - beam.dy * t;
                if (Math.round(tx) < 0 || Math.round(tx) >= width || Math.round(ty) < 0 || Math.round(ty) >= height)
                    continue;

                var fall = 1.0 - t / algo.presetTrailLength;
                fall = Math.sqrt(fall);
                var bri = fall * glowMul * Math.max(0, beam.life / beam.maxLife);
                var pixel = colorAtTrail(t, bri);
                var halo = colorAtTrail(t, bri * 0.5);

                addPixel(map, width, height, tx, ty, pixel);
                if (beam.dx !== 0) {
                    addPixel(map, width, height, tx, ty - 1, halo);
                    addPixel(map, width, height, tx, ty + 1, halo);
                } else {
                    addPixel(map, width, height, tx - 1, ty, halo);
                    addPixel(map, width, height, tx + 1, ty, halo);
                }
            }
        }

        if (algo.beams.length < 2)
            renderAmbient(map, width, height, bass, highs);

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
