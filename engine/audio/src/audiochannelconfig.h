/*
  Q Light Controller Plus
  audiochannelconfig.h

  Copyright (c) Massimo Callegari

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#pragma once

#include <QString>

struct EnvelopeConfig
{
    double attackMs = 25.0;
    double releaseMs = 180.0;
};

struct TriggerConfig
{
    double highThreshold = 0.65;
    double lowThreshold = 0.45;
    double holdMs = 80.0;
    double cooldownMs = 120.0;
};

struct BandLayout
{
    double subMaxHz = 60.0;
    double bassMaxHz = 250.0;
    double lowMidMaxHz = 500.0;
    double midMaxHz = 2000.0;
    double highMaxHz = 5000.0;
};

struct NoiseGateConfig
{
    double thresholdDb = -54.0;
    double holdMs = 120.0;
};

/**
 * Direct aubio configuration — every field maps 1:1 to an aubio_*_set_*() call
 * inside AubioProcessor. No QLC+ post-processing is applied to these values.
 */
struct AubioConfig
{
    enum BeatPhaseSource
    {
        AubioSamples = 0, // Derive beatPhase from aubio's internal sample counter.
        QlcTimer     = 1  // Derive beatPhase from QLC+ wall-clock time (future).
    };

    // PRE-aubio PCM gain. Multiplied into the float-normalized hop buffer before
    // any aubio_*_do() call. This is the ONLY place the user-facing input gain
    // is applied — aubio output is never post-multiplied. Non-aubio param, but
    // lives here because it gates what aubio sees.
    double inputGainLinear = 1.0;

    // Mel filterbank — aubio_filterbank_set_norm / aubio_filterbank_set_power.
    // norm: 1 = each mel filter normalized to unit area (default), 0 = raw triangular weights.
    // power: input |X|^power before filtering. 1 = magnitude, 2 = power (energy).
    // Changing either requires rebuilding the filterbank (norm must be set before
    // set_mel_coeffs_slaney).
    double filterbankNorm = 1.0;
    double filterbankPower = 1.0;

    // Onset detection — applied to all 9 detector instances.
    double onsetThreshold = 0.3;       // aubio_onset_set_threshold
    double onsetMinIntervalMs = 50.0;  // aubio_onset_set_minioi_ms
    double onsetSilenceDb = -70.0;     // aubio_onset_set_silence (dBFS gate for detection)
    double onsetDelayMs = 0.0;         // aubio_onset_set_delay_ms; 0 = use aubio default (~4.3 * hop / SR)

    // Pitch detection — aubio_pitch_*.
    QString pitchMethod = QStringLiteral("yinfft"); // yin, yinfft, yinfast, schmitt, fcomb, mcomb
    double pitchSilenceDb = -40.0;     // aubio_pitch_set_silence
    double pitchTolerance = 0.7;       // aubio_pitch_set_tolerance

    // Tempo / beat — aubio_tempo_*.
    QString tempoMethod = QStringLiteral("default");
    double tempoSilenceDb = -90.0;     // aubio_tempo_set_silence
    double tempoThreshold = 0.3;       // aubio_tempo_set_threshold (peak-picking)
    int tatumSubdivision = 4;          // aubio_tempo_set_tatum_signature
    BeatPhaseSource beatPhaseSource = AubioSamples;

    // Transient / Steady Separation — aubio_tss_*.
    double tssAlpha = 3.0;             // aubio_tss_set_alpha
    double tssBeta = 3.0;              // aubio_tss_set_beta
    double tssThreshold = 0.25;        // aubio_tss_set_threshold
};

struct AudioChannelConfig
{
    EnvelopeConfig envelope;
    TriggerConfig triggers;
    BandLayout bandLayout;
    NoiseGateConfig noiseGate;
    double brightnessFloor = 0.0;
    double volumeSmoothingMs = 100.0;
    AubioConfig aubio;

    static AudioChannelConfig defaults();
    static AudioChannelConfig fromLegacySliders(int gain, int reactivity, int floor, int sensitivity);
};
