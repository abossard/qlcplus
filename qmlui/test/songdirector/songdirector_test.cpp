/*
  Q Light Controller Plus - Unit test
  songdirector_test.cpp

  Licensed under the Apache License, Version 2.0
*/

#include "songdirector_test.h"
#include "songdirector.h"
#include "songsections.h"

#include "audio.h"
#include "chaser.h"
#include "collection.h"
#include "edmstructuremapping.h"
#include "doc.h"
#include "efx.h"
#include "mastertimer.h"
#include "rgbmatrix.h"
#include "scene.h"
#include "script.h"
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
        << QStringList { "intro 1 @0 +9000", "buildup 1 @9000 +8000",
                         "drop 1 @17000 +16000", "outro 1 @33000 +8000" };
    QTest::newRow("clip 30 s into the show") << 30000u
        << QStringList { "intro 1 @30000 +9000", "buildup 1 @39000 +8000",
                         "drop 1 @47000 +16000", "outro 1 @63000 +8000" };
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
    QTest::addColumn<QStringList>("defaults");   // "label=slot": slot 0 is the first scene, 1 the second, "gone" a Function that does not exist, "script" a Script, "show" the song Show
    QTest::addColumn<QStringList>("expected");   // per block: the scene slot in its Collection, or "-"

    QTest::newRow("no defaults") << QStringList() << QStringList { "-", "-", "-", "-" };
    QTest::newRow("some labels") << QStringList { "drop=1", "intro=0" } << QStringList { "0", "-", "1", "-" };
    QTest::newRow("all labels") << QStringList { "intro=0", "buildup=0", "drop=1", "outro=1" }
                                << QStringList { "0", "0", "1", "1" };
    QTest::newRow("unknown label unused") << QStringList { "chorus=1" } << QStringList { "-", "-", "-", "-" };
    QTest::newRow("default function is gone") << QStringList { "drop=gone", "outro=0" }
                                              << QStringList { "-", "-", "-", "0" };
    QTest::newRow("defaults that are no looks") << QStringList { "drop=script", "outro=show", "intro=0" }
                                                << QStringList { "0", "-", "-", "-" };
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
    Script *script = new Script(&song.doc);
    QVERIFY(song.doc.addFunction(script));
    EdmStructureMapping mapping(&song.doc);
    for (const QString &d : defaults)
    {
        const QString slot = d.section('=', 1);
        mapping.setFunctionId(d.section('=', 0, 0), slot == "gone" ? 4000 : slot == "script" ? int(script->id())
                                                    : slot == "show" ? int(song.show->id()) : int(scenes.at(slot.toInt())));
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
    QTest::newRow("block item deleted, track kept") << "item deleted" << false;
    QTest::newRow("filled block whose time no section covers, kept without a section") << "orphaned" << false;
    QTest::newRow("orphan block loaded") << "orphan loaded" << false;
    QTest::newRow("block loaded without its item") << "no item" << false;
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
    else if (setup == "item deleted" || setup == "orphaned")
    {
        QVERIFY(song.director.createBlocks(song.show, song.reader));
        Track *track = song.director.track(song.show);
        ShowFunction *buildup = track->showFunctions().at(1);
        collection = qobject_cast<Collection *>(song.doc.function(buildup->functionID()));
        QVERIFY(collection->holdUntilStopped());
        if (setup == "item deleted")
            QVERIFY(track->removeShowFunction(buildup, true));
        else
        {
            // the user fills the buildup block, then the analysis ends before it: no block covers its time
            Scene *extra = new Scene(&song.doc);
            QVERIFY(song.doc.addFunction(extra));
            collection->addFunction(extra->id());
            song.analyse(finalJson({ section("intro", 0, 9000, 100) }));
            song.director.sync(song.reader);
            QCOMPARE(song.director.blocks(song.show).count(), 2);
            QVERIFY(song.director.blocks(song.show).last().orphan);
        }
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
            if (setup != "no item")
                tracks.last()->createShowFunction(blocks.last()->id())->setDuration(9000);
        }
        // two director tracks for one Show: the first one read wins
        const QString block = QString(R"(<Block Function="%1" Section="0" Label="intro" Number="1" Confidence="50" Unsure="0" Start="0" Duration="9000"%2/>)")
            .arg("%1", setup == "orphan loaded" ? R"( Orphan="1")" : "");
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
    QTest::addColumn<QStringList>("dropped");   // "0"/"1": a scene slot, "show": the song Show itself, "gone": no such Function,
                                                // "audio": the song's Audio, "script", "chaser", "efx", "matrix": one of that type
    QTest::addColumn<int>("block");             // the block that takes the drop, -1 for none
    QTest::addColumn<QStringList>("expected");  // that block's Collection afterwards, same names

    // blocks: intro 0-9000, buildup 9000-17000, drop 17000-33000, outro 33000-41000
    QTest::newRow("inside a block") << 20000u << true << QStringList { "1" } << 2 << QStringList { "1" };
    QTest::newRow("block start") << 9000u << true << QStringList { "0" } << 1 << QStringList { "0" };
    QTest::newRow("block end belongs to the next block") << 17000u << true << QStringList { "0" } << 2 << QStringList { "0" };
    QTest::newRow("several functions") << 1u << true << QStringList { "0", "1" } << 0 << QStringList { "0", "1" };
    QTest::newRow("the song Show itself is skipped") << 1u << true << QStringList { "show", "0" } << 0 << QStringList { "0" };
    QTest::newRow("unknown function is skipped") << 1u << true << QStringList { "gone", "1" } << 0 << QStringList { "1" };
    QTest::newRow("only invalid functions still hit the block") << 1u << true << QStringList { "show" } << 0 << QStringList();
    QTest::newRow("the song's Audio is no look") << 1u << true << QStringList { "audio", "1" } << 0 << QStringList { "1" };
    QTest::newRow("a Script is no look") << 1u << true << QStringList { "script" } << 0 << QStringList();
    QTest::newRow("Chaser, EFX and RGB Matrix are looks") << 1u << true << QStringList { "chaser", "efx", "matrix" } << 0
                                                           << QStringList { "chaser", "efx", "matrix" };
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
    QMap<QString, quint32> others { { "show", song.show->id() }, { "gone", 4000u },
                                    { "audio", song.show->tracks().first()->showFunctions().first()->functionID() } };
    for (Function *f : QList<Function *> { new Script(&song.doc), new Chaser(&song.doc), new EFX(&song.doc), new RGBMatrix(&song.doc) })
    {
        QVERIFY(song.doc.addFunction(f));
        others.insert(f->type() == Function::ScriptType ? "script" : f->type() == Function::ChaserType ? "chaser"
                      : f->type() == Function::EFXType ? "efx" : "matrix", f->id());
    }
    const auto idOf = [&](const QString &name) { return others.contains(name) ? others.value(name) : scenes.at(name.toInt()); };
    QVERIFY(song.director.createBlocks(song.show, song.reader));
    Track *director = song.director.track(song.show);
    Track *track = onDirectorTrack ? director : song.show->tracks().first();
    const QStringList before = items(&song.doc, director);
    const QList<SongDirectorBlock> generated = song.director.blocks(song.show);

    QList<quint32> ids;
    for (const QString &d : dropped)
        ids << idOf(d);

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
                want << idOf(e);
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
    const QStringList baseItems { "intro 1 @0 +9000 {}", "buildup 1 @9000 +8000 {}",
                                  "drop 1 @17000 +16000 {}", "outro 1 @33000 +8000 {}" };
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
        << QStringList { "intro 1 @0 +8000 {}", "buildup 1 @8000 +8000 {}",
                         "drop 1 @16000 +16000 {}", "outro 1 @32000 +8000 {}" }
        << earlierSidecar << 4 << 1;
    const QStringList earlierItems { "intro 1 @0 +8000 {}", "buildup 1 @8000 +8000 {}", "drop 1 @16000 +16000 {}", "outro 1 @32000 +8000 {}" };
    QTest::newRow("moved block snaps back to its section") << "move drop" << earlier << earlierItems << earlierSidecar << 4 << 1;
    QTest::newRow("resized block snaps back to its section") << "shorten drop" << earlier << earlierItems << earlierSidecar << 4 << 1;
    QTest::newRow("filled block follows its section with its content") << "fill drop" << earlier
        << QStringList { "intro 1 @0 +8000 {}", "buildup 1 @8000 +8000 {}", "drop 1 @16000 +16000 {Extra}", "outro 1 @32000 +8000 {}" }
        << earlierSidecar << 4 << 1;
    QTest::newRow("deleted block item comes back at its section") << "delete drop" << earlier << earlierItems
        << earlierSidecar << 4 << 1;
    QTest::newRow("added section gets a pre-filled block")
        << "none" << QList<EdmSection> { section("intro", 0, 9000, 100), section("buildup", 9000, 17000, 40, true),
                                         section("drop", 17000, 33000, 90), section("breakdown", 33000, 37000, 70),
                                         section("outro", 37000, 45000, 60) }
        << QStringList { "intro 1 @0 +9000 {}", "buildup 1 @9000 +8000 {}", "drop 1 @17000 +16000 {}",
                         "breakdown 1 @33000 +4000 {Calm}", "outro 1 @37000 +8000 {}" }
        << QStringList { "intro 1 @0 +9000 c100 u0", "buildup 1 @9000 +8000 c40 u1", "drop 1 @17000 +16000 c90 u0",
                         "breakdown 1 @33000 +4000 c70 u0", "outro 1 @37000 +8000 c60 u0" } << 5 << 1;
    QTest::newRow("removed section takes its untouched block") << "none" << noBuildup
        << QStringList { "intro 1 @0 +9000 {}", "drop 1 @9000 +24000 {}", "outro 1 @33000 +8000 {}" }
        << QStringList { "intro 1 @0 +9000 c100 u0", "drop 1 @9000 +24000 c90 u0", "outro 1 @33000 +8000 c60 u0" } << 3 << 1;
    QTest::newRow("content of a removed section joins the block now covering its time") << "fill buildup" << noBuildup
        << QStringList { "intro 1 @0 +9000 {}", "drop 1 @9000 +24000 {Extra}", "outro 1 @33000 +8000 {}" }
        << QStringList { "intro 1 @0 +9000 c100 u0", "drop 1 @9000 +24000 c90 u0", "outro 1 @33000 +8000 c60 u0" } << 3 << 1;
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
        if (edit.endsWith(" " + song.doc.function(sf->functionID())->name().section(' ', 0, 0)))
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

void SongDirector_Test::removedBlockStopsFirst_data()
{
    QTest::addColumn<bool>("running");   // the block's Collection started by hand before the analysis changed

    QTest::newRow("running block") << true;
    QTest::newRow("stopped block") << false;
}

void SongDirector_Test::removedBlockStopsFirst()
{
    QFETCH(bool, running);

    SongShow song;
    song.director.sync(song.reader);
    Track *track = song.director.track(song.show);
    QVERIFY(track != nullptr);
    quint32 buildup = Function::invalidId();
    for (const SongDirectorBlock &b : song.director.blocks(song.show))
        if (b.generated.label == "buildup")
            buildup = b.functionId;
    auto *collection = qobject_cast<Collection *>(song.doc.function(buildup));
    QVERIFY(collection != nullptr);

    // what happens to the block's Collection, in order
    QStringList events;
    connect(collection, qOverload<quint32>(&Function::stopped), this, [&events]() { events << "stopped"; }, Qt::DirectConnection);
    connect(&song.doc, &Doc::functionRemoved, this, [&events, buildup](quint32 id) {
        if (id == buildup)
            events << "removed";
    });
    MasterTimer *timer = song.doc.masterTimer();
    timer->start();
    auto stopTimer = qScopeGuard([timer]() { timer->stop(); });
    if (running)
    {
        collection->start(timer, FunctionParent::master());
        QTRY_VERIFY(collection->isRunning());
    }

    song.analyse(finalJson({ section("intro", 0, 9000, 100), section("drop", 9000, 33000, 90), section("outro", 33000, 41000, 60) }));
    song.director.sync(song.reader);

    QVERIFY(song.doc.function(buildup) == nullptr);
    QCOMPARE(track->showFunctions().count(), 3);
    QCOMPARE(events, (running ? QStringList { "stopped", "removed" } : QStringList { "removed" }));
}

void SongDirector_Test::goneSectionContentJoins_data()
{
    QTest::addColumn<bool>("reopen");   // the workspace saved and reopened while the section was gone

    QTest::newRow("same session") << false;
    QTest::newRow("saved and reopened") << true;
}

void SongDirector_Test::goneSectionContentJoins()
{
    QFETCH(bool, reopen);

    const QList<EdmSection> base { section("intro", 0, 9000, 100), section("buildup", 9000, 17000, 40, true),
                                   section("drop", 17000, 33000, 90), section("outro", 33000, 41000, 60) };
    SongShow song(0, kSongPath, false);
    song.analyse(finalJson(base));
    Scene *extra = new Scene(&song.doc);
    extra->setName("Extra");
    QVERIFY(song.doc.addFunction(extra));
    song.director.sync(song.reader);
    Track *track = song.director.track(song.show);
    QVERIFY(track != nullptr);

    // the user fills the buildup, then the buildup leaves the analysis: its content joins the drop
    for (ShowFunction *sf : track->showFunctions())
        if (song.doc.function(sf->functionID())->name() == "buildup 1")
            qobject_cast<Collection *>(song.doc.function(sf->functionID()))->addFunction(extra->id());
    song.analyse(finalJson({ section("intro", 0, 9000, 100), section("drop", 9000, 33000, 90), section("outro", 33000, 41000, 60) }));
    song.director.sync(song.reader);
    QCOMPARE(itemsWithContent(&song.doc, track),
             (QStringList { "intro 1 @0 +9000 {}", "drop 1 @9000 +24000 {Extra}", "outro 1 @33000 +8000 {}" }));
    QCOMPARE(collections(&song.doc), 3);

    Doc *doc = &song.doc;
    SongDirector *director = &song.director;
    Show *show = song.show;
    Doc reopenedDoc(nullptr);
    SongDirector reopened(&reopenedDoc);
    if (reopen)
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
        track = director->track(show);
        QVERIFY(track != nullptr);
    }

    // a buildup comes back: a new block with the label's default, the drop keeps the joined content
    song.analyse(finalJson(base));
    director->sync(song.reader);
    QCOMPARE(itemsWithContent(doc, track),
             (QStringList { "intro 1 @0 +9000 {}", "buildup 1 @9000 +8000 {}", "drop 1 @17000 +16000 {Extra}", "outro 1 @33000 +8000 {}" }));
    QCOMPARE(collections(doc), 4);
    QCOMPARE(director->blocks(show).count(), 4);

    // and all follow the next analysis
    song.analyse(finalJson({ section("intro", 0, 8000, 100), section("buildup", 8000, 16000, 40, true),
                             section("drop", 16000, 32000, 90), section("outro", 32000, 40000, 60) }));
    director->sync(song.reader);
    QCOMPARE(itemsWithContent(doc, track),
             (QStringList { "intro 1 @0 +8000 {}", "buildup 1 @8000 +8000 {}", "drop 1 @16000 +16000 {Extra}",
                            "outro 1 @32000 +8000 {}" }));
}

namespace
{

/** intro 0-8 s, buildup 8-16 s (unsure), drop 16-32 s, outro 32-40 s; bars of 2 s from 0 */
const QList<EdmSection> kBarSections { section("intro", 0, 8000, 100), section("buildup", 8000, 16000, 40, true),
                                       section("drop", 16000, 32000, 90), section("outro", 32000, 40000, 60) };

/** "drop* 16000-24000 c100 u0": label, edited, start, end, label confidence, unsure */
QStringList describe(const QList<SongSection> &sections)
{
    QStringList list;
    for (const SongSection &s : sections)
        list << QString("%1%2 %3-%4 c%5 u%6").arg(s.label, s.edited ? "*" : "").arg(s.startMs).arg(s.endMs)
                    .arg(s.labelConfidence).arg(s.unsure ? 1 : 0);
    return list;
}

/** "move 1 13100", "relabel 1 breakdown", "split 2 23500", "merge 1", "reset 1", "reset_all" */
SectionAction action(const QString &text)
{
    const QStringList words = text.split(' ');
    SectionAction a;
    const bool known = sectionActionType(words.at(0) == "move" ? "move_boundary" : words.at(0), &a.type);
    Q_ASSERT(known);
    a.section = words.value(1, "-1").toInt();
    if (a.type == SectionAction::Relabel)
        a.label = words.value(2);
    else
        a.timeMs = words.value(2, "0").toDouble();
    return a;
}

/** The edits after each action of "a; b; c", the error of the last one */
SectionEdits edit(const EdmAnalysis &analysis, const QString &actions, QString *error)
{
    SectionEdits edits;
    for (const QString &text : actions.split("; ", Qt::SkipEmptyParts))
        edits = applySectionAction(analysis, edits, action(text), error);
    return edits;
}

}

void SongDirector_Test::sectionActions_data()
{
    QTest::addColumn<QString>("actions");
    QTest::addColumn<QStringList>("expected");   // the effective sections afterwards
    QTest::addColumn<bool>("refused");           // the last action

    const QStringList base { "intro 0-8000 c100 u0", "buildup 8000-16000 c40 u1", "drop 16000-32000 c90 u0", "outro 32000-40000 c60 u0" };
    QTest::newRow("no edits") << "" << base << false;
    QTest::newRow("move boundary later, snapped to a bar, both neighbours edited") << "move 1 13100"
        << QStringList { "intro 0-8000 c100 u0", "buildup* 8000-14000 c100 u0", "drop* 14000-32000 c100 u0", "outro 32000-40000 c60 u0" } << false;
    QTest::newRow("move boundary earlier") << "move 0 5900"
        << QStringList { "intro* 0-6000 c100 u0", "buildup* 6000-16000 c100 u0", "drop 16000-32000 c90 u0", "outro 32000-40000 c60 u0" } << false;
    QTest::newRow("move boundary snapped onto a section edge is refused") << "move 1 8600" << base << true;
    QTest::newRow("move boundary past the next section is refused") << "move 1 33000" << base << true;
    QTest::newRow("the last section has no boundary to move") << "move 3 38000" << base << true;
    QTest::newRow("move an edited boundary again") << "move 1 12000; move 1 10000"
        << QStringList { "intro 0-8000 c100 u0", "buildup* 8000-10000 c100 u0", "drop* 10000-32000 c100 u0", "outro 32000-40000 c60 u0" } << false;
    QTest::newRow("relabel") << "relabel 1 breakdown"
        << QStringList { "intro 0-8000 c100 u0", "breakdown* 8000-16000 c100 u0", "drop 16000-32000 c90 u0", "outro 32000-40000 c60 u0" } << false;
    QTest::newRow("relabel to its own label confirms it") << "relabel 1 buildup"
        << QStringList { "intro 0-8000 c100 u0", "buildup* 8000-16000 c100 u0", "drop 16000-32000 c90 u0", "outro 32000-40000 c60 u0" } << false;
    QTest::newRow("relabel to an unknown label is refused") << "relabel 1 chorus" << base << true;
    QTest::newRow("split, snapped to a bar") << "split 2 23500"
        << QStringList { "intro 0-8000 c100 u0", "buildup 8000-16000 c40 u1", "drop* 16000-24000 c100 u0", "drop* 24000-32000 c100 u0",
                         "outro 32000-40000 c60 u0" } << false;
    QTest::newRow("split snapped onto the section start is refused") << "split 2 16400" << base << true;
    QTest::newRow("split outside the section is refused") << "split 2 35000" << base << true;
    QTest::newRow("merge with the next, first label wins") << "merge 1"
        << QStringList { "intro 0-8000 c100 u0", "buildup* 8000-32000 c100 u0", "outro 32000-40000 c60 u0" } << false;
    QTest::newRow("the last section has no next to merge") << "merge 3" << base << true;
    QTest::newRow("split then merge back") << "split 2 24000; merge 2"
        << QStringList { "intro 0-8000 c100 u0", "buildup 8000-16000 c40 u1", "drop* 16000-32000 c100 u0", "outro 32000-40000 c60 u0" } << false;
    QTest::newRow("reset one") << "relabel 1 breakdown; reset 1" << base << false;
    QTest::newRow("reset one keeps the other edits") << "relabel 1 breakdown; relabel 3 groove; reset 1"
        << QStringList { "intro 0-8000 c100 u0", "buildup 8000-16000 c40 u1", "drop 16000-32000 c90 u0", "groove* 32000-40000 c100 u0" } << false;
    QTest::newRow("reset one of a moved boundary brings back only its own analysis") << "move 1 12000; reset 2"
        << QStringList { "intro 0-8000 c100 u0", "buildup* 8000-12000 c100 u0", "buildup 12000-16000 c40 u1", "drop 16000-32000 c90 u0",
                         "outro 32000-40000 c60 u0" } << false;
    QTest::newRow("reset of a merged section brings back both") << "merge 1; reset 1" << base << false;
    QTest::newRow("reset of an unedited section changes nothing") << "relabel 3 groove; reset 0"
        << QStringList { "intro 0-8000 c100 u0", "buildup 8000-16000 c40 u1", "drop 16000-32000 c90 u0", "groove* 32000-40000 c100 u0" } << false;
    QTest::newRow("reset all") << "move 1 12000; relabel 3 groove; split 0 4000; reset_all" << base << false;
    QTest::newRow("unknown section is refused") << "relabel 7 drop" << base << true;
    QTest::newRow("negative section is refused") << "merge -1" << base << true;
}

void SongDirector_Test::sectionActions()
{
    QFETCH(QString, actions);
    QFETCH(QStringList, expected);
    QFETCH(bool, refused);

    const EdmAnalysis analysis = parseEdmAnalysis(finalJson(kBarSections));
    QVERIFY(analysis.valid);
    QString error;
    const SectionEdits edits = edit(analysis, actions, &error);
    QCOMPARE(describe(effectiveSections(analysis, edits)), expected);
    QCOMPARE(error.isEmpty(), refused == false);

    // a refused action leaves the edits as they were
    if (refused)
    {
        const QString before = actions.section("; ", 0, -2);
        QString ignored;
        QCOMPARE(edits, edit(analysis, before, &ignored));
    }
}

void SongDirector_Test::editsOverReanalysis_data()
{
    QTest::addColumn<QString>("actions");               // on the first analysis
    QTest::addColumn<QList<EdmSection>>("reanalysis");
    QTest::addColumn<QStringList>("expected");

    const QList<EdmSection> later { section("intro", 0, 10000, 100), section("buildup", 10000, 18000, 40, true),
                                    section("drop", 18000, 34000, 90), section("outro", 34000, 42000, 60) };
    QTest::newRow("no edits: the new analysis") << "" << later
        << QStringList { "intro 0-10000 c100 u0", "buildup 10000-18000 c40 u1", "drop 18000-34000 c90 u0", "outro 34000-42000 c60 u0" };
    QTest::newRow("an edited section stays, the analysis fills around it") << "relabel 1 breakdown" << later
        << QStringList { "intro 0-8000 c100 u0", "breakdown* 8000-16000 c100 u0", "buildup 16000-18000 c40 u1", "drop 18000-34000 c90 u0",
                         "outro 34000-42000 c60 u0" };
    QTest::newRow("a moved boundary stays") << "move 1 12000" << later
        << QStringList { "intro 0-8000 c100 u0", "buildup* 8000-12000 c100 u0", "drop* 12000-32000 c100 u0", "drop 32000-34000 c90 u0",
                         "outro 34000-42000 c60 u0" };
    QTest::newRow("the edited section's counterpart is gone: the edit stays")
        << "relabel 1 breakdown"
        << QList<EdmSection> { section("intro", 0, 8000, 100), section("drop", 8000, 32000, 90), section("outro", 32000, 40000, 60) }
        << QStringList { "intro 0-8000 c100 u0", "breakdown* 8000-16000 c100 u0", "drop 16000-32000 c90 u0", "outro 32000-40000 c60 u0" };
    QTest::newRow("a piece under half a bar joins its analysis neighbour")
        << "relabel 1 breakdown"
        << QList<EdmSection> { section("intro", 0, 8500, 100), section("buildup", 8500, 16500, 40, true),
                               section("drop", 16500, 32500, 90), section("outro", 32500, 40500, 60) }
        << QStringList { "intro 0-8000 c100 u0", "breakdown* 8000-16000 c100 u0", "drop 16000-32500 c90 u0", "outro 32500-40500 c60 u0" };
    QTest::newRow("an edit past the new analysis end stays") << "relabel 3 groove"
        << QList<EdmSection> { section("intro", 0, 8000, 100), section("drop", 8000, 30000, 90) }
        << QStringList { "intro 0-8000 c100 u0", "drop 8000-30000 c90 u0", "groove* 32000-40000 c100 u0" };
}

void SongDirector_Test::editsOverReanalysis()
{
    QFETCH(QString, actions);
    QFETCH(QList<EdmSection>, reanalysis);
    QFETCH(QStringList, expected);

    QString error;
    const SectionEdits edits = edit(parseEdmAnalysis(finalJson(kBarSections)), actions, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(describe(effectiveSections(parseEdmAnalysis(finalJson(reanalysis)), edits)), expected);
}

namespace
{

/** A song Show with the clip 30 s in, the bar sections, blocks created with
 *  the defaults drop -> "Drop", breakdown -> "Calm" */
struct EditedSong
{
    EditedSong()
        : song(30000, kSongPath, false)
        , mapping(&song.doc)
    {
        song.analyse(finalJson(kBarSections));
        for (const char *name : {"Drop", "Calm", "Extra"})
        {
            Scene *scene = new Scene(&song.doc);
            scene->setName(name);
            song.doc.addFunction(scene);
            scenes.insert(name, scene->id());
        }
        mapping.setFunctionId("drop", int(scenes.value("Drop")));
        mapping.setFunctionId("breakdown", int(scenes.value("Calm")));
        song.director.setMapping(&mapping);
        song.director.sync(song.reader);
    }

    /** Each action of "a; b" in Show time; the result of the last one */
    bool edit(const QString &actions, QString *error)
    {
        bool done = true;
        for (const QString &text : actions.split("; ", Qt::SkipEmptyParts))
            done = song.director.editSections(song.show, action(text), song.reader, error);
        return done;
    }

    SongShow song;
    EdmStructureMapping mapping;
    QMap<QString, quint32> scenes;
};

}

void SongDirector_Test::blocksFollowSectionEdits_data()
{
    QTest::addColumn<QString>("actions");               // Show time: the clip starts at 30 s
    QTest::addColumn<bool>("fill");                     // the user adds "Extra" to the drop block first
    QTest::addColumn<QList<EdmSection>>("reanalysis");  // afterwards, none when empty
    QTest::addColumn<bool>("refused");
    QTest::addColumn<QStringList>("expected");
    QTest::addColumn<int>("expectedCollections");

    const QList<EdmSection> none;
    const QStringList base { "intro 1 @30000 +8000 {}", "buildup 1 @38000 +8000 {}", "drop 1 @46000 +16000 {Drop}",
                             "outro 1 @62000 +8000 {}" };
    QTest::newRow("no edits") << "" << false << none << false << base << 4;
    QTest::newRow("move boundary: both blocks follow, snapped to a bar") << "move 1 43100" << false << none << false
        << QStringList { "intro 1 @30000 +8000 {}", "buildup 1 @38000 +6000 {}", "drop 1 @44000 +18000 {Drop}",
                         "outro 1 @62000 +8000 {}" } << 4;
    QTest::newRow("relabel: content as generated takes the new default") << "relabel 1 breakdown" << false << none << false
        << QStringList { "intro 1 @30000 +8000 {}", "breakdown 1 @38000 +8000 {Calm}", "drop 1 @46000 +16000 {Drop}",
                         "outro 1 @62000 +8000 {}" } << 4;
    QTest::newRow("relabel: the user's content stays") << "relabel 2 groove" << true << none << false
        << QStringList { "intro 1 @30000 +8000 {}", "buildup 1 @38000 +8000 {}", "groove 1 @46000 +16000 {Drop,Extra}",
                         "outro 1 @62000 +8000 {}" } << 4;
    QTest::newRow("relabel renumbers the names") << "relabel 0 drop" << false << none << false
        << QStringList { "drop 1 @30000 +8000 {Drop}", "buildup 1 @38000 +8000 {}", "drop 2 @46000 +16000 {Drop}",
                         "outro 1 @62000 +8000 {}" } << 4;
    QTest::newRow("split: a new block with the default") << "split 2 53500" << false << none << false
        << QStringList { "intro 1 @30000 +8000 {}", "buildup 1 @38000 +8000 {}", "drop 1 @46000 +8000 {Drop}",
                         "drop 2 @54000 +8000 {Drop}", "outro 1 @62000 +8000 {}" } << 5;
    QTest::newRow("split: the new block copies the user's content") << "split 2 53500" << true << none << false
        << QStringList { "intro 1 @30000 +8000 {}", "buildup 1 @38000 +8000 {}", "drop 1 @46000 +8000 {Drop,Extra}",
                         "drop 2 @54000 +8000 {Drop,Extra}", "outro 1 @62000 +8000 {}" } << 5;
    QTest::newRow("merge: the untouched second block goes") << "merge 1" << false << none << false
        << QStringList { "intro 1 @30000 +8000 {}", "buildup 1 @38000 +24000 {}", "outro 1 @62000 +8000 {}" } << 3;
    QTest::newRow("merge: the user's content of the second block joins") << "merge 1" << true << none << false
        << QStringList { "intro 1 @30000 +8000 {}", "buildup 1 @38000 +24000 {Drop,Extra}", "outro 1 @62000 +8000 {}" } << 3;
    QTest::newRow("reset one") << "relabel 1 breakdown; reset 1" << false << none << false << base << 4;
    QTest::newRow("reset all after a merge brings the second block back") << "move 0 35000; merge 1; reset_all" << false << none << false
        << base << 4;
    QTest::newRow("refused: nothing changes") << "merge 3" << false << none << true << base << 4;
    QTest::newRow("re-analysis without edits: all blocks follow") << "" << false
        << QList<EdmSection> { section("intro", 0, 10000, 100), section("buildup", 10000, 18000, 40, true),
                               section("drop", 18000, 34000, 90), section("outro", 34000, 42000, 60) } << false
        << QStringList { "intro 1 @30000 +10000 {}", "buildup 1 @40000 +8000 {}", "drop 1 @48000 +16000 {Drop}",
                         "outro 1 @64000 +8000 {}" } << 4;
    QTest::newRow("re-analysis keeps the edited section, the rest follows") << "relabel 1 breakdown" << false
        << QList<EdmSection> { section("intro", 0, 10000, 100), section("buildup", 10000, 18000, 40, true),
                               section("drop", 18000, 34000, 90), section("outro", 34000, 42000, 60) } << false
        << QStringList { "intro 1 @30000 +8000 {}", "breakdown 1 @38000 +8000 {Calm}", "buildup 1 @46000 +2000 {}",
                         "drop 1 @48000 +16000 {Drop}", "outro 1 @64000 +8000 {}" } << 5;
}

void SongDirector_Test::blocksFollowSectionEdits()
{
    QFETCH(QString, actions);
    QFETCH(bool, fill);
    QFETCH(QList<EdmSection>, reanalysis);
    QFETCH(bool, refused);
    QFETCH(QStringList, expected);
    QFETCH(int, expectedCollections);

    EditedSong edited;
    SongShow &song = edited.song;
    Track *track = song.director.track(song.show);
    QVERIFY(track != nullptr);
    if (fill)
    {
        for (const SongDirectorBlock &b : song.director.blocks(song.show))
            if (b.generated.label == "drop")
                qobject_cast<Collection *>(song.doc.function(b.functionId))->addFunction(edited.scenes.value("Extra"));
    }
    QSignalSpy sections(&song.director, &SongDirector::sectionsChanged);
    QString error;
    QCOMPARE(edited.edit(actions, &error), refused == false);
    QCOMPARE(error.isEmpty(), refused == false);
    if (refused)
        QCOMPARE(sections.count(), 0);
    if (reanalysis.isEmpty() == false)
    {
        song.analyse(finalJson(reanalysis));
        song.director.sync(song.reader);
    }
    QCOMPARE(itemsWithContent(&song.doc, track), expected);
    QCOMPARE(collections(&song.doc), expectedCollections);
    QCOMPARE(song.director.blocks(song.show).count(), expected.count());

    // the header's sections are the blocks' sections
    QStringList headers;
    for (const SongSection &s : song.director.sections(song.show, song.reader))
        headers << QString("%1 @%2 +%3").arg(s.label).arg(s.startMs).arg(s.endMs - s.startMs);
    QStringList blocks;
    for (const QString &item : expected)
        blocks << item.section(' ', 0, 0) + " " + item.section(' ', 2, 3);
    QCOMPARE(headers, blocks);

    // a sync of the same analysis changes nothing
    QSignalSpy changed(&song.director, &SongDirector::blocksChanged);
    song.director.sync(song.reader);
    QCOMPARE(changed.count(), 0);
}

void SongDirector_Test::sectionEditsRoundTrip()
{
    EditedSong edited;
    SongShow &song = edited.song;
    QString error;
    QVERIFY2(edited.edit("move 1 43100; relabel 3 groove; split 0 35000", &error), qPrintable(error));
    const SectionEdits edits = song.director.sectionEdits(song.show);
    QCOMPARE(edits.pins.count(), 5);
    const QStringList expectedItems = itemsWithContent(&song.doc, song.director.track(song.show));

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
    QCOMPARE(director.sectionEdits(show), edits);
    QSignalSpy changed(&director, &SongDirector::blocksChanged);
    director.sync(song.reader);
    QCOMPARE(changed.count(), 0);
    QCOMPARE(itemsWithContent(&doc, director.track(show)), expectedItems);

    // reset all leaves nothing to save
    QVERIFY(director.editSections(show, action("reset_all"), song.reader, &error));
    QString empty;
    QXmlStreamWriter emptyWriter(&empty);
    director.saveXML(&emptyWriter);
    QVERIFY(empty.contains("<Sections") == false);
}

void SongDirector_Test::oldBlocksSnapBackOnLoad()
{
    // a workspace of the Song Director before section edits: names with the song title, a moved and a resized block
    EditedSong edited;
    SongShow &song = edited.song;
    Track *track = song.director.track(song.show);
    for (ShowFunction *sf : track->showFunctions())
    {
        Function *f = song.doc.function(sf->functionID());
        f->setName("Song / " + f->name());
        if (f->name().endsWith("drop 1"))
            sf->setStartTime(sf->startTime() + 1000);
        if (f->name().endsWith("outro 1"))
            sf->setDuration(3000);
    }
    QSignalSpy changed(&song.director, &SongDirector::blocksChanged);
    song.director.sync(song.reader);
    QCOMPARE(changed.count(), 1);
    QCOMPARE(itemsWithContent(&song.doc, track),
             (QStringList { "intro 1 @30000 +8000 {}", "buildup 1 @38000 +8000 {}", "drop 1 @46000 +16000 {Drop}",
                            "outro 1 @62000 +8000 {}" }));
}

void SongDirector_Test::shortPieceBetweenPins_data()
{
    QTest::addColumn<QStringList>("pins");       // "label start end", song time
    QTest::addColumn<QStringList>("expected");
    QTest::addColumn<QStringList>("afterReset"); // after a reset of section 0

    QTest::newRow("a piece under half a bar between two pins joins the previous pin")
        << QStringList { "groove 0 8000", "breakdown 8500 16000" }
        << QStringList { "groove* 0-8500 c100 u0", "breakdown* 8500-16000 c100 u0", "drop 16000-32000 c90 u0", "outro 32000-40000 c60 u0" }
        << QStringList { "intro 0-8500 c100 u0", "breakdown* 8500-16000 c100 u0", "drop 16000-32000 c90 u0", "outro 32000-40000 c60 u0" };
    QTest::newRow("a piece under half a bar before the first pin joins it")
        << QStringList { "groove 500 16000" }
        << QStringList { "groove* 0-16000 c100 u0", "drop 16000-32000 c90 u0", "outro 32000-40000 c60 u0" }
        << QStringList { "intro 0-8000 c100 u0", "buildup 8000-16000 c40 u1", "drop 16000-32000 c90 u0", "outro 32000-40000 c60 u0" };
    QTest::newRow("a piece under half a bar next to the analysis joins the analysis")
        << QStringList { "breakdown 8500 16000" }
        << QStringList { "intro 0-8500 c100 u0", "breakdown* 8500-16000 c100 u0", "drop 16000-32000 c90 u0", "outro 32000-40000 c60 u0" }
        << QStringList { "intro 0-8500 c100 u0", "breakdown* 8500-16000 c100 u0", "drop 16000-32000 c90 u0", "outro 32000-40000 c60 u0" };
}

void SongDirector_Test::shortPieceBetweenPins()
{
    QFETCH(QStringList, pins);
    QFETCH(QStringList, expected);
    QFETCH(QStringList, afterReset);

    const EdmAnalysis analysis = parseEdmAnalysis(finalJson(kBarSections));
    SectionEdits edits;
    for (const QString &pin : pins)
        edits.pins.append({ pin.section(' ', 0, 0), pin.section(' ', 1, 1).toDouble(), pin.section(' ', 2, 2).toDouble() });
    QCOMPARE(describe(effectiveSections(analysis, edits)), expected);
    QString error;
    const SectionEdits reset = applySectionAction(analysis, edits, action("reset 0"), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(describe(effectiveSections(analysis, reset)), afterReset);
}

void SongDirector_Test::splitCopyIsUserContent_data()
{
    QTest::addColumn<int>("splitAt");          // Show time; the user filled the drop with "Extra" before
    QTest::addColumn<bool>("clearFirst");      // the user empties the first half after the split
    QTest::addColumn<QString>("then");
    QTest::addColumn<QStringList>("expected");
    QTest::addColumn<bool>("firstSurvives");   // the drop's Collection is the merged block's

    QTest::newRow("split then relabel keeps the copy") << 53500 << false << "relabel 3 groove"
        << QStringList { "intro 1 @30000 +8000 {}", "buildup 1 @38000 +8000 {}", "drop 1 @46000 +8000 {Drop,Extra}",
                         "groove 1 @54000 +8000 {Drop,Extra}", "outro 1 @62000 +8000 {}" } << true;
    QTest::newRow("split then merge passes the copy on") << 53500 << true << "merge 2"
        << QStringList { "intro 1 @30000 +8000 {}", "buildup 1 @38000 +8000 {}", "drop 1 @46000 +16000 {Drop,Extra}",
                         "outro 1 @62000 +8000 {}" } << true;
    QTest::newRow("merge keeps the first block, also when it is the shorter") << 51000 << false << "merge 2"
        << QStringList { "intro 1 @30000 +8000 {}", "buildup 1 @38000 +8000 {}", "drop 1 @46000 +16000 {Drop,Extra}",
                         "outro 1 @62000 +8000 {}" } << true;
}

void SongDirector_Test::splitCopyIsUserContent()
{
    QFETCH(int, splitAt);
    QFETCH(bool, clearFirst);
    QFETCH(QString, then);
    QFETCH(QStringList, expected);
    QFETCH(bool, firstSurvives);

    EditedSong edited;
    SongShow &song = edited.song;
    quint32 drop = Function::invalidId();
    for (const SongDirectorBlock &b : song.director.blocks(song.show))
        if (b.generated.label == "drop")
            drop = b.functionId;
    auto *first = qobject_cast<Collection *>(song.doc.function(drop));
    first->addFunction(edited.scenes.value("Extra"));
    QString error;
    QVERIFY2(edited.edit(QString("split 2 %1").arg(splitAt), &error), qPrintable(error));
    // the first half keeps the block, the second is the copy
    bool firstHalf = false;
    for (const SongDirectorBlock &b : song.director.blocks(song.show))
        firstHalf |= b.functionId == drop && b.generated.startMs == 46000;
    QVERIFY(firstHalf);
    if (clearFirst)
        for (quint32 id : first->functions())
            first->removeFunction(id);
    QVERIFY2(edited.edit(then, &error), qPrintable(error));

    QCOMPARE(itemsWithContent(&song.doc, song.director.track(song.show)), expected);
    bool survived = false;
    for (const SongDirectorBlock &b : song.director.blocks(song.show))
        survived |= b.functionId == drop && b.generated.startMs == 46000;
    QCOMPARE(survived, firstSurvives);
}

void SongDirector_Test::editsOnlyOnSongShows_data()
{
    QTest::addColumn<bool>("songShow");   // the Show is Perform's Show of its song
    QTest::newRow("the song's Show") << true;
    QTest::newRow("another Show with the analysed song") << false;
}

void SongDirector_Test::editsOnlyOnSongShows()
{
    QFETCH(bool, songShow);

    SongShow song;
    song.director.setSongShowFilter([songShow](Show *) { return songShow; });
    QString error;
    QCOMPARE(song.director.editSections(song.show, action("relabel 1 breakdown"), song.reader, &error), songShow);
    QCOMPARE(error.isEmpty(), songShow);
    QCOMPARE(song.director.sectionEdits(song.show).isEmpty(), songShow == false);
    QCOMPARE(song.director.track(song.show) != nullptr, songShow);
}

void SongDirector_Test::editWhileShowRuns()
{
    EditedSong edited;
    SongShow &song = edited.song;
    song.director.setAnalysisDir(song.dir.path());
    const QStringList before = itemsWithContent(&song.doc, song.director.track(song.show));

    MasterTimer *timer = song.doc.masterTimer();
    timer->start();
    auto stopTimer = qScopeGuard([timer]() { timer->stop(); });
    song.show->start(timer, FunctionParent::master());
    QTRY_VERIFY(song.show->isRunning());

    // the section band changes at once, the blocks wait for the stop
    QSignalSpy sections(&song.director, &SongDirector::sectionsChanged);
    QString error;
    QVERIFY2(edited.edit("relabel 1 breakdown", &error), qPrintable(error));
    QCOMPARE(sections.count(), 1);
    QCOMPARE(song.director.sections(song.show, song.reader).at(1).label, QString("breakdown"));
    QCOMPARE(itemsWithContent(&song.doc, song.director.track(song.show)), before);

    song.show->stopAndWait();
    QTRY_COMPARE(itemsWithContent(&song.doc, song.director.track(song.show)),
                 (QStringList { "intro 1 @30000 +8000 {}", "breakdown 1 @38000 +8000 {Calm}", "drop 1 @46000 +16000 {Drop}",
                                "outro 1 @62000 +8000 {}" }));
}

QTEST_GUILESS_MAIN(SongDirector_Test)
