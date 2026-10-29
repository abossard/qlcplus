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
    void handoverKeepsTheOldShowPlaying();
    void everyDeckDrivesItsShow_data();
    void everyDeckDrivesItsShow();
    void weightIsTheMasterIntensity_data();
    void weightIsTheMasterIntensity();
    void aDeckStopsOnlyItsShow_data();
    void aDeckStopsOnlyItsShow();
    void disengageReleasesEveryShow_data();
    void disengageReleasesEveryShow();
    void oneShowOnTwoDecksRunsOnce_data();
    void oneShowOnTwoDecksRunsOnce();
    void endedIsPerDeck_data();
    void endedIsPerDeck();
    void aFinishBeforeAMoveStaysWithItsDeck_data();
    void aFinishBeforeAMoveStaysWithItsDeck();
    void disconnectReleasesEveryShowAtOnce();
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
    void ejectDuringTheStartDrainStopsTheShow();
    void trackChangeTickSwitchesOnce();
    void trackChangeBeforeTheFirstSampleStartsB_data();
    void trackChangeBeforeTheFirstSampleStartsB();
    void nestedDisableDuringAHandoverReleasesBoth();
    void ejectKeepsTheTempo_data();
    void ejectKeepsTheTempo();
    void activeDeckFollowsThePerDeckMasterdeck();
    void getDecksSettlesTheWeightWithItsWrite_data();
    void getDecksSettlesTheWeightWithItsWrite();
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
    void engineBpmCrawlsOnlyWhenNothingPlays_data();
    void engineBpmCrawlsOnlyWhenNothingPlays();
    void anEndRestartsNothing();
    void docTeardownWhileDecksPlay();
    void engineBpmLockReleasedOnDisconnect_data();
    void engineBpmLockReleasedOnDisconnect();
};

#endif
