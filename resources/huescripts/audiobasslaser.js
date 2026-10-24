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

    algo.setTrailLength = function(_v) { algo.presetTrailLength = Math.round(HSVUtil.parseNumber(_v, algo.presetTrailLength, 2, 20)); };
    algo.getTrailLength = function() { return algo.presetTrailLength; };
    algo.setMaxBeams = function(_v) { algo.presetMaxBeams = Math.round(HSVUtil.parseNumber(_v, algo.presetMaxBeams, 1, 16)); };
    algo.getMaxBeams = function() { return algo.presetMaxBeams; };
    algo.setDirection = function(_v) {
        if (_v === "Vertical" || parseInt(_v) === 1) algo.presetDirection = 1;
        else if (_v === "Both" || parseInt(_v) === 2) algo.presetDirection = 2;
        else algo.presetDirection = 0;
    };
    algo.getDirection = function() {
        return ["Horizontal", "Vertical", "Both"][algo.presetDirection];
    };
    algo.setSpeed = function(_v) { algo.presetSpeed = HSVUtil.parseNumber(_v, algo.presetSpeed, 1, 10); };
    algo.getSpeed = function() { return algo.presetSpeed; };

    algo.presetSmoothing = 5;
    algo.properties.push(
      "name:presetSmoothing|type:range|display:Smoothing|" +
      "values:1,10|write:setSmoothing|read:getSmoothing");
    algo.setSmoothing = function(_v) { algo.presetSmoothing = Math.round(HSVUtil.parseNumber(_v, algo.presetSmoothing, 1, 10)); };
    algo.getSmoothing = function() { return algo.presetSmoothing; };

    var smoothHigh = 0;
    var smoothOnset = 0;

    // Without user colors: white beam, blue (32, 128, 255) trail.
    var DEFAULT_COLORS = [{ h: 0, s: 0, v: 1.0 }, { h: 0.611, s: 0.875, v: 1.0 }];
    var beamColor, trailColor; // set by each rgbMap call

    algo.beams = [];
    var lastWidth = 0;
    var lastHeight = 0;

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function() {};
    algo.rgbMapGetColors = function() { return []; };


    // Returns {h, s, v} interpolated between beamColor and trailColor,
    // scaled by brightness.
    function colorAtTrail(t, brightness) {
        var mix = t / Math.max(1, algo.presetTrailLength);
        return {
            h: HSVUtil.lerpHue(beamColor.h, trailColor.h, mix),
            s: beamColor.s * (1.0 - mix) + trailColor.s * mix,
            v: (beamColor.v * (1.0 - mix) + trailColor.v * mix) * brightness
        };
    }

    function addPixel(map, width, height, x, y, color) {
        HSVUtil.addPixel(map, width, height, Math.round(x), Math.round(y), color.h, color.s, color.v);
    }

    function spawnBeam(width, height, strength) {
        var vertical = algo.presetDirection === 2 ? Math.random() >= 0.5 : algo.presetDirection === 1;
        var dir = Math.random() < 0.5 ? 1 : -1;
        var presetSpeed = 0.35 + algo.presetSpeed * 0.12;
        var strengthSpeed = 0.7 + Math.min(1.5, strength) * 0.9;
        var speed = presetSpeed * strengthSpeed;
        var beam = {
            x: vertical ? Math.floor(Math.random() * width) : (dir > 0 ? 0 : width - 1),
            y: vertical ? (dir > 0 ? 0 : height - 1) : Math.floor(Math.random() * height),
            dx: vertical ? 0 : dir * speed,
            dy: vertical ? dir * speed : 0,
            life: algo.presetTrailLength * 3,
            maxLife: algo.presetTrailLength * 3
        };
        // The draw that picked the never-read beam hue, kept for the random sequence.
        Math.random();

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

        var colors = algo.hasUserColors && algo.colors && algo.colors.length >= 2 ? algo.colors : DEFAULT_COLORS;
        beamColor = colors[0];
        trailColor = colors[1];
        var frames = HSVUtil.elapsedFrames(audio);

        var bass = audio.low;
        var rawHighs = audio.high;
        var rawOnset = Math.max(0.4, audio.onsetIntensity);
        smoothHigh = HSVUtil.smoothFollow(smoothHigh, rawHighs, algo.presetSmoothing, frames);
        smoothOnset = HSVUtil.smoothFollow(smoothOnset, rawOnset, algo.presetSmoothing, frames);
        var glowMul = (0.8 + smoothHigh * 0.4) * smoothOnset;

        if (audio.beatFired || audio.kickFired || (algo.beams.length < 3 && bass > 0.15))
            spawnBeam(width, height, Math.max(0.3, bass));

        for (var bi = algo.beams.length - 1; bi >= 0; bi--) {
            var beam = algo.beams[bi];
            beam.x += beam.dx * frames;
            beam.y += beam.dy * frames;
            beam.life -= frames;

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
            renderAmbient(map, width, height, bass, smoothHigh);

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
