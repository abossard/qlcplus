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

algo.presetSensitivity = 6;
    algo.properties.push(
      "name:presetSensitivity|type:range|display:Sensitivity|" +
      "values:1,10|write:setSensitivity|read:getSensitivity");

algo.presetDropIntensity = 8;
algo.properties.push(
  "name:presetDropIntensity|type:range|display:DropIntensity|" +
  "values:1,10|write:setDropIntensity|read:getDropIntensity");
algo.presetBuildSpeed = 5;
algo.properties.push(
  "name:presetBuildSpeed|type:range|display:BuildSpeed|" +
  "values:1,10|write:setBuildSpeed|read:getBuildSpeed");
algo.presetColorScheme = 0;
algo.properties.push(
  "name:presetColorScheme|type:list|display:ColorScheme|" +
  "values:Cool2Warm,Rainbow,Monochrome|write:setColorScheme|read:getColorScheme");
algo.presetAutoTune = 1;
algo.properties.push(
  "name:presetAutoTune|type:list|display:AutoTune|" +
  "values:Yes,No|write:setAutoTune|read:getAutoTune");

algo.buildColor = [0, 128, 255];
algo.dropColor = [255, 80, 0];

algo.rgbMapStepCount = function(width, height) { return 1; };
algo.rgbMapSetColors = function(rawColors) {
    if (rawColors && rawColors.length >= 1)
        algo.buildColor = unpackColor(rawColors[0]);
    if (rawColors && rawColors.length >= 2)
        algo.dropColor = unpackColor(rawColors[1]);
};
algo.rgbMapGetColors = function() { return []; };

algo.setDropIntensity = function(_v) { algo.presetDropIntensity = clampInt(_v, 1, 10, 8); };
algo.getDropIntensity = function() { return algo.presetDropIntensity; };
algo.setBuildSpeed = function(_v) { algo.presetBuildSpeed = clampInt(_v, 1, 10, 5); };
algo.getBuildSpeed = function() { return algo.presetBuildSpeed; };
algo.setColorScheme = function(_v) {
    if (_v === "Rainbow" || parseInt(_v) === 1) algo.presetColorScheme = 1;
    else if (_v === "Monochrome" || parseInt(_v) === 2) algo.presetColorScheme = 2;
    else algo.presetColorScheme = 0;
};
algo.getColorScheme = function() { return ["Cool2Warm", "Rainbow", "Monochrome"][algo.presetColorScheme]; };
algo.setAutoTune = function(_v) { algo.presetAutoTune = (_v === "No" || parseInt(_v) === 0) ? 0 : 1; };
algo.getAutoTune = function() { return algo.presetAutoTune ? "Yes" : "No"; };

    algo.setSensitivity = function(_v) { algo.presetSensitivity = parseInt(_v); };
    algo.getSensitivity = function() { return algo.presetSensitivity; };
function clamp(value, minValue, maxValue) {
    return Math.max(minValue, Math.min(maxValue, value));
}

function clampInt(value, minValue, maxValue, defaultValue) {
    var parsed = parseInt(value);
    if (isNaN(parsed)) parsed = defaultValue;
    return Math.max(minValue, Math.min(maxValue, parsed));
}

function lerp(a, b, t) {
    return a + (b - a) * clamp(t, 0, 1);
}

function unpackColor(color) {
    return [(color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF];
}

function colorHue(color) {
    var r = color[0] / 255.0, g = color[1] / 255.0, b = color[2] / 255.0;
    var max = Math.max(r, g, b), min = Math.min(r, g, b);
    var d = max - min;
    if (d <= 0.0001) return 0;
    var h;
    if (max === r) h = ((g - b) / d) % 6;
    else if (max === g) h = ((b - r) / d) + 2;
    else h = ((r - g) / d) + 4;
    return ((h / 6) + 1) % 1;
}

function packHsv(h, s, v) {
    var c = RGBUtil.hsv2rgb(h, s, clamp(v, 0, 1));
    return RGBUtil.rgb(c[0], c[1], c[2]);
}

function packScaled(color, brightness) {
    brightness = clamp(brightness, 0, 1);
    return RGBUtil.rgb(color[0] * brightness, color[1] * brightness, color[2] * brightness);
}

function initState() {
    algo.IDLE = 0;
    algo.BUILDING = 1;
    algo.PEAK = 2;
    algo.DROP = 3;
    algo.POST_DROP = 4;

    algo.state = algo.IDLE;
    algo.stateFrames = 0;
    algo.frame = 0;
    algo.cooldown = 0;
    algo.buildProgress = 0;
    algo.bassAbsentFrames = 0;
    algo.dropArmed = false;

    algo.featureMin = {
        energyTrend: -0.05,
        highRatio: 0.0,
        flux: 0.0,
        lows: 0.0
    };
    algo.featureMax = {
        energyTrend: 0.25,
        highRatio: 0.6,
        flux: 0.25,
        lows: 0.6
    };

    algo.history = new Array(150);
    for (var h = 0; h < algo.history.length; h++) algo.history[h] = 0;
    algo.historyIndex = 0;
}

function transitionTo(newState) {
    if (algo.state === newState) return;
    algo.state = newState;
    algo.stateFrames = 0;
    if (newState === algo.BUILDING)
        algo.buildProgress = 0;
    if (newState === algo.DROP)
        algo.buildProgress = 1;
    if (newState === algo.IDLE) {
        algo.buildProgress = 0;
        algo.dropArmed = false;
        algo.bassAbsentFrames = 0;
    }
}

function updateCalibration(name, value) {
    algo.featureMin[name] = Math.min(algo.featureMin[name] * 0.995 + value * 0.005, value);
    algo.featureMax[name] = Math.max(algo.featureMax[name] * 0.995 + value * 0.005, value);
}

function normalizeFeature(name, value, fallbackScale, fallbackOffset) {
    if (algo.presetAutoTune) {
        return clamp((value - algo.featureMin[name]) /
                     Math.max(0.001, algo.featureMax[name] - algo.featureMin[name]), 0, 1);
    }
    return clamp((value + fallbackOffset) * fallbackScale, 0, 1);
}

function extractFeatures(audio) {
    var rawLows = audio.power.low;
    var rawMids = audio.power.mid;
    var rawHighs = audio.power.high;
    var rawTotal = rawLows + rawMids + rawHighs + 0.001;

    var energyTrend = audio.features.flux;
    var highRatio = rawHighs / rawTotal;

    var flux = audio.features.flux;

    if (algo.presetAutoTune && algo.state === algo.IDLE) {
        updateCalibration("energyTrend", energyTrend);
        updateCalibration("highRatio", highRatio);
        updateCalibration("flux", flux);
        updateCalibration("lows", rawLows);
    }

    var energyTrendNorm = normalizeFeature("energyTrend", energyTrend, 2.8, 0.02);
    var highRatioNorm = normalizeFeature("highRatio", highRatio, 1.2, 0);
    var fluxNorm = normalizeFeature("flux", flux, 5.0, 0);
    var lowsNorm = normalizeFeature("lows", rawLows, 1.4, 0);

    // Add onset intensity to buildup signal
    var onsetBoost = audio.onset.intensity * 0.15;
    var buildScoreRaw = 0.35 * energyTrendNorm +
                        0.20 * highRatioNorm +
                        0.25 * fluxNorm +
                        0.20 * (1 - lowsNorm) +
                        onsetBoost;
    var buildScore = buildScoreRaw;

    var featureVotes = 0;
    if (energyTrendNorm > 0.4) featureVotes++;
    if (highRatioNorm > 0.5) featureVotes++;
    if (fluxNorm > 0.4) featureVotes++;
    if (lowsNorm < 0.3) featureVotes++;

    if (lowsNorm < 0.15)
        algo.bassAbsentFrames++;
    else
        algo.bassAbsentFrames = 0;
    if (algo.bassAbsentFrames > 10)
        algo.dropArmed = true;

    var dropDetected = algo.dropArmed && (audio.bands.low.fired || audio.beat.kick);

    algo.history[algo.historyIndex] = buildScore;
    algo.historyIndex = (algo.historyIndex + 1) % algo.history.length;

    return {
        rawLows: rawLows,
        energyTrendNorm: energyTrendNorm,
        highRatioNorm: highRatioNorm,
        fluxNorm: fluxNorm,
        lowsNorm: lowsNorm,
        midsNorm: clamp(rawMids * 1.5, 0, 1),
        highsNorm: clamp(rawHighs * 1.5, 0, 1),
        buildScore: buildScore,
        featureVotes: featureVotes,
        bassAbsent: algo.bassAbsentFrames > 10,
        dropDetected: dropDetected
    };
}

function updateState(features) {
    var sens = algo.presetSensitivity / 10.0;
    var buildEnterThresh = lerp(0.65, 0.35, sens);
    var peakThresh = lerp(0.80, 0.55, sens);

    if (algo.cooldown > 0)
        algo.cooldown--;

    if (algo.state === algo.IDLE) {
        algo.buildProgress = Math.max(0, algo.buildProgress - 0.03);
        if (features.buildScore > buildEnterThresh &&
            features.featureVotes >= 2 &&
            algo.cooldown === 0) {
            transitionTo(algo.BUILDING);
        }
    } else if (algo.state === algo.BUILDING) {
        if (features.buildScore > peakThresh && algo.stateFrames > 25)
            transitionTo(algo.PEAK);
        else if (algo.stateFrames > 200)
            transitionTo(features.buildScore > buildEnterThresh ? algo.PEAK : algo.IDLE);
        else if (features.buildScore < buildEnterThresh * 0.6 && algo.stateFrames > 50)
            transitionTo(algo.IDLE);
    } else if (algo.state === algo.PEAK) {
        algo.buildProgress = 1.0;
        algo.dropArmed = algo.dropArmed || features.bassAbsent;
        if (features.dropDetected || algo.stateFrames > 35)
            transitionTo(algo.DROP);
    } else if (algo.state === algo.DROP) {
        if (algo.stateFrames >= 20)
            transitionTo(algo.POST_DROP);
    } else if (algo.state === algo.POST_DROP) {
        if (algo.stateFrames >= 35) {  // shorter post-drop
            transitionTo(algo.IDLE);
            algo.cooldown = 75;
        }
    }
}

function renderIdle(map, width, height, features) {
    var baseHue = colorHue(algo.buildColor);
    var bass = features.lowsNorm;
    var mids = features.midsNorm;
    var highs = features.highsNorm;
    var centerX = width / 2.0;
    var centerY = height / 2.0;

    for (var y = 0; y < height; y++) {
        for (var x = 0; x < width; x++) {
            // Spatial wave — moves with audio
            var wave1 = Math.sin(x * 0.4 + algo.frame * 0.06) * 0.5 + 0.5;
            var wave2 = Math.sin(y * 0.6 - algo.frame * 0.04 + x * 0.2) * 0.5 + 0.5;

            // Bass pulses from center outward
            var dist = Math.abs(x - centerX) / Math.max(1, centerX);
            var bassPulse = (1 - dist) * bass * 0.5;

            // Highs sparkle at edges
            var edgeSpark = dist * highs * wave1 * 0.3;

            // Mids create horizontal bands
            var midBand = mids * wave2 * 0.2;

            var bri = 0.04 + bassPulse + edgeSpark + midBand;
            bri = clamp(bri, 0.02, 0.6);

            // Hue shifts with position and audio
            var hueShift = wave1 * 0.08 + bass * 0.05 - highs * 0.05;
            var sat = 0.6 + mids * 0.3;

            map[y][x] = packHsv(baseHue + hueShift, sat, bri);
        }
    }
}

function renderBuilding(map, width, height, features) {
    var buildSpeedFactor = lerp(0.5, 1.8, algo.presetBuildSpeed / 10.0);
    algo.buildProgress = clamp(algo.buildProgress +
                               features.buildScore * buildSpeedFactor * 0.02, 0, 1);

    var centerX = width / 2.0;
    var maxEdgeDist = Math.floor(width / 2);
    var activeDepth = algo.buildProgress * maxEdgeDist;
    var colorHueValue = colorHue(algo.buildColor);

    for (var y = 0; y < height; y++) {
        for (var x = 0; x < width; x++) {
            var edgeDist = Math.min(x, width - 1 - x);
            var activation = clamp(1 - Math.abs(edgeDist - activeDepth) / 3.0, 0, 1);
            var centerHeat = Math.pow(1 - Math.abs(x - centerX) / Math.max(1, centerX), 1.5) *
                             algo.buildProgress;
            var hue;
            if (algo.presetColorScheme === 0)
                hue = lerp(0.62, 0.03, algo.buildProgress);
            else if (algo.presetColorScheme === 1)
                hue = ((x / Math.max(1, width)) * 0.3 + algo.buildProgress * 0.5 + algo.frame * 0.01) % 1.0;
            else
                hue = colorHueValue;

            var bri = 0.05 + activation * 0.6 + centerHeat * 0.4;
            var sat = lerp(0.9, 0.3, algo.buildProgress);
            map[y][x] = packHsv(hue, sat, bri);
        }
    }
}

function renderPeak(map, width, height, features) {
    algo.buildProgress = 1.0;
    var centerX = (width - 1) / 2.0;
    var centerCols = (width >= 12) ? 4 : 2;
    var flicker = 0.8 + 0.2 * Math.sin(algo.frame * 0.5);
    var highFlicker = 0.85 + features.highsNorm * 0.15;
    var hue = (algo.presetColorScheme === 2) ? colorHue(algo.buildColor) : 0.08;

    for (var y = 0; y < height; y++) {
        for (var x = 0; x < width; x++) {
            var dist = Math.abs(x - centerX);
            var center = dist <= centerCols / 2.0;
            var bri = center ? 1.0 : 0.4 + Math.max(0, 1 - dist / Math.max(1, centerX)) * 0.2;
            bri *= flicker * highFlicker;
            map[y][x] = packHsv(hue, 0.12, bri);
        }
    }
}

function renderDrop(map, width, height) {
    var dropProgress = clamp(algo.stateFrames / 20.0, 0, 1);
    var intensity = clamp(algo.presetDropIntensity / 10.0, 0.1, 1.0);

    if (algo.stateFrames <= 3) {
        var flash = RGBUtil.rgb(255 * intensity, 255 * intensity, 255 * intensity);
        for (var fy = 0; fy < height; fy++)
            for (var fx = 0; fx < width; fx++)
                map[fy][fx] = flash;
        return;
    }

    var centerX = width / 2.0;
    var waveRadius = dropProgress * (width / 2.0 + 4);
    for (var y = 0; y < height; y++) {
        for (var x = 0; x < width; x++) {
            var dist = Math.abs(x - centerX);
            var ringBri = Math.max(0, 1 - Math.abs(dist - waveRadius) / 3.0);
            var afterglow = Math.max(0, 1 - dropProgress) * 0.3;
            var bri = (ringBri * (1 - dropProgress * 0.5) + afterglow) * intensity;
            map[y][x] = packScaled(algo.dropColor, bri);
        }
    }
}

function renderPostDrop(map, width, height, features) {
    var afterglow = Math.max(0, 1 - algo.stateFrames / 50.0);
    var baseHue = colorHue(algo.dropColor);
    for (var y = 0; y < height; y++) {
        for (var x = 0; x < width; x++) {
            var bassPulse = (0.04 + features.lowsNorm * 0.10) * afterglow;
            var hueShift = Math.sin(x * 0.25 + algo.frame * 0.015) * 0.03;
            map[y][x] = packHsv(baseHue + hueShift, 0.75, bassPulse);
        }
    }
}

algo.rgbMap = function(width, height, rgb, step, audio)
{
    var map = RGBUtil.createMap(width, height);
    if (!audio) return map;
    var features = extractFeatures(audio);

    updateState(features);

    if (algo.state === algo.IDLE)
        renderIdle(map, width, height, features);
    else if (algo.state === algo.BUILDING)
        renderBuilding(map, width, height, features);
    else if (algo.state === algo.PEAK)
        renderPeak(map, width, height, features);
    else if (algo.state === algo.DROP)
        renderDrop(map, width, height);
    else
        renderPostDrop(map, width, height, features);

    algo.frame++;
    algo.stateFrames++;

    return map;
};

initState();
testAlgo = algo;
algo;
