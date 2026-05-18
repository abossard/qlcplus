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

QTEST_MAIN(VdjBridge_Test)
