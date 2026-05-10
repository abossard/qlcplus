/*
  Q Light Controller Plus
  audiowater.js

  Copyright (c) QLC+ contributors
  Ported from LedFX "Water" effect (MIT License)

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
    algo.name = "Audio Water";
    algo.author = "Ported from LedFx";
    algo.acceptColors = 0;
    algo.usesAudio = true;
    algo.properties = new Array();

    algo.speed = 1;
    algo.vertical_shift = 0.12;
    algo.bass_size = 8;
    algo.mids_size = 6;
    algo.high_size = 3;
    algo.viscosity = 6;

    algo.properties.push("name:speed|type:float|display:Speed|write:setSpeed|read:getSpeed");
    algo.properties.push("name:vertical_shift|type:float|display:Vertical Shift|write:setVerticalShift|read:getVerticalShift");
    algo.properties.push("name:bass_size|type:float|display:Bass Size|write:setBassSize|read:getBassSize");
    algo.properties.push("name:mids_size|type:float|display:Mids Size|write:setMidsSize|read:getMidsSize");
    algo.properties.push("name:high_size|type:float|display:High Size|write:setHighSize|read:getHighSize");
    algo.properties.push("name:viscosity|type:float|display:Viscosity|write:setViscosity|read:getViscosity");

    function clamp(v, lo, hi) { var n = parseFloat(v); return isNaN(n) ? lo : Math.max(lo, Math.min(hi, n)); }
    algo.setSpeed = function(v) { algo.speed = clamp(v, 1, 3); };
    algo.getSpeed = function() { return algo.speed; };
    algo.setVerticalShift = function(v) { algo.vertical_shift = clamp(v, -0.2, 1); };
    algo.getVerticalShift = function() { return algo.vertical_shift; };
    algo.setBassSize = function(v) { algo.bass_size = clamp(v, 0, 15); };
    algo.getBassSize = function() { return algo.bass_size; };
    algo.setMidsSize = function(v) { algo.mids_size = clamp(v, 0, 15); };
    algo.getMidsSize = function() { return algo.mids_size; };
    algo.setHighSize = function(v) { algo.high_size = clamp(v, 0, 15); };
    algo.getHighSize = function() { return algo.high_size; };
    algo.setViscosity = function(v) { algo.viscosity = clamp(v, 2, 12); };
    algo.getViscosity = function() { return algo.viscosity; };

    var buf0 = null, buf1 = null, curBuf = 0;
    var midsEmitters = [[0.25, 1.0], [0.75, -1.0]];
    var highEmitters = [[0.125, 1.5], [0.375, -2.5], [0.625, 2.5], [0.875, -1.5]];

    function init(w) {
        buf0 = new Array(w); buf1 = new Array(w);
        for (var i = 0; i < w; i++) { buf0[i] = 0; buf1[i] = 0; }
        curBuf = 0;
        midsEmitters = [[0.25, 1.0], [0.75, -1.0]];
        highEmitters = [[0.125, 1.5], [0.375, -2.5], [0.625, 2.5], [0.875, -1.5]];
    }

    function createDrop(pos, h, w) {
        if (pos < 1 || pos >= w - 1) return;
        buf0[pos] = buf0[pos - 1] = buf0[pos + 1] = h;
        buf1[pos] = buf1[pos - 1] = buf1[pos + 1] = h;
    }

    function smooth3(arr, w) {
        if (w < 3) return;
        var prev = arr[0];
        for (var i = 1; i < w - 1; i++) {
            var cur = arr[i];
            arr[i] = (prev + cur + arr[i + 1]) / 3;
            prev = cur;
        }
    }

    function doRipple(dampFactor, w) {
        var src = (curBuf === 0) ? buf1 : buf0;
        var dst = (curBuf === 0) ? buf0 : buf1;
        for (var i = 1; i < w - 1; i++)
            dst[i] = ((src[i - 1] + src[i + 1] + src[i] * 2) / 2) - dst[i];
        smooth3(dst, w);
        for (var i = 0; i < w; i++)
            dst[i] -= dst[i] / dampFactor;
        curBuf = 1 - curBuf;
    }

    algo.rgbMapStepCount = function(width, height) { return 1; };

    algo.rgbMap = function(width, height, rgb, step, audio)
    {
        if (!buf0 || buf0.length !== width) init(width);
        var map = RGBUtil.createMap(width, height);
        if (!audio) return map;
        if (width < 5) return map;

        var speed = algo.speed;
        var dampFactor = Math.pow(2, algo.viscosity);
        var shift = algo.vertical_shift;

        var lowP = Math.min(1, Math.max(0, Math.pow(audio.power.low, 2)));
        var midP = Math.min(1, Math.max(0, Math.pow(audio.power.mid, 2)));
        var hiP  = Math.min(1, Math.max(0, Math.pow(audio.power.high, 2)));

        // 3 bass emitters at fixed positions
        createDrop(1, lowP * algo.bass_size, width);
        createDrop(Math.floor(width / 2), lowP * algo.bass_size, width);
        createDrop(width - 2, lowP * algo.bass_size, width);

        // 2 mid emitters (drifting)
        for (var i = 0; i < midsEmitters.length; i++) {
            var pos = 1 + Math.floor(midsEmitters[i][0] * (width - 2));
            createDrop(pos, midP * algo.mids_size, width);
            midsEmitters[i][0] += 0.0002 * midsEmitters[i][1] * speed;
            if (midsEmitters[i][0] < 0) midsEmitters[i][0] += 1;
            else if (midsEmitters[i][0] > 1) midsEmitters[i][0] -= 1;
        }

        // 4 high emitters (drifting)
        for (var i = 0; i < highEmitters.length; i++) {
            var pos = 1 + Math.floor(highEmitters[i][0] * (width - 2));
            createDrop(pos, hiP * algo.high_size, width);
            highEmitters[i][0] += 0.0002 * highEmitters[i][1] * speed;
            if (highEmitters[i][0] < 0) highEmitters[i][0] += 1;
            else if (highEmitters[i][0] > 1) highEmitters[i][0] -= 1;
        }

        // Ripple simulation
        var speedInt = Math.floor(speed);
        for (var s = 0; s < speedInt; s++)
            doRipple(dampFactor, width);

        // Render HSV
        var current = (curBuf === 0) ? buf0 : buf1;
        for (var x = 0; x < width; x++) {
            var val = current[x];

            // h = triangle(val)
            var h = 1 - 2 * Math.abs(val - 0.5);

            // vScaled = (val + shift) / (1 + shift)
            var vScaled = (val + shift) / (1 + shift);

            // s = clamp(2 - (vScaled + shift), 0, 1)
            var s = Math.max(0, Math.min(1, 2 - (vScaled + shift)));

            // v = clamp(vScaled, 0, 1)
            var v = Math.max(0, Math.min(1, vScaled));

            var packed = RGBUtil.hsvToRgb(h, s, v);
            for (var y = 0; y < height; y++)
                map[y][x] = packed;
        }

        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
