/*
  Q Light Controller Plus
  audiochannel.h

  Copyright (c) Massimo Callegari

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#pragma once

#include "audiochannelconfig.h"
#include "audiosnapshot.h"
#include "melpostprocessor.h"

#include <QMutex>

#include <deque>

struct AudioFrame;

class AudioChannel
{
public:
    explicit AudioChannel(const AudioChannelConfig &config);
    ~AudioChannel();

    void update(const AudioFrame &frame, double audioDtMs);
    AudioSnapshot snapshot() const;
    void updateConfig(const AudioChannelConfig &config);
    AudioChannelConfig config() const;

private:
    AudioChannelConfig m_config;
    AudioChannelConfig m_pendingConfig;
    bool m_hasPendingConfig = false;
    mutable QMutex m_mutex;

    AudioSnapshot m_snapshot;

    static constexpr int kBandCount = 3;
    // Schmitt trigger slots managed by updateTriggers(): 3 bank triggers
    // (low/mid/high) + volume + beat. Kick uses its own m_kickState below.
    static constexpr int kTriggerCount = 5;

    double m_envSmoothed[kBandCount] = {};

    struct TriggerInternal
    {
        bool active = false;
        double heldMs = 0.0;
        double cooldownMs = 0.0;
    };
    TriggerInternal m_triggerState[kTriggerCount];

    double m_volumeSmoothed = 0.0;

    double m_bandValues[kBandCount] = {};
    double m_triggerValues[kTriggerCount] = {};
    bool m_triggerFired[kTriggerCount] = {};
    bool m_triggerReleased[kTriggerCount] = {};
    double m_volumeRaw = 0.0;
    double m_volumeNormalized = 0.0;
    double m_noiseGateHeldMs = 0.0;
    bool m_noiseGateClosed = false;
    // LedFx audio.py:41 — volume_filter ExpFilter alpha (rise=decay=0.99).
    // Smooths rmsDb before gate comparison so per-hop noise doesn't chatter
    // the gate near threshold. Initialized very low so the gate starts closed.
    static constexpr double kGateVolumeAlpha = 0.99;
    double m_gateVolumeSmoothed = -100.0;
    // LedFx audio.py:409 — default min_volume = 0.2 in the 0..1 normalized
    // domain (volume = 1 + db_spl/100). NOT yet wired into the gate; the
    // dB-domain smoothed gate (Fix 3) is working well. Kept here so future
    // work can swap the comparison to:
    //     m_gateVolumeSmoothedNorm < kLedFxMinVolume
    // with the threshold expressed in LedFx-portable units.
    static constexpr double kLedFxMinVolume = 0.2;
    bool m_currentBeat = false;

    // Kick detector state
    double m_kickSpike = 0.0;       // LedFx beatPower/history ratio
    TriggerInternal m_kickState;
    bool m_kickFired = false;
    bool m_kickReleased = false;
    std::deque<double> m_beatPowerHistory;
    double m_timeSinceLastBeatSec = 0.0;

    // LedFx audio.py:1159 — freq_power_filter initialized to zeros
    double m_freqPower[4] = {};  // beat, bass, mids, highs
    bool m_prevDownbeat = false;

    // Mel post-processing — legacy 40-band path.
    MelPostProcessor m_melPost;
    double m_melProcessed[AUBIO_MEL_BANDS] = {};
    double m_melNovelty[AUBIO_MEL_BANDS] = {};

    // Visualization-bank post-processors for the 3 multi-resolution mel banks.
    // Each owns its own AGC / smoothing / novelty state — matches LedFx's
    // per-bank ExpFilter chain. Sized for AudioSnapshot::kMelBankBandsMax (32).
    //
    // IMPORTANT: these are NEVER used for Low/Mid/High power slicing on the VC
    // widget — that uses the single master m_melPost AGC plus the four Hz
    // cutoffs (beat/bass/mids/highs). See plan-clean-engineering.md §0.
    MelPostProcessor m_melPostLow;
    MelPostProcessor m_melPostMid;
    MelPostProcessor m_melPostHigh;
    double m_melLowProcessed [AudioSnapshot::kMelBankBandsMax] = {};
    double m_melLowNovelty   [AudioSnapshot::kMelBankBandsMax] = {};
    double m_melMidProcessed [AudioSnapshot::kMelBankBandsMax] = {};
    double m_melMidNovelty   [AudioSnapshot::kMelBankBandsMax] = {};
    double m_melHighProcessed[AudioSnapshot::kMelBankBandsMax] = {};
    double m_melHighNovelty  [AudioSnapshot::kMelBankBandsMax] = {};

    void updateNoiseGateState(const AudioFrame &frame, double dtMs);
    void updateFreqPower(const AudioFrame &frame);
    void updateEnvelopes(const AudioFrame &frame, double dtMs);
    void updateTriggers(double dtMs);
    void updateVolume(const AudioFrame &frame, double dtMs);
    void updateMelPost(const AudioFrame &frame);
    void updateKickDetector(const AudioFrame &frame, double dtMs);
    void buildSnapshot(const AudioFrame &frame, double dtMs);

    static double alpha(double dtMs, double tauMs);
};
