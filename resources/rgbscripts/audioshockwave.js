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

algo.presetAmbientSpeed = 0.12;
algo.properties.push(
  "name:presetAmbientSpeed|type:float|display:Ambient Speed|" +
  "write:setAmbientSpeed|read:getAmbientSpeed");
algo.presetWaveDecay = 0.03;
algo.properties.push(
  "name:presetWaveDecay|type:float|display:Wave Decay|" +
  "write:setWaveDecay|read:getWaveDecay");

algo.waves = new Array();
algo.dtAccum = 0;

var DOMINANT_TINT = 0.6;
var AMBIENT_RING_FREQ = 0.65;
var AMBIENT_BASE_BRI = 0.08;
var AMBIENT_RING_BRI = 0.12;
var AMBIENT_CENTER_BRI = 0.08;
var SPEED_SCALE = 0.5;
var KICK_INTENSITY_BOOST = 1.5;
var MAX_INTENSITY = 1.5;
var MIN_BASS_FOR_FILL = 0.1;
var FILL_INTENSITY = 0.6;
var MAX_FILL_WAVES = 3;

var MIN_RENDER_BRI = 0.005;
var MIN_WAVE_INTENSITY = 0.01;

var waveColorPacked = 0xFFFFFF;
var bgColor = [0, 16, 48];
var lastW = 0, lastH = 0;

algo.rgbMapStepCount = function(width, height) { return 1; };
algo.rgbMapSetColors = function(rawColors) {
    if (rawColors && rawColors.length >= 1)
        waveColorPacked = rawColors[0] & 0xFFFFFF;
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

algo.setAmbientSpeed = function(_v) { algo.presetAmbientSpeed = parseFloat(_v); };
algo.getAmbientSpeed = function() { return algo.presetAmbientSpeed; };
algo.setWaveDecay = function(_v) { algo.presetWaveDecay = parseFloat(_v); };
algo.getWaveDecay = function() { return algo.presetWaveDecay; };
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

function spawnWave(width, height, intensity, audio) {
    var dominantPacked = AudioColors.dominantColor(algo, audio, waveColorPacked, 0.05);
    var color = AudioColors.blendPacked(waveColorPacked, dominantPacked, DOMINANT_TINT);
    algo.waves.push({
        cx: width / 2,
        cy: height / 2,
        radius: 0,
        maxRadius: Math.sqrt(width * width + height * height),
        intensity: clamp(intensity * KICK_INTENSITY_BOOST, 0, MAX_INTENSITY),
        color: color
    });

    while (algo.waves.length > algo.presetMaxWaves)
        algo.waves.shift();
}

function renderAmbient(map, width, height, cx, cy, maxRadius) {
    var phase = algo.dtAccum * algo.presetAmbientSpeed * 0.025;
    for (var y = 0; y < height; y++) {
        for (var x = 0; x < width; x++) {
            var dist = Math.sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy));
            var ring = Math.sin(dist * AMBIENT_RING_FREQ - phase) * 0.5 + 0.5;
            var centerLift = 1.0 - Math.min(1.0, dist / Math.max(1.0, maxRadius));
            var bri = AMBIENT_BASE_BRI + ring * AMBIENT_RING_BRI + centerLift * AMBIENT_CENTER_BRI;
            map[(y) * width + (x)] = RGBUtil.rgb(bgColor[0] * bri, bgColor[1] * bri, bgColor[2] * bri);
        }
    }
}

algo.rgbMap = function(width, height, rgb, step, audio)
{
    var dtMs = audio && audio.timing && audio.timing.consumerDtMs > 0 ? audio.timing.consumerDtMs : 40;
    var frameScale = dtMs / 40;
    algo.dtAccum += dtMs;
    if (width !== lastW || height !== lastH) {
        algo.waves = [];
        lastW = width;
        lastH = height;
    }

    var map = RGBUtil.createFlatMap(width, height);
    if (!audio) return map;
    var cx = width / 2;
    var cy = height / 2;
    var maxRadius = Math.sqrt(width * width + height * height);
    renderAmbient(map, width, height, cx, cy, maxRadius);

    var bass = audio.power.low;

    if (audio.bands.low.fired || audio.beat.kick || audio.onset.fired)
        spawnWave(width, height, Math.max(0.5, bass), audio);

    if (algo.waves.length < MAX_FILL_WAVES && bass > MIN_BASS_FOR_FILL)
        spawnWave(width, height, FILL_INTENSITY, audio);

    var totalR = new Array(height);
    var totalG = new Array(height);
    var totalB = new Array(height);
    for (var ty = 0; ty < height; ty++) {
        totalR[ty] = new Array(width);
        totalG[ty] = new Array(width);
        totalB[ty] = new Array(width);
        for (var tx = 0; tx < width; tx++) {
            totalR[ty][tx] = 0;
            totalG[ty][tx] = 0;
            totalB[ty][tx] = 0;
        }
    }

    var waveWidth = algo.presetWaveWidth;
    var onsetIntensity = audio.onset.intensity;
    for (var wi = 0; wi < algo.waves.length; wi++) {
        var wave = algo.waves[wi];
        var fade = Math.max(0, 1 - wave.radius / Math.max(1, wave.maxRadius));
        var wr = (wave.color >> 16) & 0xFF;
        var wg = (wave.color >> 8) & 0xFF;
        var wb = wave.color & 0xFF;

        for (var y = 0; y < height; y++) {
            for (var x = 0; x < width; x++) {
                var dist = Math.sqrt((x - wave.cx) * (x - wave.cx) + (y - wave.cy) * (y - wave.cy));
                var ringDist = Math.abs(dist - wave.radius);
                if (ringDist < waveWidth) {
                    var ringBri = (1 - ringDist / waveWidth);
                    ringBri = Math.sqrt(ringBri) * wave.intensity * fade * onsetIntensity;
                    totalR[y][x] += wr * ringBri;
                    totalG[y][x] += wg * ringBri;
                    totalB[y][x] += wb * ringBri;
                }
            }
        }
    }

    for (var py = 0; py < height; py++) {
        for (var px = 0; px < width; px++) {
            var rr = totalR[py][px], gg = totalG[py][px], bb = totalB[py][px];
            var peak = Math.max(rr, gg, bb) / 255.0;
            if (peak > MIN_RENDER_BRI) {
                var scale = peak > 1.5 ? 1.5 / peak : 1.0;
                map[(py) * width + (px)] = RGBUtil.rgb(rr * scale, gg * scale, bb * scale);
            }
        }
    }

    var speed = algo.presetSpeed * SPEED_SCALE;
    var decayFactor = 1 - algo.presetDecay * algo.presetWaveDecay;
    for (var ui = algo.waves.length - 1; ui >= 0; ui--) {
        algo.waves[ui].radius += speed * frameScale;
        algo.waves[ui].intensity *= Math.pow(decayFactor, frameScale);
        if (algo.waves[ui].intensity < MIN_WAVE_INTENSITY || algo.waves[ui].radius > algo.waves[ui].maxRadius)
            algo.waves.splice(ui, 1);
    }

    return map;
};

algo;
