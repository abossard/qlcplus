/*
  Q Light Controller Plus - Unit test
  vdjtelemetryclient_test.h
*/

#ifndef VDJTELEMETRYCLIENT_TEST_H
#define VDJTELEMETRYCLIENT_TEST_H

#include <QObject>

class VdjTelemetryClient_Test : public QObject
{
    Q_OBJECT

private slots:
    // NDJSON parser tests
    void parseSubscribedDeckTrigger_data();
    void parseSubscribedDeckTrigger();
    void parseSubscribedGlobalTrigger_data();
    void parseSubscribedGlobalTrigger();
    void parseBeatEvent();
    void parseMalformedJson();
    void parseUnknownEvent();

    // Subscription handshake
    void subscriptionMessageShape();
    void subscriptionTriggerCount_data();
    void subscriptionTriggerCount();

    // TCP server lifecycle
    void serverStartStop();
    void clientConnectAndHandshake();
    void clientReconnectReplacesOld();
    void partialLineBuffering();

    // VdjBridge integration
    void bridgeTelemetryStatusProperty();
    void bridgeDeckTriggerRouting();
    void bridgeGlobalTriggerRouting();
    void bridgeClientDisconnectResetsDeckState_data();
    void bridgeClientDisconnectResetsDeckState();
    void stopDropsTheClient();
    void framing_data();
    void framing();
    void beatsOfAConnectionStartAtItsSnapshot_data();
    void beatsOfAConnectionStartAtItsSnapshot();
    void anObjectOpenPastTheCapDropsTheClient_data();
    void anObjectOpenPastTheCapDropsTheClient();
    void aConnectionThatEndsAdvertisesAgain_data();
    void aConnectionThatEndsAdvertisesAgain();
    void aBonjourFailureShowsInTheStatus();
    void parametersAreKeptWithTheInputsUniverse_data();
    void parametersAreKeptWithTheInputsUniverse();
    void aPortEditRebindsWhenFinished_data();
    void aPortEditRebindsWhenFinished();
    void aSavedPortRestartsAnInputOpenedOnABusyPort();
    void openingAnOpenInputBindsABusyPortAgain();
    void bonjourAdvertisesOnlyAListeningServer_data();
    void bonjourAdvertisesOnlyAListeningServer();
    void aPortOutsideOneTo65535IsIgnored_data();
    void aPortOutsideOneTo65535IsIgnored();
};

#endif
