/*
  Q Light Controller Plus - Unit test

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef AUDIOCHANNEL_TEST_H
#define AUDIOCHANNEL_TEST_H

#include <QObject>

class AudioChannel_Test final : public QObject
{
    Q_OBJECT

private slots:
    void testEnvelopeSmoothing();
    void testAgc();
    void testTriggerFired();
    void testTriggerCooldown();
    void testTriggerHold();
    void testConfigUpdate();
    void testSnapshotThreadSafety();
    void testBrightnessFloor();
    void testEnvelopeExactAlpha();
    void testEnvelopeSteadyState();
    void testTriggerSchmittNoChatter();
    void testFrameRateIndependence();
    void testMultiChannelIsolation();
};

#endif // AUDIOCHANNEL_TEST_H
