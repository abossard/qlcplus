#include <QtTest>

#include "audiochannel_test.h"
#include "audiochannel.h"
#include "audioframe.h"

#include <algorithm>

void AudioChannel_Test::fullBankPowers()
{
    auto config = AudioChannelConfig::defaults();
    config.aubio.melBanks.high.post.enabled = false;
    config.aubio.melBanks.mid.post.enabled = false;
    AudioChannel channel(config);
    AubioResults results;
    results.melHighCount = results.melMidCount = results.melLowCount = 24;
    std::fill_n(results.melHigh, 1, 2.0);
    std::fill(results.melHigh + 1, results.melHigh + 3, 0.2);
    std::fill(results.melHigh + 3, results.melHigh + 15, 0.4);
    std::fill(results.melHigh + 15, results.melHigh + 22, 0.7);
    std::fill(results.melHigh + 22, results.melHigh + 24, 9.0);
    std::fill_n(results.melMid, 24, 0.9);
    AudioFrame frame;
    frame.aubio = &results;
    frame.rms = 0.1;
    frame.rmsDb = -20.0;
    frame.volumeNorm = 0.8;
    frame.silent = false;
    frame.sourceEpoch = 1;
    channel.update(frame, 1000.0 / 60.0);
    channel.update(frame, 1000.0 / 60.0);
    const auto snap = channel.snapshot();
    QCOMPARE(snap.beatPower, 0.97);
    QVERIFY(std::abs(snap.bassPower - 0.194) < 1e-12);
    QVERIFY(std::abs(snap.mids - 0.388) < 1e-12);
    QVERIFY(std::abs(snap.highs - 0.679) < 1e-12);
    QVERIFY(std::abs(snap.lows - 0.582) < 1e-12);
}

void AudioChannel_Test::postFilterFreeze()
{
    MelPostProcessor reference, interrupted;
    constexpr double input[] = {0.1, 0.2, 0.8, 0.03, 0.4};
    double a[5], b[5], noveltyA[5], noveltyB[5];
    reference.process(input, 5, a, noveltyA);
    interrupted.process(input, 5, b, noveltyB);
    const double gain = interrupted.melGain();
    for (int i = 0; i < 1800; ++i)
        interrupted.process(input, 5, b, noveltyB, true);
    QCOMPARE(interrupted.melGain(), gain);
    QVERIFY(std::all_of(b, b + 5, [](double x) { return x == 0.0; }));
    constexpr double changed[] = {0.4, 0.1, 0.03, 0.5, 0.7};
    reference.process(changed, 5, a, noveltyA);
    interrupted.process(changed, 5, b, noveltyB);
    for (int i = 0; i < 5; ++i)
    {
        QCOMPARE(a[i], b[i]);
        QCOMPARE(noveltyA[i], noveltyB[i]);
    }
}

void AudioChannel_Test::postFilterReference_data()
{
    QTest::addColumn<double>("scale");
    QTest::addColumn<bool>("closeGate");
    QTest::newRow("normal-magnitudes") << 1.0 << false;
    QTest::newRow("quiet-magnitudes") << 1e-12 << false;
    QTest::newRow("gate-after-first-frame") << 1.0 << true;
    QTest::newRow("quiet-gate-after-first-frame") << 1e-12 << true;
}

void AudioChannel_Test::postFilterReference()
{
    QFETCH(double, scale);
    QFETCH(bool, closeGate);
    // ledfx-distinct-melbank-outputs-v1, native filterbank with five identity
    // coefficients. Public output writes must not overwrite deferred filter state.
    static const double input[3][5] = {
        {0.1, 0.2, 0.8, 0.03, 0.4}, {0.4, 0.1, 0.03, 0.5, 0.7}, {0.07, 0.8, 0.2, 0.6, 0.1}};
    static const double expected[3][5] = {
        {0.03891469064389192, 0.1516764705638721, 2.304238762791988, 0.0036635846608090434, 0.5911842376761681},
        {0.5860260972325633, 0.0727601920735724, 0.6938377354841335, 0.9074915719237857, 1.7622877482183728},
        {0.1888086909899229, 2.1932064688654043, 0.31019567858004915, 1.2556813383859866, 0.5548672120232524}};
    static const double expectedNovelty[3][5] = {
        {},
        {0.5362238895975568, -0.00011837441773544995, -0.0024156015409617757, 0.8858418103164434, 1.1477985506824147},
        {0.4772371047705247, 2.077474753694014, -0.002652880410583256, 1.2271026004260464, 0.9755567259889149}};
    MelPostProcessor processor;
    double processed[5], novelty[5];
    for (int frame = 0; frame < 3; ++frame)
    {
        double raw[5];
        for (int i = 0; i < 5; ++i)
            raw[i] = input[frame][i] * scale;
        processor.process(raw, 5, processed, novelty);
        for (int i = 0; i < 5; ++i)
        {
            QVERIFY(std::abs(processed[i] - expected[frame][i]) < 1e-6);
            QVERIFY(std::abs(novelty[i] - expectedNovelty[frame][i]) < 1e-6);
        }
        if (frame == 0 && closeGate)
        {
            processor.process(input[2], 5, processed, novelty, true);
            QVERIFY(std::all_of(processed, processed + 5, [](double v) { return v == 0.0; }));
            QVERIFY(std::all_of(novelty, novelty + 5, [](double v) { return v == 0.0; }));
        }
        std::fill_n(processed, 5, -17.0);
        std::fill_n(novelty, 5, 23.0);
    }
}

void AudioChannel_Test::publicationCounters()
{
    AudioChannel channel(AudioChannelConfig::defaults());
    AubioResults result;
    AudioFrame frame;
    frame.aubio = &result;
    frame.sourceEpoch = 7;
    frame.silent = false;
    frame.rms = 0.1;
    frame.rmsDb = -20.0;
    frame.volumeNorm = 0.8;
    for (uint64_t n = 1; n <= 10; ++n)
    {
        frame.frameIndex = n;
        result.onsets.hfc = n % 2 == 0;
        result.beat = n % 3 == 0;
        frame.beatDetected = result.beat;
        channel.update(frame, 1000.0 / 60.0);
    }
    const auto firstRead = channel.snapshot();
    const auto secondRead = channel.snapshot();
    QCOMPARE(firstRead.events.onset, uint64_t(5));
    QCOMPARE(firstRead.events.beat, uint64_t(3));
    QCOMPARE(secondRead.events.onset, firstRead.events.onset);
    QCOMPARE(secondRead.events.beat, firstRead.events.beat);
    channel.invalidate(8);
    QVERIFY(!channel.snapshot().available);
    QCOMPARE(channel.snapshot().events.onset, uint64_t(0));
}

QTEST_MAIN(AudioChannel_Test)
