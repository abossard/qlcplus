/*
  Q Light Controller Plus - Unit test
  vcaudiotriggers_golden_test.h

  Golden tests for the data contracts that VCAudioTriggers (qmlui) depends on:
   - The legacy spectrum aggregation that backs lowsPower/midsPower/highsPower
   - The AudioChannel snapshot bands that back the new perceptual Q_PROPERTYs
     (subPower, bassPower, lowMidPower, midPower, highPower)
   - Profile resolution via Doc::audioProfileForFunction()
   - audioProfileId XML round-trip on RGBMatrix (the persisted owner used by
     the script engine to look up a profile)

  VCAudioTriggers itself lives in qmlui/ and is not directly instantiated in
  this engine-level test (see docs/audio-dsp-reviews/p2b-golden-expose.md).

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef VCAUDIOTRIGGERS_GOLDEN_TEST_H
#define VCAUDIOTRIGGERS_GOLDEN_TEST_H

#include <QObject>

class VCAudioTriggersGoldenTest final : public QObject
{
    Q_OBJECT

private slots:
    void testLegacyBarsStillWork();
    void testNewPerceptualBands();
    void testDualPathCoexistence();
    void testProfileIdPersistence();
    void testAudioProfileForFunction();
};

#endif // VCAUDIOTRIGGERS_GOLDEN_TEST_H
