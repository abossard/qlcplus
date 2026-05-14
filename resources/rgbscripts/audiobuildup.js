/*
  Q Light Controller Plus
  audiobuildup.js

  Copyright (c) QLC+ contributors

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

var algo = new Object;
algo.apiVersion = 3;
algo.name = "Audio Buildup";
algo.author = "QLC+ contributors";
algo.acceptColors = 2;  // buildup color + drop color
algo.usesAudio = true;
algo.properties = new Array();

algo.presetCycleBeats = 16.0;
algo.properties.push(
  "name:presetCycleBeats|type:float|display:Cycle (beats)|" +
  "write:setCycleBeats|read:getCycleBeats");

algo.presetDropIntensity = 0.8;
algo.properties.push(
  "name:presetDropIntensity|type:float|display:DropIntensity|" +
  "write:setDropIntensity|read:getDropIntensity");

algo.presetColorScheme = 0;
algo.properties.push(
  "name:presetColorScheme|type:list|display:ColorScheme|" +
  "values:Cool2Warm,Rainbow,Monochrome|write:setColorScheme|read:getColorScheme");

algo.buildColor = {h: 0.583, s: 1.0, v: 1.0};
algo.dropColor  = {h: 0.052, s: 1.0, v: 1.0};

algo.rgbMapStepCount = function(width, height) { return 1; };
algo.rgbMapSetColors = function(rawColors) { };
algo.rgbMapGetColors = function() { return []; };

algo.setCycleBeats = function(_v) { algo.presetCycleBeats = parseFloat(_v); };
algo.getCycleBeats = function() { return algo.presetCycleBeats; };
algo.setDropIntensity = function(_v) { algo.presetDropIntensity = parseFloat(_v); };
algo.getDropIntensity = function() { return algo.presetDropIntensity; };
algo.setColorScheme = function(_v) {
    if (_v === "Rainbow" || parseInt(_v) === 1) algo.presetColorScheme = 1;
    else if (_v === "Monochrome" || parseInt(_v) === 2) algo.presetColorScheme = 2;
    else algo.presetColorScheme = 0;
};
algo.getColorScheme = function() { return ["Cool2Warm", "Rainbow", "Monochrome"][algo.presetColorScheme]; };

// --- Constants (all timing in beats) ---
var DROP_BEATS = 2.0;
var COOLDOWN_BEATS = 2.0;
var DROP_FLASH_FRAC = 0.15;

var ACTIVATION_WIDTH = 3.0;
var BUILD_HUE_COOL = 0.62;
var BUILD_HUE_WARM = 0.03;
var BUILD_RAINBOW_SPREAD = 0.3;
var BUILD_RAINBOW_PROG = 0.5;
var BUILD_BRI_BASE = 0.05;
var BUILD_BRI_ACT = 0.6;
var BUILD_BRI_HEAT = 0.4;
var BUILD_SAT_HIGH = 0.9;
var BUILD_SAT_LOW = 0.3;

var DROP_RING_WIDTH = 3.0;
var DROP_AFTERGLOW_AMP = 0.3;
var DROP_RING_DECAY = 0.5;

var COOLDOWN_BASE_BRI = 0.04;
var COOLDOWN_PULSE_AMP = 0.20;
var COOLDOWN_HUE_AMP = 0.03;
var COOLDOWN_SAT = 0.75;

// --- Helpers ---

function lerp(a, b, t) {
    return a + (b - a) * Math.max(0, Math.min(1, t));
}

function initState() {
    algo.FILLING = 0;
    algo.DROP = 1;
    algo.COOLDOWN = 2;

    algo.state = algo.FILLING;
    algo.beatCount = 0;
    algo.fillProgress = 0;
    algo.lastBeatFired = false;

    algo.dropPhase = { phase: 0 };
    algo.cooldownPhase = { phase: 0 };
    algo.fillSparkPhase = { phase: 0 };
}

function transitionTo(newState) {
    if (algo.state === newState) return;
    algo.state = newState;
    if (newState === algo.FILLING) {
        algo.beatCount = 0;
        algo.fillProgress = 0;
    }
    if (newState === algo.DROP) {
        algo.fillProgress = 1.0;
        algo.dropPhase.phase = 0;
    }
    if (newState === algo.COOLDOWN) {
        algo.cooldownPhase.phase = 0;
    }
}

// --- Renderers ---
function renderFilling(map, width, height, audio, buildCol) {
    var centerX = width / 2.0;
    var maxEdgeDist = Math.floor(width / 2);
    var activeDepth = algo.fillProgress * maxEdgeDist;
    var monoHue = buildCol.h;
    algo.fillSparkPhase.phase = (algo.fillSparkPhase.phase + audio.dt * (1.0 / Math.max(1.0, algo.presetCycleBeats))) % 1.0;
    var shimmer01 = algo.fillSparkPhase.phase;
    var beatPulse = audio.cosPulse;

    for (var y = 0; y < height; y++) {
        for (var x = 0; x < width; x++) {
            var edgeDist = Math.min(x, width - 1 - x);
            var activation = Math.max(0, Math.min(1, 1 - Math.abs(edgeDist - activeDepth) / ACTIVATION_WIDTH));
            var centerHeat = Math.pow(1 - Math.abs(x - centerX) / Math.max(1, centerX), 1.5) *
                             algo.fillProgress;
            var hue;
            if (algo.presetColorScheme === 0)
                hue = lerp(BUILD_HUE_COOL, BUILD_HUE_WARM, algo.fillProgress);
            else if (algo.presetColorScheme === 1)
                hue = ((x / Math.max(1, width)) * BUILD_RAINBOW_SPREAD +
                       algo.fillProgress * BUILD_RAINBOW_PROG +
                       shimmer01) % 1.0;
            else
                hue = monoHue;

            var bri = BUILD_BRI_BASE + activation * BUILD_BRI_ACT + centerHeat * BUILD_BRI_HEAT;
            bri *= (1.0 + 0.15 * beatPulse);
            var sat = lerp(BUILD_SAT_HIGH, BUILD_SAT_LOW, algo.fillProgress);
            HSVUtil.setPixel(map, width, x, y, hue, sat, bri);
        }
    }
}

function renderDrop(map, width, height, audio, dropCol) {
    algo.dropPhase.phase = (algo.dropPhase.phase + audio.dt * (1.0 / DROP_BEATS)) % 1.0;
    var dropProgress = Math.min(1.0, algo.dropPhase.phase);
    var intensity = Math.max(0.1, Math.min(1.0, algo.presetDropIntensity));

    if (dropProgress <= DROP_FLASH_FRAC) {
        for (var fy = 0; fy < height; fy++)
            for (var fx = 0; fx < width; fx++)
                HSVUtil.setPixel(map, width, fx, fy, 0, 0, intensity);
        if (dropProgress >= 1.0) transitionTo(algo.COOLDOWN);
        return;
    }

    var centerX = width / 2.0;
    var ringT = (dropProgress - DROP_FLASH_FRAC) / (1.0 - DROP_FLASH_FRAC);
    var waveRadius = ringT * (width / 2.0 + 4);
    for (var y = 0; y < height; y++) {
        for (var x = 0; x < width; x++) {
            var dist = Math.abs(x - centerX);
            var ringBri = Math.max(0, 1 - Math.abs(dist - waveRadius) / DROP_RING_WIDTH);
            var afterglow = Math.max(0, 1 - ringT) * DROP_AFTERGLOW_AMP;
            var bri = (ringBri * (1 - ringT * DROP_RING_DECAY) + afterglow) * intensity;
            HSVUtil.setPixel(map, width, x, y, dropCol.h, dropCol.s, dropCol.v * bri);
        }
    }

    if (dropProgress >= 1.0) transitionTo(algo.COOLDOWN);
}

function renderCooldown(map, width, height, audio, dropCol) {
    algo.cooldownPhase.phase = (algo.cooldownPhase.phase + audio.dt * (1.0 / COOLDOWN_BEATS)) % 1.0;
    var t = Math.min(1.0, algo.cooldownPhase.phase);
    var afterglow = Math.max(0, 1 - t);
    var beatPulse = audio.cosPulse;
    var baseHue = dropCol.h;
    algo.fillSparkPhase.phase = (algo.fillSparkPhase.phase + audio.dt * (1.0 / (COOLDOWN_BEATS * 2))) % 1.0;
    var hueShift01 = algo.fillSparkPhase.phase;

    for (var y = 0; y < height; y++) {
        for (var x = 0; x < width; x++) {
            var pulse = (COOLDOWN_BASE_BRI + beatPulse * COOLDOWN_PULSE_AMP) * afterglow;
            var hueShift = Math.sin(x * 0.25 + hueShift01 * Math.PI * 2) * COOLDOWN_HUE_AMP;
            HSVUtil.setPixel(map, width, x, y, baseHue + hueShift, COOLDOWN_SAT, pulse);
        }
    }

    if (t >= 1.0) transitionTo(algo.FILLING);
}

algo.rgbMap = function(width, height, rgb, step, audio)
{
    var map = HSVUtil.createMap(width, height);
    if (!audio) return map;

    var buildCol = (algo.colors && algo.colors.length >= 1)
        ? algo.colors[0] : algo.buildColor;
    var dropCol = (algo.colors && algo.colors.length >= 2)
        ? algo.colors[1] : algo.dropColor;

    // Edge-detect beat fires
    var beatNow = !!audio.beatFired;
    var beatEdge = beatNow && !algo.lastBeatFired;
    algo.lastBeatFired = beatNow;

    if (algo.state === algo.FILLING) {
        if (beatEdge) {
            algo.beatCount++;
            algo.fillProgress = algo.beatCount / algo.presetCycleBeats;
            if (algo.fillProgress >= 1.0) {
                transitionTo(algo.DROP);
            }
        }
    }

    if (algo.state === algo.FILLING)
        renderFilling(map, width, height, audio, buildCol);
    else if (algo.state === algo.DROP)
        renderDrop(map, width, height, audio, dropCol);
    else
        renderCooldown(map, width, height, audio, dropCol);

    return map;
};

initState();
testAlgo = algo;
algo;
