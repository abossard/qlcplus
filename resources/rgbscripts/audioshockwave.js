/*
  Q Light Controller Plus
  audioshockwave.js

  Copyright (c) QLC+ contributors

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

var algo = new Object;
algo.apiVersion = 3;
algo.name = "Audio Shockwave";
algo.author = "QLC+ contributors";
algo.acceptColors = 2;  // wave color + bg color
algo.usesAudio = true;
algo.properties = new Array();

    AudioParams.installTrigger(algo, {gain: 7, reactivity: 7, sensitivity: 7});

algo.presetMaxWaves = 6;
algo.properties.push(
  "name:presetMaxWaves|type:range|display:MaxWaves|" +
  "values:1,12|write:setMaxWaves|read:getMaxWaves");
algo.presetWaveWidth = 2;
algo.properties.push(
  "name:presetWaveWidth|type:range|display:WaveWidth|" +
  "values:1,5|write:setWaveWidth|read:getWaveWidth");
algo.presetSpeed = 5;
algo.properties.push(
  "name:presetSpeed|type:range|display:Speed|" +
  "values:1,10|write:setSpeed|read:getSpeed");
algo.presetDecay = 5;
algo.properties.push(
  "name:presetDecay|type:range|display:Decay|" +
  "values:1,10|write:setDecay|read:getDecay");

algo.waves = new Array();
algo.bassFilter = null;
algo.frame = 0;

var waveColor = [255, 255, 255];
var bgColor = [0, 16, 48];

algo.rgbMapStepCount = function(width, height) { return 1; };
algo.rgbMapSetColors = function(rawColors) {
    if (rawColors && rawColors.length >= 1)
        waveColor = unpackColor(rawColors[0]);
    if (rawColors && rawColors.length >= 2)
        bgColor = unpackColor(rawColors[1]);
};
algo.rgbMapGetColors = function() { return []; };

algo.setMaxWaves = function(_v) { algo.presetMaxWaves = clampInt(_v, 1, 12, 6); };
algo.getMaxWaves = function() { return algo.presetMaxWaves; };
algo.setWaveWidth = function(_v) { algo.presetWaveWidth = clampInt(_v, 1, 5, 2); };
algo.getWaveWidth = function() { return algo.presetWaveWidth; };
algo.setSpeed = function(_v) { algo.presetSpeed = clampInt(_v, 1, 10, 5); };
algo.getSpeed = function() { return algo.presetSpeed; };
algo.setDecay = function(_v) { algo.presetDecay = clampInt(_v, 1, 10, 5); };
algo.getDecay = function() { return algo.presetDecay; };

function clamp(value, minValue, maxValue) {
    return Math.max(minValue, Math.min(maxValue, value));
}

function clampInt(value, minValue, maxValue, defaultValue) {
    var parsed = parseInt(value);
    if (isNaN(parsed)) parsed = defaultValue;
    return Math.max(minValue, Math.min(maxValue, parsed));
}

function unpackColor(color) {
    return [(color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF];
}



function spawnWave(width, height, intensity) {
    algo.waves.push({
        cx: width / 2,
        cy: height / 2,
        radius: 0,
        maxRadius: Math.sqrt(width * width + height * height),
        intensity: clamp(intensity * 1.5, 0, 1.5),
        birth: algo.frame
    });

    while (algo.waves.length > algo.presetMaxWaves)
        algo.waves.shift();
}

function renderAmbient(map, width, height, cx, cy, maxRadius) {
    var phase = algo.frame * 0.12;
    for (var y = 0; y < height; y++) {
        for (var x = 0; x < width; x++) {
            var dist = Math.sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy));
            var ring = Math.sin(dist * 0.65 - phase) * 0.5 + 0.5;
            var centerLift = 1.0 - Math.min(1.0, dist / Math.max(1.0, maxRadius));
            var bri = 0.08 + ring * 0.12 + centerLift * 0.08;
            map[y][x] = RGBUtil.rgb(bgColor[0] * bri, bgColor[1] * bri, bgColor[2] * bri);
        }
    }
}

algo.rgbMap = function(width, height, rgb, step, audio)
{
    algo.frame++;

    var map = RGBUtil.createMap(width, height);
    if (!audio || !audio.mel || audio.mel.length === 0) return map;
    var cx = width / 2;
    var cy = height / 2;
    var maxRadius = Math.sqrt(width * width + height * height);
    renderAmbient(map, width, height, cx, cy, maxRadius);

    var bass = clamp(audio.lows, 0, 1);

    if (audio.triggers.low.firedThisFrame || AudioParams.kickFired(audio))
        spawnWave(width, height, Math.max(0.5, bass));

    if (algo.waves.length < 3 && bass > 0.1)
        spawnWave(width, height, 0.6);

    var total = new Array(height);
    for (var ty = 0; ty < height; ty++) {
        total[ty] = new Array(width);
        for (var tx = 0; tx < width; tx++)
            total[ty][tx] = 0;
    }

    var waveWidth = algo.presetWaveWidth;
    var onsetIntensity = Math.max(0.4, AudioParams.maxOnsetIntensity(audio));
    for (var wi = 0; wi < algo.waves.length; wi++) {
        var wave = algo.waves[wi];
        var fade = Math.max(0, 1 - wave.radius / Math.max(1, wave.maxRadius));

        for (var y = 0; y < height; y++) {
            for (var x = 0; x < width; x++) {
                var dist = Math.sqrt((x - wave.cx) * (x - wave.cx) + (y - wave.cy) * (y - wave.cy));
                var ringDist = Math.abs(dist - wave.radius);
                if (ringDist < waveWidth) {
                    var ringBri = (1 - ringDist / waveWidth);
                    ringBri = Math.sqrt(ringBri) * wave.intensity * fade * onsetIntensity;
                    total[y][x] += ringBri;
                }
            }
        }
    }

    for (var py = 0; py < height; py++) {
        for (var px = 0; px < width; px++) {
            var totalBri = clamp(total[py][px], 0, 1.5);
            if (totalBri > 0.005)
                map[py][px] = RGBUtil.rgb(waveColor[0] * totalBri, waveColor[1] * totalBri, waveColor[2] * totalBri);
        }
    }

    var speed = algo.presetSpeed * 0.5;
    var decayFactor = 1 - algo.presetDecay * 0.03;
    for (var ui = algo.waves.length - 1; ui >= 0; ui--) {
        algo.waves[ui].radius += speed;
        algo.waves[ui].intensity *= decayFactor;
        if (algo.waves[ui].intensity < 0.01 || algo.waves[ui].radius > algo.waves[ui].maxRadius)
            algo.waves.splice(ui, 1);
    }

    return map;
};

algo;
