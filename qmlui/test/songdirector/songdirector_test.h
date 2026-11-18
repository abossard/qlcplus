/*
  Q Light Controller Plus - Unit test
  songdirector_test.h

  Licensed under the Apache License, Version 2.0
*/

#ifndef SONGDIRECTOR_TEST_H
#define SONGDIRECTOR_TEST_H

#include <QObject>
#include <QtTest>

class SongDirector_Test : public QObject
{
    Q_OBJECT

private slots:
    void songBlocks_data();
    void songBlocks();
    void createBlocks_data();
    void createBlocks();
    void createBlocksFromDefaults_data();
    void createBlocksFromDefaults();
    void createBlocksOncePerShow();
    void createBlocksNeedsAnalysedSong_data();
    void createBlocksNeedsAnalysedSong();
    void workspaceRoundTrip();
    void holdOnlyOnSidecarBlocks_data();
    void holdOnlyOnSidecarBlocks();
    void dropOnBlock_data();
    void dropOnBlock();
    void syncCreatesBlocksOnce_data();
    void syncCreatesBlocksOnce();
    void followAnalysis_data();
    void followAnalysis();
    void movedAudioKeepsAuthoredLooks();
    void trimmedAudioUsesVisibleSourceSections();
    void extendingTrimRestoresAuthoredLooks();
    void reusedCollectionIdIsIndependent();
    void removedBlockStopsFirst_data();
    void removedBlockStopsFirst();
    void goneSectionContentJoins_data();
    void goneSectionContentJoins();
    void sectionActions_data();
    void sectionActions();
    void editsOverReanalysis_data();
    void editsOverReanalysis();
    void blocksFollowSectionEdits_data();
    void blocksFollowSectionEdits();
    void sectionEditsRoundTrip();
    void oldLabelPinLoads_data();
    void oldLabelPinLoads();
    void oldBlocksSnapBackOnLoad();
    void shortPieceBetweenPins_data();
    void shortPieceBetweenPins();
    void splitCopyIsUserContent_data();
    void splitCopyIsUserContent();
    void editsOnlyOnSongShows_data();
    void editsOnlyOnSongShows();
    void editWhileShowRuns();
};

#endif
