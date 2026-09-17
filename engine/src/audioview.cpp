#include "audioview.h"
#include "audiosnapshot.h"

#include <algorithm>
#include <chrono>
#include <cmath>

int64_t AudioRenderView::nowNs()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

bool audioSnapshotAvailable(const AudioSnapshot &snapshot, int64_t nowNs)
{
    return snapshot.available && (snapshot.publishTimeNs <= 0
        || nowNs - snapshot.publishTimeNs <= 250000000);
}

AudioRenderView AudioRenderView::fromSnapshot(const AudioSnapshot &s, int64_t now)
{
    AudioRenderView v;
    v.sourceId = s.sourceId;
    v.profileId = s.profileId;
    v.sourceEpoch = s.sourceEpoch;
    v.frameSequence = s.frameSequence;
    v.configRevision = s.configRevision;
    v.sampleTime = s.sampleTime;
    v.renderTimeNs = now;
    v.publishTimeNs = s.publishTimeNs;
    v.staleAgeMs = s.publishTimeNs > 0 ? std::max(0.0, double(now - s.publishTimeNs) / 1e6) : 0;
    v.available = audioSnapshotAvailable(s, now);
    v.status = s.available && !v.available ? QStringLiteral("stale") : s.status;
    v.counters = {s.events.onset, s.events.beat, s.events.kick, s.events.bar};
    const AudioSnapshot::MelBankSnapshot *banks[] = {&s.melLow, &s.melMid, &s.melHigh};
    for (int i = 0; i < 3; ++i)
    {
        const auto &src = *banks[i];
        auto &dst = v.banks[i];
        const int count = std::clamp(src.count, 0, AudioSnapshot::kMelBankBandsMax);
        dst.minHz = src.minHz;
        dst.maxHz = src.maxHz;
        for (int j = 0; j < count; ++j)
        {
            dst.centersHz.append(src.centersHz[j]);
            dst.processed.append(v.available ? src.processed[j] : 0);
            dst.novelty.append(v.available ? src.novelty[j] : 0);
        }
    }
    if (!v.available)
        return v;
    v.gateOpen = !s.noiseGateClosed;
    v.rawRms = s.volume.raw;
    v.rmsDb = s.features.rmsDb;
    v.peakDb = s.features.peakDb;
    v.volume = s.volume.volumeNorm;
    v.beat = s.beatPower;
    v.bass = s.bassPower;
    v.low = s.lows;
    v.mid = s.mids;
    v.high = s.highs;
    if (std::isfinite(s.powersRaw[0]))
        v.rawBeat = s.powersRaw[0];
    if (std::isfinite(s.powersRaw[1]))
        v.rawBass = s.powersRaw[1];
    v.rawLow = 0.5 * (v.rawBeat + v.rawBass);
    if (std::isfinite(s.powersRaw[2]))
        v.rawMid = s.powersRaw[2];
    if (std::isfinite(s.powersRaw[3]))
        v.rawHigh = s.powersRaw[3];
    v.pitchValid = std::isfinite(s.pitch.hz) && s.pitch.hz > 0;
    if (v.pitchValid)
    {
        v.pitchHz = s.pitch.hz;
        v.pitchMidi = 69.0 + 12.0 * std::log2(s.pitch.hz / 440.0);
        if (std::isfinite(s.pitch.confidence))
            v.pitchConfidence = s.pitch.confidence;
    }
    v.onsetIntensity = s.onsets.thresholdedDescriptors[
        std::clamp(s.config.aubio.onsetMethodIndex, 0, AUBIO_ONSET_METHODS - 1)];
    v.tempoValid = s.music.valid && std::isfinite(s.music.bpm) && s.music.bpm > 0;
    v.bpm = s.music.bpm;
    v.confidence = s.music.beatConfidence;
    v.phase = s.music.beatPhase;
    v.barPhase = s.music.barPhase;
    v.beatInBar = s.music.beatInBar;
    v.beatsPerBar = std::max(1, s.music.beatsPerBar);
    return v;
}

void AudioEventCursor::advance(AudioRenderView &v)
{
    const bool sameSource = m_initialized && m_sourceId == v.sourceId
        && m_profileId == v.profileId && m_epoch == v.sourceEpoch;
    const bool transientUnavailable = m_initialized
        && !v.available
        && v.profileId == m_profileId
        && (v.sourceId.isEmpty() || v.status == QStringLiteral("reset"));
    v.deltaSeconds = (sameSource || transientUnavailable)
        ? std::max(0.0, double(v.renderTimeNs - m_timeNs) / 1e9)
        : 0;
    v.elapsedBeats = v.tempoValid ? v.deltaSeconds * v.bpm / 60 : 0;
    for (size_t i = 0; i < m_counters.size(); ++i)
        v.deltas[i] = sameSource && m_available && v.available && v.counters[i] >= m_counters[i]
            ? v.counters[i] - m_counters[i] : 0;
    if (!transientUnavailable)
    {
        m_sourceId = v.sourceId;
        m_profileId = v.profileId;
        m_epoch = v.sourceEpoch;
        m_counters = v.counters;
    }
    m_timeNs = v.renderTimeNs;
    m_available = v.available;
    m_initialized = true;
}

QVariantMap audioViewToVariant(const AudioRenderView &v)
{
    const double visualBpm = v.tempoValid ? v.bpm : 120;
    QVariantMap counters, deltas, banks;
    const char *eventNames[] = {"onset", "beat", "kick", "bar"};
    for (size_t i = 0; i < v.counters.size(); ++i)
    {
        counters.insert(eventNames[i], QVariant::fromValue(qulonglong(v.counters[i])));
        deltas.insert(eventNames[i], QVariant::fromValue(qulonglong(v.deltas[i])));
    }
    const char *bankNames[] = {"low", "mid", "full"};
    for (int i = 0; i < 3; ++i)
    {
        QVariantList centers, processed, novelty;
        for (int j = 0; j < v.banks[i].processed.size(); ++j)
        {
            centers.append(v.banks[i].centersHz[j]);
            processed.append(v.banks[i].processed[j]);
            novelty.append(v.banks[i].novelty[j]);
        }
        banks.insert(bankNames[i], QVariantMap{
            {"count", processed.size()}, {"centersHz", centers},
            {"minHz", v.banks[i].minHz}, {"maxHz", v.banks[i].maxHz},
            {"processed", processed}, {"novelty", novelty}});
    }
    return {
        {"version", 6}, {"beat", v.beat}, {"bass", v.bass}, {"low", v.low},
        {"mid", v.mid}, {"high", v.high}, {"onset", v.deltas[0] > 0},
        {"onsetIntensity", v.onsetIntensity}, {"beatFired", v.deltas[1] > 0},
        {"kickFired", v.deltas[2] > 0}, {"downbeat", v.deltas[3] > 0},
        {"bpm", visualBpm}, {"phase", v.phase}, {"barPhase", v.barPhase},
        {"dt", v.deltaSeconds * visualBpm / 60},
        {"cosPulse", v.tempoValid ? std::max(0.0, std::cos(v.phase * 3.141592653589793)) : 0},
        {"sourceId", v.sourceId}, {"profileId", v.profileId},
        {"sourceEpoch", QVariant::fromValue(qulonglong(v.sourceEpoch))},
        {"frameSequence", QVariant::fromValue(qulonglong(v.frameSequence))},
        {"configRevision", QVariant::fromValue(qulonglong(v.configRevision))},
        {"sampleTime", QVariant::fromValue(qulonglong(v.sampleTime))},
        {"publishTimeNs", QVariant::fromValue(qlonglong(v.publishTimeNs))},
        {"available", v.available}, {"status", v.status}, {"staleAgeMs", v.staleAgeMs},
        {"gateOpen", v.gateOpen}, {"banks", banks},
        {"volume", QVariantMap{{"rawRms", v.rawRms}, {"rmsDb", v.rmsDb},
            {"peakDb", v.peakDb}, {"normalized", v.volume}}},
        {"tempo", QVariantMap{{"valid", v.tempoValid}, {"bpm", v.bpm},
            {"confidence", v.confidence}, {"beatPhase", v.phase}, {"barPhase", v.barPhase},
            {"beatInBar", v.beatInBar}, {"beatsPerBar", v.beatsPerBar}}},
        {"powers", QVariantMap{{"raw", QVariantMap{
            {"beat", v.rawBeat}, {"bass", v.rawBass}, {"low", v.rawLow},
            {"mid", v.rawMid}, {"high", v.rawHigh}
        }}}},
        {"pitch", QVariantMap{{"valid", v.pitchValid}, {"hz", v.pitchHz},
            {"midi", v.pitchMidi}, {"confidence", v.pitchConfidence}}},
        {"events", QVariantMap{{"counters", counters}, {"delta", deltas}}},
        {"timing", QVariantMap{{"deltaSeconds", v.deltaSeconds}, {"elapsedBeats", v.elapsedBeats}}}
    };
}

double audioFrequency(const AudioRenderView &v, int index, int count)
{
    if (!v.available || count <= 0 || index < 0 || index >= count)
        return 0;
    if (count == 3)
        return std::clamp(index == 0 ? v.low : index == 1 ? v.mid : v.high, 0.0, 1.0);
    return audioSpectrum(v, index, count);
}

double audioSpectrum(const AudioRenderView &v, int index, int count)
{
    if (!v.available || count <= 0 || index < 0 || index >= count)
        return 0;
    double value = 0;
    if (!v.banks[2].processed.isEmpty())
    {
        const auto &bins = v.banks[2].processed;
        const double position = count == 1 ? 0 : double(index) * (bins.size() - 1) / (count - 1);
        const int left = int(position), right = std::min(left + 1, int(bins.size()) - 1);
        value = bins[left] + (bins[right] - bins[left]) * (position - left);
    }
    return std::isfinite(value) ? std::clamp(value, 0.0, 1.0) : 0;
}
