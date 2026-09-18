/*
  Q Light Controller Plus - Unit test
  vdjbridge_test.cpp
*/

#include <QtTest>
#include <QSignalSpy>
#include <QDir>
#include <QSettings>

// Access private members for test setup
#define private public
#include "showfactory.h"
#include "mastertimer.h"
#include "inputoutputmap.h"
#undef private

#include "vdjbridge_test.h"
#include "vdjbridge.h"
#include "djfsm.h"
#include "../../../plugins/vdjbridge/vdjbridgeplugin.h"
#include "../../../plugins/vdjbridge/configurevdjbridge.h"

#include "doc.h"
#include "show.h"
#include "scene.h"
#include "track.h"
#include "showfunction.h"
#include "function.h"
#include "inputoutputmap.h"
#include <QTableWidget>

void VdjBridge_Test::initTestCase()
{
    QCoreApplication::setOrganizationName("qlcplus-test");
    QCoreApplication::setApplicationName("vdjbridge_test");
    QSettings::setDefaultFormat(QSettings::IniFormat);

    const QString settingsRoot = QDir::cleanPath(QCoreApplication::applicationDirPath()
                                                 + "/vdjbridge_test_settings");
    QDir().mkpath(settingsRoot);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsRoot);

    QSettings isolated(QSettings::IniFormat, QSettings::UserScope,
                       QCoreApplication::organizationName(),
                       QCoreApplication::applicationName());
    isolated.clear();
}

void VdjBridge_Test::cleanupTestCase()
{
    QSettings isolated(QSettings::IniFormat, QSettings::UserScope,
                       QCoreApplication::organizationName(),
                       QCoreApplication::applicationName());
    isolated.clear();
}

void VdjBridge_Test::initialState()
{
    VdjBridge b;
    QCOMPARE(b.connected(), false);
    QCOMPARE(b.beatCount(), 0);
}

void VdjBridge_Test::beatTicksCounterAndConnected()
{
    VdjBridge b;
    QSignalSpy connectedSpy(&b, &VdjBridge::connectedChanged);
    QSignalSpy beatSpy(&b, &VdjBridge::beatReceived);

    b.onBeat();

    QCOMPARE(b.connected(), true);
    QCOMPARE(b.beatCount(), 1);
    QCOMPARE(connectedSpy.count(), 1);
    QCOMPARE(beatSpy.count(), 1);

    b.onBeat();
    QCOMPARE(b.beatCount(), 2);
    // connected stays true — no extra connectedChanged emission
    QCOMPARE(connectedSpy.count(), 1);
}

// --- Helper: create a Doc + Show with enough duration for testing ---

struct TestFixture {
    Doc *doc;
    VdjBridge *bridge;
    Show *show;
    Scene *scene;

    TestFixture()
    {
        doc = new Doc(nullptr, 4);
        bridge = new VdjBridge();
        bridge->setDoc(doc);
        // Auto-start/sync is gated behind Perform mode; enable it for the
        // auto-play tests below.
        bridge->setPerformMode(true);

        // Create a Show with External sync and 5-minute duration
        show = new Show(doc);
        show->setName("Test Artist - Test Song");
        show->setSyncSource(1); // External
        doc->addFunction(show);

        scene = new Scene(doc);
        doc->addFunction(scene);

        Track *track = new Track(Function::invalidId(), show);
        track->setName("Audio");
        show->addTrack(track);

        ShowFunction *sf = track->createShowFunction(scene->id());
        sf->setStartTime(0);
        sf->setDuration(300000); // 5 minutes — won't auto-stop

        // Inject filepath→showId into ShowFactory's private mapping
        ShowFactory *factory = bridge->showFactory();
        factory->m_filepathToShowId.insert("/music/test.mp3", show->id());
        factory->m_createdShows.insert("/music/test.mp3");

        // Set deck 0's filepath via the bridge (onDeckTrigger is a private slot)
        QMetaObject::invokeMethod(bridge, "onDeckTrigger",
            Q_ARG(int, 0),
            Q_ARG(QString, "get_filepath"),
            Q_ARG(QVariant, QVariant("/music/test.mp3")));

        // Make deck 1 the active/master deck so Perform-mode auto-play (which
        // is gated on the FSM's active deck) targets it.
        QMetaObject::invokeMethod(bridge, "onGlobalTrigger",
            Q_ARG(QString, "masterdeck"),
            Q_ARG(QVariant, QVariant("on"))); // on => deck 1
    }

    ~TestFixture()
    {
        doc->masterTimer()->stop();
        delete bridge;
        delete doc;
    }

    void startMasterTimer()
    {
        doc->masterTimer()->start();
    }

    void processTick()
    {
        // Let MasterTimer thread process the start queue
        QTest::qWait(50);
    }

    void sendPlay(bool on)
    {
        QMetaObject::invokeMethod(bridge, "onDeckTrigger",
            Q_ARG(int, 0),
            Q_ARG(QString, "play"),
            Q_ARG(QVariant, QVariant(on ? "on" : "off")));
    }
};

struct PerformShowFixture
{
    Show *show = nullptr;
    Track *mixedTrack = nullptr;
    Track *mutedTrack = nullptr;
    Function *audioNow = nullptr;
    Function *audioFuture = nullptr;
    Scene *sceneNow = nullptr;
    Scene *sceneFuture = nullptr;
};

static PerformShowFixture createPerformShowFixture(Doc *doc, const QString &name, bool withMutedTrack)
{
    PerformShowFixture fixture;
    fixture.show = new Show(doc);
    fixture.show->setName(name);
    doc->addFunction(fixture.show);

    fixture.audioNow = new Function(doc, Function::AudioType);
    fixture.audioNow->setName(name + " Audio Now");
    doc->addFunction(fixture.audioNow);

    fixture.audioFuture = new Function(doc, Function::AudioType);
    fixture.audioFuture->setName(name + " Audio Future");
    doc->addFunction(fixture.audioFuture);

    fixture.sceneNow = new Scene(doc);
    fixture.sceneNow->setName(name + " Scene Now");
    fixture.sceneNow->setDuration(5000);
    doc->addFunction(fixture.sceneNow);

    fixture.sceneFuture = new Scene(doc);
    fixture.sceneFuture->setName(name + " Scene Future");
    fixture.sceneFuture->setDuration(3000);
    doc->addFunction(fixture.sceneFuture);

    fixture.mixedTrack = new Track(Function::invalidId(), fixture.show);
    fixture.mixedTrack->setName("Mixed");
    fixture.show->addTrack(fixture.mixedTrack);

    ShowFunction *audioNowItem = fixture.mixedTrack->createShowFunction(fixture.audioNow->id());
    audioNowItem->setStartTime(0);
    audioNowItem->setDuration(5000);

    ShowFunction *sceneNowItem = fixture.mixedTrack->createShowFunction(fixture.sceneNow->id());
    sceneNowItem->setStartTime(0);
    sceneNowItem->setDuration(5000);

    ShowFunction *audioFutureItem = fixture.mixedTrack->createShowFunction(fixture.audioFuture->id());
    audioFutureItem->setStartTime(1800);
    audioFutureItem->setDuration(3000);

    ShowFunction *sceneFutureItem = fixture.mixedTrack->createShowFunction(fixture.sceneFuture->id());
    sceneFutureItem->setStartTime(1800);
    sceneFutureItem->setDuration(3000);

    if (withMutedTrack)
    {
        fixture.mutedTrack = new Track(Function::invalidId(), fixture.show);
        fixture.mutedTrack->setName("Muted");
        fixture.mutedTrack->setMute(true);
        fixture.show->addTrack(fixture.mutedTrack);
    }

    return fixture;
}

// --- C3: When VDJ deck play=true, auto-start the show ---

void VdjBridge_Test::autoStartShowOnPlay()
{
    TestFixture f;
    f.startMasterTimer();

    QVERIFY(!f.show->isRunning());

    f.sendPlay(true);
    f.processTick();

    QVERIFY(f.show->isRunning());
    QVERIFY(!f.show->isPaused());
    QCOMPARE(f.show->syncSource(), 1); // External
}

// --- C5: When VDJ deck play=false while running, auto-pause ---

void VdjBridge_Test::autoPauseShowOnPlayOff()
{
    TestFixture f;
    f.startMasterTimer();

    // Start the show
    f.sendPlay(true);
    f.processTick();
    QVERIFY(f.show->isRunning());

    // Pause it
    f.sendPlay(false);

    QVERIFY(f.show->isRunning());  // still running, just paused
    QVERIFY(f.show->isPaused());
}

// --- C6: When VDJ deck play=true while paused, auto-resume ---

void VdjBridge_Test::autoResumeShowOnPlayOn()
{
    TestFixture f;
    f.startMasterTimer();

    // Start the show
    f.sendPlay(true);
    f.processTick();
    QVERIFY(f.show->isRunning());

    // Pause it
    f.sendPlay(false);
    QVERIFY(f.show->isPaused());

    // Resume it
    f.sendPlay(true);

    QVERIFY(f.show->isRunning());
    QVERIFY(!f.show->isPaused());
}

// --- Full cycle: start → pause → resume (no restart) ---

void VdjBridge_Test::autoStartPauseResumeCycle()
{
    TestFixture f;
    f.startMasterTimer();

    // Initially not running
    QVERIFY(!f.show->isRunning());
    QVERIFY(!f.show->isPaused());

    // 1. Play → auto-start
    f.sendPlay(true);
    f.processTick();
    QVERIFY(f.show->isRunning());
    QVERIFY(!f.show->isPaused());

    // 2. Stop → auto-pause (not stop)
    f.sendPlay(false);
    QVERIFY(f.show->isRunning());
    QVERIFY(f.show->isPaused());

    // 3. Play again → auto-resume (not restart)
    f.sendPlay(true);
    QVERIFY(f.show->isRunning());
    QVERIFY(!f.show->isPaused());

    // 4. Stop again → paused again
    f.sendPlay(false);
    QVERIFY(f.show->isRunning());
    QVERIFY(f.show->isPaused());

    // 5. Play once more → resumes smoothly
    f.sendPlay(true);
    QVERIFY(f.show->isRunning());
    QVERIFY(!f.show->isPaused());
}

// --- Perform adopts the show's sync source and restores it on release.
//     This covers the reloaded-workspace case: shows are Autonomous after a
//     load (sync source is not persisted), and manual playback must keep
//     working once Perform is off. ---

void VdjBridge_Test::performAdoptsAndReleasesSyncSource()
{
    Doc *doc = new Doc(nullptr, 4);
    VdjBridge bridge;
    bridge.setDoc(doc);

    // a plain show as it comes out of a workspace load: Autonomous sync
    Show *show = new Show(doc);
    show->setName("Adopt Me");
    doc->addFunction(show);
    Scene *scene = new Scene(doc);
    doc->addFunction(scene);
    Track *track = new Track(Function::invalidId(), show);
    show->addTrack(track);
    ShowFunction *sf = track->createShowFunction(scene->id());
    sf->setStartTime(0);
    sf->setDuration(300000);
    QCOMPARE(show->syncSource(), 0); // Autonomous

    // mapping restored (as DjManager does after rebuildFromDoc)
    bridge.showFactory()->registerMapping("/music/adopt.mp3", show->id());

    auto deckTrigger = [&bridge](const QString &t, const QVariant &v) {
        QMetaObject::invokeMethod(&bridge, "onDeckTrigger",
            Q_ARG(int, 0), Q_ARG(QString, t), Q_ARG(QVariant, v));
    };
    deckTrigger("get_filepath", QVariant("/music/adopt.mp3"));
    QMetaObject::invokeMethod(&bridge, "onGlobalTrigger",
        Q_ARG(QString, "masterdeck"), Q_ARG(QVariant, QVariant("on")));

    doc->masterTimer()->start();

    // enabling Perform adopts the show: External sync while performing
    bridge.setPerformMode(true);
    QCOMPARE(bridge.performFsm()->state(), PerformFsm::PerformState::Suspended);
    QCOMPARE(show->syncSource(), 1);

    deckTrigger("play", QVariant("on"));
    QTest::qWait(50);
    QCOMPARE(bridge.performFsm()->state(), PerformFsm::PerformState::Live);
    QVERIFY(show->isRunning());

    // Perform off: released — paused and the original sync source restored,
    // so manual playback advances normally again
    bridge.setPerformMode(false);
    QCOMPARE(bridge.performFsm()->state(), PerformFsm::PerformState::Idle);
    QVERIFY(show->isPaused());
    QCOMPARE(show->syncSource(), 0);

    doc->masterTimer()->stop();
    delete doc;
}

void VdjBridge_Test::performAdoptionSuppressesAudioForAlreadyRunningShow()
{
    Doc *doc = new Doc(nullptr, 4);
    VdjBridge bridge;
    bridge.setDoc(doc);

    PerformShowFixture fixture = createPerformShowFixture(doc, "Adopt Running", false);
    bridge.showFactory()->registerMapping("/music/adopt-running.mp3", fixture.show->id());

    auto deckTrigger = [&bridge](int idx, const QString &t, const QVariant &v) {
        QMetaObject::invokeMethod(&bridge, "onDeckTrigger",
            Q_ARG(int, idx), Q_ARG(QString, t), Q_ARG(QVariant, v));
    };
    deckTrigger(0, "get_filepath", QVariant("/music/adopt-running.mp3"));
    deckTrigger(0, "play", QVariant("on"));
    QMetaObject::invokeMethod(&bridge, "onGlobalTrigger",
        Q_ARG(QString, "masterdeck"), Q_ARG(QVariant, QVariant("on")));

    QSignalSpy audioStoppedSpy(fixture.audioNow, QOverload<quint32>::of(&Function::stopped));
    QSignalSpy sceneRunningSpy(fixture.sceneNow, QOverload<quint32>::of(&Function::running));

    doc->masterTimer()->start();
    fixture.show->start(doc->masterTimer(), FunctionParent::master());
    QTest::qWait(80);

    QVERIFY(fixture.show->isRunning());
    QVERIFY(fixture.audioNow->isRunning());
    QVERIFY(sceneRunningSpy.count() > 0);

    bridge.setPerformMode(true);
    QCOMPARE(bridge.performFsm()->state(), PerformFsm::PerformState::Live);

    bool audioSuppressed = false;
    for (int i = 0; i < 20 && !audioSuppressed; ++i)
    {
        QTest::qWait(20);
        audioSuppressed = audioStoppedSpy.count() > 0 && !fixture.audioNow->isRunning();
    }

    QVERIFY2(audioSuppressed, "Perform adoption must stop the running Audio function.");
    QVERIFY(sceneRunningSpy.count() > 0);
    QVERIFY(fixture.show->isRunning());

    bridge.setPerformMode(false);
    doc->masterTimer()->stop();
    delete doc;
}

void VdjBridge_Test::performReleaseOrSwitchClearsTransientAudioSuppression_data()
{
    QTest::addColumn<bool>("switchDeck");
    QTest::newRow("release") << false;
    QTest::newRow("switch") << true;
}

void VdjBridge_Test::performReleaseOrSwitchClearsTransientAudioSuppression()
{
    QFETCH(bool, switchDeck);

    Doc *doc = new Doc(nullptr, 4);
    VdjBridge bridge;
    bridge.setDoc(doc);

    PerformShowFixture showA = createPerformShowFixture(doc, "Perform A", true);
    PerformShowFixture showB = createPerformShowFixture(doc, "Perform B", true);
    QSignalSpy showASceneNowRunningSpy(showA.sceneNow, QOverload<quint32>::of(&Function::running));
    QSignalSpy showASceneFutureRunningSpy(showA.sceneFuture, QOverload<quint32>::of(&Function::running));
    QSignalSpy showBSceneNowRunningSpy(showB.sceneNow, QOverload<quint32>::of(&Function::running));
    bridge.showFactory()->registerMapping("/music/perform-a.mp3", showA.show->id());
    bridge.showFactory()->registerMapping("/music/perform-b.mp3", showB.show->id());

    auto deckTrigger = [&bridge](int idx, const QString &t, const QVariant &v) {
        QMetaObject::invokeMethod(&bridge, "onDeckTrigger",
            Q_ARG(int, idx), Q_ARG(QString, t), Q_ARG(QVariant, v));
    };
    deckTrigger(0, "get_filepath", QVariant("/music/perform-a.mp3"));
    deckTrigger(1, "get_filepath", QVariant("/music/perform-b.mp3"));
    deckTrigger(0, "play", QVariant("on"));
    deckTrigger(1, "play", QVariant("on"));
    QMetaObject::invokeMethod(&bridge, "onGlobalTrigger",
        Q_ARG(QString, "masterdeck"), Q_ARG(QVariant, QVariant("on")));

    doc->masterTimer()->start();
    showA.show->start(doc->masterTimer(), FunctionParent::master());
    QTest::qWait(120);
    QVERIFY(showA.audioNow->isRunning());
    QVERIFY(showASceneNowRunningSpy.count() > 0);
    QVERIFY(!showA.audioFuture->isRunning());
    QCOMPARE(showASceneFutureRunningSpy.count(), 0);

    bridge.setPerformMode(true);
    QCOMPARE(bridge.performFsm()->state(), PerformFsm::PerformState::Live);
    deckTrigger(0, "get_time elapsed absolute", QVariant(1200.0));
    QTest::qWait(60);
    QVERIFY(showA.show->performAudioSuppressed());
    QVERIFY(!showA.audioNow->isRunning());
    QVERIFY(showASceneNowRunningSpy.count() > 0);
    QVERIFY(!showA.audioFuture->isRunning());
    QCOMPARE(showASceneFutureRunningSpy.count(), 0);

    if (switchDeck)
    {
        QMetaObject::invokeMethod(&bridge, "onGlobalTrigger",
            Q_ARG(QString, "masterdeck"), Q_ARG(QVariant, QVariant("off")));
        QTest::qWait(120);
        QVERIFY(!showA.show->performAudioSuppressed());
        QVERIFY(showA.show->isPaused());
        QVERIFY(showB.show->performAudioSuppressed());
        QVERIFY(!showB.audioNow->isRunning());
        QVERIFY(showBSceneNowRunningSpy.count() > 0);

        showA.show->setPause(false);
        bool activeResumed = false;
        for (int i = 0; i < 40 && !activeResumed; ++i)
        {
            QTest::qWait(25);
            activeResumed = showA.audioNow->isRunning();
        }
        QVERIFY2(activeResumed, "Audio active at suppression time must resume after local unpause.");
        QVERIFY(showASceneNowRunningSpy.count() > 0);

        bool futureAudioStarted = false;
        bool futureSceneStarted = showASceneFutureRunningSpy.count() > 0;
        for (int i = 0; i < 30 && !(futureAudioStarted && futureSceneStarted); ++i)
        {
            QTest::qWait(50);
            futureAudioStarted = futureAudioStarted || showA.audioFuture->isRunning();
            futureSceneStarted = futureSceneStarted || showASceneFutureRunningSpy.count() > 0;
        }
        QVERIFY2(futureAudioStarted, "Future Audio must still start when due after suppression clears.");
        QVERIFY2(futureSceneStarted, "Future Scene on mixed track must still start when due.");

        bridge.setPerformMode(false);
        QTest::qWait(60);
        QVERIFY(!showB.show->performAudioSuppressed());
    }
    else
    {
        bridge.setPerformMode(false);
        QCOMPARE(bridge.performFsm()->state(), PerformFsm::PerformState::Idle);
        QVERIFY(showA.show->isPaused());
        QTest::qWait(40);
        QVERIFY(!showA.show->performAudioSuppressed());
        QVERIFY(!showB.show->performAudioSuppressed());

        showA.show->setPause(false);
        bool activeResumed = false;
        for (int i = 0; i < 40 && !activeResumed; ++i)
        {
            QTest::qWait(25);
            activeResumed = showA.audioNow->isRunning();
        }
        QVERIFY2(activeResumed, "Audio active at suppression time must resume after Perform release.");
        QVERIFY(showASceneNowRunningSpy.count() > 0);

        bool futureAudioStarted = false;
        bool futureSceneStarted = showASceneFutureRunningSpy.count() > 0;
        for (int i = 0; i < 30 && !(futureAudioStarted && futureSceneStarted); ++i)
        {
            QTest::qWait(50);
            futureAudioStarted = futureAudioStarted || showA.audioFuture->isRunning();
            futureSceneStarted = futureSceneStarted || showASceneFutureRunningSpy.count() > 0;
        }
        QVERIFY2(futureAudioStarted, "Suppressed future Audio must not be skipped permanently.");
        QVERIFY2(futureSceneStarted, "Future Scene on mixed track must remain scheduled.");
    }

    QCOMPARE(showA.mixedTrack->isMute(), false);
    QCOMPARE(showA.mutedTrack->isMute(), true);
    QCOMPARE(showB.mixedTrack->isMute(), false);
    QCOMPARE(showB.mutedTrack->isMute(), true);
    doc->masterTimer()->stop();
    delete doc;
}

void VdjBridge_Test::debugTableTracksLatestValuesAndCounts()
{
    VdjBridgePlugin plugin;
    ConfigureVdjBridge dialog(&plugin);
    QTableWidget *table = dialog.findChild<QTableWidget*>(QStringLiteral("debugTable"));

    QVERIFY(table != nullptr);

    QMetaObject::invokeMethod(&dialog, "slotDeckTrigger",
        Q_ARG(int, 0),
        Q_ARG(QString, QStringLiteral("get_title")),
        Q_ARG(QVariant, QVariant(QStringLiteral("Track A"))));
    QCOMPARE(table->rowCount(), 1);
    QCOMPARE(table->item(0, 2)->text(), QStringLiteral("Track A"));
    QCOMPARE(table->item(0, 3)->text(), QStringLiteral("1"));
    QCOMPARE(table->item(0, 4)->text(), QStringLiteral("1"));

    QMetaObject::invokeMethod(&dialog, "slotDeckTrigger",
        Q_ARG(int, 0),
        Q_ARG(QString, QStringLiteral("get_title")),
        Q_ARG(QVariant, QVariant(QStringLiteral("Track A"))));
    QCOMPARE(table->rowCount(), 1);
    QCOMPARE(table->item(0, 2)->text(), QStringLiteral("Track A"));
    QCOMPARE(table->item(0, 3)->text(), QStringLiteral("2"));
    QCOMPARE(table->item(0, 4)->text(), QStringLiteral("2"));

    QMetaObject::invokeMethod(&dialog, "slotDeckTrigger",
        Q_ARG(int, 0),
        Q_ARG(QString, QStringLiteral("get_title")),
        Q_ARG(QVariant, QVariant(QStringLiteral("Track B"))));
    QCOMPARE(table->rowCount(), 1);
    QCOMPARE(table->item(0, 2)->text(), QStringLiteral("Track B"));
    QCOMPARE(table->item(0, 3)->text(), QStringLiteral("3"));
    QCOMPARE(table->item(0, 4)->text(), QStringLiteral("1"));

    QMetaObject::invokeMethod(&dialog, "slotGlobalTrigger",
        Q_ARG(QString, QString()),
        Q_ARG(QVariant, QVariant()));
    QCOMPARE(table->rowCount(), 2);
    QCOMPARE(table->item(1, 1)->text(), QStringLiteral("<empty>"));
    QCOMPARE(table->item(1, 2)->text(), QStringLiteral("<empty>"));

    QMetaObject::invokeMethod(&dialog, "slotClearLog");
    QCOMPARE(table->rowCount(), 0);
}

void VdjBridge_Test::engineBpmFollowsVdjAndIgnoresJitter()
{
    Doc doc(nullptr, 4);
    // The VDJ plugin acts as the beat generator in the real app.
    doc.inputOutputMap()->setBeatGeneratorType(InputOutputMap::Plugin);

    VdjBridge bridge;
    bridge.setDoc(&doc);

    // Make deck 1 the active/master deck.
    QMetaObject::invokeMethod(&bridge, "onGlobalTrigger",
        Q_ARG(QString, "masterdeck"), Q_ARG(QVariant, QVariant("on")));

    auto deckTrigger = [&](const QString &t, const QVariant &v) {
        QMetaObject::invokeMethod(&bridge, "onDeckTrigger",
            Q_ARG(int, 0), Q_ARG(QString, t), Q_ARG(QVariant, v));
    };

    // A song with a steady BPM of 128 on a PLAYING deck → the engine BPM
    // follows it. (BPM only tracks the song tempo while the deck is playing.)
    deckTrigger("get_filepath", QVariant("/m/a.mp3"));
    deckTrigger("get_title", QVariant("Song A"));
    deckTrigger("get_artist", QVariant("Artist A"));
    deckTrigger("get_bpm", QVariant(128.0));
    deckTrigger("play", QVariant("on"));
    QCOMPARE(doc.inputOutputMap()->bpmNumber(), 128);

    // A jittered beat pulse must NOT change the BPM (external lock active).
    QMetaObject::invokeMethod(doc.inputOutputMap(), "slotPluginBeat",
        Q_ARG(quint32, 0u), Q_ARG(quint32, 8341u), Q_ARG(uchar, uchar(255)),
        Q_ARG(QString, QStringLiteral("beat")));
    QCOMPARE(doc.inputOutputMap()->bpmNumber(), 128);

    // A genuinely new steady BPM is followed.
    deckTrigger("get_bpm", QVariant(124.0));
    QCOMPARE(doc.inputOutputMap()->bpmNumber(), 124);
}

void VdjBridge_Test::engineBpmDropsLowWhenPaused()
{
    Doc doc(nullptr, 4);
    doc.inputOutputMap()->setBeatGeneratorType(InputOutputMap::Plugin);

    VdjBridge bridge;
    bridge.setDoc(&doc);
    bridge.setPausedBpm(1); // explicit (also the default)

    QMetaObject::invokeMethod(&bridge, "onGlobalTrigger",
        Q_ARG(QString, "masterdeck"), Q_ARG(QVariant, QVariant("on")));
    auto deckTrigger = [&](const QString &t, const QVariant &v) {
        QMetaObject::invokeMethod(&bridge, "onDeckTrigger",
            Q_ARG(int, 0), Q_ARG(QString, t), Q_ARG(QVariant, v));
    };

    deckTrigger("get_filepath", QVariant("/m/a.mp3"));
    deckTrigger("get_title", QVariant("Song A"));
    deckTrigger("get_artist", QVariant("Artist A"));
    deckTrigger("get_bpm", QVariant(128.0));

    // Playing → steady song BPM.
    deckTrigger("play", QVariant("on"));
    QCOMPARE(doc.inputOutputMap()->bpmNumber(), 128);

    // Paused → BPM drops to the configured low value.
    deckTrigger("play", QVariant("off"));
    QCOMPARE(doc.inputOutputMap()->bpmNumber(), 1);

    // Resumed → back to the steady song BPM.
    deckTrigger("play", QVariant("on"));
    QCOMPARE(doc.inputOutputMap()->bpmNumber(), 128);

    // The paused value is a parameter.
    bridge.setPausedBpm(20);
    deckTrigger("play", QVariant("off"));
    QCOMPARE(doc.inputOutputMap()->bpmNumber(), 20);
}

void VdjBridge_Test::engineBpmLockReleasedOnDisconnect()
{
    Doc doc(nullptr, 4);
    doc.inputOutputMap()->setBeatGeneratorType(InputOutputMap::Plugin);

    VdjBridge bridge;
    bridge.setDoc(&doc);

    // Make deck 1 the active/master deck and push a steady VDJ BPM.
    QMetaObject::invokeMethod(&bridge, "onGlobalTrigger",
        Q_ARG(QString, "masterdeck"), Q_ARG(QVariant, QVariant("on")));
    auto deckTrigger = [&](const QString &t, const QVariant &v) {
        QMetaObject::invokeMethod(&bridge, "onDeckTrigger",
            Q_ARG(int, 0), Q_ARG(QString, t), Q_ARG(QVariant, v));
    };
    deckTrigger("get_filepath", QVariant("/m/a.mp3"));
    deckTrigger("get_title", QVariant("Song A"));
    deckTrigger("get_artist", QVariant("Artist A"));
    deckTrigger("get_bpm", QVariant(128.0));
    deckTrigger("play", QVariant("on"));

    // Pushing the BPM engages the authoritative external lock.
    QCOMPARE(doc.inputOutputMap()->bpmNumber(), 128);
    QVERIFY(doc.inputOutputMap()->m_externalBpmLock == true);

    auto pulse = [&]() {
        QMetaObject::invokeMethod(doc.inputOutputMap(), "slotPluginBeat",
            Q_ARG(quint32, 0u), Q_ARG(quint32, 8341u), Q_ARG(uchar, uchar(255)),
            Q_ARG(QString, QStringLiteral("beat")));
    };

    // While locked, a beat pulse primes the beat clock but cannot move the BPM.
    pulse();
    QCOMPARE(doc.inputOutputMap()->bpmNumber(), 128);

    // Disconnecting VDJ must release the lock so OS2L/timing beats resume.
    QMetaObject::invokeMethod(&bridge, "onTelemetryClientDisconnected");
    QVERIFY(doc.inputOutputMap()->m_externalBpmLock == false);

    // With the lock released, an inter-beat-timed pulse derives the BPM from
    // arrival timing again, moving it away from the locked 128.
    QTest::qWait(100);   // ~600 bpm if derived from timing
    pulse();
    QVERIFY(doc.inputOutputMap()->bpmNumber() != 128);
}

QTEST_MAIN(VdjBridge_Test)