/*
  Q Light Controller Plus
  audiochannel.h

  Copyright (c) Massimo Callegari

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#pragma once

#include "audiochannelconfig.h"
#include "audiosnapshot.h"

#include <QMutex>

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

    static constexpr int kBandCount = 5;
    static constexpr int kTriggerCount = 7;

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
    bool m_currentBeat = false;

    void updateEnvelopes(const AudioFrame &frame, double dtMs);
    void updateTriggers(double dtMs);
    void updateVolume(const AudioFrame &frame, double dtMs);
    void buildSnapshot(const AudioFrame &frame, double dtMs);

    static double alpha(double dtMs, double tauMs);
};
