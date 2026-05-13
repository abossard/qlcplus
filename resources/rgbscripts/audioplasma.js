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

    var DEFAULT_HSV_STOPS = [
        { h: 0.000, s: 1.0, v: 1.0 },  // red
        { h: 0.078, s: 1.0, v: 1.0 },  // orange
        { h: 0.137, s: 1.0, v: 1.0 },  // yellow
        { h: 0.333, s: 1.0, v: 1.0 },  // green
        { h: 0.444, s: 1.0, v: 0.78 },
        { h: 0.667, s: 1.0, v: 1.0 },  // blue
        { h: 0.833, s: 1.0, v: 0.50 }, // purple
        { h: 0.917, s: 1.0, v: 1.0 }   // magenta
    ];

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

    function clamp(v, lo, hi) { if (isNaN(v)) return lo; return Math.max(lo, Math.min(hi, v)); }
    algo.setDensity = function(v) { algo.density = clamp(parseFloat(v), 0.001, 2.0); };
    algo.getDensity = function() { return algo.density; };
    algo.setLower = function(v) { algo.lower = clamp(parseFloat(v), 0.01, 1.0); };
    algo.getLower = function() { return algo.lower; };
    algo.setDensityVertical = function(v) { algo.density_vertical = clamp(parseFloat(v), 0.01, 0.3); };
    algo.getDensityVertical = function() { return algo.density_vertical; };
    algo.setTwist = function(v) { algo.twist = clamp(parseFloat(v), 0.01, 0.3); };
    algo.getTwist = function() { return algo.twist; };
    algo.setRadius = function(v) { algo.radius = clamp(parseFloat(v), 0.01, 1.0); };
    algo.getRadius = function() { return algo.radius; };
    algo.setFrequencyRange = function(v) { algo.frequency_range = String(v); };
    algo.getFrequencyRange = function() { return algo.frequency_range; };

    var timeState = { position: 0 };

    function gradientStops() {
        return (algo.colors && algo.colors.length > 0)
            ? algo.colors : DEFAULT_HSV_STOPS;
    }

    function powerFor(audio) {
        if (algo.frequency_range === "Beat") return audio.power.detail.beat;
        if (algo.frequency_range === "Bass") return audio.power.detail.bass;
        if (algo.frequency_range === "Mids") return audio.power.mid;
        if (algo.frequency_range === "High") return audio.power.high;
        return audio.power.low; // Lows (beat+bass)
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

        var dtMs = audio.timing.consumerDtMs > 0 ? audio.timing.consumerDtMs : 40;
        var bpm = (audio.beat) ? audio.beat.bpm : 0;

        // BPM-scaled free-running time replaces LedFX's wall-clock self.now.
        // One unit of "time" advances per beat (matches LedFX seconds at 60 BPM).
        var time = HSVUtil.beatPosition(1.0, timeState, bpm, dtMs);

        var power = powerFor(audio);
        var density = algo.density;
        var lower = algo.lower;
        var density_vertical = algo.density_vertical;
        var twist = algo.twist;
        var radius = algo.radius;

        // scale = lower + (power * density)
        var scale = lower + (power * density);

        // Coordinate ranges, matching np.ogrid[0:min(W,W*scale):complex(W)]
        var xExtent = Math.min(width,  width  * scale);
        var yExtent = Math.min(height, height * scale);
        var xStep = (width  > 1) ? xExtent / (width  - 1) : 0;
        var yStep = (height > 1) ? yExtent / (height - 1) : 0;

        var hsvStops = gradientStops();

        for (var iy = 0; iy < height; iy++) {
            var y = iy * yStep;
            for (var ix = 0; ix < width; ix++) {
                var x = ix * xStep;

                var v1 = Math.sin(x * 0.1 + time) * Math.cos(y * 0.1 - time);
                var v2 = Math.sin((x * density_vertical + y * twist + time) * 2.5);
                var v3 = Math.sin(Math.sqrt(x * x + y * y) * radius - time);

                // No per-frame normalization. Map raw sum into [0,1].
                var t = (v1 + v2 + v3 + 3) / 6.0;
                if (t < 0) t = 0; else if (t > 1) t = 1;

                var hsv = HSVUtil.gradientAt(hsvStops, t);
                var i = (iy * width + ix) * 3;
                map[i] = hsv.h; map[i+1] = hsv.s; map[i+2] = hsv.v;
            }
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
