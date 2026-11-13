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

    // review findings: the director on real Virtual Console buttons
    void lookPressSkipsNonToggleButtons_data();
    void lookPressSkipsNonToggleButtons();
    void monitoredLookIsReleased();
    void noPressWhenBlocked_data();
    void noPressWhenBlocked();
    void pressReachesALookOnAnotherFramePage();
    void releaseIsAlwaysAllowed_data();
    void releaseIsAlwaysAllowed();
    void lookPressIsTraced();
    void testMappingCombosFollowWorkspaceLoad();
    void analysisSectionsFollowAudioStart();
    void analysisBandPosition_data();
    void analysisBandPosition();
};

#endif
