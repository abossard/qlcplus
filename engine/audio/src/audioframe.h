/*
  Q Light Controller Plus
  audioframe.h

  Copyright (c) Massimo Callegari

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#pragma once

#include <cstdint>

struct AubioResults;

/**
 * One canonical mono PCM hop. Borrowed samples and optional legacy Aubio results
 * are valid only during synchronous processing of this frame.
 */
struct AudioFrame
{
    uint64_t frameIndex = 0;
    const float *samples = nullptr;
    uint64_t sourceEpoch = 0;
    uint64_t sampleTime = 0;
    uint32_t sampleRate = 30000;
    uint32_t sampleCount = 500;
    bool silent = false;
    bool beatDetected = false;

    double rms = 0.0;
    double peak = 0.0;
    double rmsDb = -96.0;
    double peakDb = -96.0;
    double crestFactor = 1.0;

    // clamp(1 + raw RMS dBFS / 100, 0, 1), with digital silence at zero.
    double volumeNorm = 0.0;

    /** Optional borrowed results for legacy consumers without raw PCM. */
    const AubioResults *aubio = nullptr;
};
