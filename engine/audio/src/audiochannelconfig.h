/*
  Q Light Controller Plus
  audiochannelconfig.h

  Copyright (c) Massimo Callegari

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#pragma once

#include <QString>
#include <cstdint>

// Single source of truth for the maximum band count of any one multi-resolution
// mel bank. Mirrored as aliases by aubioresults.h (AUBIO_MELBANK_MAX),
// audiosnapshot.h (kMelBankBandsMax) and MelBankConfig::kMaxBandsPerBank below
// so existing call sites compile unchanged.
static constexpr int kMaxMelBands = 32;

// matt_mel master mel bank frequency range. Used by both AubioProcessor (when
// building the 40-band master filterbank for melScale == "matt_mel") and
// audiochannel.cpp (hzToMasterMelBin analytical mapping). LedFx melbank.py:
// 264-287 — matt_mel warp: 3700 * log12(1 + f/230). Spans 20 Hz .. 15 kHz.
static constexpr double kMattMelMinHz = 20.0;
static constexpr double kMattMelMaxHz = 15000.0;

struct EnvelopeConfig
{
    // In-class defaults are the single source of truth. AudioChannelConfig::
    // defaults() simply returns AudioChannelConfig{}. Values picked: 15ms
    // attack / 150ms release matches the multi-mel + kick docs.
    double attackMs = 15.0;
    double releaseMs = 150.0;
};

struct TriggerConfig
{
    double highThreshold = 0.65;
    double lowThreshold = 0.45;
    double holdMs = 80.0;
    double cooldownMs = 120.0;
};

struct NoiseGateConfig
{
    // In-class defaults are the single source of truth (see EnvelopeConfig).
    double thresholdDb = -60.0;
    double holdMs = 120.0;
};

struct KickConfig
{
    double beatMaxHz = 100.0;        // LedFx audio.py:1107-1112 (freq_max_mels[0])
    double beatMinPercentDiff = 0.5; // LedFx audio.py:1196
    double beatMinAmplitude = 0.5;   // LedFx audio.py:1198 (compared to np.max of slice)
    double beatRefractorySec = 0.1;  // LedFx audio.py:1197 (beat_min_time_since)
    int beatHistoryLen = 10;         // LedFx audio.py:1199 (beat_power_history_len; LedFx uses sample_rate*0.2)

    bool enabled = true;
};

/**
 * Multi-resolution mel filterbank configuration. Three nested matt_mel
 * banks share the existing FFT (m_fftGrain) and run after the legacy 40-band
 * filterbank inside AubioProcessor. Each bank has its own independent
 * MelPostProcessor instance in AudioChannel so AGC/smoothing/novelty state is
 * per-bank (matches LedFx semantics).
 *
 * Defaults are EDM-tuned (low=20-350Hz, mid=20-2kHz, high=20-15kHz, 24 bands
 * each). All ranges start at 20 Hz so bass effects can always read from the
 * `low` bank. The 3 banks are the only frequency decomposition exposed to
 * scripts and the VC widget — there is no enable toggle, AubioProcessor
 * always builds them.
 */
struct MelBankConfig
{
    static constexpr int kMaxBandsPerBank = kMaxMelBands;

    struct Bank
    {
        double minHz = 20.0;
        double maxHz = 350.0;
        int bands = 24;

        bool operator==(const Bank &o) const
        {
            return minHz == o.minHz && maxHz == o.maxHz && bands == o.bands;
        }
        bool operator!=(const Bank &o) const { return !(*this == o); }
    };

    Bank low  = { 20.0,   350.0, 24 };
    Bank mid  = { 20.0,  2000.0, 24 };
    Bank high = { 20.0, 15000.0, 24 };

    // Tracks which preset the bank ranges came from for the VC properties UI
    // (round-tripped through XML, not consumed by the DSP). Set to "Custom"
    // by callers that mutate any minHz/maxHz/bands directly.
    QString preset = QStringLiteral("EDM");

    bool operator==(const MelBankConfig &o) const
    {
        // `preset` is intentionally NOT part of equality: it's a UI label,
        // not a DSP input, so a preset rename must NOT trigger a filterbank
        // rebuild in AubioProcessor::needsFullRebuild().
        return low == o.low && mid == o.mid && high == o.high;
    }
    bool operator!=(const MelBankConfig &o) const { return !(*this == o); }
};

/**
 * LedFx-style mel post-processing config (mirrors MelPostProcessor::Config).
 * Defaults to bypass so existing projects/scripts see unchanged mel values.
 */
struct MelPostConfig
{
    double powerFactor = 2.0;       // LedFx default: tan(0.5*pi*(0.4+1)/2) ≈ 2.0
    double gaussianSigma = 1.0;    // LedFx: fast_blur_array sigma=1.0 for mel_gain
    double smoothDecay = 0.7;       // LedFx melbank.py:376
    double smoothRise = 0.99;       // LedFx melbank.py:376
    double commonDecay = 0.99;      // LedFx melbank.py:377
    double commonRise = 0.01;       // LedFx melbank.py:377
    double diffDecay = 0.15;        // LedFx melbank.py:378
    double diffRise = 0.99;         // LedFx melbank.py:378
    bool enabled = true;            // on by default (LedFx always processes)
};

/**
 * Per-onset-method override. Sentinel values (negative / large negative) mean
 * "use aubio_onset_set_default_parameters() default for this method". Real
 * values are forwarded directly to the matching aubio_onset_set_*() call.
 * Kept POD so it round-trips through XML cleanly.
 */
struct OnsetMethodOverride
{
    double threshold = -1.0;       // sentinel: < 0 => keep aubio default
    double silenceDb = -999.0;     // sentinel: < -900 => keep aubio default
    double minioiMs = -1.0;        // sentinel: < 0 => keep aubio default
    double delayMs = -9999.0;      // sentinel: < -9000 => keep aubio default
    double compression = -1.0;     // sentinel: < 0 => keep aubio default
    int awhitening = -1;           // sentinel: < 0 => keep default; 0=off 1=on
};

/**
 * Direct aubio configuration — every field maps 1:1 to an aubio_*_set_*() call
 * inside AubioProcessor. No QLC+ post-processing is applied to these values,
 * Samples flow from AudioCapture into aubio after int16->float conversion.
 * Optional pre-emphasis is the only PCM pre-processing stage; input gain is
 * an OS / hardware concern and is NOT applied here.
 */
struct AubioConfig
{
    // Mel filterbank — aubio_filterbank_set_norm / aubio_filterbank_set_power.
    // norm: 1 = each mel filter normalized to unit area (default), 0 = raw triangular weights.
    // power: input |X|^power before filtering. 1 = magnitude, 2 = power (energy).
    // Changing either requires rebuilding the filterbank (norm must be set before
    // set_mel_coeffs_slaney).
    double filterbankNorm = 1.0;
    double filterbankPower = 1.0;

    // Onset detection — global threshold/silence/minioi/delay overrides have
    // been removed: aubio_onset_set_default_parameters() applies per-method
    // tuned values for all four, and a single global override would destroy
    // that tuning. We let aubio own these entirely.

    // Pitch detection — aubio_pitch_*.
    QString pitchMethod = QStringLiteral("yinfft"); // yin, yinfft, yinfast, schmitt, fcomb, mcomb
    QString pitchUnit = QStringLiteral("Hz");       // Hz, midi, cent, bin (aubio_pitch_set_unit)
    double pitchSilenceDb = -40.0;     // aubio_pitch_set_silence
    double pitchTolerance = 0.7;       // aubio_pitch_set_tolerance

    // Tempo / beat — aubio_tempo_*.
    QString tempoMethod = QStringLiteral("default");
    double tempoSilenceDb = -90.0;     // aubio_tempo_set_silence
    double tempoThreshold = 0.3;       // aubio_tempo_set_threshold (peak-picking)
    int tatumSubdivision = 4;          // aubio_tempo_set_tatum_signature
    int beatsPerBar = 4;               // bar oscillator length (1..8; 4 = common time)
    bool preEmphasisEnabled = true;     // LedFx-style high-frequency pre-emphasis before pvoc

    // Transient / Steady Separation — aubio_tss_*.
    double tssAlpha = 3.0;             // aubio_tss_set_alpha
    double tssBeta = 3.0;              // aubio_tss_set_beta
    double tssThreshold = 0.25;        // aubio_tss_set_threshold

    // Phase vocoder window type — aubio_pvoc_set_window().
    // Accepted: "default", "rectangle", "hamming", "hanning", "hanningz",
    // "blackman", "blackman_harris", "gaussian", "welch", "parzen". Changing
    // requires a full rebuild (window is baked into the pvoc internals).
    QString windowType = QStringLiteral("default");

    // Mel filterbank scale for the 40-band master mel bank. Accepted values:
    //   "matt_mel" — LedFx-style triangle bands over [kMattMelMinHz, min(kMattMelMaxHz, sr/2)]
    //                (default; matches the 3 nested matt_mel sub-banks below)
    //   "htk"      — aubio_filterbank_set_mel_coeffs_htk() over [0, sr/2]
    //   "slaney"   — aubio_filterbank_set_mel_coeffs_slaney() (legacy aubio default)
    // Changing requires a full rebuild. hzToMasterMelBin() in audiochannel.cpp
    // branches on this value so the freq_mel_indexes lookup matches the bank
    // actually built here.
    QString melScale = QStringLiteral("matt_mel");

    // Onset — compression and adaptive whitening intentionally NOT exposed
    // globally: aubio_onset_set_default_parameters() applies the per-method
    // tuned defaults (e.g., specflux: compression=10 + whitening on; hfc:
    // compression=1; energy/phase: both off). Use onsetOverrides[i] for
    // per-method tuning that bypasses the defaults only where requested.
    // Per-method enable. Indices match kOnsetMethods[]:
    // 0=energy, 1=hfc, 2=complex, 3=phase, 4=wphase,
    // 5=specdiff, 6=kl, 7=mkl, 8=specflux. Toggling triggers a targeted
    // create/destroy of the affected onset detector(s) only.
    bool onsetMethodEnabled[9] = { true, true, true, true, true, true, true, true, true };

    // Primary onset method for audio.onset.fired/intensity.
    // 0=energy 1=hfc 2=complex 3=phase 4=wphase 5=specdiff 6=kl 7=mkl 8=specflux
    // Default: specflux (best general-purpose, pending validation vs hfc).
    int onsetMethodIndex = 8;

    // Per-method tuning overrides (sentinel = aubio default). Applied AFTER
    // aubio_onset_set_default_parameters() in initialize() / on enable.
    OnsetMethodOverride onsetOverrides[9];

    // Tempo — aubio_tempo_set_delay_ms.
    double tempoDelayMs = 0.0;

    // Tempo decay on silence — when no beats are detected for coastBeats
    // beat-periods, BPM decays exponentially toward tempoDecayTargetBpm.
    // coastBeats: how many beat-periods of silence before decay starts (default 2)
    // tempoDecayHalfLifeBeats: BPM halves every N beats during decay (default 1.5)
    // tempoDecayTargetBpm: floor BPM to decay toward (min 1; 0 is not safe for consumers)
    double coastBeats = 4.0;
    double tempoDecayHalfLifeBeats = 0.5;
    double tempoDecayTargetBpm = 1.0;

    // Note detection — aubio_notes_set_*.
    double noteSilenceDb = -70.0;
    double noteMinIntervalMs = 30.0;
    double noteReleaseDropDb = 10.0;

    // MFCC — aubio_mfcc_set_power / set_scale.
    double mfccPower = 1.0;
    double mfccScale = 1.0;

    // Multi-resolution mel filterbanks (matt_mel triangle bands). Owned here
    // so AubioConfig stays the single source of truth handed to
    // AubioProcessor each pass. Any change to a bank's minHz/maxHz/bands
    // triggers a full rebuild (coefficient matrix is baked at init time).
    // The 3 banks are always built; there is no enable toggle.
    MelBankConfig melBanks;
};

struct AudioChannelConfig
{
    EnvelopeConfig envelope;
    TriggerConfig triggers;
    NoiseGateConfig noiseGate;
    KickConfig kick;
    MelPostConfig melPost;
    double brightnessFloor = 0.0;
    double volumeSmoothingMs = 100.0;
    double freqPowerDecay = 0.2;     // LedFx audio.py:1160 (alpha_decay)
    double freqPowerRise = 0.97;     // LedFx audio.py:1160 (alpha_rise)
    AubioConfig aubio;

    static AudioChannelConfig defaults();
    static AudioChannelConfig fromLegacySliders(int gain, int reactivity, int floor, int sensitivity);
};

/**
 * Read aubio's per-method default onset parameters (threshold/silence/minioi/
 * delay/compression/awhitening) by spinning up a throwaway aubio_onset_t,
 * calling aubio_onset_set_default_parameters(method), and reading the
 * getters. Implementation lives in aubioprocessor.cpp so that callers do
 * NOT need <aubio/aubio.h> in their translation unit. The returned struct
 * carries real (non-sentinel) values.
 *
 * methodIndex maps to kOnsetMethods order:
 *   0 energy, 1 hfc, 2 complex, 3 phase, 4 wphase,
 *   5 specdiff, 6 kl, 7 mkl, 8 specflux.
 *
 * Out-of-range index returns a default-constructed (sentinel-filled)
 * OnsetMethodOverride.
 */
OnsetMethodOverride readAubioOnsetDefaults(int methodIndex, uint32_t sampleRate = 44100);
