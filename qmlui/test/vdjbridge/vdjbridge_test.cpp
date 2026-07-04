/*
  Q Light Controller Plus - Unit test
  vdjbridge_test.cpp
*/

#include <QtTest>
#include <QSignalSpy>

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