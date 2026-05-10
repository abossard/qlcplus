/*
  Q Light Controller Plus
  audio_colors.js — color helpers for audio-reactive RGB scripts.
  All DSP comes from C++; this file only picks/tints colors using the gradient.
*/

var AudioColors = {
    DEFAULT_BANDS: [0xFF4000, 0x00FF64, 0x4080FF],

    bands: function(algo) {
        return (algo.gradientBandColors && algo.gradientBandColors.length >= 3)
            ? algo.gradientBandColors : AudioColors.DEFAULT_BANDS;
    },

    /**
     * Pick the dominant band's color, tinted slightly toward the runner-up.
     * Never averages in RGB (which washes to white). The dominant band's
     * hue always wins; secondary bands add subtle tint at most.
     * Hysteresis prevents flickering when bands are close in power.
     * State stored on algo object (per-script, not shared).
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

        var domColor = colors[domIdx] | 0;
        var domPower = powers[domIdx] || 0;
        if (domPower < 0.001) return domColor;

        var runIdx = -1, runPower = -1;
        for (var j = 0; j < 3; j++) {
            if (j !== domIdx && (powers[j] || 0) > runPower) {
                runPower = powers[j];
                runIdx = j;
            }
        }

        if (runIdx < 0) return domColor;

        // Tint: max 30% from runner-up to keep saturation
        var tint = Math.min(0.3, runPower / Math.max(0.001, domPower) * 0.3);
        var runColor = colors[runIdx] | 0;

        var r = ((domColor >> 16) & 0xFF) * (1 - tint) + ((runColor >> 16) & 0xFF) * tint;
        var g = ((domColor >> 8) & 0xFF) * (1 - tint) + ((runColor >> 8) & 0xFF) * tint;
        var b = (domColor & 0xFF) * (1 - tint) + (runColor & 0xFF) * tint;
        return RGBUtil.rgb(Math.round(r), Math.round(g), Math.round(b));
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

AudioColors.blendPacked = function(a, b, t) {
    var s = 1 - t;
    return RGBUtil.rgb(
        ((a >> 16) & 0xFF) * s + ((b >> 16) & 0xFF) * t,
        ((a >> 8) & 0xFF) * s + ((b >> 8) & 0xFF) * t,
        (a & 0xFF) * s + (b & 0xFF) * t
    );
};
