/*
  Q Light Controller Plus
  osc_audio_source_test.cpp — Validates OscAudioSource + SnapshotMapper

  Copyright (c) Massimo Callegari
  Licensed under the Apache License, Version 2.0.
*/

#include <QCoreApplication>
#include <QUdpSocket>
#include <QTest>
#include <QSignalSpy>
#include <QByteArray>
#include <cstring>

#include "oscaudiosource.h"
#include "audiochannel.h"
#include "audiochannelconfig.h"

// Helper to build a single OSC message with one float argument
static QByteArray buildOscFloat(const char *address, float value)
{
    QByteArray data;

    // Address (null-terminated, padded to 4 bytes)
    int addrLen = int(strlen(address)) + 1;
    int addrPadded = (addrLen + 3) & ~3;
    data.append(address, int(strlen(address)));
    data.append(addrPadded - int(strlen(address)), '\0');

    // Type tag ",f\0\0"
    data.append(",f", 2);
    data.append(2, '\0');

    // Float value (big-endian)
    char bytes[4];
    auto *src = reinterpret_cast<const char *>(&value);
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    bytes[0] = src[3]; bytes[1] = src[2]; bytes[2] = src[1]; bytes[3] = src[0];
#else
    memcpy(bytes, src, 4);
#endif
    data.append(bytes, 4);

    return data;
}

class OscAudioSourceTest : public QObject
{
    Q_OBJECT

private slots:

    void testSchmittTrigger()
    {
        OscSchmittTrigger trigger { 0.65, 0.45, 80.0, 120.0 };

        // Below threshold — should not fire
        auto ts1 = trigger.update(0.3, 16.0);
        QVERIFY(!ts1.firedThisFrame);
        QVERIFY(!ts1.active);

        // Cross high threshold — should fire
        auto ts2 = trigger.update(0.7, 16.0);
        QVERIFY(ts2.firedThisFrame);
        QVERIFY(ts2.active);

        // Stay above low threshold — should remain active, not re-fire
        auto ts3 = trigger.update(0.5, 16.0);
        QVERIFY(!ts3.firedThisFrame);
        QVERIFY(ts3.active);

        // Drop below low threshold after hold period
        for (int i = 0; i < 10; i++) // 160ms of hold
            trigger.update(0.5, 16.0);
        auto ts4 = trigger.update(0.3, 16.0);
        QVERIFY(ts4.releasedThisFrame);
        QVERIFY(!ts4.active);

        // During cooldown — should not re-fire
        auto ts5 = trigger.update(0.7, 16.0);
        QVERIFY(!ts5.firedThisFrame);
        QVERIFY(!ts5.active);
    }

    void testOscParsing()
    {
        // Create an OscAudioSource, start it on a test port
        OscAudioSource source;
        source.setPort(19999); // Use non-standard port for testing
        source.start();
        QVERIFY(source.isRunning());

        // Create a channel to inject into
        AudioChannel channel(AudioChannelConfig::defaults());
        source.setTargetChannel(&channel);

        // Send an OSC message
        QUdpSocket sender;
        QByteArray msg = buildOscFloat("/audio/level/bass", 0.75f);
        sender.writeDatagram(msg, QHostAddress::LocalHost, 19999);

        // Process events to receive the datagram
        QTest::qWait(50);

        // Check that the raw state was updated
        SynRawState raw = source.rawState();
        QVERIFY(raw.hasData);
        QCOMPARE(int(raw.levelBass * 100), 75);

        // Send BPM
        QByteArray bpmMsg = buildOscFloat("/audio/bpm", 128.0f);
        sender.writeDatagram(bpmMsg, QHostAddress::LocalHost, 19999);
        QTest::qWait(50);

        raw = source.rawState();
        QCOMPARE(int(raw.bpm), 128);

        // Check that snapshot was injected
        AudioSnapshot snap = channel.snapshot();
        QVERIFY(snap.lows > 0.0);
        QCOMPARE(int(snap.music.bpm), 128);

        source.stop();
        QVERIFY(!source.isRunning());
    }

    void testSnapshotMapping()
    {
        OscAudioSource source;
        source.setPort(19998);
        source.start();

        AudioChannel channel(AudioChannelConfig::defaults());
        channel.setExternalSource(true);
        source.setTargetChannel(&channel);

        QUdpSocket sender;

        // Send a complete set of audio data
        auto send = [&](const char *addr, float val) {
            QByteArray msg = buildOscFloat(addr, val);
            sender.writeDatagram(msg, QHostAddress::LocalHost, 19998);
        };

        send("/audio/level/bass", 0.8f);
        send("/audio/level/mid", 0.5f);
        send("/audio/level/midhigh", 0.3f);
        send("/audio/level/high", 0.2f);
        send("/audio/level/all", 0.6f);
        send("/audio/bpm", 130.0f);
        send("/audio/bpm/bpmconfidence", 0.95f);
        send("/audio/beat/onbeat", 1.0f);
        send("/audio/beat/beattime", 3.0f);
        send("/audio/presence/bass", 0.4f);
        send("/audio/energy/intensity", 0.7f);
        send("/audio/hits/bass", 0.9f);

        // Wait for data to be received and injected
        QTest::qWait(100);

        AudioSnapshot snap = channel.snapshot();

        // Verify band powers
        QVERIFY(snap.lows > 0.5);
        QVERIFY(snap.mids > 0.3);
        QVERIFY(snap.highs > 0.1);

        // Verify BPM
        QCOMPARE(int(snap.music.bpm), 130);
        QVERIFY(snap.music.beatConfidence > 0.9);

        // Verify volume
        QVERIFY(snap.volume.raw > 0.5);
        QVERIFY(snap.volume.volumeNorm > 0.6);

        // Verify noise gate is open (loud signal)
        QVERIFY(!snap.noiseGateClosed);

        source.stop();
    }

    void testExternalSourceGating()
    {
        AudioChannel channel(AudioChannelConfig::defaults());

        // Without external source, update() should work normally
        QVERIFY(!channel.hasExternalSource());

        // With external source, injectSnapshot works
        channel.setExternalSource(true);
        QVERIFY(channel.hasExternalSource());

        AudioSnapshot snap;
        snap.lows = 0.42;
        snap.music.bpm = 140.0;
        channel.injectSnapshot(snap);

        AudioSnapshot result = channel.snapshot();
        QCOMPARE(int(result.lows * 100), 42);
        QCOMPARE(int(result.music.bpm), 140);

        // Disable external source
        channel.setExternalSource(false);
        QVERIFY(!channel.hasExternalSource());
    }
};

QTEST_MAIN(OscAudioSourceTest)
#include "osc_audio_source_test.moc"
