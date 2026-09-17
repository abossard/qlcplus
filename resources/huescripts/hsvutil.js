/*
  Q Light Controller Plus
  hsvutil.js

  Shared HSV helpers for QLC+ scripts. The engine consumes only HSV
  Float32Array maps; this file exposes no RGB packing helpers.

  Pixel map contract (rgbscriptv4.cpp):
    - rgbMap() must return a Float32Array of length width*height*3
    - Each pixel is 3 interleaved floats: [h, s, v] in [0,1]
    - Engine converts HSV -> packed RGB before writing to RGBMap

  Color contract:
    - algo.colors = [{h,s,v}, ...]  (user-picked palette, length = acceptColors)

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

var HSVUtil = {};

/**
 * Allocate an HSV pixel map. Returns a Float32Array of length width*height*3
 * (row-major, 3 floats per pixel: H, S, V).
 */
HSVUtil.createMap = function(width, height) {
    return new Float32Array(width * height * 3);
};

/**
 * Set an HSV pixel in a Float32Array map.
 * Inline `var i=(y*w+x)*3; map[i]=h; map[i+1]=s; map[i+2]=v;` is faster in
 * tight inner loops; prefer this helper outside hot paths.
 */
HSVUtil.setPixel = function(map, width, x, y, h, s, v) {
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
HSVUtil.gradientAt = function(stops, t) {
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

HSVUtil.hsvToRgb = function(h, s, v) {
    h = HSVUtil.mod1(h);
    s = HSVUtil.clamp01(s);
    // Keep intermediate overdrive until output brightness and final clipping.
    v = Math.max(0, v);
    var i = Math.floor(h * 6);
    var f = h * 6 - i;
    var p = v * (1 - s);
    var q = v * (1 - f * s);
    var r = v * (1 - (1 - f) * s);
    switch (i % 6) {
        case 0: return [v, r, p];
        case 1: return [q, v, p];
        case 2: return [p, v, r];
        case 3: return [p, q, v];
        case 4: return [r, p, v];
        default: return [v, p, q];
    }
};

HSVUtil.rgbToHsv = function(r, g, b) {
    r = HSVUtil.clamp01(r);
    g = HSVUtil.clamp01(g);
    b = HSVUtil.clamp01(b);
    var max = Math.max(r, g, b);
    var min = Math.min(r, g, b);
    var delta = max - min;
    var h = 0;
    if (delta > 0) {
        if (max === r) h = ((g - b) / delta) / 6;
        else if (max === g) h = (2 + (b - r) / delta) / 6;
        else h = (4 + (r - g) / delta) / 6;
    }
    return {
        h: HSVUtil.mod1(h),
        s: max > 0 ? delta / max : 0,
        v: max
    };
};

HSVUtil.rgbToHsvUnclipped = function(r, g, b) {
    var scale = Math.max(1, r, g, b);
    var hsv = HSVUtil.rgbToHsv(r / scale, g / scale, b / scale);
    hsv.v *= scale;
    return hsv;
};

HSVUtil.parsePositions = function(csv, count) {
    if (!csv || typeof csv !== "string") return null;
    var values = csv.split(",").map(function(token) {
        return parseFloat(String(token).trim());
    });
    if (values.length !== count) return null;
    for (var i = 0; i < values.length; i++) {
        if (!isFinite(values[i]) || values[i] < 0 || values[i] > 1) return null;
        if (i > 0 && values[i] < values[i - 1]) return null;
    }
    return values;
};

HSVUtil.gradientRgbAt = function(stops, t, positionsCsv) {
    if (!stops || stops.length === 0) return [0, 0, 0];
    if (stops.length === 1) return HSVUtil.hsvToRgb(stops[0].h, stops[0].s, stops[0].v);
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    var positions = HSVUtil.parsePositions(positionsCsv, stops.length);
    if (!positions) {
        positions = new Array(stops.length);
        for (var i = 0; i < stops.length; i++)
            positions[i] = stops.length <= 1 ? 0 : i / (stops.length - 1);
    }
    var first = stops[0];
    var last = stops[stops.length - 1];
    if (positions[0] > 0) {
        stops = [{h: first.h, s: first.s, v: first.v}].concat(stops);
        positions = [0].concat(positions);
    }
    if (positions[positions.length - 1] < 1) {
        stops = stops.concat([{h: last.h, s: last.s, v: last.v}]);
        positions = positions.concat([1]);
    }
    var segment = 0;
    while (segment + 1 < positions.length && t > positions[segment + 1]) segment++;
    var leftPos = positions[segment];
    var rightPos = positions[Math.min(segment + 1, positions.length - 1)];
    var localT = rightPos > leftPos ? (t - leftPos) / (rightPos - leftPos) : 0;
    localT = HSVUtil.clamp01(localT);
    var slope = 1.5;
    var powT = Math.pow(localT, slope);
    var invPowT = Math.pow(1 - localT, slope);
    var eased = powT + invPowT > 0 ? powT / (powT + invPowT) : localT;
    var a = HSVUtil.hsvToRgb(stops[segment].h, stops[segment].s, stops[segment].v);
    var b = HSVUtil.hsvToRgb(stops[Math.min(segment + 1, stops.length - 1)].h,
                             stops[Math.min(segment + 1, stops.length - 1)].s,
                             stops[Math.min(segment + 1, stops.length - 1)].v);
    return [
        a[0] + (b[0] - a[0]) * eased,
        a[1] + (b[1] - a[1]) * eased,
        a[2] + (b[2] - a[2]) * eased
    ];
};

HSVUtil.gradientLedfxAt = function(stops, t, positionsCsv) {
    var rgb = HSVUtil.gradientRgbAt(stops, t, positionsCsv);
    return HSVUtil.rgbToHsv(rgb[0], rgb[1], rgb[2]);
};

HSVUtil.parseHexRgb = function(hex) {
    if (typeof hex !== "string" || !/^#[0-9a-f]{6}$/i.test(hex))
        return [0, 0, 0];
    return [
        parseInt(hex.substr(1, 2), 16) / 255,
        parseInt(hex.substr(3, 2), 16) / 255,
        parseInt(hex.substr(5, 2), 16) / 255
    ];
};

HSVUtil.attachStripTransformControls = function(algo, options) {
    options = options || {};
    var prefix = options.prefix || "";
    var display = options.display || "";
    var definitions = [
        ["flip", "Flip", "list", "Off,On", "Off"],
        ["mirror", "Mirror", "list", "Off,On", "Off"],
        ["backgroundMode", "Background Mode", "list", "Off,Additive", "Off"],
        ["backgroundColor", "Background Color", "string", "", "#000000"],
        ["backgroundBrightness", "Background Brightness", "float", "0,1", 1],
        ["brightness", "Brightness", "float", "0,1", 1],
        ["blur", "Blur", "float", "0,10", 0]
    ];
    var readers = {};
    function normalize(key, value) {
        if (key === "flip" || key === "mirror") {
            if (value === true || value === "On" || value === "Yes" || value === "true")
                return "On";
            if (value === false || value === "Off" || value === "No" || value === "false")
                return "Off";
        } else if (key === "backgroundMode") {
            if (value === "Off" || value === "Additive") return value;
        } else if (key === "backgroundColor") {
            if (typeof value === "string" && /^#[0-9a-f]{6}$/i.test(value)) return value;
        } else {
            if (value !== null && String(value).trim() !== "" && isFinite(Number(value)))
                return Math.max(0, Math.min(key === "blur" ? 10 : 1, Number(value)));
        }
        throw new Error("Invalid " + key + " output control: " + value);
    }
    definitions.forEach(function(definition) {
        var key = definition[0];
        var name = prefix ? prefix + key.charAt(0).toUpperCase() + key.slice(1) : key;
        var method = name.charAt(0).toUpperCase() + name.slice(1);
        var write = "set" + method;
        var read = "get" + method;
        var existing = algo.properties.filter(function(property) {
            return property.split("|")[0] === "name:" + name;
        })[0];
        if (existing) {
            existing.split("|").forEach(function(field) {
                if (field.indexOf("read:") === 0) read = field.slice(5);
            });
            if (typeof algo[read] !== "function")
                throw new Error("Missing reader for output control " + name);
        } else {
            if (algo[write] || algo[read])
                throw new Error("Output control method collision for " + name);
            var value = normalize(key, options.defaults && options.defaults[key] !== undefined
                ? options.defaults[key] : definition[4]);
            algo[write] = function(next) { value = normalize(key, next); };
            algo[read] = function() { return value; };
            algo.properties.push("name:" + name + "|type:" + definition[2] +
                "|display:" + display + definition[1] +
                (definition[3] ? "|values:" + definition[3] : "") +
                "|write:" + write + "|read:" + read);
        }
        readers[key] = function() { return normalize(key, algo[read]()); };
    });
    return function() {
        var result = {};
        definitions.forEach(function(definition) {
            result[definition[0]] = readers[definition[0]]();
        });
        return result;
    };
};

HSVUtil.applyStripTransforms = function(map, width, height, opts) {
    opts = opts || {};
    var n = width * height;
    if (n <= 0) return map;
    var brightness = parseFloat(opts.brightness);
    if (!isFinite(brightness)) brightness = 1;
    brightness = HSVUtil.clamp01(brightness);
    var sigma = Math.max(0, Math.min(10, parseFloat(opts.blur) || 0));
    var identity = opts.flip !== "On" && opts.mirror !== "On" &&
        opts.backgroundMode !== "Additive" && brightness === 1 && sigma === 0;
    if (identity) {
        for (var i = 0; i < map.length; i++) {
            if (!isFinite(map[i]) || map[i] < 0 || map[i] > 1) {
                identity = false;
                break;
            }
        }
        if (identity) return map;
    }
    var pixels = new Array(n);
    for (var i = 0; i < n; i++) {
        var o = i * 3;
        pixels[i] = HSVUtil.hsvToRgb(map[o], map[o + 1], map[o + 2]);
    }
    if (opts.flip === "On") pixels.reverse();
    if (opts.mirror === "On") {
        var mirrored = new Array(n * 2);
        for (var i = 0; i < n; i++) {
            mirrored[i] = pixels[n - 1 - i];
            mirrored[n + i] = pixels[i];
        }
        var folded = new Array(n);
        for (var i = 0; i < n; i++) {
            var a = mirrored[i * 2];
            var b = mirrored[i * 2 + 1];
            folded[i] = [
                Math.max(a[0], b[0]),
                Math.max(a[1], b[1]),
                Math.max(a[2], b[2])
            ];
        }
        pixels = folded;
    }
    if (opts.backgroundMode === "Additive") {
        var bg = HSVUtil.parseHexRgb(opts.backgroundColor || "#000000");
        var bgScale = parseFloat(opts.backgroundBrightness);
        if (!isFinite(bgScale)) bgScale = 1;
        bgScale = HSVUtil.clamp01(bgScale);
        for (var i = 0; i < n; i++) {
            pixels[i][0] += bg[0] * bgScale;
            pixels[i][1] += bg[1] * bgScale;
            pixels[i][2] += bg[2] * bgScale;
        }
    }
    for (var i = 0; i < n; i++) {
        pixels[i][0] *= brightness;
        pixels[i][1] *= brightness;
        pixels[i][2] *= brightness;
    }
    if (sigma > 0 && n > 3) {
        var radius = Math.max(1, Math.min(Math.floor((n - 1) / 2), Math.round(4 * sigma)));
        var kernel = new Array(radius * 2 + 1);
        var sum = 0;
        for (var d = -radius; d <= radius; d++) {
            var w = Math.exp(-d * d / (2 * sigma * sigma));
            kernel[d + radius] = w;
            sum += w;
        }
        for (var k = 0; k < kernel.length; k++) kernel[k] /= sum;
        var blurred = new Array(n);
        for (var i = 0; i < n; i++) {
            var rgb = [0, 0, 0];
            for (var d = -radius; d <= radius; d++) {
                var j = i + d;
                if (j < 0 || j >= n) continue;
                var w = kernel[d + radius];
                rgb[0] += pixels[j][0] * w;
                rgb[1] += pixels[j][1] * w;
                rgb[2] += pixels[j][2] * w;
            }
            blurred[i] = rgb;
        }
        pixels = blurred;
    }
    for (var i = 0; i < n; i++) {
        var hsv = HSVUtil.rgbToHsv(pixels[i][0], pixels[i][1], pixels[i][2]);
        var o = i * 3;
        map[o] = hsv.h;
        map[o + 1] = hsv.s;
        map[o + 2] = hsv.v;
    }
    return map;
};

/**
 * Resample an array to a new size using linear interpolation.
 * Matches numpy.interp behaviour over an evenly spaced grid.
 */
HSVUtil.interpolate = function(arr, size) {
    if (arr.length === 0) return new Array(size).fill(0);
    if (arr.length === size) return arr.slice();
    if (arr.length === 1) return new Array(size).fill(arr[0]);
    if (size === 1) return [arr[0]];

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
HSVUtil.clamp01 = function(x) {
    return x < 0 ? 0 : (x > 1 ? 1 : x);
};

/** Wrap x into [0, 1) (positive modulo 1). */
HSVUtil.mod1 = function(x) {
    var m = x - Math.floor(x);
    return m < 0 ? m + 1 : m;
};

/** Triangle wave, period 1, range [0, 1]. f(0)=0, f(0.5)=1, f(1)=0. */
HSVUtil.triangle = function(x) {
    return 1 - Math.abs(2 * HSVUtil.mod1(x) - 1);
};

/** Sine wave normalized to [0, 1] with period 1. */
HSVUtil.sin01 = function(x) {
    return 0.5 + 0.5 * Math.sin(2 * Math.PI * x);
};


/***********************************************************************
 * Time / beat helpers
 ***********************************************************************/

/**
 * LedFX-compatible sawtooth 0->1.
 * Loops every 65.536/modifier seconds (65536/modifier ms).
 */
HSVUtil.time01 = function(modifier, timestepMs) {
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
HSVUtil.beatTime = function(speed, state, bpm, dtMs) {
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
HSVUtil.beatAngle = function(speed, state, bpm, dtMs) {
    return HSVUtil.beatTime(speed, state, bpm, dtMs) * 2 * Math.PI;
};

/**
 * BPM-locked continuous accumulator (no wrap). For noise field offsets
 * and angles where phase wrapping causes visible discontinuities.
 */
HSVUtil.beatPosition = function(speed, state, bpm, dtMs) {
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
HSVUtil._grad3 = [[1,1,0],[-1,1,0],[1,-1,0],[-1,-1,0],[1,0,1],[-1,0,1],[1,0,-1],[-1,0,-1],[0,1,1],[0,-1,1],[0,1,-1],[0,-1,-1]];
HSVUtil._perm = (function() {
    var p = [151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,190,6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,88,237,149,56,87,174,20,125,136,171,168,68,175,74,165,71,134,139,48,27,166,77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,102,143,54,65,25,63,161,1,216,80,73,209,76,132,187,208,89,18,169,200,196,135,130,116,188,159,86,164,100,109,198,173,186,3,64,52,217,226,250,124,123,5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,223,183,170,213,119,248,152,2,44,154,163,70,221,153,101,155,167,43,172,9,129,22,39,253,19,98,108,110,79,113,224,232,178,185,112,104,218,246,97,228,251,34,242,193,238,210,144,12,191,179,162,241,81,51,145,235,249,14,239,107,49,192,214,31,181,199,106,157,184,84,204,176,115,121,50,45,127,4,150,254,138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180];
    var perm = new Array(512);
    for (var i = 0; i < 512; i++) perm[i] = p[i & 255];
    return perm;
})();

HSVUtil.simplex2d = function(xin, yin) {
    var F2 = 0.5 * (Math.sqrt(3) - 1);
    var G2 = (3 - Math.sqrt(3)) / 6;
    var perm = HSVUtil._perm;
    var grad3 = HSVUtil._grad3;

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
 * 3D simplex noise. Range approximately -1..1.
 */
HSVUtil.simplex3d = function(xin, yin, zin) {
    var F3 = 1 / 3;
    var G3 = 1 / 6;
    var perm = HSVUtil._perm;
    var grad3 = HSVUtil._grad3;

    var s = (xin + yin + zin) * F3;
    var i = Math.floor(xin + s);
    var j = Math.floor(yin + s);
    var k = Math.floor(zin + s);
    var t = (i + j + k) * G3;
    var X0 = i - t;
    var Y0 = j - t;
    var Z0 = k - t;
    var x0 = xin - X0;
    var y0 = yin - Y0;
    var z0 = zin - Z0;

    var i1, j1, k1, i2, j2, k2;
    if (x0 >= y0) {
        if (y0 >= z0) { i1 = 1; j1 = 0; k1 = 0; i2 = 1; j2 = 1; k2 = 0; }
        else if (x0 >= z0) { i1 = 1; j1 = 0; k1 = 0; i2 = 1; j2 = 0; k2 = 1; }
        else { i1 = 0; j1 = 0; k1 = 1; i2 = 1; j2 = 0; k2 = 1; }
    } else {
        if (y0 < z0) { i1 = 0; j1 = 0; k1 = 1; i2 = 0; j2 = 1; k2 = 1; }
        else if (x0 < z0) { i1 = 0; j1 = 1; k1 = 0; i2 = 0; j2 = 1; k2 = 1; }
        else { i1 = 0; j1 = 1; k1 = 0; i2 = 1; j2 = 1; k2 = 0; }
    }

    var x1 = x0 - i1 + G3;
    var y1 = y0 - j1 + G3;
    var z1 = z0 - k1 + G3;
    var x2 = x0 - i2 + 2 * G3;
    var y2 = y0 - j2 + 2 * G3;
    var z2 = z0 - k2 + 2 * G3;
    var x3 = x0 - 1 + 3 * G3;
    var y3 = y0 - 1 + 3 * G3;
    var z3 = z0 - 1 + 3 * G3;

    var ii = i & 255;
    var jj = j & 255;
    var kk = k & 255;
    var gi0 = perm[ii + perm[jj + perm[kk]]] % 12;
    var gi1 = perm[ii + i1 + perm[jj + j1 + perm[kk + k1]]] % 12;
    var gi2 = perm[ii + i2 + perm[jj + j2 + perm[kk + k2]]] % 12;
    var gi3 = perm[ii + 1 + perm[jj + 1 + perm[kk + 1]]] % 12;

    var n0 = 0, n1 = 0, n2 = 0, n3 = 0;
    var t0 = 0.6 - x0 * x0 - y0 * y0 - z0 * z0;
    if (t0 > 0) { t0 *= t0; n0 = t0 * t0 * (grad3[gi0][0] * x0 + grad3[gi0][1] * y0 + grad3[gi0][2] * z0); }
    var t1 = 0.6 - x1 * x1 - y1 * y1 - z1 * z1;
    if (t1 > 0) { t1 *= t1; n1 = t1 * t1 * (grad3[gi1][0] * x1 + grad3[gi1][1] * y1 + grad3[gi1][2] * z1); }
    var t2 = 0.6 - x2 * x2 - y2 * y2 - z2 * z2;
    if (t2 > 0) { t2 *= t2; n2 = t2 * t2 * (grad3[gi2][0] * x2 + grad3[gi2][1] * y2 + grad3[gi2][2] * z2); }
    var t3 = 0.6 - x3 * x3 - y3 * y3 - z3 * z3;
    if (t3 > 0) { t3 *= t3; n3 = t3 * t3 * (grad3[gi3][0] * x3 + grad3[gi3][1] * y3 + grad3[gi3][2] * z3); }
    return 32 * (n0 + n1 + n2 + n3);
};

HSVUtil.audioSeconds = function(audio) {
    if (audio && audio.timing && isFinite(audio.timing.deltaSeconds))
        return Math.max(0, audio.timing.deltaSeconds);
    if (audio && isFinite(audio.dt) && isFinite(audio.bpm) && audio.bpm > 0)
        return Math.max(0, audio.dt * 60 / audio.bpm);
    return 0;
};

HSVUtil.audioIdentityKey = function(audio, previous) {
    audio = audio || {};
    var profile = String(audio.profileId === undefined ? "legacy" : audio.profileId);
    var source = audio.sourceId || "";
    if (previous && audio.available === false && (!source || audio.status === "reset") &&
        previous.indexOf(profile + "|") === 0)
        return previous;
    return profile + "|" + (audio.sourceEpoch || 0) + "|" +
        (audio.configRevision || 0) + "|" + source;
};

HSVUtil.powerForRange = function(audio, range, useRaw) {
    if (!audio) return 0;
    var raw = useRaw && audio.powers ? (audio.powers.raw || {}) : {};
    var beat = HSVUtil.clamp01(useRaw && isFinite(raw.beat) ? raw.beat : (audio.beat || 0));
    var bass = HSVUtil.clamp01(useRaw && isFinite(raw.bass) ? raw.bass : (audio.bass || 0));
    var low = HSVUtil.clamp01(useRaw && isFinite(raw.low) ? raw.low : (audio.low || 0));
    var mid = HSVUtil.clamp01(useRaw && isFinite(raw.mid) ? raw.mid : (audio.mid || 0));
    var high = HSVUtil.clamp01(useRaw && isFinite(raw.high) ? raw.high : (audio.high || 0));
    if (range === "Beat") return beat;
    if (range === "Bass") return bass;
    if (range === "Mids") return mid;
    if (range === "High") return high;
    if (range === "Lows") return low;
    return HSVUtil.clamp01((beat + bass) * 0.5);
};

HSVUtil.aggressiveTopEndBias = function(x, boost) {
    x = HSVUtil.clamp01(x);
    boost = HSVUtil.clamp01(boost);
    var aggressive = 1 - Math.pow(1 - x, 4);
    return (1 - boost) * x + boost * aggressive;
};

/**
 * Generate a 2D noise field (height x width), values 0-1.
 * Returns a 2D array indexed as field[y][x].
 */
HSVUtil.noiseField2d = function(width, height, freq, offsetX, offsetY) {
    var field = new Array(height);
    for (var y = 0; y < height; y++) {
        field[y] = new Array(width);
        for (var x = 0; x < width; x++) {
            var n = HSVUtil.simplex2d(
                (x / Math.max(1, width - 1)) * freq + offsetX,
                (y / Math.max(1, height - 1)) * freq + offsetY
            );
            field[y][x] = (n + 1) * 0.5;
        }
    }
    return field;
};
