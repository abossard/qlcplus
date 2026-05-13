/*
  Q Light Controller Plus
  audio_colors.js — color helpers for audio-reactive RGB scripts.
  All DSP comes from C++; this file only picks/tints colors using the gradient.

  All colors are {h, s, v} objects (hue 0-1, saturation 0-1, value 0-1).
*/

var AudioColors = {
    DEFAULT_BANDS: [
        {h: 0.042, s: 1.0, v: 1.0},
        {h: 0.399, s: 1.0, v: 1.0},
        {h: 0.611, s: 0.749, v: 1.0}
    ],

    bands: function(algo) {
        return (algo.colors && algo.colors.length >= 3)
            ? algo.colors : AudioColors.DEFAULT_BANDS;
    },

    /**
     * Pick the dominant band's HSV color, tinted slightly toward the runner-up.
     * Hue interpolated via shortest arc. Returns {h, s, v}.
     */
    blendByPower: function(algo, audio) {
        var colors = AudioColors.bands(algo);
        var powers = audio.power.bands;

        var HYSTERESIS = 0.15;
        if (algo._prevDomIdx === undefined) algo._prevDomIdx = 0;
        var prevIdx = algo._prevDomIdx;
        var prevPower = powers[prevIdx] || 0;

        var maxIdx = 0, maxPower = powers[0] || 0;
        for (var i = 1; i < 3; i++) {
            if ((powers[i] || 0) > maxPower) {
                maxPower = powers[i];
                maxIdx = i;
            }
        }

        var domIdx = (maxIdx !== prevIdx && maxPower > prevPower * (1 + HYSTERESIS))
            ? maxIdx : prevIdx;
        algo._prevDomIdx = domIdx;

        var dom = colors[domIdx];
        var domPower = powers[domIdx] || 0;
        if (domPower < 0.001) return {h: dom.h, s: dom.s, v: dom.v};

        var runIdx = -1, runPower = -1;
        for (var j = 0; j < 3; j++) {
            if (j !== domIdx && (powers[j] || 0) > runPower) {
                runPower = powers[j];
                runIdx = j;
            }
        }

        if (runIdx < 0) return {h: dom.h, s: dom.s, v: dom.v};

        var tint = Math.min(0.3, runPower / Math.max(0.001, domPower) * 0.3);
        return AudioColors.blend(dom, colors[runIdx], tint);
    },

    /**
     * Blend two {h,s,v} colors. t=0 returns a, t=1 returns b.
     * Hue interpolated via shortest arc.
     */
    blend: function(a, b, t) {
        var s1 = 1 - t;
        var dh = b.h - a.h;
        if (dh > 0.5) dh -= 1;
        else if (dh < -0.5) dh += 1;
        var h = a.h + t * dh;
        h = h - Math.floor(h);
        return {
            h: h,
            s: a.s * s1 + b.s * t,
            v: a.v * s1 + b.v * t
        };
    },

    dominant: function(algo, audio) {
        return AudioColors.bands(algo)[AudioColors.dominantIndex(audio)];
    },

    fluxPunch: function(audio) {
        return Math.max(1, Math.min(3, 1 + audio.features.flux * 2));
    },

    noveltyBoost: function(audio) {
        return 1.0 + 0.30 * audio.spectrum.novelty.mean;
    }
};

AudioColors.dominantIndex = function(audio) {
    if (!audio || !audio.power) return 0;
    return audio.power.dominant === "high" ? 2 : (audio.power.dominant === "mid" ? 1 : 0);
};

AudioColors.dominantColor = function(algo, audio, fallback, threshold) {
    if (!audio || !audio.power) return fallback;
    var minPower = threshold === undefined ? 0.05 : threshold;
    if (minPower > 0 && audio.power.dominantValue < minPower)
        return fallback;
    var bands = AudioColors.bands(algo);
    var idx = AudioColors.dominantIndex(audio);
    return bands[idx] !== undefined ? bands[idx] : fallback;
};
