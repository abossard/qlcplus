/*
  Q Light Controller Plus - Unit test utilities

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include "audioframe_test_utils.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <numeric>
#include <random>
#include <vector>

#ifdef HAS_FFTW3
#include <fftw3.h>
#endif

namespace
{
constexpr double kDbFloor = -96.0;
constexpr double kDbFloorLinear = 1.584893192461114e-5; // 10^-4.8
constexpr double kSilenceRms = 0.002;
constexpr double kSilenceMagnitude = 1e-6;
constexpr double kMinBandHz = 40.0;
constexpr double kMaxBandHz = 5000.0;
constexpr double kTwoPi = 6.28318530717958647692;
constexpr std::size_t kBands32Count = 32;

struct FrameBuffers
{
    std::vector<int16_t> samples;
    std::vector<double> magnitudes;
    std::vector<double> bands32;
};

FrameBuffers &nextBuffers()
{
    thread_local std::vector<std::unique_ptr<FrameBuffers>> buffers;
    buffers.push_back(std::make_unique<FrameBuffers>());
    return *buffers.back();
}

double linearFromDb(double db)
{
    return std::pow(10.0, db / 20.0);
}

int16_t toInt16(double sample)
{
    const double clipped = std::clamp(sample, -1.0, 32767.0 / 32768.0);
    return static_cast<int16_t>(std::lround(clipped * 32768.0));
}

double hanning(uint32_t i, uint32_t size)
{
    if (size <= 1)
        return 1.0;

    return 0.5 * (1.0 - std::cos((kTwoPi * double(i)) / double(size - 1)));
}

void computeMagnitudes(const std::vector<int16_t> &samples, uint32_t sampleRate, uint32_t fftSize,
                       std::vector<double> &magnitudes)
{
    (void) sampleRate;

    const uint32_t binCount = (fftSize / 2) + 1;
    magnitudes.assign(binCount, 0.0);

    if (fftSize == 0 || samples.empty())
        return;

    const double mean = std::accumulate(samples.begin(), samples.end(), 0.0) / double(samples.size());
    std::vector<double> fftInput(fftSize, 0.0);
    for (uint32_t i = 0; i < fftSize; ++i)
    {
        const double normalized = (double(samples[i]) - mean) / 32768.0;
        fftInput[i] = normalized * hanning(i, fftSize);
    }

#ifdef HAS_FFTW3
    std::vector<double> input = fftInput;
    fftw_complex *output = reinterpret_cast<fftw_complex*>(fftw_malloc(sizeof(fftw_complex) * binCount));
    fftw_plan plan = output != nullptr
        ? fftw_plan_dft_r2c_1d(static_cast<int>(fftSize), input.data(), output, FFTW_ESTIMATE)
        : nullptr;
    if (plan != nullptr)
    {
        fftw_execute(plan);
        fftw_destroy_plan(plan);

        for (uint32_t k = 0; k < binCount; ++k)
            magnitudes[k] = std::hypot(output[k][0], output[k][1]);
        fftw_free(output);
        return;
    }
    if (output != nullptr)
        fftw_free(output);
#endif

    for (uint32_t k = 0; k < binCount; ++k)
    {
        double real = 0.0;
        double imag = 0.0;
        for (uint32_t n = 0; n < fftSize; ++n)
        {
            const double phase = -kTwoPi * double(k) * double(n) / double(fftSize);
            real += fftInput[n] * std::cos(phase);
            imag += fftInput[n] * std::sin(phase);
        }
        magnitudes[k] = std::hypot(real, imag);
    }
}

void computeBands32(const std::vector<double> &magnitudes, uint32_t sampleRate, uint32_t fftSize,
                    std::vector<double> &bands32)
{
    bands32.assign(kBands32Count, 0.0);
    if (sampleRate == 0 || fftSize == 0 || magnitudes.empty())
        return;

    const int maxBin = int(fftSize / 2);
    const double nyquist = double(sampleRate) / 2.0;
    const double maxFreq = std::min(kMaxBandHz, nyquist);
    const double logRange = (maxFreq > kMinBandHz) ? std::log(maxFreq / kMinBandHz) : 0.0;
    if (maxBin <= 1 || logRange <= 0.0)
        return;

    for (std::size_t b = 0; b < kBands32Count; ++b)
    {
        const double bandStartHz = kMinBandHz * std::exp(logRange * (double(b) / double(kBands32Count)));
        const double bandEndHz = kMinBandHz * std::exp(logRange * (double(b + 1) / double(kBands32Count)));

        int startBin = std::clamp(int(std::floor((bandStartHz * double(fftSize)) / double(sampleRate))), 1, maxBin);
        int endBin = std::clamp(int(std::ceil((bandEndHz * double(fftSize)) / double(sampleRate))), 1, maxBin + 1);

        if (b == kBands32Count - 1)
            endBin = maxBin + 1;
        if (endBin <= startBin)
            endBin = std::min(maxBin + 1, startBin + 1);

        double magnitudeSum = 0.0;
        for (int bin = startBin; bin < endBin && bin < int(magnitudes.size()); ++bin)
            magnitudeSum += magnitudes[std::size_t(bin)];

        bands32[b] = magnitudeSum / (double(endBin - startBin) * kTwoPi);
    }
}

double dbFromLinear(double value)
{
    return 20.0 * std::log10(std::max(value, kDbFloorLinear));
}

double binFrequency(uint32_t bin, uint32_t sampleRate, uint32_t fftSize)
{
    return (fftSize == 0) ? 0.0 : (double(bin) * double(sampleRate) / double(fftSize));
}

bool isAnalysisBin(uint32_t bin, uint32_t sampleRate, uint32_t fftSize)
{
    const double frequency = binFrequency(bin, sampleRate, fftSize);
    return frequency >= kMinBandHz && frequency <= std::min(kMaxBandHz, double(sampleRate) / 2.0);
}

double computeCentroidHz(const std::vector<double> &magnitudes, uint32_t sampleRate, uint32_t fftSize)
{
    double weightedSum = 0.0;
    double magnitudeSum = 0.0;
    for (uint32_t bin = 0; bin < magnitudes.size(); ++bin)
    {
        if (!isAnalysisBin(bin, sampleRate, fftSize))
            continue;

        weightedSum += binFrequency(bin, sampleRate, fftSize) * magnitudes[bin];
        magnitudeSum += magnitudes[bin];
    }
    return magnitudeSum > 0.0 ? weightedSum / magnitudeSum : 0.0;
}

double computeRolloffHz(const std::vector<double> &magnitudes, uint32_t sampleRate, uint32_t fftSize)
{
    double totalEnergy = 0.0;
    for (uint32_t bin = 0; bin < magnitudes.size(); ++bin)
    {
        if (isAnalysisBin(bin, sampleRate, fftSize))
            totalEnergy += magnitudes[bin] * magnitudes[bin];
    }

    if (totalEnergy <= 0.0)
        return 0.0;

    const double threshold = 0.85 * totalEnergy;
    double cumulative = 0.0;
    for (uint32_t bin = 0; bin < magnitudes.size(); ++bin)
    {
        if (!isAnalysisBin(bin, sampleRate, fftSize))
            continue;

        cumulative += magnitudes[bin] * magnitudes[bin];
        if (cumulative >= threshold)
            return binFrequency(bin, sampleRate, fftSize);
    }
    return 0.0;
}

double computeFlatness(const std::vector<double> &magnitudes, uint32_t sampleRate, uint32_t fftSize)
{
    constexpr double epsilon = 1e-10;
    double logSum = 0.0;
    double linearSum = 0.0;
    uint32_t count = 0;

    for (uint32_t bin = 0; bin < magnitudes.size(); ++bin)
    {
        if (!isAnalysisBin(bin, sampleRate, fftSize))
            continue;

        logSum += std::log(magnitudes[bin] + epsilon);
        linearSum += magnitudes[bin];
        ++count;
    }

    if (count == 0)
        return 1.0;

    const double geometricMean = std::exp(logSum / double(count));
    const double arithmeticMean = linearSum / double(count);
    return geometricMean / (arithmeticMean + epsilon);
}

AudioFrame makeFrameFromSamples(std::vector<int16_t> sampleData, uint64_t frameIndex, uint32_t sampleRate, uint32_t fftSize)
{
    FrameBuffers &buffers = nextBuffers();
    buffers.samples = std::move(sampleData);
    if (buffers.samples.size() != fftSize)
        buffers.samples.resize(fftSize, 0);

    const double mean = std::accumulate(buffers.samples.begin(), buffers.samples.end(), 0.0) / double(fftSize);

    double sumSq = 0.0;
    double peak = 0.0;
    for (int16_t sample : buffers.samples)
    {
        const double normalized = (double(sample) - mean) / 32768.0;
        sumSq += normalized * normalized;
        peak = std::max(peak, std::abs(normalized));
    }

    computeMagnitudes(buffers.samples, sampleRate, fftSize, buffers.magnitudes);
    computeBands32(buffers.magnitudes, sampleRate, fftSize, buffers.bands32);

    const double rms = (fftSize == 0) ? 0.0 : std::sqrt(sumSq / double(fftSize));
    const double maxMagnitude = buffers.magnitudes.empty() ? 0.0 : *std::max_element(buffers.magnitudes.begin(), buffers.magnitudes.end());
    const bool silent = rms < kSilenceRms && maxMagnitude < kSilenceMagnitude;

    AudioFrame frame{};
    frame.frameIndex = frameIndex;
    frame.hostTimeNs = 0;
    frame.sampleRate = sampleRate;
    frame.fftSize = fftSize;
    frame.binCount = (fftSize / 2) + 1;
    frame.silent = silent;
    frame.samples = buffers.samples.data();
    frame.sampleCount = buffers.samples.size();
    frame.rms = rms;
    frame.peak = peak;
    frame.dcOffset = mean / 32768.0;
    frame.magnitudes = buffers.magnitudes.data();
    frame.bands32 = buffers.bands32.data();
    frame.rmsDb = dbFromLinear(rms);
    frame.peakDb = dbFromLinear(peak);
    frame.crestFactor = silent ? 1.0 : (peak / std::max(rms, 1e-9));
    frame.spectralFlux = 0.0;
    frame.spectralCentroidHz = silent ? 0.0 : computeCentroidHz(buffers.magnitudes, sampleRate, fftSize);
    frame.spectralRolloffHz = silent ? 0.0 : computeRolloffHz(buffers.magnitudes, sampleRate, fftSize);
    frame.spectralFlatness = silent ? 1.0 : computeFlatness(buffers.magnitudes, sampleRate, fftSize);
    frame.noiseFloorDb = kDbFloor;
    frame.beatDetected = false;
    return frame;
}
}

namespace AudioTestUtils
{
AudioFrame makeSilentFrame(uint64_t frameIndex, uint32_t sampleRate, uint32_t fftSize)
{
    return makeFrameFromSamples(std::vector<int16_t>(fftSize, 0), frameIndex, sampleRate, fftSize);
}

AudioFrame makeSineFrame(double frequencyHz, double amplitudeDb, uint64_t frameIndex, uint32_t sampleRate, uint32_t fftSize)
{
    std::vector<int16_t> samples(fftSize, 0);
    const double amplitude = linearFromDb(amplitudeDb);
    for (uint32_t i = 0; i < fftSize; ++i)
    {
        const double phase = kTwoPi * frequencyHz * double(i) / double(sampleRate);
        samples[i] = toInt16(amplitude * std::sin(phase));
    }
    return makeFrameFromSamples(std::move(samples), frameIndex, sampleRate, fftSize);
}

AudioFrame makeNoiseFrame(double amplitudeDb, uint64_t frameIndex, uint32_t sampleRate, uint32_t fftSize)
{
    std::vector<int16_t> samples(fftSize, 0);
    std::mt19937 generator(0x514c4350u + static_cast<uint32_t>(frameIndex));
    std::uniform_real_distribution<double> distribution(-linearFromDb(amplitudeDb), linearFromDb(amplitudeDb));
    for (uint32_t i = 0; i < fftSize; ++i)
        samples[i] = toInt16(distribution(generator));

    return makeFrameFromSamples(std::move(samples), frameIndex, sampleRate, fftSize);
}

AudioFrame makeImpulseFrame(uint64_t frameIndex, uint32_t sampleRate, uint32_t fftSize)
{
    std::vector<int16_t> samples(fftSize, 0);
    if (!samples.empty())
        samples[samples.size() / 2] = 32767;

    return makeFrameFromSamples(std::move(samples), frameIndex, sampleRate, fftSize);
}
}
