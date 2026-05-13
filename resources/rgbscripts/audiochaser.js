/*
  Q Light Controller Plus
  audiochaser.js

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
    algo.name = "Audio Chaser";
    algo.author = "QLC+ contributors";
    algo.acceptColors = 3; // low/mid/high mel-bank gradient
    algo.usesAudio = true;
    algo.properties = new Array();

    algo.presetBaseSpeed = 5;
    algo.properties.push(
      "name:presetBaseSpeed|type:range|display:Base Speed|" +
      "values:1,10|write:setBaseSpeed|read:getBaseSpeed");
    algo.presetDotCount = 5;
    algo.properties.push(
      "name:presetDotCount|type:range|display:Dot Count|" +
      "values:1,15|write:setDotCount|read:getDotCount");
    algo.presetTrailLength = 5;
    algo.properties.push(
      "name:presetTrailLength|type:range|display:Trail Length|" +
      "values:1,10|write:setTrailLength|read:getTrailLength");
    algo.presetSpeedMode = 0;
    algo.properties.push(
      "name:presetSpeedMode|type:list|display:Speed Driver|" +
      "values:Volume,Bass,Combined|write:setSpeedMode|read:getSpeedMode");
    algo.presetBounce = 0;
    algo.properties.push(
      "name:presetBounce|type:list|display:Bounce|" +
      "values:No,Yes|write:setBounce|read:getBounce");

    algo.setBaseSpeed = function(_v) { algo.presetBaseSpeed = parseInt(_v); };
    algo.getBaseSpeed = function() { return algo.presetBaseSpeed; };
    algo.setDotCount = function(_v) { algo.presetDotCount = parseInt(_v); };
    algo.getDotCount = function() { return algo.presetDotCount; };
    algo.setTrailLength = function(_v) { algo.presetTrailLength = parseInt(_v); };
    algo.getTrailLength = function() { return algo.presetTrailLength; };
    algo.setSpeedMode = function(_v) {
        if (_v === "Bass") algo.presetSpeedMode = 1;
        else if (_v === "Combined") algo.presetSpeedMode = 2;
        else algo.presetSpeedMode = 0;
    };
    algo.getSpeedMode = function() {
        return ["Volume", "Bass", "Combined"][algo.presetSpeedMode];
    };
    algo.setBounce = function(_v) { algo.presetBounce = (_v === "Yes") ? 1 : 0; };
    algo.getBounce = function() { return algo.presetBounce ? "Yes" : "No"; };

    var DEFAULT_BAND_COLORS = [
        {h: 0.958, s: 1.0, v: 1.0},
        {h: 0.167, s: 1.0, v: 1.0},
        {h: 0.611, s: 0.75, v: 1.0}
    ];

    // Dots: [{pos, row, speed, dir, band}]  band: 0=low, 1=mid, 2=high
    var dots = null;
    var dotsWidth = 0;
    var dotsHeight = 0;

    function initDots(width, height, count) {
        dots = [];
        for (var i = 0; i < count; i++) {
            dots.push({
                pos: Math.random() * width,
                row: Math.floor(Math.random() * height),
                speed: 1 + Math.random() * 2,
                dir: (Math.random() > 0.5) ? 1 : -1,
                band: i % 3
            });
        }
    }

    var TWO_PI = Math.PI * 2;

    // Additive HSV blend: blends hue circularly weighted by brightness, adds V
    var blendAddHsv = function(map, idx, nh, ns, nv) {
        var ev = map[idx + 2];
        if (ev < 0.001) {
            map[idx] = nh; map[idx + 1] = ns; map[idx + 2] = nv;
            return;
        }
        var eh = map[idx];
        var totalV = ev + nv;
        var cx = Math.cos(eh * TWO_PI) * ev + Math.cos(nh * TWO_PI) * nv;
        var cy = Math.sin(eh * TWO_PI) * ev + Math.sin(nh * TWO_PI) * nv;
        var bh = Math.atan2(cy, cx) / TWO_PI;
        if (bh < 0) bh += 1;
        map[idx] = bh;
        map[idx + 1] = (map[idx + 1] * ev + ns * nv) / totalV;
        map[idx + 2] = Math.min(1, totalV);
    };

    algo.rgbMapStepCount = function(width, height) { return 1; };

    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() {
        return algo.colors ? algo.colors.slice() : DEFAULT_BAND_COLORS.slice();
    };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        if (!dots || dots.length !== algo.presetDotCount ||
            dotsWidth !== width || dotsHeight !== height) {
            initDots(width, height, algo.presetDotCount);
            dotsWidth = width;
            dotsHeight = height;
        }

        var map = HSVUtil.createMap(width, height);
        if (!audio) return map;

        var dt = audio.timing.consumerDtMs / 1000.0;

        // Get 3 mel-bank powers and matching gradient colors
        var powers = audio.power.bands;
        var vol = audio.volume.normalized;
        var colorStops = algo.colors || DEFAULT_BAND_COLORS;

        // Speed multiplier from audio
        var speedMult;
        if (algo.presetSpeedMode === 0) {
            speedMult = 0.5 + vol * 4;
        } else if (algo.presetSpeedMode === 1) {
            speedMult = 0.5 + powers[1] * 5;
        } else {
            speedMult = 0.5 + (powers[0] + powers[1] + powers[2]) * 2.0;
        }

        var baseSpeed = algo.presetBaseSpeed * 5;
        var trailLen = algo.presetTrailLength;

        // Kick flash for speed boost
        var kickFlash = audio.beat.kick ? 1.0 : 0.0;
        speedMult += kickFlash * 0.5;

        // Move dots
        for (var di = 0; di < dots.length; di++) {
            var dot = dots[di];
            var bandPower = powers[dot.band];

            // Each dot's speed is modulated by its band's power
            var dotSpeed = baseSpeed * speedMult * dot.speed * (0.3 + bandPower * 0.7);
            dot.pos += dot.dir * dotSpeed * dt;

            // Bounce or wrap
            if (algo.presetBounce) {
                if (dot.pos >= width) { dot.pos = width - 1; dot.dir = -1; }
                if (dot.pos < 0) { dot.pos = 0; dot.dir = 1; }
            } else {
                if (dot.pos >= width) dot.pos -= width;
                if (dot.pos < 0) dot.pos += width;
            }

            // Render dot with trail
            var color = colorStops[dot.band];
            var beatBoost = 1.0 + 0.25 * audio.beat.cosPulse;
            var brightness = (0.5 + bandPower * 0.5) * beatBoost;
            var headX = Math.floor(dot.pos);

            for (var t = 0; t < trailLen + 1; t++) {
                var tx = headX - dot.dir * t;
                if (algo.presetBounce) {
                    if (tx < 0 || tx >= width) continue;
                } else {
                    tx = ((tx % width) + width) % width;
                }

                var trailFade = 1 - (t / (trailLen + 1));
                var fade = brightness * trailFade * trailFade;

                // 2D: render dot at its row with vertical spread
                var centerY = dot.row;
                var spread = Math.max(0, Math.ceil(height / 6));
                for (var dy = -spread; dy <= spread; dy++) {
                    var py = centerY + dy;
                    if (py < 0 || py >= height) continue;
                    var yFade = fade * (1 - Math.abs(dy) / (spread + 1));
                    var pixV = color.v * yFade;
                    if (pixV > 0.001) {
                        blendAddHsv(map, (py * width + tx) * 3, color.h, color.s, pixV);
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
