/*
  Q Light Controller Plus - Unit test
  edmlooks_test.h

  Licensed under the Apache License, Version 2.0
*/

#ifndef EDMLOOKS_TEST_H
#define EDMLOOKS_TEST_H

#include <QObject>

class EdmLooks_Test : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void testMappingCombosFollowWorkspaceLoad();
    void analysisSectionsFollowAudioStart();
    void analysisBandPosition_data();
    void analysisBandPosition();
};

#endif
