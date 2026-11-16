#include "live_control_tools_test.h"
#include "app.h"
#include "doc.h"
#include "mastertimer.h"
#include "show.h"
#include "showmanager.h"
#include "showcommandrecorder.h"
#include "functionparent.h"
#include "collection.h"
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
#include "functionmanager.h"
#include "djsetrecorder.h"
#include "vdjbridge.h"
#include "showfactory.h"
#include "showrunner.h"
#include "audio.h"
#include "edmanalysisreader.h"
#include "songdirector.h"
#include "edmstructuremapping.h"
#include "vcbridgev5.h"
#include "workspacebridgev5.h"
#include "tool_registry.h"
#include "mcpserver.h"

#include <QtTest>
#include <QCryptographicHash>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QTemporaryDir>
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

/** The analysis folder the app is configured with */
QString analysisDir()
{
    return QSettings().value("edmanalysis/outputDir").toString();
}

Json invokeShowTool(Doc *doc, VCBridge *vc, const char *name, const Json &args)
{
    fastmcpp::tools::ToolManager tm;
    registerShowTools(tm, doc, vc);
    if (!tm.has(name))
        return {{"error", std::string("tool not registered: ") + name}};
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
    // an analysis folder of its own, as a user who moved it: everything reads this one
    QSettings().setValue("edmanalysis/outputDir", m_settings.filePath("configured-edm-analysis"));
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

void McpLiveControlTools_Test::aClipWaitingForThePlayingSetHoldsTheSave_data()
{
    QTest::addColumn<bool>("performOff");  // the waiting clip closed

    QTest::newRow("its song still plays") << false;
    QTest::newRow("Perform went off") << true;
}

void McpLiveControlTools_Test::aClipWaitingForThePlayingSetHoldsTheSave()
{
    QFETCH(bool, performOff);
    Doc *doc = m_app->doc();
    auto *bridge = m_app->findChild<VdjBridge *>();
    auto *recorder = m_app->findChild<DjSetRecorder *>();
    QVERIFY(bridge);
    QVERIFY(recorder);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Show *set = nullptr;
    const auto clear = qScopeGuard([&]() {
        invoke(doc, m_vc.get(), m_workspace.get(), "set_perform_mode", {{"on", false}});
        bridge->djFsm()->onDisconnected();
        if (set != nullptr && set->isRunning())
            set->stopAndWait();
        doc->masterTimer()->stop();
    });
    const auto send = [&](int deck, const QString &key, const QVariant &value) {
        return QMetaObject::invokeMethod(bridge, "onDeckTrigger", Q_ARG(int, deck - 1), Q_ARG(QString, key),
                                        Q_ARG(QVariant, value));
    };
    const auto play = [&](int deck) {
        QVERIFY(send(deck, "get_filepath", QStringLiteral("/music/p7-wait%1.mp3").arg(deck)));
        QVERIFY(send(deck, "get_title", QStringLiteral("Wait song %1").arg(deck)));
        QVERIFY(send(deck, "loaded", "on"));
        QVERIFY(send(deck, "masterdeck", deck == 1 ? "on" : "off"));
        QVERIFY(send(deck, "play", "on"));
        QVERIFY(QMetaObject::invokeMethod(bridge, "onTickEnded"));
        QVERIFY(send(deck, "get_time elapsed absolute", 1500));
        QVERIFY(QMetaObject::invokeMethod(bridge, "onTickEnded"));
    };
    play(1);
    const Json on = invoke(doc, m_vc.get(), m_workspace.get(), "set_perform_mode", {{"on", true}});
    QVERIFY2(!on.contains("error"), on.dump().c_str());
    QVERIFY(send(1, "get_time elapsed absolute", 1600));
    QVERIFY(QMetaObject::invokeMethod(bridge, "onTickEnded"));
    set = qobject_cast<Show *>(doc->function(recorder->setShowId()));
    QVERIFY(set);
    // somebody plays the set while it records: deck 2's song waits for it to stop
    doc->masterTimer()->start();
    set->setSyncSource(ShowRunner::External);
    set->setExternalElapsedTime(0);
    set->start(doc->masterTimer(), FunctionParent::master());
    QTRY_VERIFY(set->isRunning());
    play(2);
    if (performOff)
        invoke(doc, m_vc.get(), m_workspace.get(), "set_perform_mode", {{"on", false}});
    QCOMPARE(set->tracks().at(1)->showFunctions().count(), 0);

    // a saved set holds every song played
    QVERIFY(!m_app->saveWorkspace(dir.filePath(QStringLiteral("waiting.qxw"))));
    set->stopAndWait();
    QTRY_COMPARE_WITH_TIMEOUT(set->tracks().at(1)->showFunctions().count(), 1, 2000);
    QVERIFY(m_app->saveWorkspace(dir.filePath(QStringLiteral("complete.qxw"))));
}

void McpLiveControlTools_Test::songBlocks_data()
{
    QTest::addColumn<QString>("target");
    QTest::addColumn<int>("creates");
    QTest::addColumn<QString>("error");
    QTest::newRow("analysed song") << QStringLiteral("analysed") << 1 << QString();
    QTest::newRow("second create") << QStringLiteral("analysed") << 2 << QStringLiteral("already has a Song Director track");
    QTest::newRow("song not analysed") << QStringLiteral("not analysed") << 1 << QStringLiteral("no analysed audio clip");
    QTest::newRow("not a show") << QStringLiteral("scene") << 1 << QStringLiteral("no Show with ID");
    QTest::newRow("missing function") << QStringLiteral("missing") << 1 << QStringLiteral("no Show with ID");
}

void McpLiveControlTools_Test::songBlocks()
{
    QFETCH(QString, target);
    QFETCH(int, creates);
    QFETCH(QString, error);
    auto *doc = m_app->doc();

    // the analysis folder the app reads, under the test's HOME: intro 0-9 s, drop 9-25 s
    const QDir analysis(analysisDir());
    QVERIFY(analysis.mkpath("final"));
    QFile index(analysis.filePath("index.json"));
    QVERIFY(index.open(QIODevice::WriteOnly));
    index.write(R"({"bbbbbbbbbbbbbbbb": {"path": "/music/song-director.mp3"}})");
    index.close();
    QFile final(analysis.filePath("final/bbbbbbbbbbbbbbbb.json"));
    QVERIFY(final.open(QIODevice::WriteOnly));
    final.write(R"({"schema": "qlcplus-edm-final/3", "grid": {"bpm": 120.0, "period": 0.5, "beats_per_bar": 4, "first_downbeat": 0.0},
        "sections": [{"label": "intro", "start": 0.0, "end": 9.0, "confidence": 70, "label_confidence": 80},
                     {"label": "drop", "start": 9.0, "end": 25.0, "confidence": 50, "label_confidence": 40, "unsure": true}]})");
    final.close();

    auto *show = new Show(doc);
    show->setName("Song");
    QVERIFY(doc->addFunction(show));
    auto *audio = new Audio(doc);
    QVERIFY(doc->addFunction(audio));
    audio->setSourceFileName(target == "not analysed" ? "/music/other.mp3" : "/music/song-director.mp3");
    auto *track = new Track(Function::invalidId(), show);
    show->addTrack(track);
    track->createShowFunction(audio->id())->setStartTime(2000);
    auto *scene = new Scene(doc);
    QVERIFY(doc->addFunction(scene));
    const quint32 id = target == "scene" ? scene->id() : target == "missing" ? 99999u : show->id();

    Json created;
    for (int i = 0; i < creates; i++)
        created = invokeShowTool(doc, m_vc.get(), "create_song_blocks", {{"showID", id}});
    const Json queried = invokeShowTool(doc, m_vc.get(), "query_song_blocks", {{"showID", id}});

    if (error.isEmpty() == false)
    {
        QVERIFY2(created.contains("error"), created.dump().c_str());
        QVERIFY2(QString::fromStdString(created.at("error").get<std::string>()).contains(error), created.dump().c_str());
        if (target == "analysed")
            QCOMPARE(show->tracks().count(), 2);
        else if (target == "not analysed")
        {
            QCOMPARE(show->tracks().count(), 1);
            QCOMPARE(queried.at("trackID"), Json(nullptr));
            QCOMPARE(queried.at("blocks"), Json::array());
        }
        return;
    }

    QVERIFY2(!created.contains("error"), created.dump().c_str());
    QCOMPARE(created, queried);
    Track *director = m_app->findChild<SongDirector *>()->track(show);
    QVERIFY(director != nullptr);
    QCOMPARE(queried.at("showID"), Json(show->id()));
    QCOMPARE(queried.at("trackID"), Json(director->id()));
    const Json blocks = queried.at("blocks");
    QCOMPARE(int(blocks.size()), 2);
    QCOMPARE(blocks[0].at("label"), Json("intro"));
    QCOMPARE(blocks[0].at("number"), Json(1));
    QCOMPARE(blocks[0].at("section"), Json(0));
    QCOMPARE(blocks[0].at("startMs"), Json(2000));
    QCOMPARE(blocks[0].at("durationMs"), Json(9000));
    QCOMPARE(blocks[0].at("confidence"), Json(80));
    QCOMPARE(blocks[0].at("unsure"), Json(false));
    QCOMPARE(blocks[1].at("label"), Json("drop"));
    QCOMPARE(blocks[1].at("startMs"), Json(11000));
    QCOMPARE(blocks[1].at("durationMs"), Json(16000));
    QCOMPARE(blocks[1].at("unsure"), Json(true));
    for (int i = 0; i < 2; i++)
    {
        const ShowFunction *sf = director->showFunctions().at(i);
        QCOMPARE(blocks[i].at("functionID"), Json(sf->functionID()));
        QCOMPARE(blocks[i].at("content"), Json::array());
        QCOMPARE(QString::fromStdString(blocks[i].at("name").get<std::string>()),
                 doc->function(sf->functionID())->name());
    }
}

void McpLiveControlTools_Test::songBlockDrop_data()
{
    QTest::addColumn<int>("dropAtMs");      // show time of the drop
    QTest::addColumn<QString>("error");     // of the drop
    QTest::addColumn<int>("filledBlock");   // block holding the scene afterwards, -1 for none

    // blocks: intro 2000-11000, drop 11000-27000
    QTest::newRow("drop into the first block") << 5000 << QString() << 0;
    QTest::newRow("drop into the second block") << 20000 << QString() << 1;
    QTest::newRow("drop at the block start") << 11000 << QString() << 1;
    QTest::newRow("drop before the first block") << 500 << QStringLiteral("no Song Director block") << -1;
    QTest::newRow("drop after the last block") << 27000 << QStringLiteral("no Song Director block") << -1;
}

void McpLiveControlTools_Test::songBlockDrop()
{
    QFETCH(int, dropAtMs);
    QFETCH(QString, error);
    QFETCH(int, filledBlock);
    auto *doc = m_app->doc();

    const QDir analysis(analysisDir());
    QVERIFY(analysis.mkpath("final"));
    QFile index(analysis.filePath("index.json"));
    QVERIFY(index.open(QIODevice::WriteOnly));
    const QString id = QString(QCryptographicHash::hash(QTest::currentDataTag(), QCryptographicHash::Sha256).toHex()).left(16);
    const QString path = QStringLiteral("/music/song-drop-%1.mp3").arg(id);
    index.write(QStringLiteral(R"({"%1": {"path": "%2"}})").arg(id, path).toUtf8());
    index.close();
    QFile final(analysis.filePath("final/" + id + ".json"));
    QVERIFY(final.open(QIODevice::WriteOnly));
    final.write(R"({"schema": "qlcplus-edm-final/3", "grid": {"bpm": 120.0, "period": 0.5, "beats_per_bar": 4, "first_downbeat": 0.0},
        "sections": [{"label": "intro", "start": 0.0, "end": 9.0, "confidence": 70, "label_confidence": 80},
                     {"label": "drop", "start": 9.0, "end": 25.0, "confidence": 90, "label_confidence": 90}]})");
    final.close();

    auto *show = new Show(doc);
    show->setName("Song");
    QVERIFY(doc->addFunction(show));
    auto *audio = new Audio(doc);
    QVERIFY(doc->addFunction(audio));
    audio->setSourceFileName(path);
    auto *track = new Track(Function::invalidId(), show);
    show->addTrack(track);
    track->createShowFunction(audio->id())->setStartTime(2000);
    auto *scene = new Scene(doc);
    QVERIFY(doc->addFunction(scene));
    const Json created = invokeShowTool(doc, m_vc.get(), "create_song_blocks", {{"showID", show->id()}});
    QVERIFY2(!created.contains("error"), created.dump().c_str());
    Track *director = m_app->findChild<SongDirector *>()->track(show);
    QVERIFY(director != nullptr);

    const Json dropped = invokeShowTool(doc, m_vc.get(), "drop_on_song_block",
                                        {{"showID", show->id()}, {"functionIDs", {scene->id()}}, {"timeMs", dropAtMs}});
    if (error.isEmpty() == false)
    {
        QVERIFY2(dropped.contains("error"), dropped.dump().c_str());
        QVERIFY2(QString::fromStdString(dropped.at("error").get<std::string>()).contains(error), dropped.dump().c_str());
    }
    else
        QVERIFY2(!dropped.contains("error"), dropped.dump().c_str());

    // a drop changes content only: the blocks keep start and length
    QCOMPARE(director->showFunctions().count(), 2);
    QCOMPARE(director->showFunctions().at(0)->startTime(), 2000u);
    QCOMPARE(director->showFunctions().at(0)->duration(), 9000u);
    QCOMPARE(director->showFunctions().at(1)->startTime(), 11000u);
    QCOMPARE(director->showFunctions().at(1)->duration(), 16000u);
    for (int i = 0; i < 2; i++)
    {
        auto *collection = qobject_cast<Collection *>(doc->function(director->showFunctions().at(i)->functionID()));
        QVERIFY(collection != nullptr);
        QCOMPARE(collection->functions(), i == filledBlock ? QList<quint32> { scene->id() } : QList<quint32>());
    }
}

void McpLiveControlTools_Test::songBlockMarks_data()
{
    QTest::addColumn<QByteArray>("sections");
    QTest::addColumn<QStringList>("expected");
    // marks read "unsure:percent": the percent is the section's label confidence, the
    // analysis' own "confidence" is a constant 50 on the sections it flags unsure
    QTest::newRow("unsure shows the label confidence")
        << QByteArray(R"([{"label": "intro", "start": 0.0, "end": 9.0, "confidence": 80, "label_confidence": 95},
                          {"label": "buildup", "start": 9.0, "end": 25.0, "confidence": 50, "label_confidence": 9, "unsure": true}])")
        << QStringList{"0:95", "1:9"};
    QTest::newRow("no unsure section")
        << QByteArray(R"([{"label": "drop", "start": 0.0, "end": 9.0, "confidence": 90, "label_confidence": 38},
                          {"label": "outro", "start": 9.0, "end": 25.0, "confidence": 50, "label_confidence": 52}])")
        << QStringList{"0:38", "0:52"};
    QTest::newRow("every section unsure")
        << QByteArray(R"([{"label": "buildup", "start": 0.0, "end": 9.0, "confidence": 50, "label_confidence": 23, "unsure": true},
                          {"label": "buildup", "start": 9.0, "end": 25.0, "confidence": 50, "label_confidence": 18, "unsure": true}])")
        << QStringList{"1:23", "1:18"};
}

void McpLiveControlTools_Test::songBlockMarks()
{
    QFETCH(QByteArray, sections);
    QFETCH(QStringList, expected);
    auto *doc = m_app->doc();

    const QDir analysis(analysisDir());
    QVERIFY(analysis.mkpath("final"));
    QFile index(analysis.filePath("index.json"));
    QVERIFY(index.open(QIODevice::WriteOnly));
    // one file per row: the reader caches an analysis by its file's modification time
    const QString id = QString(QCryptographicHash::hash(QTest::currentDataTag(), QCryptographicHash::Sha256).toHex()).left(16);
    const QString path = QStringLiteral("/music/song-marks-%1.mp3").arg(id);
    index.write(QStringLiteral(R"({"%1": {"path": "%2"}})").arg(id, path).toUtf8());
    index.close();
    QFile final(analysis.filePath("final/" + id + ".json"));
    QVERIFY(final.open(QIODevice::WriteOnly));
    final.write(R"({"schema": "qlcplus-edm-final/3", "grid": {"bpm": 120.0, "period": 0.5, "beats_per_bar": 4, "first_downbeat": 0.0},
        "sections": )" + sections + "}");
    final.close();

    auto *show = new Show(doc);
    show->setName("Marks");
    QVERIFY(doc->addFunction(show));
    auto *audio = new Audio(doc);
    QVERIFY(doc->addFunction(audio));
    audio->setSourceFileName(path);
    auto *track = new Track(Function::invalidId(), show);
    show->addTrack(track);
    track->createShowFunction(audio->id());

    const Json created = invokeShowTool(doc, m_vc.get(), "create_song_blocks", {{"showID", show->id()}});
    QVERIFY2(!created.contains("error"), created.dump().c_str());
    Track *director = m_app->findChild<SongDirector *>()->track(show);
    QVERIFY(director != nullptr);

    // what the Show Manager QML asks for each block it draws
    ShowManager *manager = m_app->showManager();
    QCOMPARE(director->showFunctions().count(), expected.count());
    QStringList marks;
    for (const ShowFunction *sf : director->showFunctions())
    {
        const QVariantMap mark = manager->songBlockMark(sf->functionID());
        marks.append(QStringLiteral("%1:%2").arg(mark.value("unsure").toBool() ? 1 : 0).arg(mark.value("confidence").toInt()));
    }
    QCOMPARE(marks, expected);

    // a Function that is not a block has no mark, and neither has the audio clip
    QVERIFY(manager->songBlockMark(audio->id()).isEmpty());
    QVERIFY(manager->songBlockMark(99999).isEmpty());
}

void McpLiveControlTools_Test::songBlocksArguments_data()
{
    QTest::addColumn<QString>("tool");
    QTest::addColumn<QByteArray>("arguments");
    QTest::addColumn<QString>("error");
    for (const QString &tool : {QStringLiteral("create_song_blocks"), QStringLiteral("query_song_blocks")})
    {
        const QByteArray name = tool.toUtf8();
        QTest::newRow((name + "-missing").constData()) << tool << QByteArray("{}") << QStringLiteral("showID");
        QTest::newRow((name + "-negative").constData()) << tool << QByteArray(R"({"showID":-1})") << QStringLiteral("showID");
        QTest::newRow((name + "-string").constData()) << tool << QByteArray(R"({"showID":"1"})") << QStringLiteral("showID");
        QTest::newRow((name + "-unknown-field").constData()) << tool << QByteArray(R"({"showID":1,"extra":1})") << QStringLiteral("unknown fields");
    }
}

void McpLiveControlTools_Test::songBlocksArguments()
{
    QFETCH(QString, tool);
    QFETCH(QByteArray, arguments);
    QFETCH(QString, error);
    const Json result = invokeShowTool(m_app->doc(), m_vc.get(), tool.toUtf8().constData(), Json::parse(arguments.constData()));
    QVERIFY2(result.contains("error"), result.dump().c_str());
    QVERIFY2(QString::fromStdString(result.at("error").get<std::string>()).contains(error), result.dump().c_str());
}

void McpLiveControlTools_Test::songLookDefaults_data()
{
    QTest::addColumn<QString>("arguments");
    QTest::addColumn<QString>("error");
    QTest::addColumn<int>("dropDefault");
    QTest::addColumn<bool>("preset");   // drop defaults to the scene before the call
    QTest::newRow("query only") << QStringLiteral("{}") << QString() << -1 << false;
    QTest::newRow("set a scene") << QStringLiteral(R"({"label":"drop","functionID":$SCENE})") << QString() << 1 << false;
    QTest::newRow("set a collection") << QStringLiteral(R"({"label":"drop","functionID":$COLLECTION})") << QString() << 2 << false;
    QTest::newRow("clear") << QStringLiteral(R"({"label":"drop","functionID":-1})") << QString() << -1 << true;
    QTest::newRow("a Show is no look") << QStringLiteral(R"({"label":"drop","functionID":$SHOW})") << QStringLiteral("not a look") << -1 << false;
    QTest::newRow("missing function") << QStringLiteral(R"({"label":"drop","functionID":4000})") << QStringLiteral("not a look") << -1 << false;
    QTest::newRow("unknown label") << QStringLiteral(R"({"label":"chorus","functionID":$SCENE})") << QStringLiteral("unknown label") << -1 << false;
    QTest::newRow("label alone") << QStringLiteral(R"({"label":"drop"})") << QStringLiteral("go together") << -1 << false;
    QTest::newRow("function alone") << QStringLiteral(R"({"functionID":$SCENE})") << QStringLiteral("go together") << -1 << false;
    QTest::newRow("string function") << QStringLiteral(R"({"label":"drop","functionID":"1"})") << QStringLiteral("functionID") << -1 << false;
    QTest::newRow("unknown field") << QStringLiteral(R"({"extra":1})") << QStringLiteral("unknown fields") << -1 << false;
    QTest::newRow("id above int32 is no function") << QStringLiteral(R"({"label":"drop","functionID":2147483648})")
                                                   << QStringLiteral("not a look") << 1 << true;
    QTest::newRow("largest id is no function") << QStringLiteral(R"({"label":"drop","functionID":4294967294})")
                                               << QStringLiteral("not a look") << 1 << true;
    QTest::newRow("id beyond the id range") << QStringLiteral(R"({"label":"drop","functionID":4294967295})")
                                            << QStringLiteral("functionID") << 1 << true;
}

void McpLiveControlTools_Test::songLookDefaults()
{
    QFETCH(QString, arguments);
    QFETCH(QString, error);
    QFETCH(int, dropDefault);
    QFETCH(bool, preset);
    auto *doc = m_app->doc();

    auto *scene = new Scene(doc);
    QVERIFY(doc->addFunction(scene));
    auto *collection = new Collection(doc);
    QVERIFY(doc->addFunction(collection));
    auto *show = new Show(doc);
    QVERIFY(doc->addFunction(show));
    arguments.replace("$SCENE", QString::number(scene->id()))
        .replace("$COLLECTION", QString::number(collection->id()))
        .replace("$SHOW", QString::number(show->id()));
    const int expected = dropDefault == 1 ? int(scene->id()) : dropDefault == 2 ? int(collection->id()) : -1;
    auto *mapping = m_app->findChild<EdmStructureMapping *>();
    mapping->setFunctionId("drop", preset ? int(scene->id()) : -1);
    auto noDefaults = qScopeGuard([mapping]() {
        for (const QString &label : EdmStructureMapping::labels())
            mapping->setFunctionId(label, -1);
    });

    const Json result = invokeShowTool(doc, m_vc.get(), "song_look_defaults", Json::parse(arguments.toStdString()));
    if (error.isEmpty() == false)
    {
        QVERIFY2(result.contains("error"), result.dump().c_str());
        QVERIFY2(QString::fromStdString(result.at("error").get<std::string>()).contains(error), result.dump().c_str());
        QCOMPARE(mapping->functionId("drop"), preset ? int(scene->id()) : -1);
        return;
    }
    QVERIFY2(!result.contains("error"), result.dump().c_str());
    QCOMPARE(result.at("defaults").at("drop"), Json(expected));
    QCOMPARE(result.at("defaults").at("outro"), Json(-1));
    QCOMPARE(m_app->findChild<EdmStructureMapping *>()->functionId("drop"), expected);
}

namespace
{
/** Each write gets a newer file time, so a reader's cache takes it */
QDateTime nextFileTime()
{
    static int written = 0;
    return QDateTime::currentDateTime().addSecs(++written);
}

/** (Re)write the current row's analysis */
void rewriteAnalysis(const QByteArray &sections)
{
    const QString id = QString(QCryptographicHash::hash(QTest::currentDataTag(), QCryptographicHash::Sha256).toHex()).left(16);
    QFile final(QDir(analysisDir()).filePath("final/" + id + ".json"));
    QVERIFY(final.open(QIODevice::WriteOnly));
    final.write(R"({"schema": "qlcplus-edm-final/3", "grid": {"bpm": 120.0, "period": 0.5, "beats_per_bar": 4, "first_downbeat": 0.0},
        "sections": )" + sections + "}");
    QVERIFY(final.flush());   // a write on close would set the time again
    QVERIFY(final.setFileTime(nextFileTime(), QFileDevice::FileModificationTime));
}

/** A song Show whose first audio clip (at 2000 ms) plays a song analysed as \a sections, in the app's analysis folder */
Show *analysedSongShow(Doc *doc, const QByteArray &sections)
{
    const QDir analysis(analysisDir());
    const QString id = QString(QCryptographicHash::hash(QTest::currentDataTag(), QCryptographicHash::Sha256).toHex()).left(16);
    const QString path = QStringLiteral("/music/song-%1-%2.mp3").arg(QTest::currentTestFunction(), id);
    QFile index(analysis.filePath("index.json"));
    if (analysis.mkpath("final") == false || index.open(QIODevice::WriteOnly) == false)
        return nullptr;
    index.write(QStringLiteral(R"({"%1": {"path": "%2"}})").arg(id, path).toUtf8());
    if (index.flush() == false || index.setFileTime(nextFileTime(), QFileDevice::FileModificationTime) == false)
        return nullptr;
    index.close();
    rewriteAnalysis(sections);

    auto *show = new Show(doc);
    show->setName("Song");
    doc->addFunction(show);
    auto *audio = new Audio(doc);
    doc->addFunction(audio);
    audio->setSourceFileName(path);
    auto *track = new Track(Function::invalidId(), show);
    show->addTrack(track);
    track->createShowFunction(audio->id())->setStartTime(2000);
    return show;
}
}

void McpLiveControlTools_Test::songDirectorItemsRefused_data()
{
    QTest::addColumn<QString>("tool");
    QTest::addColumn<QString>("target");  // "director track", "block" (its Collection or item), "scene", "audio item"
    QTest::addColumn<bool>("refused");
    QTest::addColumn<bool>("gone");       // the song's analysis is gone: blocks and track are ordinary content
    QTest::newRow("add a Scene to the Song Director track") << "add_show_items" << "director track" << true << false;
    QTest::newRow("add a block Collection to another track") << "add_show_items" << "block" << true << false;
    QTest::newRow("add a Scene to another track") << "add_show_items" << "scene" << false << false;
    QTest::newRow("delete a block item") << "delete_show_items" << "block" << true << false;
    QTest::newRow("delete the Song Director track") << "delete_show_items" << "director track" << true << false;
    QTest::newRow("delete the audio item") << "delete_show_items" << "audio item" << false << false;
    QTest::newRow("delete a block item once the analysis is gone") << "delete_show_items" << "block" << false << true;
    QTest::newRow("delete the Song Director track once the analysis is gone")
        << "delete_show_items" << "director track" << false << true;
}

void McpLiveControlTools_Test::songDirectorItemsRefused()
{
    QFETCH(QString, tool);
    QFETCH(QString, target);
    QFETCH(bool, refused);
    QFETCH(bool, gone);
    auto *doc = m_app->doc();
    Show *song = analysedSongShow(doc, R"([{"label": "intro", "start": 0.0, "end": 8.0, "confidence": 70},
                                           {"label": "drop", "start": 8.0, "end": 24.0, "confidence": 90}])");
    QVERIFY(song != nullptr);
    const Json created = invokeShowTool(doc, m_vc.get(), "create_song_blocks", {{"showID", song->id()}});
    QVERIFY2(!created.contains("error"), created.dump().c_str());
    Track *director = m_app->findChild<SongDirector *>()->track(song);
    QVERIFY(director != nullptr);
    auto *scene = new Scene(doc);
    QVERIFY(doc->addFunction(scene));
    const auto directorState = [&]() {
        QStringList list;
        for (Track *t : song->tracks())
            for (ShowFunction *sf : t->showFunctions())
                if (t == director || doc->function(sf->functionID())->type() == Function::CollectionType)
                    list << QString("%1:%2@%3+%4").arg(t->name()).arg(sf->functionID()).arg(sf->startTime()).arg(sf->duration());
        return list;
    };
    QTemporaryDir empty;
    SongDirector *songDirector = m_app->findChild<SongDirector *>();
    const auto restore = qScopeGuard([&]() {
        songDirector->setAnalysisDir(analysisDir());
        m_app->showManager()->setAnalysisDir(analysisDir());
    });
    if (gone)
    {
        songDirector->setAnalysisDir(empty.path());
        m_app->showManager()->setAnalysisDir(empty.path());
        QVERIFY(songDirector->sections(song).isEmpty());
    }
    const QStringList before = directorState();
    const int itemsBefore = [&]() { int n = 0; for (Track *t : song->tracks()) n += t->showFunctions().count(); return n; }();

    Json args{{"showID", song->id()}};
    if (tool == "add_show_items")
    {
        args["trackName"] = target == "director track" ? director->name().toStdString() : std::string("Looks");
        args["items"] = Json::array({{{"functionID", target == "block" ? director->showFunctions().first()->functionID() : scene->id()},
                                      {"startTime", 40000}, {"duration", 1000}}});
    }
    else if (target == "director track")
        args["trackNames"] = Json::array({director->name().toStdString()});
    else
        args["itemIDs"] = Json::array({target == "block" ? director->showFunctions().first()->id()
                                                         : song->tracks().first()->showFunctions().first()->id()});
    const Json result = invokeShowTool(doc, m_vc.get(), tool.toUtf8().constData(), args);
    QVERIFY2(result.is_array() && result.size() == 1, result.dump().c_str());
    QCOMPARE(result.at(0).contains("error"), refused);
    if (refused)
        QVERIFY2(QString::fromStdString(result.at(0).at("error").get<std::string>()).contains("Song Director"), result.dump().c_str());

    if (gone)
    {
        if (target == "director track")
            QVERIFY(!song->tracks().contains(director));
        else
            QCOMPARE(director->showFunctions().count(), 1);
        return;
    }
    // D16: the director track and its blocks stay as they are
    QCOMPARE(m_app->findChild<SongDirector *>()->track(song), director);
    QCOMPARE(directorState(), before);
    const int itemsAfter = [&]() { int n = 0; for (Track *t : song->tracks()) n += t->showFunctions().count(); return n; }();
    QCOMPARE(itemsAfter, refused ? itemsBefore : itemsBefore + (tool == "add_show_items" ? 1 : -1));
}

void McpLiveControlTools_Test::songAnalysisFolder_data()
{
    QTest::addColumn<QString>("reader");   // what reads the song's analysis
    QTest::newRow("section band") << QStringLiteral("band");
    QTest::newRow("Create song blocks") << QStringLiteral("button");
    QTest::newRow("create_song_blocks") << QStringLiteral("tool");
    QTest::newRow("automatic blocks") << QStringLiteral("sync");
}

void McpLiveControlTools_Test::songAnalysisFolder()
{
    QFETCH(QString, reader);
    auto *doc = m_app->doc();
    ShowManager *manager = m_app->showManager();
    QVERIFY(QDir(analysisDir()) != QDir(EdmAnalysisReader::defaultDir()));

    // the analysis is in the configured folder only
    Show *show = analysedSongShow(doc, R"([{"label": "intro", "start": 0.0, "end": 9.0, "confidence": 70},
                                           {"label": "drop", "start": 9.0, "end": 25.0, "confidence": 90}])");
    QVERIFY(show != nullptr);
    QVERIFY(QFileInfo::exists(EdmAnalysisReader::defaultDir() + "/index.json") == false);

    if (reader == "band")
    {
        manager->setCurrentShowID(int(show->id()));
        QCOMPARE(manager->analysisSections().count(), 2);
        QCOMPARE(manager->analysisSections().at(1).toMap().value("startMs").toDouble(), 11000.0);
        return;
    }
    if (reader == "button")
    {
        manager->setCurrentShowID(int(show->id()));
        QVERIFY(manager->createSongBlocks());
    }
    else if (reader == "tool")
    {
        const Json created = invokeShowTool(doc, m_vc.get(), "create_song_blocks", {{"showID", show->id()}});
        QVERIFY2(!created.contains("error"), created.dump().c_str());
    }
    else
    {
        // the user's Assign makes it the song's Show (the factory's own index is built earlier in this run)
        auto *factory = m_app->findChild<ShowFactory *>();
        QVERIFY(factory != nullptr);
        factory->registerMapping(ShowFactory::songAudio(doc, show)->getSourceFileName(), show->id());
        m_app->findChild<SongDirector *>()->sync();
    }
    Track *track = m_app->findChild<SongDirector *>()->track(show);
    QVERIFY(track != nullptr);
    QCOMPARE(track->showFunctions().count(), 2);
    QCOMPARE(track->showFunctions().at(1)->startTime(), 11000u);
}

void McpLiveControlTools_Test::songBlocksReadOnly_data()
{
    QTest::addColumn<bool>("hasBlocks");
    QTest::newRow("Show with blocks") << true;
    QTest::newRow("Show without blocks") << false;
}

void McpLiveControlTools_Test::songBlocksReadOnly()
{
    QFETCH(bool, hasBlocks);
    auto *doc = m_app->doc();
    ShowManager *manager = m_app->showManager();

    Show *song = analysedSongShow(doc, R"([{"label": "intro", "start": 0.0, "end": 9.0, "confidence": 70}])");
    QVERIFY(song != nullptr);
    if (hasBlocks)
    {
        const Json created = invokeShowTool(doc, m_vc.get(), "create_song_blocks", {{"showID", song->id()}});
        QVERIFY2(!created.contains("error"), created.dump().c_str());
    }
    auto *other = new Show(doc);
    QVERIFY(doc->addFunction(other));
    manager->setCurrentShowID(int(other->id()));
    manager->setReadOnly(true);
    auto writable = qScopeGuard([manager]() { manager->setReadOnly(false); });

    const Json created = invokeShowTool(doc, m_vc.get(), "create_song_blocks", {{"showID", song->id()}});

    QVERIFY2(created.contains("error"), created.dump().c_str());
    QVERIFY2(QString::fromStdString(created.at("error").get<std::string>()).contains("read-only"), created.dump().c_str());
    QCOMPARE(manager->currentShowID(), int(other->id()));
    QCOMPARE(m_app->findChild<SongDirector *>()->track(song) != nullptr, hasBlocks);
}

void McpLiveControlTools_Test::songBlockRemovedCleanly_data()
{
    QTest::addColumn<QString>("use");   // what the user did with the drop block before a re-analysis removed it
    QTest::newRow("selected in the Show Manager") << QStringLiteral("selected");
    QTest::newRow("copied to the clipboard") << QStringLiteral("copied");
    QTest::newRow("open in the Function editor") << QStringLiteral("editor");
}

void McpLiveControlTools_Test::songBlockRemovedCleanly()
{
    QFETCH(QString, use);
    auto *doc = m_app->doc();
    ShowManager *manager = m_app->showManager();
    FunctionManager *functions = m_app->functionManager();
    auto *director = m_app->findChild<SongDirector *>();

    Show *show = analysedSongShow(doc, R"([{"label": "intro", "start": 0.0, "end": 9.0, "confidence": 70},
                                           {"label": "drop", "start": 9.0, "end": 25.0, "confidence": 90}])");
    QVERIFY(show != nullptr);
    const Json created = invokeShowTool(doc, m_vc.get(), "create_song_blocks", {{"showID", show->id()}});
    QVERIFY2(!created.contains("error"), created.dump().c_str());
    Track *track = director->track(show);
    QVERIFY(track != nullptr);
    QCOMPARE(manager->currentShowID(), int(show->id()));
    ShowFunction *drop = track->showFunctions().at(1);
    const quint32 dropCollection = drop->functionID();
    const int trackIdx = int(show->tracks().indexOf(track));
    manager->resetItemsSelection();
    manager->copyToClipboard();   // empties it

    if (use == "selected" || use == "copied")
        manager->setItemSelection(trackIdx, drop, nullptr, true, 0);
    if (use == "copied")
    {
        manager->copyToClipboard();
        manager->resetItemsSelection();
        QCOMPARE(manager->clipboardItemsCount(), 1);
    }
    if (use == "editor")
    {
        functions->setEditorFunction(dropCollection, false, false);
        QVERIFY(functions->currentEditor() != nullptr);
    }

    rewriteAnalysis(R"([{"label": "intro", "start": 0.0, "end": 25.0, "confidence": 70}])");
    director->sync();

    QVERIFY(doc->function(dropCollection) == nullptr);
    QCOMPARE(track->showFunctions().count(), 1);
    QCOMPARE(manager->selectedItemsCount(), 0);
    QCOMPARE(manager->clipboardItemsCount(), 0);
    QVERIFY(functions->currentEditor() == nullptr);
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
    const quint32 id = reason == "VC button"
        ? (QTest::currentDataTag() == QByteArray("missing-widget") ? 2147483647u : vc->page(0)->id())
        : button->id();
    if (QTest::currentDataTag() == QByteArray("wrong-widget-type"))
        QVERIFY(id > quint32(std::numeric_limits<int>::max()));
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

void McpLiveControlTools_Test::songSectionEdits_data()
{
    QTest::addColumn<QByteArray>("edits");     // edit_song_sections arguments without showID, one after the other
    QTest::addColumn<QStringList>("expected"); // "label start-end", * when edited, in Show time (clip at 2 s)
    QTest::addColumn<QString>("error");        // of the last edit

    const QStringList base { "intro 2000-10000", "drop 10000-26000" };
    QTest::newRow("move_boundary, snapped to a bar") << QByteArray(R"([{"action":"move_boundary","section":0,"timeMs":13100}])")
        << QStringList { "intro* 2000-14000", "drop* 14000-26000" } << QString();
    QTest::newRow("relabel") << QByteArray(R"([{"action":"relabel","section":1,"label":"breakdown"}])")
        << QStringList { "intro 2000-10000", "breakdown* 10000-26000" } << QString();
    QTest::newRow("split") << QByteArray(R"([{"action":"split","section":1,"timeMs":18300}])")
        << QStringList { "intro 2000-10000", "drop* 10000-18000", "drop* 18000-26000" } << QString();
    QTest::newRow("merge") << QByteArray(R"([{"action":"merge","section":0}])") << QStringList { "intro* 2000-26000" } << QString();
    QTest::newRow("reset") << QByteArray(R"([{"action":"merge","section":0},{"action":"reset","section":0}])") << base << QString();
    QTest::newRow("reset_all") << QByteArray(R"([{"action":"relabel","section":0,"label":"breakdown"},{"action":"split","section":1,"timeMs":18000},{"action":"reset_all"}])")
        << base << QString();
    QTest::newRow("unknown action") << QByteArray(R"([{"action":"jump","section":0}])") << base << QStringLiteral("unknown section action");
    QTest::newRow("move_boundary without timeMs") << QByteArray(R"([{"action":"move_boundary","section":0}])") << base << QStringLiteral("timeMs");
    QTest::newRow("relabel without label") << QByteArray(R"([{"action":"relabel","section":0}])") << base << QStringLiteral("label");
    QTest::newRow("relabel to an unknown label") << QByteArray(R"([{"action":"relabel","section":0,"label":"chorus"}])") << base
        << QStringLiteral("unknown label");
    // D20: the analysis tool's five labels only
    QTest::newRow("relabel to groove") << QByteArray(R"([{"action":"relabel","section":0,"label":"groove"}])") << base
        << QStringLiteral("unknown label");
    QTest::newRow("merge without section") << QByteArray(R"([{"action":"merge"}])") << base << QStringLiteral("section");
    QTest::newRow("section out of range") << QByteArray(R"([{"action":"merge","section":5}])") << base << QStringLiteral("no section 5");
    QTest::newRow("negative section") << QByteArray(R"([{"action":"merge","section":-1}])") << base << QStringLiteral("section");
    QTest::newRow("boundary outside the sections") << QByteArray(R"([{"action":"move_boundary","section":0,"timeMs":40000}])") << base
        << QStringLiteral("empty");
    QTest::newRow("unknown field") << QByteArray(R"([{"action":"merge","section":0,"extra":1}])") << base << QStringLiteral("unknown fields");
    QTest::newRow("section past the int range") << QByteArray(R"([{"action":"merge","section":2147483648}])") << base
        << QStringLiteral("no section 2147483648");
    QTest::newRow("label on split") << QByteArray(R"([{"action":"split","section":1,"timeMs":18000,"label":"drop"}])") << base
        << QStringLiteral("split takes no label");
    QTest::newRow("timeMs on merge") << QByteArray(R"([{"action":"merge","section":0,"timeMs":1000}])") << base
        << QStringLiteral("merge takes no timeMs");
    QTest::newRow("section on reset_all") << QByteArray(R"([{"action":"reset_all","section":0}])") << base
        << QStringLiteral("reset_all takes no section");
}

void McpLiveControlTools_Test::songSectionEditsReadOnly()
{
    auto *doc = m_app->doc();
    ShowManager *manager = m_app->showManager();
    Show *song = analysedSongShow(doc, R"([{"label": "intro", "start": 0.0, "end": 8.0, "confidence": 70},
                                           {"label": "drop", "start": 8.0, "end": 24.0, "confidence": 90}])");
    QVERIFY(song != nullptr);
    auto *other = new Show(doc);
    QVERIFY(doc->addFunction(other));
    manager->setCurrentShowID(int(other->id()));
    manager->setReadOnly(true);
    auto writable = qScopeGuard([manager]() { manager->setReadOnly(false); });

    const Json result = invokeShowTool(doc, m_vc.get(), "edit_song_sections",
                                       {{"showID", song->id()}, {"action", "relabel"}, {"section", 0}, {"label", "breakdown"}});

    QVERIFY2(result.contains("error"), result.dump().c_str());
    QVERIFY2(QString::fromStdString(result.at("error").get<std::string>()).contains("read-only"), result.dump().c_str());
    QCOMPARE(manager->currentShowID(), int(other->id()));
    QVERIFY(m_app->findChild<SongDirector *>()->sectionEdits(song).isEmpty());
}

void McpLiveControlTools_Test::songSectionEdits()
{
    QFETCH(QByteArray, edits);
    QFETCH(QStringList, expected);
    QFETCH(QString, error);
    auto *doc = m_app->doc();
    ShowManager *manager = m_app->showManager();

    Show *song = analysedSongShow(doc, R"([{"label": "intro", "start": 0.0, "end": 8.0, "confidence": 70},
                                           {"label": "drop", "start": 8.0, "end": 24.0, "confidence": 90}])");
    QVERIFY(song != nullptr);
    const Json created = invokeShowTool(doc, m_vc.get(), "create_song_blocks", {{"showID", song->id()}});
    QVERIFY2(!created.contains("error"), created.dump().c_str());

    Json result;
    for (Json args : Json::parse(edits.constData()))
    {
        args["showID"] = song->id();
        result = invokeShowTool(doc, m_vc.get(), "edit_song_sections", args);
    }
    if (error.isEmpty())
        QVERIFY2(!result.contains("error"), result.dump().c_str());
    else
    {
        QVERIFY2(result.contains("error"), result.dump().c_str());
        QVERIFY2(QString::fromStdString(result.at("error").get<std::string>()).contains(error), result.dump().c_str());
    }

    const Json queried = invokeShowTool(doc, m_vc.get(), "query_song_sections", {{"showID", song->id()}});
    if (error.isEmpty())
        QCOMPARE(result, queried);
    QStringList sections;
    for (const Json &s : queried.at("sections"))
        sections << QStringLiteral("%1%2 %3-%4").arg(QString::fromStdString(s.at("label").get<std::string>()),
                                                     s.at("edited").get<bool>() ? "*" : "")
                        .arg(s.at("startMs").get<double>()).arg(s.at("endMs").get<double>());
    QCOMPARE(sections, expected);

    // the Show Manager's section band and the blocks show the same sections
    QCOMPARE(manager->currentShowID(), int(song->id()));
    QStringList band;
    for (const QVariant &v : manager->analysisSections())
    {
        const QVariantMap s = v.toMap();
        band << QStringLiteral("%1%2 %3-%4").arg(s.value("label").toString(), s.value("edited").toBool() ? "*" : "")
                    .arg(s.value("startMs").toDouble()).arg(s.value("endMs").toDouble());
    }
    QCOMPARE(band, expected);
    QStringList blocks;
    const Json songBlocks = invokeShowTool(doc, m_vc.get(), "query_song_blocks", {{"showID", song->id()}});
    for (const Json &b : songBlocks.at("blocks"))
        blocks << QStringLiteral("%1 %2-%3").arg(QString::fromStdString(b.at("label").get<std::string>()))
                      .arg(b.at("startMs").get<int>()).arg(b.at("startMs").get<int>() + b.at("durationMs").get<int>());
    std::sort(blocks.begin(), blocks.end(), [](const QString &a, const QString &b) {
        return a.section(' ', 1).section('-', 0, 0).toInt() < b.section(' ', 1).section('-', 0, 0).toInt();
    });
    QStringList unstarred = expected;
    unstarred.replaceInStrings("*", "");
    QCOMPARE(blocks, unstarred);
}

QTEST_MAIN(McpLiveControlTools_Test)
