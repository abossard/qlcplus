#pragma once

#include <cstdint>
#include <QMetaType>

static constexpr int AUBIO_MEL_BANDS = 40;
static constexpr int AUBIO_MFCC_COEFFS = 13;
static constexpr int AUBIO_ONSET_METHODS = 9;

struct AubioResults
{
    // Mel filterbank (40 bands, 0-1 normalized)
    double mel[AUBIO_MEL_BANDS] = {};

    // MFCC (13 coefficients)
    double mfcc[AUBIO_MFCC_COEFFS] = {};

    // Spectral descriptors
    double centroidHz = 0.0;
    double spread = 0.0;
    double rolloffHz = 0.0;
    double flux = 0.0;
    double hfc = 0.0;

    // Pitch
    double pitchHz = 0.0;
    double pitchConfidence = 0.0;

    // Tempo / beat
    bool beat = false;
    double bpm = 0.0;
    double beatConfidence = 0.0;
    bool tatum = false;
    /** Phase within the current beat in [0,1). 0 = on a beat, 0.5 = halfway to next. */
    double beatPhase = 0.0;

    // Onset detection (9 methods)
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
        int voteCount = 0;
    } onsets;

    // Notes
    double noteMidi = 0.0;
    double noteVelocity = 0.0;
    bool noteOn = false;
    bool noteOff = false;

    // Volume (RMS from aubio's perspective)
    double rms = 0.0;
    double peak = 0.0;

    // Transient/Steady separation (per-buffer aggregates)
    double transientEnergy = 0.0;  // max-over-hops of tE / (tE + sE)
    double steadyEnergy = 0.0;     // max-over-hops of sE / (tE + sE)
    double transientRatio = 0.0;   // last-hop tE / (tE + sE) — 0 = all steady, 1 = all transient
};

Q_DECLARE_METATYPE(AubioResults)
