/*
  Q Light Controller Plus
  audiocapture.cpp

  Copyright (c) Massimo Callegari

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include <algorithm>
#include <cmath>
#include <memory>
#include <samplerate.h>

#include <QDebug>
#include <QElapsedTimer>
#include <qmath.h>

#include "audiocapture.h"
#include "audioanalyzer.h"
#include "../../src/audioview.h"
#include "audioframe.h"
#include "aubioresults.h"

AudioCapture::AudioCapture(QObject* parent)
    : QThread(parent)
    , m_userStop(true)
    , m_pause(false)
    , m_captureSize(0)
    , m_sampleRate(0)
    , m_channels(0)
    , m_audioBuffer(nullptr)
    , m_signalPower(0)
    , m_smoothedSignalPower(0.0)
{
    qRegisterMetaType<AubioResults>("AubioResults");

    m_bufferSize = AUDIO_DEFAULT_BUFFER_SIZE;
    m_sampleRate = AUDIO_DEFAULT_SAMPLE_RATE;
    m_channels = AUDIO_DEFAULT_CHANNELS;

#ifdef AUDIO_DEBUG
    qDebug() << "[AudioCapture] initialize" << m_sampleRate << m_channels;
#endif

    m_captureSize = m_bufferSize * m_channels;
    m_audioBuffer = new int16_t[m_captureSize];

    int error = 0;
    m_resampler = src_new(SRC_SINC_FASTEST, 1, &error);
    if (!m_resampler)
        qWarning() << "[AudioCapture] Cannot create resampler:" << src_strerror(error);
}

AudioCapture::~AudioCapture()
{
    Q_ASSERT(!this->isRunning());

    delete[] m_audioBuffer;
    src_delete(m_resampler);
}

int AudioCapture::defaultBarsNumber() const
{
    return FREQ_SUBBANDS_DEFAULT_NUMBER;
}

double AudioCapture::bandMagnitude(int bandIndex, int numBands) const
{
    QMutexLocker locker(&m_mutex);
    const AudioAnalyzer *analyzer = m_analyzer ? m_analyzer : m_fallbackAnalyzer.get();
    if (!analyzer)
        return 0.0;
    return audioFrequency(AudioRenderView::fromSnapshot(
        analyzer->activeSnapshot(), AudioRenderView::nowNs()), bandIndex, numBands);
}

double AudioCapture::bandMaxMagnitude(int numBands) const
{
    return numBands > 0 ? 1.0 : 0.0;
}

void AudioCapture::setAnalyzer(AudioAnalyzer *analyzer)
{
    QMutexLocker locker(&m_mutex);
    m_analyzer = analyzer;
}

void AudioCapture::setAubioConfig(const AubioConfig &cfg)
{
    Q_UNUSED(cfg)
}

bool AudioCapture::applyCaptureFormat(unsigned int sampleRate, unsigned int channels)
{
    if (sampleRate == 0 || channels == 0 || channels > 64
        || !src_is_valid_ratio(double(AUDIO_ANALYSIS_SAMPLE_RATE) / sampleRate)
        || !m_resampler)
    {
        qWarning() << "[AudioCapture] Invalid negotiated format"
                   << sampleRate << "Hz" << channels << "channels";
        return false;
    }

    const unsigned int captureSize = m_bufferSize * channels;
    std::unique_ptr<int16_t[]> resizedBuffer;
    if (captureSize != m_captureSize)
        resizedBuffer = std::make_unique<int16_t[]>(captureSize);

    QMutexLocker locker(&m_mutex);
    if (resizedBuffer)
    {
        delete[] m_audioBuffer;
        m_audioBuffer = resizedBuffer.release();
        m_captureSize = captureSize;
    }

    m_sampleRate = sampleRate;
    m_channels = channels;
    m_appliedSampleRate = sampleRate;
    m_appliedChannels = channels;
    m_floatBuffer.resize(captureSize);
    resetStream();
    return true;
}

QString AudioCapture::statusMessage() const
{
    QMutexLocker locker(&m_mutex);
    return m_statusMessage;
}

void AudioCapture::setStatus(Status status, const QString &message, bool force)
{
    {
        QMutexLocker locker(&m_mutex);
        if (!force && m_status == status && m_statusMessage == message)
            return;
        m_status = status;
        m_statusMessage = message;
    }
    emit statusChanged(status, message, sourceEpoch());
}

void AudioCapture::resetStream()
{
    clearPendingInput();
    if (m_resampler)
        src_reset(m_resampler);
    m_pendingMono.clear();
    m_frameSamplesUsed = 0;
    m_sampleTime = 0;
    m_frameIndex = 0;
    m_signalPower = 0;
    m_smoothedSignalPower = 0.0;
}

void AudioCapture::invalidate(Status status, const QString &message)
{
    {
        QMutexLocker locker(&m_mutex);
        ++m_sourceEpoch;
        resetStream();
        if (m_analyzer)
            m_analyzer->invalidate(sourceEpoch());
        if (m_fallbackAnalyzer)
            m_fallbackAnalyzer->invalidate(sourceEpoch());
    }
    emit volumeChanged(0);
    setStatus(status, message, true);
}

void AudioCapture::restart()
{
    m_restart = true;
    if (!isRunning())
    {
        invalidate(Starting, QStringLiteral("Waiting for input"));
        QMutexLocker locker(&m_mutex);
        if (m_registerCount > 0)
            start();
    }
}

int AudioCapture::lowCutBin(int N)
{
    if (N < 3) return 0;
    const double logRange = qLn(double(SPECTRUM_MAX_FREQUENCY) / double(SPECTRUM_MIN_FREQUENCY));
    const double lowRatio = qLn(250.0 / double(SPECTRUM_MIN_FREQUENCY)) / logRange;
    return qBound(1, int(N * lowRatio), N - 2);
}

int AudioCapture::highCutBin(int N)
{
    if (N < 3) return N;
    const double logRange = qLn(double(SPECTRUM_MAX_FREQUENCY) / double(SPECTRUM_MIN_FREQUENCY));
    const double highRatio = qLn(2000.0 / double(SPECTRUM_MIN_FREQUENCY)) / logRange;
    int lowCut = lowCutBin(N);
    int lastMid = qBound(lowCut, int(N * highRatio), N - 2);
    return lastMid + 1;
}

void AudioCapture::registerBandsNumber(int number)
{
    Q_UNUSED(number)
    QMutexLocker locker(&m_mutex);
    const bool wasZero = (m_registerCount == 0);
    m_registerCount++;
    if (wasZero)
    {
        locker.unlock();
        start();
    }
}

void AudioCapture::unregisterBandsNumber(int number)
{
    Q_UNUSED(number)
    QMutexLocker locker(&m_mutex);
    if (m_registerCount > 0)
        m_registerCount--;
    if (m_registerCount == 0)
    {
        locker.unlock();
        stop();
    }
}

void AudioCapture::stop()
{
#ifdef AUDIO_DEBUG
    qDebug() << "[AudioCapture] stop capture";
#endif
    m_userStop = true;
    requestInterruption();
    wait();
}

bool AudioCapture::processData()
{
    std::array<float, AUDIO_DEFAULT_BUFFER_SIZE> mono;
    for (unsigned int i = 0; i < m_readFrames; ++i)
    {
        double mixed = 0.0;
        for (unsigned int channel = 0; channel < m_channels; ++channel)
        {
            const size_t index = size_t(i) * m_channels + channel;
            const float value = m_floatInput ? m_floatBuffer[index]
                                             : float(m_audioBuffer[index]) / 32768.0f;
            mixed += value;
        }
        mono[i] = float(mixed / m_channels);
    }

    m_pendingMono.insert(m_pendingMono.end(), mono.begin(), mono.begin() + m_readFrames);
    size_t consumed = 0;
    while (true)
    {
        long generated = 0;
        long used = 0;
        if (m_sampleRate == AUDIO_ANALYSIS_SAMPLE_RATE)
        {
            used = long(std::min(m_pendingMono.size() - consumed,
                                m_frameSamples.size() - m_frameSamplesUsed));
            std::copy_n(m_pendingMono.data() + consumed, used, m_frameSamples.data() + m_frameSamplesUsed);
            generated = used;
        }
        else
        {
            SRC_DATA data {};
            data.data_in = m_pendingMono.data() + consumed;
            data.input_frames = long(m_pendingMono.size() - consumed);
            data.data_out = m_frameSamples.data() + m_frameSamplesUsed;
            data.output_frames = long(m_frameSamples.size() - m_frameSamplesUsed);
            data.src_ratio = double(AUDIO_ANALYSIS_SAMPLE_RATE) / m_sampleRate;
            const int error = src_process(m_resampler, &data);
            if (error)
            {
                qWarning() << "[AudioCapture] Resampling failed:" << src_strerror(error);
                return false;
            }
            used = data.input_frames_used;
            generated = data.output_frames_gen;
        }
        consumed += size_t(used);
        m_frameSamplesUsed += size_t(generated);
        if (m_frameSamplesUsed == m_frameSamples.size())
        {
            publishFrame();
            m_frameSamplesUsed = 0;
        }
        if (used == 0 && generated == 0)
            break;
    }
    m_pendingMono.erase(m_pendingMono.begin(), m_pendingMono.begin() + consumed);
    return true;
}

void AudioCapture::publishFrame()
{
    ++m_frameIndex;
    const double frameSec = double(AUDIO_ANALYSIS_HOP_SIZE) / AUDIO_ANALYSIS_SAMPLE_RATE;
    static constexpr double kAttackTauSec = 0.040;
    static constexpr double kReleaseTauSec = 0.200;
    const double attackAlpha = (frameSec > 0.0) ? (1.0 - qExp(-frameSec / kAttackTauSec)) : 1.0;
    const double releaseAlpha = (frameSec > 0.0) ? (1.0 - qExp(-frameSec / kReleaseTauSec)) : 1.0;

    auto smoothPower = [&](double rawPower) -> quint32
    {
        rawPower = qBound(0.0, rawPower, 32767.0);
        const double alpha = (rawPower > m_smoothedSignalPower) ? attackAlpha : releaseAlpha;
        m_smoothedSignalPower += alpha * (rawPower - m_smoothedSignalPower);
        m_smoothedSignalPower = qBound(0.0, m_smoothedSignalPower, 32767.0);
        return quint32(qRound(m_smoothedSignalPower));
    };

    double sumSq = 0.0;
    double peakAbs = 0.0;
    for (float sample : m_frameSamples)
    {
        const double x = sample;
        sumSq += x * x;
        peakAbs = std::max(peakAbs, std::abs(x));
    }
    const double rms = qSqrt(sumSq / double(m_frameSamples.size()));

    const bool silent = (rms == 0.0);

    const double rawPower = silent ? 0.0 : qBound(0.0, rms * 32768.0, 32767.0);
    const quint32 power = smoothPower(rawPower);
    const quint32 prevPower = m_signalPower;
    m_signalPower = power;

    AudioFrame frame;
    frame.frameIndex = m_frameIndex;
    frame.samples = m_frameSamples.data();
    frame.sourceEpoch = sourceEpoch();
    frame.sampleTime = m_sampleTime;
    frame.sampleRate = AUDIO_ANALYSIS_SAMPLE_RATE;
    frame.sampleCount = AUDIO_ANALYSIS_HOP_SIZE;
    frame.silent = silent;
    frame.rms = rms;
    frame.peak = peakAbs;
    frame.rmsDb = (rms > 0.0) ? (20.0 * std::log10(rms)) : -100.0;
    frame.peakDb = (peakAbs > 0.0) ? (20.0 * std::log10(peakAbs)) : -100.0;
    frame.crestFactor = (rms > 0.0) ? (peakAbs / rms) : 1.0;

    frame.volumeNorm = frame.silent ? 0.0
        : std::clamp(1.0 + frame.rmsDb / 100.0, 0.0, 1.0);
    if (m_analyzer)
        m_analyzer->processFrame(frame);
    else
    {
        if (!m_fallbackAnalyzer)
            m_fallbackAnalyzer = std::make_unique<AudioAnalyzer>();
        m_fallbackAnalyzer->processFrame(frame);
    }

    if (frame.aubio && frame.aubio->beat)
        emit beatDetected(qRound(frame.aubio->bpm));
    emit frameReady(frame);
    const AubioResults emptyResults {};
    emit aubioDataReady(frame.aubio ? *frame.aubio : emptyResults, power);
    m_sampleTime += AUDIO_ANALYSIS_HOP_SIZE;

    if (power != prevPower)
        emit volumeChanged(int(power));

}

void AudioCapture::run()
{
#ifdef AUDIO_DEBUG
    qDebug() << "[AudioCapture] start capture";
#endif

    m_userStop = false;
    m_restart = false;
    invalidate(Starting, QStringLiteral("Waiting for input"));
    bool initialized = false;
    QElapsedTimer lastData;
    while (!m_userStop && !isInterruptionRequested())
    {
        if (m_restart.exchange(false))
        {
            if (initialized)
                uninitialize();
            initialized = false;
            invalidate(Starting, QStringLiteral("Restarting input"));
        }
        if (!initialized)
        {
            if (!initialize())
            {
                if (status() == Starting)
                    setStatus(Unavailable, QStringLiteral("Cannot open selected input"));
                if (!retrySource())
                    break;
                QThread::msleep(50);
                continue;
            }
            if (!applyCaptureFormat(m_sampleRate, m_channels))
            {
                uninitialize();
                setStatus(Error, QStringLiteral("Unsupported input format"));
                if (!retrySource())
                    break;
                QThread::msleep(50);
                continue;
            }
            initialized = true;
            lastData.start();
        }
        if (m_pause == false && m_captureSize != 0)
        {
            m_readFrames = m_bufferSize;
            if (readAudio(m_captureSize) == true)
            {
                const uint64_t previousFrame = m_frameIndex;
                bool processed;
                {
                    QMutexLocker locker(&m_mutex);
                    processed = processData();
                }
                if (!processed)
                {
                    uninitialize();
                    initialized = false;
                    invalidate(Error, QStringLiteral("Audio conversion failed"));
                    if (!retrySource())
                        break;
                }
                else
                {
                    lastData.restart();
                    if (m_frameIndex != previousFrame)
                        setStatus(Available, QStringLiteral("Receiving audio"));
                }
            }
            else
            {
                if (sourceFailed())
                {
                    uninitialize();
                    initialized = false;
                    invalidate(Unavailable, QStringLiteral("Selected input disconnected"));
                }
                QThread::msleep(5);
            }
            // Leave time for the subscribed UI/beat timers before the 250 ms deadline.
            if (lastData.elapsed() >= 150 && status() != Unavailable)
                invalidate(Unavailable, QStringLiteral("No audio received"));
        }
        else
        {
            QThread::msleep(15);
        }

        QThread::yieldCurrentThread();
    }

    if (initialized)
        uninitialize();
    if (m_userStop || isInterruptionRequested())
        invalidate(Stopped, QStringLiteral("Input stopped"));
}
