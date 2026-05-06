/*
  Q Light Controller Plus
  audiofireworks.js

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
    algo.name = "Audio Fireworks";
    algo.author = "QLC+ contributors";
    algo.acceptColors = 0;  // colors are frequency-driven
    algo.usesAudio = true;
    algo.properties = new Array();

    AudioParams.installTrigger(algo, {gain: 7, reactivity: 7, sensitivity: 7});

    algo.presetMaxParticles = 200;
    algo.properties.push(
      "name:presetMaxParticles|type:range|display:MaxParticles|" +
      "values:50,500|write:setMaxParticles|read:getMaxParticles");
    algo.presetGravity = 3;
    algo.properties.push(
      "name:presetGravity|type:range|display:Gravity|" +
      "values:0,10|write:setGravity|read:getGravity");
    algo.presetOrigin = 0;
    algo.properties.push(
      "name:presetOrigin|type:list|display:Origin|" +
      "values:Center,Bottom,Random|write:setOrigin|read:getOrigin");
    algo.presetParticleSize = 1;
    algo.properties.push(
      "name:presetParticleSize|type:range|display:ParticleSize|" +
      "values:1,3|write:setParticleSize|read:getParticleSize");

    algo.particles = [];
    algo.bassFilter = null;
    algo.midsFilter = null;
    algo.highsFilter = null;

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };

    algo.setMaxParticles = function(_v) { algo.presetMaxParticles = clampInt(_v, 50, 500, 200); };
    algo.getMaxParticles = function() { return algo.presetMaxParticles; };
    algo.setGravity = function(_v) { algo.presetGravity = clampInt(_v, 0, 10, 3); };
    algo.getGravity = function() { return algo.presetGravity; };
    algo.setOrigin = function(_v) {
        if (_v === "Bottom" || parseInt(_v) === 1) algo.presetOrigin = 1;
        else if (_v === "Random" || parseInt(_v) === 2) algo.presetOrigin = 2;
        else algo.presetOrigin = 0;
    };
    algo.getOrigin = function() { return ["Center", "Bottom", "Random"][algo.presetOrigin]; };
    algo.setParticleSize = function(_v) { algo.presetParticleSize = clampInt(_v, 1, 3, 1); };
    algo.getParticleSize = function() { return algo.presetParticleSize; };

    function clampInt(value, minValue, maxValue, defaultValue) {
        var parsed = parseInt(value);
        if (isNaN(parsed)) parsed = defaultValue;
        return Math.max(minValue, Math.min(maxValue, parsed));
    }

    function additive(existing, newColor) {
        var er = (existing >> 16) & 0xFF, eg = (existing >> 8) & 0xFF, eb = existing & 0xFF;
        var nr = (newColor >> 16) & 0xFF, ng = (newColor >> 8) & 0xFF, nb = newColor & 0xFF;
        return RGBUtil.rgb(Math.min(255, er+nr), Math.min(255, eg+ng), Math.min(255, eb+nb));
    }

    function initState() {
    }



    function chooseOrigin(width, height) {
        if (algo.presetOrigin === 1)
            return [Math.floor(width / 2), height - 1];
        if (algo.presetOrigin === 2)
            return [Math.floor(Math.random() * width), Math.floor(Math.random() * height)];
        return [(width - 1) / 2.0, (height - 1) / 2.0];
    }

    function spawnParticle(width, height, hue, speed, life) {
        var origin = chooseOrigin(width, height);
        var angle = Math.random() * Math.PI * 2;
        algo.particles.push({
            x: origin[0],
            y: origin[1],
            vx: Math.cos(angle) * speed * 0.5,
            vy: Math.sin(angle) * speed * 0.5,
            hue: hue,
            life: life,
            maxLife: life
        });
    }

    function spawnBurst(width, height, type) {
        var countRange, speedRange, hueRange, lifeRange;
        if (type === "bass") {
            countRange = [20, 30];
            speedRange = [1.5, 3.0];
            hueRange = [0.0, 0.08];
            lifeRange = [25, 40];
        } else if (type === "mids") {
            countRange = [12, 18];
            speedRange = [2.0, 4.0];
            hueRange = [0.22, 0.35];
            lifeRange = [18, 28];
        } else {
            countRange = [6, 12];
            speedRange = [3.0, 5.0];
            hueRange = [0.55, 0.72];
            lifeRange = [8, 15];
        }

        var count = countRange[0] + Math.round(Math.random() * (countRange[1] - countRange[0]));
        var origin = chooseOrigin(width, height);
        for (var i = 0; i < count; i++) {
            var angle = Math.random() * Math.PI * 2;
            var speed = speedRange[0] + Math.random() * (speedRange[1] - speedRange[0]);
            var hue = hueRange[0] + Math.random() * (hueRange[1] - hueRange[0]);
            var life = lifeRange[0] + Math.round(Math.random() * (lifeRange[1] - lifeRange[0]));

            algo.particles.push({
                x: origin[0], y: origin[1],
                vx: Math.cos(angle) * speed * 0.5,
                vy: Math.sin(angle) * speed * 0.5,
                hue: hue,
                life: life, maxLife: life
            });
        }
    }

    function spawnAmbient(width, height) {
        var count = 2 + Math.floor(Math.random() * 2);
        for (var i = 0; i < count; i++) {
            var hue = Math.random();
            var speed = 0.4 + Math.random() * 0.8;
            var life = 6 + Math.round(Math.random() * 6);
            spawnParticle(width, height, hue, speed, life);
        }
    }

    function addPixel(map, width, height, x, y, color) {
        if (x < 0 || x >= width || y < 0 || y >= height) return;
        map[y][x] = additive(map[y][x], color);
    }

    function renderParticle(map, width, height, particle, particleSize) {
        var px = Math.round(particle.x);
        var py = Math.round(particle.y);
        if (px < 0 || px >= width || py < 0 || py >= height) return;

        var fade = particle.life / particle.maxLife;
        var bri = fade * fade;
        var color = RGBUtil.hsv2rgb(particle.hue, 1.0, bri);
        var packed = RGBUtil.rgb(color[0], color[1], color[2]);
        addPixel(map, width, height, px, py, packed);

        if (particleSize >= 2 && bri > 0.3) {
            var neighborColor = RGBUtil.rgb(color[0] * 0.5, color[1] * 0.5, color[2] * 0.5);
            addPixel(map, width, height, px - 1, py, neighborColor);
            addPixel(map, width, height, px + 1, py, neighborColor);
            addPixel(map, width, height, px, py - 1, neighborColor);
            addPixel(map, width, height, px, py + 1, neighborColor);
        }

        if (particleSize >= 3 && bri > 0.3) {
            var diagonalColor = RGBUtil.rgb(color[0] * 0.25, color[1] * 0.25, color[2] * 0.25);
            addPixel(map, width, height, px - 1, py - 1, diagonalColor);
            addPixel(map, width, height, px + 1, py - 1, diagonalColor);
            addPixel(map, width, height, px - 1, py + 1, diagonalColor);
            addPixel(map, width, height, px + 1, py + 1, diagonalColor);
        }
    }

    function renderAmbientSparkle(map, width, height, step, audioPower) {
        var count = Math.max(1, Math.floor(width * height / 80));
        var brightness = 0.02 + Math.min(0.03, audioPower * 0.01);
        for (var i = 0; i < count; i++) {
            var x = Math.floor(Math.random() * width);
            var y = Math.floor(Math.random() * height);
            var hue = (step * 0.003 + Math.random()) % 1.0;
            var color = RGBUtil.hsv2rgb(hue, 0.45, brightness * (0.5 + Math.random() * 0.5));
            map[y][x] = additive(map[y][x], RGBUtil.rgb(color[0], color[1], color[2]));
        }
    }

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        initState();

        var map = RGBUtil.createMap(width, height);
        if (!audio || !audio.mel || audio.mel.length === 0) return map;
        var bass = audio.bands.low;
        var mids = audio.bands.mid;
        var highs = audio.bands.high;

        if (audio.triggers.bass.firedThisFrame) spawnBurst(width, height, "bass");
        if (audio.triggers.mid.firedThisFrame) spawnBurst(width, height, "mids");
        if (audio.triggers.high.firedThisFrame) spawnBurst(width, height, "highs");

        if (algo.particles.length < 10 && (bass + mids + highs) > 0.1)
            spawnAmbient(width, height);

        var gravity = algo.presetGravity * 0.02;
        for (var i = algo.particles.length - 1; i >= 0; i--) {
            var particle = algo.particles[i];
            particle.x += particle.vx;
            particle.y += particle.vy;
            particle.vy += gravity;
            particle.life--;
            if (particle.life <= 0)
                algo.particles.splice(i, 1);
        }

        while (algo.particles.length > algo.presetMaxParticles)
            algo.particles.shift();

        renderAmbientSparkle(map, width, height, step, bass + mids + highs);

        for (var pi = 0; pi < algo.particles.length; pi++)
            renderParticle(map, width, height, algo.particles[pi], algo.presetParticleSize);

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
