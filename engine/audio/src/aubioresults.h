#pragma once

#include "audiochannelconfig.h"  // for kMaxMelBands

#include <cstdint>
#include <QMetaType>

static constexpr int AUBIO_MEL_BANDS = 40;
static constexpr int AUBIO_MFCC_COEFFS = 13;
static constexpr int AUBIO_ONSET_METHODS = 9;

// Upper bound for any single multi-resolution mel bank. Aliased to the single
// source of truth in audiochannelconfig.h.
static constexpr int AUBIO_MELBANK_MAX = kMaxMelBands;

/**
 * Pristine aubio output — last hop wins for all per-hop values.
 * No QLC+-side averaging, max-over-hops, clamping, normalization or scaling.
 * The only operations applied to aubio data here are float->double type
 * conversion, struct copying, and bin->Hz conversion via aubio_bintofreq().
 *
 * Range of `mel`: whatever `aubio_filterbank_set_norm/power` produces. NOT
 * guaranteed to be 0..1 — callers must NOT assume any range.
 */
struct AubioResults
{
    // Mel filterbank — last hop, raw aubio_filterbank_do output.
    double mel[AUBIO_MEL_BANDS] = {};

    // Multi-resolution mel banks (matt_mel triangles, 3 banks share m_fftGrain).
    // *Count is the valid prefix length actually filled this hop (0 when the
    // multi-mel feature is disabled in MelBankConfig). Bands beyond *Count
    // are zero-initialized.
    double melLow [AUBIO_MELBANK_MAX] = {};
    double melMid [AUBIO_MELBANK_MAX] = {};
    double melHigh[AUBIO_MELBANK_MAX] = {};
    int    melLowCount  = 0;
    int    melMidCount  = 0;
    int    melHighCount = 0;

    // MFCC — last hop, raw aubio_mfcc_do output.
    double mfcc[AUBIO_MFCC_COEFFS] = {};

    // Spectral descriptors — last hop, raw aubio_specdesc_do output.
    double centroidHz = 0.0;
    double spread = 0.0;
    double rolloffHz = 0.0;
    double flux = 0.0;
    double hfc = 0.0;

    // Pitch — last hop, raw aubio_pitch_do output.
    double pitchHz = 0.0;
    double pitchConfidence = 0.0;

    // Tempo / beat — raw aubio_tempo_get_*() output.
    bool beat = false;
    double bpm = 0.0;
    double beatConfidence = 0.0;
    bool tatum = false;
    double beatPhase = 0.0;  // 0→1 sawtooth ramp synced to BPM (computed in AubioProcessor).
    double barPhase = 0.0;   // 0→beatsPerBar ramp: beat index in bar + beatPhase.

    // Onset detection (9 methods) — last hop, raw aubio_onset_do output.
    // OR-aggregated across all hops in a process() pass: if any hop fires the
    // detector, the flag stays true for the whole pass.
    struct {
        bool energy = false;
        bool hfc = false;
        bool complex = false;
        bool phase = false;
        bool wphase = false;
        bool specdiff = false;
        bool kl = false;
        bool mkl = false;
        bool specflux = false;
    } onsets;

    // Onset diagnostics (last hop wins) — raw aubio_onset_get_descriptor /
    // aubio_onset_get_thresholded_descriptor outputs, indexed by detector
    // (same order as the onsets struct above).
    double onsetDescriptors[AUBIO_ONSET_METHODS] = {};
    double onsetThresholdedDescriptors[AUBIO_ONSET_METHODS] = {};

    // Notes — raw aubio_notes_do output.
    double noteMidi = 0.0;
    double noteVelocity = 0.0;
    bool noteOn = false;
    bool noteOff = false;

    // Transient/Steady separation — raw aubio_tss_do cvec norm arrays from the
    // last hop. Sized win/2+1; `tssBinCount` reports how many entries are
    // valid. Frequency of bin i = aubio_bintofreq(i, sampleRate, winSize).
    static constexpr int kMaxTssBins = 1025; // covers up to win=2048
    double tssTransientNorm[kMaxTssBins] = {};
    double tssSteadyNorm[kMaxTssBins] = {};
    int tssBinCount = 0;
};

Q_DECLARE_METATYPE(AubioResults)
