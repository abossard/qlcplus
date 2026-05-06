/*
  Q Light Controller Plus - Unit test
  audioslice_test.h

  Vertical-slice integration test for the audio pipeline.

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef AUDIOSLICE_TEST_H
#define AUDIOSLICE_TEST_H

#include <QObject>

class AudioSlice_Test final : public QObject
{
    Q_OBJECT

private slots:
    void testEndToEndPipeline();
    void testProfileResolutionChain();
    void testLegacyFieldsPreserved();
};

#endif // AUDIOSLICE_TEST_H
