/*
  Q Light Controller Plus
  rgbutil.js

  Shared helpers for QLC+ RGB scripts. The engine consumes only HSV
  Float32Array maps; this file therefore exposes no RGB packing helpers.

  Pixel map contract (rgbscriptv4.cpp::extractFlatArrayMap):
    - rgbMap() must return a Float32Array of length width*height*3
    - Each pixel is 3 interleaved floats: [h, s, v] in [0,1]
    - Engine converts HSV -> packed RGB before writing to RGBMap

  Color injection contract (rgbscriptv4.cpp::injectGradientArrays):
    - algo.color              = {h, s, v}              (primary user color)
    - algo.gradientColors     = [{h,s,v}, ...]         (user gradient stops, >=1)
    - algo.gradientBandColors = [{h,s,v}, {h,s,v}, {h,s,v}]
                                 (3 evenly sampled stops for low/mid/high banks)

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

var RGBUtil = {};

/**
 * Allocate an HSV pixel map. Returns a Float32Array of length width*height*3
 * (row-major, 3 floats per pixel: H, S, V).
 */
RGBUtil.createMap = function(width, height) {
    return new Float32Array(width * height * 3);
};

/**
 * Set an HSV pixel in a Float32Array map.
 * Inline `var i=(y*w+x)*3; map[i]=h; map[i+1]=s; map[i+2]=v;` is faster in
 * tight inner loops; prefer this helper outside hot paths.
 */
RGBUtil.setPixel = function(map, width, x, y, h, s, v) {
    var i = (y * width + x) * 3;
    map[i] = h;
    map[i + 1] = s;
    map[i + 2] = v;
};

/**
 * Linearly interpolate the HSV gradient defined by `stops` (evenly spaced
 * between 0 and 1) at position t in [0,1]. Hue is interpolated along the
 * shortest arc so red<->magenta does not drag through green. The C++ engine
 * mirrors this in injectGradientArrays() when sampling the band palette.
 */
RGBUtil.gradientAt = function(stops, t) {
    if (!stops || stops.length === 0) return {h: 0, s: 0, v: 0};
    if (stops.length === 1) return {h: stops[0].h, s: stops[0].s, v: stops[0].v};
    if (t < 0) t = 0; else if (t > 1) t = 1;
    var pos = t * (stops.length - 1);
    var idx = Math.floor(pos);
    if (idx >= stops.length - 1)
        return {h: stops[stops.length-1].h, s: stops[stops.length-1].s, v: stops[stops.length-1].v};
    var frac = pos - idx;
    var a = stops[idx], b = stops[idx + 1];
    var dh = b.h - a.h;
    if (dh > 0.5) dh -= 1;
    else if (dh < -0.5) dh += 1;
    var h = a.h + frac * dh;
    h = h - Math.floor(h);
    return {
        h: h,
        s: a.s + frac * (b.s - a.s),
        v: a.v + frac * (b.v - a.v)
    };
};

/**
 * Resample an array to a new size using linear interpolation.
 * Matches numpy.interp behaviour over an evenly spaced grid.
 */
RGBUtil.interpolate = function(arr, size) {
    if (arr.length === 0) return new Array(size).fill(0);
    if (arr.length === size) return arr.slice();
    if (arr.length === 1) return new Array(size).fill(arr[0]);

    var result = new Array(size);
    var ratio = (arr.length - 1) / (size - 1);
    for (var i = 0; i < size; i++) {
        var pos = i * ratio;
        var lo = Math.floor(pos);
        var hi = Math.min(lo + 1, arr.length - 1);
        var t = pos - lo;
        result[i] = arr[lo] * (1 - t) + arr[hi] * t;
    }
    return result;
};

/** Clamp x into [0, 1]. */
RGBUtil.clamp01 = function(x) {
    return x < 0 ? 0 : (x > 1 ? 1 : x);
};

/** Wrap x into [0, 1) (positive modulo 1). */
RGBUtil.mod1 = function(x) {
    var m = x - Math.floor(x);
    return m < 0 ? m + 1 : m;
};

/** Triangle wave, period 1, range [0, 1]. f(0)=0, f(0.5)=1, f(1)=0. */
RGBUtil.triangle = function(x) {
    return 1 - Math.abs(2 * RGBUtil.mod1(x) - 1);
};

/** Sine wave normalized to [0, 1] with period 1. */
RGBUtil.sin01 = function(x) {
    return 0.5 + 0.5 * Math.sin(2 * Math.PI * x);
};


/***********************************************************************
 * Time / beat helpers
 ***********************************************************************/

/**
 * LedFX-compatible sawtooth 0->1.
 * Loops every 65.536/modifier seconds (65536/modifier ms).
 */
RGBUtil.time01 = function(modifier, timestepMs) {
    if (modifier <= 0 || !isFinite(modifier) || !isFinite(timestepMs)) return 0;
    var period = 65536.0 / modifier;
    return (timestepMs % period) / period;
};

/**
 * BPM-locked sawtooth 0->1.
 *   speed = 1.0  -> 1 cycle per beat (quarter note)
 *   speed = 2.0  -> 2 cycles per beat (8th note)
 *   speed = 0.25 -> 1 cycle per bar (whole note)
 * Falls back to 120 BPM when bpm <= 0 (no audio).
 */
RGBUtil.beatTime = function(speed, state, bpm, dtMs) {
    if (!state) return 0;
    if (!isFinite(speed)) speed = 0;
    if (!isFinite(dtMs)) dtMs = 0;
    if (speed === 0) return state.phase || 0;
    var effectiveBpm = (isFinite(bpm) && bpm > 0) ? bpm : 120;
    var beatMs = 60000 / effectiveBpm;
    var p = (state.phase || 0) + (dtMs / beatMs) * speed;
    p = p - Math.floor(p);
    state.phase = p;
    return p;
};

/** Same as beatTime but returns 0->2*PI for use with Math.sin(). */
RGBUtil.beatAngle = function(speed, state, bpm, dtMs) {
    return RGBUtil.beatTime(speed, state, bpm, dtMs) * 2 * Math.PI;
};

/**
 * BPM-locked continuous accumulator (no wrap). For noise field offsets
 * and angles where phase wrapping causes visible discontinuities.
 */
RGBUtil.beatPosition = function(speed, state, bpm, dtMs) {
    if (!state) return 0;
    if (!isFinite(speed)) speed = 0;
    if (!isFinite(dtMs)) dtMs = 0;
    if (speed === 0) return state.position || 0;
    var effectiveBpm = (isFinite(bpm) && bpm > 0) ? bpm : 120;
    state.position = (state.position || 0) + (dtMs / (60000 / effectiveBpm)) * speed;
    return state.position;
};


/***********************************************************************
 * 2D Simplex noise (public domain, Stefan Gustavson). Range -1 to 1.
 ***********************************************************************/
RGBUtil._grad3 = [[1,1,0],[-1,1,0],[1,-1,0],[-1,-1,0],[1,0,1],[-1,0,1],[1,0,-1],[-1,0,-1],[0,1,1],[0,-1,1],[0,1,-1],[0,-1,-1]];
RGBUtil._perm = (function() {
    var p = [151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,190,6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,88,237,149,56,87,174,20,125,136,171,168,68,175,74,165,71,134,139,48,27,166,77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,102,143,54,65,25,63,161,1,216,80,73,209,76,132,187,208,89,18,169,200,196,135,130,116,188,159,86,164,100,109,198,173,186,3,64,52,217,226,250,124,123,5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,223,183,170,213,119,248,152,2,44,154,163,70,221,153,101,155,167,43,172,9,129,22,39,253,19,98,108,110,79,113,224,232,178,185,112,104,218,246,97,228,251,34,242,193,238,210,144,12,191,179,162,241,81,51,145,235,249,14,239,107,49,192,214,31,181,199,106,157,184,84,204,176,115,121,50,45,127,4,150,254,138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180];
    var perm = new Array(512);
    for (var i = 0; i < 512; i++) perm[i] = p[i & 255];
    return perm;
})();

RGBUtil.simplex2d = function(xin, yin) {
    var F2 = 0.5 * (Math.sqrt(3) - 1);
    var G2 = (3 - Math.sqrt(3)) / 6;
    var perm = RGBUtil._perm;
    var grad3 = RGBUtil._grad3;

    var s = (xin + yin) * F2;
    var i = Math.floor(xin + s);
    var j = Math.floor(yin + s);
    var t = (i + j) * G2;
    var X0 = i - t;
    var Y0 = j - t;
    var x0 = xin - X0;
    var y0 = yin - Y0;

    var i1, j1;
    if (x0 > y0) { i1 = 1; j1 = 0; }
    else { i1 = 0; j1 = 1; }

    var x1 = x0 - i1 + G2;
    var y1 = y0 - j1 + G2;
    var x2 = x0 - 1 + 2 * G2;
    var y2 = y0 - 1 + 2 * G2;

    var ii = i & 255;
    var jj = j & 255;
    var gi0 = perm[ii + perm[jj]] % 12;
    var gi1 = perm[ii + i1 + perm[jj + j1]] % 12;
    var gi2 = perm[ii + 1 + perm[jj + 1]] % 12;

    var n0 = 0, n1 = 0, n2 = 0;
    var t0 = 0.5 - x0 * x0 - y0 * y0;
    if (t0 >= 0) { t0 *= t0; n0 = t0 * t0 * (grad3[gi0][0] * x0 + grad3[gi0][1] * y0); }
    var t1 = 0.5 - x1 * x1 - y1 * y1;
    if (t1 >= 0) { t1 *= t1; n1 = t1 * t1 * (grad3[gi1][0] * x1 + grad3[gi1][1] * y1); }
    var t2 = 0.5 - x2 * x2 - y2 * y2;
    if (t2 >= 0) { t2 *= t2; n2 = t2 * t2 * (grad3[gi2][0] * x2 + grad3[gi2][1] * y2); }

    return 70 * (n0 + n1 + n2);
};

/**
 * Generate a 2D noise field (height x width), values 0-1.
 * Returns a 2D array indexed as field[y][x].
 */
RGBUtil.noiseField2d = function(width, height, freq, offsetX, offsetY) {
    var field = new Array(height);
    for (var y = 0; y < height; y++) {
        field[y] = new Array(width);
        for (var x = 0; x < width; x++) {
            var n = RGBUtil.simplex2d(
                (x / Math.max(1, width - 1)) * freq + offsetX,
                (y / Math.max(1, height - 1)) * freq + offsetY
            );
            field[y][x] = (n + 1) * 0.5;
        }
    }
    return field;
};
