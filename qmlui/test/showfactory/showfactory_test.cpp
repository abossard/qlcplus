/*
  Q Light Controller Plus - Unit test
  showfactory_test.cpp

  Licensed under the Apache License, Version 2.0
*/

#include "showfactory_test.h"
#include "showfactory.h"
#include "djfsm.h"

#include "doc.h"
#include "audio.h"
#include "show.h"
#include "track.h"
#include "showfunction.h"
#include "function.h"

#include <QDir>
#include <QSignalSpy>

static DjFsm::DeckSong song(const QString &filepath, const QString &title, const QString &artist, double bpm = 120.0)
{
    DjFsm::DeckSong info;
    info.filepath = filepath;
    info.title = title;
    info.artist = artist;
    info.bpm = bpm;
    return info;
}

/** A Show with one Audio clip of filepath, as a workspace load leaves it */
static Show *addSongShow(Doc *doc, const QString &name, const QString &filepath)
{
    Show *show = new Show(doc);
    show->setName(name);
    doc->addFunction(show);
    Audio *audio = new Audio(doc);
    audio->setSourceFileName(filepath);
    doc->addFunction(audio);
    Track *track = new Track(Function::invalidId(), show);
    show->addTrack(track);
    track->createShowFunction(audio->id());
    return show;
}

void ShowFactory_Test::showNameForSong_data()
{
    QTest::addColumn<QString>("filepath");
    QTest::addColumn<QString>("title");
    QTest::addColumn<QString>("artist");
    QTest::addColumn<QString>("name");

    QTest::newRow("artist and title") << "/music/track1.mp3" << "My Song" << "DJ Alpha" << "DJ Alpha - My Song";
    QTest::newRow("empty artist: title only") << "/music/track2.mp3" << "Instrumental" << "" << "Instrumental";
    QTest::newRow("no tags: the file name") << "/music/cool beat.flac" << "" << "" << "cool beat";
}

void ShowFactory_Test::showNameForSong()
{
    QFETCH(QString, filepath);
    QFETCH(QString, title);
    QFETCH(QString, artist);
    QFETCH(QString, name);

    QCOMPARE(ShowFactory::showNameForSong(song(filepath, title, artist)), name);
}

void ShowFactory_Test::buildShow_data()
{
    showNameForSong_data();
}

void ShowFactory_Test::buildShow()
{
    QFETCH(QString, filepath);
    QFETCH(QString, title);
    QFETCH(QString, artist);
    QFETCH(QString, name);

    Doc doc(nullptr, 4);
    ShowFactory factory(&doc);
    QSignalSpy created(&factory, &ShowFactory::showCreatedForSong);

    const quint32 showId = factory.buildShow(song(filepath, title, artist, 128.0));
    QVERIFY(showId != Function::invalidId());
    QCOMPARE(created.count(), 1);
    QCOMPARE(created[0][0].toString(), filepath);
    QCOMPARE(created[0][1].toUInt(), showId);

    Show *show = qobject_cast<Show *>(doc.function(showId));
    QVERIFY(show != nullptr);
    QCOMPARE(show->name(), name);
    QCOMPARE(show->path(true), kSongFolderPath);
    Audio *audio = ShowFactory::songAudio(&doc, show);
    QVERIFY(audio != nullptr);
    QCOMPARE(audio->getSourceFileName(), filepath);
    // the new Show is the song's through its clip, nothing else remembers it
    QCOMPARE(ShowFactory::songFilepath(&doc, show), filepath);
    QCOMPARE(factory.showIdForFilepath(filepath), showId);
}

void ShowFactory_Test::indexFollowsTheDoc()
{
    Doc doc(nullptr, 4);
    ShowFactory factory(&doc);
    QSignalSpy mapping(&factory, &ShowFactory::mappingChanged);
    QCOMPARE(factory.showIdForFilepath("/music/a.mp3"), Function::invalidId());

    // a loaded workspace: the Show maps without anyone registering it
    Show *a = addSongShow(&doc, "A", "/music/a.mp3");
    QCOMPARE(factory.showIdForFilepath("/music/a.mp3"), a->id());
    // the first Show of a song wins, a later one with the same clip changes nothing
    addSongShow(&doc, "A again", "/music/a.mp3");
    QCOMPARE(factory.showIdForFilepath("/music/a.mp3"), a->id());
    QVERIFY(mapping.count() > 0);

    // the Show goes: the song has none; its Audio alone maps nothing
    const quint32 aId = a->id();
    QVERIFY(doc.deleteFunction(aId));
    QCOMPARE(factory.showIdForFilepath("/music/a.mp3"), doc.functionsByType(Function::ShowType).first()->id());
    QVERIFY(doc.deleteFunction(doc.functionsByType(Function::ShowType).first()->id()));
    QCOMPARE(factory.showIdForFilepath("/music/a.mp3"), Function::invalidId());

    // a new workspace with new ids: no id survives from the old one
    Show *b = addSongShow(&doc, "B", "/music/b.mp3");
    QCOMPARE(factory.showIdForFilepath("/music/b.mp3"), b->id());
    mapping.clear();
    doc.clearContents();
    QCOMPARE(factory.showIdForFilepath("/music/b.mp3"), Function::invalidId());
    QVERIFY(mapping.count() > 0);
    QCOMPARE(mapping.last()[0].toString(), QString());
    Show *c = addSongShow(&doc, "C", "/music/c.mp3");
    QCOMPARE(factory.showIdForFilepath("/music/c.mp3"), c->id());
}

void ShowFactory_Test::overridesWinUntilTheDocIsCleared()
{
    Doc doc(nullptr, 4);
    ShowFactory factory(&doc);
    Show *a = addSongShow(&doc, "A", "/music/a.mp3");
    Show *other = new Show(&doc);
    other->setName("Other");
    doc.addFunction(other);
    QSignalSpy mapping(&factory, &ShowFactory::mappingChanged);

    // Assign: the user's Show, although the Doc names another
    factory.registerMapping("/music/a.mp3", other->id());
    QCOMPARE(factory.showIdForFilepath("/music/a.mp3"), other->id());
    QCOMPARE(mapping.count(), 1);
    QCOMPARE(mapping[0][0].toString(), QString("/music/a.mp3"));
    QCOMPARE(mapping[0][1].toUInt(), other->id());

    // Clear: none, although the Doc has one, and an unrelated Doc change keeps it so
    factory.registerMapping("/music/a.mp3", Function::invalidId());
    QCOMPARE(factory.showIdForFilepath("/music/a.mp3"), Function::invalidId());
    doc.addFunction(new Show(&doc));
    QCOMPARE(factory.showIdForFilepath("/music/a.mp3"), Function::invalidId());
    Q_UNUSED(a)

    // the workspace goes: so do the overrides
    doc.clearContents();
    Show *b = addSongShow(&doc, "A", "/music/a.mp3");
    QCOMPARE(factory.showIdForFilepath("/music/a.mp3"), b->id());
}

void ShowFactory_Test::pathSeparatorsAreOneSong()
{
    // the platform's native form (backslashes on Windows, the same path here)
    // and the saved workspace's form name one song
    Doc doc(nullptr, 4);
    ShowFactory factory(&doc);
    Show *a = addSongShow(&doc, "A", QDir::toNativeSeparators("/music/sub/a.mp3"));
    QCOMPARE(factory.showIdForFilepath("/music/sub/a.mp3"), a->id());
    QCOMPARE(factory.showIdForFilepath(QDir::toNativeSeparators("/music/sub/a.mp3")), a->id());

    Show *b = new Show(&doc);
    doc.addFunction(b);
    factory.registerMapping(QDir::toNativeSeparators("/music/sub/b.mp3"), b->id());
    QCOMPARE(factory.showIdForFilepath("/music/sub/b.mp3"), b->id());
}

QTEST_MAIN(ShowFactory_Test)
