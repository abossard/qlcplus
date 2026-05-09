/*
  Q Light Controller Plus
  audio_common.js

  Shared helpers for audio-reactive RGB scripts.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

/*
 * QLC+ Audio API v3 — Available fields on the `audio` object:
 *
 * BANK POWERS (LedFx-parity scalars, 0..~1, mean of each mel bank's processed[]):
 *   audio.lows   - low bank power (kick/bass region)
 *   audio.mids   - mid bank power (vocals/snare region)
 *   audio.highs  - high bank power (hats/cymbals region)
 *
 * MULTI-RESOLUTION MEL (from aubio, 3 banks x N bands each):
 *   audio.melLow[]          - 20-350 Hz (kick detail)
 *   audio.melMid[]          - 20-2000 Hz (vocal/snare)
 *   audio.melHigh[]         - 20-15000 Hz (hats/cymbals)
 *   audio.melRaw[0..39]     - legacy 40-band raw mel
 *   audio.mel[0..39]        - legacy 40-band processed mel
 *   audio.melNovelty[0..39] - change detection
 *   audio.melRanges.{low,mid,high}.{minHz,maxHz,bands} - per-bank metadata
 *
 * TRIGGERS (from C++ AudioChannel, Schmitt hysteresis) — 6 keys total:
 *   audio.triggers.{low, mid, high, volume, beat, kick}
 *     .value / .active / .firedThisFrame / .releasedThisFrame / .heldMs / .cooldownRemainingMs
 *
 * ONSETS (from aubio, 9 methods):
 *   audio.onsets.{energy,hfc,complex,phase,wphase,specdiff,kl,mkl,specflux}  - boolean
 *   audio.onsets.descriptors[0..8]            - continuous magnitude (HOW HARD)
 *   audio.onsets.thresholdedDescriptors[0..8] - after adaptive whitening
 *
 * MUSIC (from aubio tempo/beat):
 *   audio.music.beat / .bpm / .beatPhase (0..1) / .barPhase (0..beatsPerBar) / .beatConfidence / .tatum
 *
 * PITCH (from aubio):
 *   audio.pitch.hz / .confidence
 *
 * NOTES (from aubio):
 *   audio.note.midi / .velocity / .noteOn / .noteOff
 *
 * FEATURES (from C++ AudioAnalyzer + aubio):
 *   audio.features.rmsDb / .peakDb / .crestFactor / .centroidHz / .spread / .rolloffHz / .flatness / .flux / .hfc
 *
 * VOLUME:
 *   audio.volume.raw / .smoothed / .normalized
 *
 * TIMING:
 *   audio.audioDtMs / audio.consumerDtMs
 *
 * GRADIENT (auto-injected from RGBMatrix colors):
 *   algo.gradientColors[]         - compact valid colors
 *   algo.gradientBandColors[0..2] - 3 colors for low/mid/high
 *   audio.bandColors[0..2]        - same, on audio object
 */

// All DSP (gain, filter smoothing, AGC, hysteresis triggers, band power) is now
// performed in the C++ AudioChannel pipeline and exposed on the per-frame audio
// snapshot. AudioParams below only carries small per-script tuning values plus
// reusable helpers to read the audio snapshot in a graceful, defensive way
// (every accessor falls back to a safe default when its source field is
// missing, so scripts keep working even if features are disabled).

var AudioParams = {
    installContinuous: function(algo, defaults) {
        var d = defaults || {};
        algo.presetReactivity = d.reactivity || 5;
        algo.presetFloor = d.floor || 0;
    },

    installTrigger: function(algo, defaults) {
        var d = defaults || {};
        algo.presetReactivity = d.reactivity || 5;
        algo.presetSensitivity = d.sensitivity || 5;
    },

    filterRise: function(algo) { return 0.1 + algo.presetReactivity * 0.09; },

    applyFloor: function(algo, brightness) {
        var f = algo.presetFloor / 100.0;
        return f + (1 - f) * brightness;
    },

    triggerThreshold: function(algo) { return 0.45 - algo.presetSensitivity * 0.04; },

    // 3 default colors: low (warm) / mid (neutral) / high (cool).
    defaultBandColors: [0xFF4000, 0x00FF64, 0x4080FF],

    // Deprecated: 5-band per-script power sliders are gone — bank balance is
    // configured globally in the AudioProfile mel-bank section. Kept as a
    // no-op so older scripts that still call it keep loading.
    installBandPowerControls: function(_algo) { /* no-op (v3) */ },

    // Returns [low, mid, high] bank powers from the per-frame snapshot.
    // Defensive: missing scalars degrade gracefully to 0.
    bandPowers: function(audio, _algo) {
        if (!audio) return [0, 0, 0];
        return [
            (typeof audio.lows  === "number") ? audio.lows  : 0,
            (typeof audio.mids  === "number") ? audio.mids  : 0,
            (typeof audio.highs === "number") ? audio.highs : 0
        ];
    },

    bandWeights: function(algo, audio) {
        return AudioParams.bandPowers(audio, algo);
    },

    bandScaleForColumn: function(_algo, x, width) {
        // Columns map evenly across low/mid/high — no per-band sliders in v3.
        Math.min(2, Math.floor((x / Math.max(1, width)) * 3));
        return 1.0;
    },

    bandColors: function(algo, fallback) {
        if (algo.gradientBandColors && algo.gradientBandColors.length >= 3)
            return algo.gradientBandColors;
        return fallback || AudioParams.defaultBandColors;
    },

    colorChannels: function(packed) {
        return [(packed >> 16) & 0xFF, (packed >> 8) & 0xFF, packed & 0xFF];
    },

    centroidWarmCool: function(audio) {
        var centroid = (audio && audio.features && typeof audio.features.centroidHz === "number")
            ? audio.features.centroidHz
            : 1000;
        return Math.max(0, Math.min(1, (centroid - 200) / 3800));
    },

    temperatureBiasedBandWeights: function(algo, audio) {
        var weights = AudioParams.bandWeights(algo, audio);
        var warmCool = AudioParams.centroidWarmCool(audio);
        // [0]=low (warm), [1]=mid (neutral, untouched), [2]=high (cool)
        weights[0] *= (1 - warmCool);
        weights[2] *= warmCool;
        return weights;
    },

    blendBandColors: function(algo, audio, fallback) {
        var colors = AudioParams.bandColors(algo, fallback);
        var weights = AudioParams.temperatureBiasedBandWeights(algo, audio);
        var totalWeight = 0;
        var r = 0, g = 0, b = 0;

        for (var i = 0; i < 3; i++) {
            var w = weights[i];
            var packed = colors[i] | 0;
            totalWeight += w;
            r += ((packed >> 16) & 0xFF) * w;
            g += ((packed >> 8) & 0xFF) * w;
            b += (packed & 0xFF) * w;
        }

        if (totalWeight < 0.001) return 0;
        return RGBUtil.rgb(Math.round(r / totalWeight), Math.round(g / totalWeight), Math.round(b / totalWeight));
    },

    dominantBandColor: function(algo, audio, fallback) {
        var colors = AudioParams.bandColors(algo, fallback);
        var maxIdx = AudioParams.dominantBandIndex(algo, audio);
        return colors[maxIdx] | 0;
    },

    dominantBandIndex: function(algo, audio) {
        var weights = AudioParams.bandWeights(algo, audio);
        var maxIdx = 0;
        var maxVal = weights[0];

        for (var i = 1; i < 3; i++) {
            if (weights[i] > maxVal) {
                maxVal = weights[i];
                maxIdx = i;
            }
        }

        return maxIdx;
    },

    anyOnsetFired: function(audio) {
        if (!audio || !audio.onsets) return false;
        var o = audio.onsets;
        return !!(o.energy || o.hfc || o.complex || o.phase || o.wphase ||
                  o.specdiff || o.kl || o.mkl || o.specflux);
    },

    triggerModeFired: function(algo, audio) {
        if (!audio) return false;
        if (algo.presetTriggerMode === 2)
            return !!(audio.note && audio.note.noteOn);
        if (algo.presetTriggerMode === 1)
            return AudioParams.anyOnsetFired(audio);
        return !!(audio.triggers && audio.triggers.beat && audio.triggers.beat.firedThisFrame);
    },

    punchFactor: function(audio) {
        var flux = (audio && audio.features && typeof audio.features.flux === "number")
            ? audio.features.flux
            : 0;
        return Math.max(1, Math.min(3, 1.0 + flux * 2.0));
    },

    applyPunch: function(brightness, audio) {
        return Math.min(1, brightness * AudioParams.punchFactor(audio));
    },

    // ---- v2 helpers: graceful access to optional audio snapshot fields ----

    // Returns audio.triggers.kick.firedThisFrame, with low as fallback when
    // the dedicated kick channel is not available.
    kickFired: function(audio) {
        if (!audio || !audio.triggers) return false;
        var t = audio.triggers;
        if (t.kick && typeof t.kick.firedThisFrame === "boolean")
            return t.kick.firedThisFrame;
        return !!(t.low && t.low.firedThisFrame);
    },

    kickActive: function(audio) {
        if (!audio || !audio.triggers) return false;
        var t = audio.triggers;
        if (t.kick && typeof t.kick.active === "boolean")
            return t.kick.active;
        return !!(t.low && t.low.active);
    },

    // Phase within current beat, 0..1. Returns 0 if unavailable.
    beatPhase: function(audio) {
        if (audio && audio.music && typeof audio.music.beatPhase === "number")
            return audio.music.beatPhase;
        return 0;
    },

    // Phase within current 4-beat bar, 0..1. Returns 0 if unavailable.
    barPhase: function(audio) {
        if (audio && audio.music && typeof audio.music.barPhase === "number")
            return audio.music.barPhase;
        return 0;
    },

    // 0..1 brightness pulse synced to the beat (peaks at start of beat).
    beatPulse: function(audio) {
        var p = AudioParams.beatPhase(audio);
        // Cosine envelope: 1.0 on the beat, drops to ~0 by mid-beat.
        return Math.max(0, Math.cos(p * Math.PI));
    },

    // True at the start of a bar (beat 0, within first quarter of that beat).
    isDownbeat: function(audio) {
        var p = AudioParams.barPhase(audio);
        return Math.floor(p) === 0 && (p - Math.floor(p)) < 0.25;
    },

    // Largest onset descriptor magnitude (0..~1 typical, can exceed 1).
    // Use to scale flash intensity proportional to onset strength.
    maxOnsetIntensity: function(audio) {
        if (!audio || !audio.onsets) return 0;
        var src = audio.onsets.thresholdedDescriptors || audio.onsets.descriptors;
        if (!src || src.length === 0) return 0;
        var m = 0;
        for (var i = 0; i < src.length; i++) {
            var v = src[i] || 0;
            if (v > m) m = v;
        }
        return m;
    },

    // Returns the requested multi-mel band ('low'|'mid'|'high') if available,
    // otherwise falls back to audio.mel. Always returns an array.
    multiMel: function(audio, band) {
        if (!audio) return [];
        if (band === "low" && audio.melLow && audio.melLow.length) return audio.melLow;
        if (band === "mid" && audio.melMid && audio.melMid.length) return audio.melMid;
        if (band === "high" && audio.melHigh && audio.melHigh.length) return audio.melHigh;
        return audio.mel || [];
    },

    // Concatenated low+mid+high mel bands (72 values) for high-resolution
    // spectrum displays. Falls back to audio.mel when multi-mel is disabled.
    fullMel: function(audio) {
        if (!audio) return [];
        if (audio.melLow && audio.melMid && audio.melHigh &&
            audio.melLow.length && audio.melMid.length && audio.melHigh.length) {
            return audio.melLow.concat(audio.melMid, audio.melHigh);
        }
        return audio.mel || [];
    },

    // Average mel novelty across all bands, 0..~1. 0 if unavailable.
    melNoveltyAvg: function(audio) {
        if (!audio || !audio.melNovelty || !audio.melNovelty.length) return 0;
        var s = 0;
        for (var i = 0; i < audio.melNovelty.length; i++)
            s += audio.melNovelty[i] || 0;
        return s / audio.melNovelty.length;
    },

    // Maximum mel novelty across all bands.
    melNoveltyMax: function(audio) {
        if (!audio || !audio.melNovelty || !audio.melNovelty.length) return 0;
        var m = 0;
        for (var i = 0; i < audio.melNovelty.length; i++) {
            var v = audio.melNovelty[i] || 0;
            if (v > m) m = v;
        }
        return m;
    },

    // Spectral flux (0..~1). Useful for buildup detection.
    flux: function(audio) {
        if (audio && audio.features && typeof audio.features.flux === "number")
            return audio.features.flux;
        return 0;
    },

    // Pitch in Hz, or 0 when unavailable / low confidence.
    pitchHz: function(audio, minConfidence) {
        if (!audio || !audio.pitch) return 0;
        var conf = (typeof audio.pitch.confidence === "number") ? audio.pitch.confidence : 1;
        if (conf < (minConfidence || 0.5)) return 0;
        return audio.pitch.hz || 0;
    }
};
