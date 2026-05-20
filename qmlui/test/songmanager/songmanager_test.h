/*
  Q Light Controller Plus
  songmanager_test.h

  Licensed under the Apache License, Version 2.0
*/

#ifndef SONGMANAGER_TEST_H
#define SONGMANAGER_TEST_H

#include <QObject>
#include <QtTest>

class Doc;

class SongManagerTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // SongListModel
    void testAddSong();
    void testAddSongDedup();
    void testClear();
    void testSetPlaying();
    void testMarkPlayed();
    void testMarkEdited();
    void testRebuildFromDoc();

    // SongSortFilterProxyModel
    void testSortAlphabetical();
    void testSortRecentlyPlayed();
    void testSortRecentlyEdited();
    void testSortDescending();
    void testSearchFilter();
    void testSearchAndSort();

private:
    Doc *m_doc = nullptr;
};

#endif // SONGMANAGER_TEST_H
