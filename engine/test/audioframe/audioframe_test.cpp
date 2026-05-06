/*
  Q Light Controller Plus - Unit test

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include <QtTest>

#include <cmath>

#include "audioframe_test.h"
#include "audioframe_test_utils.h"

namespace
{
bool fuzzyCompare(double actual, double expected, double tolerance)
{
    return std::abs(actual - expected) <= tolerance;
}
}

void AudioFrame_Test::silentFrame()
{
    const AudioFrame frame = AudioTestUtils::makeSilentFrame();

    QVERIFY(frame.silent);
    QCOMPARE(frame.frameIndex, uint64_t(0));
    QCOMPARE(frame.sampleRate, uint32_t(44100));
    QCOMPARE(frame.fftSize, uint32_t(2048));
    QCOMPARE(frame.binCount, uint32_t(1025));
    QCOMPARE(frame.sampleCount, std::size_t(2048));
    QVERIFY(frame.samples != nullptr);
    QVERIFY(frame.magnitudes != nullptr);
    QVERIFY(frame.bands32 != nullptr);
    QVERIFY2(frame.rms < 1e-12, qPrintable(QString("rms: %1").arg(frame.rms)));
    QCOMPARE(frame.peak, 0.0);
    QCOMPARE(frame.rmsDb, -96.0);
    QCOMPARE(frame.peakDb, -96.0);
    QCOMPARE(frame.crestFactor, 1.0);
    QCOMPARE(frame.spectralFlatness, 1.0);
}

void AudioFrame_Test::sineFrame()
{
    const AudioFrame frame = AudioTestUtils::makeSineFrame(1000.0, -20.0);

    QVERIFY(!frame.silent);
    QVERIFY2(fuzzyCompare(frame.rms, 0.1 / std::sqrt(2.0), 0.006),
             qPrintable(QString("rms: %1").arg(frame.rms)));
    QVERIFY2(fuzzyCompare(frame.peak, 0.1, 0.006),
             qPrintable(QString("peak: %1").arg(frame.peak)));
    QVERIFY2(fuzzyCompare(frame.crestFactor, std::sqrt(2.0), 0.15),
             qPrintable(QString("crestFactor: %1").arg(frame.crestFactor)));
    QVERIFY2(fuzzyCompare(frame.spectralCentroidHz, 1000.0, 70.0),
             qPrintable(QString("centroid: %1").arg(frame.spectralCentroidHz)));
    QVERIFY2(frame.spectralFlatness < 0.05,
             qPrintable(QString("flatness: %1").arg(frame.spectralFlatness)));
}

void AudioFrame_Test::noiseFrame()
{
    const AudioFrame frame = AudioTestUtils::makeNoiseFrame(-20.0);

    QVERIFY(!frame.silent);
    QVERIFY2(frame.rms > 0.04, qPrintable(QString("rms: %1").arg(frame.rms)));
    QVERIFY2(frame.peak > 0.09, qPrintable(QString("peak: %1").arg(frame.peak)));
    QVERIFY2(frame.spectralFlatness > 0.5,
             qPrintable(QString("flatness: %1").arg(frame.spectralFlatness)));
}

void AudioFrame_Test::impulseFrame()
{
    const AudioFrame frame = AudioTestUtils::makeImpulseFrame();

    QVERIFY(!frame.silent);
    QVERIFY2(fuzzyCompare(frame.rms, 1.0 / std::sqrt(2048.0), 0.002),
             qPrintable(QString("rms: %1").arg(frame.rms)));
    QVERIFY2(frame.peak > 0.99, qPrintable(QString("peak: %1").arg(frame.peak)));
    QVERIFY2(frame.crestFactor > 20.0,
             qPrintable(QString("crestFactor: %1").arg(frame.crestFactor)));
    QVERIFY2(frame.spectralFlatness > 0.7,
             qPrintable(QString("flatness: %1").arg(frame.spectralFlatness)));
}

QTEST_MAIN(AudioFrame_Test)
