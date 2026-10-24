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
    var fillCredit = 0;
    var freedFrames = [];

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
    // A wave keeps at least this share of its intensity per 40 ms step at any Decay x Wave Decay.
    var MIN_DECAY_FACTOR = 0.1;

    // Tints toward the dominant band (lows, mids, highs); the two colors are wave and background.
    var BAND_COLORS = [
        {h: 0.042, s: 1.0, v: 1.0},
        {h: 0.399, s: 1.0, v: 1.0},
        {h: 0.611, s: 0.749, v: 1.0}
    ];

    // Wave and background colors: user-picked or defaults
    var DEFAULT_WAVE = {h: 0, s: 0, v: 1};       // white
    var DEFAULT_BG   = {h: 0.611, s: 1.0, v: 0.188}; // dark blue
    var waveColor = DEFAULT_WAVE;
    var bgColor   = DEFAULT_BG;
    var lastW = 0, lastH = 0;

    function updateColors() {
        var user = algo.hasUserColors && algo.colors ? algo.colors : [];
        waveColor = user[0] || DEFAULT_WAVE;
        bgColor   = (user.length > 1) ? user[1] : DEFAULT_BG;
    }

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };

    algo.setMaxWaves = function(_v) { algo.presetMaxWaves = Math.round(HSVUtil.parseNumber(_v, algo.presetMaxWaves, 1, 12)); };
    algo.getMaxWaves = function() { return algo.presetMaxWaves; };
    algo.setWaveWidth = function(_v) { algo.presetWaveWidth = HSVUtil.parseNumber(_v, algo.presetWaveWidth, 1, 5); };
    algo.getWaveWidth = function() { return algo.presetWaveWidth; };
    algo.setSpeed = function(_v) { algo.presetSpeed = HSVUtil.parseNumber(_v, algo.presetSpeed, 1, 10); };
    algo.getSpeed = function() { return algo.presetSpeed; };
    algo.setDecay = function(_v) { algo.presetDecay = HSVUtil.parseNumber(_v, algo.presetDecay, 1, 10); };
    algo.getDecay = function() { return algo.presetDecay; };

    algo.setAmbientSpeed = function(_v) { algo.presetAmbientSpeed = HSVUtil.parseNumber(_v, algo.presetAmbientSpeed, 0, 5); };
    algo.getAmbientSpeed = function() { return algo.presetAmbientSpeed; };
    algo.setWaveDecay = function(_v) { algo.presetWaveDecay = HSVUtil.parseNumber(_v, algo.presetWaveDecay, 0, 1); };
    algo.getWaveDecay = function() { return algo.presetWaveDecay; };

    algo.presetSmoothing = 5;
    algo.properties.push(
      "name:presetSmoothing|type:range|display:Smoothing|" +
      "values:1,10|write:setSmoothing|read:getSmoothing");
    algo.setSmoothing = function(_v) { algo.presetSmoothing = Math.round(HSVUtil.parseNumber(_v, algo.presetSmoothing, 1, 10)); };
    algo.getSmoothing = function() { return algo.presetSmoothing; };

    var smoothLow = 0;

    function spawnWave(intensity, audio) {
        // Tint wave color toward dominant band color
        var domPower = Math.max(audio.low, audio.mid, audio.high);
        var color = waveColor;
        if (domPower >= 0.05) {
            var base = HSVUtil.hsvToRgb(waveColor.h, waveColor.s, waveColor.v);
            var dom = BAND_COLORS[HSVUtil.dominantBand(audio)];
            var tint = HSVUtil.hsvToRgb(dom.h, dom.s, dom.v);
            var t = DOMINANT_TINT;
            color = HSVUtil.rgbToHsv(base[0] + (tint[0] - base[0]) * t,
                base[1] + (tint[1] - base[1]) * t, base[2] + (tint[2] - base[2]) * t);
        }
        algo.waves.push({
            radius: 0,
            intensity: Math.max(0, Math.min(MAX_INTENSITY, intensity * KICK_INTENSITY_BOOST)),
            color: color
        });

        while (algo.waves.length > algo.presetMaxWaves)
            algo.waves.shift();
    }

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        updateColors();
        var dtMs = audio ? HSVUtil.audioSeconds(audio) * 1000 : 40;
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

        // Asymmetric EMA: smooth spawn-intensity driver
        smoothLow = HSVUtil.smoothFollow(smoothLow, audio.low, algo.presetSmoothing,
            audio.timing ? audio.timing.deltaSeconds * 50 : undefined);

        var count = audio.events ? Math.max(audio.events.delta.beat, audio.events.delta.onset) :
            (audio.beatFired || audio.onset ? 1 : 0);
        for (var spawn = 0; spawn < Math.min(count, algo.presetMaxWaves); spawn++)
            spawnWave(Math.max(0.5, smoothLow), audio);

        // Waves expand and fade per 40 ms step.
        var speed = algo.presetSpeed * SPEED_SCALE;
        var decayFactor = Math.max(MIN_DECAY_FACTOR, 1 - algo.presetDecay * algo.presetWaveDecay);
        function ageWave(wave, steps) {
            wave.radius += speed * steps;
            wave.intensity *= Math.pow(decayFactor, steps);
        }

        // Fill waves: at most one per 20 ms frame of audio while fewer than MAX_FILL_WAVES are alive.
        // A wave due in an earlier frame of this render, and not before the wave whose place it
        // takes left the map, starts that many frames older.
        fillCredit += HSVUtil.elapsedFrames(audio);
        freedFrames.sort(function(a, b) { return b - a; });
        while (fillCredit >= 1 && algo.waves.length < MAX_FILL_WAVES && smoothLow > MIN_BASS_FOR_FILL) {
            fillCredit -= 1;
            var freed = freedFrames.length ? Math.floor(freedFrames.shift()) : Infinity;
            spawnWave(FILL_INTENSITY, audio);
            ageWave(algo.waves[algo.waves.length - 1], Math.min(fillCredit, freed) / 2);
        }
        fillCredit -= Math.floor(fillCredit);
        freedFrames = [];

        // Render waves: intensity is snapshot at spawn, not re-read each frame
        var waveWidth = algo.presetWaveWidth;
        for (var wi = 0; wi < algo.waves.length; wi++) {
            var wave = algo.waves[wi];
            var fade = Math.max(0, 1 - wave.radius / Math.max(1, maxRadius));

            for (var y = 0; y < height; y++) {
                for (var x = 0; x < width; x++) {
                    var dist = Math.sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy));
                    var ringDist = Math.abs(dist - wave.radius);
                    if (ringDist < waveWidth) {
                        var ringBri = (1 - ringDist / waveWidth);
                        ringBri = Math.sqrt(ringBri) * wave.intensity * fade;

                        if (ringBri > MIN_RENDER_BRI)
                            HSVUtil.blendPixel(map, (y * width + x) * 3,
                                wave.color.h, wave.color.s, wave.color.v * ringBri);
                    }
                }
            }
        }

        for (var ui = algo.waves.length - 1; ui >= 0; ui--) {
            var aged = algo.waves[ui];
            ageWave(aged, frameScale);
            if (aged.intensity < MIN_WAVE_INTENSITY || aged.radius > maxRadius) {
                // How many frames before the next render the wave passed the map edge.
                freedFrames.push(aged.radius > maxRadius ? (aged.radius - maxRadius) / (speed / 2) : 0);
                algo.waves.splice(ui, 1);
            }
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
