/*
  Q Light Controller Plus - Unit test
  djfsm_test.h

  Licensed under the Apache License, Version 2.0
*/

#ifndef DJFSM_TEST_H
#define DJFSM_TEST_H

#include <QObject>
#include <QtTest>

class DjFsm_Test : public QObject
{
    Q_OBJECT

private slots:
    void initialState();
    void loadSequenceAnnouncesOnceAndReachesLoaded();
    void repeatedMetadataDoesNotReannounce();
    void newFilepathOnSameDeckReannounces();
    void placeholderTitleIsIgnored();
    void playPauseTransitions();
    void masterDeckDrivesActiveDeckAndSong();
    void emptyFilepathResetsDeck();
    void disconnectResetsEverything();
    void defaultTwoDecksIgnoresPhantomDeck();
    void emptyDeckIsNeverPlaying();
    void setDeckCountResetsDroppedDecks();
    void metadataFirstOrderAnnouncesCorrectSong();
    void filepathFirstAfterPreviousSongIsNotStale();
    void splitFieldOrderDoesNotAnnounceStale();
    void volumeIsTracked();
    void positionIsTrackedAndThrottled();
    void playBeforeFilepathIsRecovered();
    void playAcceptsBoolAndNumericForms();
};

#endif // DJFSM_TEST_H
