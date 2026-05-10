/*
  Q Light Controller Plus
  audiobeatcolors.js

  Four-color beat-synced RGB Matrix effect.

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
    algo.name = "Audio Beat Colors";
    algo.author = "QLC+ contributors";
    algo.acceptColors = 4;
    algo.usesAudio = true;
    algo.properties = new Array();

    algo.presetTransition = 0;
    algo.properties.push(
      "name:presetTransition|type:list|display:Transition|" +
      "values:Cut,Fade,SweepLR,SweepTB,Pulse|write:setTransition|read:getTransition");

    algo.presetPulse = 4;
    algo.properties.push(
      "name:presetPulse|type:range|display:Beat Pulse|" +
      "values:0,10|write:setPulse|read:getPulse");

    algo.presetDownbeatBoost = 3;
    algo.properties.push(
      "name:presetDownbeatBoost|type:range|display:Downbeat Boost|" +
      "values:0,10|write:setDownbeatBoost|read:getDownbeatBoost");

    algo.presetSweepWidth = 3;
    algo.properties.push(
      "name:presetSweepWidth|type:range|display:Sweep Width|" +
      "values:1,10|write:setSweepWidth|read:getSweepWidth");

    algo.presetBeat1 = 1;
    algo.properties.push(
      "name:presetBeat1|type:list|display:Beat 1|" +
      "values:Off,On|write:setBeat1|read:getBeat1");

    algo.presetBeat2 = 1;
    algo.properties.push(
      "name:presetBeat2|type:list|display:Beat 2|" +
      "values:Off,On|write:setBeat2|read:getBeat2");

    algo.presetBeat3 = 1;
    algo.properties.push(
      "name:presetBeat3|type:list|display:Beat 3|" +
      "values:Off,On|write:setBeat3|read:getBeat3");

    algo.presetBeat4 = 1;
    algo.properties.push(
      "name:presetBeat4|type:list|display:Beat 4|" +
      "values:Off,On|write:setBeat4|read:getBeat4");

    algo.setTransition = function(_v) {
        if (_v === "Fade") algo.presetTransition = 1;
        else if (_v === "SweepLR") algo.presetTransition = 2;
        else if (_v === "SweepTB") algo.presetTransition = 3;
        else if (_v === "Pulse") algo.presetTransition = 4;
        else algo.presetTransition = 0;
    };
    algo.getTransition = function() {
        return ["Cut", "Fade", "SweepLR", "SweepTB", "Pulse"][algo.presetTransition];
    };
    algo.setPulse = function(_v) { algo.presetPulse = parseInt(_v); };
    algo.getPulse = function() { return algo.presetPulse; };
    algo.setDownbeatBoost = function(_v) { algo.presetDownbeatBoost = parseInt(_v); };
    algo.getDownbeatBoost = function() { return algo.presetDownbeatBoost; };
    algo.setSweepWidth = function(_v) { algo.presetSweepWidth = parseInt(_v); };
    algo.getSweepWidth = function() { return algo.presetSweepWidth; };
    algo.setBeat1 = function(_v) { algo.presetBeat1 = (_v === "On") ? 1 : 0; };
    algo.getBeat1 = function() { return algo.presetBeat1 ? "On" : "Off"; };
    algo.setBeat2 = function(_v) { algo.presetBeat2 = (_v === "On") ? 1 : 0; };
    algo.getBeat2 = function() { return algo.presetBeat2 ? "On" : "Off"; };
    algo.setBeat3 = function(_v) { algo.presetBeat3 = (_v === "On") ? 1 : 0; };
    algo.getBeat3 = function() { return algo.presetBeat3 ? "On" : "Off"; };
    algo.setBeat4 = function(_v) { algo.presetBeat4 = (_v === "On") ? 1 : 0; };
    algo.getBeat4 = function() { return algo.presetBeat4 ? "On" : "Off"; };

    var DEFAULT_COLORS = [0xFF3030, 0xFFD020, 0x30FF80, 0x3080FF];

    function clamp01(v) {
        return Math.max(0, Math.min(1, v));
    }

    function unpack(packed) {
        return [(packed >> 16) & 0xFF, (packed >> 8) & 0xFF, packed & 0xFF];
    }

    function mix(a, b, t) {
        t = clamp01(t);
        return [
            a[0] * (1 - t) + b[0] * t,
            a[1] * (1 - t) + b[1] * t,
            a[2] * (1 - t) + b[2] * t
        ];
    }

    function packedBeatColors() {
        var src = (algo.gradientColors && algo.gradientColors.length > 0)
            ? algo.gradientColors : DEFAULT_COLORS;
        var colors = src.slice();

        var originalLength = colors.length;
        for (var i = colors.length; i < 4; i++)
            colors.push(colors[i % originalLength]);

        return colors.slice(0, 4);
    }

    function beatIndexFor(audio, step) {
        if (audio && audio.beat.bpm > 0)
            return ((Math.floor(audio.bar.beat) % 4) + 4) % 4;
        return ((step % 4) + 4) % 4;
    }

    function beforeEdgeBlend(value, phase, width) {
        var edge = phase * (1 + width) - width * 0.5;
        return clamp01((edge - value + width * 0.5) / width);
    }

    function pulseBlend(x01, y01, phase, width) {
        var dx = x01 - 0.5;
        var dy = y01 - 0.5;
        var distance = Math.sqrt(dx * dx + dy * dy) / Math.sqrt(0.5);
        return beforeEdgeBlend(distance, phase, width);
    }

    algo.rgbMapStepCount = function(width, height) { return 4; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() {
        return packedBeatColors();
    };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        var map = RGBUtil.createMap(width, height);
        var packed = packedBeatColors();
        var beat = beatIndexFor(audio, step);
        var beatMask = [algo.presetBeat1, algo.presetBeat2, algo.presetBeat3, algo.presetBeat4];
        if (!beatMask[beat])
            return map;

        var prevBeat = beat;
        for (var i = 1; i <= 4; i++) {
            var candidate = (beat - i + 4) % 4;
            if (beatMask[candidate]) { prevBeat = candidate; break; }
        }
        var currentColor = unpack(packed[beat] | 0);
        var previousColor = unpack(packed[prevBeat] | 0);
        var phase = (audio && audio.beat.bpm > 0) ? clamp01(audio.beat.phase) : 0;
        var cosPulse = audio ? clamp01(audio.beat.cosPulse) : 0;
        var brightness = 0.5 + 0.5 * cosPulse;

        brightness += cosPulse * (algo.presetPulse / 10.0);
        if (audio && audio.bar.downbeat)
            brightness += algo.presetDownbeatBoost / 10.0;
        brightness = clamp01(brightness);

        var sweepWidth = 0.02 + 0.28 * (algo.presetSweepWidth / 10.0);

        for (var y = 0; y < height; y++) {
            var y01 = height <= 1 ? 0.5 : y / (height - 1);
            for (var x = 0; x < width; x++) {
                var x01 = width <= 1 ? 0.5 : x / (width - 1);
                var color;

                if (algo.presetTransition === 1) {
                    color = mix(previousColor, currentColor, phase);
                } else if (algo.presetTransition === 2) {
                    color = mix(previousColor, currentColor, beforeEdgeBlend(x01, phase, sweepWidth));
                } else if (algo.presetTransition === 3) {
                    color = mix(previousColor, currentColor, beforeEdgeBlend(y01, phase, sweepWidth));
                } else if (algo.presetTransition === 4) {
                    color = mix(previousColor, currentColor, pulseBlend(x01, y01, phase, sweepWidth));
                } else {
                    color = currentColor;
                }

                map[y][x] = RGBUtil.rgb(
                    color[0] * brightness,
                    color[1] * brightness,
                    color[2] * brightness);
            }
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
