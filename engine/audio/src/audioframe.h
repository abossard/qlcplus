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
 * One block of analyzed audio. Built once per AudioCapture buffer by the analyzer
 * running on the AudioCapture thread.
 *
 * Spectral / pitch / onset / tempo features come from the attached AubioResults
 * pointer. Time-domain RMS / peak / dB metrics are computed in AudioCapture.
 *
 * The AubioResults pointed to is owned by AudioCapture and is valid only for the
 * duration of the synchronous analyzer callback that receives this frame.
 */
struct AudioFrame
{
    uint64_t frameIndex = 0;
    uint32_t sampleRate = 44100;
    uint32_t sampleCount = 0;
    bool silent = false;
    bool beatDetected = false;

    double rms = 0.0;
    double peak = 0.0;
    double rmsDb = -96.0;
    double peakDb = -96.0;
    double crestFactor = 1.0;

    /** Aubio analysis results for this frame. Non-owning — borrowed from AudioCapture. */
    const AubioResults *aubio = nullptr;
};
