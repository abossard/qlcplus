/*
  Q Light Controller Plus
  audioplasma.js

  Copyright (c) QLC+ contributors
  Ported from LedFX "Plasma2d" effect (MIT License)

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
    algo.name = "Audio Plasma";
    algo.author = "Ported from LedFx";
    algo.acceptColors = 5;
    algo.usesAudio = true;
    algo.properties = new Array();
    algo.presetMode = "Artistic";
    algo.properties.push("name:mode|type:list|display:Response|values:Artistic,Plasma2d,PlasmaWled2d|write:setMode|read:getMode");
    algo.setMode = function(v) {
        var next = (v === "Plasma2d" || v === "PlasmaWled2d") ? v : "Artistic";
        if (next !== algo.presetMode) resetSourceModeState();
        algo.presetMode = next;
    };
    algo.getMode = function() { return algo.presetMode; };

    algo.density = 0.5;
    algo.lower = 0.01;
    algo.density_vertical = 0.1;
    algo.twist = 0.07;
    algo.radius = 0.2;
    algo.frequency_range = "Lows (beat+bass)";

    algo.properties.push("name:density|type:float|display:Density|write:setDensity|read:getDensity");
    algo.properties.push("name:lower|type:float|display:Lower|write:setLower|read:getLower");
    algo.properties.push("name:density_vertical|type:float|display:Vertical Density|write:setDensityVertical|read:getDensityVertical");
    algo.properties.push("name:twist|type:float|display:Twist|write:setTwist|read:getTwist");
    algo.properties.push("name:radius|type:float|display:Radius|write:setRadius|read:getRadius");
    algo.properties.push("name:frequency_range|type:list|display:Frequency Range|values:Beat,Bass,Lows (beat+bass),Mids,High|write:setFrequencyRange|read:getFrequencyRange");
    algo.presetSpeedDivisor = 32;
    algo.properties.push("name:presetSpeedDivisor|type:range|display:Wled Speed Divisor|values:0,255|write:setSpeedDivisor|read:getSpeedDivisor");
    algo.presetHorizontalStretch = 100;
    algo.properties.push("name:presetHorizontalStretch|type:range|display:Wled Horizontal Stretch|values:0,255|write:setHorizontalStretch|read:getHorizontalStretch");
    algo.presetVerticalStretch = 100;
    algo.properties.push("name:presetVerticalStretch|type:range|display:Wled Vertical Stretch|values:0,255|write:setVerticalStretch|read:getVerticalStretch");
    algo.presetSizeMultiplier = 1;
    algo.properties.push("name:presetSizeMultiplier|type:float|values:0,1|display:Wled Size Multiplier|write:setSizeMultiplier|read:getSizeMultiplier");
    algo.presetSpeedMultiplier = 1;
    algo.properties.push("name:presetSpeedMultiplier|type:float|values:0,1|display:Wled Speed Multiplier|write:setSpeedMultiplier|read:getSpeedMultiplier");

    algo.setDensity = function(v) { algo.density = parseFloat(v); };
    algo.getDensity = function() { return algo.density; };
    algo.setLower = function(v) { algo.lower = parseFloat(v); };
    algo.getLower = function() { return algo.lower; };
    algo.setDensityVertical = function(v) { algo.density_vertical = parseFloat(v); };
    algo.getDensityVertical = function() { return algo.density_vertical; };
    algo.setTwist = function(v) { algo.twist = parseFloat(v); };
    algo.getTwist = function() { return algo.twist; };
    algo.setRadius = function(v) { algo.radius = parseFloat(v); };
    algo.getRadius = function() { return algo.radius; };
    algo.setFrequencyRange = function(v) { algo.frequency_range = String(v); };
    algo.getFrequencyRange = function() { return algo.frequency_range; };
    function parseByteControl(v, fallback) {
        var value = parseInt(v, 10);
        if (!isFinite(value)) value = fallback;
        return Math.max(0, Math.min(255, value));
    }
    algo.setSpeedDivisor = function(v) { algo.presetSpeedDivisor = parseByteControl(v, 32); };
    algo.getSpeedDivisor = function() { return algo.presetSpeedDivisor; };
    algo.setHorizontalStretch = function(v) { algo.presetHorizontalStretch = parseByteControl(v, 100); };
    algo.getHorizontalStretch = function() { return algo.presetHorizontalStretch; };
    algo.setVerticalStretch = function(v) { algo.presetVerticalStretch = parseByteControl(v, 100); };
    algo.getVerticalStretch = function() { return algo.presetVerticalStretch; };
    algo.setSizeMultiplier = function(v) { algo.presetSizeMultiplier = Math.max(0, parseFloat(v) || 0); };
    algo.getSizeMultiplier = function() { return algo.presetSizeMultiplier; };
    algo.setSpeedMultiplier = function(v) { algo.presetSpeedMultiplier = Math.max(0, parseFloat(v) || 0); };
    algo.getSpeedMultiplier = function() { return algo.presetSpeedMultiplier; };

    algo.presetSmoothing = 5;
    algo.properties.push("name:presetSmoothing|type:range|display:Smoothing|values:1,10|write:setSmoothing|read:getSmoothing");
    algo.setSmoothing = function(v) { algo.presetSmoothing = parseInt(v); };
    algo.getSmoothing = function() { return algo.presetSmoothing; };

    var smoothPower = 0;
    var position = 0;

    var sourceModeState = {
        mode: "",
        audioIdentity: "",
        geometryKey: "",
        seconds: 0,
        motion: 0,
        freeSeconds: 0
    };
    var SIN8 = new Array(256);
    var COS8 = new Array(256);
    for (var lut = 0; lut < 256; lut++) {
        SIN8[lut] = Math.max(0, Math.min(255, Math.floor(Math.sin(lut * (2 * Math.PI / 255)) * 127.5 + 127.5)));
        COS8[lut] = Math.max(0, Math.min(255, Math.floor(Math.cos(lut * (2 * Math.PI / 255)) * 127.5 + 127.5)));
    }

    function powerFor(audio) {
        if (algo.frequency_range === "Beat") return audio.beat;
        if (algo.frequency_range === "Bass") return audio.bass;
        if (algo.frequency_range === "Mids") return audio.mid;
        if (algo.frequency_range === "High") return audio.high;
        return audio.low; // Lows (beat+bass)
    }

    // Coordinate ranges match np.ogrid[0:min(W,W*scale):complex(W)].
    function plasmaValues(width, height, power, time) {
        var scale = algo.lower + (power * algo.density);
        var xStep = (width > 1) ? Math.min(width, width * scale) / (width - 1) : 0;
        var yStep = (height > 1) ? Math.min(height, height * scale) / (height - 1) : 0;
        var values = new Array(width * height);
        for (var iy = 0; iy < height; iy++) {
            var y = iy * yStep;
            for (var ix = 0; ix < width; ix++) {
                var x = ix * xStep;
                values[iy * width + ix] =
                    Math.sin(x * 0.1 + time) * Math.cos(y * 0.1 - time) +
                    Math.sin((x * algo.density_vertical + y * algo.twist + time) * 2.5) +
                    Math.sin(Math.sqrt(x * x + y * y) * algo.radius - time);
            }
        }
        return values;
    }

    function resetSourceModeState() {
        sourceModeState.mode = "";
        sourceModeState.audioIdentity = "";
        sourceModeState.geometryKey = "";
        sourceModeState.seconds = 0;
        sourceModeState.motion = 0;
        sourceModeState.freeSeconds = 0;
    }

    function syncSourceModeState(mode, width, height, audio) {
        var nextAudioIdentity = HSVUtil.audioIdentityKey(audio, sourceModeState.audioIdentity);
        var nextGeometryKey = width + "x" + height;
        if (sourceModeState.mode !== mode ||
            sourceModeState.audioIdentity !== nextAudioIdentity ||
            sourceModeState.geometryKey !== nextGeometryKey) {
            sourceModeState.seconds = 0;
            sourceModeState.motion = 0;
            sourceModeState.freeSeconds = 0;
        }
        sourceModeState.mode = mode;
        sourceModeState.audioIdentity = nextAudioIdentity;
        sourceModeState.geometryKey = nextGeometryKey;
    }

    function truncToUint8(value) {
        if (!isFinite(value)) return 0;
        var t = value < 0 ? Math.ceil(value) : Math.floor(value);
        var wrapped = t % 256;
        if (wrapped < 0) wrapped += 256;
        return wrapped;
    }

    function sin8(value) {
        return SIN8[truncToUint8(value)];
    }

    function cos8(value) {
        return COS8[truncToUint8(value)];
    }

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        // HSV-only contract: return a Float32Array of interleaved H,S,V floats.
        var map = HSVUtil.createMap(width, height);
        if (!audio) return map;
        if (width <= 0 || height <= 0) return map;
        var mode = algo.presetMode;
        var dtSeconds = HSVUtil.audioSeconds(audio);
        var rawPower = powerFor(audio);
        var power = HSVUtil.clamp01(isFinite(rawPower) ? rawPower : 0);

        if (mode === "Plasma2d") {
            syncSourceModeState(mode, width, height, audio);
            sourceModeState.seconds += dtSeconds;
            var values = plasmaValues(width, height, power, sourceModeState.seconds);
            var minV = Infinity;
            var maxV = -Infinity;
            for (var m = 0; m < values.length; m++) {
                if (values[m] < minV) minV = values[m];
                if (values[m] > maxV) maxV = values[m];
            }
            var span = maxV - minV;
            for (var p = 0; p < values.length; p++) {
                var tP = span > 0 ? HSVUtil.clamp01((values[p] - minV) / span) : 0;
                var hsvP = HSVUtil.gradientLedfxAt(algo.colors, tP);
                map[p * 3] = hsvP.h;
                map[p * 3 + 1] = hsvP.s;
                map[p * 3 + 2] = hsvP.v;
            }
            return map;
        }

        if (mode === "PlasmaWled2d") {
            syncSourceModeState(mode, width, height, audio);
            var powerW = power * 2;
            var sizeb = powerW * algo.presetSizeMultiplier;
            var speedb = powerW * algo.presetSpeedMultiplier;
            // Source control allows 0. Use +1 as the safe effective denominator.
            var speedDenominator = algo.presetSpeedDivisor + 1;
            var a;
            if (algo.presetSpeedMultiplier > 0) {
                sourceModeState.motion += speedb * dtSeconds * 60;
                a = Math.floor(sourceModeState.motion * 1000) / speedDenominator;
            } else {
                sourceModeState.freeSeconds += dtSeconds;
                a = Math.floor(sourceModeState.freeSeconds * 1000) / speedDenominator;
            }
            var baseHStretch = algo.presetHorizontalStretch;
            var baseVStretch = algo.presetVerticalStretch;
            var hStretch = Math.max(0, baseHStretch - (sizeb * baseHStretch / 3));
            var vStretch = Math.max(0, baseVStretch - (sizeb * baseVStretch / 3));
            for (var iyW = 0; iyW < height; iyW++) {
                for (var ixW = 0; ixW < width; ixW++) {
                    var xVal = iyW * hStretch / 16 + a / 3;
                    var yVal = ixW * vStretch / 16 + a / 4;
                    var wave = sin8(cos8(xVal) + sin8(yVal) + a) / 255;
                    var hsvW = HSVUtil.gradientLedfxAt(algo.colors, HSVUtil.clamp01(wave));
                    HSVUtil.setPixel(map, width, ixW, iyW, hsvW.h, hsvW.s, hsvW.v);
                }
            }
            return map;
        }

        smoothPower = HSVUtil.smoothFollow(smoothPower, rawPower, algo.presetSmoothing);

        // BPM-scaled free-running time replaces LedFX's wall-clock self.now.
        // One unit of "time" advances per beat (matches LedFX seconds at 60 BPM).
        position += isFinite(audio.dt) ? audio.dt : (dtSeconds * (audio.bpm > 0 ? audio.bpm : 120) / 60);

        var plasma = plasmaValues(width, height, smoothPower, position);
        for (var i = 0; i < plasma.length; i++) {
            // No per-frame normalization. Map raw sum into [0,1].
            var hsv = HSVUtil.gradientAt(algo.colors, HSVUtil.clamp01((plasma[i] + 3) / 6.0));
            map[i * 3] = hsv.h;
            map[i * 3 + 1] = hsv.s;
            map[i * 3 + 2] = hsv.v;
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
