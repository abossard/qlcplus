/*
  Q Light Controller Plus - Unit test
  vdjbridge_test.h
*/

#ifndef VDJBRIDGE_TEST_H
#define VDJBRIDGE_TEST_H

#include <QObject>

class VdjBridge_Test : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void initialState();
    void beatTicksCounterAndConnected();
    void autoStartShowOnPlay();
    void autoPauseShowOnPlayOff();
    void autoResumeShowOnPlayOn();
    void autoStartPauseResumeCycle();
    void performAdoptsAndReleasesSyncSource();
    void performAdoptionSuppressesAudioForAlreadyRunningShow();
    void performReleaseOrSwitchClearsTransientAudioSuppression_data();
    void performReleaseOrSwitchClearsTransientAudioSuppression();
    void debugTableTracksLatestValuesAndCounts();
    void engineBpmFollowsVdjAndIgnoresJitter();
    void engineBpmDropsLowWhenPaused();
    void engineBpmLockReleasedOnDisconnect();
};

#endif
