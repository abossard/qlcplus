/*
  Q Light Controller Plus - Unit test
  djmanager_test.h

  Licensed under the Apache License, Version 2.0
*/

#ifndef DJMANAGER_TEST_H
#define DJMANAGER_TEST_H

#include <QObject>
#include <QtTest>

class Doc;

class DjManager_Test : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // DjSongModel
    void modelUpsertAddsAndDedupsByFilepath();
    void modelReadsTheShowFromTheIndex();
    void modelActiveAndPlayingFlags();

    // DjManager orchestration
    void songChangeAddsOneEntryAndAssignsExistingShow();
    void repeatedSongChangeDoesNotRecreateShow();
    void songChangeCreatesShowWhenNoneExists_data();
    void songChangeCreatesShowWhenNoneExists();
    void metadataFirstReloadDoesNotCorruptPreviousRow();
    void removedShowUnassignsButKeepsRow();

    // Per-song actions
    void createShowAction();
    void assignShowAction();
    void loadShowEmitsRequest();

    // Perform mode
    void performModeDelegatesToBridge();
    void performModeGatesAutoPlay();
    void performOffStopsRunningShow();
    void performRunsEveryDeckAndMovesDominance();
    void externalPositionSyncDrivesShowWhenPerforming();
    void performShowsActiveShowInShowManager();
    void performResolvesShowAfterWorkspaceReload();

    // Deck table / topology
    void deckTableReflectsFsmStateAndUpdates();
    void deckTablePositionReflectsActiveDeck();
    void deckTableShowsTheMixWeight_data();
    void deckTableShowsTheMixWeight();
    void getDecksDrivesDeckCount();
    void deckCountSettingLimitsTable();
    void perDeckMasterdeckSetsActiveDeck();

    // Show creation and the index
    void newShowWinsAfterAWorkspaceLoad();
    void playingASongModifiesTheWorkspace();
    void emptyArtistReachesTheRow();
    void newShowBecomesTheSongsShow();
    void deletedAssignedShowUnassignsTheRow();
    void workspaceLoadKeepsThePlayingSong_data();
    void workspaceLoadKeepsThePlayingSong();
    void twoDecksWithOneSongBuildOneShow();
    void liveWorkspaceClearKeepsPerformOffTheRecycledId();

private:
    Doc *m_doc = nullptr;
};

#endif // DJMANAGER_TEST_H
