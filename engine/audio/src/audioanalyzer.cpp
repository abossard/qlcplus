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

    if (!m_hasTimingSample)
    {
        m_avgChannelTimeUs = channelUs;
        m_avgFrameTimeUs = frameUs;
        m_hasTimingSample = true;
    }
    else
    {
        constexpr double a = 0.1;
        m_avgChannelTimeUs = (1.0 - a) * m_avgChannelTimeUs + a * channelUs;
        m_avgFrameTimeUs   = (1.0 - a) * m_avgFrameTimeUs   + a * frameUs;
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
    return m_avgFrameTimeUs;
}

double AudioAnalyzer::avgChannelTimeUs() const
{
    return m_avgChannelTimeUs;
}

double AudioAnalyzer::computeAudioDtMs(const AudioFrame &frame)
{
    if (frame.sampleRate == 0 || frame.sampleCount == 0)
        return 0.0;

    return double(frame.sampleCount) / double(frame.sampleRate) * 1000.0;
}
