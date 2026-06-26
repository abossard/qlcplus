/*
  Q Light Controller Plus - Unit test
  audiobpmtag_test.h

  Licensed under the Apache License, Version 2.0
*/

#ifndef AUDIOBPMTAG_TEST_H
#define AUDIOBPMTAG_TEST_H

#include <QObject>
#include <QtTest>

class AudioBpmTag_Test : public QObject
{
    Q_OBJECT

private slots:
    void parsesBpmAcrossId3Variants_data();
    void parsesBpmAcrossId3Variants();
    void readsBpmFromFile();
    void appliesFileBpmToShow();
};

#endif // AUDIOBPMTAG_TEST_H
