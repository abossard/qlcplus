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
    void songChangeCreatesShowWhenNoneExists();
    void metadataFirstReloadDoesNotCorruptPreviousRow();
    void clearShowSurvivesUnrelatedFunctionAdd();
    void removedShowUnassignsButKeepsRow();

    // Per-song actions
    void createShowAction();
    void assignAndClearShowAction();
    void loadShowEmitsRequest();

    // Perform mode
    void performModeDelegatesToBridge();
    void performModeGatesAutoPlay();
    void performOffStopsRunningShow();
    void performLoadsAndSwitchesActiveShow();
    void externalPositionSyncDrivesShowWhenPerforming();
    void performShowsActiveShowInShowManager();
    void performResolvesShowAfterWorkspaceReload();

    // Deck table / topology
    void deckTableReflectsFsmStateAndUpdates();
    void deckTablePositionReflectsActiveDeck();
    void getDecksDrivesDeckCount();
    void deckCountSettingLimitsTable();
    void perDeckMasterdeckSetsActiveDeck();

    // Show creation and the index
    void deferredShowCreationRevalidates_data();
    void deferredShowCreationRevalidates();
    void playingASongModifiesTheWorkspace();

private:
    Doc *m_doc = nullptr;
};

#endif // DJMANAGER_TEST_H
