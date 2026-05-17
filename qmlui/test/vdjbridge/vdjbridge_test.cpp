/*
  Q Light Controller Plus - Unit test
  vdjbridge_test.cpp
*/

#include <QtTest>
#include <QSignalSpy>

#include "vdjbridge_test.h"
#include "vdjbridge.h"

void VdjBridge_Test::initialState()
{
    VdjBridge b;
    QCOMPARE(b.connected(), false);
    QCOMPARE(b.bpm(), 0.0);
    QCOMPARE(b.beatPos(), 0.0);
    QCOMPARE(b.beatCount(), 0);
    QVERIFY(b.currentSongName().isEmpty());
    QVERIFY(b.currentSongPath().isEmpty());
}

void VdjBridge_Test::beatUpdatesBpmAndConnected()
{
    VdjBridge b;
    QSignalSpy connectedSpy(&b, &VdjBridge::connectedChanged);
    QSignalSpy beatSpy(&b, &VdjBridge::beatChanged);

    b.onBeatInfo(128.0, 4.0, false);

    QCOMPARE(b.connected(), true);
    QCOMPARE(b.bpm(), 128.0);
    QCOMPARE(b.beatPos(), 4.0);
    QCOMPARE(b.beatCount(), 1);
    QCOMPARE(connectedSpy.count(), 1);
    QCOMPARE(beatSpy.count(), 1);

    b.onBeatInfo(128.0, 5.0, false);
    QCOMPARE(b.beatCount(), 2);
    // connected stays true — no extra connectedChanged emission
    QCOMPARE(connectedSpy.count(), 1);
}

void VdjBridge_Test::beatChangeResetsCounter()
{
    VdjBridge b;
    b.onBeatInfo(120.0, 1.0, false);
    b.onBeatInfo(120.0, 2.0, false);
    b.onBeatInfo(120.0, 3.0, false);
    QCOMPARE(b.beatCount(), 3);

    // change=true marks a new segment — counter resets, then this beat counts as 1
    b.onBeatInfo(120.0, 1.0, true);
    QCOMPARE(b.beatCount(), 1);
}

void VdjBridge_Test::songChangeEmitsSongChanged()
{
    VdjBridge b;
    QSignalSpy songSpy(&b, &VdjBridge::songChanged);
    QSignalSpy elapsedSpy(&b, &VdjBridge::songElapsedChanged);

    QVariantMap song;
    song["name"] = "Strobe";
    song["artist"] = "Deadmau5";
    song["bpm"] = 128.0;
    song["path"] = "/music/strobe.mp3";

    b.onSongReceived(song);

    QCOMPARE(songSpy.count(), 1);
    QCOMPARE(elapsedSpy.count(), 0);
    QCOMPARE(b.currentSongName(), QStringLiteral("Strobe"));
    QCOMPARE(b.currentArtist(), QStringLiteral("Deadmau5"));
    QCOMPARE(b.currentSongPath(), QStringLiteral("/music/strobe.mp3"));
    QCOMPARE(b.bpm(), 128.0);
}

void VdjBridge_Test::sameSongElapsedEmitsElapsedOnly()
{
    VdjBridge b;
    QVariantMap song;
    song["name"] = "Strobe";
    song["artist"] = "Deadmau5";
    song["bpm"] = 128.0;
    song["elapsed"] = 5.0;
    b.onSongReceived(song);

    QSignalSpy songSpy(&b, &VdjBridge::songChanged);
    QSignalSpy elapsedSpy(&b, &VdjBridge::songElapsedChanged);

    // Same track, advanced elapsed
    song["elapsed"] = 12.0;
    b.onSongReceived(song);

    QCOMPARE(songSpy.count(), 0);
    QCOMPARE(elapsedSpy.count(), 1);
    QCOMPARE(b.currentElapsed(), 12.0);
}

void VdjBridge_Test::songWithoutBpmDoesNotZeroBpm()
{
    VdjBridge b;
    // Establish a BPM via a beat event
    b.onBeatInfo(140.0, 0.0, false);
    QCOMPARE(b.bpm(), 140.0);

    // Now a song event without bpm (or with 0) — must not clobber it
    QVariantMap song;
    song["name"] = "Track A";
    song["bpm"] = 0.0;
    b.onSongReceived(song);

    QCOMPARE(b.bpm(), 140.0);
}

QTEST_MAIN(VdjBridge_Test)
