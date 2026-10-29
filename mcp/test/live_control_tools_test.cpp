#include "live_control_tools_test.h"
#include "app.h"
#include "doc.h"
#include "mastertimer.h"
#include "show.h"
#include "showcommandrecorder.h"
#include "functionparent.h"
#include "scene.h"
#include "track.h"
#include "showfunction.h"
#include "fixture.h"
#include "scenevalue.h"
#include "universe.h"
#include "inputoutputmap.h"
#include "vcbutton.h"
#include "vcpage.h"
#include "virtualconsole.h"
#include "djmanager.h"
#include "djsetrecorder.h"
#include "vdjbridge.h"
#include "showfactory.h"
#include "showrunner.h"
#include "vcbridgev5.h"
#include "workspacebridgev5.h"
#include "tool_registry.h"
#include "mcpserver.h"

#include <QtTest>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTcpServer>
#include <fastmcpp/tools/manager.hpp>

using Json = nlohmann::json;

namespace {
Json invoke(Doc *doc, VCBridge *vc, WorkspaceBridge *workspace,
            const char *name, const Json &args = Json::object())
{
    fastmcpp::tools::ToolManager tm;
    registerLiveControlTools(tm, doc, vc, workspace);
    if (!tm.has(name))
        return {{"error", std::string("tool not registered: ") + name}};
    const Json result = tm.invoke(name, args);
    return result.is_string() ? Json::parse(result.get<std::string>()) : result;
}

Json invokeShowTool(Doc *doc, VCBridge *vc, const char *name, const Json &args)
{
    fastmcpp::tools::ToolManager tm;
    registerShowTools(tm, doc, vc);
    const Json result = tm.invoke(name, args);
    return result.is_string() ? Json::parse(result.get<std::string>()) : result;
}

class QuitObserver : public QObject
{
public:
    int requests = 0;
    bool eventFilter(QObject *, QEvent *event) override
    {
        if (event->type() != QEvent::Quit)
            return false;
        ++requests;
        QCoreApplication::exit(0);
        return true;
    }
};
}

McpLiveControlTools_Test::McpLiveControlTools_Test() = default;
McpLiveControlTools_Test::~McpLiveControlTools_Test() = default;

void McpLiveControlTools_Test::initTestCase()
{
    QVERIFY(m_settings.isValid());
    m_previousHome = qgetenv("HOME");
    qputenv("HOME", m_settings.path().toUtf8());
    QStandardPaths::setTestModeEnabled(true);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settings.path());
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, m_settings.path());
    QSettings().setValue("vdj/autoOpenInput", false);
    m_app = std::make_unique<App>();
    m_app->set3dSupported(false);
    m_app->startup();
    m_app->doc()->masterTimer()->stop();
    m_vc = std::make_unique<VCBridgeV5>(m_app->doc(), m_app->virtualConsole(), m_app.get());
    m_workspace = std::make_unique<WorkspaceBridgeV5>(m_app.get());
    if (m_vc->pagesCount() == 0)
        QVERIFY(m_vc->addPage("Live controls") >= 0);
}

void McpLiveControlTools_Test::cleanupTestCase()
{
    m_workspace.reset();
    m_vc.reset();
    m_app.reset();
    qputenv("HOME", m_previousHome);
}

void McpLiveControlTools_Test::performMode_data()
{
    QTest::addColumn<bool>("on");
    QTest::addColumn<QString>("state");
    QTest::newRow("armed") << true << QStringLiteral("Armed");
    QTest::newRow("idle") << false << QStringLiteral("Idle");
}

void McpLiveControlTools_Test::performMode()
{
    QFETCH(bool, on);
    QFETCH(QString, state);
    Doc *doc = m_app->doc();
    const Json result = invoke(doc, m_vc.get(), m_workspace.get(), "set_perform_mode", {{"on", on}});
    QVERIFY2(!result.contains("error"), result.dump().c_str());
    const Json read = invoke(doc, m_vc.get(), m_workspace.get(), "get_dj_state");
    QCOMPARE(read.at("performMode").get<bool>(), on);
    QCOMPARE(QString::fromStdString(read.at("performState").get<std::string>()), state);
    QCOMPARE(read.at("activeShowId"), Json(nullptr));
    QCOMPARE(read.at("activeDeck"), Json(0));
    QCOMPARE(read.at("decks").size(), size_t(2));
    QCOMPARE(read.at("decks").at(0).at("state"), Json("Empty"));
}

void McpLiveControlTools_Test::recording_data()
{
    QTest::addColumn<bool>("bound");
    QTest::newRow("waiting-for-show") << false;
    QTest::newRow("bound-show") << true;
}

void McpLiveControlTools_Test::recording()
{
    QFETCH(bool, bound);
    Doc *doc = m_app->doc();
    auto *recorder = m_app->findChild<ShowCommandRecorder *>();
    QVERIFY(recorder);
    auto *show = new Show(doc);
    QVERIFY(doc->addFunction(show));
    QVERIFY(recorder->setResolvedShow(bound ? show->id() : Function::invalidId()));
    const auto discard = qScopeGuard([&]() { recorder->discardRecording(); });
    const Json armed = invoke(doc, m_vc.get(), m_workspace.get(), "set_recording", {{"on", true}});
    QVERIFY2(!armed.contains("error"), armed.dump().c_str());
    const Json read = invoke(doc, m_vc.get(), m_workspace.get(), "get_recording_state");
    QCOMPARE(read.at("recording"), Json(true));
    QCOMPARE(read.at("phase"), Json(bound ? "Recording" : "Armed"));
    QCOMPARE(read.at("boundShowId"), bound ? Json(show->id()) : Json(nullptr));
    QCOMPARE(read.at("lastError"), Json(""));
    const Json off = invoke(doc, m_vc.get(), m_workspace.get(), "set_recording", {{"on", false}});
    QVERIFY2(!off.contains("error"), off.dump().c_str());
    const Json stopped = invoke(doc, m_vc.get(), m_workspace.get(), "get_recording_state");
    QCOMPARE(stopped.at("recording"), Json(false));
    QCOMPARE(stopped.at("phase"), Json("Off"));
    QCOMPARE(stopped.at("boundShowId"), Json(nullptr));
}

void McpLiveControlTools_Test::buttonInput_data()
{
    QTest::addColumn<QString>("kind");
    QTest::addColumn<bool>("record");
    QTest::newRow("flash-live") << QStringLiteral("flash") << false;
    QTest::newRow("flash-recorded") << QStringLiteral("flash") << true;
    QTest::newRow("toggle-recorded") << QStringLiteral("toggle") << true;
}

void McpLiveControlTools_Test::buttonInput()
{
    QFETCH(QString, kind);
    QFETCH(bool, record);
    Doc *doc = m_app->doc();
    VirtualConsole *vc = m_app->virtualConsole();
    auto *recorder = m_app->findChild<ShowCommandRecorder *>();
    Fixture *fixture = doc->fixtures().value(0, nullptr);
    if (!fixture)
    {
        fixture = new Fixture(doc);
        fixture->setChannels(1);
        fixture->setUniverse(0);
        fixture->setAddress(0);
        QVERIFY(doc->addFixture(fixture));
    }
    auto *scene = new Scene(doc);
    scene->setValue(SceneValue(fixture->id(), 0, 255));
    QVERIFY(doc->addFunction(scene));
    auto *show = new Show(doc);
    QVERIFY(doc->addFunction(show));
    QVERIFY(recorder->setResolvedShow(show->id()));
    QVERIFY(recorder->setRecording(record));
    const auto discard = qScopeGuard([&]() {
        recorder->discardRecording();
        doc->masterTimer()->stop();
        vc->setEditMode(true);
    });
    auto *button = new VCButton(doc, vc->page(0));
    vc->addWidgetToMap(button);
    vc->page(0)->addWidgetToPageMap(button);
    button->setFunctionID(scene->id());
    button->setActionType(kind == "flash" ? VCButton::Flash : VCButton::Toggle);
    const int widget = int(button->id());
    vc->setEditMode(false);
    doc->inputOutputMap()->startUniverses();
    doc->masterTimer()->start();
    const Json on = invoke(doc, m_vc.get(), m_workspace.get(), "press_vc_widget",
                           {{"widget", widget}, {"action", "press"}});
    QVERIFY2(!on.contains("error"), on.dump().c_str());
    const auto dmx = [&]() {
        const auto universes = doc->inputOutputMap()->claimUniverses();
        const int value = uchar(universes.at(0)->preGMValues().at(0));
        doc->inputOutputMap()->releaseUniverses(false);
        return value;
    };
    QTRY_COMPARE(dmx(), 255);
    const Json release = invoke(doc, m_vc.get(), m_workspace.get(), "press_vc_widget",
                                {{"widget", widget}, {"action", "release"}});
    QVERIFY2(!release.contains("error"), release.dump().c_str());
    if (kind == "toggle")
    {
        QCOMPARE(dmx(), 255);
        const Json off = invoke(doc, m_vc.get(), m_workspace.get(), "press_vc_widget",
                                {{"widget", widget}, {"action", "toggle"}});
        QVERIFY2(!off.contains("error"), off.dump().c_str());
    }
    QTRY_VERIFY(!scene->isRunning());
    QTRY_COMPARE(dmx(), 0);
    QVERIFY(recorder->setRecording(false));
    QCOMPARE(show->commandTrack().count(), record ? 2 : 0);
    if (record)
    {
        QCOMPARE(show->commandTrack().commands().at(0).controlId,
                 vc->widget(widget)->recordingId());
        QVERIFY(show->commandTrack().commands().at(0).on);
        QVERIFY(!show->commandTrack().commands().at(1).on);
    }
}

void McpLiveControlTools_Test::quitApp_data()
{
    QTest::addColumn<bool>("modified");
    QTest::addColumn<bool>("recording");
    QTest::addColumn<bool>("discard");
    QTest::addColumn<bool>("accepted");
    QTest::newRow("clean") << false << false << false << true;
    QTest::newRow("unsaved-refused") << true << false << false << false;
    QTest::newRow("unsaved-discarded") << true << false << true << true;
    QTest::newRow("armed-refused") << false << true << false << false;
    QTest::newRow("armed-discarded") << false << true << true << true;
}

void McpLiveControlTools_Test::quitApp()
{
    QFETCH(bool, modified);
    QFETCH(bool, recording);
    QFETCH(bool, discard);
    QFETCH(bool, accepted);
    Doc *doc = m_app->doc();
    auto *recorder = m_app->findChild<ShowCommandRecorder *>();
    QVERIFY(recorder->setRecording(recording));
    if (modified) doc->setModified();
    else doc->resetModified();
    const auto cleanup = qScopeGuard([&]() {
        recorder->discardRecording();
        doc->resetModified();
    });
    QuitObserver observer;
    qApp->installEventFilter(&observer);
    qApp->installEventFilter(m_app.get());
    Json result;
    if (accepted)
    {
        QTimer timeout;
        timeout.setSingleShot(true);
        connect(&timeout, &QTimer::timeout, qApp, []() { QCoreApplication::exit(2); });
        timeout.start(2000);
        QTimer::singleShot(0, qApp, [&]() {
            result = invoke(doc, m_vc.get(), m_workspace.get(), "quit_app", {{"discard", discard}});
            if (result.contains("error"))
                QCoreApplication::exit(1);
            QCOMPARE(observer.requests, 0);
        });
        const int exitCode = qApp->exec();
        QVERIFY2(!result.contains("error"), result.dump().c_str());
        QCOMPARE(exitCode, 0);
        QCOMPARE(observer.requests, 1);
    }
    else
    {
        result = invoke(doc, m_vc.get(), m_workspace.get(), "quit_app", Json::object());
        QVERIFY2(result.contains("error"), result.dump().c_str());
        QCoreApplication::processEvents();
        QCOMPARE(observer.requests, 0);
        QCOMPARE(doc->isModified(), modified);
        QCOMPARE(recorder->isRecording(), recording);
    }
    qApp->removeEventFilter(&observer);
}

void McpLiveControlTools_Test::djTelemetry_data()
{
    QTest::addColumn<int>("deck");
    QTest::addColumn<bool>("playing");
    QTest::newRow("deck-one-live") << 1 << true;
    QTest::newRow("deck-two-suspended") << 2 << false;
}

void McpLiveControlTools_Test::djTelemetry()
{
    QFETCH(int, deck);
    QFETCH(bool, playing);
    auto *bridge = m_app->findChild<VdjBridge *>();
    QVERIFY(bridge);
    const auto clear = qScopeGuard([&]() {
        invoke(m_app->doc(), m_vc.get(), m_workspace.get(), "set_perform_mode", {{"on", false}});
        bridge->djFsm()->onDisconnected();
    });
    const auto send = [&](const QString &key, const QVariant &value) {
        return QMetaObject::invokeMethod(bridge, "onDeckTrigger", Q_ARG(int, deck - 1),
                                        Q_ARG(QString, key), Q_ARG(QVariant, value));
    };
    QVERIFY(send("get_filepath", "/music/n1a.mp3"));
    QVERIFY(send("get_title", "Test song"));
    QVERIFY(send("get_artist", "Test artist"));
    QVERIFY(send("get_bpm", 123.5));
    QVERIFY(send("get_time elapsed absolute", 2300));
    QVERIFY(send("loaded", "on"));
    QVERIFY(send("masterdeck", "on"));
    QVERIFY(send("play", playing ? "on" : "off"));
    QVERIFY(QMetaObject::invokeMethod(bridge, "onTickEnded"));
    const Json result = invoke(m_app->doc(), m_vc.get(), m_workspace.get(),
                               "set_perform_mode", {{"on", true}});
    QVERIFY2(!result.contains("error"), result.dump().c_str());
    const Json state = invoke(m_app->doc(), m_vc.get(), m_workspace.get(), "get_dj_state");
    QCOMPARE(state.at("performState"), Json(playing ? "Live" : "Suspended"));
    QCOMPARE(state.at("activeDeck"), Json(deck));
    QVERIFY(state.at("activeShowId").is_number_unsigned());
    QVERIFY(qobject_cast<Show *>(m_app->doc()->function(state.at("activeShowId").get<quint32>())));
    const Json song = state.at("decks").at(size_t(deck - 1));
    QCOMPARE(song.at("deck"), Json(deck));
    QCOMPARE(song.at("state"), Json(playing ? "Playing" : "Loaded"));
    QCOMPARE(song.at("filepath"), Json("/music/n1a.mp3"));
    QCOMPARE(song.at("title"), Json("Test song"));
    QCOMPARE(song.at("artist"), Json("Test artist"));
    QCOMPARE(song.at("bpm"), Json(123.5));
    QCOMPARE(song.at("elapsedMs"), Json(2300));
}

void McpLiveControlTools_Test::aRecordingSetIsReadOnly_data()
{
    QTest::addColumn<QString>("tool");

    QTest::newRow("add_show_items") << "add_show_items";
    QTest::newRow("delete_show_items") << "delete_show_items";
    QTest::newRow("create_shows") << "create_shows";
}

void McpLiveControlTools_Test::aRecordingSetIsReadOnly()
{
    QFETCH(QString, tool);
    Doc *doc = m_app->doc();
    auto *bridge = m_app->findChild<VdjBridge *>();
    auto *recorder = m_app->findChild<DjSetRecorder *>();
    QVERIFY(bridge);
    QVERIFY(recorder);
    const auto clear = qScopeGuard([&]() {
        invoke(doc, m_vc.get(), m_workspace.get(), "set_perform_mode", {{"on", false}});
        bridge->djFsm()->onDisconnected();
    });
    const auto send = [&](const QString &key, const QVariant &value) {
        return QMetaObject::invokeMethod(bridge, "onDeckTrigger", Q_ARG(int, 0), Q_ARG(QString, key),
                                        Q_ARG(QVariant, value));
    };
    QVERIFY(send("get_filepath", "/music/p7-set.mp3"));
    QVERIFY(send("get_title", "Set song"));
    QVERIFY(send("loaded", "on"));
    QVERIFY(send("masterdeck", "on"));
    QVERIFY(send("play", "on"));
    QVERIFY(QMetaObject::invokeMethod(bridge, "onTickEnded"));
    const Json on = invoke(doc, m_vc.get(), m_workspace.get(), "set_perform_mode", {{"on", true}});
    QVERIFY2(!on.contains("error"), on.dump().c_str());
    // the next write opens the deck's clip in a new set
    QVERIFY(send("get_time elapsed absolute", 1500));
    QVERIFY(QMetaObject::invokeMethod(bridge, "onTickEnded"));
    auto *set = qobject_cast<Show *>(doc->function(recorder->setShowId()));
    QVERIFY(set);
    const auto clips = [&]() {
        int count = 0;
        for (Track *track : set->tracks())
            count += track->showFunctions().count();
        return count;
    };
    const int tracks = set->tracks().count();
    QCOMPARE(clips(), 1);
    // sets started in one minute share their name: create_shows finds the first
    set->setName(QStringLiteral("Recording set %1").arg(set->id()));
    auto *scene = new Scene(doc);
    QVERIFY(doc->addFunction(scene));

    const auto edit = [&]() {
        const Json item = tool == "add_show_items"
            ? Json{{"showID", int(set->id())}, {"trackName", "Deck 1"},
                   {"items", Json::array({{{"functionID", int(scene->id())}, {"startTime", 60000}, {"duration", 1000}}})}}
            : tool == "delete_show_items"
            ? Json{{"showID", int(set->id())}, {"trackNames", Json::array({"Deck 1"})}}
            : Json{{"items", Json::array({{{"name", set->name().toStdString()},
                                           {"tracks", Json::array({{{"name", "Deck 3"}}})}}})}};
        const Json result = invokeShowTool(doc, m_vc.get(), tool.toLatin1().constData(), item);
        return (result.is_array() ? result.at(0) : result).value("code", std::string());
    };
    // Perform writes it, as the Show Manager shows it read-only
    QCOMPARE(QString::fromStdString(edit()), QStringLiteral("recording"));
    QCOMPARE(set->tracks().count(), tracks);
    QCOMPARE(clips(), 1);

    // complete once Perform is off
    invoke(doc, m_vc.get(), m_workspace.get(), "set_perform_mode", {{"on", false}});
    QCOMPARE(QString::fromStdString(edit()), QString());
}

void McpLiveControlTools_Test::drivenShows_data()
{
    QTest::addColumn<int>("active");
    QTest::addColumn<QString>("second");  // what deck 2 does once Perform is on
    // per driven Show: deck, Perform state, dominance, master intensity, transport, clock
    QTest::addColumn<QString>("drives");
    QTest::newRow("deck-one-active-both-play")
        << 1 << "play" << "1 Live dominant 0.80 running external|2 Live sub 0.50 running external";
    QTest::newRow("deck-two-active-and-paused")
        << 2 << "pause" << "1 Live sub 0.80 running external|2 Suspended dominant 0.50 paused external";
    QTest::newRow("deck-two-active-never-played")
        << 2 << "cue" << "1 Live sub 0.80 running external|2 Suspended dominant 0.50 stopped external";
}

void McpLiveControlTools_Test::drivenShows()
{
    QFETCH(int, active);
    QFETCH(QString, second);
    QFETCH(QString, drives);
    Doc *doc = m_app->doc();
    auto *bridge = m_app->findChild<VdjBridge *>();
    QVERIFY(bridge);
    const auto clear = qScopeGuard([&]() {
        invoke(doc, m_vc.get(), m_workspace.get(), "set_perform_mode", {{"on", false}});
        bridge->djFsm()->onDisconnected();
        QTest::qWait(100);
        doc->masterTimer()->stop();
    });
    const auto send = [&](int deck, const QString &key, const QVariant &value) {
        return QMetaObject::invokeMethod(bridge, "onDeckTrigger", Q_ARG(int, deck - 1),
                                        Q_ARG(QString, key), Q_ARG(QVariant, value));
    };
    const auto write = [&]() { return QMetaObject::invokeMethod(bridge, "onTickEnded"); };
    for (int deck : {1, 2})
    {
        const QString file = QStringLiteral("/music/p5-deck%1.mp3").arg(deck);
        QVERIFY(send(deck, "get_filepath", file));
        QVERIFY(send(deck, "get_title", QStringLiteral("Deck %1 song").arg(deck)));
        QVERIFY(send(deck, "get_time elapsed absolute", 1000 * deck));
        QVERIFY(send(deck, "volume", deck == 1 ? 0.8 : 0.5));
        QVERIFY(send(deck, "loaded", "on"));
        QVERIFY(send(deck, "masterdeck", deck == active ? "on" : "off"));
        QVERIFY(send(deck, "play", deck == 1 ? "on" : "off"));
    }
    QVERIFY(write());
    // the song Shows' Audio clips have no file: a Scene keeps them inside their content
    QList<Show *> shows;
    for (int deck : {1, 2})
    {
        auto *show = qobject_cast<Show *>(doc->function(bridge->showFactory()->showIdForFilepath(
            QStringLiteral("/music/p5-deck%1.mp3").arg(deck))));
        QVERIFY(show);
        shows << show;
        auto *scene = new Scene(doc);
        QVERIFY(doc->addFunction(scene));
        ShowFunction *clip = show->tracks().first()->createShowFunction(scene->id());
        clip->setStartTime(0);
        clip->setDuration(600000);
    }
    const Json result = invoke(doc, m_vc.get(), m_workspace.get(), "set_perform_mode", {{"on", true}});
    QVERIFY2(!result.contains("error"), result.dump().c_str());
    const Json queued = invoke(doc, m_vc.get(), m_workspace.get(), "get_dj_state");
    QCOMPARE(queued.at("drivenShows").at(0).at("transport"), Json("queued"));
    if (second != "cue")
    {
        QVERIFY(send(2, "play", "on"));
        QVERIFY(write());
    }
    doc->masterTimer()->start();
    QTRY_VERIFY(shows[0]->isRunning() && (second == "cue" || shows[1]->isRunning()));
    if (second == "pause")
    {
        QVERIFY(send(2, "play", "off"));
        QVERIFY(write());
    }

    const Json state = invoke(doc, m_vc.get(), m_workspace.get(), "get_dj_state");
    QCOMPARE(state.at("activeDeck"), Json(active));
    QCOMPARE(state.at("decks").at(0).at("weight"), Json(0.8));
    QCOMPARE(state.at("decks").at(1).at("weight"), Json(0.5));
    QStringList seen;
    for (const Json &drive : state.at("drivenShows"))
    {
        const auto *show = qobject_cast<Show *>(doc->function(drive.at("showId").get<quint32>()));
        QVERIFY(show);
        QCOMPARE(drive.at("name"), Json(show->name().toStdString()));
        const int deck = drive.at("deck").get<int>();
        QCOMPARE(drive.at("positionMs"), Json(1000 * deck));
        // the active deck's Show is the one Perform reports as active
        QCOMPARE(drive.at("showId") == state.at("activeShowId"), deck == active);
        seen << QStringLiteral("%1 %2 %3 %4 %5 %6").arg(deck)
                    .arg(QString::fromStdString(drive.at("state").get<std::string>()),
                         drive.at("dominant").get<bool>() ? "dominant" : "sub")
                    .arg(drive.at("masterIntensity").get<double>(), 0, 'f', 2)
                    .arg(QString::fromStdString(drive.at("transport").get<std::string>()),
                         drive.at("external").get<bool>() ? "external" : "autonomous");
    }
    QCOMPARE(seen.join('|'), drives);

    invoke(doc, m_vc.get(), m_workspace.get(), "set_perform_mode", {{"on", false}});
    const Json off = invoke(doc, m_vc.get(), m_workspace.get(), "get_dj_state");
    QCOMPARE(off.at("drivenShows"), Json::array());
}

void McpLiveControlTools_Test::invalidArguments_data()
{
    QTest::addColumn<QString>("tool");
    QTest::addColumn<QByteArray>("arguments");
    QTest::addColumn<QString>("error");
    for (const QString &tool : {QStringLiteral("set_perform_mode"), QStringLiteral("set_recording")})
    {
        const QByteArray name = tool.toUtf8();
        QTest::newRow((name + "-missing-on").constData()) << tool << QByteArray("{}") << QStringLiteral("on");
        QTest::newRow((name + "-wrong-type").constData()) << tool << QByteArray(R"({"on":1})") << QStringLiteral("boolean");
        QTest::newRow((name + "-unknown-field").constData()) << tool << QByteArray(R"({"on":true,"extra":1})") << QStringLiteral("unknown fields");
    }
    for (const QString &tool : {QStringLiteral("get_dj_state"), QStringLiteral("get_recording_state"), QStringLiteral("quit_app")})
    {
        const QByteArray name = tool.toUtf8();
        QTest::newRow((name + "-not-object").constData()) << tool << QByteArray("[]") << QStringLiteral("object");
        QTest::newRow((name + "-unknown-field").constData()) << tool << QByteArray(R"({"extra":1})") << QStringLiteral("unknown fields");
    }
    QTest::newRow("discard-string") << QStringLiteral("quit_app") << QByteArray(R"({"discard":"yes"})") << QStringLiteral("boolean");
    for (const QByteArray &args : {QByteArray("{}"), QByteArray(R"({"widget":-1,"action":"press"})"),
                                  QByteArray(R"({"widget":4294967295,"action":"press"})"),
                                  QByteArray(R"({"widget":1.5,"action":"press"})"),
                                  QByteArray(R"({"widget":true,"action":"press"})")})
        QTest::newRow(("widget-id-" + args).constData()) << QStringLiteral("press_vc_widget") << args << QStringLiteral("widget");
    QTest::newRow("missing-action") << QStringLiteral("press_vc_widget") << QByteArray(R"({"widget":0})") << QStringLiteral("action");
    QTest::newRow("invalid-action") << QStringLiteral("press_vc_widget") << QByteArray(R"({"widget":0,"action":"start"})") << QStringLiteral("action");
    QTest::newRow("wrong-action-type") << QStringLiteral("press_vc_widget") << QByteArray(R"({"widget":0,"action":true})") << QStringLiteral("action");
    QTest::newRow("button-unknown-field") << QStringLiteral("press_vc_widget") << QByteArray(R"({"widget":0,"action":"press","extra":1})") << QStringLiteral("unknown fields");
}

void McpLiveControlTools_Test::invalidArguments()
{
    QFETCH(QString, tool);
    QFETCH(QByteArray, arguments);
    QFETCH(QString, error);
    const Json result = invoke(m_app->doc(), m_vc.get(), m_workspace.get(),
                               tool.toUtf8().constData(), Json::parse(arguments.constData()));
    QVERIFY2(result.contains("error"), result.dump().c_str());
    QVERIFY2(QString::fromStdString(result.at("error").get<std::string>()).contains(error), result.dump().c_str());
}

void McpLiveControlTools_Test::buttonRefusals_data()
{
    QTest::addColumn<QString>("reason");
    QTest::newRow("edit-mode") << QStringLiteral("edit mode");
    QTest::newRow("disabled") << QStringLiteral("disabled");
    QTest::newRow("hidden") << QStringLiteral("hidden");
    QTest::newRow("no-function") << QStringLiteral("bound function");
    QTest::newRow("wrong-widget-type") << QStringLiteral("VC button");
    QTest::newRow("missing-widget") << QStringLiteral("VC button");
}

void McpLiveControlTools_Test::buttonRefusals()
{
    QFETCH(QString, reason);
    auto *doc = m_app->doc();
    auto *vc = m_app->virtualConsole();
    vc->setEditMode(reason == "edit mode");
    const auto restore = qScopeGuard([&]() { vc->setEditMode(true); });
    auto *button = new VCButton(doc, vc->page(0));
    vc->addWidgetToMap(button);
    vc->page(0)->addWidgetToPageMap(button);
    button->setActionType(VCButton::Blackout);
    button->setDisabled(reason == "disabled");
    button->setVisible(reason != "hidden");
    if (reason == "bound function")
        button->setActionType(VCButton::Toggle);
    const int id = reason == "VC button"
        ? (QTest::currentDataTag() == QByteArray("missing-widget") ? 2147483647 : int(vc->page(0)->id()))
        : int(button->id());
    const bool blackoutBefore = doc->inputOutputMap()->blackout();
    const Json result = invoke(doc, m_vc.get(), m_workspace.get(), "press_vc_widget",
                               {{"widget", id}, {"action", "press"}});
    QVERIFY2(result.contains("error"), result.dump().c_str());
    QVERIFY2(QString::fromStdString(result.at("error").get<std::string>()).contains(reason), result.dump().c_str());
    QCOMPARE(doc->inputOutputMap()->blackout(), blackoutBefore);
}

void McpLiveControlTools_Test::unavailableContext_data()
{
    QTest::addColumn<QString>("tool");
    QTest::addColumn<QByteArray>("arguments");
    QTest::newRow("perform") << QStringLiteral("set_perform_mode") << QByteArray(R"({"on":true})");
    QTest::newRow("dj-state") << QStringLiteral("get_dj_state") << QByteArray("{}");
    QTest::newRow("record") << QStringLiteral("set_recording") << QByteArray(R"({"on":true})");
    QTest::newRow("recorder-state") << QStringLiteral("get_recording_state") << QByteArray("{}");
    QTest::newRow("button") << QStringLiteral("press_vc_widget") << QByteArray(R"({"widget":0,"action":"press"})");
}

void McpLiveControlTools_Test::unavailableContext()
{
    QFETCH(QString, tool);
    QFETCH(QByteArray, arguments);
    VCBridgeV5 bridge(m_app->doc(), m_app->virtualConsole());
    const Json result = invoke(m_app->doc(), &bridge, m_workspace.get(), tool.toUtf8().constData(),
                               Json::parse(arguments.constData()));
    QVERIFY2(result.contains("error"), result.dump().c_str());
    QVERIFY2(QString::fromStdString(result.at("error").get<std::string>()).contains("application context"),
             result.dump().c_str());
}

void McpLiveControlTools_Test::failedRecordingFinalization()
{
    auto *doc = m_app->doc();
    auto *recorder = m_app->findChild<ShowCommandRecorder *>();
    auto *show = new Show(doc);
    QVERIFY(doc->addFunction(show));
    show->setSyncSource(ShowRunner::External);
    show->setExternalElapsedTime(1000);
    QVERIFY(recorder->setResolvedShow(show->id()));
    auto *scene = new Scene(doc);
    QVERIFY(doc->addFunction(scene));
    const auto discard = qScopeGuard([&]() { recorder->discardRecording(); });
    const Json on = invoke(doc, m_vc.get(), m_workspace.get(), "set_recording", {{"on", true}});
    QVERIFY2(!on.contains("error"), on.dump().c_str());
    ShowCommandInput input;
    input.origin = ShowCommandOrigin::Pointer;
    input.action = ShowCommandAction::Start;
    input.functionId = scene->id();
    QVERIFY(recorder->submitUserInput(input));
    QVERIFY(doc->deleteFunction(scene->id()));
    show->setExternalElapsedTime(1500);
    QVERIFY(!recorder->submitUserInput(input));
    const Json off = invoke(doc, m_vc.get(), m_workspace.get(), "set_recording", {{"on", false}});
    QVERIFY2(off.contains("error"), off.dump().c_str());
    const Json state = invoke(doc, m_vc.get(), m_workspace.get(), "get_recording_state");
    QCOMPARE(state.at("recording"), Json(true));
    QCOMPARE(state.at("boundShowId"), Json(show->id()));
    QVERIFY(!state.at("lastError").get<std::string>().empty());
    QCOMPARE(off.at("error"), state.at("lastError"));
}

void McpLiveControlTools_Test::registration_data()
{
    QTest::addColumn<bool>("vc");
    QTest::addColumn<bool>("workspace");
    QTest::newRow("both-bridges") << true << true;
    QTest::newRow("vc-only") << true << false;
    QTest::newRow("workspace-only") << false << true;
    QTest::newRow("no-bridges") << false << false;
}

void McpLiveControlTools_Test::registration()
{
    QFETCH(bool, vc);
    QFETCH(bool, workspace);
    fastmcpp::tools::ToolManager tm;
    registerLiveControlTools(tm, m_app->doc(), vc ? m_vc.get() : nullptr,
                             workspace ? m_workspace.get() : nullptr);
    for (const char *name : {"set_perform_mode", "get_dj_state", "set_recording",
                             "get_recording_state", "press_vc_widget"})
        QCOMPARE(tm.has(name), vc);
    QCOMPARE(tm.has("quit_app"), workspace);
}

void McpLiveControlTools_Test::freezeHoldRelease_data()
{
    QTest::addColumn<bool>("editMode");
    QTest::newRow("play-mode") << false;
    QTest::newRow("entered-edit-mode") << true;
}

void McpLiveControlTools_Test::freezeHoldRelease()
{
    QFETCH(bool, editMode);
    auto *doc = m_app->doc();
    auto *vc = m_app->virtualConsole();
    vc->setEditMode(false);
    auto *button = new VCButton(doc, vc->page(0));
    vc->addWidgetToMap(button);
    vc->page(0)->addWidgetToPageMap(button);
    button->setActionType(VCButton::FreezeHold);
    const auto cleanup = qScopeGuard([&]() {
        button->setDisabled(true);
        vc->setEditMode(true);
    });
    const Json press = invoke(doc, m_vc.get(), m_workspace.get(), "press_vc_widget",
                              {{"widget", button->id()}, {"action", "press"}});
    QVERIFY2(!press.contains("error"), press.dump().c_str());
    QVERIFY(doc->inputOutputMap()->frozenMomentary());
    vc->setEditMode(editMode);
    const Json release = invoke(doc, m_vc.get(), m_workspace.get(), "press_vc_widget",
                                {{"widget", button->id()}, {"action", "release"}});
    QVERIFY2(!release.contains("error"), release.dump().c_str());
    QVERIFY(!doc->inputOutputMap()->frozenMomentary());
}

void McpLiveControlTools_Test::serverSurface_data()
{
    QTest::addColumn<QString>("tool");
    for (const char *name : {"set_perform_mode", "get_dj_state", "set_recording",
                             "get_recording_state", "press_vc_widget", "quit_app"})
        QTest::newRow(name) << QString::fromLatin1(name);
}

void McpLiveControlTools_Test::serverSurface()
{
    QFETCH(QString, tool);
    QTcpServer port;
    QVERIFY(port.listen(QHostAddress::LocalHost, 0));
    const quint16 number = port.serverPort();
    port.close();
    McpServer server(m_app->doc(), m_vc.get(), m_app->functionManager(), nullptr, m_workspace.get());
    server.startHttp(number);
    const auto stop = qScopeGuard([&]() { server.stopHttp(); });
    QNetworkAccessManager network;
    QNetworkRequest request(QUrl(QStringLiteral("http://127.0.0.1:%1/mcp").arg(number)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Accept", "application/json, text/event-stream");
    const auto post = [&](const Json &payload) {
        QNetworkReply *reply = network.post(request, QByteArray::fromStdString(payload.dump()));
        QSignalSpy finished(reply, &QNetworkReply::finished);
        if (!reply->isFinished())
            finished.wait(5000);
        return reply;
    };
    QNetworkReply *init = post({
        {"jsonrpc", "2.0"}, {"id", 1}, {"method", "initialize"},
        {"params", {{"protocolVersion", "2025-03-26"}, {"capabilities", Json::object()},
                    {"clientInfo", {{"name", "n1a-test"}, {"version", "1"}}}}}
    });
    QVERIFY(init->isFinished());
    QCOMPARE(init->error(), QNetworkReply::NoError);
    const QByteArray session = init->rawHeader("Mcp-Session-Id");
    QVERIFY(!session.isEmpty());
    request.setRawHeader("Mcp-Session-Id", session);
    QNetworkReply *list = post({{"jsonrpc", "2.0"}, {"id", 2}, {"method", "tools/list"},
                                {"params", Json::object()}});
    QVERIFY(list->isFinished());
    QCOMPARE(list->error(), QNetworkReply::NoError);
    const Json result = Json::parse(list->readAll().constData());
    const Json &tools = result.at("result").at("tools");
    const auto found = std::find_if(tools.begin(), tools.end(), [&](const Json &entry) {
        return entry.at("name") == tool.toStdString();
    });
    QVERIFY2(found != tools.end(), qPrintable(tool));
    QCOMPARE(found->at("annotations").at("readOnlyHint"),
             Json(tool == "get_dj_state" || tool == "get_recording_state"));
    QCOMPARE(found->at("annotations").at("idempotentHint"), Json(tool != "press_vc_widget"));
    for (const char *keyword : {"oneOf", "anyOf", "allOf"})
        QVERIFY(found->at("inputSchema").dump().find(keyword) == std::string::npos);
    if (tool == "get_dj_state" || tool == "get_recording_state")
    {
        QNetworkReply *call = post({{"jsonrpc", "2.0"}, {"id", 3}, {"method", "tools/call"},
                                    {"params", {{"name", tool.toStdString()}, {"arguments", Json::object()}}}});
        QVERIFY(call->isFinished());
        QCOMPARE(call->error(), QNetworkReply::NoError);
        const Json called = Json::parse(call->readAll().constData());
        QCOMPARE(called.at("result").at("isError"), Json(false));
        QVERIFY(called.at("result").at("structuredContent").contains(
            tool == "get_dj_state" ? "performMode" : "recording"));
    }
}

void McpLiveControlTools_Test::latchedToggle_data()
{
    QTest::addColumn<int>("kind");
    QTest::addColumn<bool>("record");
    QTest::newRow("blackout-rec-off") << int(VCButton::Blackout) << false;
    QTest::newRow("blackout-rec-on") << int(VCButton::Blackout) << true;
    QTest::newRow("freeze-rec-off") << int(VCButton::Freeze) << false;
    QTest::newRow("freeze-rec-on") << int(VCButton::Freeze) << true;
}

void McpLiveControlTools_Test::latchedToggle()
{
    QFETCH(int, kind);
    QFETCH(bool, record);
    auto *doc = m_app->doc();
    auto *vc = m_app->virtualConsole();
    auto *recorder = m_app->findChild<ShowCommandRecorder *>();
    auto *show = new Show(doc);
    QVERIFY(doc->addFunction(show));
    QVERIFY(recorder->setResolvedShow(show->id()));
    QVERIFY(recorder->setRecording(record));
    vc->setEditMode(false);
    auto *button = new VCButton(doc, vc->page(0));
    vc->addWidgetToMap(button);
    vc->page(0)->addWidgetToPageMap(button);
    button->setActionType(VCButton::ButtonAction(kind));
    const auto cleanup = qScopeGuard([&]() {
        recorder->discardRecording();
        doc->inputOutputMap()->setBlackout(false);
        doc->inputOutputMap()->setFrozen(false);
        vc->setEditMode(true);
    });
    const auto active = [&]() {
        return kind == int(VCButton::Blackout) ? doc->inputOutputMap()->blackout()
                                              : doc->inputOutputMap()->frozenLatch();
    };
    const auto send = [&](const char *action) {
        return invoke(doc, m_vc.get(), m_workspace.get(), "press_vc_widget",
                      {{"widget", button->id()}, {"action", action}});
    };
    const Json press = send("press");
    QVERIFY2(!press.contains("error"), press.dump().c_str());
    QVERIFY(active());
    QCOMPARE(button->state(), VCButton::Active);
    const Json release = send("release");
    QVERIFY2(!release.contains("error"), release.dump().c_str());
    QVERIFY(active());
    const Json toggle = send("toggle");
    QVERIFY2(!toggle.contains("error"), toggle.dump().c_str());
    QVERIFY(!active());
    QCOMPARE(button->state(), VCButton::Inactive);
    QVERIFY(recorder->setRecording(false));
    QCOMPARE(show->commandTrack().count(), record ? 2 : 0);
    if (record)
    {
        QVERIFY(show->commandTrack().commands().at(0).on);
        QVERIFY(!show->commandTrack().commands().at(1).on);
    }
}

QTEST_MAIN(McpLiveControlTools_Test)
