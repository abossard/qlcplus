/*
  Q Light Controller Plus
  audioanalyzer.cpp

  Copyright (c) Massimo Callegari

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include "audioanalyzer.h"

#include "audiochannel.h"
#include "audiochannelconfig.h"
#include "audioframe.h"

#include <QElapsedTimer>
#include <QMutexLocker>
#include <QDebug>

#include <algorithm>
#include <cmath>
#include <chrono>

AudioAnalyzer::AudioAnalyzer()
{
    m_defaultChannel = createChannel(AudioChannelConfig::defaults());
}

AudioAnalyzer::~AudioAnalyzer()
{
    QMutexLocker locker(&m_channelsMutex);
    qDeleteAll(m_channels);
    m_channels.clear();
    m_defaultChannel = nullptr;
}

void AudioAnalyzer::processFrame(AudioFrame &frame)
{
    if (frame.samples && (frame.sampleRate != 30000 || frame.sampleCount != 500 ||
        !std::all_of(frame.samples, frame.samples + frame.sampleCount,
                     [](float sample) { return std::isfinite(sample); })))
    {
        invalidate(frame.sourceEpoch);
        frame.aubio = &m_compatibilityResults;
        frame.beatDetected = false;
        return;
    }
    QElapsedTimer frameTimer;
    frameTimer.start();

    const double audioDtMs = computeAudioDtMs(frame);

    QElapsedTimer channelTimer;
    channelTimer.start();
    {
        QMutexLocker locker(&m_channelsMutex);
        for (AudioChannel *ch : std::as_const(m_channels))
            ch->update(frame, audioDtMs);
        // Capture's legacy notification borrows this copy until the next hop.
        // It must not point into a profile that the UI can retire meanwhile.
        const AudioChannel *selected = m_defaultChannel;
        for (const AudioChannel *channel : std::as_const(m_channels))
            if (channel->profileId() == uint32_t(m_activeSelection.load()))
                selected = channel;
        m_compatibilityResults = selected ? selected->aubioResults() : AubioResults{};
        frame.aubio = &m_compatibilityResults;
        frame.beatDetected = m_compatibilityResults.beat;
    }
    const double channelUs = double(channelTimer.nsecsElapsed()) / 1000.0;

    const double frameUs = double(frameTimer.nsecsElapsed()) / 1000.0;

    if (!m_hasTimingSample.load(std::memory_order_relaxed))
    {
        m_avgChannelTimeUs.store(channelUs, std::memory_order_relaxed);
        m_avgFrameTimeUs.store(frameUs, std::memory_order_relaxed);
        m_hasTimingSample.store(true, std::memory_order_relaxed);
    }
    else
    {
        constexpr double a = 0.1;
        const double prevCh = m_avgChannelTimeUs.load(std::memory_order_relaxed);
        const double prevFr = m_avgFrameTimeUs.load(std::memory_order_relaxed);
        m_avgChannelTimeUs.store((1.0 - a) * prevCh + a * channelUs,
                                 std::memory_order_relaxed);
        m_avgFrameTimeUs.store((1.0 - a) * prevFr + a * frameUs,
                               std::memory_order_relaxed);
    }
}

AudioChannel *AudioAnalyzer::createChannel(const AudioChannelConfig &config, uint32_t profileId)
{
    const QString error = config.validationError();
    if (!error.isEmpty())
    {
        qWarning().noquote() << "AudioAnalyzer configuration rejected:" << error;
        return nullptr;
    }
    AudioChannel *ch = new AudioChannel(config, profileId);
    QMutexLocker locker(&m_channelsMutex);
    m_channels.append(ch);
    return ch;
}

AudioSnapshot AudioAnalyzer::snapshot(uint32_t profileId) const
{
    QMutexLocker locker(&m_channelsMutex);
    for (const AudioChannel *channel : m_channels)
    {
        if (channel->profileId() == profileId)
            return channel->snapshot();
    }
    AudioSnapshot missing;
    missing.profileId = profileId;
    return missing;
}

AudioSnapshot AudioAnalyzer::snapshot(const AudioChannel *channel) const
{
    QMutexLocker locker(&m_channelsMutex);
    if (channel && m_channels.contains(const_cast<AudioChannel *>(channel)))
        return channel->snapshot();
    return {};
}

void AudioAnalyzer::setActiveProfileId(uint32_t profileId)
{
    const uint64_t previous = m_activeSelection.load();
    if (uint32_t(previous) == profileId)
        return;
    m_selectionTimeNs.store(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
    m_activeSelection.store((((previous >> 32) + 1) << 32) | profileId);
}

AudioSnapshot AudioAnalyzer::activeSnapshot() const
{
    // Resolve identity before entering the channel registry; never retain a profile pointer.
    const uint64_t selection = m_activeSelection.load();
    AudioSnapshot result = snapshot(uint32_t(selection));
    if (result.publishTimeNs <= m_selectionTimeNs.load())
    {
        result = {};
        result.profileId = uint32_t(selection);
        result.status = QStringLiteral("reset");
    }
    result.sourceEpoch ^= selection & 0xffffffff00000000ULL;
    return result;
}

void AudioAnalyzer::invalidate(uint64_t sourceEpoch)
{
    QMutexLocker locker(&m_channelsMutex);
    for (AudioChannel *channel : std::as_const(m_channels))
    {
        if (!channel->hasExternalSource())
            channel->invalidate(sourceEpoch);
    }
    m_compatibilityResults = {};
}

void AudioAnalyzer::destroyChannel(AudioChannel *channel)
{
    if (channel == nullptr)
        return;

    QMutexLocker locker(&m_channelsMutex);
    if (m_channels.removeOne(channel))
    {
        if (m_defaultChannel == channel)
            m_defaultChannel = m_channels.isEmpty() ? nullptr : m_channels.first();
        delete channel;
    }
}

AudioChannel *AudioAnalyzer::defaultChannel()
{
    QMutexLocker locker(&m_channelsMutex);
    return m_defaultChannel;
}

double AudioAnalyzer::avgFrameTimeUs() const
{
    return m_avgFrameTimeUs.load(std::memory_order_relaxed);
}

double AudioAnalyzer::avgChannelTimeUs() const
{
    return m_avgChannelTimeUs.load(std::memory_order_relaxed);
}

double AudioAnalyzer::computeAudioDtMs(const AudioFrame &frame)
{
    if (frame.sampleRate == 0 || frame.sampleCount == 0)
        return 0.0;

    return double(frame.sampleCount) / double(frame.sampleRate) * 1000.0;
}
