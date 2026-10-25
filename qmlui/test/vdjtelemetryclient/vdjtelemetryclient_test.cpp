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

    QTest::newRow("get_decks")
        << QByteArray(R"({"evt":"subscribed","trigger":"get_decks","value":2})")
        << "get_decks" << QVariant(2.0);
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

    // VDJ streams a tick's changes in this order: a track change must reach
    // consumers before the new track's position does
    QVERIFY(triggerStrings.indexOf("deck 1 get_filepath") < triggerStrings.indexOf("deck 1 get_time elapsed absolute"));
    QVERIFY(triggerStrings.indexOf("deck 1 loaded") < triggerStrings.indexOf("deck 1 get_time elapsed absolute"));
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

    // one VDJ write: a load
    fakeClient.write(R"({"evt":"subscribed","trigger":"deck 1 get_filepath","value":"/music/bridge.mp3"})" "\n");
    fakeClient.write(R"({"evt":"subscribed","trigger":"deck 1 get_title","value":"Bridge Test"})" "\n");
    fakeClient.write(R"({"evt":"subscribed","trigger":"deck 1 get_bpm","value":130.0})" "\n");
    fakeClient.write(R"({"evt":"subscribed","trigger":"deck 1 loaded","value":"on"})");
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

    DjFsm *fsm = bridge.djFsm();
    fsm->setDeckCount(4);

    QTcpSocket fakeClient;
    fakeClient.connectToHost("127.0.0.1", 18055);
    QVERIFY(fakeClient.waitForConnected(2000));
    QTRY_VERIFY_WITH_TIMEOUT(fakeClient.bytesAvailable() > 0, 2000);
    fakeClient.readAll();

    // the real deck count comes from get_decks; the per-deck master flag
    // names the active deck, the bare global flag names nothing
    fakeClient.write(R"({"evt":"subscribed","trigger":"get_decks","value":2})" "\n");
    fakeClient.write(R"({"evt":"subscribed","trigger":"masterdeck","value":"off"})" "\n");
    fakeClient.write(R"({"evt":"subscribed","trigger":"deck 2 masterdeck","value":"on"})" "\n");
    fakeClient.flush();

    QTRY_COMPARE_WITH_TIMEOUT(fsm->deckCount(), 2, 2000);
    QTRY_COMPARE_WITH_TIMEOUT(fsm->activeDeck(), 2, 2000);

    fakeClient.disconnectFromHost();
    plugin.closeInput(0, 0);
}

void VdjTelemetryClient_Test::bridgeClientDisconnectResetsDeckState_data()
{
    QTest::addColumn<bool>("closeInput");
    QTest::newRow("VDJ goes away") << false;
    QTest::newRow("the input is closed under a connected VDJ") << true;
}

void VdjTelemetryClient_Test::bridgeClientDisconnectResetsDeckState()
{
    QFETCH(bool, closeInput);

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
    fakeClient.write(R"({"evt":"subscribed","trigger":"deck 1 get_filepath","value":"/music/t.mp3"})" "\n");
    fakeClient.write(R"({"evt":"subscribed","trigger":"deck 1 get_title","value":"Test"})" "\n");
    fakeClient.write(R"({"evt":"subscribed","trigger":"deck 1 loaded","value":"on"})");
    fakeClient.flush();

    DjFsm *fsm = bridge.djFsm();
    QTRY_COMPARE_WITH_TIMEOUT(fsm->deckAt(0).song.title, QString("Test"), 1000);

    // Either end of the connection resets the FSM deck state.
    if (closeInput)
        plugin.closeInput(0, 0);
    else
        fakeClient.disconnectFromHost();
    QTRY_COMPARE_WITH_TIMEOUT(fsm->deckAt(0).song.title, QString(), 2000);

    plugin.closeInput(0, 0);
}

void VdjTelemetryClient_Test::framing_data()
{
    QTest::addColumn<QStringList>("chunks");
    QTest::addColumn<QStringList>("objects");  // "deck:trigger" per deck object, in order
    QTest::addColumn<int>("ticks");

    const QString a = R"({"evt":"subscribed","trigger":"deck 1 play","value":"off"})";
    const QString b = R"({"evt":"subscribed","trigger":"deck 2 masterdeck","value":"on"})";
    const QString beat = R"({"evt":"beat","pos":1,"bpm":120,"strength":1,"change":false})";
    QTest::newRow("one write, newline-joined, no trailing newline") << QStringList{ a + "\n" + b }
                                                                      << QStringList({ "1:play", "2:masterdeck" }) << 1;
    QTest::newRow("a beat write adjacent to a tick write") << QStringList{ beat + a + "\n" + b }
                                                              << QStringList({ "1:play", "2:masterdeck" }) << 2;
    QTest::newRow("an object cut across chunks") << QStringList({ a.left(20), a.mid(20) + "\n" + b })
                                                   << QStringList({ "1:play", "2:masterdeck" }) << 1;
    QTest::newRow("a title with braces and an escaped quote")
        << QStringList{ R"({"evt":"subscribed","trigger":"deck 1 get_title","value":"a\"b}{c\\d"})" }
        << QStringList{ "1:get_title" } << 1;
    QTest::newRow("two writes in one chunk") << QStringList{ a + b } << QStringList({ "1:play", "2:masterdeck" }) << 2;
}

void VdjTelemetryClient_Test::framing()
{
    QFETCH(QStringList, chunks);
    QFETCH(QStringList, objects);
    QFETCH(int, ticks);

    VdjTelemetryClient client;
    QSignalSpy deckSpy(&client, &VdjTelemetryClient::deckTriggerReceived);
    QSignalSpy tickSpy(&client, &VdjTelemetryClient::tickEnded);
    for (const QString &chunk : chunks)
        client.feed(chunk.toUtf8());

    QStringList seen;
    for (const QList<QVariant> &args : deckSpy)
        seen << QStringLiteral("%1:%2").arg(args[0].toInt() + 1).arg(args[1].toString());
    QCOMPARE(seen, objects);
    QCOMPARE(tickSpy.count(), ticks);
    if (objects.first() == QLatin1String("1:get_title"))
        QCOMPARE(deckSpy[0][2].toString(), QStringLiteral("a\"b}{c\\d"));
}

void VdjTelemetryClient_Test::stopDropsTheClient()
{
    VdjTelemetryClient server;
    QVERIFY(server.start(18058));
    QSignalSpy discSpy(&server, &VdjTelemetryClient::clientDisconnected);

    // nobody connected: stop is silent
    server.stop();
    QCOMPARE(discSpy.count(), 0);

    QVERIFY(server.start(18058));
    QTcpSocket client;
    client.connectToHost("127.0.0.1", 18058);
    QVERIFY(client.waitForConnected(2000));
    QTRY_COMPARE_WITH_TIMEOUT(server.status(), VdjTelemetryClient::ClientConnected, 2000);

    server.stop();
    QCOMPARE(discSpy.count(), 1);
    QCOMPARE(server.status(), VdjTelemetryClient::Idle);
}

QTEST_MAIN(VdjTelemetryClient_Test)
