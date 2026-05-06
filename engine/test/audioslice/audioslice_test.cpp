/*
  Q Light Controller Plus - Unit test
  audioslice_test.cpp

  Vertical-slice integration test that exercises the full audio pipeline:

      AudioFrame -> AudioAnalyzer -> AudioChannel -> AudioSnapshot
                                                    -> (buildAudioDataObject -> JS audio object)

  The JS-object build step is implemented in RGBScript::buildAudioDataObject(),
  which requires the per-thread static QJSEngine that RGBScript wires up at
  script load time. Spinning that up from a unit test is too heavy and would
  effectively duplicate the rgbmatrix_test fixture without adding signal,
  so this test stops at the AudioSnapshot boundary and instead verifies the
  legacy field contract by inspecting the rgbscriptv4.cpp source. See
  testLegacyFieldsPreserved() and the p3-slice-p4-cleanup review note.

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include <QtTest>
#include <QFile>
#include <QString>

#include <cmath>

#include "audioanalyzer.h"
#include "audiochannel.h"
#include "audiochannelconfig.h"
#include "audioframe.h"
#include "audioprofile.h"
#include "audiosnapshot.h"
#include "doc.h"
#include "rgbmatrix.h"

#include "audioframe_test_utils.h"
#include "audioslice_test.h"

namespace
{
constexpr double kSampleRateHz = 44100.0;
constexpr uint32_t kFftSize = 2048;
}

// ---------------------------------------------------------------------------
// Test 1: end-to-end pipeline -- AudioFrame -> Analyzer -> Channel -> Snapshot
// ---------------------------------------------------------------------------
void AudioSlice_Test::testEndToEndPipeline()
{
    AudioAnalyzer analyzer;
    AudioChannel *channel = analyzer.createChannel(AudioChannelConfig::defaults());
    QVERIFY(channel != nullptr);

    // Synthetic 1 kHz sine at -20 dBFS, 44.1 kHz, 2048-point FFT.
    AudioFrame frame = AudioTestUtils::makeSineFrame(1000.0, -20.0,
                                                     /*frameIndex=*/1,
                                                     uint32_t(kSampleRateHz),
                                                     kFftSize);

    // Drives shared features (rms/peak/centroid/...) and updates every channel.
    analyzer.processFrame(frame);

    // Sanity check on the frame the analyzer just decorated -- this is the
    // contract every consumer downstream observes.
    QVERIFY(!frame.silent);
    QVERIFY(frame.rmsDb > -40.0);
    QVERIFY(frame.rmsDb < 0.0);
    QVERIFY(frame.bands32 != nullptr);
    QVERIFY(frame.spectralCentroidHz > 500.0);
    QVERIFY(frame.spectralCentroidHz < 1500.0);

    AudioSnapshot snap = channel->snapshot();

    // Bands -- at least one perceptual band should reflect the 1 kHz tone.
    const double bandSum = snap.bands.sub + snap.bands.bass + snap.bands.lowMid +
                           snap.bands.mid + snap.bands.high;
    QVERIFY2(bandSum > 0.0, "All perceptual bands are zero after a -20 dBFS sine frame");
    QVERIFY2(snap.bands.mid > 0.0,
             qPrintable(QString("mid band must respond to 1 kHz tone, got %1").arg(snap.bands.mid)));

    // Features -- snapshot should mirror the shared frame features.
    QCOMPARE(snap.features.rmsDb, frame.rmsDb);
    QCOMPARE(snap.features.centroidHz, frame.spectralCentroidHz);
    QVERIFY(snap.features.crestFactor > 1.0);

    // Triggers -- on the very first frame after silence we must NOT spuriously
    // fire (firedThisFrame implies the trigger crossed the high threshold and
    // armed; for a single -20 dBFS sine it should be inactive). Values are
    // produced and finite.
    for (int i = 0; i < 5; ++i)
    {
        const TriggerState &t = snap.triggers[i];
        QVERIFY2(std::isfinite(t.value),
                 qPrintable(QString("trigger[%1].value is not finite: %2").arg(i).arg(t.value)));
        QVERIFY2(t.value >= 0.0,
                 qPrintable(QString("trigger[%1].value must be >= 0, got %2").arg(i).arg(t.value)));
    }
    QVERIFY(std::isfinite(snap.volumeTrigger.value));

    // audioDtMs is computed by the analyzer from the host time / fft hop.
    QVERIFY(snap.audioDtMs >= 0.0);

    analyzer.destroyChannel(channel);
}

// ---------------------------------------------------------------------------
// Test 2: AudioProfile resolution via Doc::audioProfileForFunction
// ---------------------------------------------------------------------------
void AudioSlice_Test::testProfileResolutionChain()
{
    Doc doc(nullptr);

    // Default profile -- always available, every other lookup falls back here.
    AudioProfile *defaultProfile = doc.ensureDefaultAudioProfile();
    QVERIFY(defaultProfile != nullptr);
    QCOMPARE(defaultProfile->isDefault(), true);

    // Custom profile with a non-default brightness floor.
    AudioProfile *custom = new AudioProfile(99, &doc);
    custom->setName(QStringLiteral("Custom Slice"));
    AudioChannelConfig customCfg = AudioChannelConfig::defaults();
    customCfg.brightnessFloor = 0.42;
    custom->setChannelConfig(customCfg);
    QVERIFY(doc.addAudioProfile(custom));

    // RGBMatrix bound to the custom profile.
    RGBMatrix *matrix = new RGBMatrix(&doc);
    matrix->setName(QStringLiteral("Slice Matrix"));
    matrix->setAudioProfileId(custom->id());
    QVERIFY(doc.addFunction(matrix));
    const quint32 matrixId = matrix->id();
    QVERIFY(matrixId != Function::invalidId());

    // Resolution must follow the binding.
    AudioProfile *resolved = doc.audioProfileForFunction(matrixId);
    QCOMPARE(resolved, custom);
    QCOMPARE(resolved->channelConfig().brightnessFloor, 0.42);

    // Invalid function id -> default fallback.
    AudioProfile *fallback = doc.audioProfileForFunction(Function::invalidId());
    QCOMPARE(fallback, defaultProfile);

    // Unknown function id -> default fallback.
    AudioProfile *missing = doc.audioProfileForFunction(0xDEADBEEF);
    QCOMPARE(missing, defaultProfile);

    // Matrix re-bound to invalidId -> default fallback even though function exists.
    matrix->setAudioProfileId(AudioProfile::invalidId());
    AudioProfile *reset = doc.audioProfileForFunction(matrixId);
    QCOMPARE(reset, defaultProfile);
}

// ---------------------------------------------------------------------------
// Test 3: legacy fields preserved in buildAudioDataObject()
//
// We can't call buildAudioDataObject() directly: it depends on the per-thread
// QJSEngine wired up by RGBScript::evaluate(). Instead, source-inspect the
// implementation to assert the legacy property names are still set
// unconditionally (i.e. before the early return when the AudioChannel is
// null). This keeps the contract from regressing as the new audio object is
// extended.
// ---------------------------------------------------------------------------
void AudioSlice_Test::testLegacyFieldsPreserved()
{
    QFile src(QStringLiteral(RGBSCRIPTV4_SOURCE_PATH));
    QVERIFY2(src.open(QIODevice::ReadOnly | QIODevice::Text),
             qPrintable(QString("Cannot open %1 for inspection: %2")
                            .arg(src.fileName(), src.errorString())));
    const QString source = QString::fromUtf8(src.readAll());
    src.close();

    // Find the buildAudioDataObject body and the early `return audioObj` that
    // marks the boundary between always-set legacy fields and the
    // channel-dependent enriched object.
    const int fnStart = source.indexOf(QStringLiteral("buildAudioDataObject()"));
    QVERIFY2(fnStart >= 0, "buildAudioDataObject() not found in rgbscriptv4.cpp");

    const int channelGuard = source.indexOf(QStringLiteral("if (channel == NULL)"), fnStart);
    QVERIFY2(channelGuard > fnStart,
             "channel-null guard not found inside buildAudioDataObject()");

    const QString legacyRegion = source.mid(fnStart, channelGuard - fnStart);

    // Each of these legacy property names must be set in the legacy region,
    // i.e. unconditionally on every audio object the engine builds.
    const QStringList legacyFields{
        QStringLiteral("\"spectrum\""),
        QStringLiteral("\"volume\""),
        QStringLiteral("\"beat\""),
        QStringLiteral("\"bpm\""),
        QStringLiteral("\"maxMagnitude\"")};

    for (const QString &field : legacyFields)
    {
        QVERIFY2(legacyRegion.contains(field),
                 qPrintable(QString("Legacy field %1 missing from buildAudioDataObject() "
                                    "before the channel-null guard").arg(field)));
    }
}

QTEST_MAIN(AudioSlice_Test)
