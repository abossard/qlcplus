/*
  Q Light Controller Plus - Unit test

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef AUDIOFRAME_TEST_H
#define AUDIOFRAME_TEST_H

#include <QObject>

class AudioFrame_Test final : public QObject
{
    Q_OBJECT

private slots:
    void silentFrame();
    void sineFrame();
    void noiseFrame();
    void impulseFrame();
};

#endif // AUDIOFRAME_TEST_H
