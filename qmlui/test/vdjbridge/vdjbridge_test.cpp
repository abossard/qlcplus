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
    QCOMPARE(b.beatCount(), 0);
}

void VdjBridge_Test::beatTicksCounterAndConnected()
{
    VdjBridge b;
    QSignalSpy connectedSpy(&b, &VdjBridge::connectedChanged);
    QSignalSpy beatSpy(&b, &VdjBridge::beatReceived);

    b.onBeat();

    QCOMPARE(b.connected(), true);
    QCOMPARE(b.beatCount(), 1);
    QCOMPARE(connectedSpy.count(), 1);
    QCOMPARE(beatSpy.count(), 1);

    b.onBeat();
    QCOMPARE(b.beatCount(), 2);
    // connected stays true — no extra connectedChanged emission
    QCOMPARE(connectedSpy.count(), 1);
}

QTEST_MAIN(VdjBridge_Test)
