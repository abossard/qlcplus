/*
  Q Light Controller Plus - Unit test

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef AUDIOANALYZER_TEST_H
#define AUDIOANALYZER_TEST_H

#include <QObject>

class AudioAnalyzerTest : public QObject
{
    Q_OBJECT

private slots:
    void testSilence();
    void testSineWave();
    void testWhiteNoise();
    void testImpulse();
    void testSpectralFlux();
    void testSpectralFluxFormula();
    void testSpectralFluxBinCountChange();
    void testBands32();
    void testNoiseFloorTracking();
};

#endif // AUDIOANALYZER_TEST_H
