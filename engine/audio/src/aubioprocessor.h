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

    /** Process a buffer of mono int16 PCM samples.
     *  bufferSize should be a multiple of hopSize (512).
     *  Results are aggregated across all hops in the buffer. */
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

    /** Total mono samples fed into aubio since last initialize(). Used to
     *  derive beatPhase from aubio's sample-clock view of the world. */
    uint64_t m_totalSamplesProcessed = 0;

    aubio_pvoc_t *m_pvoc = nullptr;
    aubio_tempo_t *m_tempo = nullptr;
    aubio_pitch_t *m_pitch = nullptr;
    aubio_notes_t *m_notes = nullptr;
    aubio_mfcc_t *m_mfcc = nullptr;
    aubio_filterbank_t *m_filterbank = nullptr;
    aubio_tss_t *m_tss = nullptr;

    static constexpr int kOnsetMethodCount = 9;
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
    fvec_t *m_mfccOut = nullptr;
    fvec_t *m_descOut = nullptr;
    cvec_t *m_transGrain = nullptr;
    cvec_t *m_steadGrain = nullptr;
};
