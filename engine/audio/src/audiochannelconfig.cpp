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
    AudioChannelConfig cfg;
    cfg.envelope = { 15.0, 150.0 };
    cfg.triggers = { 0.65, 0.45, 80.0, 120.0 };
    cfg.bandLayout = { 60.0, 250.0, 500.0, 2000.0, 5000.0 };
    cfg.noiseGate = { -60.0, 120.0 };
    cfg.brightnessFloor = 0.0;
    cfg.volumeSmoothingMs = 100.0;
    cfg.aubio = AubioConfig{};
    return cfg;
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
