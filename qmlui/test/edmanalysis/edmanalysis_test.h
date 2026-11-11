/*
  Q Light Controller Plus - Unit test
  edmanalysis_test.h

  Licensed under the Apache License, Version 2.0
*/

#ifndef EDMANALYSIS_TEST_H
#define EDMANALYSIS_TEST_H

#include <QObject>
#include <QtTest>

class EdmAnalysis_Test : public QObject
{
    Q_OBJECT

private slots:
    void applyEventSequence_data();
    void applyEventSequence();
    void unknownOrBrokenLinesKeepState_data();
    void unknownOrBrokenLinesKeepState();
    void trackFieldsAndArguments();
    void readerFindsSections_data();
    void readerFindsSections();
    void readerSectionConfidence_data();
    void readerSectionConfidence();
};

#endif
