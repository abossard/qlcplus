/*
  Q Light Controller Plus
  audioanalyzer.h

  Copyright (c) Massimo Callegari

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef AUDIOANALYZER_H
#define AUDIOANALYZER_H

#include <QMutex>
#include <QVector>

#include <atomic>
#include "audiosnapshot.h"

class AudioChannel;
struct AudioChannelConfig;
struct AudioFrame;

class AudioAnalyzer
{
public:
    AudioAnalyzer();
    ~AudioAnalyzer();

    /** Called synchronously on the AudioCapture thread, once per audio block. */
    void processFrame(AudioFrame &frame);

    AudioChannel *createChannel(const AudioChannelConfig &config, uint32_t profileId = UINT_MAX);
    void destroyChannel(AudioChannel *channel);
    AudioChannel *defaultChannel();
    AudioSnapshot snapshot(uint32_t profileId = UINT_MAX) const;
    AudioSnapshot snapshot(const AudioChannel *channel) const;
    void setActiveProfileId(uint32_t profileId);
    uint32_t activeProfileId() const { return uint32_t(m_activeSelection.load()); }
    AudioSnapshot activeSnapshot() const;
    void invalidate(uint64_t sourceEpoch);

    double avgFrameTimeUs() const;
    double avgChannelTimeUs() const;

private:
    QVector<AudioChannel *> m_channels;
    AudioChannel *m_defaultChannel = nullptr;
    mutable QMutex m_channelsMutex;
    std::atomic<uint64_t> m_activeSelection {UINT_MAX};
    std::atomic<int64_t> m_selectionTimeNs {0};
    AubioResults m_compatibilityResults;

    // Written on the audio capture thread, read from the UI thread (telemetry
    // overlay). std::atomic<double> is lock-free on x86_64 / arm64 with the
    // toolchains QLC+ ships against.
    std::atomic<double> m_avgChannelTimeUs { 0.0 };
    std::atomic<double> m_avgFrameTimeUs   { 0.0 };
    std::atomic<bool>   m_hasTimingSample  { false };

    double computeAudioDtMs(const AudioFrame &frame);
};

#endif // AUDIOANALYZER_H
