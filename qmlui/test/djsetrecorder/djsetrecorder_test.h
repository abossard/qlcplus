/*
  Q Light Controller Plus - Unit test
  djsetrecorder_test.h

  Licensed under the Apache License, Version 2.0
*/

#ifndef DJSETRECORDER_TEST_H
#define DJSETRECORDER_TEST_H

#include <QObject>
#include <QtTest>

class DjSetRecorder_Test : public QObject
{
    Q_OBJECT

private slots:
    void setName();
    void setIsAShowOfDeckTracks_data();
    void setIsAShowOfDeckTracks();
    void clipsFollowTheSongStays_data();
    void clipsFollowTheSongStays();
    void clipAutomationFollowsTheDeck_data();
    void clipAutomationFollowsTheDeck();
    void clipsReachTheSetAtABoundedRate_data();
    void clipsReachTheSetAtABoundedRate();
    void aClipWaitsForARunningSetToStop_data();
    void aClipWaitsForARunningSetToStop();
    void aWaitingClipChecksItsSongAgain_data();
    void aWaitingClipChecksItsSongAgain();
    void growthAfterASaveMarksTheWorkspaceModified_data();
    void growthAfterASaveMarksTheWorkspaceModified();
    void setChangesRestartTheAutosave_data();
    void setChangesRestartTheAutosave();
    void performOffFinalizesTheSet_data();
    void performOffFinalizesTheSet();
    void recordHeadFollowsTheSet_data();
    void recordHeadFollowsTheSet();
    void replayReproducesTheSet_data();
    void replayReproducesTheSet();
    void replayedSetHoldsEverySongStay_data();
    void replayedSetHoldsEverySongStay();
    void recordedSetSurvivesSaveAndLoad();
};

#endif // DJSETRECORDER_TEST_H
