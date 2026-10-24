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
    void deckTransportDrivesTheShow();
    void handoverLeavesTheShowInOrder();
    void disengageStopsTheShow_data();
    void disengageStopsTheShow();
    void deckResetReleasesBeforeItsPositionDrops_data();
    void deckResetReleasesBeforeItsPositionDrops();
    void stoppedShowStartsAgainInsideTheContent();
    void pastTheContentEndsUntilAJumpBack();
    void pauseBeforeTheFirstTickCancelsTheStart();
    void activeDeckFollowsThePerDeckMasterdeck();
    void capture_ejectReleasesPerform();
    void performAdoptsAndReleasesSyncSource();
    void performAdoptionSuppressesAudioForAlreadyRunningShow();
    void performReleaseOrSwitchClearsTransientAudioSuppression_data();
    void performReleaseOrSwitchClearsTransientAudioSuppression();
    void debugTableTracksLatestValuesAndCounts();
    void engineBpmFollowsVdjAndIgnoresJitter();
    void engineBpmOnlyForThePluginBeatGenerator();
    void engineBpmDropsLowWhenPaused();
    void engineBpmLockReleasedOnDisconnect_data();
    void engineBpmLockReleasedOnDisconnect();
};

#endif
