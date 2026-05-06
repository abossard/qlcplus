/*
  Q Light Controller Plus - Unit test
  vcaudiotriggers_golden_test.cpp

  See vcaudiotriggers_golden_test.h and docs/audio-dsp-reviews/p2b-golden-expose.md
  for what is (and is NOT) covered here.

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include <QtTest>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include <algorithm>
#include <cmath>

#include "audioanalyzer.h"
#include "audiocapture.h"
#include "audiochannel.h"
#include "audiochannelconfig.h"
#include "audioframe.h"
#include "audioprofile.h"
#include "audiosnapshot.h"
#include "doc.h"
#include "rgbmatrix.h"

#include "audioframe_test_utils.h"
#include "vcaudiotriggers_golden_test.h"

namespace
{
constexpr double kDtMs = 10.0;

// Replica of VCAudioTriggers::slotSpectrumDataChanged() low/mid/high aggregation.
// The widget receives spectrumBands of size m_spectrumBars.count() - 1; we
// drive that with frame.bands32 (32 perceptual log bands) which is the same
// data path AudioCapture pushes into the widget in the new pipeline.
struct LegacyPowers
{
    double lows = 0.0;
    double mids = 0.0;
    double highs = 0.0;
};

LegacyPowers aggregateLegacyPowers(const double *bands, int n, double maxMagnitude)
{
    LegacyPowers out;
    if (n < 3 || maxMagnitude <= 0.0)
        return out;

    const int lowCut = AudioCapture::lowCutBin(n);
    const int highCut = AudioCapture::highCutBin(n);

    double lowSum = 0.0, midSum = 0.0, highSum = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const double v = std::clamp(bands[i] / maxMagnitude, 0.0, 1.0);
        if (i < lowCut)
            lowSum += v;
        else if (i < highCut)
            midSum += v;
        else
            highSum += v;
    }

    out.lows = (lowCut > 0) ? lowSum / lowCut : 0.0;
    out.mids = (highCut > lowCut) ? midSum / (highCut - lowCut) : 0.0;
    out.highs = (n > highCut) ? highSum / (n - highCut) : 0.0;
    return out;
}

AudioChannelConfig fastConfig()
{
    AudioChannelConfig cfg = AudioChannelConfig::defaults();
    cfg.envelope.attackMs = 5.0;
    cfg.envelope.releaseMs = 50.0;
    cfg.volumeSmoothingMs = 1.0;
    cfg.agc.enabled = false;
    cfg.agc.inputGainLinear = 1.0;
    cfg.noiseGate.thresholdDb = -200.0;
    cfg.noiseGate.holdMs = 1e9;
    return cfg;
}

double maxBandMagnitude(const AudioFrame &frame)
{
    double m = 0.0;
    if (frame.bands32 != nullptr)
    {
        for (int i = 0; i < 32; ++i)
            m = std::max(m, frame.bands32[i]);
    }
    return m;
}
}

void VCAudioTriggersGoldenTest::testLegacyBarsStillWork()
{
    // Drive the analyzer with a non-silent low-frequency sine — the legacy path
    // should yield strictly positive low/mid/high powers (lows dominating).
    AudioAnalyzer analyzer;
    AudioFrame frame = AudioTestUtils::makeSineFrame(120.0, -10.0);
    analyzer.processFrame(frame);

    QVERIFY(!frame.silent);
    QVERIFY(frame.bands32 != nullptr);

    const double maxMag = maxBandMagnitude(frame);
    QVERIFY2(maxMag > 0.0, "Analyzer must populate bands32 with non-zero values");

    const LegacyPowers p = aggregateLegacyPowers(frame.bands32, 32, maxMag);

    // For a 120 Hz sine, lows must dominate but the others should still be
    // computable and finite.
    QVERIFY2(p.lows > 0.0, "lowsPower must be non-zero for 120 Hz tone");
    QVERIFY(std::isfinite(p.lows) && std::isfinite(p.mids) && std::isfinite(p.highs));
    QVERIFY(p.lows >= p.mids);
    QVERIFY(p.lows >= p.highs);
    QVERIFY(p.lows <= 1.0 && p.mids <= 1.0 && p.highs <= 1.0);

    // Broadband (white noise) input should activate all three bins.
    AudioFrame noise = AudioTestUtils::makeNoiseFrame(-12.0);
    analyzer.processFrame(noise);
    const double maxNoise = maxBandMagnitude(noise);
    QVERIFY(maxNoise > 0.0);
    const LegacyPowers pn = aggregateLegacyPowers(noise.bands32, 32, maxNoise);
    QVERIFY2(pn.lows > 0.0 && pn.mids > 0.0 && pn.highs > 0.0,
             "All three legacy bins must light up on broadband noise");
}

void VCAudioTriggersGoldenTest::testNewPerceptualBands()
{
    // This exercises the exact data the new VCAudioTriggers Q_PROPERTYs read:
    // VCAudioTriggers::updateAudioProfileSnapshotPowers() copies
    // channel->snapshot().bands.{sub,bass,lowMid,mid,high} into m_*Power.
    AudioAnalyzer analyzer;
    AudioChannel channel(fastConfig());

    // Process several frames so the envelope can rise.
    for (int i = 0; i < 20; ++i)
    {
        AudioFrame frame = AudioTestUtils::makeNoiseFrame(-6.0, uint64_t(i));
        analyzer.processFrame(frame);
        channel.update(frame, kDtMs);
    }

    const AudioSnapshot snap = channel.snapshot();
    QVERIFY2(snap.bands.sub > 0.0,    "subPower must reflect the snapshot");
    QVERIFY2(snap.bands.bass > 0.0,   "bassPower must reflect the snapshot");
    QVERIFY2(snap.bands.lowMid > 0.0, "lowMidPower must reflect the snapshot");
    QVERIFY2(snap.bands.mid > 0.0,    "midPower must reflect the snapshot");
    QVERIFY2(snap.bands.high > 0.0,   "highPower must reflect the snapshot");

    QVERIFY(snap.bands.sub <= 1.0);
    QVERIFY(snap.bands.bass <= 1.0);
    QVERIFY(snap.bands.lowMid <= 1.0);
    QVERIFY(snap.bands.mid <= 1.0);
    QVERIFY(snap.bands.high <= 1.0);

    // Documented derived value: low = (sub + bass) / 2
    QVERIFY(std::abs(snap.bands.low - (snap.bands.sub + snap.bands.bass) * 0.5) < 1e-9);

    // Silence drains the channel back toward zero.
    for (int i = 0; i < 200; ++i)
        channel.update(AudioTestUtils::makeSilentFrame(), kDtMs);

    const AudioSnapshot quiet = channel.snapshot();
    QVERIFY(quiet.bands.sub < 0.05);
    QVERIFY(quiet.bands.high < 0.05);
}

void VCAudioTriggersGoldenTest::testDualPathCoexistence()
{
    // Both the legacy aggregation and the new channel snapshot must produce
    // sensible values from the SAME frame, in any order, without one stomping
    // on the other (they share read access to frame.bands32).
    AudioAnalyzer analyzer;
    AudioChannel channel(fastConfig());

    LegacyPowers lastLegacy;
    AudioSnapshot lastSnap;
    for (int i = 0; i < 10; ++i)
    {
        AudioFrame frame = AudioTestUtils::makeSineFrame(800.0, -8.0, uint64_t(i));
        analyzer.processFrame(frame);

        // Path A: legacy aggregation reading frame.bands32.
        const double maxMag = maxBandMagnitude(frame);
        lastLegacy = aggregateLegacyPowers(frame.bands32, 32, maxMag);

        // Path B: AudioChannel update + snapshot.
        channel.update(frame, kDtMs);
        lastSnap = channel.snapshot();
    }

    // Legacy mids should fire (800 Hz lives in the mid bin).
    QVERIFY2(lastLegacy.mids > 0.0, "legacy mids must fire for 800 Hz");
    QVERIFY(std::isfinite(lastLegacy.lows));
    QVERIFY(std::isfinite(lastLegacy.highs));

    // New perceptual mid/lowMid must also be non-zero — independent path.
    QVERIFY2(lastSnap.bands.mid > 0.0 || lastSnap.bands.lowMid > 0.0,
             "new mid/lowMid bands must fire for 800 Hz");

    // Both paths must remain bounded.
    QVERIFY(lastLegacy.lows <= 1.0 && lastLegacy.mids <= 1.0 && lastLegacy.highs <= 1.0);
    QVERIFY(lastSnap.bands.sub <= 1.0 && lastSnap.bands.high <= 1.0);
}

void VCAudioTriggersGoldenTest::testProfileIdPersistence()
{
    // VCAudioTriggers (qmlui) is not built into this engine-level test.
    // Use RGBMatrix as a proxy — it stores audioProfileId with the same
    // semantics and is what Doc::audioProfileForFunction() reads in Phase 3.
    Doc doc(nullptr);
    RGBMatrix matrix(&doc);
    matrix.setName(QStringLiteral("Test Matrix"));
    const quint32 pid = 17;
    matrix.setAudioProfileId(pid);

    QString xml;
    {
        QXmlStreamWriter writer(&xml);
        QVERIFY(matrix.saveXML(&writer));
    }
    QVERIFY(xml.contains(QStringLiteral("AudioProfileID")));
    QVERIFY(xml.contains(QStringLiteral(">17<")));

    QXmlStreamReader reader(xml);
    QVERIFY(reader.readNextStartElement());

    Doc doc2(nullptr);
    RGBMatrix loaded(&doc2);
    QVERIFY(loaded.loadXML(reader));
    QCOMPARE(loaded.audioProfileId(), pid);
}

void VCAudioTriggersGoldenTest::testAudioProfileForFunction()
{
    // Doc::audioProfileForFunction() is the resolution helper Phase 3's
    // script engine will call. Verify the three documented outcomes:
    //  1. Function references a valid profile id  -> returns that profile
    //  2. Function references invalid/missing id  -> returns default profile
    //  3. Unknown function id                     -> returns default profile
    Doc doc(nullptr);

    AudioProfile *def = new AudioProfile(0, &doc);
    def->setName(QStringLiteral("Default"));
    def->setIsDefault(true);
    QVERIFY(doc.addAudioProfile(def));

    AudioProfile *custom = new AudioProfile(7, &doc);
    custom->setName(QStringLiteral("Custom"));
    QVERIFY(doc.addAudioProfile(custom));

    RGBMatrix *m1 = new RGBMatrix(&doc);
    m1->setName(QStringLiteral("M1 with custom"));
    m1->setAudioProfileId(7);
    QVERIFY(doc.addFunction(m1));

    RGBMatrix *m2 = new RGBMatrix(&doc);
    m2->setName(QStringLiteral("M2 no profile"));
    // leaves audioProfileId == AudioProfile::invalidId()
    QVERIFY(doc.addFunction(m2));

    RGBMatrix *m3 = new RGBMatrix(&doc);
    m3->setName(QStringLiteral("M3 dangling profile"));
    m3->setAudioProfileId(9999); // does not exist in Doc
    QVERIFY(doc.addFunction(m3));

    // Case 1: explicit valid profile.
    QCOMPARE(doc.audioProfileForFunction(m1->id()), custom);
    // Case 2: no profile set -> default.
    QCOMPARE(doc.audioProfileForFunction(m2->id()), def);
    // Case 3: dangling id -> default fallback.
    QCOMPARE(doc.audioProfileForFunction(m3->id()), def);
    // Case 4: unknown function id -> default fallback.
    QCOMPARE(doc.audioProfileForFunction(123456), def);

    // No profiles at all: should hand back nullptr without crashing.
    Doc empty(nullptr);
    RGBMatrix *m4 = new RGBMatrix(&empty);
    m4->setName(QStringLiteral("M4 empty doc"));
    QVERIFY(empty.addFunction(m4));
    QCOMPARE(empty.audioProfileForFunction(m4->id()),
             static_cast<AudioProfile *>(nullptr));
}

QTEST_GUILESS_MAIN(VCAudioTriggersGoldenTest)
