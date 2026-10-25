/*
  Q Light Controller Plus - Unit test
  vdjbridge_test.cpp
*/

#include <QtTest>
#include <QBuffer>
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
#include "vdjcapturereplay.h"
#include "../../../plugins/vdjbridge/vdjbridgeplugin.h"
#include "../../../plugins/vdjbridge/configurevdjbridge.h"

#include "doc.h"
#include "audio.h"
#include "show.h"
#include "showrunner.h"
#include "scene.h"
#include "track.h"
#include "showfunction.h"
#include "function.h"
#include "inputoutputmap.h"
#include "showcommandtrack.h"
#include <QTableWidget>

using State = PerformFsm::PerformState;

/** The Scene clip of a rig Show, for running() spies */
static Scene *clipOf(Doc &doc, Show *show)
{
    for (Track *t : show->tracks())
        for (ShowFunction *sf : t->showFunctions())
            if (Scene *s = qobject_cast<Scene *>(doc.function(sf->functionID())))
                return s;
    return nullptr;
}

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

// --- Helper: a Doc, a bridge and Shows with one Scene clip each, ticked by hand ---

/** One trigger of deck 1's write under way, for a bridge without a rig */
static void send(VdjBridge &bridge, const char *trigger, const QVariant &value)
{
    QMetaObject::invokeMethod(&bridge, "onDeckTrigger", Q_ARG(int, 0),
                              Q_ARG(QString, QString::fromLatin1(trigger)), Q_ARG(QVariant, value));
}

struct PerformRig
{
    Doc doc{ nullptr, 4 };
    VdjBridge bridge;

    PerformRig()
    {
        bridge.setDoc(&doc);
    }

    ~PerformRig()
    {
        bridge.setPerformMode(false);
        for (Function *function : doc.functions())
            function->stop(FunctionParent::master());
        tick(2);
    }

    /** A Show whose content ends at durationMs, mapped to filepath */
    Show *addShow(const QString &name, const QString &filepath, quint32 durationMs = 300000)
    {
        Show *show = new Show(&doc);
        show->setName(name);
        doc.addFunction(show);
        Scene *scene = new Scene(&doc);
        doc.addFunction(scene);
        Track *track = new Track(Function::invalidId(), show);
        show->addTrack(track);
        ShowFunction *sf = track->createShowFunction(scene->id());
        sf->setStartTime(0);
        sf->setDuration(durationMs);
        bridge.showFactory()->registerMapping(filepath, show->id());
        return show;
    }

    /** One trigger of a VDJ write under way */
    void send(int index0, const char *trigger, const QVariant &value)
    {
        QMetaObject::invokeMethod(&bridge, "onDeckTrigger", Q_ARG(int, index0),
                                  Q_ARG(QString, QString::fromLatin1(trigger)), Q_ARG(QVariant, value));
    }

    /** The write ends: the decks commit, the adopted Show takes the position */
    void tickEnd() { QMetaObject::invokeMethod(&bridge, "onTickEnded"); }

    /** One trigger as a write of its own */
    void deck(int index0, const char *trigger, const QVariant &value)
    {
        send(index0, trigger, value);
        tickEnd();
    }

    /** A handover as VirtualDJ streams it: the old master's flag off and the new one's on, one write */
    void master(int index0)
    {
        for (int d = 0; d < DjFsm::MaxDecks; d++)
            if (d != index0)
                send(d, "masterdeck", QVariant("off"));
        deck(index0, "masterdeck", QVariant("on"));
    }

    /** An Automix load: one write */
    void load(int index0, const QString &filepath)
    {
        send(index0, "get_filepath", filepath);
        send(index0, "get_title", QStringLiteral("Title %1").arg(index0));
        send(index0, "get_artist", QStringLiteral("Artist"));
        send(index0, "get_bpm", 128.0);
        send(index0, "loaded", QVariant("on"));
        tickEnd();
    }

    void play(int index0, bool on) { deck(index0, "play", QVariant(on ? "on" : "off")); }
    void elapsed(int index0, double ms) { deck(index0, "get_time elapsed absolute", ms); }

    /** One MasterTimer tick plus the queued notifications it leaves behind */
    void tick(int ticks = 1)
    {
        for (int i = 0; i < ticks; i++)
        {
            doc.masterTimer()->timerTick();
            QCoreApplication::processEvents();
        }
    }

    State state() const { return bridge.performFsm()->state(); }
};

void VdjBridge_Test::deckTransportDrivesTheShow()
{
    PerformRig rig;
    Show *show = rig.addShow("A", "/music/a.mp3");
    rig.load(0, "/music/a.mp3");
    rig.master(0);
    rig.elapsed(0, 12000);
    rig.bridge.setPerformMode(true);
    QCOMPARE(rig.state(), State::Suspended);
    QCOMPARE(show->syncSource(), int(ShowRunner::External));
    QVERIFY(!show->isRunning());

    // play: started at the deck position, on the external clock
    rig.play(0, true);
    QCOMPARE(rig.state(), State::Live);
    QCOMPARE(show->externalElapsedTime(), 12000u);
    rig.tick();
    QVERIFY(show->isRunning());
    QVERIFY(!show->isPaused());
    QVERIFY(show->performAudioSuppressed());

    // pause: the playhead freezes, the Show keeps its look
    rig.play(0, false);
    QCOMPARE(rig.state(), State::Suspended);
    QVERIFY(show->isRunning());
    QVERIFY(show->isPaused());

    // play again: resumed, not restarted
    rig.play(0, true);
    QVERIFY(show->isRunning());
    QVERIFY(!show->isPaused());

    // the deck's clock reaches the Show on every sample
    rig.elapsed(0, 15000);
    QCOMPARE(show->externalElapsedTime(), 15000u);
}

void VdjBridge_Test::handoverLeavesTheShowInOrder()
{
    PerformRig rig;
    Show *a = rig.addShow("A", "/music/a.mp3");
    Show *b = rig.addShow("B", "/music/b.mp3");
    rig.load(0, "/music/a.mp3");
    rig.load(1, "/music/b.mp3");
    rig.play(0, true);
    rig.play(1, true);
    rig.elapsed(0, 40000);
    rig.elapsed(1, 7000);
    rig.master(0);
    rig.bridge.setPerformMode(true);
    rig.tick();
    QVERIFY(a->isRunning());

    // what each outside step finds in the engine, in the order it is emitted
    QStringList log;
    const auto engine = [&]() {
        return QStringLiteral("A %1/%2 B %3/%4")
            .arg(a->stopped() ? "stopping" : "running", a->syncSource() == ShowRunner::External ? "ext" : "auto",
                 b->isRunning() ? "running" : rig.doc.masterTimer()->isStartQueued(b) ? "queued" : "idle",
                 b->syncSource() == ShowRunner::External ? "ext" : "auto");
    };
    // destroyed first, so the rig's teardown no longer reaches the log
    QObject listener;
    connect(&rig.bridge, &VdjBridge::performShowResolved, &listener, [&](quint32 id) {
        log << QStringLiteral("resolve %1: %2").arg(id == a->id() ? "A" : id == b->id() ? "B" : "none", engine());
    });
    connect(&rig.bridge, &VdjBridge::performReleased, &listener, [&]() { log << QStringLiteral("released"); });
    connect(&rig.bridge, &VdjBridge::performShowSelected, &listener, [&](int id) {
        log << QStringLiteral("select %1: %2").arg(quint32(id) == b->id() ? "B" : "?", engine());
    });
    QSignalSpy states(rig.bridge.performFsm(), &PerformFsm::stateChanged);

    // deck 1 masterdeck off, deck 2 masterdeck on: one handover
    rig.master(1);

    QCOMPARE(states.count(), 1);
    QCOMPARE(rig.state(), State::Live);
    QCOMPARE(rig.bridge.performFsm()->activeShowId(), b->id());
    // the recorder leaves A while A is still live, binds B once B is on the
    // external clock and started, and only then the Show Manager follows
    QCOMPARE(log, QStringList({ "resolve none: A running/ext B idle/auto",
                                "resolve B: A stopping/auto B queued/ext",
                                "select B: A stopping/auto B queued/ext" }));
    // B begins at deck 2's position; A is stopped, not paused: its faders leave the mix
    QCOMPARE(b->externalElapsedTime(), 7000u);
    QVERIFY(!a->performAudioSuppressed());
    rig.tick();
    QVERIFY(!a->isRunning());
    QVERIFY(b->isRunning() && !b->isPaused());

    // back to deck 1: A starts again at deck 1's position
    rig.elapsed(0, 45000);
    rig.master(0);
    QCOMPARE(a->externalElapsedTime(), 45000u);
    QCOMPARE(a->syncSource(), int(ShowRunner::External));
    rig.tick();
    QVERIFY(a->isRunning() && !a->isPaused());
    QVERIFY(!b->isRunning());
    QCOMPARE(b->syncSource(), int(ShowRunner::Autonomous));
}

void VdjBridge_Test::disengageStopsTheShow_data()
{
    QTest::addColumn<bool>("playing");
    QTest::newRow("from Live") << true;
    QTest::newRow("from Suspended") << false;
}

void VdjBridge_Test::disengageStopsTheShow()
{
    QFETCH(bool, playing);

    PerformRig rig;
    Show *show = rig.addShow("A", "/music/a.mp3");
    rig.load(0, "/music/a.mp3");
    rig.master(0);
    rig.play(0, true);
    rig.bridge.setPerformMode(true);
    rig.tick();
    QVERIFY(show->isRunning());
    rig.play(0, playing);
    QCOMPARE(show->isPaused(), !playing);

    QSignalSpy released(&rig.bridge, &VdjBridge::performReleased);
    QSignalSpy resolved(&rig.bridge, &VdjBridge::performShowResolved);
    rig.bridge.setPerformMode(false);
    QCOMPARE(rig.state(), State::Idle);
    QCOMPARE(released.count(), 1);
    QCOMPARE(resolved.count(), 0);
    QVERIFY(show->stopped());
    QCOMPARE(show->syncSource(), int(ShowRunner::Autonomous));
    QVERIFY(!show->performAudioSuppressed());
    rig.tick();
    QVERIFY(!show->isRunning());
}

void VdjBridge_Test::deckResetReleasesBeforeItsPositionDrops_data()
{
    QTest::addColumn<QString>("reset");
    QTest::addColumn<bool>("playing");
    for (const char *reset : { "loaded off (eject)", "empty filepath", "disconnect" })
    {
        QTest::newRow(QByteArray(reset) + " while live") << QString::fromLatin1(reset) << true;
        QTest::newRow(QByteArray(reset) + " while suspended") << QString::fromLatin1(reset) << false;
    }
}

void VdjBridge_Test::deckResetReleasesBeforeItsPositionDrops()
{
    QFETCH(QString, reset);
    QFETCH(bool, playing);
    if (reset.startsWith("loaded"))
        reset = "loaded off";

    PerformRig rig;
    Show *show = rig.addShow("A", "/music/a.mp3");
    rig.load(0, "/music/a.mp3");
    rig.master(0);
    rig.play(0, true);
    rig.elapsed(0, 50000);
    rig.bridge.setPerformMode(true);
    rig.tick();
    QVERIFY(show->isRunning());
    QCOMPARE(show->externalElapsedTime(), 50000u);
    rig.play(0, playing);
    QCOMPARE(rig.state(), playing ? State::Live : State::Suspended);

    QSignalSpy resolved(&rig.bridge, &VdjBridge::performShowResolved);
    QSignalSpy states(rig.bridge.performFsm(), &PerformFsm::stateChanged);
    if (reset == QLatin1String("loaded off"))
        rig.deck(0, "loaded", QVariant("off"));
    else if (reset == QLatin1String("empty filepath"))
        rig.deck(0, "get_filepath", QVariant(""));
    else
        QMetaObject::invokeMethod(&rig.bridge, "onTelemetryClientDisconnected");

    // one transition to Armed: the recorder is released first, the Show
    // stopped, and the empty deck's position 0 never reaches it
    QCOMPARE(states.count(), 1);
    QCOMPARE(rig.state(), State::Armed);
    QCOMPARE(resolved.count(), 1);
    QCOMPARE(resolved[0][0].toUInt(), PerformFsm::InvalidShowId);
    QVERIFY(show->stopped());
    QCOMPARE(show->externalElapsedTime(), 50000u);
    QCOMPARE(rig.bridge.djFsm()->deckAt(0).state, DjFsm::DeckState::Empty);
}

void VdjBridge_Test::disconnectThenReconnectAdoptsAgain()
{
    PerformRig rig;
    Show *show = rig.addShow("A", "/music/a.mp3");
    rig.load(0, "/music/a.mp3");
    rig.master(0);
    rig.play(0, true);
    rig.elapsed(0, 30000);
    rig.bridge.setPerformMode(true);
    rig.tick();
    QVERIFY(show->isRunning());

    QMetaObject::invokeMethod(&rig.bridge, "onTelemetryClientDisconnected");
    QCOMPARE(rig.state(), State::Armed);
    rig.tick(2);
    QVERIFY(!show->isRunning());

    // VDJ comes back: the snapshot loads the same song, master and transport
    rig.load(0, "/music/a.mp3");
    rig.master(0);
    rig.elapsed(0, 31000);
    rig.play(0, true);
    QCOMPARE(rig.state(), State::Live);
    QCOMPARE(show->externalElapsedTime(), 31000u);
    rig.tick();
    QVERIFY(show->isRunning() && !show->isPaused());
    QCOMPARE(show->syncSource(), int(ShowRunner::External));
}

void VdjBridge_Test::stoppedShowStartsAgainInsideTheContent()
{
    PerformRig rig;
    Show *show = rig.addShow("A", "/music/a.mp3", 60000);
    rig.load(0, "/music/a.mp3");
    rig.master(0);
    rig.play(0, true);
    rig.elapsed(0, 20000);
    rig.bridge.setPerformMode(true);
    rig.tick();
    QVERIFY(show->isRunning());

    // stopped from outside (MCP, the Show Manager): Perform brings it back
    show->stop(FunctionParent::master());
    rig.tick();
    QVERIFY(!show->isRunning());
    QCOMPARE(rig.state(), State::Live);
    QVERIFY(rig.doc.masterTimer()->isStartQueued(show));
    rig.tick();
    QVERIFY(show->isRunning());
    QCOMPARE(show->syncSource(), int(ShowRunner::External));
}

void VdjBridge_Test::theRunnerEndsTheShowUntilAJumpBack()
{
    PerformRig rig;
    Show *show = rig.addShow("A", "/music/a.mp3", 5000);
    QSignalSpy started(show, QOverload<quint32>::of(&Function::running));
    rig.load(0, "/music/a.mp3");
    rig.master(0);
    rig.play(0, true);
    rig.elapsed(0, 1000);
    rig.bridge.setPerformMode(true);
    rig.tick();
    QVERIFY(show->isRunning());

    // the deck plays past the content: the runner ends the Show, and that is
    // the one end Perform knows; dark, and it stays so
    rig.elapsed(0, 6000);
    QCOMPARE(rig.state(), State::Live);
    rig.tick(3);
    QCOMPARE(rig.state(), State::Ended);
    QVERIFY(!show->isRunning());
    QVERIFY(!rig.doc.masterTimer()->isStartQueued(show));
    QCOMPARE(show->syncSource(), int(ShowRunner::External));
    rig.elapsed(0, 7000);
    rig.tick(3);
    QCOMPARE(started.count(), 1);

    // a backward jump below where it ended brings the Show back there
    rig.elapsed(0, 2000);
    QCOMPARE(rig.state(), State::Live);
    QCOMPARE(show->externalElapsedTime(), 2000u);
    rig.tick();
    QVERIFY(show->isRunning());
    QCOMPARE(started.count(), 2);
}

void VdjBridge_Test::aQueuedFinishWaitsForTheNextTracksSample()
{
    // two files of one Show: the first plays past the content, the runner's
    // finish is queued when the second loads with no sample yet. It is the
    // first track's end, not the second's: the Show starts at the sample.
    PerformRig rig;
    Show *show = rig.addShow("A", "/music/a.mp3", 5000);
    rig.bridge.showFactory()->registerMapping("/music/b.mp3", show->id());
    rig.load(0, "/music/a.mp3");
    rig.master(0);
    rig.play(0, true);
    rig.elapsed(0, 1000);
    rig.bridge.setPerformMode(true);
    rig.tick();
    QVERIFY(show->isRunning());

    rig.elapsed(0, 6000);
    for (int i = 0; i < 3; i++)
        rig.doc.masterTimer()->timerTick();
    QVERIFY(!show->isRunning());
    rig.load(0, "/music/b.mp3");
    QCoreApplication::processEvents();
    QCOMPARE(rig.state(), State::Live);

    rig.elapsed(0, 100);
    rig.tick(2);
    QVERIFY(show->isRunning());
    QCOMPARE(show->externalElapsedTime(), 100u);
}

void VdjBridge_Test::mutedTrackDoesNotRestartTheShow()
{
    // totalDuration() counts a muted track's clip, the runner skips it: only
    // the runner's end counts
    PerformRig rig;
    Show *show = rig.addShow("A", "/music/a.mp3", 5000);
    Scene *longScene = new Scene(&rig.doc);
    rig.doc.addFunction(longScene);
    Track *muted = new Track(Function::invalidId(), show);
    muted->setMute(true);
    show->addTrack(muted);
    ShowFunction *sf = muted->createShowFunction(longScene->id());
    sf->setStartTime(0);
    sf->setDuration(60000);
    QCOMPARE(show->totalDuration(), 60000u);

    QSignalSpy started(show, QOverload<quint32>::of(&Function::running));
    rig.load(0, "/music/a.mp3");
    rig.master(0);
    rig.play(0, true);
    rig.elapsed(0, 1000);
    rig.bridge.setPerformMode(true);
    rig.tick();
    QVERIFY(show->isRunning());

    rig.elapsed(0, 6000);
    rig.tick(20);
    QCOMPARE(rig.state(), State::Ended);
    QVERIFY(!show->isRunning());
    QCOMPARE(started.count(), 1);
}

void VdjBridge_Test::beatShowEndsWhereTheRunnerEnds()
{
    // a BPM Show's clips are placed in beats: at 120 BPM its 4000 (4 beats)
    // end at 2000 ms of the deck, not at 4000 ms
    PerformRig rig;
    Show *show = rig.addShow("A", "/music/a.mp3", 4000);
    show->setTimeDivision(Show::BPM_4_4, 120);
    Scene *clip = clipOf(rig.doc, show);
    clip->setTempoType(Function::Beats);
    QSignalSpy started(show, QOverload<quint32>::of(&Function::running));
    rig.load(0, "/music/a.mp3");
    rig.master(0);
    rig.play(0, true);
    rig.elapsed(0, 1000);
    rig.bridge.setPerformMode(true);
    rig.tick();
    QVERIFY(show->isRunning());

    rig.elapsed(0, 3000);
    rig.tick(20);
    QCOMPARE(rig.state(), State::Ended);
    QVERIFY(!show->isRunning());
    QCOMPARE(started.count(), 1);
}

void VdjBridge_Test::recordingKeepsTheShowPastItsContent_data()
{
    QTest::addColumn<bool>("empty");
    QTest::newRow("a take past the content") << false;
    QTest::newRow("an empty Show under REC starts") << true;
}

void VdjBridge_Test::recordingKeepsTheShowPastItsContent()
{
    QFETCH(bool, empty);

    // REC keeps the runner alive past its content: no end, so no Ended
    PerformRig rig;
    Show *show;
    if (empty)
    {
        show = new Show(&rig.doc);
        show->setName("Empty");
        rig.doc.addFunction(show);
        rig.bridge.showFactory()->registerMapping("/music/a.mp3", show->id());
    }
    else
    {
        show = rig.addShow("A", "/music/a.mp3", 5000);
    }
    show->setCommandRecording(true);
    rig.load(0, "/music/a.mp3");
    rig.master(0);
    rig.play(0, true);
    rig.elapsed(0, empty ? 0 : 1000);
    rig.bridge.setPerformMode(true);
    QCOMPARE(rig.state(), State::Live);
    rig.tick(2);
    QVERIFY(show->isRunning());
    rig.elapsed(0, 6000);
    rig.tick(3);
    QCOMPARE(rig.state(), State::Live);
    QVERIFY(show->isRunning());
}

void VdjBridge_Test::theFinalCommandAtTheExtentRuns()
{
    PerformRig rig;
    Show *show = rig.addShow("A", "/music/a.mp3", 5000);
    Scene *target = new Scene(&rig.doc);
    rig.doc.addFunction(target);
    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::start(1, 5000, target->id())));
    QVERIFY(track.setExtent(5000));
    QVERIFY(show->setCommandTrack(track));
    QSignalSpy ran(target, QOverload<quint32>::of(&Function::running));

    rig.load(0, "/music/a.mp3");
    rig.master(0);
    rig.play(0, true);
    rig.elapsed(0, 4960);
    rig.bridge.setPerformMode(true);
    rig.tick();
    QVERIFY(show->isRunning());
    // the runner runs the command at its extent, then ends
    rig.elapsed(0, 5000);
    rig.tick(3);
    QCOMPARE(ran.count(), 1);
    QVERIFY(!show->isRunning());
    QCOMPARE(rig.state(), State::Ended);
}

void VdjBridge_Test::pauseBeforeTheFirstTickCancelsTheStart()
{
    PerformRig rig;
    Show *show = rig.addShow("A", "/music/a.mp3");
    QSignalSpy clipRan(clipOf(rig.doc, show), QOverload<quint32>::of(&Function::running));
    rig.load(0, "/music/a.mp3");
    rig.master(0);
    rig.bridge.setPerformMode(true);

    // play then pause within one tick: the queued start runs nothing
    rig.play(0, true);
    QVERIFY(rig.doc.masterTimer()->isStartQueued(show));
    rig.play(0, false);
    QCOMPARE(rig.state(), State::Suspended);
    QVERIFY(show->stopped());
    rig.tick(1);
    QCOMPARE(clipRan.count(), 0);
    rig.tick(1);
    QVERIFY(!show->isRunning());

    // play again: a fresh start
    rig.play(0, true);
    rig.tick();
    QVERIFY(show->isRunning() && !show->isPaused());
    QCOMPARE(clipRan.count(), 1);
}

void VdjBridge_Test::handoverDuringTheStartDrainStopsTheShow()
{
    PerformRig rig;
    MasterTimer *mt = rig.doc.masterTimer();
    Show *a = rig.addShow("A", "/music/a.mp3");
    Show *b = rig.addShow("B", "/music/b.mp3");
    rig.load(0, "/music/a.mp3");
    rig.load(1, "/music/b.mp3");
    rig.play(0, true);
    rig.play(1, true);
    rig.master(0);
    rig.bridge.setPerformMode(true);
    QVERIFY(mt->isStartQueued(a));

    // the timer has taken A off its start queue and not yet run its preRun()
    {
        QMutexLocker lock(&mt->m_functionListMutex);
        mt->m_startQueue.removeAll(a);
        mt->m_startingFunctions.append(a);
    }
    QVERIFY(!a->isRunning() && !mt->isStartQueued(a));
    rig.master(1);
    QCOMPARE(rig.state(), State::Live);
    QCOMPARE(a->syncSource(), int(ShowRunner::Autonomous));
    QVERIFY(a->stopped());

    // the timer finishes its drain: A gets its preRun(), retired from the start
    {
        QMutexLocker lock(&mt->m_functionListMutex);
        mt->m_startingFunctions.clear();
        mt->m_startQueue.prepend(a);
    }
    rig.tick(2);
    QVERIFY(b->isRunning());
    QVERIFY(!a->isRunning());
}

void VdjBridge_Test::trackChangeTickSwitchesOnce()
{
    // one VDJ tick in subscription order: the new filepath, then the new position
    PerformRig rig;
    Show *a = rig.addShow("A", "/music/a.mp3", 30000);
    Show *b = rig.addShow("B", "/music/b.mp3");
    QSignalSpy clipARan(clipOf(rig.doc, a), QOverload<quint32>::of(&Function::running));
    rig.load(0, "/music/a.mp3");
    rig.master(0);
    rig.play(0, true);
    rig.elapsed(0, 1000);
    rig.bridge.setPerformMode(true);
    rig.tick();
    QVERIFY(a->isRunning());
    QCOMPARE(clipARan.count(), 1);
    rig.elapsed(0, 40000);
    rig.tick(3);
    QCOMPARE(rig.state(), State::Ended);
    QVERIFY(!a->isRunning());

    QSignalSpy states(rig.bridge.performFsm(), &PerformFsm::stateChanged);
    QSignalSpy selected(&rig.bridge, &VdjBridge::performShowSelected);
    rig.send(0, "get_filepath", QVariant("/music/b.mp3"));
    rig.send(0, "get_time elapsed absolute", 15298.0);
    rig.tickEnd();
    QCOMPARE(rig.bridge.performFsm()->activeShowId(), b->id());
    QCOMPARE(states.count(), 1);
    QCOMPARE(selected.count(), 1);
    QCOMPARE(b->externalElapsedTime(), 15298u);
    rig.tick(2);
    QVERIFY(b->isRunning());
    QVERIFY(!a->isRunning());
    QCOMPARE(clipARan.count(), 1);
}

void VdjBridge_Test::trackChangeBeforeTheFirstSampleStartsB_data()
{
    QTest::addColumn<bool>("sampleFirst");
    QTest::newRow("finished delivered before the first sample") << false;
    QTest::newRow("first sample delivered before finished") << true;
}

void VdjBridge_Test::trackChangeBeforeTheFirstSampleStartsB()
{
    QFETCH(bool, sampleFirst);

    // B once ended and still holds a clock past its content; a timer tick lands
    // between its filepath and its first position sample
    PerformRig rig;
    Show *a = rig.addShow("A", "/music/a.mp3");
    Show *b = rig.addShow("B", "/music/b.mp3", 5000);
    b->setExternalElapsedTime(200000);
    QSignalSpy finished(b, &Show::showFinished);
    rig.load(0, "/music/a.mp3");
    rig.master(0);
    rig.play(0, true);
    rig.elapsed(0, 1000);
    rig.bridge.setPerformMode(true);
    rig.tick();
    QVERIFY(a->isRunning());

    rig.deck(0, "get_filepath", QVariant("/music/b.mp3"));
    QCOMPARE(rig.state(), State::Live);
    QVERIFY(rig.doc.masterTimer()->isStartQueued(b));
    if (sampleFirst)
    {
        rig.doc.masterTimer()->timerTick();
        rig.elapsed(0, 1500);
        QCoreApplication::processEvents();
    }
    else
    {
        rig.tick();
        rig.elapsed(0, 1500);
    }
    rig.elapsed(0, 2500);
    rig.tick(2);
    QCOMPARE(finished.count(), 0);
    QCOMPARE(rig.state(), State::Live);
    QVERIFY(b->isRunning() && !b->isPaused());
    QCOMPARE(b->externalElapsedTime(), 2500u);
}

void VdjBridge_Test::nestedDisableDuringAHandoverLeavesBUntouched()
{
    PerformRig rig;
    Show *a = rig.addShow("A", "/music/a.mp3");
    Show *b = rig.addShow("B", "/music/b.mp3");
    rig.load(0, "/music/a.mp3");
    rig.load(1, "/music/b.mp3");
    rig.play(0, true);
    rig.play(1, true);
    rig.master(0);
    rig.bridge.setPerformMode(true);
    rig.tick();
    QVERIFY(a->isRunning());

    // a synchronous listener disables Perform while the recorder leaves A:
    // its plan runs after the handover, from the state the handover reached
    QObject listener;
    connect(&rig.bridge, &VdjBridge::performShowResolved, &listener, [&](quint32 id) {
        if (id == PerformFsm::InvalidShowId)
            rig.bridge.setPerformMode(false);
    });
    rig.master(1);
    QCOMPARE(rig.state(), State::Idle);
    rig.tick(2);
    QVERIFY(!a->isRunning());
    QCOMPARE(b->syncSource(), int(ShowRunner::Autonomous));
    QVERIFY(!b->performAudioSuppressed());
    QVERIFY(!b->isRunning());
}

void VdjBridge_Test::ejectKeepsTheTempo_data()
{
    QTest::addColumn<bool>("disconnect");
    QTest::newRow("eject") << false;
    QTest::newRow("disconnect after the eject") << true;
}

void VdjBridge_Test::ejectKeepsTheTempo()
{
    QFETCH(bool, disconnect);

    Doc doc(nullptr, 4);
    doc.inputOutputMap()->setBeatGeneratorType(InputOutputMap::Plugin);
    VdjBridge bridge;
    bridge.setDoc(&doc);
    auto deckTrigger = [&](const QString &t, const QVariant &v) {
        QMetaObject::invokeMethod(&bridge, "onDeckTrigger", Q_ARG(int, 0), Q_ARG(QString, t), Q_ARG(QVariant, v));
        QMetaObject::invokeMethod(&bridge, "onTickEnded");
    };
    deckTrigger("masterdeck", QVariant("on"));
    // an Automix load: one write
    send(bridge, "get_filepath", QVariant("/m/a.mp3"));
    send(bridge, "get_title", QVariant("Song A"));
    send(bridge, "get_artist", QVariant("Artist A"));
    send(bridge, "loaded", QVariant("on"));
    deckTrigger("get_bpm", QVariant(128.0));
    deckTrigger("play", QVariant("on"));
    QCOMPARE(doc.inputOutputMap()->bpmNumber(), 128);

    // the playing active deck is ejected: no song, no crawl, the tempo stays
    deckTrigger("loaded", QVariant("off"));
    QCOMPARE(doc.inputOutputMap()->bpmNumber(), 128);
    if (disconnect)
    {
        QMetaObject::invokeMethod(&bridge, "onTelemetryClientDisconnected");
        QVERIFY(doc.inputOutputMap()->m_externalBpmLock == false);
        QCOMPARE(doc.inputOutputMap()->bpmNumber(), 128);
    }
}

void VdjBridge_Test::activeDeckFollowsThePerDeckMasterdeck()
{
    PerformRig rig;
    DjFsm *fsm = rig.bridge.djFsm();
    rig.load(0, "/music/a.mp3");
    rig.load(1, "/music/b.mp3");
    QCOMPARE(fsm->activeDeck(), 0);

    // the bare global flag is VDJ's default deck, not a deck index: ignored
    QMetaObject::invokeMethod(&rig.bridge, "onGlobalTrigger",
                              Q_ARG(QString, "masterdeck"), Q_ARG(QVariant, QVariant("off")));
    QCOMPARE(fsm->activeDeck(), 0);

    rig.master(0);
    QCOMPARE(fsm->activeDeck(), 1);
    rig.master(1);
    QCOMPARE(fsm->activeDeck(), 2);
    // a phantom deck beyond get_decks never becomes master
    rig.deck(2, "masterdeck", QVariant("on"));
    QCOMPARE(fsm->activeDeck(), 2);
}

void VdjBridge_Test::deletedShowBindsAgainWhenItComesBack()
{
    // Perform's Show is deleted and comes back (undo): the song index names it again
    PerformRig rig;
    // mapped by its Audio clip, as a built Show is, not by a choice
    Show *show = new Show(&rig.doc);
    show->setName("A");
    rig.doc.addFunction(show);
    Audio *audio = new Audio(&rig.doc);
    audio->setSourceFileName("/m/a.mp3");
    rig.doc.addFunction(audio);
    Track *track = new Track(Function::invalidId(), show);
    show->addTrack(track);
    ShowFunction *sf = track->createShowFunction(audio->id());
    sf->setStartTime(0);
    sf->setDuration(300000);
    const quint32 id = show->id();
    QSignalSpy resolved(&rig.bridge, &VdjBridge::performShowResolved);
    rig.load(0, "/m/a.mp3");
    rig.master(0);
    rig.bridge.setPerformMode(true);
    QCOMPARE(rig.bridge.performFsm()->activeShowId(), id);

    QBuffer xml;
    xml.open(QIODevice::WriteOnly);
    QXmlStreamWriter writer(&xml);
    QVERIFY(show->saveXML(&writer));
    QVERIFY(rig.doc.deleteFunction(id));
    QCOMPARE(rig.bridge.performFsm()->activeShowId(), PerformFsm::InvalidShowId);

    QXmlStreamReader reader(xml.data());
    reader.readNextStartElement();
    QVERIFY(Function::loader(reader, &rig.doc));
    QCOMPARE(rig.bridge.performFsm()->activeShowId(), id);
    QCOMPARE(resolved.last().at(0).toUInt(), id);
}

void VdjBridge_Test::handoverInsideALoadWriteResolvesOnce()
{
    // one write: deck 1 loads C, its master flag goes off and deck 2's on;
    // the recorder sees the handover from A to B alone, Perform never adopts C
    PerformRig rig;
    rig.addShow("A", "/m/a.mp3");
    Show *b = rig.addShow("B", "/m/b.mp3");
    Show *c = rig.addShow("C", "/m/c.mp3");
    rig.load(0, "/m/a.mp3");
    rig.load(1, "/m/b.mp3");
    rig.master(0);
    rig.bridge.setPerformMode(true);
    QSignalSpy resolved(&rig.bridge, &VdjBridge::performShowResolved);

    rig.send(0, "get_filepath", QVariant("/m/c.mp3"));
    rig.send(0, "get_title", QVariant("C"));
    rig.send(0, "masterdeck", QVariant("off"));
    rig.send(1, "masterdeck", QVariant("on"));
    rig.tickEnd();

    QStringList sequence;
    for (const QList<QVariant> &args : resolved)
    {
        const quint32 id = args[0].toUInt();
        sequence << (id == PerformFsm::InvalidShowId ? QStringLiteral("-") : rig.doc.function(id)->name());
    }
    QCOMPARE(sequence, QStringList({ "-", "B" }));
    QCOMPARE(rig.bridge.performFsm()->activeShowId(), b->id());
    QCOMPARE(c->syncSource(), int(ShowRunner::Autonomous));
    QVERIFY(c->stopped());
}

void VdjBridge_Test::capture_ejectReleasesPerform()
{
    // VDJ's eject (measured): placeholder title, get_bpm 120, loaded off, no
    // empty filepath, for a deck Perform has adopted
    PerformRig rig;
    Show *b = rig.addShow("B", "/music/track-4.mp3");
    VdjTelemetryClient client;
    VdjCapture::wire(client, &rig.bridge);
    rig.master(1);
    rig.bridge.setPerformMode(true);
    QSignalSpy resolved(&rig.bridge, &VdjBridge::performShowResolved);
    QSignalSpy positions(b, &Show::externalElapsedTimeChanged);

    QVERIFY(VdjCapture::replay(client, "eject") > 0);

    QCOMPARE(rig.state(), State::Armed);
    QCOMPARE(rig.bridge.djFsm()->deckAt(1).state, DjFsm::DeckState::Empty);
    QVERIFY(b->stopped());
    QCOMPARE(b->syncSource(), int(ShowRunner::Autonomous));
    // the snapshot write carries the song and its position together: B takes
    // it, and the eject's 0 never reaches it
    QVERIFY(b->externalElapsedTime() > 0);
    for (const QList<QVariant> &args : positions)
        QVERIFY(args[0].toUInt() > 0);
    QCOMPARE(resolved.count(), 2);
    QCOMPARE(resolved[0][0].toUInt(), b->id());
    QCOMPARE(resolved[1][0].toUInt(), PerformFsm::InvalidShowId);
}

void VdjBridge_Test::capture_ejectPerform_data()
{
    QTest::addColumn<QString>("capture");
    QTest::addColumn<int>("master");            // deck index
    QTest::addColumn<QStringList>("files");     // Shows S0, S1, ... mapped to these
    QTest::addColumn<QStringList>("resolved");  // performShowResolved in order, "-" = released
    QTest::addColumn<int>("adopted");           // the Show synced at the end, -1 = none
    QTest::addColumn<bool>("positioned");       // any Show took a position

    QTest::newRow("the same file again: resolved again, its position follows")
        << "eject_reload_same_file" << 0 << QStringList{ "/music/track-6.mp3" }
        << QStringList({ "S0", "-", "S0" }) << 0 << true;
    // the load over S0 releases it before S1 resolves
    QTest::newRow("eject while playing: released")
        << "eject_while_playing" << 0 << QStringList({ "/music/track-5.mp3", "/music/track-6.mp3" })
        << QStringList({ "S0", "-", "S1", "-" }) << -1 << true;
    QTest::newRow("eject paused at zero: released, no position")
        << "eject_paused_at_zero" << 1 << QStringList{ "/music/track-9.mp3" }
        << QStringList({ "S0", "-" }) << -1 << false;
}

void VdjBridge_Test::capture_ejectPerform()
{
    QFETCH(QString, capture);
    QFETCH(int, master);
    QFETCH(QStringList, files);
    QFETCH(QStringList, resolved);
    QFETCH(int, adopted);
    QFETCH(bool, positioned);

    PerformRig rig;
    QList<Show *> shows;
    for (int i = 0; i < files.size(); ++i)
        shows << rig.addShow(QStringLiteral("S%1").arg(i), files[i]);
    VdjTelemetryClient client;
    VdjCapture::wire(client, &rig.bridge);
    rig.master(master);
    rig.bridge.setPerformMode(true);
    QSignalSpy resolvedSpy(&rig.bridge, &VdjBridge::performShowResolved);
    int positions = 0;
    for (Show *show : shows)
        connect(show, &Show::externalElapsedTimeChanged, &rig.bridge, [&positions]() { positions++; });

    QVERIFY(VdjCapture::replay(client, capture.toLatin1().constData()) > 0);
    rig.tick(2);

    QStringList sequence;
    for (const QList<QVariant> &args : resolvedSpy)
    {
        const quint32 id = args[0].toUInt();
        sequence << (id == PerformFsm::InvalidShowId ? QStringLiteral("-") : rig.doc.function(id)->name());
    }
    QCOMPARE(sequence, resolved);
    QCOMPARE(positions > 0, positioned);
    QCOMPARE(rig.bridge.djFsm()->deckAt(master).state == DjFsm::DeckState::Empty, adopted < 0);
    for (int i = 0; i < shows.size(); ++i)
    {
        QCOMPARE(shows[i]->syncSource() == int(ShowRunner::External), i == adopted);
        if (i != adopted)
            QVERIFY(shows[i]->stopped());
    }
}

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
        QMetaObject::invokeMethod(&bridge, "onTickEnded");
    };
    deckTrigger("get_filepath", QVariant("/music/adopt.mp3"));
    deckTrigger("loaded", QVariant("on"));
    deckTrigger("masterdeck", QVariant("on"));

    doc->masterTimer()->start();

    // enabling Perform adopts the show: External sync while performing
    bridge.setPerformMode(true);
    QCOMPARE(bridge.performFsm()->state(), PerformFsm::PerformState::Suspended);
    QCOMPARE(show->syncSource(), 1);

    deckTrigger("play", QVariant("on"));
    QTest::qWait(50);
    QCOMPARE(bridge.performFsm()->state(), PerformFsm::PerformState::Live);
    QVERIFY(show->isRunning());

    // Perform off: released, stopped and Autonomous again, so manual
    // playback advances normally
    bridge.setPerformMode(false);
    QCOMPARE(bridge.performFsm()->state(), PerformFsm::PerformState::Idle);
    QTRY_VERIFY(!show->isRunning());
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
        QMetaObject::invokeMethod(&bridge, "onTickEnded");
    };
    deckTrigger(0, "get_filepath", QVariant("/music/adopt-running.mp3"));
    deckTrigger(0, "loaded", QVariant("on"));
    deckTrigger(0, "play", QVariant("on"));
    deckTrigger(0, "masterdeck", QVariant("on"));

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
        QMetaObject::invokeMethod(&bridge, "onTickEnded");
    };
    deckTrigger(0, "get_filepath", QVariant("/music/perform-a.mp3"));
    deckTrigger(0, "loaded", QVariant("on"));
    deckTrigger(1, "get_filepath", QVariant("/music/perform-b.mp3"));
    deckTrigger(1, "loaded", QVariant("on"));
    deckTrigger(0, "play", QVariant("on"));
    deckTrigger(1, "play", QVariant("on"));
    deckTrigger(0, "masterdeck", QVariant("on"));

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
        QMetaObject::invokeMethod(&bridge, "onDeckTrigger", Q_ARG(int, 0), Q_ARG(QString, "masterdeck"),
                                  Q_ARG(QVariant, QVariant("off")));
        deckTrigger(1, "masterdeck", QVariant("on"));
        QTest::qWait(120);
        QVERIFY(!showA.show->performAudioSuppressed());
        QVERIFY(!showA.show->isRunning());
        QVERIFY(showB.show->performAudioSuppressed());
        QVERIFY(!showB.audioNow->isRunning());
        QVERIFY(showBSceneNowRunningSpy.count() > 0);
    }
    else
    {
        bridge.setPerformMode(false);
        QCOMPARE(bridge.performFsm()->state(), PerformFsm::PerformState::Idle);
        QTest::qWait(40);
        QVERIFY(!showA.show->isRunning());
        QVERIFY(!showA.show->performAudioSuppressed());
        QVERIFY(!showB.show->performAudioSuppressed());
    }

    // a released Show, started again by hand, plays its Audio
    showA.show->start(doc->masterTimer(), FunctionParent::master());
    bool activeResumed = false;
    for (int i = 0; i < 40 && !activeResumed; ++i)
    {
        QTest::qWait(25);
        activeResumed = showA.audioNow->isRunning();
    }
    QVERIFY2(activeResumed, "Audio must play again once Perform released the Show.");
    QVERIFY(showASceneNowRunningSpy.count() > 0);

    // the restarted Show reaches its 1800 ms items at the autonomous pace
    bool futureAudioStarted = false;
    bool futureSceneStarted = showASceneFutureRunningSpy.count() > 0;
    for (int i = 0; i < 80 && !(futureAudioStarted && futureSceneStarted); ++i)
    {
        QTest::qWait(50);
        futureAudioStarted = futureAudioStarted || showA.audioFuture->isRunning();
        futureSceneStarted = futureSceneStarted || showASceneFutureRunningSpy.count() > 0;
    }
    QVERIFY2(futureAudioStarted, "Future Audio must still start when due after suppression clears.");
    QVERIFY2(futureSceneStarted, "Future Scene on mixed track must still start when due.");

    if (switchDeck)
    {
        bridge.setPerformMode(false);
        QTest::qWait(60);
        QVERIFY(!showB.show->performAudioSuppressed());
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

    auto deckTrigger = [&](const QString &t, const QVariant &v) {
        QMetaObject::invokeMethod(&bridge, "onDeckTrigger",
            Q_ARG(int, 0), Q_ARG(QString, t), Q_ARG(QVariant, v));
        QMetaObject::invokeMethod(&bridge, "onTickEnded");
    };
    deckTrigger("masterdeck", QVariant("on"));

    // A song with a steady BPM of 128 on a PLAYING deck → the engine BPM
    // follows it. (BPM only tracks the song tempo while the deck is playing.)
    // an Automix load: one write
    send(bridge, "get_filepath", QVariant("/m/a.mp3"));
    send(bridge, "get_title", QVariant("Song A"));
    send(bridge, "get_artist", QVariant("Artist A"));
    send(bridge, "loaded", QVariant("on"));
    deckTrigger("get_bpm", QVariant(128.0));
    deckTrigger("play", QVariant("on"));
    QCOMPARE(doc.inputOutputMap()->bpmNumber(), 128);

    // A jittered beat pulse must NOT change the BPM (external lock active).
    QMetaObject::invokeMethod(doc.inputOutputMap(), "slotPluginBeat",
        Q_ARG(quint32, 0u), Q_ARG(quint32, 8341u), Q_ARG(uchar, uchar(255)),
        Q_ARG(QString, QStringLiteral("beat")));
    QCOMPARE(doc.inputOutputMap()->bpmNumber(), 128);

    // A genuinely new steady BPM is followed, at the deck's next write.
    deckTrigger("get_bpm", QVariant(124.0));
    deckTrigger("get_time elapsed absolute", QVariant(1000.0));
    QCOMPARE(doc.inputOutputMap()->bpmNumber(), 124);
}

void VdjBridge_Test::engineBpmOnlyForThePluginBeatGenerator()
{
    Doc doc(nullptr, 4);
    doc.inputOutputMap()->setBeatGeneratorType(InputOutputMap::Internal);
    const int before = doc.inputOutputMap()->bpmNumber();

    VdjBridge bridge;
    bridge.setDoc(&doc);
    auto deckTrigger = [&](const QString &t, const QVariant &v) {
        QMetaObject::invokeMethod(&bridge, "onDeckTrigger",
            Q_ARG(int, 0), Q_ARG(QString, t), Q_ARG(QVariant, v));
        QMetaObject::invokeMethod(&bridge, "onTickEnded");
    };
    deckTrigger("masterdeck", QVariant("on"));
    deckTrigger("get_filepath", QVariant("/m/a.mp3"));
    deckTrigger("loaded", QVariant("on"));
    deckTrigger("get_bpm", QVariant(128.0));
    deckTrigger("play", QVariant("on"));

    // the internal metronome keeps its own tempo and stays unlocked
    QCOMPARE(doc.inputOutputMap()->bpmNumber(), before);
    QVERIFY(doc.inputOutputMap()->m_externalBpmLock == false);
    doc.inputOutputMap()->setBeatGeneratorType(InputOutputMap::Disabled);
}

void VdjBridge_Test::engineBpmDropsLowWhenPaused()
{
    Doc doc(nullptr, 4);
    doc.inputOutputMap()->setBeatGeneratorType(InputOutputMap::Plugin);

    VdjBridge bridge;
    bridge.setDoc(&doc);

    auto deckTrigger = [&](const QString &t, const QVariant &v) {
        QMetaObject::invokeMethod(&bridge, "onDeckTrigger",
            Q_ARG(int, 0), Q_ARG(QString, t), Q_ARG(QVariant, v));
        QMetaObject::invokeMethod(&bridge, "onTickEnded");
    };
    deckTrigger("masterdeck", QVariant("on"));

    // an Automix load: one write
    send(bridge, "get_filepath", QVariant("/m/a.mp3"));
    send(bridge, "get_title", QVariant("Song A"));
    send(bridge, "get_artist", QVariant("Artist A"));
    send(bridge, "loaded", QVariant("on"));
    deckTrigger("get_bpm", QVariant(128.0));

    // Playing → steady song BPM.
    deckTrigger("play", QVariant("on"));
    QCOMPARE(doc.inputOutputMap()->bpmNumber(), 128);

    // Paused → BPM drops to a crawl as a visual cue.
    deckTrigger("play", QVariant("off"));
    QCOMPARE(doc.inputOutputMap()->bpmNumber(), 1);

    // Resumed → back to the steady song BPM.
    deckTrigger("play", QVariant("on"));
    QCOMPARE(doc.inputOutputMap()->bpmNumber(), 128);
}

void VdjBridge_Test::engineBpmLockReleasedOnDisconnect_data()
{
    QTest::addColumn<bool>("playing");
    QTest::newRow("playing") << true;
    QTest::newRow("paused: the crawl does not outlive the connection") << false;
}

void VdjBridge_Test::engineBpmLockReleasedOnDisconnect()
{
    QFETCH(bool, playing);

    Doc doc(nullptr, 4);
    doc.inputOutputMap()->setBeatGeneratorType(InputOutputMap::Plugin);

    VdjBridge bridge;
    bridge.setDoc(&doc);

    auto deckTrigger = [&](const QString &t, const QVariant &v) {
        QMetaObject::invokeMethod(&bridge, "onDeckTrigger",
            Q_ARG(int, 0), Q_ARG(QString, t), Q_ARG(QVariant, v));
        QMetaObject::invokeMethod(&bridge, "onTickEnded");
    };
    deckTrigger("masterdeck", QVariant("on"));
    // an Automix load: one write
    send(bridge, "get_filepath", QVariant("/m/a.mp3"));
    send(bridge, "get_title", QVariant("Song A"));
    send(bridge, "get_artist", QVariant("Artist A"));
    send(bridge, "loaded", QVariant("on"));
    deckTrigger("get_bpm", QVariant(128.0));
    deckTrigger("play", QVariant("on"));
    deckTrigger("play", QVariant(playing ? "on" : "off"));

    // Pushing the BPM engages the authoritative external lock.
    QCOMPARE(doc.inputOutputMap()->bpmNumber(), playing ? 128 : 1);
    QVERIFY(doc.inputOutputMap()->m_externalBpmLock == true);

    auto pulse = [&]() {
        QMetaObject::invokeMethod(doc.inputOutputMap(), "slotPluginBeat",
            Q_ARG(quint32, 0u), Q_ARG(quint32, 8341u), Q_ARG(uchar, uchar(255)),
            Q_ARG(QString, QStringLiteral("beat")));
    };

    // While locked, a beat pulse primes the beat clock but cannot move the BPM.
    pulse();
    QCOMPARE(doc.inputOutputMap()->bpmNumber(), playing ? 128 : 1);

    // Disconnecting VDJ releases the lock and leaves the song's tempo, so
    // OS2L/timing beats resume from a sane value.
    QMetaObject::invokeMethod(&bridge, "onTelemetryClientDisconnected");
    QVERIFY(doc.inputOutputMap()->m_externalBpmLock == false);
    QCOMPARE(doc.inputOutputMap()->bpmNumber(), 128);
    QCOMPARE(bridge.djFsm()->activeDeck(), 0);

    // With the lock released, an inter-beat-timed pulse derives the BPM from
    // arrival timing again, moving it away from the locked 128.
    QTest::qWait(100);   // ~600 bpm if derived from timing
    pulse();
    QVERIFY(doc.inputOutputMap()->bpmNumber() != 128);
}

QTEST_MAIN(VdjBridge_Test)
