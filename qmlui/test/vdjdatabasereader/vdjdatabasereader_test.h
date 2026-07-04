/*
  Q Light Controller Plus - Unit test
  vdjdatabasereader_test.h

  Licensed under the Apache License, Version 2.0
*/

#ifndef VDJDATABASEREADER_TEST_H
#define VDJDATABASEREADER_TEST_H

#include <QObject>
#include <QtTest>

class VdjDatabaseReader_Test : public QObject
{
    Q_OBJECT

private slots:
    void parsesSongGridAndPois();
    void scanPhaseUsedWhenNoBeatgridPoi();
    void beatPeriodStoredAsSecondsOrBpm();
    void songWithoutScanIsInvalid();
    void unknownSongReturnsInvalid();
    void lookupInDatabaseCachesAndRefreshesOnMtime();
    void missingDatabaseFileReturnsInvalid();
    void databaseCandidatesIncludeHomeAndDrive();
    void validatesDatabaseRootElement();
    void malformedXmlDoesNotCrash();
};

#endif // VDJDATABASEREADER_TEST_H
