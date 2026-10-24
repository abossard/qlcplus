/*
  Q Light Controller Plus
  audioblocks.js

  Copyright (c) QLC+ contributors
  Ported from LedFx "Block Reflections" effect (MIT License)
  Original by LedFX contributors: https://github.com/LedFx/LedFx

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
    algo.name = "Audio Blocks";
    algo.author = "Ported from LedFx";
    algo.acceptColors = 3;
    algo.usesAudio = true;
    algo.properties = new Array();
    algo.presetMode = "Artistic";
    algo.properties.push("name:mode|type:list|display:Response|values:Artistic,LedFx Block Reflections|write:setMode|read:getMode");
    algo.setMode = function(v) { algo.presetMode = v === "LedFx Block Reflections" ? v : "Artistic"; };
    algo.getMode = function() { return algo.presetMode; };

    algo.speed = 1.0;
    algo.properties.push(
      "name:speed|type:float|display:Speed (cyc/beat)|" +
      "write:setSpeed|read:getSpeed");

    algo.reactivity = 0.5;
    algo.properties.push(
      "name:reactivity|type:float|display:Reactivity|" +
      "write:setReactivity|read:getReactivity");

    algo.fix_hues = "Yes";
    algo.properties.push(
      "name:fix_hues|type:list|display:Fix Hues|" +
      "values:No,Yes|write:setFixHues|read:getFixHues");

    algo.setSpeed = function(_v) { algo.speed = HSVUtil.parseNumber(_v, algo.speed, 0, 10); };
    algo.getSpeed = function() { return algo.speed; };
    algo.setReactivity = function(_v) { algo.reactivity = HSVUtil.parseNumber(_v, algo.reactivity, 0, 1); };
    algo.getReactivity = function() { return algo.reactivity; };
    algo.setFixHues = function(_v) { algo.fix_hues = (_v === "No") ? "No" : "Yes"; };
    algo.getFixHues = function() { return algo.fix_hues; };

    // Per-track ratios relative to algo.speed (the base rate).
    var T3_RATIO = 5.0;
    var T4_RATIO = 2.0;
    var PI_SQUARED = Math.PI * Math.PI;

    algo.lowsPower = 0;
    var ledFxLowsPower = 0;
    var phase1 = 0;
    var phase3 = 0;
    var phase4 = 0;
    var ledFxMs = 0;
    var ledFxKeys = {audioKey: "", geometryKey: ""};
    var getLedFxOutputOptions = HSVUtil.attachStripTransformControls(algo, {
        prefix: "ledFx",
        display: "LedFx "
    });

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };


    function fixHueFast(hue) {
        hue = HSVUtil.mod1(hue);
        return HSVUtil.sin01((hue - 0.5) / 2.0);
    }

    // Calls plot(i, hue, v) for each of the n block pixels at track phases t1..t4.
    function forEachBlock(n, t1, t2, t3, t4, plot) {
        var m = 0.3 + HSVUtil.triangle(t1) * 0.2;
        var c = HSVUtil.triangle(t3) * 10.0 + 4.0 * HSVUtil.sin01(t4);
        var fixHues = algo.fix_hues !== "No";
        var offset = HSVUtil.sin01(t2);
        for (var i = 0; i < n; i++) {
            var h = i;
            h -= n / 2.0;
            h /= n;
            h *= c;
            h = HSVUtil.wrap(h, m);
            h += offset;

            var v = Math.abs(h);
            v += Math.abs(m) + t1;
            v = HSVUtil.mod1(v);
            v *= v;

            plot(i, fixHues ? fixHueFast(h) : HSVUtil.mod1(h), v);
        }
    }

    function renderArtistic(width, height, audio) {
        var map = HSVUtil.createMap(width, height);
        var keep = Math.pow(0.95, HSVUtil.elapsedFrames(audio));
        algo.lowsPower = audio.low * (1 - keep) + algo.lowsPower * keep;
        phase1 = (phase1 + audio.dt * algo.speed) % 1.0;
        phase3 = (phase3 + audio.dt * algo.speed * T3_RATIO) % 1.0;
        phase4 = (phase4 + audio.dt * algo.speed * T4_RATIO) % 1.0;
        forEachBlock(Math.max(1, width), phase1,
            phase1 * PI_SQUARED + (0.8 * algo.reactivity * algo.lowsPower),
            phase3 + (algo.reactivity * algo.lowsPower),
            phase4 * PI_SQUARED,
            function(x, hue, v) {
                for (var y = 0; y < height; y++)
                    HSVUtil.setPixel(map, width, x, y, hue, 1.0, v);
            });
        return map;
    }

    function renderLedFx(width, height, audio) {
        var map = HSVUtil.createMap(width, height);
        if (HSVUtil.audioStateChanged(ledFxKeys, audio, width, height)) {
            ledFxMs = 0;
            ledFxLowsPower = 0;
        }
        var dt = HSVUtil.audioSeconds(audio);
        var alpha = dt > 0 ? (1 - Math.pow(0.95, dt * 60)) : 0;
        ledFxLowsPower += (HSVUtil.powerForRange(audio, "Lows", true) - ledFxLowsPower) * alpha;
        ledFxMs += dt * 1000.0;
        var t1 = HSVUtil.time01(algo.speed, ledFxMs);
        var stops = HSVUtil.ledFxGradientStops(algo);
        var outputOptions = getLedFxOutputOptions();
        forEachBlock(Math.max(1, width * height), t1,
            t1 * PI_SQUARED + (0.8 * algo.reactivity * ledFxLowsPower),
            HSVUtil.time01(algo.speed * T3_RATIO, ledFxMs) + (algo.reactivity * ledFxLowsPower),
            HSVUtil.time01(algo.speed * T4_RATIO, ledFxMs) * PI_SQUARED,
            function(i, hue, v) {
                var base = HSVUtil.gradientRgbAt(stops, hue);
                var hsv = HSVUtil.rgbToHsvUnclipped(base[0] * v, base[1] * v, base[2] * v);
                HSVUtil.setPixel(map, width, i % width, Math.floor(i / width), hsv.h, hsv.s, hsv.v);
            });
        return HSVUtil.applyStripTransforms(map, width, height, outputOptions);
    }

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        if (!audio) return HSVUtil.createMap(width, height);
        return algo.presetMode === "LedFx Block Reflections"
            ? renderLedFx(width, height, audio)
            : renderArtistic(width, height, audio);
    };

    testAlgo = algo;
    return algo;
  }
)();
