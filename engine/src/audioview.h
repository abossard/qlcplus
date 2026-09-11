#ifndef AUDIOVIEW_H
#define AUDIOVIEW_H

#include <QString>
#include <QVariantMap>
#include <QVector>
#include <array>
#include <cstdint>
#include <climits>

struct AudioSnapshot;

bool audioSnapshotAvailable(const AudioSnapshot &snapshot, int64_t nowNs);

// Scalar/bank render data deliberately excludes the optional FFT/TSS diagnostics.
struct AudioRenderView
{
    struct Bank
    {
        QVector<double> centersHz, processed, novelty;
        double minHz = 0, maxHz = 0;
    };
    QString sourceId, status = QStringLiteral("unavailable");
    uint32_t profileId = UINT_MAX;
    uint64_t sourceEpoch = 0, frameSequence = 0, configRevision = 0, sampleTime = 0;
    int64_t renderTimeNs = 0, publishTimeNs = 0;
    bool available = false, gateOpen = false, tempoValid = false;
    double staleAgeMs = 0, rawRms = 0, rmsDb = -96, peakDb = -96, volume = 0;
    double beat = 0, bass = 0, low = 0, mid = 0, high = 0, onsetIntensity = 0;
    double bpm = 0, confidence = 0, phase = 0, barPhase = 0;
    int beatInBar = 0, beatsPerBar = 4;
    double deltaSeconds = 0, elapsedBeats = 0;
    std::array<uint64_t, 4> counters{}, deltas{}; // onset, tempo beat, kick, bar
    std::array<Bank, 3> banks;

    static int64_t nowNs();
    static AudioRenderView fromSnapshot(const AudioSnapshot &snapshot, int64_t nowNs);
};

class AudioEventCursor
{
public:
    void advance(AudioRenderView &view);
    void reset() { *this = AudioEventCursor(); }
private:
    QString m_sourceId;
    uint32_t m_profileId = UINT_MAX;
    uint64_t m_epoch = 0;
    std::array<uint64_t, 4> m_counters{};
    int64_t m_timeNs = 0;
    bool m_initialized = false, m_available = false;
};

QVariantMap audioViewToVariant(const AudioRenderView &view);
double audioFrequency(const AudioRenderView &view, int index, int count);
double audioSpectrum(const AudioRenderView &view, int index, int count);

#endif
