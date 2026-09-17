/*
  Q Light Controller Plus
  audiopower.js — 5-segment power meter (Audio API v2)

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
    algo.name = "Audio Power";
    algo.author = "QLC+ contributors";
    algo.acceptColors = 5;
    algo.usesAudio = true;
    algo.properties = new Array();
    algo.presetMode = "Artistic";
    algo.properties.push("name:mode|type:list|display:Response|values:Artistic,LedFx Power|write:setMode|read:getMode");
    algo.setMode = function(v) { algo.presetMode = v === "LedFx Power" ? v : "Artistic"; };
    algo.getMode = function() { return algo.presetMode; };

    var DEFAULT_GRADIENT = [
        {h: 0.0,   s: 1.0, v: 1.0},
        {h: 0.078, s: 1.0, v: 1.0},
        {h: 0.131, s: 1.0, v: 1.0},
        {h: 0.333, s: 1.0, v: 1.0},
        {h: 0.611, s: 0.75, v: 1.0}
    ];

    algo.blur = 0.0;
    algo.bass_decay_rate = 0.05;
    algo.sparks_decay_rate = 0.15;
    algo.sparks_color = "#ffffff";
    algo.frequency_range = "Lows (beat+bass)";

    algo.properties.push("name:blur|type:float|display:Blur|write:setBlur|read:getBlur");
    algo.properties.push("name:bass_decay_rate|type:float|display:Bass Decay Rate|write:setBassDecayRate|read:getBassDecayRate");
    algo.properties.push("name:sparks_decay_rate|type:float|display:Sparks Decay Rate|write:setSparksDecayRate|read:getSparksDecayRate");
    algo.properties.push("name:sparks_color|type:string|display:Spark Color (#rrggbb)|write:setSparksColor|read:getSparksColor");
    algo.properties.push("name:frequency_range|type:list|display:Frequency Range|values:Beat,Bass,Lows (beat+bass),Mids,High|write:setFrequencyRange|read:getFrequencyRange");

    algo.setBlur = function(v) { algo.blur = parseFloat(v); };
    algo.getBlur = function() { return algo.blur; };
    algo.setBassDecayRate = function(v) { algo.bass_decay_rate = parseFloat(v); };
    algo.getBassDecayRate = function() { return algo.bass_decay_rate; };
    algo.setSparksDecayRate = function(v) { algo.sparks_decay_rate = parseFloat(v); };
    algo.getSparksDecayRate = function() { return algo.sparks_decay_rate; };
    algo.setSparksColor = function(v) {
        if (typeof v === "string" && /^#[0-9a-f]{6}$/i.test(v))
            algo.sparks_color = v.toLowerCase();
    };
    algo.getSparksColor = function() { return algo.sparks_color; };
    algo.setFrequencyRange = function(v) { algo.frequency_range = String(v); };
    algo.getFrequencyRange = function() { return algo.frequency_range; };

    algo.presetSmoothing = 5;
    algo.properties.push("name:presetSmoothing|type:range|display:Smoothing|values:1,10|write:setSmoothing|read:getSmoothing");
    algo.setSmoothing = function(v) { algo.presetSmoothing = parseInt(v); };
    algo.getSmoothing = function() { return algo.presetSmoothing; };
    var readOutputControls = HSVUtil.attachStripTransformControls(algo, {
        display: "",
        defaults: {
            flip: "Off",
            mirror: "Off",
            backgroundMode: "Off",
            backgroundColor: "#000000",
            backgroundBrightness: 1,
            brightness: 1
        }
    });

    var smoothBands = [0, 0, 0, 0, 0];

    // Bar-level build-up / release state
    var barEnergy = 0;
    var peakEnergy = 0;
    var releaseFlash = 0;

    var sparksV = null;
    var bassV = null;
    var sparkColor = {h: 0.884, s: 1.0, v: 1.0};
    var lastW = 0;
    var ledSparks = null;
    var ledBass = null;
    var ledFilteredBass = 0;
    var lastLedN = 0;
    var lastMode = "";
    var lastIdentityKey = "";


    function ensure(width) {
        if (lastW === width && sparksV) return;
        lastW = width;
        sparksV = new Array(width);
        bassV = new Array(width);
        for (var i = 0; i < width; i++) { sparksV[i] = 0; bassV[i] = 0; }
    }

    function ensureLed(n) {
        if (lastLedN === n && ledSparks) return;
        lastLedN = n;
        ledFilteredBass = 0;
        ledSparks = new Array(n);
        ledBass = new Array(n);
        for (var i = 0; i < n; i++) {
            ledSparks[i] = [0, 0, 0];
            ledBass[i] = [0, 0, 0];
        }
    }

    function scaleInPlace(arr, factor) {
        for (var i = 0; i < arr.length; i++) arr[i] *= factor;
    }

    function gradientStops() {
        return (algo.colors && algo.colors.length >= 3) ? algo.colors : DEFAULT_GRADIENT;
    }

    function boxBlur(strip, radius) {
        if (radius < 0.5) return strip;
        var n = strip.length;
        var r = Math.max(1, Math.round(radius));
        var out = new Array(n);
        for (var i = 0; i < n; i++) {
            var bh = 0, bs = 0, bv = 0, c = 0;
            for (var k = -r; k <= r; k++) {
                var idx = i + k;
                if (idx < 0 || idx >= n) continue;
                bh += strip[idx].h; bs += strip[idx].s; bv += strip[idx].v; c++;
            }
            out[i] = {h: bh / c, s: bs / c, v: bv / c};
        }
        return out;
    }

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step, audio) {
        var map = HSVUtil.createMap(width, height);
        if (!audio) return map;
        var ledFxMode = algo.presetMode === "LedFx Power";
        var identityKey = HSVUtil.audioIdentityKey(audio, lastIdentityKey);
        var identityChanged = identityKey !== lastIdentityKey;
        lastIdentityKey = identityKey;
        if (identityChanged) {
            smoothBands = [0, 0, 0, 0, 0];
            barEnergy = 0;
            peakEnergy = 0;
            releaseFlash = 0;
            lastW = 0;
            sparksV = null;
            bassV = null;
            lastLedN = 0;
            ledSparks = null;
            ledBass = null;
            ledFilteredBass = 0;
        }
        if (lastMode !== algo.presetMode) {
            lastMode = algo.presetMode;
            lastLedN = 0;
        }
        if (ledFxMode) {
            var n = width * height;
            ensureLed(n);
            var stops = gradientStops();
            var sparkRgb = HSVUtil.parseHexRgb(algo.sparks_color);
            var elapsed = HSVUtil.audioSeconds(audio);
            var elapsedSteps = elapsed * 60;
            if (elapsedSteps > 0) {
                var sparkDecay = Math.pow(
                    1.0 - Math.max(0, Math.min(1, algo.sparks_decay_rate)),
                    elapsedSteps
                );
                var bassDecay = Math.pow(
                    1.0 - Math.max(0, Math.min(1, algo.bass_decay_rate)),
                    elapsedSteps
                );
                for (var i = 0; i < n; i++) {
                    ledSparks[i][0] *= sparkDecay;
                    ledSparks[i][1] *= sparkDecay;
                    ledSparks[i][2] *= sparkDecay;
                    ledBass[i][0] *= bassDecay;
                    ledBass[i][1] *= bassDecay;
                    ledBass[i][2] *= bassDecay;
                }
            }

            var onsetDelta = audio.events && audio.events.delta && isFinite(audio.events.delta.onset)
                ? Math.max(0, Math.floor(audio.events.delta.onset))
                : (audio.onset ? 1 : 0);
            var sparksPerEvent = Math.max(1, Math.floor(n / 20));
            for (var e = 0; e < onsetDelta; e++) {
                for (var s = 0; s < sparksPerEvent; s++) {
                    var sx = Math.floor(Math.random() * Math.max(1, n));
                    ledSparks[sx][0] = sparkRgb[0];
                    ledSparks[sx][1] = sparkRgb[1];
                    ledSparks[sx][2] = sparkRgb[2];
                }
            }

            var bassRaw = HSVUtil.powerForRange(audio, algo.frequency_range, true);
            if (elapsedSteps > 0) {
                var bassCoeffPerStep = bassRaw > ledFilteredBass ? 0.8 : 0.1;
                var bassCoeff = 1 - Math.pow(1 - bassCoeffPerStep, elapsedSteps);
                ledFilteredBass += bassCoeff * (bassRaw - ledFilteredBass);
            }
            var bass = HSVUtil.clamp01(ledFilteredBass);
            var bassIdx = Math.floor(bass * n);
            var bassColor = HSVUtil.gradientRgbAt(stops, bass);
            for (var b = 0; b < bassIdx && b < n; b++) {
                ledBass[b][0] = bassColor[0];
                ledBass[b][1] = bassColor[1];
                ledBass[b][2] = bassColor[2];
            }

            var novelty = [];
            var bank = audio && audio.banks && audio.banks.full;
            if (bank && bank.count && Array.isArray(bank.novelty))
                novelty = HSVUtil.interpolate(bank.novelty, n);
            else
                novelty = new Array(n).fill(0);

            for (var x = 0; x < n; x++) {
                var spatial = n <= 1 ? 0 : x / (n - 1);
                var bg = HSVUtil.gradientRgbAt(stops, spatial);
                var scale = HSVUtil.clamp01(isFinite(novelty[x]) ? novelty[x] : 0);
                var r = bg[0] * scale + ledBass[x][0] + ledSparks[x][0];
                var g = bg[1] * scale + ledBass[x][1] + ledSparks[x][1];
                var bb = bg[2] * scale + ledBass[x][2] + ledSparks[x][2];
                var hsvLed = HSVUtil.rgbToHsvUnclipped(HSVUtil.clamp01(r), HSVUtil.clamp01(g), HSVUtil.clamp01(bb));
                var o = x * 3;
                map[o] = hsvLed.h;
                map[o + 1] = hsvLed.s;
                map[o + 2] = hsvLed.v;
            }
            return HSVUtil.applyStripTransforms(map, width, height, readOutputControls());
        }

        ensure(width);

        scaleInPlace(sparksV, 1.0 - Math.max(0, Math.min(1, algo.sparks_decay_rate)));
        scaleInPlace(bassV, 1.0 - Math.max(0, Math.min(1, algo.bass_decay_rate)));

        // Sparks on onset
        if (audio.onset) {
            var numSparks = Math.floor(width / 20);
            var stops = gradientStops();
            sparkColor = stops[stops.length - 1];
            for (var s = 0; s < numSparks; s++) {
                var sx = Math.floor(Math.random() * width);
                sparksV[sx] = sparkColor.v;
            }
        }

        // Bass fill from left
        var bassRaw = audio.low;
        var bass = Math.max(0, Math.min(1, bassRaw));
        var bassIdx = Math.floor(bass * width);
        var bassHsv = HSVUtil.gradientAt(gradientStops(), bass);
        for (var i = 0; i < bassIdx && i < width; i++)
            bassV[i] = bassHsv.v;

        // Build 5-band interpolated power strip
        var rawBands = [audio.beat, audio.bass, audio.low, audio.mid, audio.high];
        // Asymmetric EMA per bin (fast attack, slow decay)
        var smoothing = algo.presetSmoothing / 10.0;
        var riseAlpha = 0.5 * (1 - smoothing) + 0.05;
        var decayAlpha = 0.02 + 0.03 * (1 - smoothing);
        for (var sb = 0; sb < 5; sb++) {
            var sa = rawBands[sb] > smoothBands[sb] ? riseAlpha : decayAlpha;
            smoothBands[sb] += sa * (rawBands[sb] - smoothBands[sb]);
        }
        var bands = smoothBands;

        // --- Bar-level build-up ---
        var rawEnergy = (audio.low + audio.mid * 0.5 + audio.high * 0.3) / 1.8;
        barEnergy += rawEnergy * audio.dt;
        if (barEnergy > peakEnergy) peakEnergy = barEnergy;
        if (audio.downbeat) {
            releaseFlash = Math.min(1, peakEnergy * 0.5);
            barEnergy = 0;
            peakEnergy = 0;
        }
        releaseFlash *= 0.85;

        var strip = new Array(width);
        for (var x = 0; x < width; x++) {
            var spatial = width <= 1 ? 0 : x / (width - 1);
            // Interpolate band power at this position
            var bandPos = spatial * (bands.length - 1);
            var lo = Math.floor(bandPos);
            var hi = Math.min(bands.length - 1, lo + 1);
            var frac = bandPos - lo;
            var melVal = bands[lo] * (1 - frac) + bands[hi] * frac;

            var gradHsv = HSVUtil.gradientAt(gradientStops(), spatial);
            // Release briefly expands all bars toward peak
            var baseV = gradHsv.v * Math.min(1, melVal + releaseFlash * 0.3);
            var bV = bassV[x];
            var spV = sparksV[x];
            var totalV = Math.max(0, Math.min(1, baseV + bV + spV));
            var ph = gradHsv.h, ps = gradHsv.s;
            if (spV > baseV && spV > bV) {
                ph = sparkColor.h; ps = sparkColor.s;
            } else if (bV > baseV) {
                ph = bassHsv.h; ps = bassHsv.s;
            }
            strip[x] = {h: ph, s: ps, v: totalV};
        }

        strip = boxBlur(strip, algo.blur);
        for (var y = 0; y < height; y++)
            for (var px = 0; px < width; px++)
                HSVUtil.setPixel(map, width, px, y, strip[px].h, strip[px].s, strip[px].v);
        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
