/*
  Q Light Controller Plus - Unit test utilities

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef AUDIOFRAME_TEST_UTILS_H
#define AUDIOFRAME_TEST_UTILS_H

#include <cstdint>

#include "audioframe.h"

namespace AudioTestUtils
{
    AudioFrame makeSilentFrame(uint64_t frameIndex = 0, uint32_t sampleRate = 44100, uint32_t fftSize = 2048);

    AudioFrame makeSineFrame(double frequencyHz, double amplitudeDb = -20.0,
                             uint64_t frameIndex = 0, uint32_t sampleRate = 44100, uint32_t fftSize = 2048);

    AudioFrame makeNoiseFrame(double amplitudeDb = -20.0, uint64_t frameIndex = 0,
                              uint32_t sampleRate = 44100, uint32_t fftSize = 2048);

    AudioFrame makeImpulseFrame(uint64_t frameIndex = 0, uint32_t sampleRate = 44100, uint32_t fftSize = 2048);
}

#endif // AUDIOFRAME_TEST_UTILS_H
