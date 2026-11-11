/*
  Q Light Controller Plus - Unit test
  edmanalysis_test.cpp

  Licensed under the Apache License, Version 2.0
*/

#include "edmanalysis_test.h"
#include "edmanalysisprogress.h"
#include "edmanalysisreader.h"
#include "edmsongposition.h"

#include <QCryptographicHash>
#include <QFileDevice>

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

void EdmAnalysis_Test::readerFindsSections_data()
{
    QTest::addColumn<QString>("audioPath");
    QTest::addColumn<int>("count");
    QTest::addColumn<QString>("firstLabel");

    QTest::newRow("analysed track") << "/music/a.mp3" << 2 << "intro";
    QTest::newRow("unknown track") << "/music/b.mp3" << 0 << "";
    QTest::newRow("no audio") << "" << 0 << "";
}

void EdmAnalysis_Test::readerFindsSections()
{
    QFETCH(QString, audioPath);
    QFETCH(int, count);
    QFETCH(QString, firstLabel);

    QTemporaryDir dir;
    QDir().mkpath(dir.filePath("final"));
    writeFile(dir.filePath("index.json"), R"({"abababababababab": {"title": "a", "path": "/music/a.mp3"}})");
    writeFile(dir.filePath("final/abababababababab.json"), R"({"schema": "qlcplus-edm-final/2", "grid": {"bpm": 120.0, "period": 0.5, "beats_per_bar": 4, "first_downbeat": 0.5}, "sections": [
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
    }
}

void EdmAnalysis_Test::readerSectionConfidence_data()
{
    QTest::addColumn<QString>("sectionJson");
    QTest::addColumn<int>("confidence");
    QTest::addColumn<int>("labelConfidence");

    QTest::newRow("schema 2 with both values")
        << R"({"label": "drop", "start_bar": 17, "end_bar": 33, "start": 30.0, "end": 90.25, "confidence": 85, "label_confidence": 40})"
        << 85 << 40;
    QTest::newRow("zero is a value, not unknown")
        << R"({"label": "drop", "start": 30.0, "end": 90.25, "confidence": 0, "label_confidence": 100})"
        << 0 << 100;
    QTest::newRow("schema 2 without values")
        << R"({"label": "drop", "start_bar": 17, "end_bar": 33, "start": 30.0, "end": 90.25})"
        << 50 << 50;
    QTest::newRow("only one value")
        << R"({"label": "drop", "start": 30.0, "end": 90.25, "label_confidence": 70})"
        << 50 << 70;
    QTest::newRow("old file with reviewed")
        << R"({"label": "drop", "start": 30.0, "end": 90.25, "reviewed": true})"
        << 50 << 50;
}

void EdmAnalysis_Test::readerSectionConfidence()
{
    QFETCH(QString, sectionJson);
    QFETCH(int, confidence);
    QFETCH(int, labelConfidence);

    QTemporaryDir dir;
    QDir().mkpath(dir.filePath("final"));
    writeFile(dir.filePath("index.json"), R"({"abababababababab": {"title": "a", "path": "/music/a.mp3"}})");
    writeFile(dir.filePath("final/abababababababab.json"),
              QString(R"({"schema": "qlcplus-edm-final/2", "grid": {"bpm": 120.0, "period": 0.5, "beats_per_bar": 4, "first_downbeat": 0.5}, "sections": [%1]})").arg(sectionJson).toUtf8());

    EdmAnalysisReader reader(dir.path());
    QVariantList sections = reader.sections("/music/a.mp3");

    QCOMPARE(sections.size(), 1);
    QVariantMap section = sections.first().toMap();
    QCOMPARE(section.value("label").toString(), QString("drop"));
    QCOMPARE(section.value("startMs").toDouble(), 30000.0);
    QCOMPARE(section.value("endMs").toDouble(), 90250.0);
    QCOMPARE(section.value("confidence").toInt(), confidence);
    QCOMPARE(section.value("labelConfidence").toInt(), labelConfidence);
    QVERIFY(section.contains("confidence"));
    QVERIFY(section.contains("labelConfidence"));
    QVERIFY(section.contains("reviewed") == false);
}

static const QString kGrid = R"("grid": {"bpm": 120.0, "period": 0.5, "beats_per_bar": 4, "first_downbeat": 1.0})";
static const QString kSection = R"({"label": "intro", "start_bar": 0, "end_bar": 4, "start": 0.0, "end": 9.0})";
static const QString kEvent = R"({"kind": "drop", "bar": 8, "time": 17.0, "confidence": 80})";

static QByteArray document(const QString &schema, const QString &grid, const QString &sections, const QString &events,
                           const QString &extra = QString())
{
    return QString(R"({"schema": "%1", "track": {"title": "t", "sha256": "0123456789abcdef0123"}, %2, "sections": [%3], "events": [%4]%5})")
        .arg(schema, grid, sections, events, extra).toUtf8();
}

void EdmAnalysis_Test::parseDocument_data()
{
    QTest::addColumn<QByteArray>("bytes");
    QTest::addColumn<bool>("valid");
    QTest::addColumn<int>("schema");
    QTest::addColumn<int>("events");
    QTest::addColumn<int>("sectionConfidence");
    QTest::addColumn<int>("eventConfidence");
    QTest::addColumn<int>("gridConfidence");

    const QString full = R"({"label": "drop", "start_bar": 4, "end_bar": 12, "start": 9.0, "end": 25.0, "confidence": 85, "label_confidence": 40, "energy": 0.7})";
    const QString gridConf = R"("grid": {"bpm": 120.0, "period": 0.5, "beats_per_bar": 4, "first_downbeat": 1.0, "confidence": 92})";

    QTest::newRow("schema 2") << document("qlcplus-edm-final/2", gridConf, full, kEvent) << true << 2 << 1 << 85 << 80 << 92;
    QTest::newRow("schema 3 with unknown fields")
        << document("qlcplus-edm-final/3", gridConf, full.chopped(1) + R"(, "novel": [1, 2])" + "}", kEvent.chopped(1) + R"(, "extra": {"a": 1}})",
                    R"(, "stems": {"x": 1})")
        << true << 3 << 1 << 85 << 80 << 92;
    QTest::newRow("wrong schema") << document("qlcplus-edm-final/1", gridConf, full, kEvent) << false << 0 << 0 << 0 << 0 << 0;
    QTest::newRow("unknown schema") << document("something-else/2", gridConf, full, kEvent) << false << 0 << 0 << 0 << 0 << 0;
    QTest::newRow("broken json") << QByteArray(R"({"schema": "qlcplus-edm-final/2", "sec)") << false << 0 << 0 << 0 << 0 << 0;
    QTest::newRow("not an object") << QByteArray("[1, 2]") << false << 0 << 0 << 0 << 0 << 0;
    QTest::newRow("no grid") << document("qlcplus-edm-final/2", R"("x": 1)", kSection, kEvent) << false << 0 << 0 << 0 << 0 << 0;
    QTest::newRow("no sections") << document("qlcplus-edm-final/2", kGrid, "", kEvent) << false << 0 << 0 << 0 << 0 << 0;
    QTest::newRow("missing optional confidences") << document("qlcplus-edm-final/2", kGrid, kSection, kEvent) << true << 2 << 1 << 50 << 80 << 50;
    QTest::newRow("events without confidence")
        << document("qlcplus-edm-final/2", kGrid, kSection, R"({"kind": "vocal_in", "bar": 2, "time": 5.0})") << true << 2 << 1 << 50 << kEdmNoConfidence << 50;
}

void EdmAnalysis_Test::parseDocument()
{
    QFETCH(QByteArray, bytes);
    QFETCH(bool, valid);
    QFETCH(int, schema);
    QFETCH(int, events);
    QFETCH(int, sectionConfidence);
    QFETCH(int, eventConfidence);
    QFETCH(int, gridConfidence);

    const EdmAnalysis a = parseEdmAnalysis(bytes);

    QCOMPARE(a.valid, valid);
    if (valid == false)
        return;
    QCOMPARE(a.schema, schema);
    QCOMPARE(a.id, QString("0123456789abcdef"));
    QCOMPARE(a.title, QString("t"));
    QCOMPARE(a.grid.bpm, 120.0);
    QCOMPARE(a.grid.barMs, 2000.0);
    QCOMPARE(a.grid.beatsPerBar, 4);
    QCOMPARE(a.grid.firstDownbeatMs, 1000.0);
    QCOMPARE(a.grid.confidence, gridConfidence);
    QCOMPARE(a.sections.size(), 1);
    QCOMPARE(a.sections[0].confidence, sectionConfidence);
    QCOMPARE(a.events.size(), events);
    QCOMPARE(a.events[0].confidence, eventConfidence);
    QCOMPARE(a.events[0].bar, a.events[0].kind == "vocal_in" ? 2.0 : 8.0);
}

void EdmAnalysis_Test::confidenceIsRounded_data()
{
    QTest::addColumn<QString>("value");
    QTest::addColumn<int>("section");
    QTest::addColumn<int>("label");
    QTest::addColumn<int>("event");
    QTest::addColumn<int>("grid");

    QTest::newRow("fraction rounds up") << "77.5" << 78 << 78 << 78 << 78;
    QTest::newRow("fraction rounds down") << "77.4" << 77 << 77 << 77 << 77;
    QTest::newRow("integer") << "77" << 77 << 77 << 77 << 77;
    QTest::newRow("zero") << "0" << 0 << 0 << 0 << 0;
    QTest::newRow("missing") << "" << 50 << 50 << kEdmNoConfidence << 50;
}

void EdmAnalysis_Test::confidenceIsRounded()
{
    QFETCH(QString, value);
    QFETCH(int, section);
    QFETCH(int, label);
    QFETCH(int, event);
    QFETCH(int, grid);

    auto field = [&](const char *name) { return value.isEmpty() ? QString() : QString(R"(, "%1": %2)").arg(name, value); };
    const QString gridJson = QString(R"("grid": {"bpm": 120.0, "period": 0.5, "first_downbeat": 1.0%1})").arg(field("confidence"));
    const QString sectionJson = QString(R"({"label": "drop", "start": 0.0, "end": 9.0%1%2})").arg(field("confidence"), field("label_confidence"));
    const QString eventJson = QString(R"({"kind": "drop", "bar": 8, "time": 17.0%1})").arg(field("confidence"));

    const EdmAnalysis a = parseEdmAnalysis(document("qlcplus-edm-final/3", gridJson, sectionJson, eventJson));

    QVERIFY(a.valid);
    QCOMPARE(a.sections[0].confidence, section);
    QCOMPARE(a.sections[0].labelConfidence, label);
    QCOMPARE(a.events[0].confidence, event);
    QCOMPARE(a.grid.confidence, grid);
}

static void writeAnalysisDir(const QString &dir, const QString &audioPathInIndex, const QString &id, const QByteArray &final)
{
    QDir().mkpath(dir + "/final");
    writeFile(dir + "/index.json", QString(R"({"%1": {"title": "a", "path": "%2"}})").arg(id, audioPathInIndex).toUtf8());
    if (final.isEmpty() == false)
        writeFile(dir + "/final/" + id + ".json", final);
}

void EdmAnalysis_Test::lookupByPath_data()
{
    QTest::addColumn<QString>("relativePath");
    QTest::addColumn<bool>("found");

    QTest::newRow("same path as the index") << "music/a.mp3" << true;
    QTest::newRow("through a symlink") << "link/a.mp3" << true;
    QTest::newRow("missing file by its plain path") << "gone/b.mp3" << true;
    QTest::newRow("unknown file") << "music/zzz.mp3" << false;
}

void EdmAnalysis_Test::lookupByPath()
{
    QFETCH(QString, relativePath);
    QFETCH(bool, found);

    QTemporaryDir dir;
    QVERIFY(QDir().mkpath(dir.filePath("music")));
    writeFile(dir.filePath("music/a.mp3"), "audio");
    QVERIFY(QFile::link(dir.filePath("music"), dir.filePath("link")));
    const QString canonical = QFileInfo(dir.filePath("music/a.mp3")).canonicalFilePath();
    const QString id = relativePath.startsWith("gone") ? "bbbbbbbbbbbbbbbb" : "aaaaaaaaaaaaaaaa";
    const QString indexed = relativePath.startsWith("gone") ? dir.filePath("gone/b.mp3") : canonical;
    writeAnalysisDir(dir.path(), indexed, id, document("qlcplus-edm-final/2", kGrid, kSection, kEvent));

    EdmAnalysisReader reader(dir.path());
    const EdmAnalysis a = reader.analysis(dir.filePath(relativePath));

    QCOMPARE(a.valid, found);
    QCOMPARE(reader.sections(dir.filePath(relativePath)).size(), found ? 1 : 0);
}

void EdmAnalysis_Test::lookupByContent()
{
    QTemporaryDir dir;
    QVERIFY(QDir().mkpath(dir.filePath("moved")));
    const QByteArray audio = "the audio bytes";
    writeFile(dir.filePath("moved/a.mp3"), audio);
    writeFile(dir.filePath("moved/other.mp3"), "other bytes");
    const QString id = QString(QCryptographicHash::hash(audio, QCryptographicHash::Sha256).toHex()).left(16);
    writeAnalysisDir(dir.path(), "/old/place/a.mp3", id, document("qlcplus-edm-final/2", kGrid, kSection, kEvent));

    EdmAnalysisReader reader(dir.path());

    QVERIFY(reader.analysis(dir.filePath("moved/a.mp3")).valid == false);
    QCOMPARE(reader.idByContent(dir.filePath("moved/a.mp3")), id);
    QVERIFY(reader.analysisById(id).valid);
    QCOMPARE(reader.idByContent(dir.filePath("moved/other.mp3")), QString());
    QCOMPARE(reader.idByContent(dir.filePath("moved/nothing.mp3")), QString());
}

void EdmAnalysis_Test::lookupWithoutFinalFile()
{
    QTemporaryDir dir;
    writeFile(dir.filePath("a.mp3"), "audio");
    writeAnalysisDir(dir.path(), dir.filePath("a.mp3"), "aaaaaaaaaaaaaaaa", QByteArray());

    EdmAnalysisReader reader(dir.path());

    QVERIFY(reader.analysis(dir.filePath("a.mp3")).valid == false);
    QVERIFY(reader.analysisById("aaaaaaaaaaaaaaaa").valid == false);
}

void EdmAnalysis_Test::cacheFollowsFinalFile()
{
    QTemporaryDir dir;
    writeFile(dir.filePath("a.mp3"), "audio");
    const QString id = "aaaaaaaaaaaaaaaa";
    writeAnalysisDir(dir.path(), dir.filePath("a.mp3"), id, document("qlcplus-edm-final/2", kGrid, kSection, kEvent));
    const QString finalPath = dir.filePath("final/" + id + ".json");

    EdmAnalysisReader reader(dir.path());
    QCOMPARE(reader.analysisById(id).sections.size(), 1);

    writeFile(finalPath, document("qlcplus-edm-final/2", kGrid, kSection + "," + kSection, kEvent));
    QFile file(finalPath);
    QVERIFY(file.open(QIODevice::ReadWrite));
    QVERIFY(file.setFileTime(QDateTime::currentDateTime().addSecs(60), QFileDevice::FileModificationTime));
    file.close();

    QCOMPARE(reader.analysisById(id).sections.size(), 2);
    QCOMPARE(reader.sections(dir.filePath("a.mp3")).size(), 2);
}

/** 20 bars: intro 0-4, buildup 4-8, drop 8-16, outro 16-20; a bar is 4 * period; downbeat at 1 s */
static EdmAnalysis songWithBpm(double bpm)
{
    const double period = 60.0 / bpm;
    const double bar = period * 4;
    auto section = [&](const char *label, int from, int to, int confidence) {
        return QString(R"({"label": "%1", "start_bar": %2, "end_bar": %3, "start": %4, "end": %5, "confidence": %6})")
            .arg(label).arg(from).arg(to).arg(1.0 + from * bar, 0, 'f', 6).arg(1.0 + to * bar, 0, 'f', 6).arg(confidence);
    };
    auto event = [&](const char *kind, int atBar, const QString &confidence) {
        return QString(R"({"kind": "%1", "bar": %2, "time": %3%4})").arg(kind).arg(atBar).arg(1.0 + atBar * bar, 0, 'f', 6).arg(confidence);
    };
    const QString grid = QString(R"("grid": {"bpm": %1, "period": %2, "beats_per_bar": 4, "first_downbeat": 1.0})").arg(bpm).arg(period, 0, 'f', 9);
    const QString sections = section("intro", 0, 4, 100) + "," + section("buildup", 4, 8, 70) + "," + section("drop", 8, 16, 90) + "," + section("outro", 16, 20, 60);
    const QString events = event("vocal_in", 2, "") + "," + event("drop", 8, R"(, "confidence": 80)") + "," + event("vocal_out", 18, "");
    return parseEdmAnalysis(document("qlcplus-edm-final/2", grid, sections, events));
}

void EdmAnalysis_Test::songPosition_data()
{
    QTest::addColumn<double>("bpm");
    QTest::addColumn<double>("bar");
    QTest::addColumn<int>("section");
    QTest::addColumn<double>("barInSection");
    QTest::addColumn<double>("barsToNextSection");
    QTest::addColumn<int>("nextDrop");
    QTest::addColumn<double>("barsToDrop");

    QTest::newRow("before the first downbeat") << 120.0 << -0.5 << 0 << 0.0 << 4.5 << 1 << 8.5;
    QTest::newRow("on a section boundary") << 120.0 << 8.0 << 2 << 0.0 << 8.0 << -1 << -1.0;
    QTest::newRow("mid section") << 120.0 << 5.5 << 1 << 1.5 << 2.5 << 1 << 2.5;
    QTest::newRow("last bar") << 120.0 << 19.5 << 3 << 3.5 << -1.0 << -1 << -1.0;
    QTest::newRow("past the end") << 120.0 << 20.0 << -1 << -1.0 << -1.0 << -1 << -1.0;
    QTest::newRow("140 bpm") << 140.0 << 5.5 << 1 << 1.5 << 2.5 << 1 << 2.5;
    QTest::newRow("124 bpm") << 124.0 << 12.25 << 2 << 4.25 << 3.75 << -1 << -1.0;
}

void EdmAnalysis_Test::songPosition()
{
    QFETCH(double, bpm);
    QFETCH(double, bar);
    QFETCH(int, section);
    QFETCH(double, barInSection);
    QFETCH(double, barsToNextSection);
    QFETCH(int, nextDrop);
    QFETCH(double, barsToDrop);

    const EdmAnalysis a = songWithBpm(bpm);
    QVERIFY(a.valid);
    const EdmSongPosition p = edmSongPosition(a, a.grid.firstDownbeatMs + bar * a.grid.barMs, "drop");

    QVERIFY(p.valid);
    QCOMPARE(p.bar, bar);
    QCOMPARE(p.section, section);
    QCOMPARE(p.barInSection, barInSection);
    QCOMPARE(p.barsToNextSection, barsToNextSection);
    QCOMPARE(p.nextEvent, nextDrop);
    QCOMPARE(p.barsToNextEvent, barsToDrop);

    QVERIFY(edmSongPosition(EdmAnalysis(), 1000).valid == false);
}

static QString describe(const EdmCrossings &c)
{
    QStringList parts;
    for (const EdmCrossing &x : c.crossed)
        parts << QString("%1%2@%3:%4").arg(x.kind == EdmCrossing::SectionStart ? "S" : "E").arg(x.index).arg(x.bar).arg(x.confidence);
    return parts.join(" ");
}

void EdmAnalysis_Test::firstCrossings()
{
    const EdmAnalysis a = songWithBpm(120);

    QCOMPARE(describe(edmFirstCrossings(a, 9.5)), QString("S2@8:90"));
    QCOMPARE(edmFirstCrossings(a, 9.5).activeSection, 2);
    QCOMPARE(describe(edmFirstCrossings(a, -2)), QString("S0@0:100"));
    QCOMPARE(describe(edmFirstCrossings(a, 25)), QString());
    QCOMPARE(edmFirstCrossings(a, 25).activeSection, -1);
}

void EdmAnalysis_Test::crossingsOfOneStep_data()
{
    QTest::addColumn<double>("from");
    QTest::addColumn<double>("to");
    QTest::addColumn<QString>("crossed");
    QTest::addColumn<bool>("jumped");
    QTest::addColumn<int>("active");

    QTest::newRow("one boundary") << 3.5 << 4.5 << "S1@4:70" << false << 1;
    QTest::newRow("no boundary") << 4.5 << 5.5 << "" << false << 1;
    QTest::newRow("three sections in one step") << 3.5 << 16.5 << "S1@4:70 S2@8:90 E1@8:80 S3@16:60" << false << 3;
    QTest::newRow("ends on an event bar") << 7.0 << 8.0 << "S2@8:90 E1@8:80" << false << 2;
    QTest::newRow("starts on an event bar") << 8.0 << 9.0 << "" << false << 2;
    QTest::newRow("event without confidence") << 17.0 << 19.0 << "E2@18:-1" << false << 3;
    QTest::newRow("step into the first section does not report it") << -1.0 << 0.5 << "" << false << 0;
    QTest::newRow("backward jump") << 12.0 << 5.0 << "" << true << 1;
    QTest::newRow("jump back to before the downbeat") << 12.0 << -1.0 << "" << true << 0;
}

void EdmAnalysis_Test::crossingsOfOneStep()
{
    QFETCH(double, from);
    QFETCH(double, to);
    QFETCH(QString, crossed);
    QFETCH(bool, jumped);
    QFETCH(int, active);

    const EdmCrossings c = edmCrossings(songWithBpm(120), from, to);

    QCOMPARE(describe(c), crossed);
    QCOMPARE(c.jumped, jumped);
    QCOMPARE(c.activeSection, active);
}

void EdmAnalysis_Test::consecutiveStepsReportOnce_data()
{
    QTest::addColumn<QList<double>>("positions");
    QTest::addColumn<QString>("all");

    QTest::newRow("small steps") << QList<double>{-0.5, 1.0, 7.5, 8.0, 8.2, 17.0, 19.9, 25.0}
                                 << "S0@0:100 E0@2:-1 S1@4:70 S2@8:90 E1@8:80 S3@16:60 E2@18:-1";
    QTest::newRow("one step over everything") << QList<double>{-0.5, 30.0} << "S0@0:100 E0@2:-1 S1@4:70 S2@8:90 E1@8:80 S3@16:60 E2@18:-1";
    QTest::newRow("starting mid song") << QList<double>{9.0, 10.0, 16.0} << "S2@8:90 S3@16:60";
    QTest::newRow("stopped on every boundary") << QList<double>{0.0, 4.0, 4.0, 8.0, 8.0, 16.0} << "S0@0:100 E0@2:-1 S1@4:70 S2@8:90 E1@8:80 S3@16:60";
}

void EdmAnalysis_Test::consecutiveStepsReportOnce()
{
    QFETCH(QList<double>, positions);
    QFETCH(QString, all);

    const EdmAnalysis a = songWithBpm(120);
    QStringList seen;
    seen << describe(edmFirstCrossings(a, positions.first()));
    for (int i = 1; i < positions.size(); i++)
        seen << describe(edmCrossings(a, positions[i - 1], positions[i]));
    seen.removeAll(QString());

    QCOMPARE(seen.join(" "), all);
}

QTEST_GUILESS_MAIN(EdmAnalysis_Test)
