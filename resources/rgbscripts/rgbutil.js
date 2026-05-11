/*
  Q Light Controller Plus
  rgbutil.js

  Generic (non-audio) RGB visual helpers shared across bundled
  rgbscripts: color packing, HSV conversion, 2D map allocation,
  array resampling, and 2D simplex noise.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

var RGBUtil = {};

/**
 * Pack RGB values into a single 0xRRGGBB integer for QLC+ RGBMap.
 * Each component is clamped to 0-255 and rounded.
 *
 * @param {number} r - Red 0-255
 * @param {number} g - Green 0-255
 * @param {number} b - Blue 0-255
 * @returns {number} 0xRRGGBB packed colour
 */
RGBUtil.rgb = function(r, g, b) {
    r = Math.max(0, Math.min(255, Math.round(r)));
    g = Math.max(0, Math.min(255, Math.round(g)));
    b = Math.max(0, Math.min(255, Math.round(b)));
    return (r << 16) | (g << 8) | b;
};

/**
 * HSV to RGB conversion.
 * @param {number} h - Hue (0-1, wraps)
 * @param {number} s - Saturation (0-1)
 * @param {number} v - Value (0-1)
 * @returns {Array} [r, g, b] each 0-255
 */
RGBUtil.hsv2rgb = function(h, s, v) {
    h = ((h % 1) + 1) % 1; // wrap to 0-1
    var i = Math.floor(h * 6);
    var f = h * 6 - i;
    var p = v * (1 - s);
    var q = v * (1 - f * s);
    var t = v * (1 - (1 - f) * s);
    var r, g, b;
    switch (i % 6) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        case 5: r = v; g = p; b = q; break;
    }
    return [Math.round(r * 255), Math.round(g * 255), Math.round(b * 255)];
};

/**
 * Create an empty pixel map. Returns a flat Uint32Array of length
 * width*height (row-major: index = y*width + x), per the Phase 3
 * fast-path contract with the C++ engine (engine/src/rgbscriptv4.cpp).
 *
 * Note: this is no longer a 2D nested Array. Code that previously did
 * `map[y][x] = c` must use `map[y * width + x] = c`.
 *
 * @param {number} width
 * @param {number} height
 * @returns {Uint32Array} flat row-major map of zeros, length = width*height
 */
RGBUtil.createMap = function(width, height) {
    return new Uint32Array(width * height);
};

/**
 * Alias of createMap kept for clarity in scripts that want to make the
 * flat-array contract explicit at the call site.
 *
 * @param {number} width
 * @param {number} height
 * @returns {Uint32Array} flat row-major map of zeros, length = width*height
 */
RGBUtil.createFlatMap = function(width, height) {
    return new Uint32Array(width * height);
};

/**
 * Set a pixel in a flat Uint32Array map. Helper for readability;
 * prefer inline `map[y * width + x] = color` in hot inner loops.
 *
 * @param {Uint32Array} map
 * @param {number} width  - row stride
 * @param {number} x
 * @param {number} y
 * @param {number} color  - packed 0xAARRGGBB
 */
RGBUtil.setPixel = function(map, width, x, y, color) {
    map[y * width + x] = color;
};

/**
 * Resample an array to a new size using linear interpolation.
 * Matches numpy.interp behaviour over an evenly spaced grid.
 *
 * @param {Array} arr  - Source array
 * @param {number} size - Target size
 * @returns {Array} Interpolated array of length 'size'
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

/**
 * Compact a raw color array (as received by rgbMapSetColors) into the list of
 * active gradient stops. Invalid/reset slots are signalled by the engine as 0
 * (see RGBMatrix::updateColorDelta) and are skipped — they do NOT become black
 * stops. Remaining colors are masked to 0xRRGGBB.
 *
 * @param {Array} rawColors - Array of packed colors (0xAARRGGBB) from engine
 * @returns {Array} compacted array of 0xRRGGBB stops in original order
 */
RGBUtil.buildGradientColors = function(rawColors) {
    var out = [];
    if (!rawColors) return out;
    for (var i = 0; i < rawColors.length; i++) {
        var c = rawColors[i];
        if (c === 0) continue;
        out.push(c & 0xFFFFFF);
    }
    return out;
};

/**
 * Linearly interpolate the gradient defined by `colors` (evenly spaced stops
 * between 0 and 1) at position `t` in [0, 1].
 *
 * @param {Array} colors - Packed 0xRRGGBB stops, evenly spaced
 * @param {number} t     - Position 0..1 (clamped)
 * @returns {number} interpolated 0xRRGGBB
 */
RGBUtil.gradientColorAt = function(colors, t) {
    if (!colors || colors.length === 0) return 0;
    if (colors.length === 1) return colors[0] & 0xFFFFFF;
    if (t < 0) t = 0;
    else if (t > 1) t = 1;
    var pos = t * (colors.length - 1);
    var idx = Math.floor(pos);
    if (idx >= colors.length - 1) return colors[colors.length - 1] & 0xFFFFFF;
    var frac = pos - idx;
    var c1 = colors[idx], c2 = colors[idx + 1];
    var r1 = (c1 >> 16) & 0xFF, g1 = (c1 >> 8) & 0xFF, b1 = c1 & 0xFF;
    var r2 = (c2 >> 16) & 0xFF, g2 = (c2 >> 8) & 0xFF, b2 = c2 & 0xFF;
    var r = Math.round(r1 + frac * (r2 - r1));
    var g = Math.round(g1 + frac * (g2 - g1));
    var b = Math.round(b1 + frac * (b2 - b1));
    return (r << 16) | (g << 8) | b;
};

/**
 * Pre-sample a gradient into `n` evenly spaced colors (LUT). Useful to avoid
 * per-pixel interpolation cost in inner rgbMap loops.
 *
 * @param {Array} colors - Packed 0xRRGGBB stops
 * @param {number} n     - Output sample count
 * @returns {Array} array of n packed 0xRRGGBB colors
 */
RGBUtil.gradientLut = function(colors, n) {
    if (n <= 0) return [];
    if (n === 1) return [RGBUtil.gradientColorAt(colors, 0.5)];
    var lut = new Array(n);
    var denom = n - 1;
    for (var i = 0; i < n; i++)
        lut[i] = RGBUtil.gradientColorAt(colors, i / denom);
    return lut;
};

/**
 * Clamp x into [0, 1].
 */
RGBUtil.clamp01 = function(x) {
    return x < 0 ? 0 : (x > 1 ? 1 : x);
};

/**
 * Wrap x into [0, 1) (positive modulo 1).
 */
RGBUtil.mod1 = function(x) {
    var m = x - Math.floor(x);
    return m < 0 ? m + 1 : m;
};

/**
 * Triangle wave with period 1 and range [0, 1]. f(0)=0, f(0.5)=1, f(1)=0.
 */
RGBUtil.triangle = function(x) {
    return 1 - Math.abs(2 * RGBUtil.mod1(x) - 1);
};

/**
 * Sine wave normalized to [0, 1] with period 1.
 */
RGBUtil.sin01 = function(x) {
    return 0.5 + 0.5 * Math.sin(2 * Math.PI * x);
};

/**
 * HSV to RGB conversion returning a packed 0xRRGGBB integer.
 * @param {number} h - Hue (0-1, wraps)
 * @param {number} s - Saturation (0-1)
 * @param {number} v - Value (0-1)
 * @returns {number} packed 0xRRGGBB
 */
RGBUtil.hsvToRgb = function(h, s, v) {
    var rgb = RGBUtil.hsv2rgb(h, s, v);
    return RGBUtil.rgb(rgb[0], rgb[1], rgb[2]);
};

/**
 * Pre-built 256-entry rainbow gradient LUT. Each entry is [r, g, b] in 0-255.
 */
RGBUtil.RAINBOW_LUT = (function() {
    var lut = new Array(256);
    for (var i = 0; i < 256; i++) {
        var h = i / 256;
        var sector = Math.floor(h * 6);
        var f = h * 6 - sector;
        var r, g, b;
        switch (sector % 6) {
            case 0: r=255; g=f*255; b=0; break;
            case 1: r=(1-f)*255; g=255; b=0; break;
            case 2: r=0; g=255; b=f*255; break;
            case 3: r=0; g=(1-f)*255; b=255; break;
            case 4: r=f*255; g=0; b=255; break;
            case 5: r=255; g=0; b=(1-f)*255; break;
        }
        lut[i] = [r, g, b];
    }
    return lut;
})();

/**
 * LedFX-style HSV: gradient LUT lookup, then s/v applied in RGB domain.
 * - h wraps to [0,1) and indexes the rainbow LUT.
 * - s desaturates toward the max channel: pixel += (max - pixel) * (1 - s).
 * - v multiplies brightness; final clamp happens at RGBUtil.rgb().
 * s/v are NOT pre-clamped; out-of-range values clip naturally at rgb().
 */
RGBUtil.hsvLedFx = function(h, s, v) {
    var idx = Math.floor(((h % 1 + 1) % 1) * 255.999);
    var c = RGBUtil.RAINBOW_LUT[idx];
    var r = c[0], g = c[1], b = c[2];
    var m = Math.max(r, g, b);
    var oneMinusS = 1 - s;
    r = (r + (m - r) * oneMinusS) * v / 255;
    g = (g + (m - g) * oneMinusS) * v / 255;
    b = (b + (m - b) * oneMinusS) * v / 255;
    return RGBUtil.rgb(r * 255, g * 255, b * 255);
};

/**
 * Additively blend two packed 0xRRGGBB colors with per-channel clamping at 255.
 */
RGBUtil.blendAdd = function(a, b) {
    var r = ((a >> 16) & 0xFF) + ((b >> 16) & 0xFF);
    var g = ((a >> 8) & 0xFF) + ((b >> 8) & 0xFF);
    var bl = (a & 0xFF) + (b & 0xFF);
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (bl > 255) bl = 255;
    return (r << 16) | (g << 8) | bl;
};

/**
 * Scale a packed 0xRRGGBB color by a factor in [0, 1] (clamped).
 */
RGBUtil.scaleColor = function(c, factor) {
    if (factor <= 0) return 0;
    if (factor > 1) factor = 1;
    var r = Math.round(((c >> 16) & 0xFF) * factor);
    var g = Math.round(((c >> 8) & 0xFF) * factor);
    var b = Math.round((c & 0xFF) * factor);
    return (r << 16) | (g << 8) | b;
};


/**
 * LedFX-compatible sawtooth 0→1.
 * Loops every 65.536/modifier seconds (65536/modifier ms).
 * modifier ≈ cycles per 65.536 seconds; higher = faster.
 * @param {number} modifier - Speed multiplier
 * @param {number} timestepMs - Accumulated time in milliseconds
 * @returns {number} 0.0 to 1.0 sawtooth
 */
RGBUtil.time01 = function(modifier, timestepMs) {
    if (modifier <= 0 || !isFinite(modifier) || !isFinite(timestepMs)) return 0;
    var period = 65536.0 / modifier;
    return (timestepMs % period) / period;
};

/**
 * BPM-locked sawtooth 0→1.
 *   speed = 1.0  → 1 cycle per beat (quarter note)
 *   speed = 2.0  → 2 cycles per beat (8th note)
 *   speed = 0.25 → 1 cycle per bar (whole note)
 * Falls back to 120 BPM when bpm <= 0 (no audio).
 * @param {number} speed  Cycles per beat (>0; 0/NaN→1)
 * @param {Object} state  Persistent accumulator { phase: 0 }, owned by caller
 * @param {number} bpm    Tempo in beats/min (0/NaN→120 fallback)
 * @param {number} dtMs   Frame delta in ms (may be negative for direction flip)
 * @returns {number} sawtooth in [0, 1)
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

/** Same as beatTime but returns 0→2π for use with Math.sin(). */
RGBUtil.beatAngle = function(speed, state, bpm, dtMs) {
    return RGBUtil.beatTime(speed, state, bpm, dtMs) * 2 * Math.PI;
};

/**
 * BPM-locked continuous accumulator (no wrap). For noise field offsets
 * and angles where phase wrapping causes visible discontinuities.
 * @param {number} speed  Cycles per beat
 * @param {Object} state  Persistent { position: 0 }, owned by caller
 * @param {number} bpm    Tempo (0→120 fallback)
 * @param {number} dtMs   Frame delta in ms
 * @returns {number} cumulative position (unbounded)
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


/*
 * 2D Simplex noise (public domain, Stefan Gustavson).
 * Returns value in range -1 to 1.
 */
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
 *
 * @param {number} width
 * @param {number} height
 * @param {number} freq    - Spatial frequency (cells per axis)
 * @param {number} offsetX - Noise X offset (e.g. animated over time)
 * @param {number} offsetY - Noise Y offset
 * @returns {Array} 2D array indexed as field[y][x] with values in 0-1
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
            field[y][x] = (n + 1) * 0.5; // normalize to 0-1
        }
    }
    return field;
};
