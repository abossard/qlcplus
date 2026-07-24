/*
  Q Light Controller Plus - Unit test
  audiocapture_test.h

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef AUDIOCAPTURE_TEST_H
#define AUDIOCAPTURE_TEST_H

#include <QObject>

class AudioCapture_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void negotiatedFormatUpdatesPipeline_data();
    void negotiatedFormatUpdatesPipeline();
    void negotiatedFormatTransitions_data();
    void negotiatedFormatTransitions();
    void negotiatedFormatUpdatesAubio();
    void negotiatedFormatUpdatesTracker();
    void invalidNegotiatedFormat_data();
    void invalidNegotiatedFormat();
    void qt6SampleConversion_data();
    void qt6SampleConversion();
    void qt6UnsupportedSampleFormat_data();
    void qt6UnsupportedSampleFormat();
    void qt6PartialFrameReads();
    void qt6PoisonedBlockRecovery_data();
    void qt6PoisonedBlockRecovery();
};

#endif // AUDIOCAPTURE_TEST_H
