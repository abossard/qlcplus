/*
  Q Light Controller Plus
  audiowaterfall.js

  Copyright (c) QLC+ contributors

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

var testAlgo;

(function() {
    var algo = {};
    algo.apiVersion = 3;
    algo.name = "Audio Waterfall";
    algo.author = "QLC+ contributors";
    algo.acceptColors = 5;
    algo.usesAudio = true;
    algo.properties = [];

    algo.presetBands = 16;
    algo.presetAggregation = "Mean";
    algo.presetCenter = "Off";
    algo.presetDropSeconds = 3.0;
    algo.presetFade = 0.0;
    algo.properties.push("name:bands|type:range|display:Bands|values:1,64|write:setBands|read:getBands");
    algo.properties.push("name:aggregation|type:list|display:Aggregation|values:Mean,Max|write:setAggregation|read:getAggregation");
    algo.properties.push("name:centerMode|type:list|display:Center Mode|values:Off,On|write:setCenterMode|read:getCenterMode");
    algo.properties.push("name:dropSeconds|type:float|values:0.1,10|display:Drop Seconds|write:setDropSeconds|read:getDropSeconds");
    algo.properties.push("name:fadeOut|type:float|values:0,1|display:Fade Out|write:setFadeOut|read:getFadeOut");
    algo.setBands = function(v) { algo.presetBands = Math.max(1, Math.min(64, parseInt(v) || 1)); };
    algo.getBands = function() { return algo.presetBands; };
    algo.setAggregation = function(v) { algo.presetAggregation = v === "Max" ? "Max" : "Mean"; };
    algo.getAggregation = function() { return algo.presetAggregation; };
    algo.setCenterMode = function(v) { algo.presetCenter = v === "On" ? "On" : "Off"; };
    algo.getCenterMode = function() { return algo.presetCenter; };
    algo.setDropSeconds = function(v) { algo.presetDropSeconds = Math.max(0.1, Math.min(10, parseFloat(v) || 0.1)); };
    algo.getDropSeconds = function() { return algo.presetDropSeconds; };
    algo.setFadeOut = function(v) { algo.presetFade = Math.max(0, Math.min(1, parseFloat(v) || 0)); };
    algo.getFadeOut = function() { return algo.presetFade; };
    algo.setFade = function(v) {
        if (v === "On") algo.presetFade = 1;
        else if (v === "Off") algo.presetFade = 0;
        else algo.setFadeOut(v);
    };
    algo.getFade = function() { return algo.presetFade > 0 ? "On" : "Off"; };
    var readOutputControls = HSVUtil.attachStripTransformControls(algo);

    var state = { geometryKey: "", audioKey: "", rows: [], dropRemainder: 0 };

    function reset(geometryKey, audioKey, width, height) {
        state.geometryKey = geometryKey;
        state.audioKey = audioKey;
        state.rows = [];
        for (var i = 0; i < height; i++) {
            state.rows.push(makeEmptyRow(width));
        }
        state.dropRemainder = 0;
    }

    function makeEmptyRow(width) {
        var row = new Array(width);
        for (var x = 0; x < width; x++) row[x] = [0, 0, 0];
        return row;
    }

    function bandVolumes(values, width, height) {
        var pixelCount = Math.max(1, width * height);
        var samples = HSVUtil.interpolate(values, pixelCount);
        for (var i = 0; i < samples.length; i++)
            samples[i] = HSVUtil.clamp01(samples[i]);
        var bands = Math.max(1, Math.min(algo.presetBands, pixelCount));
        var out = new Array(bands);
        var base = Math.floor(pixelCount / bands);
        var extra = pixelCount % bands;
        var cursor = 0;
        for (var b = 0; b < bands; b++) {
            var len = base + (b < extra ? 1 : 0);
            var end = cursor + Math.max(1, len);
            var sum = 0;
            var peak = 0;
            var n = 0;
            for (var j = cursor; j < end && j < samples.length; j++) {
                var v = samples[j];
                sum += v;
                if (v > peak) peak = v;
                n++;
            }
            out[b] = n > 0 ? (algo.presetAggregation === "Max" ? peak : sum / n) : 0;
            cursor = end;
        }
        return out;
    }

    function buildRow(width, height, values) {
        var grouped = bandVolumes(values, width, height);
        var bands = grouped.length;
        var row = new Array(width);
        var colors = algo.colors && algo.colors.length
            ? algo.colors
            : [{ h: 0, s: 1, v: 1 }, { h: 0.58, s: 1, v: 1 }];
        for (var i = 0; i < bands; i++) {
            var start = Math.floor((width / bands) * i);
            var end = Math.max(start, Math.floor((width / bands) * (i + 1) - 1));
            var rgb = HSVUtil.gradientRgbAt(colors, HSVUtil.clamp01(grouped[i]));
            for (var x = start; x <= end && x < width; x++)
                row[x] = [rgb[0], rgb[1], rgb[2]];
        }
        for (var fill = 0; fill < width; fill++)
            if (!row[fill]) row[fill] = [0, 0, 0];
        return row;
    }

    function shiftNormal(width, height) {
        if (height < 2) return;
        for (var y = height - 1; y > 0; y--) state.rows[y] = state.rows[y - 1];
        state.rows[0] = makeEmptyRow(width);
    }

    function shiftCenter(width, height) {
        var odd = (height % 2) !== 0;
        if ((odd && height < 3) || (!odd && height < 4)) return;
        var half = Math.floor(height / 2);
        for (var y = height - 1; y > half; y--) state.rows[y] = state.rows[y - 1];
        var topMax = odd ? half : half - 1;
        for (var y2 = 0; y2 < topMax; y2++) state.rows[y2] = state.rows[y2 + 1];
        state.rows[half] = makeEmptyRow(width);
        if (!odd) state.rows[half - 1] = makeEmptyRow(width);
    }

    function writeTopRow(row, width, height) {
        if (algo.presetCenter === "On") {
            var half = Math.floor(height / 2);
            state.rows[half] = row;
            if ((height % 2) === 0 && half > 0) state.rows[half - 1] = row.slice();
        } else {
            state.rows[0] = row;
        }
    }

    algo.rgbMapStepCount = function() { return 1; };
    algo.rgbMapSetColors = function() {};
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step, audio) {
        var map = HSVUtil.createMap(width, height);
        var geometryKey = [width, height].join("|");
        var audioKey = HSVUtil.audioIdentityKey(audio, state.audioKey);
        if (state.geometryKey !== geometryKey || state.audioKey !== audioKey || state.rows.length !== height)
            reset(geometryKey, audioKey, width, height);

        var dt = HSVUtil.audioSeconds(audio);
        var dropTick = Math.max(0.0001, algo.presetDropSeconds / Math.max(1, height));
        if (algo.presetCenter === "On") dropTick *= 2;
        var total = dt + state.dropRemainder;
        var ticks = Math.floor(total / dropTick);
        state.dropRemainder = total - ticks * dropTick;
        if (ticks >= height) {
            for (var clearY = 0; clearY < height; clearY++)
                state.rows[clearY] = makeEmptyRow(width);
            ticks = 0;
            state.dropRemainder = 0;
        }
        while (ticks-- > 0) {
            if (algo.presetCenter === "On") shiftCenter(width, height);
            else shiftNormal(width, height);
        }

        var bank = audio && audio.banks && audio.banks.full;
        var values = bank && bank.count ? (bank.novelty || bank.processed || []) : [];
        var row = buildRow(width, height, values);
        writeTopRow(row, width, height);

        var halfHeight = Math.max(1, Math.floor(height / 2));
        for (var y = 0; y < height; y++) {
            var fade = 1;
            if (algo.presetFade > 0) {
                if (algo.presetCenter === "On") {
                    var dist = Math.abs(y - halfHeight);
                    fade = Math.max(0, 1 - (dist / halfHeight) * algo.presetFade * 2);
                } else {
                    fade = Math.max(0, 1 - (y / halfHeight) * algo.presetFade);
                }
            }
            for (var x2 = 0; x2 < width; x2++) {
                var p = state.rows[y][x2];
                var r = p[0] * fade;
                var g = p[1] * fade;
                var b = p[2] * fade;
                var hsv = HSVUtil.rgbToHsvUnclipped(r, g, b);
                HSVUtil.setPixel(map, width, x2, y, hsv.h, hsv.s, hsv.v);
            }
        }
        return HSVUtil.applyStripTransforms(map, width, height, readOutputControls());
    };

    testAlgo = algo;
    return algo;
})();
