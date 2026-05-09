/*
  Q Light Controller Plus
  audiocapture.cpp

  Copyright (c) Massimo Callegari

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include <algorithm>
#include <cmath>

#include <QSettings>
#include <QDebug>
#include <qmath.h>

#include "audiocapture.h"
#include "audioanalyzer.h"
#include "audioframe.h"
#include "audiochannelconfig.h"
#include "aubioprocessor.h"
#include "aubioresults.h"

#define M_2PI       6.28318530718

AudioCapture::AudioCapture(QObject* parent)
    : QThread(parent)
    , m_userStop(true)
    , m_pause(false)
    , m_captureSize(0)
    , m_sampleRate(0)
    , m_channels(0)
    , m_audioBuffer(nullptr)
    , m_audioMixdown(nullptr)
    , m_signalPower(0)
    , m_smoothedSignalPower(0.0)
    , m_aubio(new AubioProcessor)
{
    qRegisterMetaType<AubioResults>("AubioResults");

    m_bufferSize = AUDIO_DEFAULT_BUFFER_SIZE;
    m_sampleRate = AUDIO_DEFAULT_SAMPLE_RATE;
    m_channels = AUDIO_DEFAULT_CHANNELS;

    QSettings settings;
    QVariant var = settings.value(SETTINGS_AUDIO_INPUT_SRATE);
    if (var.isValid())
        m_sampleRate = var.toInt();

    var = settings.value(SETTINGS_AUDIO_INPUT_CHANNELS);
    if (var.isValid())
        m_channels = var.toInt();

    qDebug() << "[AudioCapture] initialize" << m_sampleRate << m_channels;

    m_captureSize = m_bufferSize * m_channels;
    m_audioBuffer = new int16_t[m_captureSize];
    m_audioMixdown = new int16_t[m_bufferSize];

    m_aubio->initialize(m_sampleRate);
}

AudioCapture::~AudioCapture()
{
    Q_ASSERT(!this->isRunning());

    delete[] m_audioBuffer;
    delete[] m_audioMixdown;
    delete m_aubio;
}

int AudioCapture::defaultBarsNumber() const
{
    return FREQ_SUBBANDS_DEFAULT_NUMBER;
}

double AudioCapture::bandMagnitude(int bandIndex, int numBands) const
{
    Q_UNUSED(bandIndex)
    Q_UNUSED(numBands)
    return 0.0;
}

double AudioCapture::bandMaxMagnitude(int numBands) const
{
    Q_UNUSED(numBands)
    return 0.0;
}

void AudioCapture::setAnalyzer(AudioAnalyzer *analyzer)
{
    QMutexLocker locker(&m_mutex);
    m_analyzer = analyzer;
}

void AudioCapture::setAubioConfig(const AubioConfig &cfg)
{
    if (m_aubio != nullptr)
        m_aubio->setPendingConfig(cfg);
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
    qDebug() << "[AudioCapture] stop capture";
    while (this->isRunning())
    {
        m_userStop = true;
        usleep(10000);
    }
}

void AudioCapture::processData()
{
    unsigned int i, j;
    m_frameIndex++;

    const double frameSec = (m_sampleRate > 0) ? (double(m_bufferSize) / double(m_sampleRate)) : 0.0;
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

    // 1) Mix down to mono
    for (i = 0; i < m_bufferSize; i++)
    {
        int32_t mix = 0;
        for (j = 0; j < m_channels; j++)
            mix += m_audioBuffer[i * m_channels + j];
        m_audioMixdown[i] = int16_t(mix / int32_t(m_channels ? m_channels : 1));
    }

    // 2) DC removal + RMS / peak (silence detection)
    long long acc = 0;
    for (i = 0; i < m_bufferSize; ++i)
        acc += m_audioMixdown[i];
    const double mean = double(acc) / double(m_bufferSize);

    double sumSq = 0.0;
    double peakAbs = 0.0;
    for (i = 0; i < m_bufferSize; ++i)
    {
        const double x = (double(m_audioMixdown[i]) - mean) / 32768.0;
        sumSq += x * x;
        peakAbs = std::max(peakAbs, std::abs(x));
    }
    const double rms = qSqrt(sumSq / double(m_bufferSize));

    static constexpr double kSilenceRms = 0.002;
    const bool silent = (rms < kSilenceRms);

    // 3) Aubio analysis
    m_aubio->process(m_audioMixdown, int(m_bufferSize));
    const AubioResults &aubio = m_aubio->results();

    // 4) Smoothed power for legacy volumeChanged consumers
    const double rawPower = silent ? 0.0 : qBound(0.0, rms * 32768.0, 32767.0);
    const quint32 power = smoothPower(rawPower);
    const quint32 prevPower = m_signalPower;
    m_signalPower = power;

    // 5) Build frame & dispatch to analyzer
    AudioFrame frame;
    frame.frameIndex = m_frameIndex;
    frame.sampleRate = m_sampleRate;
    frame.sampleCount = m_bufferSize;
    frame.silent = silent;
    frame.beatDetected = aubio.beat;
    frame.rms = rms;
    frame.peak = peakAbs;
    frame.rmsDb = (rms > 0.0) ? (20.0 * std::log10(rms)) : -96.0;
    frame.peakDb = (peakAbs > 0.0) ? (20.0 * std::log10(peakAbs)) : -96.0;
    frame.crestFactor = (rms > 0.0) ? (peakAbs / rms) : 1.0;

    // LedFx audio.py:1021 — volume = 1 + aubio.db_spl(raw) / 100.
    // Map rmsDb (≤ 0 for normalized PCM) into 0..1 with the same shape so
    // downstream consumers (gates / brightness floors) can compare against
    // LedFx-style thresholds such as min_volume = 0.2 (audio.py:409).
    // Digital silence (frame.silent) is explicitly zeroed so volumeNorm == 0
    // is a reliable silence indicator at the AudioFrame level.
    frame.volumeNorm = frame.silent ? 0.0
        : std::clamp(1.0 + frame.rmsDb / 100.0, 0.0, 1.0);
    frame.aubio = &aubio;

    if (m_analyzer)
        m_analyzer->processFrame(frame);

    // 6) Emit signals
    emit aubioDataReady(aubio, power);

    if (power != prevPower)
        emit volumeChanged(int(power));

    if (aubio.beat)
        emit beatDetected();
}

void AudioCapture::run()
{
    qDebug() << "[AudioCapture] start capture";

    m_userStop = false;

    if (!initialize())
    {
        qWarning() << "[AudioCapture] Could not initialize audio capture, abandon";
        return;
    }

    while (!m_userStop)
    {
        if (m_pause == false && m_captureSize != 0)
        {
            if (readAudio(m_captureSize) == true)
            {
                QMutexLocker locker(&m_mutex);
                processData();
            }
            else
            {
                QThread::msleep(5);
            }
        }
        else
        {
            QThread::msleep(15);
        }

        QThread::yieldCurrentThread();
    }

    uninitialize();
}
