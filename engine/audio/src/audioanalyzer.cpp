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

#include <algorithm>

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
    QElapsedTimer frameTimer;
    frameTimer.start();

    const double audioDtMs = computeAudioDtMs(frame);

    QElapsedTimer channelTimer;
    channelTimer.start();
    {
        QMutexLocker locker(&m_channelsMutex);
        for (AudioChannel *ch : std::as_const(m_channels))
            ch->update(frame, audioDtMs);
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

AudioChannel *AudioAnalyzer::createChannel(const AudioChannelConfig &config)
{
    AudioChannel *ch = new AudioChannel(config);
    QMutexLocker locker(&m_channelsMutex);
    m_channels.append(ch);
    return ch;
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
