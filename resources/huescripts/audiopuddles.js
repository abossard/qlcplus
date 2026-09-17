/*
  Q Light Controller Plus
  audiopuddles.js

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
    algo.name = "Audio Puddles";
    algo.author = "QLC+ contributors";
    algo.acceptColors = 5;
    algo.usesAudio = true;
    algo.properties = new Array();
    algo.presetMode = "Artistic";
    algo.properties.push("name:mode|type:list|display:Response|values:Artistic,Rain Pulse|write:setMode|read:getMode");
    algo.setMode = function(v) { algo.presetMode = v === "Rain Pulse" ? v : "Artistic"; };
    algo.getMode = function() { return algo.presetMode; };

    var DEFAULT_GRADIENT = [
        {h: 0.708, s: 1.0, v: 1.0},
        {h: 0.500, s: 1.0, v: 1.0},
        {h: 0.417, s: 1.0, v: 1.0},
        {h: 0.167, s: 1.0, v: 1.0},
        {h: 0.958, s: 1.0, v: 1.0}
    ];
    var DEFAULT_BANDS = [
        {h: 0.042, s: 1.0, v: 1.0},
        {h: 0.399, s: 1.0, v: 1.0},
        {h: 0.611, s: 0.749, v: 1.0}
    ];

    algo.presetMaxRipples = 8;
    algo.properties.push(
      "name:presetMaxRipples|type:range|display:Max Concurrent Ripples|" +
      "values:1,32|write:setMaxRipples|read:getMaxRipples");
    algo.setMaxRipples = function(v) { algo.presetMaxRipples = parseInt(v); };
    algo.getMaxRipples = function() { return algo.presetMaxRipples; };

    algo.presetExpansionSpeed = 30;
    algo.properties.push(
      "name:presetExpansionSpeed|type:range|display:Expansion Speed (px/s)|" +
      "values:5,200|write:setExpansionSpeed|read:getExpansionSpeed");
    algo.setExpansionSpeed = function(v) { algo.presetExpansionSpeed = parseFloat(v); };
    algo.getExpansionSpeed = function() { return algo.presetExpansionSpeed; };

    algo.presetMaxRadius = 30;
    algo.properties.push(
      "name:presetMaxRadius|type:range|display:Max Radius (px)|" +
      "values:4,120|write:setMaxRadius|read:getMaxRadius");
    algo.setMaxRadius = function(v) { algo.presetMaxRadius = parseFloat(v); };
    algo.getMaxRadius = function() { return algo.presetMaxRadius; };

    algo.presetLifeMs = 1500;
    algo.properties.push(
      "name:presetLifeMs|type:range|display:Ripple Lifetime (ms)|" +
      "values:200,5000|write:setLifeMs|read:getLifeMs");
    algo.setLifeMs = function(v) { algo.presetLifeMs = parseFloat(v); };
    algo.getLifeMs = function() { return algo.presetLifeMs; };

    algo.presetRingWidth = 2;
    algo.properties.push(
      "name:presetRingWidth|type:range|display:Ring Width (px)|" +
      "values:1,8|write:setRingWidth|read:getRingWidth");
    algo.setRingWidth = function(v) { algo.presetRingWidth = parseFloat(v); };
    algo.getRingWidth = function() { return algo.presetRingWidth; };

    algo.presetTrigger = "Onset";
    algo.properties.push(
      "name:presetTrigger|type:list|display:Trigger|" +
      "values:Onset,Kick,Beat|write:setTrigger|read:getTrigger");
    algo.setTrigger = function(v) { algo.presetTrigger = String(v); };
    algo.getTrigger = function() { return algo.presetTrigger; };

    algo.presetMinSpawnMs = 80;
    algo.properties.push(
      "name:presetMinSpawnMs|type:range|display:Min Spawn Interval (ms)|" +
      "values:0,500|write:setMinSpawnMs|read:getMinSpawnMs");
    algo.setMinSpawnMs = function(v) { algo.presetMinSpawnMs = parseFloat(v); };
    algo.getMinSpawnMs = function() { return algo.presetMinSpawnMs; };
    algo.presetRainBand = "Lows";
    algo.properties.push(
      "name:presetRainBand|type:list|display:Rain Pulse Band|" +
      "values:Lows,Mids,Highs|write:setRainBand|read:getRainBand");
    algo.setRainBand = function(v) {
      algo.presetRainBand = v === "Mids" || v === "Highs" ? v : "Lows";
    };
    algo.getRainBand = function() { return algo.presetRainBand; };
    algo.presetRainSensitivity = "Medium";
    algo.properties.push(
      "name:presetRainSensitivity|type:list|display:Rain Pulse Sensitivity|" +
      "values:Low,Medium,High|write:setRainSensitivity|read:getRainSensitivity");
    algo.setRainSensitivity = function(v) {
      algo.presetRainSensitivity = v === "Low" || v === "High" ? v : "Medium";
      if (algo.presetRainSensitivity === "Low") {
          algo.presetRainLowsSensitivity = 0.08;
          algo.presetRainMidsSensitivity = 0.08;
          algo.presetRainHighsSensitivity = 0.08;
      } else if (algo.presetRainSensitivity === "High") {
          algo.presetRainLowsSensitivity = 0.22;
          algo.presetRainMidsSensitivity = 0.22;
          algo.presetRainHighsSensitivity = 0.22;
      } else {
          algo.presetRainLowsSensitivity = 0.14;
          algo.presetRainMidsSensitivity = 0.14;
          algo.presetRainHighsSensitivity = 0.14;
      }
    };
    algo.getRainSensitivity = function() { return algo.presetRainSensitivity; };
    algo.presetRainLowsSensitivity = 0.1;
    algo.presetRainMidsSensitivity = 0.05;
    algo.presetRainHighsSensitivity = 0.1;
    algo.properties.push(
      "name:presetRainLowsSensitivity|type:float|values:0.03,0.3|display:Lows Sensitivity|" +
      "write:setRainLowsSensitivity|read:getRainLowsSensitivity");
    algo.properties.push(
      "name:presetRainMidsSensitivity|type:float|values:0.03,0.3|display:Mids Sensitivity|" +
      "write:setRainMidsSensitivity|read:getRainMidsSensitivity");
    algo.properties.push(
      "name:presetRainHighsSensitivity|type:float|values:0.03,0.3|display:Highs Sensitivity|" +
      "write:setRainHighsSensitivity|read:getRainHighsSensitivity");
    algo.setRainLowsSensitivity = function(v) {
      var value = parseFloat(v);
      if (!isFinite(value)) return;
      algo.presetRainLowsSensitivity = Math.max(0.03, Math.min(0.3, value));
    };
    algo.getRainLowsSensitivity = function() { return algo.presetRainLowsSensitivity; };
    algo.setRainMidsSensitivity = function(v) {
      var value = parseFloat(v);
      if (!isFinite(value)) return;
      algo.presetRainMidsSensitivity = Math.max(0.03, Math.min(0.3, value));
    };
    algo.getRainMidsSensitivity = function() { return algo.presetRainMidsSensitivity; };
    algo.setRainHighsSensitivity = function(v) {
      var value = parseFloat(v);
      if (!isFinite(value)) return;
      algo.presetRainHighsSensitivity = Math.max(0.03, Math.min(0.3, value));
    };
    algo.getRainHighsSensitivity = function() { return algo.presetRainHighsSensitivity; };
    algo.presetRainLowsColor = "#ffffff";
    algo.presetRainMidsColor = "#ff0000";
    algo.presetRainHighsColor = "#0000ff";
    algo.properties.push(
      "name:presetRainLowsColor|type:string|display:Lows Color (#rrggbb)|" +
      "write:setRainLowsColor|read:getRainLowsColor");
    algo.properties.push(
      "name:presetRainMidsColor|type:string|display:Mids Color (#rrggbb)|" +
      "write:setRainMidsColor|read:getRainMidsColor");
    algo.properties.push(
      "name:presetRainHighsColor|type:string|display:Highs Color (#rrggbb)|" +
      "write:setRainHighsColor|read:getRainHighsColor");
    algo.setRainLowsColor = function(v) {
      if (typeof v === "string" && /^#[0-9a-f]{6}$/i.test(v))
          algo.presetRainLowsColor = v.toLowerCase();
    };
    algo.getRainLowsColor = function() { return algo.presetRainLowsColor; };
    algo.setRainMidsColor = function(v) {
      if (typeof v === "string" && /^#[0-9a-f]{6}$/i.test(v))
          algo.presetRainMidsColor = v.toLowerCase();
    };
    algo.getRainMidsColor = function() { return algo.presetRainMidsColor; };
    algo.setRainHighsColor = function(v) {
      if (typeof v === "string" && /^#[0-9a-f]{6}$/i.test(v))
          algo.presetRainHighsColor = v.toLowerCase();
    };
    algo.getRainHighsColor = function() { return algo.presetRainHighsColor; };

    algo.ripples = [];
    algo.spawnAccumMs = 1e9; // start ready
    algo.rainPulseStrip = [];
    algo.rainPulseBaseline = [0, 0, 0];
    var lastIdentityKey = "";

    algo.dominantColor = function(audio) {
        var bands = (algo.colors && algo.colors.length >= 3)
            ? algo.colors : DEFAULT_BANDS;
        var dom = (function(){var b=[audio.low,audio.mid,audio.high];return ["low","mid","high"][b.indexOf(Math.max.apply(null,b))]})();
        return bands[dom === "high" ? 2 : (dom === "mid" ? 1 : 0)];
    };

    algo.rgbMapStepCount = function(_w, _h) { return 1; };
    algo.rgbMapSetColors = function(_raw) { };
    algo.rgbMapGetColors = function() { return []; };

    function rainThreshold(index) {
        if (index === 1) return HSVUtil.clamp01(algo.presetRainMidsSensitivity);
        if (index === 2) return HSVUtil.clamp01(algo.presetRainHighsSensitivity);
        return HSVUtil.clamp01(algo.presetRainLowsSensitivity);
    }

    function rainBandPeaks(audio) {
        var bank = audio && audio.banks && audio.banks.full;
        if (!bank || !bank.count || !bank.processed) {
            return [
                HSVUtil.clamp01(audio.low || 0),
                HSVUtil.clamp01(audio.mid || 0),
                HSVUtil.clamp01(audio.high || 0)
            ];
        }
        var values = bank.processed;
        var bounds = [0, Math.floor(values.length * 0.2), Math.floor(values.length * 0.5), values.length];
        var peaks = [0, 0, 0];
        for (var slot = 0; slot < 3; slot++) {
            for (var i = bounds[slot]; i < bounds[slot + 1]; i++)
                peaks[slot] = Math.max(peaks[slot], HSVUtil.clamp01(values[i] || 0));
        }
        return peaks;
    }

    function rainBandColor(index) {
        var explicit = [
            HSVUtil.parseHexRgb(algo.presetRainLowsColor),
            HSVUtil.parseHexRgb(algo.presetRainMidsColor),
            HSVUtil.parseHexRgb(algo.presetRainHighsColor)
        ];
        return explicit[index];
    }

    function renderRainPulse(width, height, dt, audio) {
        var n = width * height;
        var map = HSVUtil.createMap(width, height);
        if (algo.rainPulseStrip.length !== n) {
            algo.rainPulseStrip = new Array(n);
            for (var reset = 0; reset < n; reset++)
                algo.rainPulseStrip[reset] = [0, 0, 0];
        }
        if (!(dt > 0)) {
            for (var idle = 0; idle < n; idle++) {
                var idleRgb = algo.rainPulseStrip[idle];
                var idleHsv = HSVUtil.rgbToHsv(idleRgb[0] / 255, idleRgb[1] / 255, idleRgb[2] / 255);
                var io = idle * 3;
                map[io] = idleHsv.h;
                map[io + 1] = idleHsv.s;
                map[io + 2] = idleHsv.v;
            }
            return map;
        }

        var slot = algo.presetRainBand === "Mids" ? 1 : (algo.presetRainBand === "Highs" ? 2 : 0);
        var peaks = rainBandPeaks(audio);
        if (peaks[slot] - algo.rainPulseBaseline[slot] > rainThreshold(slot)) {
            var color = rainBandColor(slot);
            for (var i = 0; i < n; i++) {
                algo.rainPulseStrip[i][0] = Math.round(255 * HSVUtil.clamp01(color[0]));
                algo.rainPulseStrip[i][1] = Math.round(255 * HSVUtil.clamp01(color[1]));
                algo.rainPulseStrip[i][2] = Math.round(255 * HSVUtil.clamp01(color[2]));
            }
        }

        for (var b = 0; b < 3; b++) {
            var coeff = peaks[b] > algo.rainPulseBaseline[b] ? 0.99 : 0.5;
            algo.rainPulseBaseline[b] += coeff * (peaks[b] - algo.rainPulseBaseline[b]);
        }

        var decaySteps = dt > 0 ? Math.max(0, Math.round(dt * 50)) : 0;
        for (var step = 0; step < decaySteps; step++) {
            for (var d = 0; d < n; d++) {
                algo.rainPulseStrip[d][0] = Math.floor(algo.rainPulseStrip[d][0] * 0.9);
                algo.rainPulseStrip[d][1] = Math.floor(algo.rainPulseStrip[d][1] * 0.9);
                algo.rainPulseStrip[d][2] = Math.floor(algo.rainPulseStrip[d][2] * 0.9);
            }
        }

        for (var x = 0; x < n; x++) {
            var rgb = algo.rainPulseStrip[x];
            var hsv = HSVUtil.rgbToHsv(rgb[0] / 255, rgb[1] / 255, rgb[2] / 255);
            var o = x * 3;
            map[o] = hsv.h;
            map[o + 1] = hsv.s;
            map[o + 2] = hsv.v;
        }
        return map;
    }

    algo.rgbMap = function(width, height, rgb, step, audio) {
        if (!audio)
            return HSVUtil.createMap(width, height);
        var identityKey = HSVUtil.audioIdentityKey(audio, lastIdentityKey);
        var identityChanged = identityKey !== lastIdentityKey;
        lastIdentityKey = identityKey;
        if (identityChanged) {
            algo.ripples = [];
            algo.spawnAccumMs = 1e9;
            algo.rainPulseStrip = [];
            algo.rainPulseBaseline = [0, 0, 0];
        }
        var dt = HSVUtil.audioSeconds(audio);
        if (algo.presetMode === "Rain Pulse")
            return renderRainPulse(width, height, dt, audio);
        var map = HSVUtil.createMap(width, height);

        var trigger = false;
        var intensity = 1.0;
        if (algo.presetTrigger === "Kick") {
            trigger = audio.version >= 6 ? audio.kickFired : audio.beatFired;
            intensity = audio.onsetIntensity;
        } else if (algo.presetTrigger === "Beat") {
            trigger = audio.beatFired;
            intensity = 1.0;
        } else {
            trigger = audio.onset;
            intensity = audio.onsetIntensity;
        }

        var eventName = algo.presetTrigger === "Kick" ? "kick" :
            algo.presetTrigger === "Beat" ? "beat" : "onset";
        var count = audio.events ? audio.events.delta[eventName] : (trigger ? 1 : 0);
        algo.spawnAccumMs += dt * 1000;
        var canSpawn = algo.spawnAccumMs >= algo.presetMinSpawnMs;

        for (var spawn = 0; canSpawn && spawn < Math.min(count, algo.presetMaxRipples); spawn++) {
            var gradient = (algo.colors && algo.colors.length > 0)
                ? algo.colors : DEFAULT_GRADIENT;
            // Hue cycles by ripple index
            var rippleIdx = algo.ripples.length;
            var color = HSVUtil.gradientAt(gradient, (rippleIdx % 12) / 11.0);
            var maxR = algo.presetMaxRadius * (0.5 + 0.5 * intensity);
            var ripple = {
                cx: Math.floor(Math.random() * width),
                cy: Math.floor(Math.random() * height),
                radius: 0,
                maxRadius: maxR,
                age: 0,
                lifeMs: algo.presetLifeMs,
                color: color
            };
            if (algo.ripples.length >= algo.presetMaxRipples) {
                // Replace oldest/weakest: pick the ripple with the largest age/lifeMs ratio.
                var worstIdx = 0;
                var worstScore = -1;
                for (var k = 0; k < algo.ripples.length; k++) {
                    var rk = algo.ripples[k];
                    var score = rk.age / rk.lifeMs;
                    if (score > worstScore) { worstScore = score; worstIdx = k; }
                }
                algo.ripples[worstIdx] = ripple;
            } else {
                algo.ripples.push(ripple);
            }
            algo.spawnAccumMs = 0;
        }

        for (var i = algo.ripples.length - 1; i >= 0; i--) {
            var r = algo.ripples[i];
            r.radius += algo.presetExpansionSpeed * dt;
            r.age += dt * 1000;
            if (r.age >= r.lifeMs || r.radius > r.maxRadius) {
                algo.ripples.splice(i, 1);
            }
        }

        var ringW = algo.presetRingWidth;
        for (var ri = 0; ri < algo.ripples.length; ri++) {
            var rp = algo.ripples[ri];
            var alpha = 1.0 - (rp.age / rp.lifeMs);
            if (alpha <= 0) continue;
            var x0 = Math.floor(rp.cx - rp.radius - ringW);
            var x1 = Math.ceil(rp.cx + rp.radius + ringW);
            var y0 = Math.floor(rp.cy - rp.radius - ringW);
            var y1 = Math.ceil(rp.cy + rp.radius + ringW);
            // Clamp render-side bounds.
            if (x0 < 0) x0 = 0;
            if (y0 < 0) y0 = 0;
            if (x1 > width - 1)  x1 = width - 1;
            if (y1 > height - 1) y1 = height - 1;
            for (var y = y0; y <= y1; y++) {
                for (var x = x0; x <= x1; x++) {
                    var dx = x - rp.cx;
                    var dy = y - rp.cy;
                    var d = Math.sqrt(dx * dx + dy * dy);
                    var edge = 1 - Math.abs(d - rp.radius) / ringW;
                    if (edge > 0) {
                        var contrib_v = rp.color.v * edge * alpha;
                        var idx = (y * width + x) * 3;
                        var ev = map[idx + 2];
                        if (contrib_v > ev) { map[idx] = rp.color.h; map[idx + 1] = rp.color.s; }
                        map[idx + 2] = Math.min(1, ev + contrib_v);
                    }
                }
            }
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
