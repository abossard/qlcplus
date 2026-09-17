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
#include <memory>

struct AudioFrame;
class AubioProcessor;

class AudioChannel
{
public:
    explicit AudioChannel(const AudioChannelConfig &config, uint32_t profileId = UINT_MAX);
    ~AudioChannel();

    void update(const AudioFrame &frame, double audioDtMs);
    AudioSnapshot snapshot() const;
    void updateConfig(const AudioChannelConfig &config);
    AudioChannelConfig config() const;
    uint32_t profileId() const { return m_profileId; }
    void invalidate(uint64_t sourceEpoch);
    AubioResults aubioResults() const;

    /**
     * Inject a pre-built snapshot from an external source (e.g., Synesthesia
     * OSC). Thread-safe: uses the same m_mutex as snapshot(). When an external
     * source is active, update() becomes a no-op so the mic pipeline does not
     * overwrite injected data.
     */
    void injectSnapshot(const AudioSnapshot &snap);
    bool hasExternalSource() const;
    void setExternalSource(bool external);

private:
    const uint32_t m_profileId;
    std::unique_ptr<AubioProcessor> m_processor;
    uint64_t m_sourceEpoch = 0;
    uint64_t m_frameSequence = 0;
    uint64_t m_configRevision = 1;
    uint64_t m_pendingRevision = 1;
    AudioSnapshot::Events m_events;
    void resetState();
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
    static constexpr double kGateVolumeAlpha = 0.99;
    double m_gateVolumeSmoothed = -90.0;
    double m_freqPowerRaw[4] = {};
    bool m_currentBeat = false;
    bool m_externalSource = false;

    // Kick detector state
    double m_kickSpike = 0.0;       // LedFx beatPower/history ratio
    TriggerInternal m_kickState;
    bool m_kickFired = false;
    bool m_kickReleased = false;
    std::deque<double> m_beatPowerHistory;
    double m_timeSinceLastBeatSec = 0.0;

    // LedFx audio.py:1159 — freq_power_filter initialized to zeros
    double m_freqPower[4] = {};  // beat, bass, mids, highs

    // Mel post-processing — legacy 40-band path.
    MelPostProcessor m_melPost;
    double m_melProcessed[AUBIO_MEL_BANDS] = {};
    double m_melNovelty[AUBIO_MEL_BANDS] = {};

    // Independent AGC/smoothing for each cumulative bank. Scalar powers use
    // the full-range high bank, while kick detection uses the low bank.
    MelPostProcessor m_melPostLow;
    MelPostProcessor m_melPostMid;
    MelPostProcessor m_melPostHigh;
    // Only the short power path has extra normalization/smoothing state.
    MelPostProcessor m_powerMelPost;
    double m_powerMelProcessed[AudioSnapshot::kMelBankBandsMax] = {};
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
