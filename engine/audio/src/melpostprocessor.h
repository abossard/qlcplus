/*
  Q Light Controller Plus
  melpostprocessor.h

  Copyright (c) Massimo Callegari

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#pragma once

#include <vector>

/**
 * LedFx-style mel post-processing pipeline. Self-contained DSP block with its
 * own filter memories, kept Qt-free so it can be unit-tested in isolation.
 *
 * Pipeline (when enabled):
 *   raw mel
 *     -> pow(mel, powerFactor)              # peak isolation
 *     -> gaussian blur, take max            # AGC reference
 *     -> ExpFilter mel_gain (slow decay,    # temporal AGC tracker —
 *        fast rise) tracks blurred peak     #   matches LedFx mel_gain
 *     -> divide powered mel by mel_gain     # AGC normalization
 *     -> per-band asymmetric ExpFilter      # temporal smoothing
 *     -> processedMel[]
 *
 * Novelty:
 *   processedMel - very-slow-common-filter(processedMel) -> melNovelty[]
 *
 * When `enabled` is false the processor copies raw -> processed and zeroes
 * the novelty array, preserving backward compatibility.
 */
class MelPostProcessor
{
public:
    struct Config
    {
        double powerFactor = 1.9626105055051506;
        double gaussianSigma = 1.0;
        double smoothDecay = 0.7;     // LedFx melbank.py:376
        double smoothRise = 0.99;     // LedFx melbank.py:376
        double commonDecay = 0.99;    // LedFx melbank.py:377
        double commonRise = 0.01;     // LedFx melbank.py:377
        double diffDecay = 0.15;      // LedFx melbank.py:378
        double diffRise = 0.99;       // LedFx melbank.py:378
        double agcDecay = 0.01;       // LedFx melbank.py:375 (mel_gain alpha_decay)
        double agcRise = 0.99;        // LedFx melbank.py:375 (mel_gain alpha_rise)
        bool enabled = true;          // matches MelPostConfig default; LedFx always processes

        bool operator==(const Config &o) const
        {
            return powerFactor == o.powerFactor && gaussianSigma == o.gaussianSigma
                && smoothDecay == o.smoothDecay && smoothRise == o.smoothRise
                && commonDecay == o.commonDecay && commonRise == o.commonRise
                && diffDecay == o.diffDecay && diffRise == o.diffRise
                && agcDecay == o.agcDecay && agcRise == o.agcRise
                && enabled == o.enabled;
        }
        bool operator!=(const Config &o) const { return !(*this == o); }
    };

    MelPostProcessor();

    void setConfig(const Config &cfg);
    Config config() const { return m_config; }

    /// Live AGC tracker value (LedFx `mel_gain`). The divisor applied to the
    /// powered+blurred mel before ExpFilter smoothing. Tracks the slow peak
    /// of the blurred mel. Returns 1.0 when the processor is disabled.
    double melGain() const { return m_config.enabled ? m_melGain : 1.0; }

    /// Process one mel frame. rawMel/processedMel/noveltyMel are arrays of
    /// length `count`. noveltyMel may be nullptr if novelty is not needed.
    /// A closed gate publishes zeros without advancing any filter state.
    void process(const double *rawMel, int count,
                 double *processedMel, double *noveltyMel,
                 bool noiseGateClosed = false);

    /// Reset all filter state (call when audio is restarted or config changes
    /// drastically).
    void reset();

private:
    Config m_config;
    std::vector<double> m_smoothed;   // ExpFilter state per band
    std::vector<double> m_common;     // slow common (for novelty)
    std::vector<double> m_diff;       // diff ExpFilter state (for novelty)
    std::vector<double> m_powered;    // scratch: powered mel
    std::vector<double> m_blurred;    // scratch: gaussian-blurred mel
    std::vector<double> m_gaussianKernel;
    int m_kernelSize = 0;
    double m_kernelSigma = -1.0;      // invalidation sentinel
    double m_melGain = 0.0;
    bool m_gainInitialized = false;
    bool m_smoothInitialized = false;
    bool m_commonInitialized = false;
    bool m_diffInitialized = false;

    void rebuildKernel();
    void ensureSize(int count);
};
