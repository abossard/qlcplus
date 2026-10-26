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
#include "vdjbonjour.h"
#include "configurevdjbridge.h"
#include <QLabel>
#include <QSpinBox>
#include <QTcpServer>
#include <memory>

#ifdef Q_OS_UNIX
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {
struct VdjPeer
{
    const int socket = ::socket(AF_INET, SOCK_STREAM, 0);
    VdjPeer() = default;
    VdjPeer(const VdjPeer &) = delete;
    VdjPeer &operator=(const VdjPeer &) = delete;
    ~VdjPeer()
    {
        if (socket >= 0)
            ::close(socket);
    }
};
}
#endif

// vdjbonjour_stub.cpp
extern int vdjBonjourRegistrations;
int vdjBonjourLive();

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

    // a value reaches consumers as JSON typed it: a string stays a string
    QTest::newRow("deck1_volume_string")
        << QByteArray(R"({"evt":"subscribed","trigger":"deck 1 volume","value":"0.85"})")
        << 0 << "volume" << QVariant("0.85");
    QTest::newRow("numeric-looking title")
        << QByteArray(R"({"evt":"subscribed","trigger":"deck 1 get_title","value":"007"})")
        << 0 << "get_title" << QVariant("007");
    QTest::newRow("title 1.0")
        << QByteArray(R"({"evt":"subscribed","trigger":"deck 1 get_title","value":"1.0"})")
        << 0 << "get_title" << QVariant("1.0");
    QTest::newRow("title NaN")
        << QByteArray(R"({"evt":"subscribed","trigger":"deck 1 get_title","value":"NaN"})")
        << 0 << "get_title" << QVariant("NaN");
    QTest::newRow("title with spaces")
        << QByteArray(R"({"evt":"subscribed","trigger":"deck 1 get_title","value":" 7 "})")
        << 0 << "get_title" << QVariant(" 7 ");
    QTest::newRow("title true")
        << QByteArray(R"({"evt":"subscribed","trigger":"deck 1 get_title","value":"true"})")
        << 0 << "get_title" << QVariant("true");
    QTest::newRow("play on")
        << QByteArray(R"({"evt":"subscribed","trigger":"deck 1 play","value":"on"})")
        << 0 << "play" << QVariant("on");
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

    const QVariant actual = spy[0][2].value<QVariant>();
    QCOMPARE(actual.typeId(), expectedValue.typeId());
    QCOMPARE(actual, expectedValue);
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

    client.parseLine(R"({"evt":"subscribed","trigger":"deck 1 play","value":"on"})");
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

void VdjTelemetryClient_Test::subscriptionTriggerCount_data()
{
    QTest::addColumn<QString>("trigger");
    QTest::addColumn<bool>("subscribed");

    // the trigger names the row: a deck's masterdeck is not the bare global one
    auto row = [](const QString &trigger, bool subscribed) {
        QTest::newRow(qPrintable(trigger)) << trigger << subscribed;
    };
    // what DjFsm, VdjBridge and the DJ Manager read
    for (const char *t : { "get_filepath", "loaded", "get_time", "get_time elapsed absolute", "get_beatpos",
                           "get_title", "get_artist", "get_title_artist", "get_album", "get_genre", "get_bpm",
                           "get_key", "get_firstbeat", "get_time total", "play", "volume", "masterdeck" })
        row(QStringLiteral("deck 4 %1").arg(QLatin1String(t)), true);
    row("get_decks", true);
    // read by nothing
    for (const char *t : { "get_position", "get_vu_meter", "level", "eq_high", "eq_med", "eq_low", "gain",
                           "loop", "get_loop" })
        row(QStringLiteral("deck 1 %1").arg(QLatin1String(t)), false);
    for (const char *t : { "master_volume", "crossfader", "headphone_volume", "masterdeck",
                           "get_crossfader_result full", "mixer_order", "get_vu_meter" })
        row(QString::fromLatin1(t), false);
}

void VdjTelemetryClient_Test::subscriptionTriggerCount()
{
    QFETCH(QString, trigger);
    QFETCH(bool, subscribed);

    QByteArray msg = VdjTelemetryClient::buildSubscriptionMessage();
    QJsonDocument doc = QJsonDocument::fromJson(msg.trimmed());
    QStringList triggerStrings;
    for (const QJsonValue &v : doc.object().value("trigger").toArray())
        triggerStrings.append(v.toString());

    // 4 decks x 17 per-deck triggers + get_decks
    QCOMPARE(triggerStrings.size(), 69);
    QCOMPARE(triggerStrings.contains(trigger), subscribed);

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

void VdjTelemetryClient_Test::closingAnInputLetsThePeerFinishItsTick_data()
{
    QTest::addColumn<QString>("closingPath");
    for (const char *path : { "closeInput", "port change", "replacement", "plugin destruction" })
        QTest::newRow(path) << QString::fromLatin1(path);
    QTest::newRow("unfinished object cap") << QStringLiteral("cap");
}

void VdjTelemetryClient_Test::closingAnInputLetsThePeerFinishItsTick()
{
#ifndef Q_OS_UNIX
    QSKIP("Observing TCP EOF and errno requires POSIX sockets");
#else
    QFETCH(QString, closingPath);

    VdjPeer vdj;
    QVERIFY2(vdj.socket >= 0, std::strerror(errno));
    const int one = 1;
#ifdef Q_OS_DARWIN
    QVERIFY2(::setsockopt(vdj.socket, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one)) == 0,
             std::strerror(errno));
    const int sendFlags = 0;
#elif defined(MSG_NOSIGNAL)
    const int sendFlags = MSG_NOSIGNAL;
#else
    const int sendFlags = 0;
    QSKIP("This platform has neither SO_NOSIGPIPE nor MSG_NOSIGNAL");
#endif
    QVERIFY2(::setsockopt(vdj.socket, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one)) == 0,
             std::strerror(errno));
    const int flags = ::fcntl(vdj.socket, F_GETFL);
    QVERIFY(flags >= 0);
    QVERIFY(::fcntl(vdj.socket, F_SETFL, flags | O_NONBLOCK) == 0);

    auto plugin = std::make_unique<VdjBridgePlugin>();
    plugin->setParameter(0, 0, QLCIOPlugin::Input, QStringLiteral("hostPort"), 18074);
    plugin->setParameter(0, 0, QLCIOPlugin::Input, QStringLiteral("bonjourEnabled"), false);
    QVERIFY(plugin->openInput(0, 0));
    QSignalSpy connSpy(plugin.get(), &VdjBridgePlugin::clientConnected);
    QSignalSpy discSpy(plugin.get(), &VdjBridgePlugin::clientDisconnected);
    QSignalSpy deckSpy(plugin.get(), &VdjBridgePlugin::deckTriggerReceived);
    QSignalSpy beatSpy(plugin.get(), &VdjBridgePlugin::telemetryBeatReceived);

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(18074);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    const int connected = ::connect(vdj.socket, reinterpret_cast<sockaddr *>(&address), sizeof(address));
    QVERIFY2(connected == 0 || errno == EINPROGRESS, std::strerror(errno));
    pollfd writable{vdj.socket, POLLOUT, 0};
    QCOMPARE(::poll(&writable, 1, 2000), 1);
    int connectError = 0;
    socklen_t errorSize = sizeof(connectError);
    QCOMPARE(::getsockopt(vdj.socket, SOL_SOCKET, SO_ERROR, &connectError, &errorSize), 0);
    QCOMPARE(connectError, 0);
    QTRY_COMPARE_WITH_TIMEOUT(connSpy.count(), 1, 2000);

    QByteArray handshake;
    auto readHandshake = [&]() {
        char buffer[4096];
        const ssize_t count = ::recv(vdj.socket, buffer, sizeof(buffer), 0);
        if (count > 0)
            handshake.append(buffer, count);
        return handshake.endsWith('\n');
    };
    QTRY_VERIFY_WITH_TIMEOUT(readHandshake(), 2000);
    QCOMPARE(QJsonDocument::fromJson(handshake).object().value("evt").toString(), QStringLiteral("subscribe"));

    auto sendAll = [&](const QByteArray &data) {
        QElapsedTimer deadline;
        deadline.start();
        qsizetype sent = 0;
        while (sent < data.size() && deadline.elapsed() < 2000)
        {
            const ssize_t count = ::send(vdj.socket, data.constData() + sent, data.size() - sent, sendFlags);
            if (count > 0)
                sent += count;
            else if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
                QTest::qWait(1);
            else
                return false;
        }
        return sent == data.size();
    };
    QVERIFY(sendAll(R"({"evt":"subscribed","trigger":"deck 1 play","value":"on"})" "\n"));
    QTRY_COMPARE_WITH_TIMEOUT(deckSpy.count(), 1, 2000);
    const QByteArray beat = R"({"evt":"beat","pos":12,"bpm":126,"strength":1,"change":false})" "\n";
    for (int tick = 1; tick <= 3; ++tick)
    {
        QVERIFY(sendAll(beat));
        QTRY_COMPARE_WITH_TIMEOUT(beatSpy.count(), tick, 2000);
        QTest::qWait(40);
    }

    const QByteArray tick = R"({"evt":"subscribed","trigger":"deck 1 get_time","value":1120})" "\n";
    if (closingPath != QLatin1String("cap"))
    {
        QVERIFY(sendAll(tick));
        // Unread inbound data can reset even a normal Qt close. Observe this
        // tick before testing orderly close, rather than attributing that to abort.
        QTRY_COMPARE_WITH_TIMEOUT(deckSpy.count(), 2, 2000);
    }

    QTcpSocket replacement;
    if (closingPath == QLatin1String("closeInput"))
        plugin->closeInput(0, 0);
    else if (closingPath == QLatin1String("port change"))
        plugin->setParameter(0, 0, QLCIOPlugin::Input, QStringLiteral("hostPort"), 18075);
    else if (closingPath == QLatin1String("replacement"))
    {
        replacement.connectToHost(QHostAddress::LocalHost, 18074);
        QVERIFY(replacement.waitForConnected(2000));
        QTRY_COMPARE_WITH_TIMEOUT(connSpy.count(), 2, 2000);
    }
    else if (closingPath == QLatin1String("plugin destruction"))
        plugin.reset();
    else
        QVERIFY(sendAll(QByteArray(R"({"evt":"subscribed","trigger":"deck 1 get_title","value":")")
                        + QByteArray(1024 * 1024, 'x') + tick));
    QTRY_COMPARE_WITH_TIMEOUT(discSpy.count(), 1, 5000);

    ssize_t received = -1;
    int receiveError = 0;
    auto readClose = [&]() {
        char buffer[4096];
        received = ::recv(vdj.socket, buffer, sizeof(buffer), 0);
        receiveError = received < 0 ? errno : 0;
        return received == 0 || (received < 0 && receiveError != EAGAIN
                                && receiveError != EWOULDBLOCK && receiveError != EINTR);
    };
    QTRY_VERIFY_WITH_TIMEOUT(readClose(), 2000);
    // Exactly one write after observing the close. A later write may fail even
    // after a normal FIN, so retrying here would manufacture a reset regression.
    const ssize_t written = ::send(vdj.socket, tick.constData(), tick.size(), sendFlags);
    const int writeError = written < 0 ? errno : 0;
    const QString result = QStringLiteral("recv=%1 errno=%2 (%3); first subsequent send=%4 errno=%5 (%6)")
        .arg(received).arg(receiveError).arg(QString::fromLocal8Bit(std::strerror(receiveError)))
        .arg(written).arg(writeError).arg(QString::fromLocal8Bit(std::strerror(writeError)));
    qInfo().noquote() << result;
    QVERIFY2(received == 0, qPrintable(result));
    QVERIFY2(writeError != ECONNRESET && writeError != EPIPE, qPrintable(result));
    QCOMPARE(written, ssize_t(tick.size()));
#endif
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

void VdjTelemetryClient_Test::beatsOfAConnectionStartAtItsSnapshot_data()
{
    // one list of writes per connection: "beat", "snapshot" or "other" (an evt VDJ never sends)
    QTest::addColumn<QList<QStringList>>("connections");
    QTest::addColumn<int>("engineBeats");

    // measured: a reconnecting VDJ drains every beat it queued, then the snapshot
    QTest::newRow("backlog before the snapshot")
        << QList<QStringList>{ { "beat", "beat", "snapshot", "beat" } } << 1;
    QTest::newRow("no snapshot yet") << QList<QStringList>{ { "beat", "beat" } } << 0;
    QTest::newRow("a reconnect drains again")
        << QList<QStringList>{ { "snapshot", "beat" }, { "beat", "snapshot", "beat" } } << 2;
    QTest::newRow("only the subscription answer ends the backlog")
        << QList<QStringList>{ { "beat", "other", "beat", "snapshot", "beat" } } << 1;
}

void VdjTelemetryClient_Test::beatsOfAConnectionStartAtItsSnapshot()
{
    QFETCH(QList<QStringList>, connections);
    QFETCH(int, engineBeats);

    VdjBridgePlugin plugin;
    plugin.setParameter(0, 0, QLCIOPlugin::Input, QStringLiteral("hostPort"), 18059);
    plugin.setParameter(0, 0, QLCIOPlugin::Input, QStringLiteral("bonjourEnabled"), false);
    QVERIFY(plugin.openInput(0, 0));
    QSignalSpy beatSpy(&plugin, &QLCIOPlugin::valueChanged);
    QSignalSpy deckSpy(&plugin, &VdjBridgePlugin::deckTriggerReceived);
    QSignalSpy discSpy(&plugin, &VdjBridgePlugin::clientDisconnected);

    const QByteArray beat = R"({"evt":"beat","change":false,"pos":12,"bpm":126,"strength":1})";
    const QByteArray snapshot = R"({"evt":"subscribed","trigger":"deck 1 get_title","value":"A"})";
    const QByteArray end = R"({"evt":"subscribed","trigger":"deck 1 get_title","value":"end"})";
    const QByteArray other = R"({"evt":"hello"})";
    for (const QStringList &writes : connections)
    {
        QTcpSocket vdj;
        vdj.connectToHost("127.0.0.1", 18059);
        QVERIFY(vdj.waitForConnected(2000));
        QTRY_VERIFY_WITH_TIMEOUT(vdj.bytesAvailable() > 0, 2000);
        vdj.readAll();
        for (const QString &write : writes)
            vdj.write(write == QLatin1String("beat") ? beat : write == QLatin1String("other") ? other : snapshot);
        // in order on the socket: everything before it was parsed
        const int triggers = deckSpy.count() + writes.count(QStringLiteral("snapshot")) + 1;
        vdj.write(end);
        vdj.flush();
        QTRY_COMPARE_WITH_TIMEOUT(deckSpy.count(), triggers, 2000);
        const int ends = discSpy.count() + 1;
        vdj.disconnectFromHost();
        QTRY_COMPARE_WITH_TIMEOUT(discSpy.count(), ends, 2000);
    }
    QCOMPARE(beatSpy.count(), engineBeats);
    plugin.closeInput(0, 0);
}

void VdjTelemetryClient_Test::anObjectOpenPastTheCapDropsTheClient_data()
{
    QTest::addColumn<QByteArray>("data");
    QTest::addColumn<int>("triggers");
    QTest::addColumn<bool>("dropped");

    const QByteArray object = R"({"evt":"subscribed","trigger":"deck 1 get_time","value":1000})";
    // a snapshot is ~7 KB; the cap is 1 MB of one unfinished object
    QTest::newRow("an object open past 1 MB")
        << QByteArray(R"({"evt":"subscribed","trigger":"deck 1 get_title","value":")") + QByteArray(1024 * 1024, 'x')
        << 0 << true;
    QTest::newRow("complete objects beyond 1 MB in one write") << object.repeated(20000) << 20000 << false;
}

void VdjTelemetryClient_Test::anObjectOpenPastTheCapDropsTheClient()
{
    QFETCH(QByteArray, data);
    QFETCH(int, triggers);
    QFETCH(bool, dropped);

    VdjTelemetryClient server;
    QVERIFY(server.start(18060));
    QSignalSpy deckSpy(&server, &VdjTelemetryClient::deckTriggerReceived);
    QSignalSpy discSpy(&server, &VdjTelemetryClient::clientDisconnected);

    QTcpSocket vdj;
    vdj.connectToHost("127.0.0.1", 18060);
    QVERIFY(vdj.waitForConnected(2000));
    QTRY_VERIFY_WITH_TIMEOUT(vdj.bytesAvailable() > 0, 2000);
    vdj.readAll();
    vdj.write(data);
    vdj.write(R"({"evt":"subscribed","trigger":"deck 2 play","value":"on"})");
    vdj.flush();

    if (dropped)
    {
        QTRY_COMPARE_WITH_TIMEOUT(discSpy.count(), 1, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(vdj.state(), QAbstractSocket::UnconnectedState, 2000);
        QCOMPARE(server.status(), VdjTelemetryClient::Listening);
    }
    else
    {
        QTRY_COMPARE_WITH_TIMEOUT(deckSpy.count(), triggers + 1, 5000);
        QCOMPARE(discSpy.count(), 0);
        QCOMPARE(server.status(), VdjTelemetryClient::ClientConnected);
    }
    server.stop();
}

void VdjTelemetryClient_Test::aConnectionThatEndsAdvertisesAgain_data()
{
    QTest::addColumn<QString>("end");
    QTest::addColumn<int>("registrations");
    QTest::newRow("VDJ closes the connection") << "close" << 2;
    QTest::newRow("QLC+ drops the connection") << "drop" << 2;
    QTest::newRow("a new client replaces the old one") << "replace" << 1;
}

void VdjTelemetryClient_Test::aConnectionThatEndsAdvertisesAgain()
{
    QFETCH(QString, end);
    QFETCH(int, registrations);

    VdjBridgePlugin plugin;
    plugin.setParameter(0, 0, QLCIOPlugin::Input, QStringLiteral("hostPort"), 18061);
    const int before = vdjBonjourRegistrations;
    QVERIFY(plugin.openInput(0, 0));
    QCOMPARE(vdjBonjourRegistrations, before + 1);
    QSignalSpy discSpy(&plugin, &VdjBridgePlugin::clientDisconnected);
    QSignalSpy connSpy(&plugin, &VdjBridgePlugin::clientConnected);

    QTcpSocket vdj, next;
    vdj.connectToHost("127.0.0.1", 18061);
    QVERIFY(vdj.waitForConnected(2000));
    QTRY_VERIFY_WITH_TIMEOUT(vdj.bytesAvailable() > 0, 2000);
    vdj.readAll();
    if (end == QLatin1String("close"))
        vdj.disconnectFromHost();
    else if (end == QLatin1String("drop"))
    {
        vdj.write("{\"value\":\"" + QByteArray(1024 * 1024, 'x'));
        vdj.flush();
    }
    else
    {
        next.connectToHost("127.0.0.1", 18061);
        QVERIFY(next.waitForConnected(2000));
        QTRY_COMPARE_WITH_TIMEOUT(connSpy.count(), 2, 2000);
    }
    QTRY_COMPARE_WITH_TIMEOUT(discSpy.count(), 1, 5000);

    // measured: VDJ reconnects only to a service registered again; a
    // replacing client is already connected
    QCOMPARE(vdjBonjourRegistrations, before + registrations);
    QCOMPARE(vdjBonjourLive(), 1);
    plugin.closeInput(0, 0);
    QCOMPARE(vdjBonjourRegistrations, before + registrations);
    QCOMPARE(vdjBonjourLive(), 0);
}

void VdjTelemetryClient_Test::aBonjourFailureShowsInTheStatus()
{
    VdjBridgePlugin plugin;
    plugin.setParameter(0, 0, QLCIOPlugin::Input, QStringLiteral("hostPort"), 18062);
    QVERIFY(plugin.openInput(0, 0));
    ConfigureVdjBridge dialog(&plugin);
    QLabel *status = dialog.findChild<QLabel *>(QStringLiteral("statusLabel"));
    QVERIFY(status != nullptr);
    QSignalSpy statusSpy(&plugin, &QLCIOPlugin::connectionStatusChanged);

    // mDNSResponder's answer, as VdjBonjour reports it
    VdjBonjour *bonjour = plugin.findChild<VdjBonjour *>();
    QVERIFY(bonjour != nullptr);
    emit bonjour->serviceRegistrationFailed(QStringLiteral("DNSServiceProcessResult error -65563"));

    QCOMPARE(statusSpy.count(), 1);
    QVERIFY2(plugin.inputInfo(0).contains("error -65563"), qPrintable(plugin.inputInfo(0)));
    QVERIFY2(status->text().contains("error -65563"), qPrintable(status->text()));

    // the next registration starts clean, and says so
    plugin.setParameter(0, 0, QLCIOPlugin::Input, QStringLiteral("serviceName"), QStringLiteral("Booth"));
    QCOMPARE(statusSpy.count(), 2);
    QVERIFY(!plugin.inputInfo(0).contains("error -65563"));
    QVERIFY2(!status->text().contains("error -65563"), qPrintable(status->text()));

    emit bonjour->serviceRegistrationFailed(QStringLiteral("DNSServiceProcessResult error -65563"));
    plugin.setParameter(0, 0, QLCIOPlugin::Input, QStringLiteral("bonjourEnabled"), false);
    QVERIFY2(!status->text().contains("error -65563"), qPrintable(status->text()));
    plugin.closeInput(0, 0);
}

void VdjTelemetryClient_Test::parametersAreKeptWithTheInputsUniverse_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<QVariant>("value");
    QTest::newRow("port") << "hostPort" << QVariant(18064);
    QTest::newRow("Bonjour off") << "bonjourEnabled" << QVariant(false);
    QTest::newRow("service name") << "serviceName" << QVariant("Booth");
}

void VdjTelemetryClient_Test::parametersAreKeptWithTheInputsUniverse()
{
    QFETCH(QString, name);
    QFETCH(QVariant, value);

    VdjBridgePlugin plugin;
    plugin.setParameter(0, 0, QLCIOPlugin::Input, QStringLiteral("hostPort"), 18063);
    QVERIFY(plugin.openInput(0, 3));

    // the workspace saves a universe's patch with these parameters and sets them again on load
    plugin.setParameter(3, 0, QLCIOPlugin::Input, name, value);
    QCOMPARE(plugin.getParameters(3, 0, QLCIOPlugin::Input).value(name), value);
    // closing unmaps the universe: its patch is gone
    plugin.closeInput(0, 3);
    QVERIFY(plugin.getParameters(3, 0, QLCIOPlugin::Input).isEmpty());
}

void VdjTelemetryClient_Test::aPortEditRebindsWhenFinished_data()
{
    QTest::addColumn<bool>("inUse");
    QTest::addColumn<int>("status");
    QTest::addColumn<QString>("text"); // in the dialog's status and the input info
    QTest::newRow("a free port") << false << int(QLCIOPlugin::Advertising) << "18066";
    QTest::newRow("a port in use listens to nothing") << true << int(QLCIOPlugin::Idle) << "Port 18066 in use";
}

void VdjTelemetryClient_Test::aPortEditRebindsWhenFinished()
{
    QFETCH(bool, inUse);
    QFETCH(int, status);
    QFETCH(QString, text);

    QTcpServer other;
    if (inUse)
        QVERIFY(other.listen(QHostAddress::Any, 18066));
    VdjBridgePlugin plugin;
    plugin.setParameter(0, 0, QLCIOPlugin::Input, QStringLiteral("hostPort"), 18065);
    QVERIFY(plugin.openInput(0, 3));
    ConfigureVdjBridge dialog(&plugin);
    QSpinBox *port = dialog.findChild<QSpinBox *>();
    QVERIFY(port != nullptr);
    QSignalSpy statusSpy(&plugin, &QLCIOPlugin::connectionStatusChanged);

    // typing 18066 passes 1, 18, 180, ...: none of them is a port to bind
    port->setValue(18066);
    QCOMPARE(plugin.hostPort(), quint16(18065));
    QCOMPARE(statusSpy.count(), 0);

    emit port->editingFinished();
    QCOMPARE(plugin.hostPort(), quint16(18066));
    QCOMPARE(plugin.getParameters(3, 0, QLCIOPlugin::Input).value("hostPort").toInt(), 18066);
    QCOMPARE(statusSpy.count(), 1);
    QCOMPARE(plugin.connectionStatus(0), status);
    QLabel *label = dialog.findChild<QLabel *>(QStringLiteral("statusLabel"));
    QVERIFY2(label->text().contains(text), qPrintable(label->text()));
    QVERIFY2(plugin.inputInfo(0).contains(text), qPrintable(plugin.inputInfo(0)));
    plugin.closeInput(0, 3);
}

void VdjTelemetryClient_Test::aSavedPortRestartsAnInputOpenedOnABusyPort()
{
    // Universe::loadXML opens the input on the plugin's port first and sets
    // the saved parameters after
    QTcpServer other;
    QVERIFY(other.listen(QHostAddress::Any, 18067));
    VdjBridgePlugin plugin;
    plugin.setParameter(0, 0, QLCIOPlugin::Input, QStringLiteral("hostPort"), 18067);
    // open: InputOutputMap takes its beats, InputPatch::reconnect restores its parameters
    QVERIFY(plugin.openInput(0, 3));
    QCOMPARE(plugin.connectionStatus(0), int(QLCIOPlugin::Idle));
    QCOMPARE(vdjBonjourLive(), 0);

    plugin.setParameter(3, 0, QLCIOPlugin::Input, QStringLiteral("hostPort"), QStringLiteral("18068"));
    QCOMPARE(plugin.connectionStatus(0), int(QLCIOPlugin::Advertising));
    QCOMPARE(vdjBonjourLive(), 1);
    QCOMPARE(plugin.getParameters(3, 0, QLCIOPlugin::Input).value("hostPort").toInt(), 18068);
    QTcpSocket vdj;
    vdj.connectToHost("127.0.0.1", 18068);
    QVERIFY(vdj.waitForConnected(2000));
    QTRY_COMPARE_WITH_TIMEOUT(plugin.connectionStatus(0), int(QLCIOPlugin::Connected), 2000);
    plugin.closeInput(0, 3);
}

void VdjTelemetryClient_Test::openingAnOpenInputBindsABusyPortAgain()
{
    // vdj/autoOpenInput opened the input while the port was busy; the
    // workspace's patch opens it again once the port is free
    QTcpServer other;
    QVERIFY(other.listen(QHostAddress::Any, 18073));
    VdjBridgePlugin plugin;
    plugin.setParameter(0, 0, QLCIOPlugin::Input, QStringLiteral("hostPort"), 18073);
    QVERIFY(plugin.openInput(0, 0));
    QCOMPARE(plugin.connectionStatus(0), int(QLCIOPlugin::Idle));
    other.close();
    QSignalSpy statusSpy(&plugin, &QLCIOPlugin::connectionStatusChanged);

    QVERIFY(plugin.openInput(0, 0));
    QCOMPARE(plugin.connectionStatus(0), int(QLCIOPlugin::Advertising));
    QCOMPARE(vdjBonjourLive(), 1);
    QCOMPARE(statusSpy.count(), 1);
    plugin.closeInput(0, 0);
}

void VdjTelemetryClient_Test::bonjourAdvertisesOnlyAListeningServer_data()
{
    QTest::addColumn<QString>("edit");
    QTest::newRow("service name") << "name";
    QTest::newRow("Bonjour off and on") << "toggle";
}

void VdjTelemetryClient_Test::bonjourAdvertisesOnlyAListeningServer()
{
    QFETCH(QString, edit);

    QTcpServer other;
    QVERIFY(other.listen(QHostAddress::Any, 18070));
    VdjBridgePlugin plugin;
    plugin.setParameter(0, 0, QLCIOPlugin::Input, QStringLiteral("hostPort"), 18069);
    QVERIFY(plugin.openInput(0, 3));
    QCOMPARE(vdjBonjourLive(), 1);

    plugin.setParameter(3, 0, QLCIOPlugin::Input, QStringLiteral("hostPort"), 18070);
    QCOMPARE(vdjBonjourLive(), 0);
    if (edit == QLatin1String("name"))
        plugin.setParameter(3, 0, QLCIOPlugin::Input, QStringLiteral("serviceName"), QStringLiteral("Booth"));
    else
    {
        plugin.setParameter(3, 0, QLCIOPlugin::Input, QStringLiteral("bonjourEnabled"), false);
        plugin.setParameter(3, 0, QLCIOPlugin::Input, QStringLiteral("bonjourEnabled"), true);
    }
    // nobody listens on a port in use
    QCOMPARE(vdjBonjourLive(), 0);
    QVERIFY2(!plugin.inputInfo(0).contains("_os2l._tcp ("), qPrintable(plugin.inputInfo(0)));

    plugin.setParameter(3, 0, QLCIOPlugin::Input, QStringLiteral("hostPort"), 18071);
    QCOMPARE(vdjBonjourLive(), 1);
    QVERIFY(plugin.inputInfo(0).contains("_os2l._tcp ("));
    plugin.closeInput(0, 3);
}

void VdjTelemetryClient_Test::aPortOutsideOneTo65535IsIgnored_data()
{
    // from vdj/telemetryPort or a workspace file
    QTest::addColumn<QVariant>("port");
    QTest::addColumn<bool>("applied");
    QTest::newRow("0: any port") << QVariant(0) << false;
    QTest::newRow("\"0\"") << QVariant("0") << false;
    QTest::newRow("past 65535") << QVariant(70000) << false;
    QTest::newRow("not a number") << QVariant("port") << false;
    QTest::newRow("1") << QVariant(1) << true;
    QTest::newRow("65535") << QVariant(65535) << true;
}

void VdjTelemetryClient_Test::aPortOutsideOneTo65535IsIgnored()
{
    QFETCH(QVariant, port);
    QFETCH(bool, applied);

    VdjBridgePlugin plugin;
    plugin.setParameter(0, 0, QLCIOPlugin::Input, QStringLiteral("hostPort"), 18072);
    QVERIFY(plugin.openInput(0, 3));

    plugin.setParameter(3, 0, QLCIOPlugin::Input, QStringLiteral("hostPort"), port);
    QCOMPARE(plugin.hostPort(), quint16(applied ? port.toUInt() : 18072));
    QCOMPARE(plugin.getParameters(3, 0, QLCIOPlugin::Input).contains("hostPort"), applied);
    // whether port 1 or 65535 binds is the machine's business
    if (!applied)
        QCOMPARE(plugin.connectionStatus(0), int(QLCIOPlugin::Advertising));
    plugin.closeInput(0, 3);
}

QTEST_MAIN(VdjTelemetryClient_Test)
