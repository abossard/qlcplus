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

    AudioChannel *createChannel(const AudioChannelConfig &config);
    void destroyChannel(AudioChannel *channel);
    AudioChannel *defaultChannel();

    double avgFrameTimeUs() const;
    double avgChannelTimeUs() const;

private:
    QVector<AudioChannel *> m_channels;
    AudioChannel *m_defaultChannel = nullptr;
    mutable QMutex m_channelsMutex;

    double m_avgChannelTimeUs = 0.0;
    double m_avgFrameTimeUs = 0.0;
    bool m_hasTimingSample = false;

    double computeAudioDtMs(const AudioFrame &frame);
};

#endif // AUDIOANALYZER_H
