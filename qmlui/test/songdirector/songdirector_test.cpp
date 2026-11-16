/*
  Q Light Controller Plus - Unit test
  songdirector_test.cpp

  Licensed under the Apache License, Version 2.0
*/

#include "songdirector_test.h"
#include "songdirector.h"

#include "audio.h"
#include "collection.h"
#include "edmstructuremapping.h"
#include "doc.h"
#include "scene.h"
#include "show.h"
#include "showfunction.h"
#include "track.h"

#include <QDir>
#include <QSignalSpy>
#include <QFile>
#include <QTemporaryDir>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

Q_DECLARE_METATYPE(QList<EdmSection>)

namespace
{

EdmSection section(const QString &label, double startMs, double endMs, int confidence = 50, bool unsure = false)
{
    EdmSection s;
    s.label = label;
    s.startMs = startMs;
    s.endMs = endMs;
    s.confidence = confidence;
    s.labelConfidence = confidence;
    s.unsure = unsure;
    return s;
}

/** "drop 2 #3 @9000 +8000 c90 u0": label, number, section index, start, duration, confidence, unsure */
QString describe(const SongBlock &b)
{
    return QString("%1 %2 #%3 @%4 +%5 c%6 u%7").arg(b.label).arg(b.number).arg(b.section)
        .arg(b.startMs).arg(b.durationMs).arg(b.confidence).arg(b.unsure ? 1 : 0);
}

/** intro 0-9 s, buildup 9-17 s (unsure), drop 17-33 s, outro 33-41 s */
const QByteArray kFinal = R"({"schema": "qlcplus-edm-final/3", "track": {"title": "t", "sha256": "aaaaaaaaaaaaaaaa00"},
    "grid": {"bpm": 120.0, "period": 0.5, "beats_per_bar": 4, "first_downbeat": 1.0},
    "sections": [
      {"label": "intro", "start_bar": 0, "end_bar": 4, "start": 0.0, "end": 9.0, "confidence": 100},
      {"label": "buildup", "start_bar": 4, "end_bar": 8, "start": 9.0, "end": 17.0, "confidence": 50, "label_confidence": 40, "unsure": true},
      {"label": "drop", "start_bar": 8, "end_bar": 16, "start": 17.0, "end": 33.0, "confidence": 90},
      {"label": "outro", "start_bar": 16, "end_bar": 20, "start": 33.0, "end": 41.0, "confidence": 60}]})";

const QString kSongPath = "/music/song.mp3";

/** A song Show with its Audio clip, as the Show factory makes it, and an analysis folder */
struct SongShow
{
    explicit SongShow(quint32 clipStartMs = 0, const QString &audioPath = kSongPath, bool analysed = true)
        : doc(nullptr)
        , director(&doc)
        , reader(dir.path())
    {
        QDir(dir.path()).mkpath("final");
        if (analysed)
            analyse(kFinal);

        show = new Show(&doc);
        show->setName("Song");
        doc.addFunction(show);
        if (audioPath.isEmpty())
            return;
        Audio *audio = new Audio(&doc);
        doc.addFunction(audio);
        audio->setSourceFileName(audioPath);
        Track *track = new Track(Function::invalidId(), show);
        track->setName("Audio");
        show->addTrack(track);
        track->createShowFunction(audio->id())->setStartTime(clipStartMs);
    }

    /** Write (or rewrite) the song's analysis; each write gets a newer file time, so the reader takes it */
    void analyse(const QByteArray &json)
    {
        static int written = 0;
        const QDateTime time = QDateTime::currentDateTime().addSecs(++written);
        for (const auto &[name, data] : {std::pair { QString("index.json"),
                                                      QString(R"({"aaaaaaaaaaaaaaaa": {"path": "%1"}})").arg(kSongPath).toUtf8() },
                                          std::pair { QString("final/aaaaaaaaaaaaaaaa.json"), json }})
        {
            QFile f(dir.filePath(name));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(data);
            QVERIFY(f.flush());   // a write on close would set the time again
            QVERIFY(f.setFileTime(time, QFileDevice::FileModificationTime));
        }
    }

    Doc doc;
    QTemporaryDir dir;
    SongDirector director;
    EdmAnalysisReader reader;
    Show *show = nullptr;
};

/** "name @start +duration" of each item on the track */
QStringList items(Doc *doc, Track *track)
{
    QStringList list;
    for (ShowFunction *sf : track->showFunctions())
    {
        Function *f = doc->function(sf->functionID());
        list << QString("%1 @%2 +%3").arg(f ? f->name() : "?").arg(sf->startTime()).arg(sf->duration());
    }
    return list;
}

/** A final analysis file with these sections */
QByteArray finalJson(const QList<EdmSection> &sections)
{
    QStringList list;
    for (const EdmSection &s : sections)
        list << QString(R"({"label": "%1", "start": %2, "end": %3, "confidence": %4, "label_confidence": %5, "unsure": %6})")
                    .arg(s.label).arg(s.startMs / 1000.0).arg(s.endMs / 1000.0).arg(s.confidence).arg(s.labelConfidence)
                    .arg(s.unsure ? "true" : "false");
    return QString(R"({"schema": "qlcplus-edm-final/3", "track": {"title": "t", "sha256": "aaaaaaaaaaaaaaaa00"},
        "grid": {"bpm": 120.0, "period": 0.5, "beats_per_bar": 4, "first_downbeat": 0.0}, "sections": [%1]})")
        .arg(list.join(",")).toUtf8();
}

/** "name @start +duration {content}" of each item on the track, by start */
QStringList itemsWithContent(Doc *doc, Track *track)
{
    QStringList list;
    for (ShowFunction *sf : track->showFunctions())
    {
        Function *f = doc->function(sf->functionID());
        QStringList content;
        if (auto *collection = qobject_cast<Collection *>(f))
            for (quint32 id : collection->functions())
                content << (doc->function(id) ? doc->function(id)->name() : "?");
        list << QString("%1 @%2 +%3 {%4}").arg(f ? f->name() : "?").arg(sf->startTime()).arg(sf->duration()).arg(content.join(","));
    }
    std::sort(list.begin(), list.end(), [](const QString &a, const QString &b) {
        const uint startA = a.section('@', 1).section(' ', 0, 0).toUInt(), startB = b.section('@', 1).section(' ', 0, 0).toUInt();
        return startA != startB ? startA < startB : a < b;
    });
    return list;
}

/** The sidecar as "label number @start +duration cN uN", by start */
QStringList sidecar(const QList<SongDirectorBlock> &blocks)
{
    QList<SongBlock> generated;
    for (const SongDirectorBlock &b : blocks)
        generated << b.generated;
    std::sort(generated.begin(), generated.end(), [](const SongBlock &a, const SongBlock &b) {
        return a.startMs != b.startMs ? a.startMs < b.startMs : a.label < b.label;
    });
    QStringList list;
    for (const SongBlock &b : generated)
        list << QString("%1 %2 @%3 +%4 c%5 u%6").arg(b.label).arg(b.number).arg(b.startMs).arg(b.durationMs)
                    .arg(b.confidence).arg(b.unsure ? 1 : 0);
    return list;
}

int collections(Doc *doc)
{
    return doc->functionsByType(Function::CollectionType).count();
}

}

void SongDirector_Test::songBlocks_data()
{
    QTest::addColumn<QList<EdmSection>>("sections");
    QTest::addColumn<quint32>("clipStartMs");
    QTest::addColumn<QStringList>("expected");

    const QList<EdmSection> two { section("intro", 0, 9000, 100), section("drop", 9000, 17000, 90) };
    QTest::newRow("clip at the show start") << two << 0u
        << QStringList { "intro 1 #0 @0 +9000 c100 u0", "drop 1 #1 @9000 +8000 c90 u0" };
    QTest::newRow("clip 30 s into the show") << two << 30000u
        << QStringList { "intro 1 #0 @30000 +9000 c100 u0", "drop 1 #1 @39000 +8000 c90 u0" };
    QTest::newRow("repeated labels are numbered")
        << QList<EdmSection> { section("drop", 0, 1000), section("breakdown", 1000, 2000), section("drop", 2000, 3000) } << 0u
        << QStringList { "drop 1 #0 @0 +1000 c50 u0", "breakdown 1 #1 @1000 +1000 c50 u0", "drop 2 #2 @2000 +1000 c50 u0" };
    QTest::newRow("fractional ms leave no gap")
        << QList<EdmSection> { section("intro", 0, 1000.4), section("drop", 1000.4, 2000.6) } << 0u
        << QStringList { "intro 1 #0 @0 +1000 c50 u0", "drop 1 #1 @1000 +1001 c50 u0" };
    QTest::newRow("a section without length makes no block, the index stays")
        << QList<EdmSection> { section("intro", 0, 1000), section("buildup", 1000, 1000), section("drop", 1000, 2000) } << 0u
        << QStringList { "intro 1 #0 @0 +1000 c50 u0", "drop 1 #2 @1000 +1000 c50 u0" };
    QTest::newRow("unsure and confidence carried")
        << QList<EdmSection> { section("buildup", 0, 4000, 41, true) } << 500u
        << QStringList { "buildup 1 #0 @500 +4000 c41 u1" };
    QTest::newRow("no sections") << QList<EdmSection>() << 0u << QStringList();
}

void SongDirector_Test::songBlocks()
{
    QFETCH(QList<EdmSection>, sections);
    QFETCH(quint32, clipStartMs);
    QFETCH(QStringList, expected);

    QStringList actual;
    for (const SongBlock &b : ::songBlocks(sections, clipStartMs))
        actual << describe(b);
    QCOMPARE(actual, expected);
}

void SongDirector_Test::createBlocks_data()
{
    QTest::addColumn<quint32>("clipStartMs");
    QTest::addColumn<QStringList>("expected");

    QTest::newRow("clip at the show start") << 0u
        << QStringList { "Song / intro 1 @0 +9000", "Song / buildup 1 @9000 +8000",
                         "Song / drop 1 @17000 +16000", "Song / outro 1 @33000 +8000" };
    QTest::newRow("clip 30 s into the show") << 30000u
        << QStringList { "Song / intro 1 @30000 +9000", "Song / buildup 1 @39000 +8000",
                         "Song / drop 1 @47000 +16000", "Song / outro 1 @63000 +8000" };
}

void SongDirector_Test::createBlocks()
{
    QFETCH(quint32, clipStartMs);
    QFETCH(QStringList, expected);

    SongShow song(clipStartMs);
    QVERIFY(song.director.track(song.show) == nullptr);

    QVERIFY(song.director.createBlocks(song.show, song.reader));

    Track *track = song.director.track(song.show);
    QVERIFY(track != nullptr);
    QCOMPARE(track->name(), SongDirector::trackName());
    QCOMPARE(song.show->tracks().count(), 2);
    QVERIFY(song.show->tracks().contains(track));
    QCOMPARE(items(&song.doc, track), expected);

    const QList<SongDirectorBlock> blocks = song.director.blocks(song.show);
    QCOMPARE(blocks.count(), track->showFunctions().count());
    for (int i = 0; i < blocks.count(); i++)
    {
        const ShowFunction *sf = track->showFunctions().at(i);
        Collection *collection = qobject_cast<Collection *>(song.doc.function(sf->functionID()));
        QVERIFY(collection != nullptr);
        QVERIFY(collection->functions().isEmpty());
        QVERIFY(collection->holdUntilStopped());
        QCOMPARE(blocks.at(i).functionId, collection->id());
        QCOMPARE(blocks.at(i).generated.section, i);
        QCOMPARE(blocks.at(i).generated.startMs, sf->startTime());
        QCOMPARE(blocks.at(i).generated.durationMs, sf->duration());
        QVERIFY(blocks.at(i).content.isEmpty());
    }
    QCOMPARE(blocks.at(1).generated.label, QString("buildup"));
    QCOMPARE(blocks.at(1).generated.confidence, 40);
    QVERIFY(blocks.at(1).generated.unsure);
    QVERIFY(blocks.at(2).generated.unsure == false);
}

void SongDirector_Test::createBlocksFromDefaults_data()
{
    QTest::addColumn<QStringList>("defaults");   // "label=slot": slot 0 is the first scene, 1 the second, "gone" a Function that does not exist
    QTest::addColumn<QStringList>("expected");   // per block: the scene slot in its Collection, or "-"

    QTest::newRow("no defaults") << QStringList() << QStringList { "-", "-", "-", "-" };
    QTest::newRow("some labels") << QStringList { "drop=1", "intro=0" } << QStringList { "0", "-", "1", "-" };
    QTest::newRow("all labels") << QStringList { "intro=0", "buildup=0", "drop=1", "outro=1" }
                                << QStringList { "0", "0", "1", "1" };
    QTest::newRow("unknown label unused") << QStringList { "chorus=1" } << QStringList { "-", "-", "-", "-" };
    QTest::newRow("default function is gone") << QStringList { "drop=gone", "outro=0" }
                                              << QStringList { "-", "-", "-", "0" };
}

void SongDirector_Test::createBlocksFromDefaults()
{
    QFETCH(QStringList, defaults);
    QFETCH(QStringList, expected);

    SongShow song;
    QList<quint32> scenes;
    for (const char *name : {"Warm wash", "Drop A"})
    {
        Scene *scene = new Scene(&song.doc);
        scene->setName(name);
        QVERIFY(song.doc.addFunction(scene));
        scenes << scene->id();
    }
    EdmStructureMapping mapping(&song.doc);
    for (const QString &d : defaults)
    {
        const QString slot = d.section('=', 1);
        mapping.setFunctionId(d.section('=', 0, 0), slot == "gone" ? 4000 : int(scenes.at(slot.toInt())));
    }
    song.director.setMapping(&mapping);

    QVERIFY(song.director.createBlocks(song.show, song.reader));

    const QList<SongDirectorBlock> blocks = song.director.blocks(song.show);
    QCOMPARE(blocks.count(), expected.count());
    for (int i = 0; i < blocks.count(); i++)
    {
        auto *collection = qobject_cast<Collection *>(song.doc.function(blocks.at(i).functionId));
        QVERIFY(collection != nullptr);
        const QList<quint32> want = expected.at(i) == "-" ? QList<quint32>() : QList<quint32> { scenes.at(expected.at(i).toInt()) };
        QCOMPARE(collection->functions(), want);
        QCOMPARE(blocks.at(i).content, want);
    }

    // the sidecar keeps the generated content through a save
    QString xml;
    QXmlStreamWriter writer(&xml);
    writer.writeStartElement("Workspace");
    QVERIFY(song.doc.saveXML(&writer));
    song.director.saveXML(&writer);
    writer.writeEndElement();
    Doc doc(nullptr);
    SongDirector director(&doc);
    QXmlStreamReader reader(xml);
    QVERIFY(reader.readNextStartElement());
    QVERIFY(reader.readNextStartElement());
    QVERIFY(doc.loadXML(reader));
    QVERIFY(reader.readNextStartElement());
    QVERIFY(director.loadXML(reader));
    QCOMPARE(director.blocks(qobject_cast<Show *>(doc.function(song.show->id()))), blocks);
}

void SongDirector_Test::createBlocksOncePerShow()
{
    SongShow song;
    QVERIFY(song.director.createBlocks(song.show, song.reader));
    const int functions = song.doc.functions().count();

    QVERIFY(song.director.createBlocks(song.show, song.reader) == false);

    QCOMPARE(song.show->tracks().count(), 2);
    QCOMPARE(song.doc.functions().count(), functions);
}

void SongDirector_Test::createBlocksNeedsAnalysedSong_data()
{
    QTest::addColumn<QString>("audioPath");

    QTest::newRow("no audio clip") << QString();
    QTest::newRow("song not analysed") << QString("/music/other.mp3");
}

void SongDirector_Test::createBlocksNeedsAnalysedSong()
{
    QFETCH(QString, audioPath);

    SongShow song(0, audioPath);
    const int tracks = song.show->tracks().count();
    const int functions = song.doc.functions().count();

    QVERIFY(song.director.createBlocks(song.show, song.reader) == false);

    QVERIFY(song.director.track(song.show) == nullptr);
    QCOMPARE(song.show->tracks().count(), tracks);
    QCOMPARE(song.doc.functions().count(), functions);
}

void SongDirector_Test::workspaceRoundTrip()
{
    SongShow song(30000);
    QVERIFY(song.director.createBlocks(song.show, song.reader));
    const QStringList expectedItems = items(&song.doc, song.director.track(song.show));
    const QList<SongDirectorBlock> expectedBlocks = song.director.blocks(song.show);

    QString xml;
    QXmlStreamWriter writer(&xml);
    writer.writeStartElement("Workspace");
    QVERIFY(song.doc.saveXML(&writer));
    song.director.saveXML(&writer);
    writer.writeEndElement();

    Doc doc(nullptr);
    SongDirector director(&doc);
    QXmlStreamReader reader(xml);
    QVERIFY(reader.readNextStartElement());
    QVERIFY(reader.readNextStartElement());
    QVERIFY(doc.loadXML(reader));
    QVERIFY(reader.readNextStartElement());
    QVERIFY(director.loadXML(reader));

    Show *show = qobject_cast<Show *>(doc.function(song.show->id()));
    QVERIFY(show != nullptr);
    Track *track = director.track(show);
    QVERIFY(track != nullptr);
    QCOMPARE(track->id(), song.director.track(song.show)->id());
    QCOMPARE(items(&doc, track), expectedItems);
    QCOMPARE(director.blocks(show), expectedBlocks);
    // the hold flag is not in the Collection's XML: loading the sidecar sets it again
    for (const SongDirectorBlock &block : expectedBlocks)
        QVERIFY(qobject_cast<Collection *>(doc.function(block.functionId))->holdUntilStopped());
}

void SongDirector_Test::holdOnlyOnSidecarBlocks_data()
{
    QTest::addColumn<QString>("setup");
    QTest::addColumn<bool>("held");   // the Collection under test holds until stopped

    QTest::newRow("block loaded with its track") << "kept" << true;
    QTest::newRow("block of a track that is gone, dropped on load") << "track gone" << false;
    QTest::newRow("block of a second director track, dropped on load") << "second track" << false;
    QTest::newRow("director track deleted") << "track deleted" << false;
}

void SongDirector_Test::holdOnlyOnSidecarBlocks()
{
    QFETCH(QString, setup);
    QFETCH(bool, held);

    SongShow song;
    Collection *collection = nullptr;
    if (setup == "track deleted")
    {
        QVERIFY(song.director.createBlocks(song.show, song.reader));
        collection = qobject_cast<Collection *>(song.doc.function(song.director.blocks(song.show).first().functionId));
        QVERIFY(collection->holdUntilStopped());
        QVERIFY(song.show->removeTrack(song.director.track(song.show)->id()));
    }
    else
    {
        QList<Track *> tracks;
        QList<Collection *> blocks;
        for (int i = 0; i < 2; i++)
        {
            tracks << new Track(Function::invalidId(), song.show);
            song.show->addTrack(tracks.last());
            blocks << new Collection(&song.doc);
            QVERIFY(song.doc.addFunction(blocks.last()));
        }
        // two director tracks for one Show: the first one read wins
        const QString block = R"(<Block Function="%1" Section="0" Label="intro" Number="1" Confidence="50" Unsure="0" Start="0" Duration="9000"/>)";
        const QString xml = QString(R"(<SongDirector><Track Show="%1" ID="%2">%3</Track><Track Show="%1" ID="%4">%5</Track></SongDirector>)")
            .arg(song.show->id()).arg(setup == "track gone" ? 4000u : tracks.at(0)->id())
            .arg(block.arg(blocks.at(0)->id())).arg(tracks.at(1)->id()).arg(block.arg(blocks.at(1)->id()));
        QXmlStreamReader reader(xml);
        QVERIFY(reader.readNextStartElement());
        QVERIFY(song.director.loadXML(reader));
        collection = blocks.at(setup == "second track" ? 1 : 0);
    }
    QCOMPARE(collection->holdUntilStopped(), held);
}

void SongDirector_Test::dropOnBlock_data()
{
    QTest::addColumn<quint32>("timeMs");
    QTest::addColumn<bool>("onDirectorTrack");
    QTest::addColumn<QStringList>("dropped");   // "0"/"1": a scene slot, "show": the song Show itself, "gone": no such Function
    QTest::addColumn<int>("block");             // the block that takes the drop, -1 for none
    QTest::addColumn<QStringList>("expected");  // that block's Collection afterwards, as scene slots

    // blocks: intro 0-9000, buildup 9000-17000, drop 17000-33000, outro 33000-41000
    QTest::newRow("inside a block") << 20000u << true << QStringList { "1" } << 2 << QStringList { "1" };
    QTest::newRow("block start") << 9000u << true << QStringList { "0" } << 1 << QStringList { "0" };
    QTest::newRow("block end belongs to the next block") << 17000u << true << QStringList { "0" } << 2 << QStringList { "0" };
    QTest::newRow("several functions") << 1u << true << QStringList { "0", "1" } << 0 << QStringList { "0", "1" };
    QTest::newRow("the song Show itself is skipped") << 1u << true << QStringList { "show", "0" } << 0 << QStringList { "0" };
    QTest::newRow("unknown function is skipped") << 1u << true << QStringList { "gone", "1" } << 0 << QStringList { "1" };
    QTest::newRow("only invalid functions still hit the block") << 1u << true << QStringList { "show" } << 0 << QStringList();
    QTest::newRow("after the last block") << 41000u << true << QStringList { "0" } << -1 << QStringList();
    QTest::newRow("other track") << 20000u << false << QStringList { "0" } << -1 << QStringList();
}

void SongDirector_Test::dropOnBlock()
{
    QFETCH(quint32, timeMs);
    QFETCH(bool, onDirectorTrack);
    QFETCH(QStringList, dropped);
    QFETCH(int, block);
    QFETCH(QStringList, expected);

    SongShow song;
    QList<quint32> scenes;
    for (const char *name : {"Warm wash", "Drop A"})
    {
        Scene *scene = new Scene(&song.doc);
        scene->setName(name);
        QVERIFY(song.doc.addFunction(scene));
        scenes << scene->id();
    }
    QVERIFY(song.director.createBlocks(song.show, song.reader));
    Track *director = song.director.track(song.show);
    Track *track = onDirectorTrack ? director : song.show->tracks().first();
    const QStringList before = items(&song.doc, director);
    const QList<SongDirectorBlock> generated = song.director.blocks(song.show);

    QList<quint32> ids;
    for (const QString &d : dropped)
        ids << (d == "show" ? song.show->id() : d == "gone" ? 4000u : scenes.at(d.toInt()));

    QCOMPARE(song.director.dropOnBlock(song.show, track, timeMs, ids), block >= 0);

    // no block changes its place or length, whatever the drop did
    QCOMPARE(items(&song.doc, director), before);
    for (int i = 0; i < generated.count(); i++)
    {
        auto *collection = qobject_cast<Collection *>(song.doc.function(generated.at(i).functionId));
        QVERIFY(collection != nullptr);
        QList<quint32> want;
        if (i == block)
            for (const QString &e : expected)
                want << scenes.at(e.toInt());
        QCOMPARE(collection->functions(), want);
    }
    // the sidecar still holds what was generated, so a filled block now counts as edited
    QCOMPARE(song.director.blocks(song.show), generated);
}

void SongDirector_Test::syncCreatesBlocksOnce_data()
{
    QTest::addColumn<bool>("analysedAtStart");
    QTest::addColumn<bool>("songShow");        // Perform's Show for the song
    QTest::addColumn<QString>("afterwards");   // none, delete the director track, or delete it, save and reopen
    QTest::addColumn<int>("blocks");           // director blocks at the end, 0: no director track

    QTest::newRow("analysis there") << true << true << "none" << 4;
    QTest::newRow("analysis arrives later") << false << true << "none" << 4;
    QTest::newRow("not the song's Show") << true << false << "none" << 0;
    QTest::newRow("deleted director track is not recreated") << true << true << "delete" << 0;
    QTest::newRow("deleted track, saved and reopened, not recreated") << true << true << "delete+reopen" << 0;
}

void SongDirector_Test::syncCreatesBlocksOnce()
{
    QFETCH(bool, analysedAtStart);
    QFETCH(bool, songShow);
    QFETCH(QString, afterwards);
    QFETCH(int, blocks);

    SongShow song(0, kSongPath, analysedAtStart);
    song.director.setSongShowFilter([songShow](Show *) { return songShow; });
    QSignalSpy changed(&song.director, &SongDirector::blocksChanged);

    song.director.sync(song.reader);
    if (analysedAtStart == false)
    {
        QVERIFY(song.director.track(song.show) == nullptr);
        QCOMPARE(changed.count(), 0);
        song.analyse(kFinal);
        song.director.sync(song.reader);
    }
    QCOMPARE(changed.count(), songShow ? 1 : 0);
    if (songShow)
        QCOMPARE(changed.first().first().toUInt(), song.show->id());

    Doc *doc = &song.doc;
    SongDirector *director = &song.director;
    Show *show = song.show;
    Doc reopenedDoc(nullptr);
    SongDirector reopened(&reopenedDoc);
    if (afterwards.startsWith("delete"))
        QVERIFY(show->removeTrack(director->track(show)->id()));
    if (afterwards == "delete+reopen")
    {
        QString xml;
        QXmlStreamWriter writer(&xml);
        writer.writeStartElement("Workspace");
        QVERIFY(doc->saveXML(&writer));
        director->saveXML(&writer);
        writer.writeEndElement();
        QXmlStreamReader reader(xml);
        QVERIFY(reader.readNextStartElement());
        QVERIFY(reader.readNextStartElement());
        QVERIFY(reopenedDoc.loadXML(reader));
        QVERIFY(reader.readNextStartElement());
        QVERIFY(reopened.loadXML(reader));
        doc = &reopenedDoc;
        director = &reopened;
        show = qobject_cast<Show *>(doc->function(show->id()));
        QVERIFY(show != nullptr);
    }

    // syncing again creates nothing more
    const int functions = doc->functions().count();
    const int tracks = show->tracks().count();
    director->sync(song.reader);
    director->sync(song.reader);
    QCOMPARE(doc->functions().count(), functions);
    QCOMPARE(show->tracks().count(), tracks);

    QCOMPARE(director->track(show) != nullptr, blocks > 0);
    QCOMPARE(director->blocks(show).count(), blocks);
    if (blocks > 0)
        QCOMPARE(director->track(show)->showFunctions().count(), blocks);
}

void SongDirector_Test::followAnalysis_data()
{
    QTest::addColumn<QString>("edit");               // what the user did to a block before the analysis changed
    QTest::addColumn<QList<EdmSection>>("analysis"); // the changed analysis
    QTest::addColumn<QStringList>("expected");       // the director track afterwards
    QTest::addColumn<QStringList>("expectedSidecar");
    QTest::addColumn<int>("expectedCollections");
    QTest::addColumn<int>("emitted");

    const QList<EdmSection> base { section("intro", 0, 9000, 100), section("buildup", 9000, 17000, 40, true),
                                   section("drop", 17000, 33000, 90), section("outro", 33000, 41000, 60) };
    const QStringList baseItems { "Song / intro 1 @0 +9000 {}", "Song / buildup 1 @9000 +8000 {}",
                                  "Song / drop 1 @17000 +16000 {}", "Song / outro 1 @33000 +8000 {}" };
    const QStringList baseSidecar { "intro 1 @0 +9000 c100 u0", "buildup 1 @9000 +8000 c40 u1",
                                    "drop 1 @17000 +16000 c90 u0", "outro 1 @33000 +8000 c60 u0" };
    const QList<EdmSection> earlier { section("intro", 0, 8000, 100), section("buildup", 8000, 16000, 40, true),
                                      section("drop", 16000, 32000, 90), section("outro", 32000, 40000, 60) };
    const QStringList earlierSidecar { "intro 1 @0 +8000 c100 u0", "buildup 1 @8000 +8000 c40 u1",
                                       "drop 1 @16000 +16000 c90 u0", "outro 1 @32000 +8000 c60 u0" };
    const QList<EdmSection> noBuildup { section("intro", 0, 9000, 100), section("drop", 9000, 33000, 90),
                                        section("outro", 33000, 41000, 60) };

    QTest::newRow("unchanged analysis") << "none" << base << baseItems << baseSidecar << 4 << 0;
    QTest::newRow("edges move, untouched blocks follow") << "none" << earlier
        << QStringList { "Song / intro 1 @0 +8000 {}", "Song / buildup 1 @8000 +8000 {}",
                         "Song / drop 1 @16000 +16000 {}", "Song / outro 1 @32000 +8000 {}" }
        << earlierSidecar << 4 << 1;
    QTest::newRow("moved block stays") << "move drop" << earlier
        << QStringList { "Song / intro 1 @0 +8000 {}", "Song / buildup 1 @8000 +8000 {}",
                         "Song / drop 1 @18000 +16000 {}", "Song / outro 1 @32000 +8000 {}" }
        << QStringList { "intro 1 @0 +8000 c100 u0", "buildup 1 @8000 +8000 c40 u1",
                         "drop 1 @17000 +16000 c90 u0", "outro 1 @32000 +8000 c60 u0" } << 4 << 1;
    QTest::newRow("resized block stays") << "shorten drop" << earlier
        << QStringList { "Song / intro 1 @0 +8000 {}", "Song / buildup 1 @8000 +8000 {}",
                         "Song / drop 1 @17000 +10000 {}", "Song / outro 1 @32000 +8000 {}" }
        << QStringList { "intro 1 @0 +8000 c100 u0", "buildup 1 @8000 +8000 c40 u1",
                         "drop 1 @17000 +16000 c90 u0", "outro 1 @32000 +8000 c60 u0" } << 4 << 1;
    QTest::newRow("filled block stays") << "fill drop" << earlier
        << QStringList { "Song / intro 1 @0 +8000 {}", "Song / buildup 1 @8000 +8000 {}",
                         "Song / drop 1 @17000 +16000 {Extra}", "Song / outro 1 @32000 +8000 {}" }
        << QStringList { "intro 1 @0 +8000 c100 u0", "buildup 1 @8000 +8000 c40 u1",
                         "drop 1 @17000 +16000 c90 u0", "outro 1 @32000 +8000 c60 u0" } << 4 << 1;
    QTest::newRow("deleted block is not recreated") << "delete drop" << earlier
        << QStringList { "Song / intro 1 @0 +8000 {}", "Song / buildup 1 @8000 +8000 {}", "Song / outro 1 @32000 +8000 {}" }
        << QStringList { "intro 1 @0 +8000 c100 u0", "buildup 1 @8000 +8000 c40 u1",
                         "drop 1 @17000 +16000 c90 u0", "outro 1 @32000 +8000 c60 u0" } << 4 << 1;
    QTest::newRow("added section gets a pre-filled block")
        << "none" << QList<EdmSection> { section("intro", 0, 9000, 100), section("buildup", 9000, 17000, 40, true),
                                         section("drop", 17000, 33000, 90), section("breakdown", 33000, 37000, 70),
                                         section("outro", 37000, 45000, 60) }
        << QStringList { "Song / intro 1 @0 +9000 {}", "Song / buildup 1 @9000 +8000 {}", "Song / drop 1 @17000 +16000 {}",
                         "Song / breakdown 1 @33000 +4000 {Calm}", "Song / outro 1 @37000 +8000 {}" }
        << QStringList { "intro 1 @0 +9000 c100 u0", "buildup 1 @9000 +8000 c40 u1", "drop 1 @17000 +16000 c90 u0",
                         "breakdown 1 @33000 +4000 c70 u0", "outro 1 @37000 +8000 c60 u0" } << 5 << 1;
    QTest::newRow("removed section takes its untouched block") << "none" << noBuildup
        << QStringList { "Song / intro 1 @0 +9000 {}", "Song / drop 1 @9000 +24000 {}", "Song / outro 1 @33000 +8000 {}" }
        << QStringList { "intro 1 @0 +9000 c100 u0", "drop 1 @9000 +24000 c90 u0", "outro 1 @33000 +8000 c60 u0" } << 3 << 1;
    QTest::newRow("filled block of a removed section stays") << "fill buildup" << noBuildup
        << QStringList { "Song / intro 1 @0 +9000 {}", "Song / buildup 1 @9000 +8000 {Extra}",
                         "Song / drop 1 @9000 +24000 {}", "Song / outro 1 @33000 +8000 {}" }
        << QStringList { "intro 1 @0 +9000 c100 u0", "buildup 1 @9000 +8000 c40 u1",
                         "drop 1 @9000 +24000 c90 u0", "outro 1 @33000 +8000 c60 u0" } << 4 << 1;
    QTest::newRow("unsure and confidence follow")
        << "none" << QList<EdmSection> { section("intro", 0, 9000, 100), section("buildup", 9000, 17000, 80),
                                         section("drop", 17000, 33000, 90), section("outro", 33000, 41000, 60) }
        << baseItems
        << QStringList { "intro 1 @0 +9000 c100 u0", "buildup 1 @9000 +8000 c80 u0",
                         "drop 1 @17000 +16000 c90 u0", "outro 1 @33000 +8000 c60 u0" } << 4 << 1;
}

void SongDirector_Test::followAnalysis()
{
    QFETCH(QString, edit);
    QFETCH(QList<EdmSection>, analysis);
    QFETCH(QStringList, expected);
    QFETCH(QStringList, expectedSidecar);
    QFETCH(int, expectedCollections);
    QFETCH(int, emitted);

    SongShow song(0, kSongPath, false);
    song.analyse(finalJson({ section("intro", 0, 9000, 100), section("buildup", 9000, 17000, 40, true),
                             section("drop", 17000, 33000, 90), section("outro", 33000, 41000, 60) }));
    QMap<QString, quint32> scenes;
    for (const char *name : {"Calm", "Extra"})
    {
        Scene *scene = new Scene(&song.doc);
        scene->setName(name);
        QVERIFY(song.doc.addFunction(scene));
        scenes.insert(name, scene->id());
    }
    EdmStructureMapping mapping(&song.doc);
    mapping.setFunctionId("breakdown", int(scenes.value("Calm")));
    song.director.setMapping(&mapping);
    song.director.sync(song.reader);
    Track *track = song.director.track(song.show);
    QVERIFY(track != nullptr);

    ShowFunction *target = nullptr;
    for (ShowFunction *sf : track->showFunctions())
    {
        if (edit.endsWith(" " + song.doc.function(sf->functionID())->name().section(' ', 2, 2)))
            target = sf;
    }
    QVERIFY((target != nullptr) == (edit != "none"));
    if (edit.startsWith("move"))
        target->setStartTime(target->startTime() + 1000);
    else if (edit.startsWith("shorten"))
        target->setDuration(10000);
    else if (edit.startsWith("fill"))
        qobject_cast<Collection *>(song.doc.function(target->functionID()))->addFunction(scenes.value("Extra"));
    else if (edit.startsWith("delete"))
        QVERIFY(track->removeShowFunction(target));

    QSignalSpy changed(&song.director, &SongDirector::blocksChanged);
    song.analyse(finalJson(analysis));
    song.director.sync(song.reader);

    QCOMPARE(itemsWithContent(&song.doc, track), expected);
    QCOMPARE(sidecar(song.director.blocks(song.show)), expectedSidecar);
    QCOMPARE(collections(&song.doc), expectedCollections);
    QCOMPARE(changed.count(), emitted);

    // a second sync of the same analysis changes nothing
    song.director.sync(song.reader);
    QCOMPARE(itemsWithContent(&song.doc, track), expected);
    QCOMPARE(changed.count(), emitted);
}

QTEST_GUILESS_MAIN(SongDirector_Test)
