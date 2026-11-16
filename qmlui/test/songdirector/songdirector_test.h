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
    void createBlocksOncePerShow();
    void createBlocksNeedsAnalysedSong_data();
    void createBlocksNeedsAnalysedSong();
    void workspaceRoundTrip();
};

#endif
