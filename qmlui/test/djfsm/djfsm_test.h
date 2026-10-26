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
    void placeholderTitleEmptiesDeck();
    void playPauseTransitions();
    void masterDeckDrivesActiveDeckAndSong();
    void emptyFilepathResetsDeck();
    void disconnectResetsEverything();
    void defaultTwoDecksIgnoresPhantomDeck();
    void emptyDeckIsNeverPlaying();
    void setDeckCountResetsDroppedDecks();
    void metadataFirstOrderAnnouncesCorrectSong();
    void loadWrites_data();
    void loadWrites();
    void twoDecksLoadInInterleavedWrites();
    void onChangeSemantics_data();
    void onChangeSemantics();
    void volumeIsTracked();
    void positionIsTrackedAndThrottled();
    void playBeforeFilepathIsRecovered();
    void playAcceptsBoolAndNumericForms();
    void perDeckMasterdeckDrivesActiveDeck_data();
    void perDeckMasterdeckDrivesActiveDeck();
    void loadedOffEmptiesDeck();
    void disconnectDropsActiveDeckFirst();
    void titleAfterFilepathChangesActiveSong();
    void masterFlagSurvivesTheDeckCount_data();
    void masterFlagSurvivesTheDeckCount();
    void ejectKeepsTheFaderAndTheLastValues();
    void ejectAndReload_data();
    void ejectAndReload();
    void artistAfterTitleChangesActiveSong();
    void writeNotificationsFollowItsSettledState();
    void capture_trackChangeSameBpm();
    void capture_consecutiveLoads();
    void capture_eject();
    void capture_playPauseLoad();
    void capture_masterHandover();
    void capture_connectSnapshot();
    void capture_ejectReload_data();
    void capture_ejectReload();
};

#endif // DJFSM_TEST_H
