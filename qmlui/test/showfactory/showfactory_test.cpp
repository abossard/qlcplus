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
#include "scene.h"

#include <QBuffer>
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
    QTest::newRow("no song: a name, nothing to build") << "" << "Ghost" << "Nobody" << "Nobody - Ghost";
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
    if (filepath.isEmpty())
    {
        QCOMPARE(showId, Function::invalidId());
        QCOMPARE(created.count(), 0);
        QVERIFY(doc.functions().isEmpty());
        return;
    }
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
    // several Shows of one song: the first in Doc order
    addSongShow(&doc, "A again", "/music/a.mp3");
    QCOMPARE(factory.showIdForFilepath("/music/a.mp3"), a->id());
    QVERIFY(factory.buildShow(song("/music/a.mp3", "A", "X")) != a->id());
    QCOMPARE(factory.showIdForFilepath("/music/a.mp3"), a->id());
    QVERIFY(mapping.count() > 0);

    // the Shows go: the song has none; its Audio alone maps nothing
    while (!doc.functionsByType(Function::ShowType).isEmpty())
        QVERIFY(doc.deleteFunction(doc.functionsByType(Function::ShowType).last()->id()));
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

void ShowFactory_Test::choicesWinUntilTheDocIsCleared()
{
    Doc doc(nullptr, 4);
    ShowFactory factory(&doc);
    Show *a = addSongShow(&doc, "A", "/music/a.mp3");
    Show *other = new Show(&doc);
    other->setName("Other");
    doc.addFunction(other);
    doc.resetModified();
    QSignalSpy mapping(&factory, &ShowFactory::mappingChanged);

    // Assign: the user's Show, although the Doc names another; saved with the workspace
    factory.registerMapping("/music/a.mp3", other->id());
    QCOMPARE(factory.showIdForFilepath("/music/a.mp3"), other->id());
    QVERIFY(doc.isModified());
    QCOMPARE(mapping.count(), 1);
    QCOMPARE(mapping[0][0].toString(), QString("/music/a.mp3"));
    QCOMPARE(mapping[0][1].toUInt(), other->id());
    doc.addFunction(new Show(&doc));
    QCOMPARE(factory.showIdForFilepath("/music/a.mp3"), other->id());

    // Clear drops the choice: the Doc's Show again
    factory.registerMapping("/music/a.mp3", Function::invalidId());
    QCOMPARE(factory.showIdForFilepath("/music/a.mp3"), a->id());

    // the workspace goes: so do the choices
    factory.registerMapping("/music/a.mp3", other->id());
    doc.clearContents();
    Show *b = addSongShow(&doc, "A", "/music/a.mp3");
    QCOMPARE(factory.showIdForFilepath("/music/a.mp3"), b->id());
}

void ShowFactory_Test::overrideGoesWithItsShow()
{
    // Assign a song to a Show, then delete that Show: the Doc's Show is the
    // song's again, and the freed id, handed to the next Function, maps nothing
    Doc doc(nullptr, 4);
    ShowFactory factory(&doc);
    Show *a = addSongShow(&doc, "A", "/music/a.mp3");
    Show *chosen = new Show(&doc);
    chosen->setName("Chosen");
    doc.addFunction(chosen);
    const quint32 chosenId = chosen->id();
    factory.registerMapping("/music/a.mp3", chosenId);
    QCOMPARE(factory.showIdForFilepath("/music/a.mp3"), chosenId);

    QVERIFY(doc.deleteFunction(chosenId));
    QCOMPARE(factory.showIdForFilepath("/music/a.mp3"), a->id());
    Scene *scene = new Scene(&doc);
    doc.addFunction(scene);
    QCOMPARE(scene->id(), chosenId);
    QCOMPARE(factory.showIdForFilepath("/music/a.mp3"), a->id());
}

void ShowFactory_Test::choicesSurviveSaveAndLoad_data()
{
    QTest::addColumn<QString>("choice");
    QTest::addColumn<bool>("showSurvives");

    QTest::newRow("Assign") << "assign" << true;
    QTest::newRow("New") << "new" << true;
    QTest::newRow("Clear") << "clear" << true;
    QTest::newRow("a choice whose Show is gone is dropped") << "assign" << false;
}

void ShowFactory_Test::choicesSurviveSaveAndLoad()
{
    QFETCH(QString, choice);
    QFETCH(bool, showSurvives);

    // the Doc's Show of A, and the one the user chooses instead
    Doc doc(nullptr, 4);
    ShowFactory factory(&doc);
    Show *a = addSongShow(&doc, "A", "/music/a.mp3");
    quint32 chosen = Function::invalidId();
    if (choice == QLatin1String("new"))
        chosen = factory.buildShow(song("/music/a.mp3", "A", "X"));
    else
    {
        Show *other = new Show(&doc);
        doc.addFunction(other);
        chosen = other->id();
    }
    factory.registerMapping("/music/a.mp3", chosen);
    if (choice == QLatin1String("clear"))
        factory.registerMapping("/music/a.mp3", Function::invalidId());
    const quint32 before = factory.showIdForFilepath("/music/a.mp3");
    QCOMPARE(before, choice == QLatin1String("clear") ? a->id() : chosen);

    QBuffer xml;
    xml.open(QIODevice::WriteOnly);
    QXmlStreamWriter writer(&xml);
    writer.writeStartDocument();
    factory.saveXML(&writer);
    writer.writeEndDocument();

    // reload: the Doc's Functions first, under their ids, then the choices
    doc.clearContents();
    QCOMPARE(factory.showIdForFilepath("/music/a.mp3"), Function::invalidId());
    a = addSongShow(&doc, "A", "/music/a.mp3");
    if (showSurvives)
    {
        Show *again = new Show(&doc);
        QVERIFY(doc.addFunction(again, chosen));
        if (choice == QLatin1String("new"))
            factory.buildShow(song("/music/a.mp3", "A", "X"));
    }
    QXmlStreamReader reader(xml.data());
    QVERIFY(reader.readNextStartElement() || choice == QLatin1String("clear"));
    if (choice != QLatin1String("clear"))
        QVERIFY(factory.loadXML(reader));
    QCOMPARE(factory.showIdForFilepath("/music/a.mp3"), showSurvives ? before : a->id());
}

void ShowFactory_Test::clipChangesReachTheIndex()
{
    // the index reads the first Audio clip: re-sourcing that Audio moves the song
    Doc doc(nullptr, 4);
    ShowFactory factory(&doc);
    Show *a = addSongShow(&doc, "A", "/music/a.mp3");
    QCOMPARE(factory.showIdForFilepath("/music/a.mp3"), a->id());
    QSignalSpy mapping(&factory, &ShowFactory::mappingChanged);

    ShowFactory::songAudio(&doc, a)->setSourceFileName("/music/b.mp3");
    QCOMPARE(mapping.count(), 1);
    QCOMPARE(factory.showIdForFilepath("/music/b.mp3"), a->id());
    QCOMPARE(factory.showIdForFilepath("/music/a.mp3"), Function::invalidId());

    // a renamed Show is a change of the same kind
    a->setName("A renamed");
    QVERIFY(mapping.count() >= 2);
}

void ShowFactory_Test::normalizePath_data()
{
    QTest::addColumn<QString>("path");
    QTest::addColumn<QString>("key");

    QTest::newRow("posix") << "/music/sub/a.mp3" << "/music/sub/a.mp3";
#ifdef Q_OS_WIN
    QTest::newRow("windows") << "C:\\music\\sub\\a.mp3" << "C:/music/sub/a.mp3";
#else
    QTest::newRow("a backslash is a character here") << "/music/sub\\a.mp3" << "/music/sub\\a.mp3";
#endif
}

void ShowFactory_Test::normalizePath()
{
    QFETCH(QString, path);
    QFETCH(QString, key);
    // the platform's native separators become the saved workspace's form
    QCOMPARE(ShowFactory::normalizePath(path), key);
    QCOMPARE(ShowFactory::normalizePath(path), QDir::fromNativeSeparators(path));
}

QTEST_MAIN(ShowFactory_Test)
