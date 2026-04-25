/*
  Q Light Controller Plus - Unit test
  beatquantize_test.h

  Tests for 1/16 beat subdivision support in Function::beatsToTime()
  and Function::timeToBeats().

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef BEATQUANTIZE_TEST_H
#define BEATQUANTIZE_TEST_H

#include <QObject>

class BeatQuantize_Test : public QObject
{
    Q_OBJECT

private slots:
    void beatsToTime_wholeBeats();
    void beatsToTime_eighthBeat();

    void beatsToTime_sixteenthBeat();
    void beatsToTime_sixteenthAt200BPM();
    void timeToBeats_sixteenthSnap();
    void timeToBeats_currentlySnapsTo125();
    void roundTrip_sixteenth();
    void sixteenthsSumToWholeBeat();
    void quantizerTable_coversAllSixteenths();
};

#endif // BEATQUANTIZE_TEST_H
