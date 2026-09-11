/*
  Q Light Controller Plus
  audiochannelconfig.cpp

  Copyright (c) Massimo Callegari

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include "audiochannelconfig.h"
#include <cmath>

AudioChannelConfig AudioChannelConfig::defaults()
{
    // In-class member initializers are the single source of truth for default
    // values — see EnvelopeConfig / NoiseGateConfig / TriggerConfig in
    // audiochannelconfig.h. This function intentionally adds no overrides.
    return AudioChannelConfig{};
}

QString AudioChannelConfig::validationError() const
{
    if (!std::isfinite(noiseGate.thresholdDb) || !std::isfinite(noiseGate.holdMs)
        || noiseGate.holdMs < 0.0)
        return QStringLiteral("Noise gate requires finite threshold and nonnegative hold");
    for (const auto *bank : {&aubio.melBanks.low, &aubio.melBanks.mid, &aubio.melBanks.high})
    {
        if (bank->bands < 1 || bank->bands > kMaxMelBands ||
            !std::isfinite(bank->minHz) || !std::isfinite(bank->maxHz) ||
            bank->minHz < 0.0 || bank->maxHz > 15000.0 || bank->minHz >= bank->maxHz)
            return QStringLiteral("Mel banks require 1..32 bands within 0..15000 Hz and minHz < maxHz");
    }
    for (const auto *post : {&melPost, &aubio.melBanks.low.post, &aubio.melBanks.mid.post, &aubio.melBanks.high.post})
    {
        if (!std::isfinite(post->powerFactor) || post->powerFactor <= 0.0 ||
            !std::isfinite(post->gaussianSigma) || post->gaussianSigma <= 0.0)
            return QStringLiteral("Mel exponent and Gaussian sigma must be finite and positive");
        for (double coefficient : {post->smoothDecay, post->smoothRise, post->commonDecay,
             post->commonRise, post->diffDecay, post->diffRise, post->agcDecay, post->agcRise})
        {
            if (!std::isfinite(coefficient) || coefficient < 0.0 || coefficient > 1.0)
                return QStringLiteral("Mel filter coefficients must be in 0..1");
        }
    }
    double lastCutoff = 0.0;
    for (const auto *band : {&freqPower.beat, &freqPower.bass, &freqPower.mids, &freqPower.high})
    {
        if (!std::isfinite(band->maxHz) || band->maxHz < lastCutoff ||
            band->maxHz > 15000.0 || !std::isfinite(band->rise) || !std::isfinite(band->decay) ||
            band->rise < 0.0 || band->rise > 1.0 || band->decay < 0.0 || band->decay > 1.0)
            return QStringLiteral("Power cutoffs must be ordered in 0..15000 Hz with coefficients in 0..1");
        lastCutoff = band->maxHz;
    }
    if (!std::isfinite(aubio.filterbankPower) || aubio.filterbankPower <= 0.0 ||
        !std::isfinite(aubio.filterbankNorm) || (aubio.filterbankNorm != 0.0 && aubio.filterbankNorm != 1.0))
        return QStringLiteral("Filterbank power must be positive and normalization must be 0 or 1");
    return {};
}
