/*
  Q Light Controller Plus
  audioreactor.js

  Kitchen-sink audio demo: dominant-band scenes, onset flash, beat-driven
  sparkles, optional pitch-hue palette. Each audio feature drives a single
  visual behavior (no stacking).

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
    algo.name = "Audio Reactor";
    algo.author = "QLC+ contributors";
    algo.acceptColors = 3; // low/mid/high band palette
    algo.usesAudio = true;
    algo.properties = new Array();

    algo.presetPalette = 0;
    algo.properties.push(
      "name:presetPalette|type:list|display:Palette|" +
      "values:Band Colors,Pitch Hue|write:setPalette|read:getPalette");

    algo.presetSensitivity = 1.0;
    algo.properties.push(
      "name:presetSensitivity|type:float|display:Sensitivity|" +
      "write:setSensitivity|read:getSensitivity");

    algo.presetFlash = 0.8;
    algo.properties.push(
      "name:presetFlash|type:float|display:Onset Flash|" +
      "write:setFlash|read:getFlash");

    algo.presetSparkles = 1;
    algo.properties.push(
      "name:presetSparkles|type:list|display:High Sparkles|" +
      "values:Off,On|write:setSparkles|read:getSparkles");

    algo.setPalette = function(_v) { algo.presetPalette = (_v === "Pitch Hue") ? 1 : 0; };
    algo.getPalette = function() { return algo.presetPalette ? "Pitch Hue" : "Band Colors"; };
    algo.setSensitivity = function(_v) { algo.presetSensitivity = parseFloat(_v); };
    algo.getSensitivity = function() { return algo.presetSensitivity; };
    algo.setFlash = function(_v) { algo.presetFlash = parseFloat(_v); };
    algo.getFlash = function() { return algo.presetFlash; };
    algo.setSparkles = function(_v) { algo.presetSparkles = (_v === "On") ? 1 : 0; };
    algo.getSparkles = function() { return algo.presetSparkles ? "On" : "Off"; };

    var DEFAULT_BAND_COLORS = [0xFF2040, 0x20FF80, 0x80C0FF];
    var PITCH_CONF_THRESH = 0.2;
    var FLASH_DECAY = 0.82;
    var SPARKLE_DECAY = 0.70;
    var SPARKLE_DENSITY = 0.10;
    var BASE_OVERALL = 0.35;

    // BPM-scaled wall clock (one unit per beat). No audio in time scale.
    var timeState = { position: 0 };
    var flash = 0;
    var sparkleLevel = 0;

    // Reusable sparkle bitmap (Array of 0/1) — preallocated and reused across
    // frames to avoid GC and the QV4 numeric-key→string conversion that an
    // object literal `{}` would trigger when indexed with `y * width + x`.
    var sparkleBitmap = null;
    var sparkleBitmapLen = 0;

    function unpack(packed) {
        return [(packed >> 16) & 0xFF, (packed >> 8) & 0xFF, packed & 0xFF];
    }

    function colorFor(audio, bandIndex) {
        if (algo.presetPalette && audio.pitch.hz > 0 && audio.pitch.confidence > PITCH_CONF_THRESH) {
            var midi = audio.pitch.midi;
            var hue = RGBUtil.mod1((midi % 12) / 12 + bandIndex / 12);
            return RGBUtil.hsv2rgb(hue, 0.85, 1.0);
        }
        var colors = AudioColors.bands(algo);
        return unpack(colors[bandIndex] || DEFAULT_BAND_COLORS[bandIndex]);
    }

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() {
        return AudioColors.bands(algo).slice();
    };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var map = RGBUtil.createFlatMap(width, height);
        if (!audio) return map;

        var dtMs = audio.timing.consumerDtMs > 0 ? audio.timing.consumerDtMs : 40;
        var bpm = (audio.beat) ? audio.beat.bpm : 0;
        // BPM-scaled free-running time: one unit per beat (matches seconds at 60 BPM).
        var time = RGBUtil.beatPosition(1.0, timeState, bpm, dtMs);

        var sensitivity = algo.presetSensitivity;
        var lowVis = Math.min(1, audio.power.low * sensitivity);
        var midVis = Math.min(1, audio.power.mid * sensitivity);
        var highVis = Math.min(1, audio.power.high * sensitivity);

        var dominant = audio.power.dominant;
        var dominantIndex = dominant === "high" ? 2 : (dominant === "mid" ? 1 : 0);
        var dominantValue = [lowVis, midVis, highVis][dominantIndex];
        var lowColor = colorFor(audio, 0);
        var midColor = colorFor(audio, 1);
        var highColor = colorFor(audio, 2);
        var dominantColor = [lowColor, midColor, highColor][dominantIndex];

        // Onset → flash overlay (single trigger source, no double-dipping).
        if (audio.onset.fired)
            flash = Math.max(flash, audio.onset.intensity * algo.presetFlash);
        flash *= FLASH_DECAY;

        // Beat → sparkle intensity envelope (single behavior).
        sparkleLevel *= SPARKLE_DECAY;
        if (algo.presetSparkles)
            sparkleLevel = Math.max(sparkleLevel, audio.beat.cosPulse * highVis);

        // Distribute sparkle pixels for this frame in the upper half.
        var sparkleActive = false;
        if (algo.presetSparkles && sparkleLevel > 0.02) {
            var pixelCount = width * height;
            if (sparkleBitmap === null || sparkleBitmapLen !== pixelCount) {
                sparkleBitmap = new Array(pixelCount);
                sparkleBitmapLen = pixelCount;
                for (var b = 0; b < pixelCount; b++) sparkleBitmap[b] = 0;
            } else {
                for (var b2 = 0; b2 < pixelCount; b2++) sparkleBitmap[b2] = 0;
            }
            sparkleActive = true;
            var sparkRows = Math.max(1, Math.floor(height / 2));
            var sparkCount = Math.max(1, Math.floor(width * sparkRows * SPARKLE_DENSITY * sparkleLevel));
            for (var s = 0; s < sparkCount; s++) {
                var sx = Math.floor(Math.random() * width);
                var sy = Math.floor(Math.random() * sparkRows);
                sparkleBitmap[sy * width + sx] = 1;
            }
        }

        var barPhase = audio.bar.phase01;
        var overall = BASE_OVERALL + dominantValue;
        var flashActive = flash > 0.01;
        var flashAmount = Math.min(1, flash);

        for (var y = 0; y < height; y++) {
            var y01 = height <= 1 ? 0 : y / (height - 1);
            var bottom = 1 - y01;
            var top = y01;

            for (var x = 0; x < width; x++) {
                var x01 = width <= 1 ? 0 : x / (width - 1);
                var color, level;

                if (dominant === "low") {
                    var wave = 0.5 + 0.5 * Math.sin((x01 * 2.5 + time + barPhase) * Math.PI * 2);
                    var pulseHeight = Math.min(1, 0.15 + lowVis * 1.15);
                    level = Math.max(0, (pulseHeight - bottom) / Math.max(0.001, pulseHeight));
                    level *= 0.55 + 0.45 * wave;
                    color = lowColor;
                } else if (dominant === "mid") {
                    var dx = Math.abs(x01 - barPhase);
                    dx = Math.min(dx, 1 - dx);
                    var ripple = Math.max(0, 1 - dx * (4 + midVis * 8));
                    var rowWave = 0.5 + 0.5 * Math.sin((y01 * 3 + time * 0.65) * Math.PI * 2);
                    level = Math.max(ripple, rowWave * midVis * 0.65);
                    color = midColor;
                } else {
                    var shimmer = 0.5 + 0.5 * Math.sin((x01 * 9 + y01 * 5 + time * 1.8) * Math.PI * 2);
                    level = highVis * (0.25 + 0.75 * top) * (0.45 + 0.55 * shimmer);
                    color = highColor;
                }

                // Sparkle override (single white flicker, no per-pixel state).
                if (sparkleActive && sparkleBitmap[y * width + x]) {
                    color = highColor;
                    level = Math.max(level, sparkleLevel);
                }

                // Flash override (full-frame tint, single trigger source).
                if (flashActive) {
                    color = dominantColor;
                    level = Math.max(level, flashAmount);
                }

                var brightness = Math.min(1, level) * overall;
                map[(y) * width + (x)] = RGBUtil.rgb(color[0] * brightness, color[1] * brightness, color[2] * brightness);
            }
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
