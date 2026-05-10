/*
  Q Light Controller Plus
  audioreactor.js

  Kitchen-sink audio demo: dominant-band scenes, beat power, pitch tint,
  and onset flashes.

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

    algo.presetSensitivity = 7;
    algo.properties.push(
      "name:presetSensitivity|type:range|display:Sensitivity|" +
      "values:1,10|write:setSensitivity|read:getSensitivity");

    algo.presetMotion = 6;
    algo.properties.push(
      "name:presetMotion|type:range|display:Motion|" +
      "values:1,10|write:setMotion|read:getMotion");

    algo.presetFlash = 8;
    algo.properties.push(
      "name:presetFlash|type:range|display:Onset Flash|" +
      "values:0,10|write:setFlash|read:getFlash");

    algo.presetSparkles = 1;
    algo.properties.push(
      "name:presetSparkles|type:list|display:High Sparkles|" +
      "values:Off,On|write:setSparkles|read:getSparkles");

    algo.setPalette = function(_v) { algo.presetPalette = (_v === "Pitch Hue") ? 1 : 0; };
    algo.getPalette = function() { return algo.presetPalette ? "Pitch Hue" : "Band Colors"; };
    algo.setSensitivity = function(_v) { algo.presetSensitivity = parseInt(_v); };
    algo.getSensitivity = function() { return algo.presetSensitivity; };
    algo.setMotion = function(_v) { algo.presetMotion = parseInt(_v); };
    algo.getMotion = function() { return algo.presetMotion; };
    algo.setFlash = function(_v) { algo.presetFlash = parseInt(_v); };
    algo.getFlash = function() { return algo.presetFlash; };
    algo.setSparkles = function(_v) { algo.presetSparkles = (_v === "On") ? 1 : 0; };
    algo.getSparkles = function() { return algo.presetSparkles ? "On" : "Off"; };

    var DEFAULT_BAND_COLORS = [0xFF2040, 0x20FF80, 0x80C0FF];
    var time = 0;
    var flash = 0;
    var sweep = 0;
    var sparkEnergy = null;
    var sparkColor = null;

    function unpack(packed) {
        return [(packed >> 16) & 0xFF, (packed >> 8) & 0xFF, packed & 0xFF];
    }

    function mix(a, b, t) {
        return [
            a[0] * (1 - t) + b[0] * t,
            a[1] * (1 - t) + b[1] * t,
            a[2] * (1 - t) + b[2] * t
        ];
    }

    function colorFor(audio, bandIndex) {
        if (algo.presetPalette && audio.pitch.hz > 0 && audio.pitch.confidence > 0.2) {
            var midi = audio.pitch.midi;
            var hue = RGBUtil.mod1((midi % 12) / 12 + bandIndex / 12);
            return RGBUtil.hsv2rgb(hue, 0.85, 1.0);
        }
        var colors = AudioColors.bands(algo);
        return unpack(colors[bandIndex] || DEFAULT_BAND_COLORS[bandIndex]);
    }

    function ensureSparks(width, height) {
        var count = width * height;
        if (sparkEnergy && sparkEnergy.length === count)
            return;
        sparkEnergy = new Array(count);
        sparkColor = new Array(count);
        for (var i = 0; i < count; i++) {
            sparkEnergy[i] = 0;
            sparkColor[i] = [255, 255, 255];
        }
    }

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() {
        return AudioColors.bands(algo).slice();
    };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var map = RGBUtil.createMap(width, height);
        if (!audio) return map;

        ensureSparks(width, height);

        var dt = audio.timing.consumerDtMs / 1000.0;
        var sensitivity = algo.presetSensitivity / 7.0;
        var motion = algo.presetMotion / 6.0;
        var lowVis = Math.min(1, audio.power.low * sensitivity);
        var midVis = Math.min(1, audio.power.mid * sensitivity);
        var highVis = Math.min(1, audio.power.high * sensitivity);
        var beatPower = audio.power.detail.beat;
        var beatPulse = Math.max(audio.beat.cosPulse, audio.beat.kickIntensity);
        var speed = (0.015 + 0.025 * motion) * (1 + beatPower * 2.5);
        time += dt * 1000.0 * speed;
        if (time > 1e6) time -= 1e6;
        sweep = RGBUtil.mod1(sweep + dt * 0.25 * motion * (1 + midVis * 3));

        var dominant = audio.power.dominant;
        var dominantIndex = dominant === "high" ? 2 : (dominant === "mid" ? 1 : 0);
        var dominantValue = [lowVis, midVis, highVis][dominantIndex];
        var lowColor = colorFor(audio, 0);
        var midColor = colorFor(audio, 1);
        var highColor = colorFor(audio, 2);
        var dominantColor = [lowColor, midColor, highColor][dominantIndex];

        var onset = audio.onset.fired;
        if (onset || audio.beat.fired || audio.beat.kick) {
            var onsetIntensity = audio.onset.intensity;
            flash = Math.max(flash, onsetIntensity * algo.presetFlash / 10.0);
        }
        flash *= 0.82;

        if (algo.presetSparkles && (onset || dominant === "high")) {
            var sparkCount = Math.max(1, Math.floor(width * height * highVis * 0.10));
            if (onset) sparkCount += Math.max(1, Math.floor(width / 4));
            for (var s = 0; s < sparkCount; s++) {
                var sx = Math.floor(Math.random() * width);
                var sy = Math.floor(Math.random() * Math.max(1, height / 2));
                var si = sy * width + sx;
                sparkEnergy[si] = Math.max(sparkEnergy[si], 0.5 + highVis * 0.5 + flash * 0.5);
                sparkColor[si] = highColor;
            }
        }

        for (var i = 0; i < sparkEnergy.length; i++)
            sparkEnergy[i] *= 0.80;

        var floor = 0;
        var overall = 0.35 + dominantValue + beatPulse * 0.35;
        var barPhase = audio.bar.phase01;

        for (var y = 0; y < height; y++) {
            var y01 = height <= 1 ? 0 : y / (height - 1);
            var bottom = 1 - y01;
            var top = y01;

            for (var x = 0; x < width; x++) {
                var x01 = width <= 1 ? 0 : x / (width - 1);
                var base = [0, 0, 0];
                var level = 0;

                if (dominant === "low") {
                    var wave = 0.5 + 0.5 * Math.sin((x01 * 2.5 + time + barPhase) * Math.PI * 2);
                    var pulseHeight = Math.min(1, 0.15 + lowVis * 1.15 + beatPulse * 0.25);
                    level = Math.max(0, (pulseHeight - bottom) / Math.max(0.001, pulseHeight));
                    base = mix(lowColor, midColor, wave * 0.25);
                    level *= 0.55 + 0.45 * wave;
                } else if (dominant === "mid") {
                    var center = sweep;
                    var dx = Math.abs(x01 - center);
                    dx = Math.min(dx, 1 - dx);
                    var ripple = Math.max(0, 1 - dx * (4 + midVis * 8));
                    var rowWave = 0.5 + 0.5 * Math.sin((y01 * 3 + time * 0.65) * Math.PI * 2);
                    level = Math.max(ripple, rowWave * midVis * 0.65);
                    base = mix(midColor, lowColor, rowWave * 0.25);
                } else {
                    var shimmer = 0.5 + 0.5 * Math.sin((x01 * 9 + y01 * 5 + time * 1.8) * Math.PI * 2);
                    level = highVis * (0.25 + 0.75 * top) * (0.45 + 0.55 * shimmer);
                    base = mix(highColor, midColor, shimmer * 0.20);
                }

                level += lowVis * Math.max(0, 0.25 - bottom) * 0.8;
                level += midVis * (0.5 + 0.5 * Math.sin((x01 + time * 0.2) * Math.PI * 2)) * 0.15;
                level += highVis * top * 0.20;

                var spark = sparkEnergy[y * width + x];
                if (spark > 0.01) {
                    base = mix(base, sparkColor[y * width + x], Math.min(1, spark));
                    level = Math.max(level, spark);
                }

                if (flash > 0.01) {
                    base = mix(base, dominantColor, Math.min(0.75, flash));
                    level = Math.max(level, flash);
                }

                var brightness = Math.min(1, level) * overall;
                map[y][x] = RGBUtil.rgb(base[0] * brightness, base[1] * brightness, base[2] * brightness);
            }
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
