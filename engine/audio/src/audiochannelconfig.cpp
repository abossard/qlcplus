/*
  Q Light Controller Plus
  audiochannelconfig.cpp

  Copyright (c) Massimo Callegari

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include "audiochannelconfig.h"

#include <algorithm>
#include <cmath>

namespace
{
    int bounded(int value, int minValue, int maxValue)
    {
        return std::max(minValue, std::min(value, maxValue));
    }
}

AudioChannelConfig AudioChannelConfig::defaults()
{
    // In-class member initializers are the single source of truth for default
    // values — see EnvelopeConfig / NoiseGateConfig / TriggerConfig in
    // audiochannelconfig.h. This function intentionally adds no overrides.
    return AudioChannelConfig{};
}

AudioChannelConfig AudioChannelConfig::fromLegacySliders(int /*gain*/, int reactivity, int floor, int sensitivity)
{
    AudioChannelConfig config = defaults();

    const int boundedReactivity = bounded(reactivity, 1, 10);
    const int boundedFloor = bounded(floor, 0, 100);
    const int boundedSensitivity = bounded(sensitivity, 1, 10);

    // Legacy `gain` slider is dropped: input gain is now an OS/hardware concern.

    const double alpha = std::min(0.1 + boundedReactivity * 0.09, 0.999);
    config.envelope.attackMs = -40.0 / std::log(1.0 - alpha);
    config.envelope.releaseMs = 4.0 * config.envelope.attackMs;

    config.brightnessFloor = boundedFloor / 100.0;

    config.triggers.highThreshold = 0.45 - boundedSensitivity * 0.04;
    config.triggers.lowThreshold = std::max(0.0, config.triggers.highThreshold - 0.20);

    return config;
}
