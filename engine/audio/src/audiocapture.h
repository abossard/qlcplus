/*
  Q Light Controller Plus
  audiocapture.h

  Copyright (c) Massimo Callegari

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef AUDIOCAPTURE_H
#define AUDIOCAPTURE_H

#include <stdint.h>
#include <QThread>
#include <QMutex>

#define SETTINGS_AUDIO_INPUT_DEVICE   "audio/input"
#define SETTINGS_AUDIO_INPUT_SRATE    "audio/samplerate"
#define SETTINGS_AUDIO_INPUT_CHANNELS "audio/channels"

#define AUDIO_DEFAULT_SAMPLE_RATE     44100
#define AUDIO_DEFAULT_CHANNELS        1
// Capture buffer size in mono frames. Sized to match AubioProcessor::hopSize()
// so each capture cycle feeds aubio exactly one hop, mirroring aubio's own
// examples (examples/utils.c examples_common_process) where the source reads
// hop_size samples and immediately calls the per-hop process function.
#define AUDIO_DEFAULT_BUFFER_SIZE     512

#define FREQ_SUBBANDS_MAX_NUMBER        32
#define FREQ_SUBBANDS_DEFAULT_NUMBER    16
#define SPECTRUM_MIN_FREQUENCY          40
#define SPECTRUM_MAX_FREQUENCY          5000

class AudioAnalyzer;
class AubioProcessor;
class BeatTracker;
struct AubioResults;
struct AubioConfig;

class AudioCapture : public QThread
{
    Q_OBJECT
public:
    AudioCapture(QObject* parent = 0);
    ~AudioCapture();

    int defaultBarsNumber() const;

    /** Deprecated no-op kept for binary compatibility with legacy consumers.
     *  Internally only used to track when capture should auto-start/stop. */
    void registerBandsNumber(int number);
    void unregisterBandsNumber(int number);

    static int minFrequency() { return SPECTRUM_MIN_FREQUENCY; }
    static int maxFrequency() { return SPECTRUM_MAX_FREQUENCY; }
    unsigned int sampleRate() const { return m_sampleRate; }

    /** Compute log-spaced low/high cut bin indices for legacy band splitting. */
    static int lowCutBin(int N);
    static int highCutBin(int N);

    /** Deprecated. Returns 0.0. */
    double bandMagnitude(int bandIndex, int numBands) const;
    double bandMaxMagnitude(int numBands) const;

    virtual void setVolume(qreal volume) = 0;

    void run() override;

    quint32 signalPower() const { return m_signalPower; }

    void setAnalyzer(AudioAnalyzer *analyzer);

    /** Forward an AubioConfig to the underlying AubioProcessor. Thread-safe;
     *  the new config is applied at the start of the next process() pass. */
    void setAubioConfig(const AubioConfig &cfg);

protected:
    virtual bool initialize() = 0;
    virtual void uninitialize() = 0;
    virtual void suspend() = 0;
    virtual void resume() = 0;
    virtual qint64 latency() const = 0;

    void stop();

    virtual bool readAudio(int maxSize) = 0;

    void processData();

signals:
    /** Emitted after each capture block once aubio analysis is complete. */
    void aubioDataReady(const AubioResults &results, quint32 power);
    void volumeChanged(int volume);

    /** Emitted on every beat detected by the beat tracker. @a bpm is
     *  the tracker's own tempo estimate; 0 means "no estimate", in
     *  which case the receiver has to derive the tempo from the
     *  spacing of the beat signals. */
    void beatDetected(int bpm);

protected:
    QMutex m_mutex;

    bool m_userStop, m_pause;
    unsigned int m_bufferSize, m_captureSize, m_sampleRate, m_channels;

    int16_t *m_audioBuffer;
    int16_t *m_audioMixdown;

    quint32 m_signalPower;
    double m_smoothedSignalPower;

    /** Aggregate count of registered band consumers. Used solely to start/stop the capture thread. */
    int m_registerCount = 0;

    AubioProcessor *m_aubio;

    AudioAnalyzer *m_analyzer = nullptr;

    uint64_t m_frameIndex = 0;
    BeatTracker *m_beatTracker;
};

#endif // AUDIOCAPTURE_H
