/*
  Q Light Controller Plus - Unit test
  edmstructure_test.h

  Licensed under the Apache License, Version 2.0
*/

#ifndef EDMSTRUCTURE_TEST_H
#define EDMSTRUCTURE_TEST_H

#include <QObject>
#include <QtTest>

class EdmStructure_Test : public QObject
{
    Q_OBJECT

private slots:
    void init();

    void analysedSongIsReady();
    void unevenStepsReportEveryCrossingOnce_data();
    void unevenStepsReportEveryCrossingOnce();
    void liveValues_data();
    void liveValues();
    void backwardJumpFiresAgain();
    void twoDecksKeepTheirOwnPosition();
    void ejectedDeckIsLost();
    void unknownSongIsAnalysedWithoutReloading();
    void failedRunCanBeRetried_data();
    void failedRunCanBeRetried();
    void samePathRunsOnce();
    void movedFileIsFoundByContent();
    void autoAnalyseOffQueuesNothing();
    void autoAnalyseIsOffByDefault();
    void analyseByHandRunsOnceAndReachesTheDeck();
    void leftDeckGetsNoStaleState();
    void ejectDuringRunKeepsDeckEmpty();
    void queueRunsInOrderOnce();
    void blockedQueueWaits();

    void mappingXmlRoundTrip();
    void mappingLoadKeepsUnknownAndSkipsBroken();
    void mappingLoadsFunctions_data();
    void mappingLoadsFunctions();
    void newWorkspaceClearsMapping();
    void mappingDefaultsAreLooks_data();
    void mappingDefaultsAreLooks();

    // review findings
    void destroyingParentWhileRunningDoesNotCallToolAgain();
    void readyEntryWithMissingFileCanBeRetried();
    void analysisWrittenOutsideTheQueueReachesTheDeck();
    void deckCountDecreaseLosesTheFollowedDeck();
    void nonMp3SongIsNotQueued();
    void newSongClearsLiveValues();
    void rewrittenFinalFileReachesTheDeck();
    void analysisFolderChangesAreCoalesced();
    void readyFileWinsOverAFailedEntryInTheSongList();
};

#endif
