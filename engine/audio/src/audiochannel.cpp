/*
  Q Light Controller Plus
  audiochannel.cpp

  Copyright (c) Massimo Callegari

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include "audiochannel.h"

#include "audioframe.h"
#include "aubioresults.h"

#include <QMutexLocker>
#include <QDebug>

#include <algorithm>
#include <cmath>

namespace
{
    constexpr double kSampleRateForMel = 44100.0; // assumed nyquist for mel mapping

    /** Convert a frequency in Hz to a mel filter band index in [0, AUBIO_MEL_BANDS]. */
    int melBandIndexForFrequency(double hz)
    {
        if (hz <= 0.0)
            return 0;

        const double melMax = 2595.0 * std::log10(1.0 + (kSampleRateForMel * 0.5) / 700.0);
        const double mel = 2595.0 * std::log10(1.0 + hz / 700.0);
        const double ratio = (melMax > 0.0) ? (mel / melMax) : 0.0;
        const int idx = int(std::round(ratio * double(AUBIO_MEL_BANDS)));
        return std::clamp(idx, 0, AUBIO_MEL_BANDS);
    }

    double averageMel(const double *mel, int start, int end)
    {
        if (!mel || start >= end)
            return 0.0;

        double sum = 0.0;
        for (int i = start; i < end; i++)
            sum += mel[i];

        return sum / double(end - start);
    }

    double clampUnit(double value)
    {
        return std::clamp(value, 0.0, 1.0);
    }
}

AudioChannel::AudioChannel(const AudioChannelConfig &config)
    : m_config(config)
    , m_pendingConfig(config)
{
}

AudioChannel::~AudioChannel()
{
}

void AudioChannel::update(const AudioFrame &frame, double audioDtMs)
{
    {
        QMutexLocker locker(&m_mutex);
        if (m_hasPendingConfig)
        {
            m_config = m_pendingConfig;
            m_hasPendingConfig = false;
        }
    }

    m_currentBeat = frame.beatDetected;
    updateEnvelopes(frame, audioDtMs);
    updateVolume(frame, audioDtMs);
    updateTriggers(audioDtMs);
    buildSnapshot(frame, audioDtMs);
}

AudioSnapshot AudioChannel::snapshot() const
{
    QMutexLocker locker(&m_mutex);
    return m_snapshot;
}

void AudioChannel::updateConfig(const AudioChannelConfig &config)
{
    QMutexLocker locker(&m_mutex);
    m_pendingConfig = config;
    m_hasPendingConfig = true;
}

AudioChannelConfig AudioChannel::config() const
{
    QMutexLocker locker(&m_mutex);
    return m_hasPendingConfig ? m_pendingConfig : m_config;
}

void AudioChannel::updateEnvelopes(const AudioFrame &frame, double dtMs)
{
    const double *mel = (frame.aubio != nullptr) ? frame.aubio->mel : nullptr;

    const int edges[kBandCount + 1] =
    {
        0,
        melBandIndexForFrequency(m_config.bandLayout.subMaxHz),
        melBandIndexForFrequency(m_config.bandLayout.bassMaxHz),
        melBandIndexForFrequency(m_config.bandLayout.lowMidMaxHz),
        melBandIndexForFrequency(m_config.bandLayout.midMaxHz),
        melBandIndexForFrequency(m_config.bandLayout.highMaxHz)
    };

    m_noiseGateHeldMs = (frame.rmsDb < m_config.noiseGate.thresholdDb) ?
        (m_noiseGateHeldMs + dtMs) : 0.0;
    m_noiseGateClosed = frame.silent || m_noiseGateHeldMs >= m_config.noiseGate.holdMs;

    for (int i = 0; i < kBandCount; i++)
    {
        const int start = std::clamp(edges[i], 0, AUBIO_MEL_BANDS);
        const int end = std::clamp(std::max(edges[i + 1], start + 1), 0, AUBIO_MEL_BANDS);
        // Pristine aubio mel output. NO clamp, NO scale, NO post-multiply.
        // Pre-aubio inputGainLinear is applied to PCM inside AubioProcessor.
        // If saturation is undesired, tune aubio's filterbank norm/power or
        // lower the input gain (both upstream of aubio).
        m_bandValues[i] = m_noiseGateClosed ? 0.0 : averageMel(mel, start, end);

        const double tauMs = (m_bandValues[i] > m_envSmoothed[i]) ?
            m_config.envelope.attackMs : m_config.envelope.releaseMs;
        m_envSmoothed[i] += alpha(dtMs, tauMs) * (m_bandValues[i] - m_envSmoothed[i]);
        // Smoothed envelope is QLC+'s own derived signal (used by the trigger
        // comparator's 0..1 high/low thresholds), so clamping here is allowed.
        m_envSmoothed[i] = clampUnit(m_envSmoothed[i]);
    }
}

void AudioChannel::updateVolume(const AudioFrame &frame, double dtMs)
{
    // frame.rms / rmsDb come from AubioResults which are computed from the
    // pre-aubio gain-applied PCM hop buffer. No post-multiply here.
    m_volumeRaw = frame.rms;
    m_volumeNormalized = m_noiseGateClosed ? 0.0 : clampUnit(frame.rms);
    m_volumeSmoothed += alpha(dtMs, m_config.volumeSmoothingMs) *
        (m_volumeNormalized - m_volumeSmoothed);
    m_volumeSmoothed = clampUnit(m_volumeSmoothed);
}

void AudioChannel::updateTriggers(double dtMs)
{
    for (int i = 0; i < kBandCount; i++)
        m_triggerValues[i] = m_envSmoothed[i];
    m_triggerValues[5] = m_volumeSmoothed;
    m_triggerValues[6] = m_currentBeat ? 1.0 : 0.0;

    for (int i = 0; i < kTriggerCount; i++)
    {
        m_triggerFired[i] = false;
        m_triggerReleased[i] = false;

        TriggerInternal &state = m_triggerState[i];
        if (!state.active)
        {
            state.cooldownMs = std::max(0.0, state.cooldownMs - dtMs);
            state.heldMs = 0.0;
        }

        if (state.active)
        {
            state.heldMs += dtMs;
            if (state.heldMs >= m_config.triggers.holdMs &&
                m_triggerValues[i] <= m_config.triggers.lowThreshold)
            {
                state.active = false;
                state.heldMs = 0.0;
                state.cooldownMs = m_config.triggers.cooldownMs;
                m_triggerReleased[i] = true;
            }
            continue;
        }

        if (!m_noiseGateClosed && state.cooldownMs <= 0.0 &&
            m_triggerValues[i] >= m_config.triggers.highThreshold)
        {
            state.active = true;
            state.heldMs = 0.0;
            m_triggerFired[i] = true;
        }
    }
}

void AudioChannel::buildSnapshot(const AudioFrame &frame, double dtMs)
{
    AudioSnapshot snap;

    if (frame.aubio != nullptr)
    {
        const AubioResults &a = *frame.aubio;
        std::copy(a.mel, a.mel + AUBIO_MEL_BANDS, snap.mel);
        std::copy(a.mfcc, a.mfcc + AUBIO_MFCC_COEFFS, snap.mfcc);

        snap.music.bpm = a.bpm;
        snap.music.beatConfidence = a.beatConfidence;
        snap.music.tatum = a.tatum;
        snap.music.beatPhase = a.beatPhase;

        snap.tss.transientEnergy = a.transientEnergy;
        snap.tss.steadyEnergy = a.steadyEnergy;
        snap.tss.ratio = a.transientRatio;

        snap.features.centroidHz = a.centroidHz;
        snap.features.spread = a.spread;
        snap.features.rolloffHz = a.rolloffHz;
        snap.features.flux = a.flux;
        snap.features.hfc = a.hfc;

        snap.onsets.energy = a.onsets.energy;
        snap.onsets.hfc = a.onsets.hfc;
        snap.onsets.complex_ = a.onsets.complex;
        snap.onsets.phase = a.onsets.phase;
        snap.onsets.wphase = a.onsets.wphase;
        snap.onsets.specdiff = a.onsets.specdiff;
        snap.onsets.kl = a.onsets.kl;
        snap.onsets.mkl = a.onsets.mkl;
        snap.onsets.specflux = a.onsets.specflux;
        snap.onsets.voteCount = a.onsets.voteCount;

        snap.pitch.hz = a.pitchHz;
        snap.pitch.confidence = a.pitchConfidence;

        snap.note.midi = a.noteMidi;
        snap.note.velocity = a.noteVelocity;
        snap.note.noteOn = a.noteOn;
        snap.note.noteOff = a.noteOff;
    }

    snap.bands.sub = m_envSmoothed[0];
    snap.bands.bass = m_envSmoothed[1];
    snap.bands.lowMid = m_envSmoothed[2];
    snap.bands.mid = m_envSmoothed[3];
    snap.bands.high = m_envSmoothed[4];
    snap.bands.low = (snap.bands.sub + snap.bands.bass) * 0.5;

    for (int i = 0; i < kBandCount; i++)
    {
        snap.triggers[i] =
        {
            m_triggerValues[i],
            m_triggerState[i].active,
            m_triggerFired[i],
            m_triggerReleased[i],
            m_triggerState[i].heldMs,
            m_triggerState[i].cooldownMs
        };
    }

    snap.volumeTrigger =
    {
        m_triggerValues[5],
        m_triggerState[5].active,
        m_triggerFired[5],
        m_triggerReleased[5],
        m_triggerState[5].heldMs,
        m_triggerState[5].cooldownMs
    };
    snap.beatTrigger =
    {
        m_triggerValues[6],
        m_triggerState[6].active,
        m_triggerFired[6],
        m_triggerReleased[6],
        m_triggerState[6].heldMs,
        m_triggerState[6].cooldownMs
    };

    snap.volume.raw = m_volumeRaw;
    snap.volume.smoothed = m_volumeSmoothed;
    snap.volume.normalized = m_volumeNormalized;

    snap.music.beat = frame.beatDetected;
    snap.features.rmsDb = frame.rmsDb;
    snap.features.peakDb = frame.peakDb;
    snap.features.crestFactor = frame.crestFactor;
    snap.audioDtMs = dtMs;
    snap.brightnessFloor = m_config.brightnessFloor;
    snap.noiseGateClosed = m_noiseGateClosed;

    QMutexLocker locker(&m_mutex);
    m_snapshot = snap;
}

double AudioChannel::alpha(double dtMs, double tauMs)
{
    if (dtMs <= 0.0)
        return 0.0;
    if (tauMs <= 0.0)
        return 1.0;

    return std::clamp(1.0 - std::exp(-dtMs / tauMs), 0.0, 1.0);
}
