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
#include <array>
#include <atomic>
#include <memory>
#include <vector>

#define SETTINGS_AUDIO_INPUT_DEVICE   "audio/input"
#define SETTINGS_AUDIO_INPUT_SRATE    "audio/samplerate"
#define SETTINGS_AUDIO_INPUT_CHANNELS "audio/channels"

#define AUDIO_DEFAULT_SAMPLE_RATE     44100
#define AUDIO_DEFAULT_CHANNELS        1
// Source block size. Packet and analysis frame boundaries are independent.
#define AUDIO_DEFAULT_BUFFER_SIZE     512
#define AUDIO_ANALYSIS_SAMPLE_RATE    30000
#define AUDIO_ANALYSIS_HOP_SIZE       500

#define FREQ_SUBBANDS_MAX_NUMBER        32
#define FREQ_SUBBANDS_DEFAULT_NUMBER    16
#define SPECTRUM_MIN_FREQUENCY          40
#define SPECTRUM_MAX_FREQUENCY          5000

class AudioAnalyzer;
struct AubioResults;
struct AubioConfig;
struct AudioFrame;
struct SRC_STATE_tag;

class AudioCapture : public QThread
{
    Q_OBJECT
public:
    enum Status { Stopped, Starting, Available, Unavailable, Error };
    Q_ENUM(Status)

    AudioCapture(QObject* parent = 0);
    ~AudioCapture();

    int defaultBarsNumber() const;

    /** Deprecated no-op kept for binary compatibility with legacy consumers.
     *  Internally only used to track when capture should auto-start/stop. */
    void registerBandsNumber(int number);
    void unregisterBandsNumber(int number);

    static int minFrequency() { return SPECTRUM_MIN_FREQUENCY; }
    static int maxFrequency() { return SPECTRUM_MAX_FREQUENCY; }
    unsigned int sampleRate() const { return m_appliedSampleRate.load(); }
    unsigned int channels() const { return m_appliedChannels.load(); }
    Status status() const { return m_status.load(); }
    bool available() const { return status() == Available; }
    QString statusMessage() const;
    uint64_t sourceEpoch() const { return m_sourceEpoch.load(); }
    void restart();

    /** Compute log-spaced low/high cut bin indices for legacy band splitting. */
    static int lowCutBin(int N);
    static int highCutBin(int N);

    /** Compatibility getters over the active profile's published bank values. */
    double bandMagnitude(int bandIndex, int numBands) const;
    double bandMaxMagnitude(int numBands) const;

    virtual void setVolume(qreal volume) = 0;

    void run() override;

    quint32 signalPower() const { return m_signalPower; }

    void setAnalyzer(AudioAnalyzer *analyzer);

    /** Compatibility entry point. Analysis configuration belongs to profiles. */
    void setAubioConfig(const AubioConfig &cfg);

protected:
    virtual bool initialize() = 0;
    virtual void uninitialize() = 0;
    virtual void suspend() = 0;
    virtual void resume() = 0;
    virtual qint64 latency() const = 0;

    void stop();

    virtual bool readAudio(int maxSize) = 0;
    virtual bool sourceFailed() const { return false; }
    virtual bool retrySource() const { return false; }
    virtual void clearPendingInput() {}

    bool processData();
    void setStatus(Status status, const QString &message, bool force = false);

signals:
    /** Borrowed PCM. Receivers must use a direct synchronous connection. */
    void frameReady(const AudioFrame &frame);
    void statusChanged(AudioCapture::Status status, const QString &message, quint64 sourceEpoch);
    /** Emitted after each capture block once aubio analysis is complete. */
    void aubioDataReady(const AubioResults &results, quint32 power);
    void volumeChanged(int volume);

    /** Compatibility signal from the active profile's canonical tempo detector. */
    void beatDetected(int bpm);

protected:
    mutable QMutex m_mutex;

    std::atomic<bool> m_userStop;
    bool m_pause;
    unsigned int m_bufferSize, m_captureSize, m_sampleRate, m_channels;
    unsigned int m_readFrames = AUDIO_DEFAULT_BUFFER_SIZE;

    int16_t *m_audioBuffer;
    std::vector<float> m_floatBuffer;
    bool m_floatInput = false;

    std::atomic<quint32> m_signalPower;
    double m_smoothedSignalPower;

    /** Aggregate count of registered band consumers. Used solely to start/stop the capture thread. */
    int m_registerCount = 0;

    AudioAnalyzer *m_analyzer = nullptr;

    uint64_t m_frameIndex = 0;

private:
    bool applyCaptureFormat(unsigned int sampleRate, unsigned int channels);
    void resetStream();
    void publishFrame();
    void invalidate(Status status, const QString &message);

    std::unique_ptr<AudioAnalyzer> m_fallbackAnalyzer;
    SRC_STATE_tag *m_resampler = nullptr;
    std::vector<float> m_pendingMono;
    std::array<float, AUDIO_ANALYSIS_HOP_SIZE> m_frameSamples {};
    size_t m_frameSamplesUsed = 0;
    uint64_t m_sampleTime = 0;
    std::atomic<uint64_t> m_sourceEpoch {0};
    std::atomic<Status> m_status {Stopped};
    std::atomic<bool> m_restart {false};
    QString m_statusMessage;
    std::atomic<unsigned int> m_appliedSampleRate {AUDIO_DEFAULT_SAMPLE_RATE};
    std::atomic<unsigned int> m_appliedChannels {AUDIO_DEFAULT_CHANNELS};
};

#endif // AUDIOCAPTURE_H
