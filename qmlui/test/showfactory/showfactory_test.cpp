/*
  Q Light Controller Plus - Unit test
  showfactory_test.cpp

  Licensed under the Apache License, Version 2.0
*/

#include <QtTest>
#include <QSignalSpy>

#include "showfactory_test.h"
#include "showfactory.h"
#include "djfsm.h"

#include "doc.h"
#include "show.h"
#include "audio.h"
#include "track.h"
#include "function.h"

static Doc *createDoc()
{
    Doc *doc = new Doc(nullptr, 4);
    return doc;
}

void ShowFactory_Test::initTestCase()
{
    m_doc = createDoc();
}

void ShowFactory_Test::cleanupTestCase()
{
    delete m_doc;
    m_doc = nullptr;
}

// ── Parametrized: verify Audio + Show + Track created ──

void ShowFactory_Test::createShowForSong_data()
{
    QTest::addColumn<QString>("filepath");
    QTest::addColumn<QString>("title");
    QTest::addColumn<QString>("artist");
    QTest::addColumn<double>("bpm");
    QTest::addColumn<QString>("expectedName");

    QTest::newRow("artist_and_title")
        << "/music/track1.mp3" << "My Song" << "DJ Alpha" << 128.0
        << "DJ Alpha - My Song";

    QTest::newRow("title_only")
        << "/music/track2.mp3" << "Instrumental" << "" << 140.0
        << "Instrumental";

    QTest::newRow("filename_fallback")
        << "/music/cool beat.flac" << "" << "" << 120.0
        << "cool beat";
}

void ShowFactory_Test::createShowForSong()
{
    QFETCH(QString, filepath);
    QFETCH(QString, title);
    QFETCH(QString, artist);
    QFETCH(double, bpm);
    QFETCH(QString, expectedName);

    // Fresh Doc for each row to avoid cross-contamination
    Doc *doc = createDoc();
    ShowFactory factory(doc);
    QSignalSpy spy(&factory, &ShowFactory::showCreatedForSong);

    DjFsm::DeckSong info;
    info.filepath = filepath;
    info.title = title;
    info.artist = artist;
    info.bpm = bpm;

    factory.createShowForSong(info);

    // Wait for the deferred timer (3000ms + margin)
    QVERIFY(spy.wait(4000));
    QCOMPARE(spy.count(), 1);

    quint32 showId = spy[0][1].toUInt();
    QVERIFY(showId != Function::invalidId());

    // Verify the Show exists in Doc with the expected name
    Function *showFn = doc->function(showId);
    QVERIFY(showFn != nullptr);
    QCOMPARE(showFn->type(), Function::ShowType);
    QCOMPARE(showFn->name(), expectedName);

    // Verify an Audio function also exists
    bool foundAudio = false;
    for (Function *f : doc->functionsByType(Function::AudioType))
    {
        Audio *audio = qobject_cast<Audio*>(f);
        if (audio && audio->getSourceFileName() == filepath)
        {
            foundAudio = true;
            break;
        }
    }
    QVERIFY(foundAudio);

    // Verify showIdForFilepath returns the correct ID
    QCOMPARE(factory.showIdForFilepath(filepath), showId);

    delete doc;
}

// ── Dedup: same filepath twice → only one Show ──

void ShowFactory_Test::dedupByName()
{
    Doc *doc = createDoc();
    ShowFactory factory(doc);
    QSignalSpy spy(&factory, &ShowFactory::showCreatedForSong);

    DjFsm::DeckSong info;
    info.filepath = "/music/dedup.mp3";
    info.title = "Dedup Song";
    info.artist = "Artist";
    info.bpm = 128.0;

    factory.createShowForSong(info);
    factory.createShowForSong(info); // second call — should be deduped

    // Only one timer should fire
    QVERIFY(spy.wait(4000));
    // Allow processing time for second timer (which should not exist)
    QTest::qWait(500);
    QCOMPARE(spy.count(), 1);

    // Verify only one Show in Doc
    auto shows = doc->functionsByType(Function::ShowType);
    int matchCount = 0;
    for (Function *f : shows)
    {
        if (f->name() == "Artist - Dedup Song")
            matchCount++;
    }
    QCOMPARE(matchCount, 1);

    delete doc;
}

// ── showIdForFilepath lookup ──

void ShowFactory_Test::showIdForFilepath()
{
    Doc *doc = createDoc();
    ShowFactory factory(doc);
    QSignalSpy spy(&factory, &ShowFactory::showCreatedForSong);

    // Before any creation, should return invalidId
    QCOMPARE(factory.showIdForFilepath("/nonexistent.mp3"), Function::invalidId());

    DjFsm::DeckSong info;
    info.filepath = "/music/lookup.mp3";
    info.title = "Lookup";
    info.artist = "Finder";
    info.bpm = 130.0;

    factory.createShowForSong(info);
    QVERIFY(spy.wait(4000));

    quint32 id = factory.showIdForFilepath("/music/lookup.mp3");
    QVERIFY(id != Function::invalidId());
    QCOMPARE(factory.showIdForFilepath("/other/path.mp3"), Function::invalidId());

    delete doc;
}

// ── Signal emission with correct args ──

void ShowFactory_Test::signalEmitted()
{
    Doc *doc = createDoc();
    ShowFactory factory(doc);
    QSignalSpy spy(&factory, &ShowFactory::showCreatedForSong);

    DjFsm::DeckSong info;
    info.filepath = "/music/signal.mp3";
    info.title = "Signal Check";
    info.artist = "Emitter";
    info.bpm = 125.0;

    factory.createShowForSong(info);
    QVERIFY(spy.wait(4000));
    QCOMPARE(spy.count(), 1);

    // Verify signal args
    QList<QVariant> args = spy[0];
    QCOMPARE(args[0].toString(), QString("/music/signal.mp3"));
    QVERIFY(args[1].toUInt() != Function::invalidId());

    delete doc;
}

// ── Empty filepath is rejected immediately ──

void ShowFactory_Test::emptyFilepathRejected()
{
    Doc *doc = createDoc();
    ShowFactory factory(doc);
    QSignalSpy spy(&factory, &ShowFactory::showCreatedForSong);

    DjFsm::DeckSong info;
    info.filepath = "";  // empty
    info.title = "Ghost";
    info.artist = "Nobody";
    info.bpm = 120.0;

    factory.createShowForSong(info);

    // No timer should fire — wait briefly to confirm
    QTest::qWait(500);
    QCOMPARE(spy.count(), 0);
    QVERIFY(factory.createdShows().isEmpty());

    delete doc;
}

QTEST_MAIN(ShowFactory_Test)
