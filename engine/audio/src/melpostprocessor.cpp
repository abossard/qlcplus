/*
  Q Light Controller Plus
  melpostprocessor.cpp

  Copyright (c) Massimo Callegari

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include "melpostprocessor.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr double kAgcEpsilon = 1e-4;

    void gaussianBlur1D(const double *in, double *out, int n,
                        const double *kernel, int kernelSize)
    {
        if (kernelSize <= 0)
        {
            std::copy(in, in + n, out);
            return;
        }

        const int radius = kernelSize / 2;
        for (int i = 0; i < n; i++)
        {
            double acc = 0.0;
            for (int k = 0; k < kernelSize; k++)
            {
                const int j = i + k - radius;
                if (j >= 0 && j < n)
                    acc += in[j] * kernel[k];
                // out-of-range samples contribute 0 (NumPy mode='same').
            }
            out[i] = acc;
        }
    }
}

MelPostProcessor::MelPostProcessor()
{
}

void MelPostProcessor::setConfig(const Config &cfg)
{
    m_config = cfg;
    if (std::abs(cfg.gaussianSigma - m_kernelSigma) > 1e-9)
        rebuildKernel();
}

void MelPostProcessor::reset()
{
    std::fill(m_smoothed.begin(), m_smoothed.end(), 0.0);
    std::fill(m_common.begin(), m_common.end(), 0.0);
    std::fill(m_diff.begin(), m_diff.end(), 0.0);
    m_melGain = 1e-10;
    m_smoothInitialized = false;
    m_commonInitialized = false;
    m_diffInitialized = false;
}

void MelPostProcessor::ensureSize(int count)
{
    if (int(m_smoothed.size()) != count)
    {
        m_smoothed.assign(count, 0.0);
        m_common.assign(count, 0.0);
        m_diff.assign(count, 0.0);
        m_powered.assign(count, 0.0);
        m_blurred.assign(count, 0.0);
        m_smoothInitialized = false;
        m_commonInitialized = false;
        m_diffInitialized = false;
    }
}

void MelPostProcessor::rebuildKernel()
{
    const double sigma = std::max(0.01, m_config.gaussianSigma);
    int radius = std::max(1, int(std::round(4.0 * sigma)));
    // Cap radius so kernel fits comfortably inside a 40-band mel array.
    radius = std::min(radius, 19);

    m_kernelSize = 2 * radius + 1;
    m_gaussianKernel.assign(m_kernelSize, 0.0);

    double sum = 0.0;
    for (int i = 0; i < m_kernelSize; i++)
    {
        const double x = double(i - radius);
        const double w = std::exp(-0.5 * (x * x) / (sigma * sigma));
        m_gaussianKernel[i] = w;
        sum += w;
    }
    if (sum > 0.0)
    {
        for (int i = 0; i < m_kernelSize; i++)
            m_gaussianKernel[i] /= sum;
    }
    m_kernelSigma = m_config.gaussianSigma;
}

void MelPostProcessor::process(const double *rawMel, int count,
                               double *processedMel, double *noveltyMel,
                               bool noiseGateClosed)
{
    if (count <= 0)
        return;
    if (rawMel == nullptr || processedMel == nullptr)
    {
        // Zero outputs so callers don't present a stale frame as live audio.
        if (processedMel != nullptr)
            std::fill_n(processedMel, count, 0.0);
        if (noveltyMel != nullptr)
            std::fill_n(noveltyMel, count, 0.0);
        return;
    }

    if (!m_config.enabled)
    {
        // Bypass: copy raw -> processed, zero novelty for predictable output.
        std::copy(rawMel, rawMel + count, processedMel);
        if (noveltyMel != nullptr)
            std::fill(noveltyMel, noveltyMel + count, 0.0);
        return;
    }

    ensureSize(count);
    if (m_kernelSigma < 0.0)
        rebuildKernel();

    // LedFx audio.py:1027-1041 — when the gate is closed, zero the FFT input
    // but still run the full filter chain (LedFx melbank.py:380-403 updates
    // common/diff filters every frame regardless of input). This lets
    // m_smoothed, m_common and m_diff decay naturally toward zero and
    // m_melGain decay toward kAgcEpsilon, instead of being frozen at their
    // last live values. Fix 3's smoothed gate prevents the gate from
    // reopening on noise while m_melGain divisor is small.

    // 1. Power scaling — peak isolation.
    // When gate is closed, fill m_powered with zeros directly (no heap alloc).
    if (noiseGateClosed)
    {
        std::fill(m_powered.begin(), m_powered.begin() + count, 0.0);
    }
    else if (std::abs(m_config.powerFactor - 1.0) < 1e-9)
    {
        std::copy(rawMel, rawMel + count, m_powered.data());
    }
    else
    {
        for (int i = 0; i < count; i++)
        {
            const double v = std::max(0.0, rawMel[i]);
            m_powered[i] = std::pow(v, m_config.powerFactor);
        }
    }

    // 2. Gaussian-blur for AGC reference.
    gaussianBlur1D(m_powered.data(), m_blurred.data(), count,
                   m_gaussianKernel.data(), m_kernelSize);

    double currentPeak = 0.0;
    for (int i = 0; i < count; i++)
        if (m_blurred[i] > currentPeak) currentPeak = m_blurred[i];
    if (currentPeak < kAgcEpsilon)
        currentPeak = kAgcEpsilon;

    // 3. Temporal mel_gain (LedFx ExpFilter — alphas now from config).
    // Slow decay holds gain high during quiet passages so soft bands still
    // show detail; fast rise prevents clipping on transients. Each
    // MelPostProcessor instance owns its own m_melGain, giving each bank
    // independent normalization (matches LedFx per-melbank mel_gain).
    const double agcRise = std::clamp(m_config.agcRise, 0.0, 1.0);
    const double agcDecay = std::clamp(m_config.agcDecay, 0.0, 1.0);
    const double melGainAlpha = (currentPeak > m_melGain) ? agcRise : agcDecay;
    m_melGain = melGainAlpha * currentPeak + (1.0 - melGainAlpha) * m_melGain;
    if (m_melGain < kAgcEpsilon)
        m_melGain = kAgcEpsilon;

    // 4. Per-band asymmetric ExpFilter smoothing on AGC-normalized values.
    const double aRise = std::clamp(m_config.smoothRise, 0.0, 1.0);
    const double aDecay = std::clamp(m_config.smoothDecay, 0.0, 1.0);

    for (int i = 0; i < count; i++)
    {
        double v = m_powered[i] / m_melGain;
        if (!std::isfinite(v))
            v = 0.0;

        if (!m_smoothInitialized)
        {
            m_smoothed[i] = v;
        }
        else
        {
            const double alpha = (v > m_smoothed[i]) ? aRise : aDecay;
            m_smoothed[i] = alpha * v + (1.0 - alpha) * m_smoothed[i];
        }
        processedMel[i] = m_smoothed[i];
    }
    m_smoothInitialized = true;

    // 5. Novelty: diff_filter(processed - common_filter(processed)).
    if (noveltyMel != nullptr)
    {
        const double cRise = std::clamp(m_config.commonRise, 0.0, 1.0);
        const double cDecay = std::clamp(m_config.commonDecay, 0.0, 1.0);
        const double dRise = std::clamp(m_config.diffRise, 0.0, 1.0);
        const double dDecay = std::clamp(m_config.diffDecay, 0.0, 1.0);
        for (int i = 0; i < count; i++)
        {
            if (!m_commonInitialized)
                m_common[i] = processedMel[i];
            else
            {
                // LedFx melbank.py:400 — common_filter.update(filter_banks)
                const double cAlpha = (processedMel[i] > m_common[i]) ? cRise : cDecay;
                m_common[i] = cAlpha * processedMel[i] + (1.0 - cAlpha) * m_common[i];
            }

            // LedFx melbank.py:401-403 — diff_filter.update(filter_banks - common_filter.value)
            const double rawNovelty = processedMel[i] - m_common[i];
            if (!m_diffInitialized)
                m_diff[i] = rawNovelty;
            else
            {
                const double dAlpha = (rawNovelty > m_diff[i]) ? dRise : dDecay;
                m_diff[i] = dAlpha * rawNovelty + (1.0 - dAlpha) * m_diff[i];
            }
            noveltyMel[i] = m_diff[i];
        }
        m_commonInitialized = true;
        m_diffInitialized = true;
    }
}
