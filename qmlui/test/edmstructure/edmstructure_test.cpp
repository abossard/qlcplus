/*
  Q Light Controller Plus - Unit test
  edmstructure_test.cpp

  Licensed under the Apache License, Version 2.0
*/

#include "edmstructure_test.h"
#include "edmstructure.h"
#include "edmstructuredirector.h"
#include "doc.h"

#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QPointer>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QThreadPool>

namespace
{

const QString kAutoAnalyse = "edmanalysis/autoAnalyse";

/** 20 bars of 2 s from 1 s: intro 0-4, buildup 4-8, drop 8-16, outro 16-20; drops on bars 8 and 12 */
const QString kFinal = R"({"schema": "qlcplus-edm-final/2", "track": {"title": "t", "sha256": "cccccccccccccccc00"},
    "grid": {"bpm": 120.0, "period": 0.5, "beats_per_bar": 4, "first_downbeat": 1.0},
    "sections": [
      {"label": "intro", "start_bar": 0, "end_bar": 4, "start": 0.0, "end": 9.0, "confidence": 100},
      {"label": "buildup", "start_bar": 4, "end_bar": 8, "start": 9.0, "end": 17.0, "confidence": 70},
      {"label": "drop", "start_bar": 8, "end_bar": 16, "start": 17.0, "end": 33.0, "confidence": 90},
      {"label": "outro", "start_bar": 16, "end_bar": 20, "start": 33.0, "end": 41.0, "confidence": 60}],
    "events": [
      {"kind": "vocal_in", "bar": 2, "time": 5.0},
      {"kind": "drop", "bar": 8, "time": 17.0, "confidence": 80},
      {"kind": "drop", "bar": 12, "time": 25.0, "confidence": 60}]})";

/** intro 0-4, a long buildup 4-24, drop 24-40, same grid as kFinal */
const QString kLongFinal = R"({"schema": "qlcplus-edm-final/2", "track": {"title": "long", "sha256": "999999999999999900"},
    "grid": {"bpm": 120.0, "period": 0.5, "beats_per_bar": 4, "first_downbeat": 1.0},
    "sections": [
      {"label": "intro", "start_bar": 0, "end_bar": 4, "start": 0.0, "end": 9.0, "confidence": 100},
      {"label": "buildup", "start_bar": 4, "end_bar": 24, "start": 9.0, "end": 49.0, "confidence": 70},
      {"label": "drop", "start_bar": 24, "end_bar": 40, "start": 49.0, "end": 81.0, "confidence": 90}],
    "events": [{"kind": "drop", "bar": 24, "time": 49.0, "confidence": 80}]})";

void writeFile(const QString &path, const QByteArray &data)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

int msAtBar(double bar)
{
    return qRound(1000 + bar * 2000);
}

QString describe(const EdmCrossings &c)
{
    QStringList parts;
    for (const EdmCrossing &x : c.crossed)
        parts << QString("%1%2@%3").arg(x.kind == EdmCrossing::SectionStart ? "S" : "E").arg(x.index).arg(x.bar);
    return parts.join(" ");
}

/** Look buttons that remember what was asked of them */
struct FakeSurface : EdmLookSurface
{
    QMap<quint32, bool> on;
    QStringList log;

    bool isOn(quint32 id) const override { return on.value(id); }
    void set(quint32 id, bool value) override
    {
        log << QString("%1%2").arg(value ? "on" : "off").arg(id);
        on[id] = value;
        emit changed(id);
    }
    void watch(const QList<quint32> &) override {}
    /** The DJ's hand */
    void external(quint32 id, bool value)
    {
        on[id] = value;
        emit changed(id);
    }
};

/** intro 1, buildup 2, drop 3, breakdown 2 (the same button), outro 4 */
void mapLooks(EdmStructureMapping &mapping, bool autoOn = true)
{
    mapping.setAutoEnabled(autoOn);
    mapping.setWidget("intro", 1);
    mapping.setWidget("buildup", 2);
    mapping.setWidget("drop", 3);
    mapping.setWidget("breakdown", 2);
    mapping.setWidget("outro", 4);
}

/** The analysed songs /m/a.mp3 and /m/b.mp3, an unknown /m/new.mp3, a fake tool, and everything wired */
struct Rig
{
    QTemporaryDir root;
    QString dir = root.filePath("analysis");
    QString log = root.filePath("runs.log");
    QString gate = root.filePath("gate");
    QString failFlag = root.filePath("fail");
    QString crashFlag = root.filePath("crash");
    DjFsm fsm;
    EdmAnalysisQueue queue;
    EdmStructure structure;
    EdmStructureMapping mapping;
    FakeSurface surface;
    EdmStructureDirector director;
    QList<QString> advanced;   // "deck:S1@4 E1@8:active"
    QStringList lost;          // "deck:active"
    QStringList states;        // deck 1's states, repeats folded

    Rig()
        : queue(dir)
        , structure(&fsm, &queue, dir)
        , director(&structure, &mapping, &surface)
    {
        QDir().mkpath(dir + "/final");
        writeFile(dir + "/final/aaaaaaaaaaaaaaaa.json", kFinal.toUtf8());
        writeFile(dir + "/final/bbbbbbbbbbbbbbbb.json", kFinal.toUtf8());
        QString lifted = kFinal;
        lifted.replace(R"("events": [)", R"("events": [{"kind": "lift", "bar": 6, "time": 13.0, "sources": ["dropdetector"]},)");
        writeFile(dir + "/final/dddddddddddddddd.json", lifted.toUtf8());
        writeFile(dir + "/final/9999999999999999.json", kLongFinal.toUtf8());
        writeFile(dir + "/index.json", R"({"aaaaaaaaaaaaaaaa": {"path": "/m/a.mp3"}, "bbbbbbbbbbbbbbbb": {"path": "/m/b.mp3"}, "dddddddddddddddd": {"path": "/m/lift.mp3"}, "9999999999999999": {"path": "/m/long.mp3"}})");
        writeFile(root.filePath("template.json"), kFinal.toUtf8());

        const QString script = root.filePath("tool.sh");
        writeFile(script, QString(R"(#!/bin/sh
echo run >> %1
file="$2"; out="$4"
mkdir -p "$out/final"
echo "{\"event\":\"start\",\"total\":1,\"out\":\"$out\"}"
echo "{\"event\":\"track_start\",\"index\":0,\"total\":1,\"path\":\"$file\"}"
echo "{\"event\":\"stage\",\"index\":0,\"total\":1,\"path\":\"$file\",\"stage\":\"beats\"}"
while [ -f %2 ]; do sleep 0.02; done
if [ -f %5 ]; then echo bad >&2; exit 3; fi
if [ -f %3 ]; then
  echo "{\"event\":\"track_error\",\"index\":0,\"total\":1,\"path\":\"$file\",\"error\":\"boom\"}"
  exit 1
fi
cp %4 "$out/final/cccccccccccccccc.json"
printf '{"cccccccccccccccc": {"title": "t", "path": "%s"}}' "$file" > "$out/index.json"
echo "{\"event\":\"track_done\",\"index\":0,\"total\":1,\"path\":\"$file\",\"id\":\"cccccccccccccccc\",\"bpm\":120.0,\"seconds\":1.0,\"summary\":\"intro\"}"
echo "{\"event\":\"finished\",\"total\":1,\"done\":1,\"failed\":0,\"out\":\"$out\"}"
)").arg(log, gate, failFlag, root.filePath("template.json"), crashFlag).toUtf8());
        queue.setTool([script]() { return QStringList{"/bin/sh", script}; }, QProcessEnvironment::systemEnvironment());

        QObject::connect(&structure, &EdmStructure::deckAdvanced, &structure,
                         [this](int deck, const EdmAnalysis &, const EdmSongPosition &, const EdmCrossings &c, bool active) {
            advanced << QString("%1:%2%3").arg(deck).arg(describe(c), active ? " active" : "");
        });
        QObject::connect(&structure, &EdmStructure::deckLost, &structure,
                         [this](int deck, bool active) { lost << QString("%1:%2").arg(deck).arg(active ? "active" : "idle"); });
        QObject::connect(qobject_cast<EdmDeckStructure *>(structure.deck(1)), &EdmDeckStructure::staticChanged, &structure, [this]() { recordState(); });
    }

    void recordState()
    {
        const QString s = state(1);
        if (states.isEmpty() || states.last() != s)
            states << s;
    }

    QString state(int deck) { return structure.deck(deck)->property("state").toString(); }
    double prop(int deck, const char *name) { return structure.deck(deck)->property(name).toDouble(); }
    int runs() const
    {
        QFile f(log);
        return f.open(QIODevice::ReadOnly) ? f.readAll().count('\n') : 0;
    }

    void load(int deck, const QString &path)
    {
        fsm.onDeckTrigger(deck, "get_filepath", path);
        fsm.onDeckTrigger(deck, "get_title", "t");
        fsm.onDeckTrigger(deck, "get_artist", "a");
        fsm.onDeckTrigger(deck, "get_bpm", 120.0);
        fsm.onDeckTrigger(deck, "loaded", "on");
        fsm.onTickEnded();
    }
    void eject(int deck)
    {
        fsm.onDeckTrigger(deck, "loaded", "off");
        fsm.onTickEnded();
    }
    void at(int deck, double bar)
    {
        fsm.onDeckTrigger(deck, "get_time elapsed absolute", double(msAtBar(bar)));
        fsm.onTickEnded();
    }
    void master(int deck)
    {
        for (int other = 1; other <= DjFsm::MaxDecks; other++)
            fsm.onDeckTrigger(other, "masterdeck", other == deck ? "on" : "off");
        fsm.onTickEnded();
    }

    /** Wait on the deck's static signal until its state is the one asked for */
    bool waitFor(int deck, const QString &wanted)
    {
        QSignalSpy spy(structure.deck(deck), SIGNAL(staticChanged()));
        while (state(deck) != wanted)
        {
            if (spy.wait(5000) == false)
                return false;
        }
        return true;
    }
    bool waitForQueue(const QString &path, const QString &wanted)
    {
        QSignalSpy spy(&queue, &EdmAnalysisQueue::entryChanged);
        while (queue.entry(path).state != wanted)
        {
            if (spy.wait(5000) == false)
                return false;
        }
        return true;
    }
};

}

void EdmStructure_Test::init()
{
    QCoreApplication::setOrganizationName("edmstructure_test");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    static QTemporaryDir settingsDir;
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir.path());
    QSettings().setValue(kAutoAnalyse, true);   // the tests of the automatic path; the default is off
}

void EdmStructure_Test::analysedSongIsReady()
{
    Rig rig;
    rig.master(1);
    rig.load(1, "/m/a.mp3");
    QCOMPARE(rig.state(1), QString("ready"));
    QCOMPARE(rig.structure.deck(1)->property("sections").toList().size(), 4);
    QCOMPARE(rig.structure.deck(1)->property("events").toList().size(), 3);
    QCOMPARE(rig.prop(1, "totalBars"), 20.0);
    const QVariantMap first = rig.structure.deck(1)->property("sections").toList().first().toMap();
    QCOMPARE(first.value("label").toString(), QString("intro"));
    QCOMPARE(first.value("endBar").toDouble(), 4.0);
    QCOMPARE(first.value("confidence").toInt(), 100);
    QCOMPARE(rig.structure.deck(1)->property("events").toList().first().toMap().value("confidence").toInt(), -1);

    rig.at(1, 5.5);
    QCOMPARE(rig.advanced, QStringList{"1:S1@4 active"});
    QCOMPARE(rig.structure.activeDeck(), 1);
}

void EdmStructure_Test::unevenStepsReportEveryCrossingOnce_data()
{
    QTest::addColumn<QList<double>>("bars");
    QTest::addColumn<QStringList>("expected");

    QTest::newRow("uneven steps") << QList<double>{0.0, 1.5, 3.9, 4.0, 9.7, 17.0, 19.9}
        << QStringList{"1:S0@0 active", "1:E0@2 active", "1:S1@4 active", "1:S2@8 E1@8 active", "1:E2@12 S3@16 active"};
    QTest::newRow("one step over everything") << QList<double>{-0.5, 25.0}
        << QStringList{"1:S0@0 active", "1:E0@2 S1@4 S2@8 E1@8 E2@12 S3@16 active"};
    QTest::newRow("stopped on every boundary") << QList<double>{0.0, 4.0, 4.0, 8.0, 8.0, 12.0}
        << QStringList{"1:S0@0 active", "1:E0@2 S1@4 active", "1:S2@8 E1@8 active", "1:E2@12 active"};
}

void EdmStructure_Test::unevenStepsReportEveryCrossingOnce()
{
    QFETCH(QList<double>, bars);
    QFETCH(QStringList, expected);

    Rig rig;
    rig.master(1);
    rig.load(1, "/m/a.mp3");
    for (double bar : bars)
        rig.at(1, bar);

    QCOMPARE(rig.advanced, expected);
}

void EdmStructure_Test::liveValues_data()
{
    QTest::addColumn<double>("bar");
    QTest::addColumn<int>("section");
    QTest::addColumn<double>("barInSection");
    QTest::addColumn<double>("barsToNextSection");
    QTest::addColumn<QString>("nextLabel");
    QTest::addColumn<int>("nextConfidence");
    QTest::addColumn<double>("barsToDrop");
    QTest::addColumn<int>("dropConfidence");

    QTest::newRow("buildup") << 5.5 << 1 << 1.5 << 2.5 << "drop" << 90 << 2.5 << 80;
    QTest::newRow("first drop") << 9.0 << 2 << 1.0 << 7.0 << "outro" << 60 << 3.0 << 60;
    QTest::newRow("no drop left") << 13.0 << 2 << 5.0 << 3.0 << "outro" << 60 << -1.0 << -1;
    QTest::newRow("last section") << 18.0 << 3 << 2.0 << -1.0 << "" << -1 << -1.0 << -1;
}

void EdmStructure_Test::liveValues()
{
    QFETCH(double, bar);
    QFETCH(int, section);
    QFETCH(double, barInSection);
    QFETCH(double, barsToNextSection);
    QFETCH(QString, nextLabel);
    QFETCH(int, nextConfidence);
    QFETCH(double, barsToDrop);
    QFETCH(int, dropConfidence);

    Rig rig;
    rig.load(1, "/m/a.mp3");
    QSignalSpy live(rig.structure.deck(1), SIGNAL(liveChanged()));
    rig.at(1, bar);

    QVERIFY(live.count() >= 1);
    QCOMPARE(rig.prop(1, "bar"), bar);
    QCOMPARE(rig.structure.deck(1)->property("section").toInt(), section);
    QCOMPARE(rig.prop(1, "barInSection"), barInSection);
    QCOMPARE(rig.prop(1, "barsToNextSection"), barsToNextSection);
    QCOMPARE(rig.structure.deck(1)->property("nextSectionLabel").toString(), nextLabel);
    QCOMPARE(rig.structure.deck(1)->property("nextSectionConfidence").toInt(), nextConfidence);
    QCOMPARE(rig.prop(1, "barsToDrop"), barsToDrop);
    QCOMPARE(rig.structure.deck(1)->property("dropConfidence").toInt(), dropConfidence);
}

void EdmStructure_Test::backwardJumpFiresAgain()
{
    Rig rig;
    rig.master(1);
    rig.load(1, "/m/a.mp3");
    rig.at(1, 9.0);
    rig.advanced.clear();

    rig.at(1, 5.0);   // a jump back crosses nothing
    QCOMPARE(rig.advanced, QStringList());
    rig.at(1, 9.0);
    QCOMPARE(rig.advanced.last(), QString("1:S2@8 E1@8 active"));
}

void EdmStructure_Test::twoDecksKeepTheirOwnPosition()
{
    Rig rig;
    rig.master(1);
    rig.load(1, "/m/a.mp3");
    rig.load(2, "/m/b.mp3");
    rig.at(1, 5.5);
    rig.at(2, 9.0);
    rig.advanced.clear();

    QCOMPARE(rig.prop(1, "bar"), 5.5);
    QCOMPARE(rig.prop(2, "bar"), 9.0);
    QCOMPARE(rig.structure.deck(1)->property("section").toInt(), 1);
    QCOMPARE(rig.structure.deck(2)->property("section").toInt(), 2);

    rig.master(2);
    QCOMPARE(rig.structure.activeDeck(), 2);
    QCOMPARE(rig.advanced, (QStringList{"1:", "2: active"}));

    rig.at(2, 12.5);
    QCOMPARE(rig.advanced.last(), QString("2:E2@12 active"));
    QCOMPARE(rig.prop(1, "bar"), 5.5);
}

void EdmStructure_Test::ejectedDeckIsLost()
{
    Rig rig;
    rig.master(1);
    rig.load(1, "/m/a.mp3");
    rig.at(1, 1.0);
    rig.lost.clear();

    rig.eject(1);

    QCOMPARE(rig.state(1), QString("none"));
    QCOMPARE(rig.structure.deck(1)->property("sections").toList().size(), 0);
    QCOMPARE(rig.lost, QStringList{"1:active"});
}

void EdmStructure_Test::unknownSongIsAnalysedWithoutReloading()
{
    Rig rig;
    writeFile(rig.gate, "");
    rig.master(1);
    rig.load(1, "/m/new.mp3");
    QCOMPARE(rig.state(1), QString("lookup"));

    QVERIFY(rig.waitFor(1, "analysing"));
    QSignalSpy stage(rig.structure.deck(1), SIGNAL(staticChanged()));
    while (rig.structure.deck(1)->property("stage").toString().isEmpty())
        QVERIFY(stage.wait(5000));
    QCOMPARE(rig.structure.deck(1)->property("stage").toString(), QString("beats"));
    QCOMPARE(rig.structure.analysisStateForPath("/m/new.mp3"), QString("analysing"));
    QFile::remove(rig.gate);

    QVERIFY(rig.waitFor(1, "ready"));
    QCOMPARE(rig.structure.deck(1)->property("sections").toList().size(), 4);
    QCOMPARE(rig.states, (QStringList{"none", "lookup", "queued", "analysing", "ready"}));

    rig.at(1, 5.5);
    QCOMPARE(rig.advanced, QStringList{"1:S1@4 active"});
}

void EdmStructure_Test::failedRunCanBeRetried_data()
{
    QTest::addColumn<bool>("crash");
    QTest::addColumn<QString>("error");

    QTest::newRow("the tool reports an error") << false << "boom";
    QTest::newRow("the tool exits without a track_done") << true << "bad";
}

void EdmStructure_Test::failedRunCanBeRetried()
{
    QFETCH(bool, crash);
    QFETCH(QString, error);

    Rig rig;
    writeFile(crash ? rig.crashFlag : rig.failFlag, "");
    rig.load(1, "/m/new.mp3");

    QVERIFY(rig.waitFor(1, "failed"));
    QCOMPARE(rig.structure.deck(1)->property("error").toString(), error);
    QCOMPARE(rig.structure.analysisStateForPath("/m/new.mp3"), QString("failed"));
    QCOMPARE(rig.runs(), 1);

    QFile::remove(crash ? rig.crashFlag : rig.failFlag);
    rig.structure.retry("/m/new.mp3");
    QVERIFY(rig.waitFor(1, "ready"));
    QCOMPARE(rig.runs(), 2);
}

void EdmStructure_Test::samePathRunsOnce()
{
    Rig rig;
    rig.load(1, "/m/new.mp3");
    rig.load(2, "/m/new.mp3");

    QVERIFY(rig.waitFor(1, "ready"));
    QVERIFY(rig.waitFor(2, "ready"));
    QCOMPARE(rig.runs(), 1);

    rig.eject(1);
    rig.load(1, "/m/new.mp3");
    QVERIFY(rig.waitFor(1, "ready"));
    QCOMPARE(rig.runs(), 1);
}

void EdmStructure_Test::movedFileIsFoundByContent()
{
    Rig rig;
    const QByteArray audio = "moved audio";
    const QString moved = rig.root.filePath("moved.mp3");
    writeFile(moved, audio);
    const QString id = QString(QCryptographicHash::hash(audio, QCryptographicHash::Sha256).toHex()).left(16);
    writeFile(rig.dir + "/final/" + id + ".json", kFinal.toUtf8());

    rig.load(1, moved);

    QVERIFY(rig.waitFor(1, "ready"));
    QCOMPARE(rig.queue.entry(moved).state, QString());
    QCOMPARE(rig.runs(), 0);
}

void EdmStructure_Test::autoAnalyseOffQueuesNothing()
{
    QSettings().setValue(kAutoAnalyse, false);
    Rig rig;
    rig.load(1, "/m/new.mp3");

    QVERIFY(rig.waitFor(1, "none"));
    QCOMPARE(rig.queue.entry("/m/new.mp3").state, QString());
    QCOMPARE(rig.runs(), 0);
}

void EdmStructure_Test::autoAnalyseIsOffByDefault()
{
    QSettings().remove(kAutoAnalyse);
    Rig rig;
    rig.load(1, "/m/new.mp3");

    QVERIFY(rig.waitFor(1, "none"));
    QCOMPARE(rig.queue.entry("/m/new.mp3").state, QString());
    QCOMPARE(rig.runs(), 0);
}

void EdmStructure_Test::analyseByHandRunsOnceAndReachesTheDeck()
{
    QSettings().setValue(kAutoAnalyse, false);
    Rig rig;
    rig.master(1);
    rig.load(1, "/m/new.mp3");
    QVERIFY(rig.waitFor(1, "none"));
    rig.at(1, 5.5);

    rig.structure.analyse("/m/new.mp3");
    rig.structure.analyse("/m/new.mp3");
    QVERIFY(rig.state(1) == "queued" || rig.state(1) == "analysing");
    QVERIFY(rig.waitFor(1, "ready"));
    rig.structure.analyse("/m/new.mp3");
    QCOMPARE(rig.runs(), 1);

    rig.at(1, 6.0);   // the deck plays on: its first sample after the analysis reports the section
    QCOMPARE(rig.advanced, QStringList{"1:S1@4 active"});
}

void EdmStructure_Test::leftDeckGetsNoStaleState()
{
    Rig rig;
    rig.load(1, "/m/new.mp3");
    rig.eject(1);
    QThreadPool::globalInstance()->waitForDone();
    QCoreApplication::processEvents();
    QCOMPARE(rig.state(1), QString("none"));
    QCOMPARE(rig.queue.entry("/m/new.mp3").state, QString());

    rig.load(1, "/m/new.mp3");
    rig.load(1, "/m/a.mp3");
    QThreadPool::globalInstance()->waitForDone();
    QCoreApplication::processEvents();
    QCOMPARE(rig.state(1), QString("ready"));
    QCOMPARE(rig.queue.entry("/m/new.mp3").state, QString());
    QCOMPARE(rig.runs(), 0);
}

void EdmStructure_Test::ejectDuringRunKeepsDeckEmpty()
{
    Rig rig;
    writeFile(rig.gate, "");
    rig.load(1, "/m/new.mp3");
    QVERIFY(rig.waitFor(1, "analysing"));

    rig.eject(1);
    QCOMPARE(rig.state(1), QString("none"));
    QFile::remove(rig.gate);
    QVERIFY(rig.waitForQueue("/m/new.mp3", "ready"));

    QCOMPARE(rig.state(1), QString("none"));
    QCOMPARE(rig.structure.deck(1)->property("sections").toList().size(), 0);
    rig.load(1, "/m/new.mp3");
    QCOMPARE(rig.state(1), QString("ready"));
}

void EdmStructure_Test::queueRunsInOrderOnce()
{
    Rig rig;
    writeFile(rig.gate, "");
    QStringList order;
    connect(&rig.queue, &EdmAnalysisQueue::entryChanged, this, [&](const QString &path) {
        if (rig.queue.entry(path).state == "analysing" && order.contains(path) == false)
            order << path;
    });

    rig.queue.enqueue("/m/x.mp3");
    rig.queue.enqueue("/m/y.mp3");
    rig.queue.enqueue("/m/x.mp3");
    QCOMPARE(rig.queue.entry("/m/y.mp3").state, QString("queued"));
    QFile::remove(rig.gate);
    QVERIFY(rig.waitForQueue("/m/y.mp3", "ready"));

    QCOMPARE(order, (QStringList{"/m/x.mp3", "/m/y.mp3"}));
    QCOMPARE(rig.runs(), 2);
}

void EdmStructure_Test::blockedQueueWaits()
{
    Rig rig;
    rig.queue.setBlocked(true);
    rig.queue.enqueue("/m/x.mp3");
    QCOMPARE(rig.queue.entry("/m/x.mp3").state, QString("queued"));
    QCOMPARE(rig.queue.running(), false);

    rig.queue.setBlocked(false);
    QVERIFY(rig.waitForQueue("/m/x.mp3", "ready"));
}


void EdmStructure_Test::lookDecision_data()
{
    QTest::addColumn<bool>("autoOn");
    QTest::addColumn<QList<quint32>>("on");
    QTest::addColumn<bool>("overridden");
    QTest::addColumn<bool>("activeDeck");
    QTest::addColumn<bool>("sectionChanged");
    QTest::addColumn<bool>("becameActive");
    QTest::addColumn<QString>("label");
    QTest::addColumn<quint32>("press");
    QTest::addColumn<QList<quint32>>("release");
    QTest::addColumn<bool>("newOverridden");

    const quint32 none = kNoLookWidget;
    using L = QList<quint32>;
    //                                     auto    on        ovr      act     changed became  label
    QTest::newRow("section change")        << true << L{2} << false << true << true << false << "drop" << 3u << L{2} << false;
    QTest::newRow("first look")            << true << L{} << false << true << true << false << "intro" << 1u << L{} << false;
    QTest::newRow("unmapped label")        << true << L{2} << false << true << true << false << "groove" << none << L{} << false;
    QTest::newRow("became active")         << true << L{} << false << true << false << true << "drop" << 3u << L{} << false;
    QTest::newRow("not the active deck")   << true << L{2} << false << false << true << false << "drop" << none << L{} << false;
    QTest::newRow("auto off")              << false << L{2} << false << true << true << false << "drop" << none << L{} << false;
    QTest::newRow("same section")          << true << L{2} << false << true << false << false << "drop" << none << L{} << false;
    QTest::newRow("overridden, same section") << true << L{2} << true << true << false << false << "drop" << none << L{} << true;
    QTest::newRow("override, section change: the DJ's look is released") << true << L{2} << true << true << true << false << "drop" << 3u << L{2} << false;
    QTest::newRow("override, unmapped section") << true << L{2} << true << true << true << false << "groove" << none << L{} << false;
    QTest::newRow("same button twice")     << true << L{2} << false << true << true << false << "breakdown" << none << L{} << false;
    QTest::newRow("two stray looks on")    << true << L{1, 2} << false << true << true << false << "drop" << 3u << L{1, 2} << false;
    QTest::newRow("target on and another on") << true << L{2, 3} << false << true << true << false << "drop" << none << L{2} << false;
}

void EdmStructure_Test::lookDecision()
{
    QFETCH(bool, autoOn);
    QFETCH(QList<quint32>, on);
    QFETCH(bool, overridden);
    QFETCH(bool, activeDeck);
    QFETCH(bool, sectionChanged);
    QFETCH(bool, becameActive);
    QFETCH(QString, label);
    QFETCH(quint32, press);
    QFETCH(QList<quint32>, release);
    QFETCH(bool, newOverridden);

    EdmStructureMapping mapping;
    mapLooks(mapping, autoOn);
    EdmLookState state;
    state.overridden = overridden;
    EdmLookEvent event;
    event.activeDeck = activeDeck;
    event.sectionChanged = sectionChanged;
    event.becameActive = becameActive;
    event.label = label;
    event.mappedOn = on;

    const EdmLookDecision d = edmLookDecide(mapping, state, event);

    QCOMPARE(d.press, press);
    QCOMPARE(d.release, release);
    QCOMPARE(d.state.overridden, newOverridden);
}

void EdmStructure_Test::externalChange_data()
{
    QTest::addColumn<bool>("autoOn");
    QTest::addColumn<quint32>("widget");
    QTest::addColumn<bool>("overridden");

    QTest::newRow("a look button") << true << 3u << true;
    QTest::newRow("another button") << true << 77u << false;
    QTest::newRow("auto off") << false << 3u << false;
}

void EdmStructure_Test::externalChange()
{
    QFETCH(bool, autoOn);
    QFETCH(quint32, widget);
    QFETCH(bool, overridden);

    EdmStructureMapping mapping;
    mapLooks(mapping, autoOn);

    QCOMPARE(edmLookExternalChange(mapping, EdmLookState(), widget).overridden, overridden);
}

void EdmStructure_Test::mappingXmlRoundTrip()
{
    EdmStructureMapping mapping;
    mapLooks(mapping);
    mapping.setWidget("chorus", 9);     // a label this version does not know
    mapping.setWidget("groove", 999);   // a widget the workspace no longer has

    QString xml;
    QXmlStreamWriter writer(&xml);
    mapping.saveXML(&writer);

    EdmStructureMapping loaded;
    QXmlStreamReader reader(xml);
    QVERIFY(reader.readNextStartElement());
    QVERIFY(loaded.loadXML(reader));

    QCOMPARE(loaded.autoEnabled(), true);
    for (const QString &label : {"intro", "buildup", "drop", "breakdown", "groove", "outro", "chorus"})
        QCOMPARE(loaded.widget(label), mapping.widget(label));
    QCOMPARE(loaded.widget("chorus"), 9u);
    QCOMPARE(loaded.widgetId("none"), -1);
}

void EdmStructure_Test::mappingLoadKeepsUnknownAndSkipsBroken()
{
    EdmStructureMapping mapping;
    mapping.setWidget("drop", 5);
    QXmlStreamReader reader(R"(<SongStructure Auto="0"><Look Label="drop" Widget="x"/><Look Widget="3"/><Look Label="outro" Widget="4"/></SongStructure>)");
    QVERIFY(reader.readNextStartElement());
    QVERIFY(mapping.loadXML(reader));

    QCOMPARE(mapping.autoEnabled(), false);
    QCOMPARE(mapping.widget("drop"), kNoLookWidget);
    QCOMPARE(mapping.widget("outro"), 4u);

    QXmlStreamReader other("<Other/>");
    QVERIFY(other.readNextStartElement());
    QVERIFY(mapping.loadXML(other) == false);
}

void EdmStructure_Test::newWorkspaceClearsMapping()
{
    Doc doc(this);
    EdmStructureMapping mapping(&doc);
    mapLooks(mapping);

    emit doc.cleared();

    QCOMPARE(mapping.autoEnabled(), false);
    QCOMPARE(mapping.widget("drop"), kNoLookWidget);
}

void EdmStructure_Test::directorPressesTheLookOfEachSection()
{
    Rig rig;
    mapLooks(rig.mapping);
    rig.master(1);
    rig.load(1, "/m/a.mp3");

    rig.at(1, 0.0);
    rig.at(1, 2.0);
    QCOMPARE(rig.surface.log, QStringList{"on1"});
    rig.at(1, 4.5);
    QCOMPARE(rig.surface.log, (QStringList{"on1", "off1", "on2"}));
    rig.at(1, 8.5);
    QCOMPARE(rig.surface.log, (QStringList{"on1", "off1", "on2", "off2", "on3"}));
    QCOMPARE(rig.director.followedDeck(), 1);
    QCOMPARE(rig.director.followedLabel(), QString("drop"));

    rig.at(1, 3.0);   // a jump back: no look
    QCOMPARE(rig.surface.log.size(), 5);
}

void EdmStructure_Test::directorHoldsAfterTheDjChangesALook()
{
    Rig rig;
    mapLooks(rig.mapping);
    rig.master(1);
    rig.load(1, "/m/a.mp3");
    rig.at(1, 4.5);
    rig.surface.log.clear();

    rig.surface.external(2, false);
    QCOMPARE(rig.director.overridden(), true);
    rig.at(1, 6.0);
    QCOMPARE(rig.surface.log, QStringList());

    rig.at(1, 8.5);
    QCOMPARE(rig.surface.log, QStringList{"on3"});
    QCOMPARE(rig.director.overridden(), false);
}

void EdmStructure_Test::directorKeepsTheOverrideThroughLoops()
{
    Rig rig;
    mapLooks(rig.mapping);
    rig.master(1);
    rig.load(1, "/m/a.mp3");
    rig.at(1, 4.5);
    rig.surface.external(4, true);   // the DJ's own look
    rig.surface.log.clear();

    for (int wrap = 0; wrap < 3; wrap++)
    {
        rig.at(1, 7.0);
        rig.at(1, 5.0);
    }
    QCOMPARE(rig.director.overridden(), true);
    QCOMPARE(rig.surface.log, QStringList());

    rig.at(1, 8.5);
    QCOMPARE(rig.director.overridden(), false);
    QCOMPARE(rig.surface.log, (QStringList{"off2", "off4", "on3"}));
}

void EdmStructure_Test::directorReleasesEveryOtherLook()
{
    Rig rig;
    mapLooks(rig.mapping);
    rig.master(1);
    rig.load(1, "/m/a.mp3");
    rig.at(1, 4.5);
    rig.surface.external(1, true);   // two looks on, nothing like a solo frame to stop one
    rig.surface.log.clear();

    rig.at(1, 8.5);

    QCOMPARE(rig.surface.log, (QStringList{"off1", "off2", "on3"}));
    QCOMPARE(rig.surface.isOn(1), false);
    QCOMPARE(rig.surface.isOn(2), false);
    QCOMPARE(rig.surface.isOn(3), true);
}

void EdmStructure_Test::directorIgnoresLifts()
{
    Rig rig;
    mapLooks(rig.mapping);
    rig.master(1);
    rig.load(1, "/m/lift.mp3");
    rig.at(1, 4.5);
    rig.advanced.clear();
    rig.surface.log.clear();

    rig.at(1, 7.0);   // crosses the lift on bar 6

    QCOMPARE(rig.advanced, QStringList{"1:E0@6 active"});
    QCOMPARE(rig.surface.log, QStringList());
}

void EdmStructure_Test::directorIgnoresStateChangesThatKeepTheLookOn()
{
    Rig rig;
    mapLooks(rig.mapping);
    rig.master(1);
    rig.load(1, "/m/a.mp3");
    rig.at(1, 4.5);

    rig.surface.external(2, true);   // reported again, still on
    rig.surface.external(3, false);  // another look reporting while off

    QCOMPARE(rig.director.overridden(), false);
}

void EdmStructure_Test::directorFollowsOnlyTheActiveDeck()
{
    Rig rig;
    mapLooks(rig.mapping);
    rig.master(1);
    rig.load(1, "/m/a.mp3");
    rig.load(2, "/m/b.mp3");
    rig.at(1, 4.5);
    rig.at(2, 9.0);
    QCOMPARE(rig.surface.log, QStringList{"on2"});

    rig.at(2, 17.0);
    QCOMPARE(rig.surface.log.size(), 1);

    rig.master(2);
    QCOMPARE(rig.surface.log.mid(1), (QStringList{"off2", "on4"}));
    QCOMPARE(rig.director.followedDeck(), 2);
}

void EdmStructure_Test::directorChangesNothingWithoutAnalysisOrAuto_data()
{
    QTest::addColumn<bool>("autoOn");
    QTest::addColumn<QString>("path");

    QTest::newRow("auto off") << false << "/m/a.mp3";
    QTest::newRow("song without analysis") << true << "/m/new.mp3";
}

void EdmStructure_Test::directorChangesNothingWithoutAnalysisOrAuto()
{
    QFETCH(bool, autoOn);
    QFETCH(QString, path);

    QSettings().setValue(kAutoAnalyse, false);
    Rig rig;
    mapLooks(rig.mapping, autoOn);
    rig.master(1);
    rig.load(1, path);
    rig.at(1, 4.5);
    rig.at(1, 9.0);
    QThreadPool::globalInstance()->waitForDone();
    QCoreApplication::processEvents();
    rig.at(1, 12.0);

    QCOMPARE(rig.surface.log, QStringList());
}

void EdmStructure_Test::directorLeavesLightsWhenTheDeckIsEjected()
{
    Rig rig;
    mapLooks(rig.mapping);
    rig.master(1);
    rig.load(1, "/m/a.mp3");
    rig.at(1, 4.5);
    rig.surface.log.clear();

    rig.eject(1);

    QCOMPARE(rig.surface.log, QStringList());
    QCOMPARE(rig.surface.isOn(2), true);
    QCOMPARE(rig.director.followedDeck(), 0);
}

void EdmStructure_Test::destroyingParentWhileRunningDoesNotCallToolAgain()
{
    QTemporaryDir root;
    QObject *parent = new QObject;
    QObject *manager = new QObject(parent);   // the App creates the page before the queue
    EdmAnalysisQueue *queue = new EdmAnalysisQueue(root.filePath("analysis"), parent);
    const QPointer<QObject> watched(manager);
    int calls = 0;
    bool calledWithoutManager = false;
    // sh -c keeps the tool running; the queue appends its own arguments after the command
    queue->setTool([&]() {
        calls++;
        calledWithoutManager = calledWithoutManager || watched.isNull();
        return QStringList{"/bin/sh", "-c", "sleep 30"};
    }, QProcessEnvironment::systemEnvironment());

    queue->enqueue("/m/x.mp3");
    queue->enqueue("/m/y.mp3");
    QVERIFY(queue->running());
    QCOMPARE(calls, 1);

    delete parent;

    QVERIFY2(calledWithoutManager == false, qPrintable(QString("the tool command ran %1 times, once after its owner was gone").arg(calls)));
}

void EdmStructure_Test::readyEntryWithMissingFileCanBeRetried()
{
    Rig rig;
    rig.load(1, "/m/new.mp3");
    rig.structure.analyse("/m/new.mp3");
    QVERIFY(rig.waitFor(1, "ready"));
    QVERIFY(QFile::remove(rig.dir + "/final/cccccccccccccccc.json"));
    rig.queue.setBlocked(true);

    rig.structure.retry("/m/new.mp3");

    QCOMPARE(rig.queue.entry("/m/new.mp3").state, QString("queued"));
}

void EdmStructure_Test::analysisWrittenOutsideTheQueueReachesTheDeck()
{
    QSettings().setValue(kAutoAnalyse, false);
    Rig rig;
    rig.load(1, "/m/new.mp3");
    QVERIFY(rig.waitFor(1, "none"));

    // the Audio Analysis page (or a run from the shell) writes the song's files
    writeFile(rig.dir + "/final/eeeeeeeeeeeeeeee.json", kFinal.toUtf8());
    writeFile(rig.dir + "/index.json", R"({"aaaaaaaaaaaaaaaa": {"path": "/m/a.mp3"}, "eeeeeeeeeeeeeeee": {"path": "/m/new.mp3"}})");
    QFile index(rig.dir + "/index.json");
    QVERIFY(index.open(QIODevice::ReadWrite));
    QVERIFY(index.setFileTime(QDateTime::currentDateTime().addSecs(60), QFileDevice::FileModificationTime));
    index.close();

    rig.structure.analyse("/m/new.mp3");

    QCOMPARE(rig.state(1), QString("ready"));
}

void EdmStructure_Test::deckCountDecreaseLosesTheFollowedDeck()
{
    Rig rig;
    mapLooks(rig.mapping);
    rig.fsm.setDeckCount(4);
    rig.master(3);
    rig.load(3, "/m/a.mp3");
    rig.at(3, 4.5);
    QCOMPARE(rig.director.followedDeck(), 3);
    rig.lost.clear();

    rig.fsm.setDeckCount(2);

    QCOMPARE(rig.lost, QStringList{"3:active"});
    QCOMPARE(rig.director.followedDeck(), 0);
}

void EdmStructure_Test::directorKeepsFollowingWhenTheOldMasterIsEjectedInTheSameWrite()
{
    Rig rig;
    mapLooks(rig.mapping);
    rig.master(2);
    rig.load(1, "/m/a.mp3");
    rig.load(2, "/m/b.mp3");
    rig.at(1, 4.5);
    rig.at(2, 9.0);
    QCOMPARE(rig.director.followedDeck(), 2);

    // one write: deck 2 ejected, deck 1 the master
    rig.fsm.onDeckTrigger(2, "loaded", "off");
    rig.fsm.onDeckTrigger(2, "masterdeck", "off");
    rig.fsm.onDeckTrigger(1, "masterdeck", "on");
    rig.fsm.onTickEnded();

    QCOMPARE(rig.director.followedDeck(), 1);
}

void EdmStructure_Test::directorSeesReleaseOfButtonOnBeforeWatch_data()
{
    QTest::addColumn<bool>("newWorkspace");

    QTest::newRow("onBeforeWatch") << false;
    QTest::newRow("clearedWorkspace") << true;
}

void EdmStructure_Test::directorSeesReleaseOfButtonOnBeforeWatch()
{
    QFETCH(bool, newWorkspace);

    Rig rig;
    if (newWorkspace)
    {
        // workspace A: the director turns look 3 on
        mapLooks(rig.mapping);
        rig.master(1);
        rig.load(1, "/m/a.mp3");
        rig.at(1, 9.0);
        QCOMPARE(rig.surface.isOn(3), true);

        // workspace B: new buttons with the same ids, all off, the same mapping loaded
        rig.surface.on.clear();
        QString xml;
        QXmlStreamWriter writer(&xml);
        rig.mapping.saveXML(&writer);
        QXmlStreamReader reader(xml);
        QVERIFY(reader.readNextStartElement());
        QVERIFY(rig.mapping.loadXML(reader));

        rig.surface.external(3, true);   // the DJ presses look 3 in workspace B
    }
    else
    {
        rig.surface.on[3] = true;         // a look already on when the director starts watching
        mapLooks(rig.mapping);
        rig.surface.external(3, false);  // the DJ turns it off
    }

    QCOMPARE(rig.director.overridden(), true);
}

void EdmStructure_Test::nonMp3SongIsNotQueued()
{
    QSettings().setValue(kAutoAnalyse, false);
    Rig rig;
    // the real tool skips files that are not MP3: it reports no track and exits 0
    const QString script = rig.root.filePath("mp3only.sh");
    writeFile(script, QString(R"(#!/bin/sh
echo run >> %1
echo "{\"event\":\"start\",\"total\":0,\"out\":\"$4\"}"
echo "{\"event\":\"finished\",\"total\":0,\"done\":0,\"failed\":0,\"out\":\"$4\"}"
)").arg(rig.log).toUtf8());
    rig.queue.setTool([script]() { return QStringList{"/bin/sh", script}; }, QProcessEnvironment::systemEnvironment());
    rig.load(1, "/m/song.m4a");
    QVERIFY(rig.waitFor(1, "none"));

    rig.structure.analyse("/m/song.m4a");
    QSignalSpy entries(&rig.queue, &EdmAnalysisQueue::entryChanged);
    while (rig.queue.running() || rig.queue.entry("/m/song.m4a").state == "queued")
        QVERIFY(entries.wait(5000));

    QCOMPARE(rig.runs(), 0);
    QVERIFY2(rig.state(1) != "failed" || rig.structure.deck(1)->property("error").toString().contains("exit code 0") == false,
             qPrintable(rig.structure.deck(1)->property("error").toString()));
}

void EdmStructure_Test::newSongClearsLiveValues()
{
    Rig rig;
    rig.master(1);
    rig.load(1, "/m/a.mp3");
    rig.at(1, 12.0);
    QCOMPARE(rig.structure.deck(1)->property("section").toInt(), 2);

    // B's filepath alone: B's first position sample is still ahead
    rig.fsm.onDeckTrigger(1, "get_filepath", "/m/b.mp3");
    rig.fsm.onTickEnded();
    QCOMPARE(rig.state(1), QString("ready"));

    QCOMPARE(rig.structure.deck(1)->property("section").toInt(), -1);
    QCOMPARE(rig.prop(1, "bar"), 0.0);
}

void EdmStructure_Test::rewrittenFinalFileReachesTheDeck()
{
    Rig rig;
    rig.load(1, "/m/a.mp3");
    QCOMPARE(rig.state(1), QString("ready"));

    // `edm-analysis confidence` or a rebuild rewrites only final/<id>.json
    QString relabelled = kFinal;
    relabelled.replace(R"("label": "intro")", R"("label": "breakdown")");
    writeFile(rig.dir + "/final/aaaaaaaaaaaaaaaa.json", relabelled.toUtf8());
    QFile final(rig.dir + "/final/aaaaaaaaaaaaaaaa.json");
    QVERIFY(final.open(QIODevice::ReadWrite));
    QVERIFY(final.setFileTime(QDateTime::currentDateTime().addSecs(60), QFileDevice::FileModificationTime));
    final.close();

    QSignalSpy changed(rig.structure.deck(1), SIGNAL(staticChanged()));
    auto firstLabel = [&]() { return rig.structure.deck(1)->property("sections").toList().first().toMap().value("label").toString(); };
    while (firstLabel() != "breakdown")
        QVERIFY2(changed.wait(3000), qPrintable("the deck still shows " + firstLabel()));
}

void EdmStructure_Test::analysisFolderChangesAreCoalesced()
{
    // no Rig: nothing writes into this folder, so no late file events from the OS reach the test
    QTemporaryDir root;
    DjFsm fsm;
    EdmAnalysisQueue queue(root.filePath("analysis"));
    EdmStructure structure(&fsm, &queue, root.filePath("analysis"));
    QSignalSpy refreshed(&structure, &EdmStructure::analysisFilesChanged);

    // a batch run: a change every 100 ms for 800 ms, longer than the 500 ms wait
    for (int i = 0; i < 8; i++)
    {
        QVERIFY(QMetaObject::invokeMethod(&structure, "slotFolderChanged"));
        QTest::qWait(100);
    }
    QCOMPARE(refreshed.count(), 0);
    QVERIFY(refreshed.wait(3000));
    QVERIFY(refreshed.wait(1000) == false);

    QCOMPARE(refreshed.count(), 1);
}

void EdmStructure_Test::readyFileWinsOverAFailedEntryInTheSongList()
{
    Rig rig;
    writeFile(rig.failFlag, "");
    rig.queue.enqueue("/m/x.mp3");
    QVERIFY(rig.waitForQueue("/m/x.mp3", "failed"));

    // the page or a shell run analyses the song after all
    writeFile(rig.dir + "/final/eeeeeeeeeeeeeeee.json", kFinal.toUtf8());
    writeFile(rig.dir + "/index.json", R"({"aaaaaaaaaaaaaaaa": {"path": "/m/a.mp3"}, "eeeeeeeeeeeeeeee": {"path": "/m/x.mp3"}})");
    QFile index(rig.dir + "/index.json");
    QVERIFY(index.open(QIODevice::ReadWrite));
    QVERIFY(index.setFileTime(QDateTime::currentDateTime().addSecs(60), QFileDevice::FileModificationTime));
    index.close();
    QSignalSpy rows(&rig.structure, &EdmStructure::analysisStateChanged);

    rig.structure.analyse("/m/x.mp3");

    QCOMPARE(rig.structure.analysisStateForPath("/m/x.mp3"), QString("ready"));
    QVERIFY2(rows.isEmpty() == false, "the song list is not told to read the states again");
}

void EdmStructure_Test::newSongOnTheDeckTriggersItsSection()
{
    Rig rig;
    mapLooks(rig.mapping);
    rig.master(1);
    rig.load(1, "/m/a.mp3");
    rig.at(1, 4.5);
    rig.at(1, 8.5);                  // the drop look
    rig.surface.external(4, true);   // the DJ's own look
    QCOMPARE(rig.director.overridden(), true);

    // song B on the same deck, its first sample in its drop: the same section index as before
    rig.load(1, "/m/b.mp3");
    rig.at(1, 8.5);

    QCOMPARE(rig.director.overridden(), false);
}

void EdmStructure_Test::directorPressesOnlyOnForwardSectionChanges_data()
{
    QTest::addColumn<QList<double>>("bars");
    QTest::addColumn<bool>("overridden");
    QTest::addColumn<QStringList>("presses");

    const QStringList dropAgain = {"off4", "on3"};
    QTest::newRow("1-bar loop over the buildup/drop boundary") << QList<double>{23.5, 24.5, 23.5, 24.5, 23.5, 24.5} << false << dropAgain;
    QTest::newRow("cue back 16 bars, then play into the drop") << QList<double>{8.5, 23.0, 24.5} << false << dropAgain;
    QTest::newRow("backward jump into another section") << QList<double>{8.5} << true << QStringList();
    QTest::newRow("a loop inside the drop") << QList<double>{28.0, 25.0, 28.0, 25.0} << true << QStringList();
}

void EdmStructure_Test::directorPressesOnlyOnForwardSectionChanges()
{
    QFETCH(QList<double>, bars);
    QFETCH(bool, overridden);
    QFETCH(QStringList, presses);

    Rig rig;
    mapLooks(rig.mapping);
    rig.master(1);
    rig.load(1, "/m/long.mp3");
    rig.at(1, 20.0);
    rig.at(1, 24.5);                 // the drop look
    rig.surface.external(3, false);  // the DJ takes the drop look off
    rig.surface.external(4, true);   // and puts their own on
    rig.surface.log.clear();

    for (double bar : bars)
        rig.at(1, bar);

    QCOMPARE(rig.director.overridden(), overridden);
    QCOMPARE(rig.surface.log, presses);
}

QTEST_GUILESS_MAIN(EdmStructure_Test)
