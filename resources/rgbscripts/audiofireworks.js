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
    algo.acceptColors = 3;  // low/mid/high mel-bank gradient
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
    algo.presetTriggerMode = 0;
    algo.properties.push(
      "name:triggerMode|type:list|display:Trigger Mode|" +
      "values:Beat,Onset,Note|write:setTriggerMode|read:getTriggerMode");

    algo.particles = [];
    var DEFAULT_BAND_COLORS = [0xFF0040, 0xFFFF00, 0x4080FF];

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() {
        return algo.gradientBandColors ? algo.gradientBandColors.slice() : DEFAULT_BAND_COLORS.slice();
    };

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
    algo.setTriggerMode = function(_v) {
        if (_v === "Onset") algo.presetTriggerMode = 1;
        else if (_v === "Note") algo.presetTriggerMode = 2;
        else algo.presetTriggerMode = 0;
    };
    algo.getTriggerMode = function() {
        return ["Beat", "Onset", "Note"][algo.presetTriggerMode];
    };

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

    function bandColorPacked(bandIndex) {
        var colors = algo.gradientBandColors || DEFAULT_BAND_COLORS;
        return colors[Math.max(0, Math.min(2, bandIndex))] & 0xFFFFFF;
    }

    function randomWeightedBand(powers, start, end) {
        var total = 0;
        for (var i = start; i <= end; i++) {
            var p = (powers && typeof powers[i] === "number" && isFinite(powers[i])) ? powers[i] : 0;
            if (p < 0) p = 0;
            total += p;
        }
        if (total <= 0.001) return start + Math.floor(Math.random() * (end - start + 1));
        var pick = Math.random() * total;
        for (var j = start; j <= end; j++) {
            var pj = (powers && typeof powers[j] === "number" && isFinite(powers[j])) ? powers[j] : 0;
            if (pj < 0) pj = 0;
            pick -= pj;
            if (pick <= 0) return j;
        }
        return end;
    }


    function chooseOrigin(width, height) {
        if (algo.presetOrigin === 1)
            return [Math.floor(width / 2), height - 1];
        if (algo.presetOrigin === 2)
            return [Math.floor(Math.random() * width), Math.floor(Math.random() * height)];
        return [(width - 1) / 2.0, (height - 1) / 2.0];
    }

    function spawnParticle(width, height, color, speed, life) {
        var origin = chooseOrigin(width, height);
        var angle = Math.random() * Math.PI * 2;
        algo.particles.push({
            x: origin[0],
            y: origin[1],
            vx: Math.cos(angle) * speed * 0.5,
            vy: Math.sin(angle) * speed * 0.5,
            color: color,
            life: life,
            maxLife: life
        });
    }

    function spawnBurst(width, height, type, powers) {
        var countRange, speedRange, bandStart, bandEnd, lifeRange;
        if (type === "low") {
            countRange = [24, 36];
            speedRange = [1.2, 2.5];
            bandStart = 0; bandEnd = 0;
            lifeRange = [30, 45];
        } else if (type === "mid") {
            countRange = [16, 24];
            speedRange = [1.8, 3.5];
            bandStart = 1; bandEnd = 1;
            lifeRange = [22, 35];
        } else {
            countRange = [8, 14];
            speedRange = [2.5, 4.5];
            bandStart = 2; bandEnd = 2;
            lifeRange = [12, 20];
        }

        var count = countRange[0] + Math.round(Math.random() * (countRange[1] - countRange[0]));
        var origin = chooseOrigin(width, height);
        for (var i = 0; i < count; i++) {
            var angle = Math.random() * Math.PI * 2;
            var speed = speedRange[0] + Math.random() * (speedRange[1] - speedRange[0]);
            var bandIndex = randomWeightedBand(powers, bandStart, bandEnd);
            var color = bandColorPacked(bandIndex);
            var life = lifeRange[0] + Math.round(Math.random() * (lifeRange[1] - lifeRange[0]));

            algo.particles.push({
                x: origin[0], y: origin[1],
                vx: Math.cos(angle) * speed * 0.5,
                vy: Math.sin(angle) * speed * 0.5,
                color: color,
                life: life, maxLife: life
            });
        }
    }

    function spawnAmbient(width, height, powers) {
        var count = 2 + Math.floor(Math.random() * 2);
        for (var i = 0; i < count; i++) {
            var bandIndex = randomWeightedBand(powers, 0, 2);
            var speed = 0.4 + Math.random() * 0.8;
            var life = 6 + Math.round(Math.random() * 6);
            spawnParticle(width, height, bandColorPacked(bandIndex), speed, life);
        }
    }

    function dominantBandType(audio) {
        return ["low", "mid", "high"][AudioParams.dominantBandIndex(algo, audio)];
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
        var pr = (particle.color >> 16) & 0xFF;
        var pg = (particle.color >> 8) & 0xFF;
        var pb = particle.color & 0xFF;
        addPixel(map, width, height, px, py, RGBUtil.rgb(pr * bri, pg * bri, pb * bri));

        if (particleSize >= 2 && bri > 0.3) {
            var neighborColor = RGBUtil.rgb(pr * bri * 0.5, pg * bri * 0.5, pb * bri * 0.5);
            addPixel(map, width, height, px - 1, py, neighborColor);
            addPixel(map, width, height, px + 1, py, neighborColor);
            addPixel(map, width, height, px, py - 1, neighborColor);
            addPixel(map, width, height, px, py + 1, neighborColor);
        }

        if (particleSize >= 3 && bri > 0.3) {
            var diagonalColor = RGBUtil.rgb(pr * bri * 0.25, pg * bri * 0.25, pb * bri * 0.25);
            addPixel(map, width, height, px - 1, py - 1, diagonalColor);
            addPixel(map, width, height, px + 1, py - 1, diagonalColor);
            addPixel(map, width, height, px - 1, py + 1, diagonalColor);
            addPixel(map, width, height, px + 1, py + 1, diagonalColor);
        }
    }

    function renderAmbientSparkle(map, width, height, powers) {
        var count = Math.max(1, Math.floor(width * height / 80));
        var audioPower = powers[0] + powers[1] + powers[2] + powers[3] + powers[4];
        var brightness = 0.02 + Math.min(0.03, audioPower * 0.01);
        for (var i = 0; i < count; i++) {
            var x = Math.floor(Math.random() * width);
            var y = Math.floor(Math.random() * height);
            var packed = bandColorPacked(randomWeightedBand(powers, 0, 4));
            var sr = (packed >> 16) & 0xFF, sg = (packed >> 8) & 0xFF, sb = packed & 0xFF;
            var sparkle = brightness * (0.5 + Math.random() * 0.5);
            map[y][x] = additive(map[y][x], RGBUtil.rgb(sr * sparkle, sg * sparkle, sb * sparkle));
        }
    }

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        initState();

        var map = RGBUtil.createMap(width, height);
        if (!audio) return map;
        var powers = AudioParams.powerArray(audio);
        var totalPower = AudioParams.totalPower(audio);

        // Kick flash triggers extra bursts
        var kickFlash = AudioParams.kickFired(audio) ? 1.0 : 0.0;
        var onsetIntensity = AudioParams.maxOnsetIntensity(audio);

        if (AudioParams.triggerModeFired(algo, audio))
            spawnBurst(width, height, dominantBandType(audio), powers);

        // Extra burst on strong kicks
        if (kickFlash > 0 && onsetIntensity > 0.6 && algo.particles.length < algo.presetMaxParticles * 0.7)
            spawnBurst(width, height, "low", powers);

        if (algo.particles.length < 10 && totalPower > 0.1)
            spawnAmbient(width, height, powers);

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

        renderAmbientSparkle(map, width, height, powers);

        for (var pi = 0; pi < algo.particles.length; pi++)
            renderParticle(map, width, height, algo.particles[pi], algo.presetParticleSize);

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
