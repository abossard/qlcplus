/*
  Q Light Controller Plus
  audiosnapshot.h

  Copyright (c) Massimo Callegari

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#pragma once

#include "aubioresults.h"

struct TriggerState
{
    double value = 0.0;
    bool active = false;
    bool firedThisFrame = false;
    bool releasedThisFrame = false;
    double heldMs = 0.0;
    double cooldownRemainingMs = 0.0;
};

/**
 * v3 audio snapshot, sourced from AubioProcessor + AudioChannel envelopes.
 * No more legacy 32-bin FFT spectrum or AGC fields.
 */
struct AudioSnapshot
{
    // Mel spectrum (40 bands from aubio filterbank)
    double mel[AUBIO_MEL_BANDS] = {};

    // Post-processed mel (LedFx-style power scaling + Gaussian AGC + ExpFilter
    // smoothing). Equal to `mel` when MelPostProcessor is disabled (bypass).
    double melProcessed[AUBIO_MEL_BANDS] = {};

    // Novelty: processedMel minus a very-slow common filter. Zero when the
    // post-processor is disabled.
    double melNovelty[AUBIO_MEL_BANDS] = {};

    /**
     * Multi-resolution mel banks (Phase 3+4 placeholder; populated by Phase
     * 1+2). Each bank carries its own raw / processed / novelty triple,
     * matching the LedFx "each bank has its own ExpFilter chain" guarantee.
     * `count` is 0 when the bank is unconfigured/disabled — consumers must
     * treat that as "no data" and fall back to the legacy 40-band `mel`.
     */
    static constexpr int kMelBankBandsMax = kMaxMelBands;
    struct MelBankSnapshot
    {
        double raw      [kMelBankBandsMax] = {};
        double processed[kMelBankBandsMax] = {};
        double novelty  [kMelBankBandsMax] = {};
        int    count = 0;
        double minHz = 0.0;
        double maxHz = 0.0;
    };
    MelBankSnapshot melLow;
    MelBankSnapshot melMid;
    MelBankSnapshot melHigh;

    // MFCC (13 coefficients from aubio)
    double mfcc[AUBIO_MFCC_COEFFS] = {};

    // LedFx-parity scalar bank power (mean of each mel bank's processed[]).
    // Populated by AudioChannel::buildSnapshot(); zero when banks disabled.
    double lows  = 0.0;
    double mids  = 0.0;
    double highs = 0.0;

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
    } volume;

    struct
    {
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
};
