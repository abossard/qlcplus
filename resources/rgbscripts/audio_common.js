/*
  Q Light Controller Plus
  audio_common.js

  Shared helpers for audio-reactive RGB scripts.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

var AudioParams = {
    installContinuous: function(algo, defaults) {
        var d = defaults || {};
        algo.presetGain = d.gain || 5;
        algo.presetReactivity = d.reactivity || 5;
        algo.presetFloor = d.floor || 0;

        algo.properties.push("name:presetGain|type:range|display:Gain|values:1,10|write:setGain|read:getGain");
        algo.properties.push("name:presetReactivity|type:range|display:Reactivity|values:1,10|write:setReactivity|read:getReactivity");
        algo.properties.push("name:presetFloor|type:range|display:Floor|values:0,100|write:setFloor|read:getFloor");

        algo.setGain = function(v) { algo.presetGain = Math.max(1, Math.min(10, parseInt(v))); };
        algo.getGain = function() { return algo.presetGain; };
        algo.setReactivity = function(v) { algo.presetReactivity = Math.max(1, Math.min(10, parseInt(v))); };
        algo.getReactivity = function() { return algo.presetReactivity; };
        algo.setFloor = function(v) { algo.presetFloor = Math.max(0, Math.min(100, parseInt(v))); };
        algo.getFloor = function() { return algo.presetFloor; };
    },

    installTrigger: function(algo, defaults) {
        var d = defaults || {};
        algo.presetGain = d.gain || 5;
        algo.presetReactivity = d.reactivity || 5;
        algo.presetSensitivity = d.sensitivity || 5;

        algo.properties.push("name:presetGain|type:range|display:Gain|values:1,10|write:setGain|read:getGain");
        algo.properties.push("name:presetReactivity|type:range|display:Reactivity|values:1,10|write:setReactivity|read:getReactivity");
        algo.properties.push("name:presetSensitivity|type:range|display:Sensitivity|values:1,10|write:setSensitivity|read:getSensitivity");

        algo.setGain = function(v) { algo.presetGain = Math.max(1, Math.min(10, parseInt(v))); };
        algo.getGain = function() { return algo.presetGain; };
        algo.setReactivity = function(v) { algo.presetReactivity = Math.max(1, Math.min(10, parseInt(v))); };
        algo.getReactivity = function() { return algo.presetReactivity; };
        algo.setSensitivity = function(v) { algo.presetSensitivity = Math.max(1, Math.min(10, parseInt(v))); };
        algo.getSensitivity = function() { return algo.presetSensitivity; };
    },

    gainFactor: function(algo) { return 0.6 + algo.presetGain * 0.2; },
    filterRise: function(algo) { return 0.1 + algo.presetReactivity * 0.09; },
    applyFloor: function(algo, brightness) {
        var f = algo.presetFloor / 100.0;
        return f + (1 - f) * brightness;
    },
    triggerThreshold: function(algo) { return 0.45 - algo.presetSensitivity * 0.04; },

    createFilter: function(algo, baseDecay) {
        return new LedFx.ExpFilter(baseDecay, AudioParams.filterRise(algo));
    },

    /**
     * Redistribute a raw FFT-style spectrum into numBands log-spaced bands
     * over the 40-5000Hz range, matching the formula used by the
     * AudioCapture C++ engine. Each output band is the average of the
     * input bins whose centre frequencies fall inside it.
     *
     * Use when a script needs more or fewer bands than the engine's
     * default 16, without losing log spacing.
     *
     * @param {Array}  spectrum - Source spectrum (already log-spaced or
     *                            raw — values are averaged proportionally).
     * @param {number} numBands - Desired number of output bands.
     * @returns {Array} numBands-length array of normalized values.
     */
    logScaleBands: function(spectrum, numBands) {
        numBands = Math.floor(Number(numBands) || 0);
        if (!spectrum || spectrum.length === 0 || numBands <= 0) {
            var empty = new Array(numBands);
            for (var i = 0; i < numBands; i++) empty[i] = 0;
            return empty;
        }
        if (spectrum.length === numBands) return spectrum.slice();

        var srcLen = spectrum.length;
        var out = new Array(numBands);

        for (var b = 0; b < numBands; b++) {
            var startFrac = (b / numBands) * srcLen;
            var endFrac = ((b + 1) / numBands) * srcLen;

            // Proper fractional overlap weighting
            var sum = 0;
            var weight = 0;
            var firstBin = Math.floor(startFrac);
            var lastBin = Math.min(srcLen - 1, Math.floor(endFrac - 1e-9));
            if (lastBin < firstBin) lastBin = firstBin;

            for (var i = firstBin; i <= lastBin; i++) {
                var lo = (i < startFrac) ? startFrac : i;
                var hi = (i + 1 > endFrac) ? endFrac : i + 1;
                var w = hi - lo;
                sum += spectrum[i] * w;
                weight += w;
            }
            out[b] = weight > 0 ? sum / weight : 0;
        }
        return out;
    },

    /**
     * Adaptive gain control. Tracks the recent peak of the input on
     * algo._agcPeak and scales value by 1/peak so loud and quiet songs
     * both reach the top of the 0-1 range.
     *
     * Peak rises instantly to new maxima and decays slowly so brief
     * silences don't blow up the gain.
     *
     * @param {object} algo  - The script's algo object (used for state).
     * @param {number} value - Input value in 0-1 space.
     * @returns {number} Auto-gained value in 0-1 space.
     */
    adaptiveGain: function(algo, value) {
        if (typeof algo._agcPeak !== "number" || isNaN(algo._agcPeak))
            algo._agcPeak = 0.1;
        if (value > algo._agcPeak)
            algo._agcPeak = value;
        else
            algo._agcPeak = algo._agcPeak * 0.995 + value * 0.005;

        // Floor the peak so silence doesn't divide by ~0.
        var peak = algo._agcPeak < 0.05 ? 0.05 : algo._agcPeak;
        var out = value / peak;
        if (out < 0) out = 0;
        if (out > 1) out = 1;
        return out;
    },

    /**
     * Soft-saturate a 0-1 value above 'knee' using a tanh shoulder.
     * Below the knee the signal is unchanged; above it values are
     * compressed smoothly toward 1, preventing harsh visual peaks.
     *
     * @param {number} value - Input in 0-1 space.
     * @param {number} knee  - Threshold in 0-1 (e.g. 0.7).
     * @returns {number} Soft-clipped value in 0-1.
     */
    softSaturate: function(value, knee) {
        if (knee >= 1) return value < 0 ? 0 : (value > 1 ? 1 : value);
        if (knee < 0) knee = 0;
        if (value <= knee) return value < 0 ? 0 : value;
        var headroom = 1 - knee;
        var excess = (value - knee) / headroom;
        return knee + headroom * Math.tanh(excess);
    },

    /**
     * Schmitt-trigger style hysteresis. Returns true once value rises
     * above onThreshold and stays true until value falls below
     * offThreshold. Prevents rapid on/off flicker around a single
     * threshold.
     *
     * State is stored on algo as algo['_hyst_' + key].
     *
     * @param {object} algo          - The script's algo object.
     * @param {string} key           - Unique state key (e.g. "kick").
     * @param {number} value         - Current value in 0-1 space.
     * @param {number} onThreshold   - Rising-edge threshold.
     * @param {number} offThreshold  - Falling-edge threshold (< onThreshold).
     * @returns {boolean} Current trigger state.
     */
    hysteresisTrigger: function(algo, key, value, onThreshold, offThreshold) {
        var stateKey = "_hyst_" + key;
        var on = algo[stateKey] === true;
        if (!on && value >= onThreshold) on = true;
        else if (on && value <= offThreshold) on = false;
        algo[stateKey] = on;
        return on;
    },

    /**
     * Frame-rate normalize an exponential-decay constant. Decay
     * constants tuned at QLC+'s default 25fps tick rate become too
     * fast or too slow if the actual frame rate differs (e.g.
     * 30fps preview, 50fps fast tick). This rescales the per-frame
     * decay so the perceived time-constant stays the same.
     *
     *     normalized = decay ^ (25 / fps)
     *
     * Example: a 0.9 decay at 25fps becomes ~0.917 at 30fps.
     *
     * @param {number} decay - Per-frame decay coefficient at 25fps (0-1).
     * @param {number} fps   - Actual frame rate.
     * @returns {number} Decay coefficient adjusted for fps.
     */
    frameNormalizedDecay: function(decay, fps) {
        if (!fps || fps <= 0) return decay;
        if (decay <= 0) return 0;
        if (decay >= 1) return 1;
        return Math.pow(decay, 25.0 / fps);
    }
};
