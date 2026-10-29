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

    /** A mixer trigger of the write under way, e.g. the crossfader */
    void global(const char *trigger, const QVariant &value)
    {
        QMetaObject::invokeMethod(&bridge, "onGlobalTrigger", Q_ARG(QString, QString::fromLatin1(trigger)),
                                  Q_ARG(QVariant, value));
    }

    /** What Perform drives the Show with, as one line */
    QString drive(Show *show) const
    {
        const char *transport = show->isRunning() ? (show->isPaused() ? "paused" : "running")
                                : doc.masterTimer()->isStartQueued(show) ? "queued" : "idle";
        return QStringLiteral("%1 %2 %3 %4 %5 %6")
            .arg(transport, show->syncSource() == ShowRunner::External ? "ext" : "auto",
                 show->performAudioSuppressed() ? "muted" : "audible", show->isDominant() ? "dom" : "sub")
            .arg(show->masterIntensity(), 0, 'f', 2)
            .arg(show->externalElapsedTime());
    }

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

    /** The Perform state of the Show's drive, Idle when no deck drives it */
    State driveState(Show *show) const
    {
        for (const PerformFsm::Drive &drive : bridge.performFsm()->snapshot().drives)
            if (drive.showId == show->id())
                return drive.state;
        return State::Idle;
    }
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

void VdjBridge_Test::handoverKeepsTheOldShowPlaying()
{
    PerformRig rig;
    Show *a = rig.addShow("A", "/music/a.mp3");
    Show *b = rig.addShow("B", "/music/b.mp3");
    rig.load(0, "/music/a.mp3");
    rig.load(1, "/music/b.mp3");
    rig.deck(0, "volume", 0.8);
    rig.deck(1, "volume", 0.6);
    rig.play(0, true);
    rig.play(1, true);
    rig.elapsed(0, 40000);
    rig.elapsed(1, 7000);
    rig.master(0);
    rig.bridge.setPerformMode(true);
    rig.tick();
    QCOMPARE(rig.drive(a), QString("running ext muted dom 0.80 40000"));
    QCOMPARE(rig.drive(b), QString("running ext muted sub 0.60 7000"));
    QSignalSpy aStarted(a, QOverload<quint32>::of(&Function::running));
    QSignalSpy bStarted(b, QOverload<quint32>::of(&Function::running));

    // what each outside step finds in the engine, in the order it is emitted
    QStringList log;
    const auto engine = [&]() {
        return QStringLiteral("A %1/%2 B %3/%4")
            .arg(a->stopped() ? "stopping" : "running", a->isDominant() ? "dom" : "sub",
                 b->stopped() ? "stopping" : "running", b->isDominant() ? "dom" : "sub");
    };
    // destroyed first, so the rig's teardown no longer reaches the log
    QObject listener;
    connect(&rig.bridge, &VdjBridge::performShowResolved, &listener, [&](quint32 id) {
        log << QStringLiteral("resolve %1: %2").arg(id == a->id() ? "A" : id == b->id() ? "B" : "none", engine());
    });
    connect(&rig.bridge, &VdjBridge::performReleased, &listener, [&]() { log << QStringLiteral("released"); });
    connect(&rig.bridge, &VdjBridge::performShowSelected, &listener, [&](int id) {
        log << QStringLiteral("select %1: %2").arg(quint32(id) == a->id() ? "A" : quint32(id) == b->id() ? "B" : "?",
                                                   engine());
    });
    QSignalSpy states(rig.bridge.performFsm(), &PerformFsm::stateChanged);

    // deck 1 masterdeck off, deck 2 masterdeck on: one handover
    rig.master(1);

    QCOMPARE(states.count(), 1);
    QCOMPARE(rig.state(), State::Live);
    QCOMPARE(rig.bridge.performFsm()->activeShowId(), b->id());
    // the recorder leaves A while A is still dominant, binds B once B is,
    // and only then the Show Manager follows; neither Show stops
    QCOMPARE(log, QStringList({ "resolve none: A running/dom B running/sub",
                                "resolve B: A running/sub B running/dom",
                                "select B: A running/sub B running/dom" }));
    rig.elapsed(0, 41000);
    rig.elapsed(1, 8000);
    rig.tick();
    QCOMPARE(rig.drive(a), QString("running ext muted sub 0.80 41000"));
    QCOMPARE(rig.drive(b), QString("running ext muted dom 0.60 8000"));

    // back to deck 1: dominance moves back, still no restart
    log.clear();
    rig.master(0);
    QCOMPARE(log, QStringList({ "resolve none: A running/sub B running/dom",
                                "resolve A: A running/dom B running/sub",
                                "select A: A running/dom B running/sub" }));
    rig.tick();
    QCOMPARE(rig.drive(a), QString("running ext muted dom 0.80 41000"));
    QCOMPARE(rig.drive(b), QString("running ext muted sub 0.60 8000"));
    QCOMPARE(aStarted.count(), 0);
    QCOMPARE(bStarted.count(), 0);
}

void VdjBridge_Test::aSetLeavesPerformItsSongs_data()
{
    QTest::addColumn<QString>("order");
    QTest::addColumn<QString>("expected");  // A's drive, without its clock

    QTest::newRow("the set plays A, then Perform") << "set first" << "running ext muted dom 0.80";
    QTest::newRow("Perform plays A, then the set") << "Perform first" << "running ext muted sub 0.80";
    QTest::newRow("then A's deck ejects") << "eject" << "idle auto audible dom 1.00";
}

void VdjBridge_Test::aSetLeavesPerformItsSongs()
{
    QFETCH(QString, order);
    QFETCH(QString, expected);

    PerformRig rig;
    Show *a = rig.addShow("A", "/music/a.mp3");
    // a recorded set: its first second plays A
    Show *set = new Show(&rig.doc);
    rig.doc.addFunction(set);
    set->setNestedAudioSuppressed(true);
    Track *track = new Track(Function::invalidId(), set);
    set->addTrack(track);
    ShowFunction *clip = track->createShowFunction(a->id());
    clip->setStartTime(0);
    clip->setDuration(1000);
    rig.load(0, "/music/a.mp3");
    rig.deck(0, "volume", 0.8);
    rig.play(0, true);
    rig.elapsed(0, 40000);
    if (order == "set first")
    {
        rig.master(0);
        set->start(rig.doc.masterTimer(), FunctionParent::master());
        rig.tick(2);
        QVERIFY(a->isRunning());
        rig.bridge.setPerformMode(true);
        rig.tick();
    }
    else
    {
        // deck 2 is the active one
        rig.addShow("B", "/music/b.mp3");
        rig.load(1, "/music/b.mp3");
        rig.play(1, true);
        rig.master(1);
        rig.bridge.setPerformMode(true);
        rig.tick();
        set->start(rig.doc.masterTimer(), FunctionParent::master());
        rig.tick(2);
        if (order == "eject")
        {
            rig.deck(0, "loaded", QVariant("off"));
            rig.tick(2);
        }
    }
    QCOMPARE(rig.drive(a).section(' ', 0, 4), expected);

    // past the set's clip
    rig.tick(60);
    QCOMPARE(rig.drive(a).section(' ', 0, 4), expected);
}

void VdjBridge_Test::everyDeckDrivesItsShow_data()
{
    QTest::addColumn<QString>("deck2");   // what deck 2 does with B's song
    QTest::addColumn<QString>("drive");   // B's drive afterwards, as PerformRig::drive

    QTest::newRow("deck 2 plays") << "play" << "running ext muted sub 0.30 7000";
    QTest::newRow("deck 2 paused after playing") << "pause" << "paused ext muted sub 0.30 7000";
    QTest::newRow("deck 2 cued, never played") << "cue" << "idle ext muted sub 0.30 7000";
    QTest::newRow("deck 2's song has no show") << "unmapped" << "idle auto audible dom 1.00 0";
}

void VdjBridge_Test::everyDeckDrivesItsShow()
{
    QFETCH(QString, deck2);
    QFETCH(QString, drive);

    PerformRig rig;
    Show *a = rig.addShow("A", "/music/a.mp3");
    Show *b = rig.addShow("B", deck2 == "unmapped" ? "/music/other.mp3" : "/music/b.mp3");
    rig.load(0, "/music/a.mp3");
    rig.load(1, "/music/b.mp3");
    rig.deck(0, "volume", 0.8);
    rig.deck(1, "volume", 0.3);
    rig.elapsed(0, 12000);
    rig.elapsed(1, 7000);
    rig.master(0);
    rig.play(0, true);
    QSignalSpy selected(&rig.bridge, &VdjBridge::performShowSelected);
    QSignalSpy resolved(&rig.bridge, &VdjBridge::performShowResolved);
    rig.bridge.setPerformMode(true);
    rig.tick();
    if (deck2 == "play" || deck2 == "pause")
    {
        rig.play(1, true);
        rig.tick();
        if (deck2 == "pause")
            rig.play(1, false);
    }
    rig.tick();

    // each Show on its deck's clock and weight; the active deck's alone dominant,
    // recorded and shown
    QCOMPARE(rig.drive(a), QString("running ext muted dom 0.80 12000"));
    QCOMPARE(rig.drive(b), drive);
    QCOMPARE(rig.state(), State::Live);
    QCOMPARE(resolved.count(), 1);
    QCOMPARE(resolved[0][0].toUInt(), a->id());
    QCOMPARE(selected.count(), 1);
    QCOMPARE(selected[0][0].toUInt(), a->id());
}

void VdjBridge_Test::weightIsTheMasterIntensity_data()
{
    QTest::addColumn<double>("volume1");
    QTest::addColumn<double>("volume2");
    QTest::addColumn<double>("crossfader");  // < 0: not sent
    QTest::addColumn<QString>("masters");    // A and B

    // deck 1 on the crossfader's left, deck 2 on its right, both faders full
    QTest::newRow("deck 2's fader") << 1.0 << 0.5 << -1.0 << "1.00 0.50";
    QTest::newRow("crossfader at the middle") << 1.0 << 1.0 << 0.5 << "1.00 1.00";
    QTest::newRow("crossfader a quarter to the left") << 1.0 << 1.0 << 0.25 << "1.00 0.50";
    QTest::newRow("crossfader near the right") << 1.0 << 1.0 << 0.9 << "0.20 1.00";
    QTest::newRow("deck 1's fader down, crossfader left") << 0.0 << 1.0 << 0.0 << "0.00 0.00";
}

void VdjBridge_Test::weightIsTheMasterIntensity()
{
    QFETCH(double, volume1);
    QFETCH(double, volume2);
    QFETCH(double, crossfader);
    QFETCH(QString, masters);

    PerformRig rig;
    Show *a = rig.addShow("A", "/music/a.mp3");
    Show *b = rig.addShow("B", "/music/b.mp3");
    rig.load(0, "/music/a.mp3");
    rig.load(1, "/music/b.mp3");
    rig.send(0, "volume", 1.0);
    rig.send(0, "leftcross", QVariant("on"));
    rig.send(1, "volume", 1.0);
    rig.send(1, "rightcross", QVariant("on"));
    rig.tickEnd();
    rig.master(0);
    rig.play(0, true);
    rig.play(1, true);
    rig.bridge.setPerformMode(true);
    rig.tick();
    const auto both = [&]() {
        return QStringLiteral("%1 %2").arg(a->masterIntensity(), 0, 'f', 2).arg(b->masterIntensity(), 0, 'f', 2);
    };
    QCOMPARE(both(), QString("1.00 1.00"));

    // the mixer moves in one write: each Show takes its own deck's weight
    rig.send(0, "volume", volume1);
    rig.send(1, "volume", volume2);
    if (crossfader >= 0)
        rig.global("crossfader", crossfader);
    rig.tickEnd();
    QCOMPARE(both(), masters);

    // released at full again
    rig.bridge.setPerformMode(false);
    QCOMPARE(both(), QString("1.00 1.00"));
}

void VdjBridge_Test::aDeckStopsOnlyItsShow_data()
{
    QTest::addColumn<QString>("action");  // on deck 2
    QTest::addColumn<QString>("b");       // B's drive afterwards
    QTest::addColumn<QString>("c");       // C's (mapped to /music/c.mp3)

    QTest::newRow("pause") << "pause" << "paused ext muted sub 0.50 7000" << "idle auto audible dom 1.00 0";
    QTest::newRow("eject") << "eject" << "idle auto audible dom 1.00 7000" << "idle auto audible dom 1.00 0";
    QTest::newRow("a song without a show") << "unmapped" << "idle auto audible dom 1.00 7000"
                                           << "idle auto audible dom 1.00 0";
    QTest::newRow("another show's song") << "load c" << "idle auto audible dom 1.00 7000"
                                         << "running ext muted sub 0.50 2000";
}

void VdjBridge_Test::aDeckStopsOnlyItsShow()
{
    QFETCH(QString, action);
    QFETCH(QString, b);
    QFETCH(QString, c);

    PerformRig rig;
    Show *showA = rig.addShow("A", "/music/a.mp3");
    Show *showB = rig.addShow("B", "/music/b.mp3");
    Show *showC = rig.addShow("C", "/music/c.mp3");
    rig.load(0, "/music/a.mp3");
    rig.load(1, "/music/b.mp3");
    rig.deck(0, "volume", 1.0);
    rig.deck(1, "volume", 0.5);
    rig.elapsed(0, 12000);
    rig.elapsed(1, 7000);
    rig.master(0);
    rig.play(0, true);
    rig.play(1, true);
    rig.bridge.setPerformMode(true);
    rig.tick();
    QSignalSpy aStarted(showA, QOverload<quint32>::of(&Function::running));
    QSignalSpy resolved(&rig.bridge, &VdjBridge::performShowResolved);
    QSignalSpy selected(&rig.bridge, &VdjBridge::performShowSelected);

    if (action == "pause")
    {
        rig.play(1, false);
    }
    else if (action == "eject")
    {
        rig.deck(1, "loaded", QVariant("off"));
    }
    else
    {
        rig.load(1, action == "load c" ? "/music/c.mp3" : "/music/other.mp3");
        rig.elapsed(1, 2000);
    }
    rig.tick(2);

    QCOMPARE(rig.drive(showB), b);
    QCOMPARE(rig.drive(showC), c);
    // the active deck's Show never notices
    QCOMPARE(rig.drive(showA), QString("running ext muted dom 1.00 12000"));
    QCOMPARE(aStarted.count(), 0);
    QCOMPARE(resolved.count(), 0);
    QCOMPARE(selected.count(), 0);
}

void VdjBridge_Test::disengageReleasesEveryShow_data()
{
    QTest::addColumn<QString>("deck2");
    QTest::newRow("deck 2 plays") << "play";
    QTest::newRow("deck 2 paused") << "pause";
    QTest::newRow("deck 2 cued") << "cue";
}

void VdjBridge_Test::disengageReleasesEveryShow()
{
    QFETCH(QString, deck2);

    PerformRig rig;
    Show *a = rig.addShow("A", "/music/a.mp3");
    Show *b = rig.addShow("B", "/music/b.mp3");
    rig.load(0, "/music/a.mp3");
    rig.load(1, "/music/b.mp3");
    rig.deck(0, "volume", 0.7);
    rig.deck(1, "volume", 0.4);
    rig.elapsed(0, 12000);
    rig.elapsed(1, 7000);
    rig.master(0);
    rig.play(0, true);
    rig.bridge.setPerformMode(true);
    if (deck2 != "cue")
        rig.play(1, true);
    rig.tick();
    if (deck2 == "pause")
        rig.play(1, false);
    QVERIFY(rig.drive(b).contains("ext muted sub 0.40"));

    QSignalSpy released(&rig.bridge, &VdjBridge::performReleased);
    QSignalSpy resolved(&rig.bridge, &VdjBridge::performShowResolved);
    rig.bridge.setPerformMode(false);
    QCOMPARE(released.count(), 1);
    QCOMPARE(resolved.count(), 0);
    // stopped, Autonomous, audible, full and dominant: as before Perform
    QVERIFY(a->stopped());
    QVERIFY(b->stopped());
    rig.tick();
    QCOMPARE(rig.drive(a), QString("idle auto audible dom 1.00 12000"));
    QCOMPARE(rig.drive(b), QString("idle auto audible dom 1.00 7000"));
}

void VdjBridge_Test::oneShowOnTwoDecksRunsOnce_data()
{
    QTest::addColumn<QString>("action");
    QTest::addColumn<QString>("drive");  // S's drive afterwards
    QTest::addColumn<State>("state");

    // deck 1 at 30000 and a 0.9 fader, deck 2 at 9000 and 0.4, both play S's song
    QTest::newRow("the handover takes S to deck 2") << "master 2" << "running ext muted dom 0.40 9000" << State::Live;
    QTest::newRow("deck 2 ejects: S stays on deck 1") << "eject 2" << "running ext muted dom 0.90 30000" << State::Live;
    QTest::newRow("deck 1 ejects: S goes on with deck 2") << "eject 1" << "running ext muted sub 0.40 9000"
                                                          << State::Armed;
    QTest::newRow("deck 2 pauses: S plays on with deck 1") << "pause 2" << "running ext muted dom 0.90 30000"
                                                           << State::Live;
}

void VdjBridge_Test::oneShowOnTwoDecksRunsOnce()
{
    QFETCH(QString, action);
    QFETCH(QString, drive);
    QFETCH(State, state);

    PerformRig rig;
    Show *show = rig.addShow("S", "/music/s.mp3");
    rig.load(0, "/music/s.mp3");
    rig.load(1, "/music/s.mp3");
    rig.deck(0, "volume", 0.9);
    rig.deck(1, "volume", 0.4);
    rig.elapsed(0, 30000);
    rig.elapsed(1, 9000);
    rig.master(0);
    rig.play(0, true);
    rig.play(1, true);
    rig.bridge.setPerformMode(true);
    rig.tick();
    QCOMPARE(rig.drive(show), QString("running ext muted dom 0.90 30000"));
    QSignalSpy started(show, QOverload<quint32>::of(&Function::running));
    QSignalSpy stopped(show, QOverload<quint32>::of(&Function::stopped));

    if (action == "master 2")
        rig.master(1);
    else if (action == "eject 2")
        rig.deck(1, "loaded", QVariant("off"));
    else if (action == "eject 1")
        rig.deck(0, "loaded", QVariant("off"));
    else
        rig.play(1, false);
    rig.tick(2);

    // one Show, one run: it changes clocks, it never restarts
    QCOMPARE(rig.drive(show), drive);
    QCOMPARE(rig.state(), state);
    QCOMPARE(started.count(), 0);
    QCOMPARE(stopped.count(), 0);
}

void VdjBridge_Test::endedIsPerDeck_data()
{
    QTest::addColumn<int>("steps");
    QTest::addColumn<QString>("drive");  // S's drive afterwards
    QTest::addColumn<State>("state");

    // S ends at 20000; deck 2 plays it at 19000, deck 3 at 9000, deck 1 plays A and is active
    QTest::newRow("deck 2 runs past S") << 1 << "idle ext muted sub 0.50 21000" << State::Ended;
    QTest::newRow("the handover takes S to deck 3, before its end") << 2 << "running ext muted dom 0.30 9000"
                                                                    << State::Live;
    QTest::newRow("back to deck 1: S is deck 2's again, past its end") << 3 << "idle ext muted sub 0.50 21000"
                                                                       << State::Ended;
}

void VdjBridge_Test::endedIsPerDeck()
{
    QFETCH(int, steps);
    QFETCH(QString, drive);
    QFETCH(State, state);

    PerformRig rig;
    rig.bridge.djFsm()->setDeckCount(3);
    Show *a = rig.addShow("A", "/music/a.mp3");
    Show *s = rig.addShow("S", "/music/s.mp3", 20000);
    rig.load(0, "/music/a.mp3");
    rig.load(1, "/music/s.mp3");
    rig.load(2, "/music/s.mp3");
    rig.deck(0, "volume", 1.0);
    rig.deck(1, "volume", 0.5);
    rig.deck(2, "volume", 0.3);
    rig.elapsed(0, 12000);
    rig.elapsed(1, 19000);
    rig.elapsed(2, 9000);
    rig.master(0);
    for (int deck = 0; deck < 3; deck++)
        rig.play(deck, true);
    rig.bridge.setPerformMode(true);
    rig.tick();
    QCOMPARE(rig.drive(s), QString("running ext muted sub 0.50 19000"));

    // deck 2 plays past S's content: the runner ends it there, A plays on
    rig.elapsed(1, 21000);
    rig.tick(3);
    if (steps >= 2)
    {
        rig.master(2);
        rig.tick(2);
    }
    if (steps >= 3)
    {
        rig.master(0);
        rig.tick(2);
    }
    QCOMPARE(rig.drive(s), drive);
    QCOMPARE(rig.driveState(s), state);
    QVERIFY(a->isRunning());
    QCOMPARE(a->isDominant(), steps != 2);
}

void VdjBridge_Test::aFinishBeforeAMoveStaysWithItsDeck_data()
{
    QTest::addColumn<bool>("wound");  // the run wound down before the move

    QTest::newRow("restarted by the move's plan") << true;
    QTest::newRow("restarted by the queued stop") << false;
}

void VdjBridge_Test::aFinishBeforeAMoveStaysWithItsDeck()
{
    QFETCH(bool, wound);

    // S ends at 20000; deck 1 plays it at 19000, deck 2 at 9000
    PerformRig rig;
    Show *s = rig.addShow("S", "/music/s.mp3", 20000);
    rig.load(0, "/music/s.mp3");
    rig.load(1, "/music/s.mp3");
    rig.deck(0, "volume", 1.0);
    rig.deck(1, "volume", 0.5);
    rig.elapsed(0, 19000);
    rig.elapsed(1, 9000);
    rig.master(0);
    rig.play(0, true);
    rig.play(1, true);
    rig.bridge.setPerformMode(true);
    rig.tick();
    QCOMPARE(rig.drive(s), QString("running ext muted dom 1.00 19000"));

    // the runner ends S on deck 1's clock: it stops S on the timer thread and its
    // finish reaches the GUI thread one hop later, as the Show's forward is queued
    s->stop(FunctionParent::master());
    QMetaObject::invokeMethod(s, "showFinished", Qt::QueuedConnection);
    if (wound)
    {
        for (int i = 0; i < 5 && s->isRunning(); i++)
            rig.doc.masterTimer()->timerTick();
        QVERIFY(!s->isRunning());
    }
    // deck 1 ejects before the finish arrives: S goes on with deck 2, and its
    // next run starts before the GUI thread handles the finish
    rig.deck(0, "loaded", QVariant("off"));
    rig.doc.masterTimer()->timerTick();
    QCoreApplication::processEvents();
    rig.tick(3);

    // deck 2 is before S's end: S plays there
    QCOMPARE(rig.driveState(s), State::Live);
    QCOMPARE(rig.drive(s), QString("running ext muted sub 0.50 9000"));
}

void VdjBridge_Test::disconnectReleasesEveryShowAtOnce()
{
    PerformRig rig;
    Show *a = rig.addShow("A", "/music/a.mp3");
    Show *b = rig.addShow("B", "/music/b.mp3");
    rig.load(0, "/music/a.mp3");
    rig.load(1, "/music/b.mp3");
    rig.master(1);
    rig.play(0, true);
    rig.play(1, true);
    rig.elapsed(0, 5000);
    rig.elapsed(1, 6000);
    rig.bridge.setPerformMode(true);
    rig.tick();
    QVERIFY(a->isRunning() && b->isRunning());

    QSignalSpy states(rig.bridge.performFsm(), &PerformFsm::stateChanged);
    QSignalSpy resolved(&rig.bridge, &VdjBridge::performShowResolved);
    QMetaObject::invokeMethod(&rig.bridge, "onTelemetryClientDisconnected");

    // one transition: the recorder leaves B once, both Shows go dark at once,
    // and the emptied decks' position 0 reaches neither
    QCOMPARE(states.count(), 1);
    QCOMPARE(rig.state(), State::Armed);
    QCOMPARE(resolved.count(), 1);
    QCOMPARE(resolved[0][0].toUInt(), PerformFsm::InvalidShowId);
    QVERIFY(a->stopped() && b->stopped());
    rig.tick();
    QCOMPARE(rig.drive(a), QString("idle auto audible dom 1.00 5000"));
    QCOMPARE(rig.drive(b), QString("idle auto audible dom 1.00 6000"));
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

void VdjBridge_Test::ejectDuringTheStartDrainStopsTheShow()
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
    rig.deck(0, "loaded", QVariant("off"));
    QCOMPARE(rig.state(), State::Armed);
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

void VdjBridge_Test::nestedDisableDuringAHandoverReleasesBoth()
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
    QVERIFY(a->isRunning() && b->isRunning());

    // a synchronous listener disables Perform while the recorder leaves A:
    // its plan runs after the handover, from the state the handover reached
    QStringList log;
    QObject listener;
    connect(&rig.bridge, &VdjBridge::performShowResolved, &listener, [&](quint32 id) {
        log << QStringLiteral("resolve %1").arg(id == b->id() ? "B" : id == a->id() ? "A" : "none");
        if (id == PerformFsm::InvalidShowId)
            rig.bridge.setPerformMode(false);
    });
    connect(&rig.bridge, &VdjBridge::performShowSelected, &listener, [&](int id) {
        log << QStringLiteral("select %1").arg(quint32(id) == b->id() ? "B" : "?");
    });
    connect(&rig.bridge, &VdjBridge::performReleased, &listener, [&]() { log << QStringLiteral("released"); });
    rig.master(1);
    QCOMPARE(rig.state(), State::Idle);
    QCOMPARE(log, QStringList({ "resolve none", "resolve B", "select B", "released" }));
    rig.tick(2);
    QCOMPARE(rig.drive(a).section(' ', 0, 4), QString("idle auto audible dom 1.00"));
    QCOMPARE(rig.drive(b).section(' ', 0, 4), QString("idle auto audible dom 1.00"));
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

void VdjBridge_Test::capture_mixWeights_data()
{
    QTest::addColumn<QString>("capture");
    QTest::addColumn<QString>("weights");  // decks 1 to 4 at the end
    QTest::addColumn<int>("active");

    // deck 1 at its 50% fader on the left at the middle (full), deck 2 through
    // (full at the middle on its side as well: the djfsm rows cover through)
    QTest::newRow("crossfader sweep") << "crossfader_moves" << "0.500 1.000 0.000 0.000" << 2;
    // no sides in the subscription of the day: the faders alone
    QTest::newRow("handover") << "master_handover" << "1.000 1.000 0.000 0.000" << 1;
}

void VdjBridge_Test::getDecksSettlesTheWeightWithItsWrite_data()
{
    QTest::addColumn<double>("crossfader");  // after get_decks in the same write, < 0 for none
    QTest::addColumn<QStringList>("notified");

    // deck 3 on the left at a full fader, the crossfader at three quarters
    QTest::newRow("get_decks alone") << -1.0 << QStringList{ "3=0.50" };
    QTest::newRow("get_decks, then the crossfader") << 0.25 << QStringList{ "3=1.00" };
}

void VdjBridge_Test::getDecksSettlesTheWeightWithItsWrite()
{
    QFETCH(double, crossfader);
    QFETCH(QStringList, notified);

    PerformRig rig;
    const DjFsm *fsm = rig.bridge.djFsm();
    auto global = [&](const char *trigger, const QVariant &value) {
        QMetaObject::invokeMethod(&rig.bridge, "onGlobalTrigger", Q_ARG(QString, QString::fromLatin1(trigger)),
                                  Q_ARG(QVariant, value));
    };
    rig.send(2, "volume", 1.0);
    rig.send(2, "leftcross", QVariant("on"));
    global("crossfader", 0.75);
    rig.tickEnd();
    QCOMPARE(fsm->deckCount(), 2);

    QStringList seen;
    connect(fsm, &DjFsm::weightChanged, &rig.bridge, [&](int deck) {
        seen << QString("%1=%2").arg(deck).arg(fsm->deckAt(deck - 1).weight, 0, 'f', 2);
    });
    // get_decks opens a write like any trigger: its end settles the weight, once
    global("get_decks", 4);
    if (crossfader >= 0)
        global("crossfader", crossfader);
    QCOMPARE(seen, QStringList());
    rig.tickEnd();
    QCOMPARE(seen, notified);
}

void VdjBridge_Test::capture_mixWeights()
{
    QFETCH(QString, capture);
    QFETCH(QString, weights);
    QFETCH(int, active);

    // the bridge hands the mixer's global triggers to the FSM
    PerformRig rig;
    VdjTelemetryClient client;
    VdjCapture::wire(client, &rig.bridge);
    QVERIFY(VdjCapture::replay(client, capture.toLatin1().constData()) > 0);

    const DjFsm *fsm = rig.bridge.djFsm();
    QStringList seen;
    for (int i = 0; i < DjFsm::MaxDecks; i++)
        seen << QString::number(fsm->deckAt(i).weight, 'f', 3);
    QCOMPARE(seen.join(' '), weights);
    QCOMPARE(fsm->activeDeck(), active);
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
    // the recorder sees the handover from A to B alone, C runs on deck 1 under B
    PerformRig rig;
    Show *a = rig.addShow("A", "/m/a.mp3");
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
    QCOMPARE(a->syncSource(), int(ShowRunner::Autonomous));
    QCOMPARE(c->syncSource(), int(ShowRunner::External));
    QVERIFY(!c->isDominant());
    QVERIFY(b->isDominant());
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
    QTest::newRow("switch, then the old deck ejects") << true;
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
        // the old deck plays on: its Show too, VirtualDJ still plays the audio
        QVERIFY(showA.show->performAudioSuppressed());
        QVERIFY(showA.show->isRunning());
        QVERIFY(!showA.audioNow->isRunning());
        QVERIFY(showB.show->performAudioSuppressed());
        QVERIFY(!showB.audioNow->isRunning());
        QVERIFY(showBSceneNowRunningSpy.count() > 0);
        // the old deck ejects: its Show is released
        deckTrigger(0, "loaded", QVariant("off"));
        QTest::qWait(120);
        QVERIFY(!showA.show->performAudioSuppressed());
        QVERIFY(!showA.show->isRunning());
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

void VdjBridge_Test::engineBpmCrawlsOnlyWhenNothingPlays_data()
{
    QTest::addColumn<int>("unknown");    // the deck whose song's tempo VirtualDJ never sent, 0: none
    QTest::addColumn<QString>("moves");  // deck transport, in order
    QTest::addColumn<int>("bpm");

    // deck 1 (active) plays a 128 BPM song, deck 2 a 124 BPM one
    QTest::newRow("the active deck pauses, deck 2 plays on") << 0 << "pause 1" << 124;
    QTest::newRow("both pause") << 0 << "pause 1,pause 2" << 1;
    QTest::newRow("deck 2 plays again under the paused active deck") << 0 << "pause 1,pause 2,play 2" << 124;
    QTest::newRow("the active deck plays again") << 0 << "pause 1,play 1" << 128;
    QTest::newRow("the active deck ejects, deck 2 plays on") << 0 << "eject 1" << 124;
    QTest::newRow("both pause, the active deck ejects, deck 2 plays") << 0 << "pause 1,pause 2,eject 1,play 2" << 124;
    // a known tempo leads; a song of unknown tempo playing stops the crawl, keeps the engine
    QTest::newRow("the active deck's tempo unknown, deck 2 plays") << 1 << "" << 124;
    QTest::newRow("the active deck pauses, deck 2's tempo unknown") << 2 << "pause 1" << 128;
    QTest::newRow("both pause, deck 2 plays at an unknown tempo") << 2 << "pause 1,pause 2,play 2" << 1;
}

void VdjBridge_Test::engineBpmCrawlsOnlyWhenNothingPlays()
{
    QFETCH(int, unknown);
    QFETCH(QString, moves);
    QFETCH(int, bpm);

    PerformRig rig;
    rig.doc.inputOutputMap()->setBeatGeneratorType(InputOutputMap::Plugin);
    for (int deck = 0; deck < 2; deck++)
    {
        rig.send(deck, "get_filepath", deck == 0 ? "/m/a.mp3" : "/m/b.mp3");
        rig.send(deck, "get_title", QStringLiteral("Title %1").arg(deck));
        if (unknown != deck + 1)
            rig.send(deck, "get_bpm", deck == 0 ? 128.0 : 124.0);
        rig.send(deck, "loaded", QVariant("on"));
        rig.tickEnd();
    }
    rig.master(0);
    rig.play(0, true);
    rig.play(1, true);
    QCOMPARE(rig.doc.inputOutputMap()->bpmNumber(), unknown == 1 ? 124 : 128);

    // other decks' Shows play on under a paused active deck: their beats keep a tempo
    for (const QString &move : moves.split(',', Qt::SkipEmptyParts))
    {
        const int deck = move.section(' ', 1).toInt() - 1;
        if (move.startsWith("eject"))
            rig.deck(deck, "loaded", QVariant("off"));
        else
            rig.play(deck, move.startsWith("play"));
    }
    QCOMPARE(rig.doc.inputOutputMap()->bpmNumber(), bpm);
    rig.doc.inputOutputMap()->setBeatGeneratorType(InputOutputMap::Disabled);
}

void VdjBridge_Test::anEndRestartsNothing()
{
    // the runner stops S at its end and forwards the finish; its postRun's stop
    // reaches the bridge before the plan the finish asks for
    PerformRig rig;
    Show *s = rig.addShow("S", "/music/s.mp3", 20000);
    QSignalSpy started(s, QOverload<quint32>::of(&Function::running));
    rig.load(0, "/music/s.mp3");
    rig.master(0);
    rig.play(0, true);
    rig.elapsed(0, 19000);
    rig.bridge.setPerformMode(true);
    rig.tick();
    QVERIFY(s->isRunning());

    rig.elapsed(0, 21000);
    s->stop(FunctionParent::master());
    QMetaObject::invokeMethod(s, "showFinished", Qt::QueuedConnection);
    for (int i = 0; i < 5 && s->isRunning(); i++)
        rig.doc.masterTimer()->timerTick();
    QVERIFY(!s->isRunning());
    QCoreApplication::processEvents();

    QCOMPARE(rig.state(), State::Ended);
    QVERIFY(!rig.doc.masterTimer()->m_startQueue.contains(s));
    rig.tick(2);
    QCOMPARE(started.count(), 1);
}

void VdjBridge_Test::docTeardownWhileDecksPlay()
{
    // the Doc deletes its timer before its Functions: Perform plans nothing on a dying Doc
    VdjBridge bridge;
    Doc *doc = new Doc(nullptr, 4);
    bridge.setDoc(doc);
    PerformShowFixture a = createPerformShowFixture(doc, "A", false);
    PerformShowFixture b = createPerformShowFixture(doc, "B", false);
    bridge.showFactory()->registerMapping("/music/a.mp3", a.show->id());
    bridge.showFactory()->registerMapping("/music/b.mp3", b.show->id());
    for (int deck = 0; deck < 2; deck++)
    {
        QMetaObject::invokeMethod(&bridge, "onDeckTrigger", Q_ARG(int, deck), Q_ARG(QString, "get_filepath"),
                                  Q_ARG(QVariant, QVariant(deck == 0 ? "/music/a.mp3" : "/music/b.mp3")));
        QMetaObject::invokeMethod(&bridge, "onDeckTrigger", Q_ARG(int, deck), Q_ARG(QString, "loaded"),
                                  Q_ARG(QVariant, QVariant("on")));
        QMetaObject::invokeMethod(&bridge, "onDeckTrigger", Q_ARG(int, deck), Q_ARG(QString, "play"),
                                  Q_ARG(QVariant, QVariant("on")));
    }
    QMetaObject::invokeMethod(&bridge, "onDeckTrigger", Q_ARG(int, 0), Q_ARG(QString, "masterdeck"),
                              Q_ARG(QVariant, QVariant("on")));
    QMetaObject::invokeMethod(&bridge, "onTickEnded");
    bridge.setPerformMode(true);
    doc->masterTimer()->timerTick();
    QVERIFY(a.show->isRunning() && b.show->isRunning());

    delete doc;
    QCOMPARE(bridge.performFsm()->snapshot().drives.size(), 2);
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
