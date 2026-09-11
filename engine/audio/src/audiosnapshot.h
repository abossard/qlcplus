/*
  Q Light Controller Plus
  audiosnapshot.h

  Copyright (c) Massimo Callegari

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#pragma once

#include "aubioresults.h"
#include <QString>
#include <climits>

struct TriggerState
{
    double value = 0.0;
    bool active = false;
    bool firedThisFrame = false;
    bool releasedThisFrame = false;
    double heldMs = 0.0;
    double cooldownRemainingMs = 0.0;
};

/** Coherent profile publication. Legacy fields remain alongside source,
 * configuration, bank metadata and cumulative event counters. */
struct AudioSnapshot
{
    QString sourceId;
    uint32_t profileId = UINT_MAX;
    uint64_t sourceEpoch = 0;
    uint64_t frameSequence = 0;
    uint64_t configRevision = 0;
    AudioChannelConfig config;
    uint64_t sampleTime = 0;
    int64_t publishTimeNs = 0;
    bool available = false;
    QString status = QStringLiteral("unavailable");
    struct Events
    {
        uint64_t onset = 0;
        uint64_t beat = 0;
        uint64_t kick = 0;
        uint64_t bar = 0;
    } events;

    // Mel spectrum (40 bands from aubio filterbank)
    double mel[AUBIO_MEL_BANDS] = {};

    // Post-processed mel (LedFx-style power scaling + Gaussian AGC + ExpFilter
    // smoothing). Equal to `mel` when MelPostProcessor is disabled (bypass).
    double melProcessed[AUBIO_MEL_BANDS] = {};

    // Novelty: processedMel minus a very-slow common filter. Zero when the
    // post-processor is disabled.
    double melNovelty[AUBIO_MEL_BANDS] = {};

    /** Each bank has independent normalization and a valid prefix of count
     * bins. Centers describe the same applied configuration as its values. */
    static constexpr int kMelBankBandsMax = kMaxMelBands;
    struct MelBankSnapshot
    {
        double raw      [kMelBankBandsMax] = {};
        double processed[kMelBankBandsMax] = {};
        double novelty  [kMelBankBandsMax] = {};
        double centersHz[kMelBankBandsMax] = {};
        int    count = 0;
        double minHz = 0.0;
        double maxHz = 0.0;
        double gain = 0.0;
    };
    MelBankSnapshot melLow;
    MelBankSnapshot melMid;
    MelBankSnapshot melHigh;

    // MFCC (13 coefficients from aubio)
    double mfcc[AUBIO_MFCC_COEFFS] = {};

    // LedFx-parity scalar bank power (mean of each mel bank's processed[]).
    // Populated by AudioChannel::buildSnapshot(); zero when banks disabled.
    // LedFx audio.py:1283-1342 — 4 raw freq_power slots + lows composite.
    double beatPower = 0.0;   // freq_power_filter[0], 0-100 Hz
    double bassPower = 0.0;   // freq_power_filter[1], 100-250 Hz
    double lows  = 0.0;   // (beat + bass) / 2
    double mids  = 0.0;   // freq_power_filter[2], 250-3000 Hz
    double highs = 0.0;   // freq_power_filter[3], 3000-10000 Hz
    double powersRaw[4] = {};

    // 3 mel-bank Schmitt triggers ([0]=low, [1]=mid, [2]=high) plus
    // volume / beat / kick = 6 triggers total.
    TriggerState triggers[3];
    TriggerState volumeTrigger;
    TriggerState beatTrigger;
    // Kick / bass Schmitt-trigger detector. `value` is the raw spike ratio
    // (current low-mel energy / slow-release envelope); can exceed 1.0.
    TriggerState kickTrigger;

    struct
    {
        double raw = 0.0;
        double smoothed = 0.0;
        double normalized = 0.0;
        // LedFx audio.py:1021 — volume = 1 + aubio.db_spl(raw) / 100.
        // Mirrors AudioFrame::volumeNorm; lets QML / scripts read a 0..1
        // volume that's directly comparable to LedFx's min_volume threshold
        // (audio.py:409, default 0.2).
        double volumeNorm = 0.0;
    } volume;

    struct
    {
        bool valid = false;
        int beatInBar = 0;
        int beatsPerBar = 4;
        bool beat = false;
        double bpm = 0.0;
        double beatPhase = 0.0;
        double barPhase = 0.0;
        double beatConfidence = 0.0;
        bool tatum = false;
    } music;

    struct
    {
        double rmsDb = -96.0;
        double peakDb = -96.0;
        double crestFactor = 1.0;
        double centroidHz = 0.0;
        double spread = 0.0;
        double rolloffHz = 0.0;
        double flux = 0.0;
        double hfc = 0.0;
        // Spectral flatness (Wiener entropy): geometric_mean / arithmetic_mean
        // of the post-processed mel magnitudes. 0..1 (1 == white noise / flat,
        // 0 == single tone). Computed in AudioChannel::buildSnapshot().
        double flatness = 0.0;
    } features;

    // Live AGC scalar from MelPostProcessor (LedFx mel_gain). Reflects the
    // current divisor applied to the master mel before publishing
    // melProcessed[]. 1.0 when the post-processor is disabled (bypass).
    double melAgcGain = 1.0;

    struct
    {
        bool energy = false;
        bool hfc = false;
        bool complex_ = false;
        bool phase = false;
        bool wphase = false;
        bool specdiff = false;
        bool kl = false;
        bool mkl = false;
        bool specflux = false;
        // Diagnostic outputs from aubio_onset_get_descriptor /
        // aubio_onset_get_thresholded_descriptor. Indexed in the same order
        // as the boolean fields above (energy=0 ... specflux=8).
        double descriptors[AUBIO_ONSET_METHODS] = {};
        double thresholdedDescriptors[AUBIO_ONSET_METHODS] = {};
    } onsets;

    struct
    {
        double hz = 0.0;
        double value = 0.0;
        QString unit = QStringLiteral("Hz");
        double confidence = 0.0;
    } pitch;

    struct
    {
        double midi = 0.0;
        double velocity = 0.0;
        bool noteOn = false;
        bool noteOff = false;
    } note;

    /**
     * Raw aubio_tss_do per-bin cvec norms, last hop wins. Bin i frequency =
     * aubio_bintofreq(i, sampleRate, winSize). Consumers do their own
     * derivations (sums, ratios, band groupings).
     */
    struct
    {
        double transientNorm[AubioResults::kMaxTssBins] = {};
        double steadyNorm[AubioResults::kMaxTssBins] = {};
        int binCount = 0;
    } tss;

    double audioDtMs = 0.0;
    double brightnessFloor = 0.0;
    bool noiseGateClosed = false;
    bool downbeatFired = false;
};
