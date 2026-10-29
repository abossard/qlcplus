/*
  Q Light Controller Plus - Unit test
  djsetrecorder_test.cpp

  Licensed under the Apache License, Version 2.0
*/

// the set replays on manual ticks
#define private public
#include "mastertimer.h"
#undef private

#include "djsetrecorder_test.h"
#include "djsetrecorder.h"
#include "djfsm.h"
#include "showfactory.h"
#include "vdjcapturereplay.h"

#include "doc.h"
#include "show.h"
#include "track.h"
#include "scene.h"
#include "fixture.h"
#include "showrunner.h"
#include "showfunction.h"
#include "showclipautomation.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include <algorithm>

namespace
{

/** VirtualDJ's write interval at its default frequency */
const int kWriteMs = 33;

QString songPath(const QString &letter)
{
    return QStringLiteral("/music/%1.mp3").arg(letter);
}

/** A deck as a script plays it: VirtualDJ's position, rate and transport */
struct DeckSim
{
    double position = 0.0;
    double rate = 1.0;
    bool playing = false;
    int sent = -1;          //!< the position last written
};

/**
 * A Doc with a song Show per song letter, each playing its own Scene for ten
 * minutes, the decks at full fader, and the recorder on a clock the script
 * moves. Script steps, separated by ';':
 *   +N           N ms of playback, a write every kWriteMs with every playing deck's position that moved
 *   ~N           N ms on the clock, no write
 *   wait         the event loop runs for two update intervals, the clock stands still
 *   dK.load=X    song X on deck K (N has no Show), position 0
 *   dK.song=X    song X on deck K without a position: its first sample comes with the next write
 *   dK.half=X@P  the first half of a write loading X on deck K, position P first; the write stays open
 *   dK.rest=X    the rest of that write
 *   dK.at=P      position P: written at once on a paused deck, else by the next write
 *   dK.send=P    position P in a write of its own, in the millisecond of the write before
 *   dK.play, dK.pause, dK.eject, dK.master (the only master flag on)
 *   dK.rate=R    pitch, dK.vol=V fader, dK.side=left|right
 *   x=V          crossfader; count=N deck count; disconnect
 *   perform=on|off, remove-set, remove-song=X, map-set=X (song X plays the set),
 *   map=X:Y (song X plays Y's Show), nest-set=X (X's Show holds the set),
 *   save (the Doc as saved), load (another workspace with the same songs),
 *   remove-clip=K (deck K's first clip leaves the set)
 */
struct Rig
{
    Doc doc;
    DjFsm fsm;
    ShowFactory factory;
    DjSetRecorder recorder;
    qint64 now = 0;
    std::array<DeckSim, DjFsm::MaxDecks> sims;
    QHash<quint32, QString> letters;    //!< song Show -> letter
    QHash<QString, Show *> songs;       //!< letter -> song Show

    QHash<QString, QString> paths;

    explicit Rig(const QHash<QString, QString> &songPaths = { { "A", songPath("A") },
                                                              { "B", songPath("B") },
                                                              { "C", songPath("C") } })
        : doc(nullptr)
        , factory(&doc)
        , recorder(&doc, &fsm, &factory)
        , paths(songPaths)
    {
        recorder.setClock([this]() { return now; });
        addSongs();
        for (int deck = 1; deck <= DjFsm::MaxDecks; ++deck)
            fsm.onDeckTrigger(deck, "volume", 1.0);
        fsm.onTickEnded();
    }

    void addSongs()
    {
        letters.clear();
        songs.clear();
        Fixture *fixture = new Fixture(&doc);
        fixture->setUniverse(0);
        fixture->setAddress(0);
        fixture->setChannels(quint32(paths.count()));
        doc.addFixture(fixture);
        QStringList keys = paths.keys();
        keys.sort();
        for (const QString &letter : keys)
        {
            Scene *scene = new Scene(&doc);
            scene->setValue(fixture->id(), quint32(songs.count()), 200);
            doc.addFunction(scene);
            Show *song = new Show(&doc);
            song->setName(letter);
            doc.addFunction(song);
            Track *track = new Track(Function::invalidId(), song);
            song->addTrack(track);
            ShowFunction *clip = track->createShowFunction(scene->id());
            clip->setDuration(600000);
            factory.registerMapping(paths.value(letter), song->id());
            letters.insert(song->id(), letter);
            songs.insert(letter, song);
        }
    }

    void write(int deck, const QString &trigger, const QVariant &value)
    {
        fsm.onDeckTrigger(deck, trigger, value);
    }

    void writePosition(int deck)
    {
        DeckSim &sim = sims[deck - 1];
        sim.sent = qRound(sim.position);
        write(deck, "get_time elapsed absolute", sim.sent);
    }

    void advance(int ms)
    {
        const qint64 until = now + ms;
        while (now < until)
        {
            const qint64 step = qMin<qint64>(kWriteMs, until - now);
            now += step;
            bool wrote = false;
            for (int d = 0; d < DjFsm::MaxDecks; ++d)
            {
                DeckSim &sim = sims[d];
                if (sim.playing == false)
                    continue;
                sim.position += step * sim.rate;
                // VirtualDJ sends what changed
                if (qRound(sim.position) == sim.sent)
                    continue;
                writePosition(d + 1);
                wrote = true;
            }
            // VirtualDJ sends nothing while nothing changes
            if (wrote)
                fsm.onTickEnded();
        }
    }

    void run(const QString &script)
    {
        for (const QString &step : script.split(';', Qt::SkipEmptyParts))
        {
            const QString key = step.section('=', 0, 0), value = step.section('=', 1);
            const int deck = key.startsWith('d') && key.contains('.') ? key.mid(1, 1).toInt() : 0;
            const QString verb = key.section('.', 1);
            DeckSim &sim = sims[qMax(0, deck - 1)];
            if (key.startsWith('+'))
                advance(key.mid(1).toInt());
            else if (key.startsWith('~'))
                now += key.mid(1).toInt();
            else if (key == "wait")
                QTest::qWait(DjSetRecorder::kUpdateIntervalMs * 2);
            else if (key == "save")
                doc.resetModified();
            else if (key == "load")
            {
                doc.clearContents();
                addSongs();
            }
            else if (key == "perform")
                recorder.setRecording(value == "on");
            else if (key == "x")
            {
                fsm.onGlobalTrigger("crossfader", value.toDouble());
                fsm.onTickEnded();
            }
            else if (key == "count")
                fsm.setDeckCount(value.toInt());
            else if (key == "disconnect")
            {
                fsm.onDisconnected();
                sims.fill(DeckSim());
            }
            else if (key == "remove-set")
                doc.deleteFunction(recorder.setShowId());
            else if (key == "remove-clip")
            {
                ShowFunction *sf = clip(value.toInt(), 0);
                for (Track *track : sets().first()->tracks())
                    track->removeShowFunction(sf);
            }
            else if (key == "remove-song")
                doc.deleteFunction(songs.value(value)->id());
            else if (key == "map-set")
                factory.registerMapping(songPath(value), recorder.setShowId());
            else if (key == "map")
                factory.registerMapping(songPath(value.section(':', 0, 0)), songs.value(value.section(':', 1))->id());
            else if (key == "nest-set")
            {
                Show *song = songs.value(value);
                Track *track = new Track(Function::invalidId(), song);
                song->addTrack(track);
                track->createShowFunction(recorder.setShowId())->setDuration(1000);
            }
            else if (verb == "load" || verb == "song" || verb == "rest")
            {
                if (verb != "rest")
                    sim.position = 0.0;
                write(deck, "get_filepath", songPath(value));
                write(deck, "get_title", "Title " + value);
                write(deck, "get_artist", "Artist " + value);
                write(deck, "get_bpm", 126.0);
                write(deck, "loaded", "on");
                if (verb == "load")
                    writePosition(deck);
                fsm.onTickEnded();
            }
            else if (verb == "half")
            {
                // VirtualDJ sends a new song's position before its filepath
                sim.position = value.section('@', 1).toDouble();
                writePosition(deck);
            }
            else if (verb == "at")
            {
                sim.position = value.toDouble();
                if (sim.playing == false)
                {
                    writePosition(deck);
                    fsm.onTickEnded();
                }
            }
            else if (verb == "send")
            {
                sim.position = value.toDouble();
                writePosition(deck);
                fsm.onTickEnded();
            }
            else if (verb == "play" || verb == "pause")
            {
                sim.playing = verb == "play";
                write(deck, "play", sim.playing ? "on" : "off");
                fsm.onTickEnded();
            }
            else if (verb == "eject")
            {
                write(deck, "get_title", "Drag a song on this deck to load it");
                write(deck, "get_bpm", 120.0);
                write(deck, "loaded", "off");
                if (sim.playing)
                    write(deck, "play", "off");
                sim.playing = false;
                fsm.onTickEnded();
            }
            else if (verb == "master")
            {
                for (int d = 1; d <= DjFsm::MaxDecks; ++d)
                    write(d, "masterdeck", d == deck ? "on" : "off");
                fsm.onTickEnded();
            }
            else if (verb == "rate")
                sim.rate = value.toDouble();
            else if (verb == "vol")
            {
                write(deck, "volume", value.toDouble());
                fsm.onTickEnded();
            }
            else if (verb == "side")
            {
                write(deck, "leftcross", value == "left" ? "on" : "off");
                write(deck, "rightcross", value == "right" ? "on" : "off");
                fsm.onTickEnded();
            }
            else
                qFatal("unknown script step %s", qPrintable(step));
        }
    }

    /** The Shows in the Sets folder, in creation order */
    QList<Show *> sets() const
    {
        QList<Show *> out;
        for (Function *function : doc.functionsByType(Function::ShowType))
        {
            if (function->path(true) == kSetFolderPath)
                out << qobject_cast<Show *>(function);
        }
        std::sort(out.begin(), out.end(), [](Show *a, Show *b) { return a->id() < b->id(); });
        return out;
    }

    /** One line per track of every set: "Deck N: <song>@<start>+<duration> ..." */
    QStringList summary() const
    {
        QStringList lines;
        for (Show *set : sets())
        {
            for (Track *track : set->tracks())
            {
                QStringList clips;
                for (ShowFunction *sf : track->showFunctions())
                    clips << QString("%1@%2+%3").arg(letters.value(sf->functionID(), "?"))
                                 .arg(sf->startTime()).arg(sf->duration());
                lines << track->name() + ":" + (clips.isEmpty() ? QString() : " " + clips.join(' '));
            }
        }
        return lines;
    }

    /** The clips on deck's track of the first set */
    QList<ShowFunction *> clips(int deck) const
    {
        const QList<Show *> all = sets();
        if (all.isEmpty())
            return {};
        for (Track *track : all.first()->tracks())
        {
            if (track->name() == QString("Deck %1").arg(deck))
                return track->showFunctions();
        }
        return {};
    }

    ShowFunction *clip(int deck, int index) const
    {
        return clips(deck).value(index);
    }
};

/** "t=v t=v ..." */
QList<std::pair<quint32, double>> points(const QString &text)
{
    QList<std::pair<quint32, double>> out;
    for (const QString &point : text.split(' ', Qt::SkipEmptyParts))
        out << std::make_pair(point.section('=', 0, 0).toUInt(), point.section('=', 1).toDouble());
    return out;
}

/** The set under External sync, its clock exactly the positions pushed */
void playExternal(Doc &doc, Show *show, quint32 ms)
{
    MasterTimer *timer = doc.masterTimer();
    if (show->isRunning() == false)
    {
        show->setSyncSource(ShowRunner::External);
        show->start(timer, FunctionParent::master());
        show->setExternalElapsedTime(0);
        timer->timerTick();
    }
    show->setExternalElapsedTime(ms);
    timer->timerTick();
    timer->timerTick();
}

void stopAndSettle(Doc &doc, Show *show)
{
    show->stop(FunctionParent::master());
    doc.masterTimer()->timerTick();
    doc.masterTimer()->timerTick();
}

/** What the replayed set must play at a set time, from the fixture's model of the decks */
struct Expectation
{
    quint32 setMs = 0;
    int deck = 0;
    QString song;
    quint32 songMs = 0;
    double level = 0.0;
    bool dominant = false;
    bool running = false;
    QString label;
};

const char *kSetCapture = "djset_mix";
const quint32 kReplayPositionTolerance = 60;
const double kReplayLevelTolerance = 0.05;

/** The fixture's song letters, Perform off time and expectations: its meta record "expect" */
bool readExpectations(QHash<QString, QString> &songs, qint64 &performOff, QVector<Expectation> &expect)
{
    QFile file(VdjCapture::path(kSetCapture));
    if (file.open(QIODevice::ReadOnly) == false)
        return false;
    while (file.atEnd() == false)
    {
        const QJsonObject record = QJsonDocument::fromJson(file.readLine()).object();
        if (record.contains("expect") == false)
            continue;
        const QJsonObject songObject = record.value("songs").toObject();
        for (auto it = songObject.begin(); it != songObject.end(); ++it)
            songs.insert(it.key(), it.value().toString());
        performOff = qint64(record.value("performOff").toDouble());
        for (const QJsonValue &value : record.value("expect").toArray())
        {
            const QJsonArray row = value.toArray();
            Expectation e;
            e.setMs = quint32(row.at(0).toInt());
            e.deck = row.at(1).toInt();
            e.song = row.at(2).toString();
            e.songMs = quint32(row.at(3).toInt());
            e.level = row.at(4).toDouble();
            e.dominant = row.at(5).toInt() != 0;
            e.running = row.at(6).toInt() != 0;
            e.label = row.at(7).toString();
            expect << e;
        }
        return expect.isEmpty() == false;
    }
    return false;
}

/** The fixture through VirtualDJ's wire, DjFsm and the recorder, Perform on throughout */
void recordCapture(Rig &rig, qint64 performOff)
{
    VdjTelemetryClient client;
    VdjCapture::wire(client, rig.fsm);
    rig.recorder.setRecording(true);
    QVERIFY(VdjCapture::replay(client, kSetCapture, [&rig](qint64 t) { rig.now = t; }) > 0);
    rig.now = performOff;
    rig.recorder.setRecording(false);
}

} // namespace

void DjSetRecorder_Test::setName()
{
    QCOMPARE(DjSetRecorder::setName(QDateTime(QDate(2026, 10, 29), QTime(21, 5, 30))),
             QString("Set 2026-10-29 21:05"));
}

void DjSetRecorder_Test::setIsAShowOfDeckTracks_data()
{
    QTest::addColumn<QString>("script");
    QTest::addColumn<QStringList>("tracks");

    QTest::newRow("two decks") << "perform=on;d1.load=A;d1.play;+100" << QStringList{ "Deck 1", "Deck 2" };
    QTest::newRow("four decks") << "count=4;perform=on;d1.load=A;d1.play;+100"
                                << QStringList{ "Deck 1", "Deck 2", "Deck 3", "Deck 4" };
    QTest::newRow("a deck beyond the tracks")
        << "perform=on;d1.load=A;d1.play;+100;count=3;d3.load=B;d3.play;+100"
        << QStringList{ "Deck 1", "Deck 2", "Deck 3" };
}

void DjSetRecorder_Test::setIsAShowOfDeckTracks()
{
    QFETCH(QString, script);
    QFETCH(QStringList, tracks);

    Rig rig;
    const QDateTime before = QDateTime::currentDateTime();
    rig.run(script);
    const QDateTime after = QDateTime::currentDateTime();

    QCOMPARE(rig.sets().count(), 1);
    Show *set = rig.sets().first();
    QCOMPARE(rig.recorder.setShowId(), set->id());
    QVERIFY2(QRegularExpression("^Set \\d{4}-\\d\\d-\\d\\d \\d\\d:\\d\\d$").match(set->name()).hasMatch(),
             qPrintable(set->name()));
    QVERIFY(set->name() == DjSetRecorder::setName(before) || set->name() == DjSetRecorder::setName(after));
    QCOMPARE(set->path(true), kSetFolderPath);
    QVERIFY(set->nestedAudioSuppressed());
    QCOMPARE(set->timeDivisionType(), Show::Time);
    QStringList names;
    for (Track *track : set->tracks())
        names << track->name();
    QCOMPARE(names, tracks);
}

void DjSetRecorder_Test::clipsFollowTheSongStays_data()
{
    QTest::addColumn<QString>("script");
    QTest::addColumn<QStringList>("summary");

    QTest::newRow("no Perform, no set") << "d1.load=A;d1.play;+1000" << QStringList();
    QTest::newRow("a loaded deck records nothing") << "perform=on;d1.load=A;+1000;perform=off" << QStringList();
    QTest::newRow("a song without a Show records nothing")
        << "perform=on;d1.load=N;d1.play;+1000;perform=off" << QStringList();
    QTest::newRow("the first play starts the set") << "perform=on;d1.load=A;+500;d1.play;+1000;perform=off"
                                                   << QStringList{ "Deck 1: A@0+1000", "Deck 2:" };
    QTest::newRow("a pause stays in the clip")
        << "perform=on;d1.load=A;d1.play;+1000;d1.pause;+500;d1.play;+500;perform=off"
        << QStringList{ "Deck 1: A@0+2000", "Deck 2:" };
    QTest::newRow("a song change ends the clip")
        << "perform=on;d1.load=A;d1.play;+1000;d1.pause;d1.load=B;+500;d1.play;+500;perform=off"
        << QStringList{ "Deck 1: A@0+1000 B@1500+500", "Deck 2:" };
    QTest::newRow("a pause before the next song stays in the clip")
        << "perform=on;d1.load=A;d1.play;+1000;d1.pause;+2000;d1.load=B;d1.play;+500;perform=off"
        << QStringList{ "Deck 1: A@0+3000 B@3000+500", "Deck 2:" };
    QTest::newRow("an eject ends the clip") << "perform=on;d1.load=A;d1.play;+1000;d1.eject;+500;perform=off"
                                            << QStringList{ "Deck 1: A@0+1000", "Deck 2:" };
    QTest::newRow("the same song after an eject is a new clip")
        << "perform=on;d1.load=A;d1.play;+1000;d1.eject;+500;d1.load=A;d1.play;+500;perform=off"
        << QStringList{ "Deck 1: A@0+1000 A@1500+500", "Deck 2:" };
    QTest::newRow("two decks") << "perform=on;d1.load=A;d2.load=B;d1.play;+1000;d2.play;+1000;perform=off"
                               << QStringList{ "Deck 1: A@0+2000", "Deck 2: B@1000+1000" };
    QTest::newRow("Perform on while a deck plays starts at its next write")
        << "d1.load=A;d1.play;+1000;perform=on;+1000;perform=off" << QStringList{ "Deck 1: A@0+967", "Deck 2:" };
    QTest::newRow("Perform on between the reads of a write")
        << "d1.load=A;d1.play;+1000;d1.half=B@5000;~10;perform=on;~10;d1.rest=B;+500;perform=off"
        << QStringList{ "Deck 1: B@0+500", "Deck 2:" };
    QTest::newRow("a song waits for its own first sample")
        << "perform=on;d1.load=A;d1.at=20000;d1.song=B;d1.play;+1000;perform=off"
        << QStringList{ "Deck 1: B@0+967", "Deck 2:" };
    QTest::newRow("a disconnect ends every clip")
        << "perform=on;d1.load=A;d2.load=B;d1.play;d2.play;+1000;disconnect;+500;perform=off"
        << QStringList{ "Deck 1: A@0+1000", "Deck 2: B@0+1000" };
    QTest::newRow("a smaller deck count ends the dropped deck's clip")
        << "count=3;perform=on;d3.load=A;d3.play;+1000;count=2;+500;perform=off"
        << QStringList{ "Deck 1:", "Deck 2:", "Deck 3: A@0+1000" };
    // the next write notices it
    QTest::newRow("a removed song Show ends its clip")
        << "perform=on;d1.load=A;d1.play;+1000;remove-song=A;+500;perform=off"
        << QStringList{ "Deck 1: A@0+1033", "Deck 2:" };
    QTest::newRow("a song that plays the set itself records nothing")
        << "perform=on;d1.load=A;d1.play;+500;map-set=B;d2.load=B;d2.play;+500;perform=off"
        << QStringList{ "Deck 1: A@0+1000", "Deck 2:" };
    QTest::newRow("a song whose Show holds the set records nothing")
        << "perform=on;d1.load=A;d1.play;+500;nest-set=B;d2.load=B;d2.play;+500;perform=off"
        << QStringList{ "Deck 1: A@0+1000", "Deck 2:" };
    // as live Perform switches; the next write notices it
    QTest::newRow("a reassigned song Show starts a new clip")
        << "perform=on;d1.load=A;d1.play;+1000;map=A:C;+1000;perform=off"
        << QStringList{ "Deck 1: A@0+1033 C@1033+967", "Deck 2:" };
    QTest::newRow("another workspace: the playing deck starts a new set")
        << "perform=on;d1.load=A;d1.play;+1000;load;+1000;perform=off" << QStringList{ "Deck 1: A@0+967", "Deck 2:" };
    // the next write starts it
    QTest::newRow("a removed set: the playing deck starts a new one")
        << "perform=on;d1.load=A;d1.play;+1000;remove-set;+1000;perform=off"
        << QStringList{ "Deck 1: A@0+967", "Deck 2:" };
    QTest::newRow("Perform again, a second set")
        << "perform=on;d1.load=A;d1.play;+1000;perform=off;+500;perform=on;+500;perform=off"
        << QStringList{ "Deck 1: A@0+1000", "Deck 2:", "Deck 1: A@0+467", "Deck 2:" };
}

void DjSetRecorder_Test::clipsFollowTheSongStays()
{
    QFETCH(QString, script);
    QFETCH(QStringList, summary);

    Rig rig;
    rig.run(script);
    QCOMPARE(rig.summary(), summary);
}

void DjSetRecorder_Test::clipAutomationFollowsTheDeck_data()
{
    QTest::addColumn<QString>("script");    // after deck 1 is master with song A and Perform is on
    QTest::addColumn<int>("deck");
    QTest::addColumn<int>("segments");      // -1: not checked
    QTest::addColumn<QString>("positions"); // clip ms = song ms, within the position tolerance
    QTest::addColumn<QString>("levels");    // clip ms = level, within the level tolerance
    QTest::addColumn<QString>("dominance"); // clip ms = 1 or 0

    QTest::newRow("in-point") << "d1.at=30000;d1.play;+2000" << 1 << 1 << "0=30000 1500=31500" << "" << "";
    QTest::newRow("loop wraps") << "d1.at=34000;d1.play;+1905;d1.at=34000;+1905;d1.at=34000;+1000" << 1 << 3
                                << "1000=35000 2500=34595 4300=34490" << "" << "";
    QTest::newRow("a seek") << "d1.at=30000;d1.play;+2000;d1.at=60000;+1000" << 1 << 2 << "1000=31000 2500=60500"
                            << "" << "";
    QTest::newRow("a pitched deck") << "d1.rate=1.06;d1.at=30000;d1.play;+4000" << 1 << 1
                                    << "1000=31060 3000=33180" << "" << "";
    QTest::newRow("a pause holds the position") << "d1.at=30000;d1.play;+2000;d1.pause;+3000;d1.play;+1000" << 1
                                                << 3 << "1000=31000 3500=32000 5500=32500" << "" << "";
    QTest::newRow("a cue while paused")
        << "d1.at=30000;d1.play;+1000;d1.pause;+500;d1.at=20000;+500;d1.play;+500" << 1 << 4
        << "500=30500 1200=31000 1700=20000 2300=20300" << "" << "";
    QTest::newRow("a held jog holds the position")
        << "d2.load=B;d2.play;d1.at=30000;d1.play;+1000;d1.rate=0;+2000;d1.rate=1;+1000" << 1 << -1
        << "500=30500 1500=31000 2500=31000 3500=31500" << "" << "";
    QTest::newRow("the update timer between the reads of a write")
        << "d1.at=30000;d1.play;+1000;d1.half=B@5000;~10;wait;~10;d1.rest=B;+500" << 1 << 1 << "1015=31015" << ""
        << "";
    QTest::newRow("a seek in the millisecond of the write before")
        << "d1.at=30000;d1.play;+1000;d1.send=60000;+20" << 1 << 2 << "500=30500 1010=60010" << "" << "";
    QTest::newRow("two fader moves in one millisecond") << "d1.play;+1000;d1.vol=0.5;d1.vol=0.3;+1000" << 1 << -1
                                                        << "" << "500=1 1500=0.3" << "";
    QTest::newRow("the fader steps") << "d1.play;+1000;d1.vol=0.5;+1000" << 1 << -1 << "" << "500=1 990=1 1500=0.5"
                                     << "";
    QTest::newRow("the crossfader sweeps")
        << "d1.side=left;d2.side=right;x=0;d1.play;+500;x=0.75;+500;x=1;+500" << 1 << -1 << ""
        << "250=1 490=1 750=0.5 1250=0" << "";
    QTest::newRow("the active deck hands over, deck 1") << "d2.load=B;d1.play;d2.play;+1000;d2.master;+1000" << 1
                                                        << -1 << "" << "" << "500=1 1500=0";
    QTest::newRow("the active deck hands over, deck 2") << "d2.load=B;d1.play;d2.play;+1000;d2.master;+1000" << 2
                                                        << -1 << "" << "" << "500=0 1500=1";
    QTest::newRow("another deck is active") << "d2.master;d1.play;+1000" << 1 << -1 << "" << "" << "0=0 500=0";
}

void DjSetRecorder_Test::clipAutomationFollowsTheDeck()
{
    QFETCH(QString, script);
    QFETCH(int, deck);
    QFETCH(int, segments);
    QFETCH(QString, positions);
    QFETCH(QString, levels);
    QFETCH(QString, dominance);

    Rig rig;
    rig.run("d1.master;d1.load=A;perform=on;" + script + ";perform=off");
    ShowFunction *clip = rig.clip(deck, 0);
    QVERIFY(clip != nullptr);
    const ShowClipAutomation automation = clip->clipAutomation();

    if (segments >= 0)
        QCOMPARE(automation.segments().count(), segments);
    for (const auto &[clipMs, songMs] : points(positions))
    {
        const qint64 played = automation.songPositionAt(clipMs);
        QVERIFY2(qAbs(played - qint64(songMs)) <= DjSetRecorder::kPositionToleranceMs,
                 qPrintable(QString("at %1: %2, not %3").arg(clipMs).arg(played).arg(songMs)));
    }
    for (const auto &[clipMs, level] : points(levels))
    {
        const qreal played = automation.levelAt(clipMs);
        QVERIFY2(qAbs(played - level) <= DjSetRecorder::kLevelTolerance + 1e-9,
                 qPrintable(QString("at %1: %2, not %3").arg(clipMs).arg(played).arg(level)));
    }
    for (const auto &[clipMs, dominant] : points(dominance))
        QCOMPARE(automation.dominantAt(clipMs), dominant != 0.0);
}

void DjSetRecorder_Test::clipsReachTheSetAtABoundedRate_data()
{
    QTest::addColumn<QString>("before");  // after song A plays on deck 1 under Perform
    QTest::addColumn<QString>("step");    // repeated while the event loop runs
    QTest::addColumn<quint32>("from");    // the song position after before
    QTest::addColumn<bool>("moving");

    QTest::newRow("a playing deck seeks") << "d1.at=30000" << "+33" << 30000u << true;
    QTest::newRow("a paused deck: no write reaches the recorder") << "d1.pause" << "~33" << 0u << false;
}

void DjSetRecorder_Test::clipsReachTheSetAtABoundedRate()
{
    QFETCH(QString, before);
    QFETCH(QString, step);
    QFETCH(quint32, from);
    QFETCH(bool, moving);

    Rig rig;
    rig.run("perform=on;d1.load=A;d1.play;" + before);
    // the clip is in the set as it opens
    ShowFunction *clip = rig.clip(1, 0);
    QVERIFY(clip != nullptr);
    QCOMPARE(clip->duration(), 1u);

    QSignalSpy updates(clip, &ShowFunction::durationChanged);
    QElapsedTimer window;
    window.start();
    int steps = 0;
    while (window.elapsed() < 1000)
    {
        rig.run(step);
        steps++;
        QTest::qWait(20);
    }
    // one update per interval at most, however many writes came in
    const qint64 bound = window.elapsed() / DjSetRecorder::kUpdateIntervalMs + 1;
    QVERIFY2(updates.count() >= 2 && updates.count() <= bound,
             qPrintable(QString("%1 updates in %2 ms, %3 steps").arg(updates.count()).arg(window.elapsed()).arg(steps)));
    QVERIFY(steps > 2 * updates.count());
    // the last one reaches the clock, with what the deck did since the clip opened
    QTRY_COMPARE_WITH_TIMEOUT(clip->duration(), quint32(rig.now), 2000);
    const qint64 end = from + (moving ? rig.now : 0);
    QVERIFY(qAbs(qint64(clip->clipAutomation().songPositionAt(clip->duration())) - end)
            <= DjSetRecorder::kPositionToleranceMs);
}

void DjSetRecorder_Test::aClipWaitsForARunningSetToStop_data()
{
    QTest::addColumn<QString>("script");  // while the set plays deck 1's song

    QTest::newRow("an eject") << "d2.load=B;d2.play;+500;d2.eject;+100";
    QTest::newRow("Perform off") << "d2.load=B;d2.play;+500;perform=off;+100";
    QTest::newRow("a newer set removed meanwhile")
        << "d2.load=B;d2.play;+500;perform=off;perform=on;+100;remove-set;+100";
}

void DjSetRecorder_Test::aClipWaitsForARunningSetToStop()
{
    QFETCH(QString, script);

    Rig rig;
    rig.run("perform=on;d1.load=A;d1.play;+1000");
    Show *set = rig.sets().first();
    playExternal(rig.doc, set, 0);
    rig.run(script + ";wait");
    // a running set's clips are its runner's
    QCOMPARE(rig.clips(2).count(), 0);

    stopAndSettle(rig.doc, set);
    QTRY_COMPARE_WITH_TIMEOUT(rig.clips(2).count(), 1, 2000);
    ShowFunction *clip = rig.clip(2, 0);
    QCOMPARE(rig.letters.value(clip->functionID()), QString("B"));
    QCOMPARE(clip->startTime(), 1000u);
    QCOMPARE(clip->duration(), 500u);
}

void DjSetRecorder_Test::growthAfterASaveMarksTheWorkspaceModified_data()
{
    QTest::addColumn<QString>("script");  // after a save while song A plays on deck 1
    QTest::addColumn<bool>("modified");

    QTest::newRow("a playing deck") << "+1000" << true;
    QTest::newRow("a paused deck") << "d1.pause;~1000" << true;
    QTest::newRow("a clip removed from the set") << "remove-clip=1;save;+1000" << false;
}

void DjSetRecorder_Test::growthAfterASaveMarksTheWorkspaceModified()
{
    QFETCH(QString, script);
    QFETCH(bool, modified);

    Rig rig;
    rig.run("perform=on;d1.load=A;d1.play;+300;save");
    rig.run(script);
    // the next update marks it
    if (modified)
    {
        QTRY_VERIFY_WITH_TIMEOUT(rig.doc.isModified(), 2000);
        QCOMPARE(rig.clip(1, 0)->duration(), 1300u);
    }
    else
    {
        rig.run("wait");
        QVERIFY(rig.doc.isModified() == false);
    }
}

void DjSetRecorder_Test::setChangesRestartTheAutosave_data()
{
    QTest::addColumn<QString>("script");  // while song A plays on deck 1, the workspace modified
    QTest::addColumn<bool>("restarted");

    QTest::newRow("a song change") << "d1.pause;d1.load=B;d1.play;+500" << true;
    QTest::newRow("an eject") << "d1.eject;+500" << true;
    QTest::newRow("a second deck") << "d2.load=B;d2.play;+500" << true;
    // the autosave fires after the last restart: growth alone must not postpone it
    QTest::newRow("growth alone") << "+1000;wait" << false;
}

void DjSetRecorder_Test::setChangesRestartTheAutosave()
{
    QFETCH(QString, script);
    QFETCH(bool, restarted);

    Rig rig;
    rig.run("perform=on;d1.load=A;d1.play;+300;wait");
    QVERIFY(rig.doc.isModified());
    // every Doc::setModified() restarts the autosave timer
    QSignalSpy restarts(&rig.doc, &Doc::modified);
    rig.run(script);
    QCOMPARE(restarts.isEmpty() == false, restarted);
}

void DjSetRecorder_Test::performOffFinalizesTheSet_data()
{
    QTest::addColumn<QString>("script");  // Perform off follows
    QTest::addColumn<bool>("modified");
    QTest::addColumn<QStringList>("summary");

    QTest::newRow("a recorded set") << "perform=on;d1.load=A;d1.play;+1000" << true
                                    << QStringList{ "Deck 1: A@0+1000", "Deck 2:" };
    QTest::newRow("a paused deck at the end") << "perform=on;d1.load=A;d1.play;+1000;d1.pause;+1000" << true
                                              << QStringList{ "Deck 1: A@0+2000", "Deck 2:" };
    QTest::newRow("Perform without a play") << "perform=on;d1.load=A;+1000" << false << QStringList();
}

void DjSetRecorder_Test::performOffFinalizesTheSet()
{
    QFETCH(QString, script);
    QFETCH(bool, modified);
    QFETCH(QStringList, summary);

    Rig rig;
    rig.run(script);
    rig.doc.resetModified();
    rig.run("perform=off");

    QCOMPARE(rig.recorder.isRecording(), false);
    QCOMPARE(rig.recorder.setShowId(), Function::invalidId());
    QCOMPARE(rig.doc.isModified(), modified);
    QCOMPARE(rig.summary(), summary);
    // nothing more reaches the finished set
    rig.run("+1000");
    QTest::qWait(DjSetRecorder::kUpdateIntervalMs * 2);
    QCOMPARE(rig.summary(), summary);
}

void DjSetRecorder_Test::replayReproducesTheSet_data()
{
    QTest::addColumn<QString>("label");

    // phases of the fixture's set, deck 1 unless named
    for (const char *label : { "in-point", "loop wraps", "seek", "pitch", "deck 2", "crossfade",
                               "active deck switch", "pause", "cue while paused", "between songs",
                               "song after eject" })
        QTest::newRow(label) << QString(label);
}

void DjSetRecorder_Test::replayReproducesTheSet()
{
    QFETCH(QString, label);

    QHash<QString, QString> paths;
    qint64 performOff = 0;
    QVector<Expectation> expect;
    QVERIFY(readExpectations(paths, performOff, expect));
    Rig rig(paths);
    recordCapture(rig, performOff);
    QCOMPARE(rig.sets().count(), 1);
    Show *set = rig.sets().first();

    int checked = 0;
    quint32 pushed = UINT_MAX;
    for (const Expectation &e : std::as_const(expect))
    {
        if (e.setMs != pushed)
            playExternal(rig.doc, set, e.setMs);
        pushed = e.setMs;
        if (e.label != label)
            continue;
        checked++;
        Show *song = rig.songs.value(e.song);
        QVERIFY(song != nullptr);
        const QString at = QString("set %1 ms, deck %2, song %3").arg(e.setMs).arg(e.deck).arg(e.song);
        QVERIFY2(song->isRunning() == e.running, qPrintable(at));
        if (e.running == false)
            continue;
        const qint64 played = song->externalElapsedTime();
        QVERIFY2(qAbs(played - qint64(e.songMs)) <= kReplayPositionTolerance,
                 qPrintable(QString("%1: plays %2, not %3").arg(at).arg(played).arg(e.songMs)));
        QVERIFY2(qAbs(song->masterIntensity() - e.level) <= kReplayLevelTolerance,
                 qPrintable(QString("%1: level %2, not %3").arg(at).arg(song->masterIntensity()).arg(e.level)));
        QVERIFY2(song->isDominant() == e.dominant, qPrintable(at));
    }
    QVERIFY2(checked >= 5, qPrintable(QString("%1 points").arg(checked)));
    stopAndSettle(rig.doc, set);
}

void DjSetRecorder_Test::replayedSetHoldsEverySongStay_data()
{
    QTest::addColumn<int>("deck");
    QTest::addColumn<int>("index");
    QTest::addColumn<QString>("song");
    QTest::addColumn<quint32>("start");
    QTest::addColumn<quint32>("duration");
    QTest::addColumn<int>("onDeck");

    // the fixture's timeline from deck 1's first play
    QTest::newRow("deck 1 plays A to its eject") << 1 << 0 << "A" << 0u << 24500u << 2;
    QTest::newRow("deck 2 plays B to Perform off") << 2 << 0 << "B" << 15500u << 14000u << 1;
    QTest::newRow("deck 1 plays C to Perform off") << 1 << 1 << "C" << 26500u << 3000u << 2;
}

void DjSetRecorder_Test::replayedSetHoldsEverySongStay()
{
    QFETCH(int, deck);
    QFETCH(int, index);
    QFETCH(QString, song);
    QFETCH(quint32, start);
    QFETCH(quint32, duration);
    QFETCH(int, onDeck);

    QHash<QString, QString> paths;
    qint64 performOff = 0;
    QVector<Expectation> expect;
    QVERIFY(readExpectations(paths, performOff, expect));
    Rig rig(paths);
    recordCapture(rig, performOff);

    ShowFunction *clip = rig.clip(deck, index);
    QVERIFY(clip != nullptr);
    QCOMPARE(rig.letters.value(clip->functionID()), song);
    // the writes' arrival jitter
    QVERIFY2(qAbs(qint64(clip->startTime()) - qint64(start)) <= 50, qPrintable(QString::number(clip->startTime())));
    QVERIFY2(qAbs(qint64(clip->duration()) - qint64(duration)) <= 50, qPrintable(QString::number(clip->duration())));
    QCOMPARE(rig.clips(deck).count(), onDeck);
}

void DjSetRecorder_Test::recordedSetSurvivesSaveAndLoad()
{
    QHash<QString, QString> paths;
    qint64 performOff = 0;
    QVector<Expectation> expect;
    QVERIFY(readExpectations(paths, performOff, expect));
    Rig rig(paths);
    recordCapture(rig, performOff);
    QCOMPARE(rig.sets().count(), 1);
    Show *set = rig.sets().first();

    QString xml;
    QXmlStreamWriter writer(&xml);
    writer.writeStartElement("Workspace");
    QVERIFY(rig.doc.saveXML(&writer));
    writer.writeEndElement();
    Doc doc(nullptr);
    QXmlStreamReader reader(xml);
    reader.readNextStartElement();
    reader.readNextStartElement();
    QVERIFY(doc.loadXML(reader));
    Show *loaded = qobject_cast<Show *>(doc.function(set->id()));
    QVERIFY(loaded != nullptr);

    QCOMPARE(loaded->name(), set->name());
    QCOMPARE(loaded->path(true), kSetFolderPath);
    QVERIFY(loaded->nestedAudioSuppressed());
    QCOMPARE(loaded->tracks().count(), set->tracks().count());
    int clips = 0;
    for (int t = 0; t < set->tracks().count(); ++t)
    {
        Track *saved = set->tracks().at(t), *read = loaded->tracks().at(t);
        QCOMPARE(read->name(), saved->name());
        QCOMPARE(read->showFunctions().count(), saved->showFunctions().count());
        for (int c = 0; c < saved->showFunctions().count(); ++c)
        {
            ShowFunction *a = saved->showFunctions().at(c), *b = read->showFunctions().at(c);
            QCOMPARE(b->functionID(), a->functionID());
            QCOMPARE(b->startTime(), a->startTime());
            QCOMPARE(b->duration(), a->duration());
            QVERIFY(a->clipAutomation().isEmpty() == false);
            QVERIFY(b->clipAutomation() == a->clipAutomation());
            clips++;
        }
    }
    QCOMPARE(clips, 3);
}

QTEST_MAIN(DjSetRecorder_Test)
