/*
  Q Light Controller Plus - Unit test
  vdjtelemetryclient_test.cpp
*/

#include <QtTest>
#include <QSignalSpy>
#include <QTcpSocket>

#include "vdjtelemetryclient_test.h"
#include "vdjtelemetryclient.h"
#include "djfsm.h"
#include "vdjbridge.h"
#include "vdjbridgeplugin.h"

// ========================================================================
// NDJSON parser tests
// ========================================================================

void VdjTelemetryClient_Test::parseSubscribedDeckTrigger_data()
{
    QTest::addColumn<QByteArray>("json");
    QTest::addColumn<int>("expectedDeck");
    QTest::addColumn<QString>("expectedTrigger");
    QTest::addColumn<QVariant>("expectedValue");

    QTest::newRow("deck1_title")
        << QByteArray(R"({"evt":"subscribed","trigger":"deck 1 get_title","value":"My Song"})")
        << 0 << "get_title" << QVariant("My Song");

    QTest::newRow("deck2_position")
        << QByteArray(R"({"evt":"subscribed","trigger":"deck 2 get_position","value":0.75})")
        << 1 << "get_position" << QVariant(0.75);

    QTest::newRow("deck3_play_bool")
        << QByteArray(R"({"evt":"subscribed","trigger":"deck 3 play","value":true})")
        << 2 << "play" << QVariant(true);

    QTest::newRow("deck4_bpm")
        << QByteArray(R"({"evt":"subscribed","trigger":"deck 4 get_bpm","value":128.5})")
        << 3 << "get_bpm" << QVariant(128.5);

    QTest::newRow("deck1_volume_string")
        << QByteArray(R"({"evt":"subscribed","trigger":"deck 1 volume","value":"0.85"})")
        << 0 << "volume" << QVariant(0.85);
}

void VdjTelemetryClient_Test::parseSubscribedDeckTrigger()
{
    QFETCH(QByteArray, json);
    QFETCH(int, expectedDeck);
    QFETCH(QString, expectedTrigger);
    QFETCH(QVariant, expectedValue);

    VdjTelemetryClient client;
    QSignalSpy spy(&client, &VdjTelemetryClient::deckTriggerReceived);

    client.parseLine(json);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy[0][0].toInt(), expectedDeck);
    QCOMPARE(spy[0][1].toString(), expectedTrigger);

    // Compare values with type awareness
    QVariant actual = spy[0][2].value<QVariant>();
    if (expectedValue.typeId() == QMetaType::Double)
        QCOMPARE(actual.toDouble(), expectedValue.toDouble());
    else if (expectedValue.typeId() == QMetaType::Bool)
        QCOMPARE(actual.toBool(), expectedValue.toBool());
    else
        QCOMPARE(actual.toString(), expectedValue.toString());
}

void VdjTelemetryClient_Test::parseSubscribedGlobalTrigger_data()
{
    QTest::addColumn<QByteArray>("json");
    QTest::addColumn<QString>("expectedTrigger");
    QTest::addColumn<QVariant>("expectedValue");

    QTest::newRow("master_volume")
        << QByteArray(R"({"evt":"subscribed","trigger":"master_volume","value":0.8})")
        << "master_volume" << QVariant(0.8);

    QTest::newRow("crossfader")
        << QByteArray(R"({"evt":"subscribed","trigger":"crossfader","value":0.5})")
        << "crossfader" << QVariant(0.5);

    QTest::newRow("masterdeck")
        << QByteArray(R"({"evt":"subscribed","trigger":"masterdeck","value":2})")
        << "masterdeck" << QVariant(2.0);
}

void VdjTelemetryClient_Test::parseSubscribedGlobalTrigger()
{
    QFETCH(QByteArray, json);
    QFETCH(QString, expectedTrigger);
    QFETCH(QVariant, expectedValue);

    VdjTelemetryClient client;
    QSignalSpy spy(&client, &VdjTelemetryClient::globalTriggerReceived);

    client.parseLine(json);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy[0][0].toString(), expectedTrigger);
    QCOMPARE(spy[0][1].value<QVariant>().toDouble(), expectedValue.toDouble());
}

void VdjTelemetryClient_Test::parseBeatEvent()
{
    VdjTelemetryClient client;
    QSignalSpy spy(&client, &VdjTelemetryClient::beatReceived);

    client.parseLine(R"({"evt":"beat","pos":233,"bpm":123.94,"strength":0.9,"change":false})");

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy[0][0].toInt(), 233);
    QCOMPARE(spy[0][1].toDouble(), 123.94);
    QCOMPARE(spy[0][2].toDouble(), 0.9);
    QCOMPARE(spy[0][3].toBool(), false);
}

void VdjTelemetryClient_Test::parseMalformedJson()
{
    VdjTelemetryClient client;
    QSignalSpy deckSpy(&client, &VdjTelemetryClient::deckTriggerReceived);
    QSignalSpy globalSpy(&client, &VdjTelemetryClient::globalTriggerReceived);
    QSignalSpy beatSpy(&client, &VdjTelemetryClient::beatReceived);

    // Should not crash or emit signals
    client.parseLine("{not valid json");
    client.parseLine("");
    client.parseLine("{}");

    QCOMPARE(deckSpy.count(), 0);
    QCOMPARE(globalSpy.count(), 0);
    QCOMPARE(beatSpy.count(), 0);
}

void VdjTelemetryClient_Test::parseUnknownEvent()
{
    VdjTelemetryClient client;
    QSignalSpy deckSpy(&client, &VdjTelemetryClient::deckTriggerReceived);

    client.parseLine(R"({"evt":"unknown_event","data":"stuff"})");

    QCOMPARE(deckSpy.count(), 0);
}

// ========================================================================
// Subscription handshake
// ========================================================================

void VdjTelemetryClient_Test::subscriptionMessageShape()
{
    QByteArray msg = VdjTelemetryClient::buildSubscriptionMessage(25);

    // Must end with newline
    QVERIFY(msg.endsWith('\n'));

    // Must be valid JSON
    QJsonDocument doc = QJsonDocument::fromJson(msg.trimmed());
    QVERIFY(doc.isObject());

    QJsonObject obj = doc.object();
    QCOMPARE(obj.value("evt").toString(), QString("subscribe"));
    QCOMPARE(obj.value("frequency").toString(), QString("25"));
    QVERIFY(obj.value("trigger").isArray());
}

void VdjTelemetryClient_Test::subscriptionTriggerCount()
{
    QByteArray msg = VdjTelemetryClient::buildSubscriptionMessage();
    QJsonDocument doc = QJsonDocument::fromJson(msg.trimmed());
    QJsonArray triggers = doc.object().value("trigger").toArray();

    // 4 decks × 25 per-deck triggers + 8 globals = 108
    // (Exact count depends on kDeckTriggers and kGlobalTriggers)
    QVERIFY(triggers.size() >= 100);
    QVERIFY(triggers.size() <= 120);

    // Verify some expected triggers exist
    QStringList triggerStrings;
    for (const QJsonValue &v : triggers)
        triggerStrings.append(v.toString());

    QVERIFY(triggerStrings.contains("deck 1 get_title"));
    QVERIFY(triggerStrings.contains("deck 4 get_position"));
    QVERIFY(triggerStrings.contains("master_volume"));
    QVERIFY(triggerStrings.contains("crossfader"));
}

// ========================================================================
// TCP server lifecycle
// ========================================================================

void VdjTelemetryClient_Test::serverStartStop()
{
    VdjTelemetryClient client;
    QCOMPARE(client.status(), VdjTelemetryClient::Idle);

    // Use port 0 to let OS assign a free port
    QVERIFY(client.start(0));
    QCOMPARE(client.status(), VdjTelemetryClient::Listening);

    client.stop();
    QCOMPARE(client.status(), VdjTelemetryClient::Idle);
}

void VdjTelemetryClient_Test::clientConnectAndHandshake()
{
    VdjTelemetryClient server;
    QVERIFY(server.start(18050));
    QCOMPARE(server.status(), VdjTelemetryClient::Listening);

    QSignalSpy connSpy(&server, &VdjTelemetryClient::clientConnected);

    QTcpSocket fakeClient;
    fakeClient.connectToHost("127.0.0.1", 18050);
    QVERIFY(fakeClient.waitForConnected(2000));
    QTRY_COMPARE_WITH_TIMEOUT(connSpy.count(), 1, 2000);
    QCOMPARE(server.status(), VdjTelemetryClient::ClientConnected);

    // Read the subscription handshake — use QTRY since event loop is shared
    QTRY_VERIFY_WITH_TIMEOUT(fakeClient.bytesAvailable() > 0, 2000);
    QByteArray data = fakeClient.readAll();
    QVERIFY(data.endsWith('\n'));

    QJsonDocument doc = QJsonDocument::fromJson(data.trimmed());
    QVERIFY(doc.isObject());
    QCOMPARE(doc.object().value("evt").toString(), QString("subscribe"));

    fakeClient.disconnectFromHost();
    server.stop();
}

void VdjTelemetryClient_Test::clientReconnectReplacesOld()
{
    VdjTelemetryClient server;
    QVERIFY(server.start(18051));

    QSignalSpy connSpy(&server, &VdjTelemetryClient::clientConnected);
    QSignalSpy discSpy(&server, &VdjTelemetryClient::clientDisconnected);

    // First client
    QTcpSocket client1;
    client1.connectToHost("127.0.0.1", 18051);
    QVERIFY(client1.waitForConnected(2000));
    QTRY_COMPARE_WITH_TIMEOUT(connSpy.count(), 1, 2000);

    // Second client — should replace first
    QTcpSocket client2;
    client2.connectToHost("127.0.0.1", 18051);
    QVERIFY(client2.waitForConnected(2000));
    QTRY_COMPARE_WITH_TIMEOUT(connSpy.count(), 2, 2000);
    QTRY_COMPARE_WITH_TIMEOUT(discSpy.count(), 1, 2000);

    QCOMPARE(server.status(), VdjTelemetryClient::ClientConnected);

    // Read handshake on client2
    QTRY_VERIFY_WITH_TIMEOUT(client2.bytesAvailable() > 0, 2000);
    QByteArray data = client2.readAll();
    QVERIFY(data.contains("subscribe"));

    client2.disconnectFromHost();
    server.stop();
}

void VdjTelemetryClient_Test::partialLineBuffering()
{
    VdjTelemetryClient server;
    QVERIFY(server.start(18052));

    QSignalSpy deckSpy(&server, &VdjTelemetryClient::deckTriggerReceived);

    QTcpSocket fakeClient;
    fakeClient.connectToHost("127.0.0.1", 18052);
    QVERIFY(fakeClient.waitForConnected(2000));

    // Read and discard the handshake
    QTRY_VERIFY_WITH_TIMEOUT(fakeClient.bytesAvailable() > 0, 2000);
    fakeClient.readAll();

    // Send a partial line
    fakeClient.write(R"({"evt":"subscribed","trigger":"deck 1 get_tit)");
    fakeClient.flush();
    QTest::qWait(100);
    QCOMPARE(deckSpy.count(), 0);

    // Send the rest with newline
    fakeClient.write(R"(le","value":"Partial Test"})");
    fakeClient.write("\n");
    fakeClient.flush();

    QTRY_COMPARE_WITH_TIMEOUT(deckSpy.count(), 1, 2000);
    QCOMPARE(deckSpy[0][0].toInt(), 0); // deck 1 = index 0
    QCOMPARE(deckSpy[0][1].toString(), QString("get_title"));
    QCOMPARE(deckSpy[0][2].value<QVariant>().toString(), QString("Partial Test"));

    fakeClient.disconnectFromHost();
    server.stop();
}

// ========================================================================
// VdjBridge integration
// ========================================================================

void VdjTelemetryClient_Test::bridgeTelemetryStatusProperty()
{
    VdjBridge bridge;
    QCOMPARE(bridge.telemetryStatus(), QString("Idle"));
    QCOMPARE(bridge.telemetryConnected(), false);

    VdjBridgePlugin plugin;
    plugin.setParameter(0, 0, QLCIOPlugin::Input,
                        QStringLiteral("hostPort"), 18053);
    plugin.setParameter(0, 0, QLCIOPlugin::Input,
                        QStringLiteral("bonjourEnabled"), false);
    bridge.attachVdjPlugin(&plugin);
    QVERIFY(plugin.openInput(0, 0));
    QCOMPARE(bridge.telemetryStatus(), QString("Listening"));
    QCOMPARE(bridge.telemetryConnected(), false);

    plugin.closeInput(0, 0);
    QCOMPARE(bridge.telemetryStatus(), QString("Idle"));
}

void VdjTelemetryClient_Test::bridgeDeckTriggerRouting()
{
    VdjBridge bridge;
    VdjBridgePlugin plugin;
    plugin.setParameter(0, 0, QLCIOPlugin::Input,
                        QStringLiteral("hostPort"), 18054);
    plugin.setParameter(0, 0, QLCIOPlugin::Input,
                        QStringLiteral("bonjourEnabled"), false);
    bridge.attachVdjPlugin(&plugin);
    QVERIFY(plugin.openInput(0, 0));

    QTcpSocket fakeClient;
    fakeClient.connectToHost("127.0.0.1", 18054);
    QVERIFY(fakeClient.waitForConnected(2000));

    // Read handshake
    QTRY_VERIFY_WITH_TIMEOUT(fakeClient.bytesAvailable() > 0, 2000);
    fakeClient.readAll();

    // Send deck 1 title
    fakeClient.write(R"({"evt":"subscribed","trigger":"deck 1 get_title","value":"Bridge Test"})" "\n");
    fakeClient.write(R"({"evt":"subscribed","trigger":"deck 1 get_bpm","value":130.0})" "\n");
    fakeClient.flush();

    // The DjFsm is the single source of truth for deck state.
    DjFsm *fsm = bridge.djFsm();
    QTRY_COMPARE_WITH_TIMEOUT(fsm->deckAt(0).song.title, QString("Bridge Test"), 1000);
    QTRY_VERIFY_WITH_TIMEOUT(qFuzzyCompare(fsm->deckAt(0).song.bpm, 130.0), 1000);

    fakeClient.disconnectFromHost();
    plugin.closeInput(0, 0);
}

void VdjTelemetryClient_Test::bridgeGlobalTriggerRouting()
{
    VdjBridge bridge;
    VdjBridgePlugin plugin;
    plugin.setParameter(0, 0, QLCIOPlugin::Input,
                        QStringLiteral("hostPort"), 18055);
    plugin.setParameter(0, 0, QLCIOPlugin::Input,
                        QStringLiteral("bonjourEnabled"), false);
    bridge.attachVdjPlugin(&plugin);
    QVERIFY(plugin.openInput(0, 0));

    QSignalSpy mixerSpy(&bridge, &VdjBridge::globalMixerChanged);
    QSignalSpy masterDeckSpy(&bridge, &VdjBridge::masterDeckChanged);

    QTcpSocket fakeClient;
    fakeClient.connectToHost("127.0.0.1", 18055);
    QVERIFY(fakeClient.waitForConnected(2000));
    QTRY_VERIFY_WITH_TIMEOUT(fakeClient.bytesAvailable() > 0, 2000);
    fakeClient.readAll();

    fakeClient.write(R"({"evt":"subscribed","trigger":"master_volume","value":0.75})" "\n");
    fakeClient.write(R"({"evt":"subscribed","trigger":"masterdeck","value":2})" "\n");
    fakeClient.flush();

    QTRY_VERIFY_WITH_TIMEOUT(mixerSpy.count() >= 1, 2000);
    QTRY_COMPARE_WITH_TIMEOUT(masterDeckSpy.count(), 1, 2000);

    QCOMPARE(bridge.masterVolume(), 0.75);
    QCOMPARE(bridge.masterDeck(), 1); // VDJ sends 1-based "2", we store 0-based 1

    fakeClient.disconnectFromHost();
    plugin.closeInput(0, 0);
}

void VdjTelemetryClient_Test::bridgeBeatSuppressesOS2L()
{
    VdjBridge bridge;
    VdjBridgePlugin plugin;
    plugin.setParameter(0, 0, QLCIOPlugin::Input,
                        QStringLiteral("hostPort"), 18056);
    plugin.setParameter(0, 0, QLCIOPlugin::Input,
                        QStringLiteral("bonjourEnabled"), false);
    bridge.attachVdjPlugin(&plugin);
    QVERIFY(plugin.openInput(0, 0));

    QSignalSpy beatSpy(&bridge, &VdjBridge::beatReceived);

    QTcpSocket fakeClient;
    fakeClient.connectToHost("127.0.0.1", 18056);
    QVERIFY(fakeClient.waitForConnected(2000));
    QTRY_VERIFY_WITH_TIMEOUT(fakeClient.bytesAvailable() > 0, 2000);
    fakeClient.readAll();

    // With telemetry connected, OS2L beats should be suppressed
    bridge.onBeat(); // This is the OS2L beat path
    QCOMPARE(beatSpy.count(), 0);

    // But telemetry beats should go through
    fakeClient.write(R"({"evt":"beat","pos":1,"bpm":128.0,"strength":0.8,"change":false})" "\n");
    fakeClient.flush();
    QTRY_COMPARE_WITH_TIMEOUT(beatSpy.count(), 1, 2000);

    fakeClient.disconnectFromHost();
    plugin.closeInput(0, 0);
}

void VdjTelemetryClient_Test::bridgeClientDisconnectResetsDeckState()
{
    VdjBridge bridge;
    VdjBridgePlugin plugin;
    plugin.setParameter(0, 0, QLCIOPlugin::Input,
                        QStringLiteral("hostPort"), 18057);
    plugin.setParameter(0, 0, QLCIOPlugin::Input,
                        QStringLiteral("bonjourEnabled"), false);
    bridge.attachVdjPlugin(&plugin);
    QVERIFY(plugin.openInput(0, 0));

    QTcpSocket fakeClient;
    fakeClient.connectToHost("127.0.0.1", 18057);
    QVERIFY(fakeClient.waitForConnected(2000));
    QTRY_VERIFY_WITH_TIMEOUT(fakeClient.bytesAvailable() > 0, 2000);
    fakeClient.readAll();

    // Set some deck data
    fakeClient.write(R"({"evt":"subscribed","trigger":"deck 1 get_title","value":"Test"})" "\n");
    fakeClient.flush();

    DjFsm *fsm = bridge.djFsm();
    QTRY_COMPARE_WITH_TIMEOUT(fsm->deckAt(0).song.title, QString("Test"), 1000);

    // Disconnect resets the FSM deck state.
    fakeClient.disconnectFromHost();
    QTRY_COMPARE_WITH_TIMEOUT(fsm->deckAt(0).song.title, QString(), 2000);

    plugin.closeInput(0, 0);
}

QTEST_MAIN(VdjTelemetryClient_Test)
