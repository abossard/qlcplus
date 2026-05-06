/*
  Q Light Controller Plus - Unit test
  audiochannel_test.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include <QtTest>
#include <QThread>
#include <QElapsedTimer>

#include <atomic>
#include <cmath>
#include <thread>

#include "audiochannel.h"
#include "audiochannelconfig.h"
#include "audioframe.h"
#include "audiosnapshot.h"

#include "audioframe_test_utils.h"
#include "audiochannel_test.h"

namespace
{
constexpr double kDtMs = 10.0;

AudioChannelConfig fastConfig()
{
    AudioChannelConfig cfg = AudioChannelConfig::defaults();
    cfg.envelope.attackMs = 5.0;
    cfg.envelope.releaseMs = 50.0;
    cfg.volumeSmoothingMs = 1.0;
    cfg.agc.enabled = false;
    cfg.agc.inputGainLinear = 1.6;
    cfg.noiseGate.thresholdDb = -200.0;
    cfg.noiseGate.holdMs = 1e9;
    cfg.triggers.highThreshold = 0.65;
    cfg.triggers.lowThreshold = 0.45;
    cfg.triggers.holdMs = 50.0;
    cfg.triggers.cooldownMs = 100.0;
    return cfg;
}

void warmUp(AudioChannel &channel, const AudioFrame &frame, int frames, double dtMs = kDtMs)
{
    for (int i = 0; i < frames; ++i)
        channel.update(frame, dtMs);
}
}

void AudioChannel_Test::testEnvelopeSmoothing()
{
    AudioChannelConfig cfg = fastConfig();
    cfg.envelope.attackMs = 20.0;
    cfg.envelope.releaseMs = 200.0;
    AudioChannel channel(cfg);

    const AudioFrame loud = AudioTestUtils::makeSineFrame(200.0, -6.0);
    const AudioFrame silence = AudioTestUtils::makeSilentFrame();

    channel.update(loud, kDtMs);
    const double firstSub = channel.snapshot().bands.sub;
    QVERIFY2(firstSub > 0.0, "Envelope must rise above zero after a loud frame");
    QVERIFY2(firstSub < 1.0, "Envelope must not jump straight to plateau");

    double prev = firstSub;
    bool monotonicAttack = true;
    for (int i = 0; i < 30; ++i)
    {
        channel.update(loud, kDtMs);
        const double now = channel.snapshot().bands.sub;
        if (now + 1e-9 < prev)
            monotonicAttack = false;
        prev = now;
    }
    QVERIFY2(monotonicAttack, "Envelope must rise monotonically with loud input");
    const double plateau = prev;
    QVERIFY2(plateau > firstSub * 1.5, "Envelope must converge well above the first-frame value");

    bool monotonicRelease = true;
    double last = plateau;
    for (int i = 0; i < 30; ++i)
    {
        channel.update(silence, kDtMs);
        const double now = channel.snapshot().bands.sub;
        if (now > last + 1e-9)
            monotonicRelease = false;
        last = now;
    }
    QVERIFY2(monotonicRelease, "Envelope must decay monotonically with silent input");
    QVERIFY2(last < plateau * 0.9, "Envelope must release noticeably below plateau");

    const AudioSnapshot snap = channel.snapshot();
    QVERIFY(std::abs(snap.bands.low - (snap.bands.sub + snap.bands.bass) * 0.5) < 1e-9);
}

void AudioChannel_Test::testAgc()
{
    AudioChannelConfig cfg = fastConfig();
    cfg.agc.enabled = true;
    cfg.agc.releaseMs = 50.0;
    cfg.agc.maxGainDb = 18.0;
    cfg.agc.noiseFloorDb = -80.0;
    AudioChannel channel(cfg);

    const AudioFrame quiet = AudioTestUtils::makeSineFrame(1000.0, -40.0);
    warmUp(channel, quiet, 200);
    const double quietGain = channel.snapshot().volume.agc;
    QVERIFY2(quietGain > 5.0,
             qPrintable(QString("AGC must boost quiet input, got %1 dB").arg(quietGain)));
    QVERIFY2(quietGain <= cfg.agc.maxGainDb + 1e-6, "AGC gain must respect maxGainDb");

    const AudioFrame loud = AudioTestUtils::makeSineFrame(1000.0, -3.0);
    channel.update(loud, kDtMs);
    const double loudGain = channel.snapshot().volume.agc;
    QVERIFY2(loudGain < quietGain - 1.0,
             qPrintable(QString("AGC must drop on loud input: quiet=%1 loud=%2")
                            .arg(quietGain).arg(loudGain)));
}

void AudioChannel_Test::testTriggerFired()
{
    AudioChannelConfig cfg = fastConfig();
    AudioChannel channel(cfg);

    const AudioFrame loud = AudioTestUtils::makeSineFrame(1000.0, -3.0);

    int firedCount = 0;
    int firstFireFrame = -1;
    for (int i = 0; i < 20; ++i)
    {
        channel.update(loud, kDtMs);
        const TriggerState &t = channel.snapshot().volumeTrigger;
        if (t.firedThisFrame)
        {
            ++firedCount;
            if (firstFireFrame < 0)
                firstFireFrame = i;
        }
    }
    QCOMPARE(firedCount, 1);
    QVERIFY(firstFireFrame >= 0);
    QVERIFY2(channel.snapshot().volumeTrigger.active, "Trigger must remain active while above low threshold");

    for (int i = 0; i < 10; ++i)
    {
        channel.update(loud, kDtMs);
        QVERIFY(!channel.snapshot().volumeTrigger.firedThisFrame);
        QVERIFY(channel.snapshot().volumeTrigger.active);
    }

    const AudioFrame silence = AudioTestUtils::makeSilentFrame();
    int releasedCount = 0;
    for (int i = 0; i < 50; ++i)
    {
        channel.update(silence, kDtMs);
        if (channel.snapshot().volumeTrigger.releasedThisFrame)
            ++releasedCount;
    }
    QCOMPARE(releasedCount, 1);
    QVERIFY(!channel.snapshot().volumeTrigger.active);
}

void AudioChannel_Test::testTriggerCooldown()
{
    AudioChannelConfig cfg = fastConfig();
    cfg.triggers.holdMs = 10.0;
    cfg.triggers.cooldownMs = 200.0;
    AudioChannel channel(cfg);

    const AudioFrame loud = AudioTestUtils::makeSineFrame(1000.0, -3.0);
    const AudioFrame silence = AudioTestUtils::makeSilentFrame();

    channel.update(loud, kDtMs);
    QVERIFY(channel.snapshot().volumeTrigger.firedThisFrame);

    for (int i = 0; i < 5; ++i)
        channel.update(loud, kDtMs);
    channel.update(silence, kDtMs);
    channel.update(silence, kDtMs);
    QVERIFY2(channel.snapshot().volumeTrigger.cooldownRemainingMs > 0.0,
             "Cooldown must be armed after release");

    int firedDuringCooldown = 0;
    for (int i = 0; i < 10; ++i)
    {
        channel.update(loud, kDtMs);
        if (channel.snapshot().volumeTrigger.firedThisFrame)
            ++firedDuringCooldown;
    }
    QCOMPARE(firedDuringCooldown, 0);

    for (int i = 0; i < 25; ++i)
        channel.update(silence, kDtMs);
    int firedAfter = 0;
    for (int i = 0; i < 5; ++i)
    {
        channel.update(loud, kDtMs);
        if (channel.snapshot().volumeTrigger.firedThisFrame)
            ++firedAfter;
    }
    QCOMPARE(firedAfter, 1);
}

void AudioChannel_Test::testTriggerHold()
{
    AudioChannelConfig cfg = fastConfig();
    cfg.triggers.holdMs = 100.0;
    cfg.triggers.cooldownMs = 0.0;
    AudioChannel channel(cfg);

    const AudioFrame loud = AudioTestUtils::makeSineFrame(1000.0, -3.0);
    const AudioFrame silence = AudioTestUtils::makeSilentFrame();

    channel.update(loud, kDtMs);
    QVERIFY(channel.snapshot().volumeTrigger.firedThisFrame);
    QVERIFY(channel.snapshot().volumeTrigger.active);

    int activeWhileHeld = 0;
    int totalSteps = 9; // 9 * 10ms = 90ms < 100ms holdMs
    for (int i = 0; i < totalSteps; ++i)
    {
        channel.update(silence, kDtMs);
        if (channel.snapshot().volumeTrigger.active)
            ++activeWhileHeld;
    }
    QCOMPARE(activeWhileHeld, totalSteps);

    bool released = false;
    for (int i = 0; i < 10; ++i)
    {
        channel.update(silence, kDtMs);
        if (channel.snapshot().volumeTrigger.releasedThisFrame)
        {
            released = true;
            break;
        }
    }
    QVERIFY2(released, "Trigger must release once hold time has elapsed");
    QVERIFY(!channel.snapshot().volumeTrigger.active);
}

void AudioChannel_Test::testConfigUpdate()
{
    AudioChannelConfig cfg = fastConfig();
    cfg.brightnessFloor = 0.1;
    AudioChannel channel(cfg);

    const AudioFrame frame = AudioTestUtils::makeSineFrame(1000.0, -3.0);
    channel.update(frame, kDtMs);
    QCOMPARE(channel.snapshot().brightnessFloor, 0.1);

    AudioChannelConfig newCfg = cfg;
    newCfg.brightnessFloor = 0.42;
    channel.updateConfig(newCfg);

    QCOMPARE(channel.config().brightnessFloor, 0.42);
    QCOMPARE(channel.snapshot().brightnessFloor, 0.1);

    channel.update(frame, kDtMs);
    QCOMPARE(channel.snapshot().brightnessFloor, 0.42);
}

void AudioChannel_Test::testSnapshotThreadSafety()
{
    AudioChannelConfig cfg = fastConfig();
    AudioChannel channel(cfg);

    const AudioFrame loud = AudioTestUtils::makeSineFrame(440.0, -6.0);
    const AudioFrame silence = AudioTestUtils::makeSilentFrame();

    std::atomic<bool> stop{false};
    std::atomic<uint64_t> snapshotCount{0};

    std::thread reader([&]() {
        while (!stop.load(std::memory_order_relaxed))
        {
            const AudioSnapshot snap = channel.snapshot();
            volatile double sink = snap.bands.sub + snap.volume.smoothed + snap.brightnessFloor;
            (void) sink;
            snapshotCount.fetch_add(1, std::memory_order_relaxed);
        }
    });

    QElapsedTimer timer;
    timer.start();
    int updates = 0;
    while (timer.elapsed() < 200)
    {
        channel.update((updates % 2) ? loud : silence, kDtMs);
        ++updates;
    }
    stop.store(true, std::memory_order_relaxed);
    reader.join();

    QVERIFY2(updates > 10, "Should have run many update() calls");
    QVERIFY2(snapshotCount.load() > 10, "Reader thread should have fetched many snapshots");
}

void AudioChannel_Test::testBrightnessFloor()
{
    AudioChannelConfig cfg = fastConfig();
    cfg.brightnessFloor = 0.3;
    AudioChannel channel(cfg);

    const AudioFrame frame = AudioTestUtils::makeSilentFrame();
    channel.update(frame, kDtMs);
    QCOMPARE(channel.snapshot().brightnessFloor, 0.3);
}

namespace
{
// Helper: build an AudioFrame whose bands32 array has all 32 entries set to
// `bandValue`, so the averaged sub/bass/etc. bands all see exactly that value.
// The caller owns `storage` (kept alive while the frame is read).
AudioFrame makeBandsFrame(double bandValue, double (&storage)[32])
{
    for (int i = 0; i < 32; ++i)
        storage[i] = bandValue;

    AudioFrame f{};
    f.frameIndex = 0;
    f.sampleRate = 44100;
    f.fftSize = 2048;
    f.binCount = 1025;
    f.silent = false;
    f.samples = nullptr;
    f.sampleCount = 0;
    f.rms = 0.5;
    f.peak = 0.5;
    f.dcOffset = 0.0;
    f.magnitudes = nullptr;
    f.bands32 = storage;
    f.rmsDb = -6.0;          // well above default noise gate threshold
    f.peakDb = -6.0;
    f.crestFactor = 1.0;
    f.spectralFlux = 0.0;
    f.spectralCentroidHz = 1000.0;
    f.spectralRolloffHz = 4000.0;
    f.spectralFlatness = 0.5;
    f.noiseFloorDb = -96.0;
    f.beatDetected = false;
    return f;
}

// Helper: build an AudioFrame with a specific RMS but no spectral content,
// used to drive the volume trigger directly without smoothing surprises from
// per-band envelopes.
AudioFrame makeRmsFrame(double rms)
{
    AudioFrame f{};
    f.frameIndex = 0;
    f.sampleRate = 44100;
    f.fftSize = 2048;
    f.binCount = 1025;
    f.silent = false;
    f.samples = nullptr;
    f.sampleCount = 0;
    f.rms = rms;
    f.peak = rms;
    f.dcOffset = 0.0;
    f.magnitudes = nullptr;
    f.bands32 = nullptr;
    f.rmsDb = (rms > 1e-6) ? 20.0 * std::log10(rms) : -96.0;
    f.peakDb = f.rmsDb;
    f.crestFactor = 1.0;
    f.spectralFlux = 0.0;
    f.spectralCentroidHz = 0.0;
    f.spectralRolloffHz = 0.0;
    f.spectralFlatness = 0.0;
    f.noiseFloorDb = -96.0;
    f.beatDetected = false;
    return f;
}

AudioChannelConfig exactMathConfig()
{
    AudioChannelConfig cfg = AudioChannelConfig::defaults();
    cfg.envelope.attackMs = 20.0;
    cfg.envelope.releaseMs = 200.0;
    cfg.agc.enabled = false;
    cfg.agc.inputGainLinear = 1.0;     // make math deterministic
    cfg.noiseGate.thresholdDb = -200.0;
    cfg.noiseGate.holdMs = 1e9;
    cfg.volumeSmoothingMs = 1.0;
    return cfg;
}
}

void AudioChannel_Test::testEnvelopeExactAlpha()
{
    AudioChannelConfig cfg = exactMathConfig();
    AudioChannel channel(cfg);

    double bands[32];
    AudioFrame loud = makeBandsFrame(0.8, bands);
    channel.update(loud, 10.0);

    // alpha_attack = 1 - exp(-10/20) = 0.3934693
    // expected     = 0 + 0.3934693 * (0.8 - 0) = 0.3147755
    const double expectedAfterAttack = 0.3147754775;
    const double sub1 = channel.snapshot().bands.sub;
    QVERIFY2(std::abs(sub1 - expectedAfterAttack) < 1e-3,
             qPrintable(QString("attack: expected ~%1, got %2")
                            .arg(expectedAfterAttack).arg(sub1)));

    double zeros[32];
    AudioFrame quiet = makeBandsFrame(0.0, zeros);
    channel.update(quiet, 10.0);

    // alpha_release = 1 - exp(-10/200) = 0.04877058
    // expected      = 0.3147755 + 0.04877058 * (0 - 0.3147755) = 0.2994243
    const double expectedAfterRelease = 0.2994243348;
    const double sub2 = channel.snapshot().bands.sub;
    QVERIFY2(std::abs(sub2 - expectedAfterRelease) < 1e-3,
             qPrintable(QString("release: expected ~%1, got %2")
                            .arg(expectedAfterRelease).arg(sub2)));
}

void AudioChannel_Test::testEnvelopeSteadyState()
{
    AudioChannelConfig cfg = exactMathConfig();
    cfg.envelope.attackMs = 25.0;
    cfg.envelope.releaseMs = 180.0;
    AudioChannel channel(cfg);

    double bands[32];
    const double inputBand = 0.6;
    AudioFrame frame = makeBandsFrame(inputBand, bands);

    // Run 200 frames at 40ms — well past the time constant.
    double samples[10] = {};
    for (int i = 0; i < 200; ++i)
    {
        channel.update(frame, 40.0);
        if (i >= 190)
            samples[i - 190] = channel.snapshot().bands.sub;
    }

    double minV = samples[0], maxV = samples[0];
    for (int i = 1; i < 10; ++i)
    {
        minV = std::min(minV, samples[i]);
        maxV = std::max(maxV, samples[i]);
    }
    QVERIFY2((maxV - minV) < 1e-3,
             qPrintable(QString("steady-state deviation too high: %1").arg(maxV - minV)));

    const double finalV = samples[9];
    QVERIFY2(std::abs(finalV - inputBand) < inputBand * 0.01,
             qPrintable(QString("final %1 not within 1%% of input %2").arg(finalV).arg(inputBand)));
}

void AudioChannel_Test::testTriggerSchmittNoChatter()
{
    AudioChannelConfig cfg = exactMathConfig();
    cfg.triggers.highThreshold = 0.6;
    cfg.triggers.lowThreshold = 0.4;
    cfg.triggers.cooldownMs = 0.0;
    cfg.triggers.holdMs = 0.0;
    AudioChannel channel(cfg);

    // Phase 1: alternate between thresholds without ever crossing high.
    for (int i = 0; i < 50; ++i)
    {
        const double v = (i % 2 == 0) ? 0.55 : 0.45;
        channel.update(makeRmsFrame(v), 10.0);
        QVERIFY2(!channel.snapshot().volumeTrigger.firedThisFrame,
                 qPrintable(QString("Schmitt: trigger fired at frame %1 (v=%2)").arg(i).arg(v)));
        QVERIFY2(!channel.snapshot().volumeTrigger.active,
                 qPrintable(QString("Schmitt: trigger active at frame %1 (v=%2)").arg(i).arg(v)));
    }

    // Phase 2: cross the high threshold.
    channel.update(makeRmsFrame(0.65), 10.0);
    QVERIFY(channel.snapshot().volumeTrigger.firedThisFrame);
    QVERIFY(channel.snapshot().volumeTrigger.active);

    // Phase 3: alternate between thresholds, above low — Schmitt holds.
    for (int i = 0; i < 20; ++i)
    {
        const double v = (i % 2 == 0) ? 0.55 : 0.45;
        channel.update(makeRmsFrame(v), 10.0);
        QVERIFY2(!channel.snapshot().volumeTrigger.firedThisFrame,
                 qPrintable(QString("Schmitt re-fire at frame %1 (v=%2)").arg(i).arg(v)));
        QVERIFY2(channel.snapshot().volumeTrigger.active,
                 qPrintable(QString("Schmitt drop at frame %1 (v=%2)").arg(i).arg(v)));
    }

    // Phase 4: drop below low threshold — release must fire.
    channel.update(makeRmsFrame(0.35), 10.0);
    QVERIFY(channel.snapshot().volumeTrigger.releasedThisFrame);
    QVERIFY(!channel.snapshot().volumeTrigger.active);
}

void AudioChannel_Test::testFrameRateIndependence()
{
    AudioChannelConfig cfg = exactMathConfig();
    cfg.envelope.attackMs = 50.0;
    cfg.envelope.releaseMs = 200.0;
    AudioChannel channelA(cfg);
    AudioChannel channelB(cfg);

    double bandsA[32];
    double bandsB[32];
    const double inputBand = 0.7;
    AudioFrame fA = makeBandsFrame(inputBand, bandsA);
    AudioFrame fB = makeBandsFrame(inputBand, bandsB);

    // 400ms total each.
    for (int i = 0; i < 20; ++i)
        channelA.update(fA, 20.0);
    for (int i = 0; i < 10; ++i)
        channelB.update(fB, 40.0);

    const double subA = channelA.snapshot().bands.sub;
    const double subB = channelB.snapshot().bands.sub;
    const double rel = std::abs(subA - subB) / std::max(subA, subB);
    QVERIFY2(rel < 0.05,
             qPrintable(QString("frame-rate dependent: A=%1 B=%2 rel=%3")
                            .arg(subA).arg(subB).arg(rel)));
}

void AudioChannel_Test::testMultiChannelIsolation()
{
    AudioChannelConfig cfgA = exactMathConfig();
    cfgA.envelope.attackMs = 10.0;
    cfgA.envelope.releaseMs = 50.0;
    cfgA.triggers.highThreshold = 0.3;
    cfgA.triggers.lowThreshold = 0.2;

    AudioChannelConfig cfgB = exactMathConfig();
    cfgB.envelope.attackMs = 100.0;
    cfgB.envelope.releaseMs = 500.0;
    cfgB.triggers.highThreshold = 0.8;
    cfgB.triggers.lowThreshold = 0.7;

    AudioChannel chA(cfgA);
    AudioChannel chB(cfgB);

    double bands[32];
    AudioFrame loud = makeBandsFrame(0.6, bands);

    // Drive both channels with the same loud frame for several steps. With A's
    // 10ms attack vs B's 100ms attack, A converges to the input much faster.
    for (int i = 0; i < 5; ++i)
    {
        chA.update(loud, 10.0);
        chB.update(loud, 10.0);
    }

    const AudioSnapshot sA = chA.snapshot();
    const AudioSnapshot sB = chB.snapshot();
    QVERIFY2(sA.bands.sub > sB.bands.sub,
             qPrintable(QString("A env should exceed B: A=%1 B=%2").arg(sA.bands.sub).arg(sB.bands.sub)));
    // A's threshold (0.3) is well below the band value; A's envelope rose past it.
    QVERIFY2(sA.triggers[0].active, "A trigger should be active (low threshold)");
    // B's threshold (0.8) is above the steady value; B should not have fired.
    QVERIFY2(!sB.triggers[0].active, "B trigger must not be active (high threshold)");

    // Now feed silence — A releases ~10x faster (50ms vs 500ms). After enough
    // frames, A's envelope drops below B's even though A started higher.
    AudioFrame silence = AudioTestUtils::makeSilentFrame();
    for (int i = 0; i < 10; ++i)
    {
        chA.update(silence, 10.0);
        chB.update(silence, 10.0);
    }
    const AudioSnapshot sA2 = chA.snapshot();
    const AudioSnapshot sB2 = chB.snapshot();
    QVERIFY2(sA2.bands.sub < sB2.bands.sub,
             qPrintable(QString("A should release faster: A=%1 B=%2").arg(sA2.bands.sub).arg(sB2.bands.sub)));
}

QTEST_MAIN(AudioChannel_Test)
