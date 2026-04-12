/*
  Q Light Controller Plus
  ledfx_compat.js

  LedFX compatibility shim for audio-reactive RGB scripts.
  Provides ExpFilter, melbank interpolation, and frequency band
  power helpers matching the LedFX Python API.

  Ported from LedFX (MIT License)
  Original: https://github.com/LedFx/LedFx

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

var LedFx = {};

/**
 * ExpFilter — Exact port of LedFX's exponential smoothing filter.
 *
 * Usage:
 *   var filter = new LedFx.ExpFilter(0.5, 0.9);
 *   var smoothed = filter.update(newValue);
 *
 * For array smoothing:
 *   var filter = new LedFx.ExpFilter(0.5, 0.9);
 *   var smoothed = filter.updateArray(newArray);
 *
 * @param {number} alpha_decay - Smoothing factor when value decreases (0-1)
 * @param {number} alpha_rise  - Smoothing factor when value increases (0-1)
 */
LedFx.ExpFilter = function(alpha_decay, alpha_rise) {
    this.alpha_decay = alpha_decay;
    this.alpha_rise = alpha_rise;
    this.value = null;
};

LedFx.ExpFilter.prototype.update = function(newValue) {
    if (this.value === null) {
        this.value = newValue;
        return this.value;
    }
    var alpha = (newValue > this.value) ? this.alpha_rise : this.alpha_decay;
    this.value = alpha * newValue + (1.0 - alpha) * this.value;
    return this.value;
};

LedFx.ExpFilter.prototype.updateArray = function(newArray) {
    if (this.value === null || this.value.length !== newArray.length) {
        this.value = new Array(newArray.length);
        for (var i = 0; i < newArray.length; i++)
            this.value[i] = newArray[i];
        return this.value;
    }
    for (var i = 0; i < newArray.length; i++) {
        var alpha = (newArray[i] > this.value[i]) ? this.alpha_rise : this.alpha_decay;
        this.value[i] = alpha * newArray[i] + (1.0 - alpha) * this.value[i];
    }
    return this.value;
};

/**
 * Interpolate an array to a new size using linear interpolation.
 * Matches LedFX's numpy.interp behavior.
 *
 * @param {Array} arr  - Source array
 * @param {number} size - Target size
 * @returns {Array} Interpolated array of length 'size'
 */
LedFx.interpolate = function(arr, size) {
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
 * Get the melbank (frequency spectrum) interpolated to the requested size.
 * Mirrors LedFX's AudioReactiveEffect.melbank(size=N) method.
 *
 * @param {object} audioData - The audioData object passed as 5th arg to rgbMap
 * @param {number} size      - Number of output frequency bins
 * @returns {Array} Array of normalized values (0.0-1.0)
 */
LedFx.melbank = function(audioData, size) {
    if (!audioData || !audioData.spectrum) return new Array(size).fill(0);
    var spectrum = [];
    var len = audioData.spectrum.length;
    for (var i = 0; i < len; i++)
        spectrum.push(audioData.spectrum[i]);
    if (size === 0 || size === len) return spectrum;
    return LedFx.interpolate(spectrum, size);
};

/**
 * Split the spectrum into thirds (lows, mids, highs).
 * Mirrors LedFX's melbank_thirds().
 *
 * @param {object} audioData - The audioData object
 * @returns {object} { lows: Array, mids: Array, highs: Array }
 */
LedFx.melbank_thirds = function(audioData) {
    if (!audioData || !audioData.spectrum) {
        return { lows: [], mids: [], highs: [] };
    }
    var s = [];
    var len = audioData.spectrum.length;
    for (var i = 0; i < len; i++) s.push(audioData.spectrum[i]);

    var third = Math.floor(len / 3);
    return {
        lows: s.slice(0, third),
        mids: s.slice(third, third * 2),
        highs: s.slice(third * 2)
    };
};

/**
 * Average of an array.
 */
LedFx.avg = function(arr) {
    if (!arr || arr.length === 0) return 0;
    var sum = 0;
    for (var i = 0; i < arr.length; i++) sum += arr[i];
    return sum / arr.length;
};

/**
 * Get low-frequency power (0.0-1.0).
 * Mirrors LedFX's data.lows_power().
 */
LedFx.lows_power = function(audioData) {
    var thirds = LedFx.melbank_thirds(audioData);
    return LedFx.avg(thirds.lows);
};

/**
 * Get mid-frequency power (0.0-1.0).
 * Mirrors LedFX's data.mids_power().
 */
LedFx.mids_power = function(audioData) {
    var thirds = LedFx.melbank_thirds(audioData);
    return LedFx.avg(thirds.mids);
};

/**
 * Get high-frequency power (0.0-1.0).
 * Mirrors LedFX's data.high_power().
 */
LedFx.high_power = function(audioData) {
    var thirds = LedFx.melbank_thirds(audioData);
    return LedFx.avg(thirds.highs);
};

/**
 * Beat oscillator: returns 0.0-1.0 position within the current beat.
 * Approximated from BPM.
 */
LedFx.beat_oscillator = function(audioData, elapsedMs) {
    var bpm = (audioData && audioData.bpm) ? audioData.bpm : 120;
    var beatMs = 60000.0 / bpm;
    return (elapsedMs % beatMs) / beatMs;
};

/**
 * Bar oscillator: returns 0.0-4.0 position within a 4-beat bar.
 */
LedFx.bar_oscillator = function(audioData, elapsedMs) {
    var bpm = (audioData && audioData.bpm) ? audioData.bpm : 120;
    var barMs = (60000.0 / bpm) * 4;
    return ((elapsedMs % barMs) / barMs) * 4.0;
};

/**
 * HSV to RGB conversion.
 * @param {number} h - Hue (0-1)
 * @param {number} s - Saturation (0-1)
 * @param {number} v - Value (0-1)
 * @returns {Array} [r, g, b] each 0-255
 */
LedFx.hsv2rgb = function(h, s, v) {
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
 * Pack RGB values into a single uint for QLC+ RGBMap.
 */
LedFx.rgb = function(r, g, b) {
    r = Math.max(0, Math.min(255, Math.round(r)));
    g = Math.max(0, Math.min(255, Math.round(g)));
    b = Math.max(0, Math.min(255, Math.round(b)));
    return (r << 16) | (g << 8) | b;
};

/**
 * Create an empty 2D map (height x width) filled with 0.
 */
LedFx.createMap = function(width, height) {
    var map = new Array(height);
    for (var y = 0; y < height; y++) {
        map[y] = new Array(width);
        for (var x = 0; x < width; x++)
            map[y][x] = 0;
    }
    return map;
};
