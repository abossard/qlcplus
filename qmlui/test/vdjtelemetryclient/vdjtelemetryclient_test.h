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
    // VdjDeckModel tests
    void deckModelInitialState();
    void deckModelSettersAndThrottle();
    void deckModelReset();

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
    void bridgeBeatSuppressesOS2L();
    void bridgeClientDisconnectResetsDeckModels();
};

#endif
