/*
  Q Light Controller Plus
  audiochannel.cpp

  Copyright (c) Massimo Callegari

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include "audiochannel.h"

#include "audioframe.h"
#include "aubioresults.h"
#include "aubioprocessor.h"

#include <aubio/aubio.h>

#include <QMutexLocker>
#include <QDebug>

#include <algorithm>
#include <cmath>
#include <chrono>

namespace
{
    double averageMel(const double *mel, int start, int end)
    {
        if (!mel || start >= end)
            return 0.0;

        double sum = 0.0;
        for (int i = start; i < end; i++)
            sum += mel[i];

        return sum / double(end - start);
    }

    inline double hzToMattMel(double hz)
    {
        return 3700.0 * std::log(1.0 + hz / 230.0) / std::log(12.0);
    }

    inline double mattMelToHz(double matt)
    {
        return 230.0 * (std::pow(12.0, matt / 3700.0) - 1.0);
    }

    // Convert a Hz frequency to a bin index inside one of the 3 multi-mel
    // banks. Mirrors LedFx audio.py:1162-1182 — "first bank-centre frequency
    // strictly above the cutoff Hz". Per matt_mel warp the bank places
    // bands+2 edges evenly in matt_mel space; the interior `bands` points
    // are centres. Returns a value in [0, bands].
    inline int hzToBankBin(double hz, const MelBankConfig::Bank &bank)
    {
        const int n = std::clamp(bank.bands, 1, MelBankConfig::kMaxBandsPerBank);
        if (hz <= bank.minHz)
            return 0;
        if (hz >= bank.maxHz)
            return n;
        const double mmin = hzToMattMel(bank.minHz);
        const double mmax = hzToMattMel(bank.maxHz);
        if (mmax <= mmin)
            return 0;
        for (int i = 0; i < n; ++i)
        {
            const double center = std::floor(double(float(mattMelToHz(
                mmin + (mmax - mmin) * double(i + 1) / double(n + 1)))));
            if (center > hz)
                return i;
        }
        return n;
    }

    int lowMelBeatEndBin(double beatMaxHz, const MelBankConfig::Bank &bank)
    {
        const int n = std::clamp(bank.bands, 1, MelBankConfig::kMaxBandsPerBank);
        const double minMel = hzToMattMel(bank.minHz);
        const double maxMel = hzToMattMel(bank.maxHz);
        // LedFx melbank.py:264-286 — matt_mel is custom LedFx code, so QLC+
        // mirrors it here instead of using aubio.hztomel/meltohz.
        for (int i = 0; i < n; ++i)
        {
            const double t = double(i + 1) / double(n + 1);
            const double hz = std::floor(double(float(mattMelToHz(minMel + (maxMel - minMel) * t))));
            // LedFx audio.py:1185-1192 — beat_max_mel_index uses i - 1 for
            // the first low-bank centre frequency above beatMaxHz.
            if (hz > beatMaxHz)
                return std::clamp(i - 1, 1, n);
        }
        return n;
    }

}

AudioChannel::AudioChannel(const AudioChannelConfig &config, uint32_t profileId)
    : m_profileId(profileId)
    , m_config(config)
    , m_pendingConfig(config)
{
    // Master 40-band post-processor — drives snap.melProcessed[] and the
    // spectral-flatness display. The 3 visualization banks each own their
    // own MelPostConfig (LedFx melbank.py:374-378 — every bank has its own
    // ExpFilter chain), so AGC/smoothing can be tuned independently.
    resetState();
    m_snapshot.profileId = profileId;
}

AudioChannel::~AudioChannel()
{
}

void AudioChannel::update(const AudioFrame &input, double audioDtMs)
{
    QMutexLocker locker(&m_mutex);
    if (m_externalSource)
        return;

    if (m_sourceEpoch != input.sourceEpoch)
    {
        m_sourceEpoch = input.sourceEpoch;
        m_frameSequence = 0;
        m_events = {};
        resetState();
    }
    if (m_hasPendingConfig)
    {
        m_config = m_pendingConfig;
        m_configRevision = m_pendingRevision;
        m_hasPendingConfig = false;
        resetState();
    }
    AudioFrame frame = input;
    updateNoiseGateState(frame, audioDtMs);
    if (frame.samples)
    {
        if (!m_processor)
        {
            m_processor = std::make_unique<AubioProcessor>();
            m_processor->setPendingConfig(m_config.aubio);
            m_processor->initialize(30000);
        }
        if (m_processor->isInitialized())
        {
            m_processor->process(frame.samples, int(frame.sampleCount), !m_noiseGateClosed);
            frame.aubio = &m_processor->results();
            frame.beatDetected = frame.aubio->beat;
        }
        else
        {
            frame.aubio = nullptr;
            frame.beatDetected = false;
        }
    }
    m_currentBeat = frame.beatDetected;

    updateMelPost(frame);
    updateFreqPower(frame);
    updateEnvelopes(frame, audioDtMs);
    updateVolume(frame, audioDtMs);
    updateTriggers(audioDtMs);
    updateKickDetector(frame, audioDtMs);
    buildSnapshot(frame, audioDtMs);
}

void AudioChannel::resetState()
{
    m_processor.reset();
    m_melPost.setConfig(m_config.melPost);
    m_melPostLow.setConfig(m_config.aubio.melBanks.low.post);
    m_melPostMid.setConfig(m_config.aubio.melBanks.mid.post);
    m_melPostHigh.setConfig(m_config.aubio.melBanks.high.post);
    for (auto *post : {&m_melPost, &m_melPostLow, &m_melPostMid, &m_melPostHigh})
        post->reset();
    std::fill_n(m_freqPower, 4, 0.0);
    std::fill_n(m_freqPowerRaw, 4, 0.0);
    std::fill_n(m_envSmoothed, kBandCount, 0.0);
    std::fill_n(m_bandValues, kBandCount, 0.0);
    std::fill_n(m_triggerValues, kTriggerCount, 0.0);
    std::fill_n(m_triggerState, kTriggerCount, TriggerInternal{});
    std::fill_n(m_triggerFired, kTriggerCount, false);
    std::fill_n(m_triggerReleased, kTriggerCount, false);
    m_volumeRaw = m_volumeSmoothed = m_volumeNormalized = 0.0;
    m_noiseGateHeldMs = 0.0;
    m_gateVolumeSmoothed = -90.0;
    m_noiseGateClosed = true;
    m_currentBeat = false;
    m_kickState = {};
    m_kickFired = m_kickReleased = false;
    m_kickSpike = m_timeSinceLastBeatSec = 0.0;
    m_beatPowerHistory.clear();
}

void AudioChannel::invalidate(uint64_t sourceEpoch)
{
    QMutexLocker locker(&m_mutex);
    resetState();
    m_sourceEpoch = sourceEpoch;
    m_frameSequence = 0;
    m_events = {};
    m_snapshot = {};
    m_snapshot.profileId = m_profileId;
    m_snapshot.sourceEpoch = sourceEpoch;
    m_snapshot.configRevision = m_configRevision;
    m_snapshot.config = m_config;
    m_snapshot.noiseGateClosed = true;
    m_snapshot.publishTimeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

AubioResults AudioChannel::aubioResults() const
{
    QMutexLocker locker(&m_mutex);
    return m_processor ? m_processor->results() : AubioResults{};
}

AudioSnapshot AudioChannel::snapshot() const
{
    QMutexLocker locker(&m_mutex);
    return m_snapshot;
}

void AudioChannel::injectSnapshot(const AudioSnapshot &snap)
{
    QMutexLocker locker(&m_mutex);
    if (m_hasPendingConfig)
    {
        m_config = m_pendingConfig;
        m_configRevision = m_pendingRevision;
        m_hasPendingConfig = false;
    }
    m_snapshot = snap;
    m_snapshot.profileId = m_profileId;
    m_snapshot.configRevision = m_configRevision;
    m_snapshot.config = m_config;
    if (snap.sourceId.isEmpty())
    {
        m_snapshot.sourceId = QStringLiteral("osc");
        m_snapshot.sourceEpoch = m_sourceEpoch;
        m_snapshot.frameSequence = ++m_frameSequence;
        m_snapshot.available = true;
        m_snapshot.status = QStringLiteral("available");
        m_snapshot.music.valid = snap.music.bpm > 0.0;
        m_events.beat += snap.music.beat ? 1 : 0;
        m_events.onset += snap.onsets.hfc ? 1 : 0;
        m_events.kick += snap.kickTrigger.firedThisFrame ? 1 : 0;
        m_events.bar += snap.downbeatFired ? 1 : 0;
        m_snapshot.events = m_events;
        m_snapshot.publishTimeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }
}

bool AudioChannel::hasExternalSource() const
{
    QMutexLocker locker(&m_mutex);
    return m_externalSource;
}

void AudioChannel::setExternalSource(bool external)
{
    QMutexLocker locker(&m_mutex);
    if (m_externalSource == external)
        return;
    m_externalSource = external;
    resetState();
    m_events = {};
    m_frameSequence = 0;
    m_snapshot = {};
    m_snapshot.profileId = m_profileId;
    m_snapshot.sourceEpoch = ++m_sourceEpoch;
}

void AudioChannel::updateConfig(const AudioChannelConfig &config)
{
    const QString error = config.validationError();
    if (!error.isEmpty())
    {
        qWarning().noquote() << "AudioChannel configuration rejected:" << error;
        return;
    }
    QMutexLocker locker(&m_mutex);
    m_pendingConfig = config;
    m_hasPendingConfig = true;
    ++m_pendingRevision;
}

AudioChannelConfig AudioChannel::config() const
{
    QMutexLocker locker(&m_mutex);
    return m_hasPendingConfig ? m_pendingConfig : m_config;
}

void AudioChannel::updateNoiseGateState(const AudioFrame &frame, double dtMs)
{
    m_gateVolumeSmoothed = kGateVolumeAlpha * frame.volumeNorm
                         + (1.0 - kGateVolumeAlpha) * m_gateVolumeSmoothed;
    const double threshold = std::clamp(1.0 + m_config.noiseGate.thresholdDb / 100.0, 0.0, 1.0);
    const bool below = m_gateVolumeSmoothed <= threshold;
    m_noiseGateHeldMs = below ? m_noiseGateHeldMs + dtMs : 0.0;
    m_noiseGateClosed = frame.silent ||
        (below && m_noiseGateHeldMs >= m_config.noiseGate.holdMs);
}

void AudioChannel::updateFreqPower(const AudioFrame &frame)
{
    if (frame.aubio == nullptr)
        return;

    // Scalar powers share the full-range bank's normalization.
    const int nHigh = frame.aubio->melHighCount;

    if (nHigh <= 0)
        return;

    const auto &fp = m_config.freqPower;
    const auto &banks = m_config.aubio.melBanks;

    int beatEnd  = std::min(nHigh, hzToBankBin(fp.beat.maxHz, banks.high));
    int bassEnd  = std::min(nHigh, hzToBankBin(fp.bass.maxHz, banks.high));
    int midEnd   = std::min(nHigh, hzToBankBin(fp.mids.maxHz, banks.high));
    int highEnd  = std::min(nHigh, hzToBankBin(fp.high.maxHz, banks.high));

    double raw[4] = {};
    raw[0] = averageMel(m_melHighProcessed, 0, beatEnd);
    raw[1] = averageMel(m_melHighProcessed, beatEnd, bassEnd);

    raw[2] = averageMel(m_melHighProcessed, bassEnd, midEnd);

    raw[3] = averageMel(m_melHighProcessed, midEnd, highEnd);

    // Smooth with per-band ExpFilter
    const FreqPowerBandConfig *cfg[4] = { &fp.beat, &fp.bass, &fp.mids, &fp.high };
    for (int i = 0; i < 4; i++)
    {
        raw[i] = std::clamp(raw[i], 0.0, 1.0);
        m_freqPowerRaw[i] = raw[i];
        double a = (raw[i] > m_freqPower[i]) ? cfg[i]->rise : cfg[i]->decay;
        m_freqPower[i] = a * raw[i] + (1.0 - a) * m_freqPower[i];
    }
}

void AudioChannel::updateEnvelopes(const AudioFrame & /*frame*/, double dtMs)
{
    // Drive Low/Mid/High envelopes from the master-mel freq_power slots
    // (single AGC), matching LedFx's lows_power/mids_power/high_power.
    double rawBand[kBandCount] = {};
    rawBand[0] = std::min(1.0, (m_freqPower[0] + m_freqPower[1]) * 0.5);
    rawBand[1] = std::min(1.0, m_freqPower[2]);
    rawBand[2] = std::min(1.0, m_freqPower[3]);

    for (int i = 0; i < kBandCount; i++)
    {
        m_bandValues[i] = m_noiseGateClosed ? 0.0 : rawBand[i];

        const double tauMs = (m_bandValues[i] > m_envSmoothed[i]) ?
            m_config.envelope.attackMs : m_config.envelope.releaseMs;
        m_envSmoothed[i] += alpha(dtMs, tauMs) * (m_bandValues[i] - m_envSmoothed[i]);
    }

    // Diagnostic: trace Low/Mid/High through the pipeline every ~0.3s
#ifdef AUDIO_DEBUG
    static int _bandDbg = 0;
    if (++_bandDbg >= 25)
    {
        _bandDbg = 0;
        if (m_freqPower[0] > 0.0 || m_freqPower[1] > 0.0 ||
            m_freqPower[2] > 0.0 || m_freqPower[3] > 0.0 || !m_noiseGateClosed)
        {
            qDebug("[BAND-DIAG] gate=%d | freqPow: beat=%.3f bass=%.3f mid=%.3f hi=%.3f"
                   " | rawBand: L=%.3f M=%.3f H=%.3f"
                   " | env: L=%.3f M=%.3f H=%.3f"
                   " | masterAgc=%.4f",
                   m_noiseGateClosed ? 1 : 0,
                   m_freqPower[0], m_freqPower[1], m_freqPower[2], m_freqPower[3],
                   rawBand[0], rawBand[1], rawBand[2],
                   m_envSmoothed[0], m_envSmoothed[1], m_envSmoothed[2],
                   m_melPost.melGain());
        }
    }
#endif
}

void AudioChannel::updateVolume(const AudioFrame &frame, double dtMs)
{
    // frame.rms / rmsDb come straight from AudioCapture (time-domain PCM
    // measurements over the whole capture buffer). No QLC+ post-processing.
    m_volumeRaw = frame.rms;
    m_volumeNormalized = m_noiseGateClosed ? 0.0 : frame.rms;
    m_volumeSmoothed += alpha(dtMs, m_config.volumeSmoothingMs) *
        (m_volumeNormalized - m_volumeSmoothed);
}

void AudioChannel::updateTriggers(double dtMs)
{
    for (int i = 0; i < kBandCount; i++)
        m_triggerValues[i] = m_envSmoothed[i];
    m_triggerValues[3] = m_volumeSmoothed;
    m_triggerValues[4] = m_currentBeat ? 1.0 : 0.0;

    // Per-band Schmitt trigger configs. Bands 0/1/2 = low/mid/high; volume
    // (idx 3) and beat (idx 4) reuse the mid-band config (sensible default
    // mid-of-the-range hysteresis; beat is binary so thresholds barely matter).
    const TriggerConfig *const cfg[kTriggerCount] = {
        &m_config.triggers.low,
        &m_config.triggers.mid,
        &m_config.triggers.high,
        &m_config.triggers.mid,
        &m_config.triggers.mid
    };

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
            if (state.heldMs >= cfg[i]->holdMs &&
                m_triggerValues[i] <= cfg[i]->lowThreshold)
            {
                state.active = false;
                state.heldMs = 0.0;
                state.cooldownMs = cfg[i]->cooldownMs;
                m_triggerReleased[i] = true;
            }
            continue;
        }

        if (!m_noiseGateClosed && state.cooldownMs <= 0.0 &&
            m_triggerValues[i] >= cfg[i]->highThreshold)
        {
            state.active = true;
            state.heldMs = 0.0;
            m_triggerFired[i] = true;
        }
    }

#ifdef AUDIO_DEBUG
    static int _trigDbg = 0;
    if (++_trigDbg >= 25)
    {
        _trigDbg = 0;
        auto st = [this](int i) { return m_triggerFired[i] ? "fire" : (m_triggerState[i].active ? "hold" : "off"); };
        qDebug().nospace() << "[AudioChannel:trig] "
            << "L=" << st(0) << " M=" << st(1) << " H=" << st(2)
            << " | vals: " << m_triggerValues[0] << " " << m_triggerValues[1]
            << " " << m_triggerValues[2]
            << " | hi(L/M/H)=" << m_config.triggers.low.highThreshold
            << "/" << m_config.triggers.mid.highThreshold
            << "/" << m_config.triggers.high.highThreshold;
    }
#endif
}

void AudioChannel::updateMelPost(const AudioFrame &frame)
{
    if (frame.aubio == nullptr)
    {
        std::fill(std::begin(m_melProcessed), std::end(m_melProcessed), 0.0);
        std::fill(std::begin(m_melNovelty), std::end(m_melNovelty), 0.0);
        std::fill(std::begin(m_melLowProcessed), std::end(m_melLowProcessed), 0.0);
        std::fill(std::begin(m_melLowNovelty),   std::end(m_melLowNovelty),   0.0);
        std::fill(std::begin(m_melMidProcessed), std::end(m_melMidProcessed), 0.0);
        std::fill(std::begin(m_melMidNovelty),   std::end(m_melMidNovelty),   0.0);
        std::fill(std::begin(m_melHighProcessed), std::end(m_melHighProcessed), 0.0);
        std::fill(std::begin(m_melHighNovelty),   std::end(m_melHighNovelty),   0.0);
        return;
    }
    m_melPost.process(frame.aubio->mel, AUBIO_MEL_BANDS,
                      m_melProcessed, m_melNovelty, m_noiseGateClosed);

    // Per-bank pipeline. Each bank's count comes from AubioProcessor; if
    // multi-mel is disabled the count is 0 and we just zero the outputs.
    auto runBank = [gateClosed = m_noiseGateClosed](
                      MelPostProcessor &p, const double *raw, int count,
                      double *processed, double *novelty,
                      int maxBands)
    {
        count = std::clamp(count, 0, maxBands);
        if (count <= 0)
        {
            std::fill(processed, processed + maxBands, 0.0);
            std::fill(novelty,   novelty   + maxBands, 0.0);
            return;
        }
        p.process(raw, count, processed, novelty, gateClosed);
        // Zero the unused tail so consumers iterating up to maxBands don't
        // see stale data when the user shrinks the band count at runtime.
        for (int i = count; i < maxBands; ++i)
        {
            processed[i] = 0.0;
            novelty[i] = 0.0;
        }
    };
    constexpr int kMax = AudioSnapshot::kMelBankBandsMax;
    runBank(m_melPostLow,  frame.aubio->melLow,  frame.aubio->melLowCount,
            m_melLowProcessed,  m_melLowNovelty,  kMax);
    runBank(m_melPostMid,  frame.aubio->melMid,  frame.aubio->melMidCount,
            m_melMidProcessed,  m_melMidNovelty,  kMax);
    runBank(m_melPostHigh, frame.aubio->melHigh, frame.aubio->melHighCount,
            m_melHighProcessed, m_melHighNovelty, kMax);
}

void AudioChannel::updateKickDetector(const AudioFrame &frame, double dtMs)
{
    m_kickFired = false;
    m_kickReleased = false;
    m_timeSinceLastBeatSec += std::max(0.0, dtMs) * 0.001;

    if (frame.aubio == nullptr || !m_config.kick.enabled)
    {
        m_kickSpike = 0.0;
        m_kickState.active = false;
        m_kickState.heldMs = 0.0;
        m_kickState.cooldownMs = 0.0;
        return;
    }

    const int lowCount = std::clamp(frame.aubio->melLowCount, 0, AudioSnapshot::kMelBankBandsMax);
    if (lowCount <= 0)
    {
        m_kickSpike = 0.0;
        m_kickState.active = false;
        m_kickState.heldMs = 0.0;
        m_kickState.cooldownMs = 0.0;
        return;
    }
    const int end = std::clamp(
        lowMelBeatEndBin(m_config.kick.beatMaxHz, m_config.aubio.melBanks.low),
        1, lowCount);

    // LedFx audio.py:1257-1259 — beat_power = np.sum(melbank);
    // melbank_max = np.max(melbank). The amplitude gate uses the per-band
    // peak, NOT the slice sum (the sum already drives the percent-diff path).
    double beatPower = 0.0;
    double melbankMax = 0.0;
    for (int i = 0; i < end; i++)
    {
        const double v = m_melLowProcessed[i];
        beatPower += v;
        if (v > melbankMax)
            melbankMax = v;
    }

    double historySum = 0.0;
    for (double v : m_beatPowerHistory)
        historySum += v;

    // LedFx audio.py:1262-1269 — percent diff uses the fixed deque capacity
    // beat_power_history_len, not the current warm-up deque size.
    const int historyLen = std::max(1, m_config.kick.beatHistoryLen);
    const double percentDiff = (historySum > 0.0)
        ? beatPower * double(historyLen) / historySum - 1.0
        : 0.0;

    // LedFx audio.py:1271 — beat_power_history.appendleft(beat_power)
    if (int(m_beatPowerHistory.size()) >= historyLen)
        m_beatPowerHistory.pop_back();
    m_beatPowerHistory.push_front(beatPower);

    // LedFx audio.py:1274-1278 — fire on (difference >= min_percent_diff)
    // AND (melbank_max >= min_amplitude) AND (refractory elapsed).
    const bool fire = percentDiff >= m_config.kick.beatMinPercentDiff &&
        melbankMax >= m_config.kick.beatMinAmplitude &&
        m_timeSinceLastBeatSec > m_config.kick.beatRefractorySec;
    if (fire)
    {
        m_kickFired = true;
        m_timeSinceLastBeatSec = 0.0;
    }

    m_kickSpike = percentDiff + 1.0;
    m_kickState.active = fire;
    m_kickState.heldMs = fire ? dtMs : 0.0;
    m_kickState.cooldownMs = std::max(0.0,
        (m_config.kick.beatRefractorySec - m_timeSinceLastBeatSec) * 1000.0);
}

void AudioChannel::buildSnapshot(const AudioFrame &frame, double dtMs)
{
    AudioSnapshot snap;
    snap.sourceId = QStringLiteral("microphone");
    snap.profileId = m_profileId;
    snap.sourceEpoch = m_sourceEpoch;
    snap.frameSequence = ++m_frameSequence;
    snap.configRevision = m_configRevision;
    snap.config = m_config;
    snap.sampleTime = frame.sampleTime;
    snap.publishTimeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    snap.available = frame.aubio != nullptr;
    snap.status = snap.available ? QStringLiteral("available") : QStringLiteral("unavailable");

    if (frame.aubio != nullptr)
    {
        const AubioResults &a = *frame.aubio;
        std::copy(a.mel, a.mel + AUBIO_MEL_BANDS, snap.mel);
        std::copy(std::begin(m_melProcessed), std::end(m_melProcessed), snap.melProcessed);
        std::copy(std::begin(m_melNovelty), std::end(m_melNovelty), snap.melNovelty);
        std::copy(a.mfcc, a.mfcc + AUBIO_MFCC_COEFFS, snap.mfcc);

        // Multi-resolution mel banks. Snapshot stores raw aubio output AND
        // the per-bank post-processed / novelty arrays produced by the 3
        // independent MelPostProcessor instances. count is 0 when the
        // multi-mel feature is disabled (consumers fall back to legacy mel).
        auto fillBank = [](AudioSnapshot::MelBankSnapshot &dst,
                           const double *raw, int rawCount,
                           const double *processed, const double *novelty,
                           const MelBankConfig::Bank &cfg)
        {
            const int n = std::clamp(rawCount, 0, AudioSnapshot::kMelBankBandsMax);
            dst.count = n;
            dst.minHz = cfg.minHz;
            dst.maxHz = cfg.maxHz;
            for (int i = 0; i < n; ++i)
            {
                dst.raw[i] = raw[i];
                dst.processed[i] = processed[i];
                dst.novelty[i] = novelty[i];
                const double step = double(i + 1) / double(n + 1);
                dst.centersHz[i] = std::floor(double(float(mattMelToHz(
                    hzToMattMel(cfg.minHz) +
                    (hzToMattMel(cfg.maxHz) - hzToMattMel(cfg.minHz)) * step))));
            }
            // Tail is already zero-initialized in MelBankSnapshot's defaults,
            // but explicit zero here protects against carry-over from a
            // previous larger band count.
            for (int i = n; i < AudioSnapshot::kMelBankBandsMax; ++i)
            {
                dst.raw[i] = 0.0;
                dst.processed[i] = 0.0;
                dst.novelty[i] = 0.0;
            }
        };
        fillBank(snap.melLow,  a.melLow,  a.melLowCount,
                 m_melLowProcessed,  m_melLowNovelty,
                 m_config.aubio.melBanks.low);
        fillBank(snap.melMid,  a.melMid,  a.melMidCount,
                 m_melMidProcessed,  m_melMidNovelty,
                 m_config.aubio.melBanks.mid);
        fillBank(snap.melHigh, a.melHigh, a.melHighCount,
                 m_melHighProcessed, m_melHighNovelty,
                 m_config.aubio.melBanks.high);
        snap.melLow.gain = m_melPostLow.melGain();
        snap.melMid.gain = m_melPostMid.melGain();
        snap.melHigh.gain = m_melPostHigh.melGain();

        snap.music.bpm = a.bpm;
        snap.music.beatConfidence = a.beatConfidence;
        snap.music.tatum = a.tatum;
        snap.music.beatPhase = a.beatPhase;
        snap.music.valid = a.tempoValid;
        snap.music.beatInBar = a.beatInBar;
        snap.music.beatsPerBar = std::clamp(m_config.aubio.beatsPerBar, 1, 8);
        snap.music.barPhase = std::clamp(a.barPhase / snap.music.beatsPerBar, 0.0, 1.0);
        snap.downbeatFired = a.barWrap;
        const bool onsets[] = {a.onsets.energy, a.onsets.hfc, a.onsets.complex,
            a.onsets.phase, a.onsets.wphase, a.onsets.specdiff, a.onsets.kl,
            a.onsets.mkl, a.onsets.specflux};
        m_events.onset += onsets[std::clamp(m_config.aubio.onsetMethodIndex, 0, 8)] ? 1 : 0;
        m_events.beat += a.beat ? 1 : 0;
        m_events.bar += a.barWrap ? 1 : 0;

        const int n = std::min<int>(a.tssBinCount, AubioResults::kMaxTssBins);
        snap.tss.binCount = n;
        for (int i = 0; i < n; i++)
        {
            snap.tss.transientNorm[i] = a.tssTransientNorm[i];
            snap.tss.steadyNorm[i] = a.tssSteadyNorm[i];
        }

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
        for (int i = 0; i < AUBIO_ONSET_METHODS; i++)
        {
            snap.onsets.descriptors[i] = a.onsetDescriptors[i];
            snap.onsets.thresholdedDescriptors[i] = a.onsetThresholdedDescriptors[i];
        }

        snap.pitch.hz = a.pitchHz;
        snap.pitch.value = a.pitchValue;
        snap.pitch.unit = m_config.aubio.pitchUnit;
        snap.pitch.confidence = a.pitchConfidence;

        snap.note.midi = a.noteMidi;
        snap.note.velocity = a.noteVelocity;
        snap.note.noteOn = a.noteOn;
        snap.note.noteOff = a.noteOff;
    }

    // m_freqPower[] already computed by updateFreqPower() earlier in the frame.
    snap.beatPower = std::min(1.0, m_freqPower[0]);
    snap.bassPower = std::min(1.0, m_freqPower[1]);
    snap.lows = std::min(1.0, (m_freqPower[0] + m_freqPower[1]) * 0.5);
    snap.mids = std::min(1.0, m_freqPower[2]);
    snap.highs = std::min(1.0, m_freqPower[3]);
    std::copy_n(m_freqPowerRaw, 4, snap.powersRaw);

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
        m_triggerValues[3],
        m_triggerState[3].active,
        m_triggerFired[3],
        m_triggerReleased[3],
        m_triggerState[3].heldMs,
        m_triggerState[3].cooldownMs
    };
    snap.beatTrigger =
    {
        m_triggerValues[4],
        m_triggerState[4].active,
        m_triggerFired[4],
        m_triggerReleased[4],
        m_triggerState[4].heldMs,
        m_triggerState[4].cooldownMs
    };
    snap.kickTrigger =
    {
        m_kickSpike,
        m_kickState.active,
        m_kickFired,
        m_kickReleased,
        m_kickState.heldMs,
        m_kickState.cooldownMs
    };

    snap.volume.raw = m_volumeRaw;
    snap.volume.smoothed = m_volumeSmoothed;
    snap.volume.normalized = m_volumeNormalized;
    // The gate meter remains observable below threshold.
    snap.volume.volumeNorm = m_gateVolumeSmoothed;

    snap.music.beat = frame.beatDetected;
    snap.features.rmsDb = frame.rmsDb;
    snap.features.peakDb = frame.peakDb;
    snap.features.crestFactor = frame.crestFactor;

    // Spectral flatness from the post-processed mel: geometric_mean /
    // arithmetic_mean over all bands with magnitude > epsilon. 1.0 = flat
    // (white noise), -> 0 for a single tone. Cheap (40 logs per hop).
    {
        double sum = 0.0;
        double logSum = 0.0;
        int n = 0;
        constexpr double kEps = 1e-12;
        for (int i = 0; i < AUBIO_MEL_BANDS; ++i)
        {
            const double v = m_melProcessed[i];
            if (v > kEps)
            {
                sum += v;
                logSum += std::log(v);
                ++n;
            }
        }
        if (n > 0 && sum > kEps)
        {
            const double geom = std::exp(logSum / double(n));
            const double arith = sum / double(n);
            snap.features.flatness = std::clamp(geom / arith, 0.0, 1.0);
        }
        else
        {
            snap.features.flatness = 0.0;
        }
    }

    snap.melAgcGain = m_melPost.melGain();
    snap.audioDtMs = dtMs;
    snap.brightnessFloor = m_config.brightnessFloor;
    snap.noiseGateClosed = m_noiseGateClosed;

    m_events.kick += m_kickFired ? 1 : 0;
    snap.events = m_events;
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
