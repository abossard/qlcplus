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
    void disconnectThenReconnectAdoptsAgain();
    void stoppedShowStartsAgainInsideTheContent();
    void theRunnerEndsTheShowUntilAJumpBack();
    void aQueuedFinishWaitsForTheNextTracksSample();
    void mutedTrackDoesNotRestartTheShow();
    void beatShowEndsWhereTheRunnerEnds();
    void recordingKeepsTheShowPastItsContent_data();
    void recordingKeepsTheShowPastItsContent();
    void theFinalCommandAtTheExtentRuns();
    void pauseBeforeTheFirstTickCancelsTheStart();
    void handoverDuringTheStartDrainStopsTheShow();
    void trackChangeTickSwitchesOnce();
    void trackChangeBeforeTheFirstSampleStartsB_data();
    void trackChangeBeforeTheFirstSampleStartsB();
    void nestedDisableDuringAHandoverLeavesBUntouched();
    void ejectKeepsTheTempo_data();
    void ejectKeepsTheTempo();
    void activeDeckFollowsThePerDeckMasterdeck();
    void capture_mixWeights_data();
    void capture_mixWeights();
    void deletedShowBindsAgainWhenItComesBack();
    void handoverInsideALoadWriteResolvesOnce();
    void capture_ejectReleasesPerform();
    void capture_ejectPerform_data();
    void capture_ejectPerform();
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
