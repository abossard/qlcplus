#pragma once

#include "aubioresults.h"
#include "audiochannelconfig.h"
#include <aubio/aubio.h>
#include <cstdint>
#include <QMutex>
#include <QMutexLocker>

class AubioProcessor
{
public:
    AubioProcessor();
    ~AubioProcessor();

    AubioProcessor(const AubioProcessor &) = delete;
    AubioProcessor &operator=(const AubioProcessor &) = delete;

    /** Initialize with sample rate. Must be called before process(). */
    void initialize(uint32_t sampleRate);

    /** Release all aubio objects. */
    void release();

    /** Process exactly one hop (hopSize() = 512) of mono int16 PCM samples.
     *  This matches aubio's own example pattern (one hop in, one report out;
     *  see aubio examples/utils.c examples_common_process). bufferSize must
     *  equal hopSize(); extra samples are ignored, fewer is a no-op. */
    void process(const int16_t *monoSamples, int bufferSize);

    /** Get the last computed results. Thread-safe for single-writer/single-reader. */
    const AubioResults &results() const { return m_results; }

    bool isInitialized() const { return m_initialized; }

    /** Stage a new AubioConfig from any thread. The config is picked up at the
     *  start of the next process() pass and applied either via aubio setters
     *  (parameter-only updates) or by destroying/recreating algorithm objects
     *  when an algorithm string actually changed. */
    void setPendingConfig(const AubioConfig &cfg);

    static constexpr uint32_t hopSize() { return 512; }
    static constexpr uint32_t windowSize() { return 1024; }

    // Onset detection methods — single source of truth. Indices match the
    // bool/override arrays in AubioConfig and are persisted by audioprofile.cpp
    // and surfaced to QML / JS via vcaudiotriggers + rgbscriptv4. Order MUST
    // remain stable across releases (XML round-trip).
    static constexpr int kOnsetMethodCount = 9;
    static constexpr const char *kOnsetMethodNames[kOnsetMethodCount] = {
        "energy", "hfc", "complex", "phase", "wphase",
        "specdiff", "kl", "mkl", "specflux"
    };

    /** Read aubio's per-method default onset parameters. Lives as a free
     *  function in audiochannelconfig.h so callers don't need to include
     *  this header (and thus don't pull in <aubio/aubio.h>). */

private:
    void processHop();
    void resetResults();

    void applyPendingConfig();
    void applyParamUpdates(const AubioConfig &oldCfg, const AubioConfig &newCfg);
    bool needsFullRebuild(const AubioConfig &oldCfg, const AubioConfig &newCfg) const;

    bool m_initialized = false;
    uint32_t m_sampleRate = 44100;

    AubioConfig m_config;
    AubioConfig m_pendingConfig;
    bool m_hasPendingConfig = false;
    QMutex m_configMutex;

    AubioResults m_results;

    aubio_pvoc_t *m_pvoc = nullptr;
    aubio_filter_t *m_preEmphasis = nullptr;
    aubio_tempo_t *m_tempo = nullptr;
    aubio_pitch_t *m_pitch = nullptr;
    aubio_notes_t *m_notes = nullptr;
    aubio_mfcc_t *m_mfcc = nullptr;
    aubio_filterbank_t *m_filterbank = nullptr;
    aubio_filterbank_t *m_filterbankLow  = nullptr;
    aubio_filterbank_t *m_filterbankMid  = nullptr;
    aubio_filterbank_t *m_filterbankHigh = nullptr;
    aubio_tss_t *m_tss = nullptr;

    aubio_onset_t *m_onsets[kOnsetMethodCount] = {};

    aubio_specdesc_t *m_descCentroid = nullptr;
    aubio_specdesc_t *m_descSpread = nullptr;
    aubio_specdesc_t *m_descRolloff = nullptr;
    aubio_specdesc_t *m_descFlux = nullptr;
    aubio_specdesc_t *m_descHfc = nullptr;

    fvec_t *m_hopBuffer = nullptr;
    cvec_t *m_fftGrain = nullptr;
    fvec_t *m_onsetOut = nullptr;
    fvec_t *m_tempoOut = nullptr;
    fvec_t *m_pitchOut = nullptr;
    fvec_t *m_notesOut = nullptr;
    fvec_t *m_melOut = nullptr;
    fvec_t *m_melLowOut  = nullptr;
    fvec_t *m_melMidOut  = nullptr;
    fvec_t *m_melHighOut = nullptr;
    fvec_t *m_mfccOut = nullptr;
    fvec_t *m_descOut = nullptr;
    cvec_t *m_transGrain = nullptr;
    cvec_t *m_steadGrain = nullptr;

    // Beat phase tracking. Persists across hops; only reset in
    // initialize()/release(). Per RD: NOT touched by resetResults() — that
    // would zero the period between every process() call and break the ramp.
    double m_beatPeriodS = 0.0;        // seconds per beat from aubio_tempo_get_period_s()
    double m_lastBeatTimeS = 0.0;      // stream-time of last detected beat (aubio_tempo_get_last_s())
    uint64_t m_processedSamples = 0;   // total mono samples processed → currentTimeS
    uint32_t m_hopsSinceBeat = 0;      // gate phase to 0 after N silent hops
    int m_barBeatCount = -1;
    int m_beatsPerBar = 4;
};
