/*
  Q Light Controller Plus
  audioshockwave.js

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
    algo.name = "Audio Shockwave";
    algo.author = "QLC+ contributors";
    algo.acceptColors = 2;
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

    // Wave and background colors: user-picked or defaults
    var DEFAULT_WAVE = {h: 0, s: 0, v: 1};       // white
    var DEFAULT_BG   = {h: 0.611, s: 1.0, v: 0.188}; // dark blue
    var waveColor = DEFAULT_WAVE;
    var bgColor   = DEFAULT_BG;
    var lastW = 0, lastH = 0;

    function updateColors() {
        if (algo.hasUserColors) {
            waveColor = algo.colors[0] || DEFAULT_WAVE;
            bgColor   = (algo.colors.length > 1) ? algo.colors[1] : DEFAULT_BG;
        }
    }



    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };

    algo.setMaxWaves = function(_v) { algo.presetMaxWaves = parseInt(_v); };
    algo.getMaxWaves = function() { return algo.presetMaxWaves; };
    algo.setWaveWidth = function(_v) { algo.presetWaveWidth = parseFloat(_v); };
    algo.getWaveWidth = function() { return algo.presetWaveWidth; };
    algo.setSpeed = function(_v) { algo.presetSpeed = parseFloat(_v); };
    algo.getSpeed = function() { return algo.presetSpeed; };
    algo.setDecay = function(_v) { algo.presetDecay = parseFloat(_v); };
    algo.getDecay = function() { return algo.presetDecay; };

    algo.setAmbientSpeed = function(_v) { algo.presetAmbientSpeed = parseFloat(_v); };
    algo.getAmbientSpeed = function() { return algo.presetAmbientSpeed; };
    algo.setWaveDecay = function(_v) { algo.presetWaveDecay = parseFloat(_v); };
    algo.getWaveDecay = function() { return algo.presetWaveDecay; };

    algo.presetSmoothing = 5;
    algo.properties.push(
      "name:presetSmoothing|type:range|display:Smoothing|" +
      "values:1,10|write:setSmoothing|read:getSmoothing");
    algo.setSmoothing = function(_v) { algo.presetSmoothing = parseInt(_v); };
    algo.getSmoothing = function() { return algo.presetSmoothing; };

    var smoothLow = 0;

    function spawnWave(width, height, intensity, audio) {
        // Tint wave color toward dominant band color
        var domIdx = (audio.mid > audio.low && audio.mid >= audio.high) ? 1 : (audio.high > audio.low) ? 2 : 0;
        var domPower = Math.max(audio.low, audio.mid, audio.high);
        var domHsv = (domPower >= 0.05 && algo.colors && algo.colors.length >= 3) ? algo.colors[domIdx] : waveColor;
        var t = DOMINANT_TINT;
        var color = {h: waveColor.h + (domHsv.h - waveColor.h) * t, s: waveColor.s + (domHsv.s - waveColor.s) * t, v: waveColor.v + (domHsv.v - waveColor.v) * t};
        algo.waves.push({
            cx: width / 2,
            cy: height / 2,
            radius: 0,
            maxRadius: Math.sqrt(width * width + height * height),
            intensity: Math.max(0, Math.min(MAX_INTENSITY, intensity * KICK_INTENSITY_BOOST)),
            color: color
        });

        while (algo.waves.length > algo.presetMaxWaves)
            algo.waves.shift();
    }

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        updateColors();
        var dtMs = audio ? (audio.dt * 60000 / audio.bpm) : 40;
        var frameScale = dtMs / 40;
        algo.dtAccum += dtMs;
        if (width !== lastW || height !== lastH) {
            algo.waves = [];
            lastW = width;
            lastH = height;
        }

        var map = HSVUtil.createMap(width, height);
        if (!audio) return map;
        var cx = width / 2;
        var cy = height / 2;
        var maxRadius = Math.sqrt(width * width + height * height);

        // Render ambient background
        var phase = algo.dtAccum * algo.presetAmbientSpeed * 0.025;
        for (var y = 0; y < height; y++) {
            for (var x = 0; x < width; x++) {
                var dist = Math.sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy));
                var ring = Math.sin(dist * AMBIENT_RING_FREQ - phase) * 0.5 + 0.5;
                var centerLift = 1.0 - Math.min(1.0, dist / Math.max(1.0, maxRadius));
                var bri = AMBIENT_BASE_BRI + ring * AMBIENT_RING_BRI + centerLift * AMBIENT_CENTER_BRI;
                var i3 = (y * width + x) * 3;
                map[i3] = bgColor.h;
                map[i3 + 1] = bgColor.s;
                map[i3 + 2] = bgColor.v * bri;
            }
        }

        var bass = audio.low;
        // Asymmetric EMA: smooth spawn-intensity driver
        var smoothing = algo.presetSmoothing / 10.0;
        var riseAlpha = 0.5 * (1 - smoothing) + 0.05;
        var decayAlpha = 0.02 + 0.03 * (1 - smoothing);
        smoothLow += (bass > smoothLow ? riseAlpha : decayAlpha) * (bass - smoothLow);

        if (audio.beatFired || audio.onset)
            spawnWave(width, height, Math.max(0.5, smoothLow), audio);

        if (algo.waves.length < MAX_FILL_WAVES && smoothLow > MIN_BASS_FOR_FILL)
            spawnWave(width, height, FILL_INTENSITY, audio);

        // Render waves: intensity is snapshot at spawn, not re-read each frame
        var waveWidth = algo.presetWaveWidth;
        for (var wi = 0; wi < algo.waves.length; wi++) {
            var wave = algo.waves[wi];
            var fade = Math.max(0, 1 - wave.radius / Math.max(1, wave.maxRadius));

            for (var y = 0; y < height; y++) {
                for (var x = 0; x < width; x++) {
                    var dist = Math.sqrt((x - wave.cx) * (x - wave.cx) + (y - wave.cy) * (y - wave.cy));
                    var ringDist = Math.abs(dist - wave.radius);
                    if (ringDist < waveWidth) {
                        var ringBri = (1 - ringDist / waveWidth);
                        ringBri = Math.sqrt(ringBri) * wave.intensity * fade;

                        if (ringBri > MIN_RENDER_BRI) {
                            var i3 = (y * width + x) * 3;
                            var existV = map[i3 + 2];
                            var addV = wave.color.v * ringBri;
                            var newV = Math.min(1, existV + addV);

                            if (existV < 0.001) {
                                map[i3] = wave.color.h;
                                map[i3 + 1] = wave.color.s;
                            } else {
                                var total = existV + addV;
                                var t = addV / Math.max(0.001, total);
                                var dh = wave.color.h - map[i3];
                                if (dh > 0.5) dh -= 1;
                                else if (dh < -0.5) dh += 1;
                                var h = map[i3] + t * dh;
                                map[i3] = h - Math.floor(h);
                                map[i3 + 1] = map[i3 + 1] * (1 - t) + wave.color.s * t;
                            }
                            map[i3 + 2] = newV;
                        }
                    }
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

    testAlgo = algo;
    return algo;
  }
)();
