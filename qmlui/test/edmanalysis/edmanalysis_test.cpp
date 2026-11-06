/*
  Q Light Controller Plus - Unit test
  edmanalysis_test.cpp

  Licensed under the Apache License, Version 2.0
*/

#include "edmanalysis_test.h"
#include "edmanalysisprogress.h"
#include "edmanalysisreader.h"

#include <QTemporaryDir>

static EdmProgress feed(const QStringList &lines)
{
    EdmProgress p;
    for (const QString &line : lines)
        p = applyEdmLine(p, line.toUtf8());
    return p;
}

static const QString kStart = R"({"event":"start","total":2,"out":"/tmp/out"})";
static const QString kStartA = R"({"event":"track_start","index":0,"total":2,"path":"/m/A - One [128 1A].mp3"})";
static const QString kStageA = R"({"event":"stage","index":0,"total":2,"path":"/m/A - One [128 1A].mp3","stage":"beats"})";
static const QString kDoneA = R"({"event":"track_done","index":0,"total":2,"path":"/m/A - One [128 1A].mp3","id":"abcd","reused":null,"seconds":11.2,"bpm":128.0,"summary":"intro buildup drop outro"})";
static const QString kStartB = R"({"event":"track_start","index":1,"total":2,"path":"/m/B - Two.mp3"})";
static const QString kErrorB = R"({"event":"track_error","index":1,"total":2,"path":"/m/B - Two.mp3","error":"CalledProcessError: ffmpeg"})";
static const QString kFinished = R"({"event":"finished","total":2,"done":1,"failed":1,"out":"/tmp/out"})";

void EdmAnalysis_Test::applyEventSequence_data()
{
    QTest::addColumn<QStringList>("lines");
    QTest::addColumn<int>("done");
    QTest::addColumn<int>("failed");
    QTest::addColumn<bool>("finished");
    QTest::addColumn<int>("current");
    QTest::addColumn<QString>("statusA");
    QTest::addColumn<QString>("stageA");
    QTest::addColumn<QString>("statusB");

    QTest::newRow("started") << QStringList{kStart} << 0 << 0 << false << -1 << "queued" << "" << "queued";
    QTest::newRow("first track running") << QStringList{kStart, kStartA, kStageA} << 0 << 0 << false << 0 << "running" << "beats" << "queued";
    QTest::newRow("first done") << QStringList{kStart, kStartA, kStageA, kDoneA} << 1 << 0 << false << -1 << "done" << "" << "queued";
    QTest::newRow("second failed, finished")
        << QStringList{kStart, kStartA, kDoneA, kStartB, kErrorB, kFinished} << 1 << 1 << true << -1 << "done" << "" << "error";
}

void EdmAnalysis_Test::applyEventSequence()
{
    QFETCH(QStringList, lines);
    QFETCH(int, done);
    QFETCH(int, failed);
    QFETCH(bool, finished);
    QFETCH(int, current);
    QFETCH(QString, statusA);
    QFETCH(QString, stageA);
    QFETCH(QString, statusB);

    EdmProgress p = feed(lines);

    QCOMPARE(p.total, 2);
    QCOMPARE(p.outDir, QString("/tmp/out"));
    QCOMPARE(p.done, done);
    QCOMPARE(p.failed, failed);
    QCOMPARE(p.finished, finished);
    QCOMPARE(p.current, current);
    QCOMPARE(p.tracks.size(), 2);
    QCOMPARE(p.tracks[0].status, statusA);
    QCOMPARE(p.tracks[0].stage, stageA);
    QCOMPARE(p.tracks[1].status, statusB);
}

void EdmAnalysis_Test::unknownOrBrokenLinesKeepState_data()
{
    QTest::addColumn<QString>("line");

    QTest::newRow("not json") << "Downloading model 12%";
    QTest::newRow("unknown event") << R"({"event":"telemetry","x":1})";
    QTest::newRow("index out of range") << R"({"event":"track_done","index":7,"bpm":120})";
    QTest::newRow("empty") << "";
}

void EdmAnalysis_Test::unknownOrBrokenLinesKeepState()
{
    QFETCH(QString, line);

    EdmProgress before = feed({kStart, kStartA, kDoneA});
    EdmProgress after = applyEdmLine(before, line.toUtf8());

    QCOMPARE(after.done, before.done);
    QCOMPARE(after.tracks.size(), before.tracks.size());
    QCOMPARE(after.tracks[0].status, QString("done"));
}

void EdmAnalysis_Test::trackFieldsAndArguments()
{
    EdmProgress p = feed({kStart, kStartA, kDoneA});
    QCOMPARE(p.tracks[0].title, QString("A - One [128 1A]"));
    QCOMPARE(p.tracks[0].bpm, 128.0);
    QCOMPARE(p.tracks[0].summary, QString("intro buildup drop outro"));
    QCOMPARE(p.tracks[0].id, QString("abcd"));

    QStringList args = edmAnalyzeArguments("/music/folder", "/tmp/out");
    QCOMPARE(args, (QStringList{"analyze", "/music/folder", "--out", "/tmp/out", "--jsonl"}));
}

static void writeFile(const QString &path, const QByteArray &data)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

void EdmAnalysis_Test::readerFindsReviewedSections_data()
{
    QTest::addColumn<QString>("audioPath");
    QTest::addColumn<int>("count");
    QTest::addColumn<QString>("firstLabel");

    QTest::newRow("analysed track") << "/music/a.mp3" << 2 << "intro";
    QTest::newRow("unknown track") << "/music/b.mp3" << 0 << "";
    QTest::newRow("no audio") << "" << 0 << "";
}

void EdmAnalysis_Test::readerFindsReviewedSections()
{
    QFETCH(QString, audioPath);
    QFETCH(int, count);
    QFETCH(QString, firstLabel);

    QTemporaryDir dir;
    QDir().mkpath(dir.filePath("final"));
    writeFile(dir.filePath("index.json"), R"({"abababababababab": {"title": "a", "path": "/music/a.mp3"}})");
    writeFile(dir.filePath("final/abababababababab.json"), R"({"schema": "qlcplus-edm-final/1", "sections": [
        {"label": "intro", "start": 0.5, "end": 30.0, "reviewed": false},
        {"label": "drop", "start": 30.0, "end": 90.25, "reviewed": true}]})");

    EdmAnalysisReader reader(dir.path());
    QVariantList sections = reader.sections(audioPath);

    QCOMPARE(sections.size(), count);
    if (count > 0)
    {
        QVariantMap first = sections.first().toMap();
        QCOMPARE(first.value("label").toString(), firstLabel);
        QCOMPARE(first.value("startMs").toDouble(), 500.0);
        QCOMPARE(sections.last().toMap().value("endMs").toDouble(), 90250.0);
        QCOMPARE(sections.last().toMap().value("reviewed").toBool(), true);
    }
}

QTEST_GUILESS_MAIN(EdmAnalysis_Test)
