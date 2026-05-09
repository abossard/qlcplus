/*
  Q Light Controller Plus
  audio_colors.js — color helpers for audio-reactive RGB scripts.
  All DSP comes from C++; this file only blends colors using the gradient.
*/

var CENTROID_WARM_HZ = 200;
var CENTROID_COOL_HZ = 4000;

var AudioColors = {
    DEFAULT_BANDS: [0xFF4000, 0x00FF64, 0x4080FF],

    bands: function(algo) {
        return (algo.gradientBandColors && algo.gradientBandColors.length >= 3)
            ? algo.gradientBandColors : AudioColors.DEFAULT_BANDS;
    },

    blendByPower: function(algo, audio) {
        var colors  = AudioColors.bands(algo);
        var weights = audio.power.bands.slice();
        var warmCool = Math.max(0, Math.min(1,
            (audio.features.centroidHz - CENTROID_WARM_HZ) / (CENTROID_COOL_HZ - CENTROID_WARM_HZ)));
        weights[0] *= (1 - warmCool);
        weights[2] *= warmCool;

        var r = 0, g = 0, b = 0, w = 0;
        for (var i = 0; i < 3; i++) {
            var c = colors[i] | 0;
            r += ((c >> 16) & 0xFF) * weights[i];
            g += ((c >> 8) & 0xFF) * weights[i];
            b += (c & 0xFF) * weights[i];
            w += weights[i];
        }
        return w < 0.001 ? 0 :
            RGBUtil.rgb(Math.round(r/w), Math.round(g/w), Math.round(b/w));
    },

    dominant: function(algo, audio) {
        return AudioColors.bands(algo)[
            audio.power.dominant === "high" ? 2 :
            audio.power.dominant === "mid"  ? 1 : 0
        ];
    },

    fluxPunch: function(audio) {
        return Math.max(1, Math.min(3, 1 + audio.features.flux * 2));
    },

    noveltyBoost: function(audio) {
        return 1.0 + 0.30 * audio.spectrum.novelty.mean;
    }
};
