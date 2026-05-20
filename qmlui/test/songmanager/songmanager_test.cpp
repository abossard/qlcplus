/*
  Q Light Controller Plus
  songmanager_test.cpp

  Licensed under the Apache License, Version 2.0
*/

#include "songmanager_test.h"
#include "songmanager.h"
#include "showfactory.h"

#include "doc.h"
#include "show.h"
#include "audio.h"
#include "track.h"
#include "showfunction.h"

#include <QSignalSpy>
#include <QThread>

static Doc *createDoc()
{
    Doc *doc = new Doc(nullptr, 4);
    return doc;
}

void SongManagerTest::initTestCase()
{
    m_doc = createDoc();
}

void SongManagerTest::cleanupTestCase()
{
    delete m_doc;
    m_doc = nullptr;
}

// ── SongListModel tests ──

void SongManagerTest::testAddSong()
{
    SongListModel model(m_doc);
    QCOMPARE(model.rowCount(), 0);

    model.addSong("/music/track1.mp3", 100);
    QCOMPARE(model.rowCount(), 1);

    QModelIndex idx = model.index(0);
    QCOMPARE(model.data(idx, SongListModel::FilepathRole).toString(), "/music/track1.mp3");
    QCOMPARE(model.data(idx, SongListModel::ShowIdRole).toUInt(), 100u);
    QCOMPARE(model.data(idx, SongListModel::IsPlayingRole).toBool(), false);
}

void SongManagerTest::testAddSongDedup()
{
    SongListModel model(m_doc);
    model.addSong("/music/track1.mp3", 100);
    model.addSong("/music/track2.mp3", 101);
    model.addSong("/music/track1.mp3", 100);  // duplicate showId
    QCOMPARE(model.rowCount(), 2);
}

void SongManagerTest::testClear()
{
    SongListModel model(m_doc);
    model.addSong("/music/track1.mp3", 100);
    model.addSong("/music/track2.mp3", 101);
    QCOMPARE(model.rowCount(), 2);

    model.clear();
    QCOMPARE(model.rowCount(), 0);

    // Clear on empty model is a no-op
    model.clear();
    QCOMPARE(model.rowCount(), 0);
}

void SongManagerTest::testSetPlaying()
{
    SongListModel model(m_doc);
    model.addSong("/music/track1.mp3", 100);
    model.addSong("/music/track2.mp3", 101);

    model.setPlaying("/music/track1.mp3", true);
    QCOMPARE(model.data(model.index(0), SongListModel::IsPlayingRole).toBool(), true);
    QCOMPARE(model.data(model.index(1), SongListModel::IsPlayingRole).toBool(), false);

    model.clearPlayingState();
    QCOMPARE(model.data(model.index(0), SongListModel::IsPlayingRole).toBool(), false);

    // Unknown filepath — no-op
    model.setPlaying("/nonexistent.mp3", true);
    QCOMPARE(model.data(model.index(0), SongListModel::IsPlayingRole).toBool(), false);
}

void SongManagerTest::testMarkPlayed()
{
    Show *s = new Show(m_doc);
    s->setName("PlayTest");
    s->setPath(kSongFolderPath);
    m_doc->addFunction(s);

    SongListModel model(m_doc);
    model.addSong("/music/track1.mp3", s->id());

    QVERIFY(!model.data(model.index(0), SongListModel::LastPlayedRole).toDateTime().isValid());

    model.markPlayed("/music/track1.mp3");
    QDateTime lp = model.data(model.index(0), SongListModel::LastPlayedRole).toDateTime();
    QVERIFY(lp.isValid());
    QVERIFY(lp.secsTo(QDateTime::currentDateTime()) < 2);

    // Also verify it's on the Function object
    QVERIFY(s->lastPlayed().isValid());

    m_doc->deleteFunction(s->id());
}

void SongManagerTest::testMarkEdited()
{
    Show *s = new Show(m_doc);
    s->setName("EditTest");
    s->setPath(kSongFolderPath);
    m_doc->addFunction(s);

    SongListModel model(m_doc);
    model.addSong("/music/track1.mp3", s->id());

    QVERIFY(!model.data(model.index(0), SongListModel::LastEditedRole).toDateTime().isValid());

    model.markEdited(s->id());
    QDateTime le = model.data(model.index(0), SongListModel::LastEditedRole).toDateTime();
    QVERIFY(le.isValid());

    // Also verify it's on the Function object
    QVERIFY(s->lastEdited().isValid());

    // Unknown showId — no-op
    model.markEdited(999);
    QCOMPARE(model.rowCount(), 1);

    m_doc->deleteFunction(s->id());
}

void SongManagerTest::testRebuildFromDoc()
{
    Show *show = new Show(m_doc);
    show->setName("Test Song");
    show->setPath(kSongFolderPath);
    m_doc->addFunction(show);

    Show *other = new Show(m_doc);
    other->setName("Other Show");
    other->setPath("Some/Other/Path");
    m_doc->addFunction(other);

    SongListModel model(m_doc);
    model.rebuildFromDoc();

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0), SongListModel::ShowIdRole).toUInt(), show->id());

    m_doc->deleteFunction(show->id());
    m_doc->deleteFunction(other->id());
}

// ── SongSortFilterProxyModel tests ──

void SongManagerTest::testSortAlphabetical()
{
    SongListModel model(m_doc);

    Show *s1 = new Show(m_doc); s1->setName("Charlie"); s1->setPath(kSongFolderPath);
    m_doc->addFunction(s1);
    Show *s2 = new Show(m_doc); s2->setName("Alpha"); s2->setPath(kSongFolderPath);
    m_doc->addFunction(s2);
    Show *s3 = new Show(m_doc); s3->setName("Bravo"); s3->setPath(kSongFolderPath);
    m_doc->addFunction(s3);

    model.addSong("/c.mp3", s1->id());
    model.addSong("/a.mp3", s2->id());
    model.addSong("/b.mp3", s3->id());

    SongSortFilterProxyModel proxy;
    proxy.setSourceModel(&model);
    proxy.setSortMode(SongSortFilterProxyModel::Alphabetical);
    proxy.setSortAscending(true);
    proxy.sort(0, Qt::AscendingOrder);

    QCOMPARE(proxy.rowCount(), 3);
    QCOMPARE(proxy.data(proxy.index(0, 0), SongListModel::TitleRole).toString(), "Alpha");
    QCOMPARE(proxy.data(proxy.index(1, 0), SongListModel::TitleRole).toString(), "Bravo");
    QCOMPARE(proxy.data(proxy.index(2, 0), SongListModel::TitleRole).toString(), "Charlie");

    m_doc->deleteFunction(s1->id());
    m_doc->deleteFunction(s2->id());
    m_doc->deleteFunction(s3->id());
}

void SongManagerTest::testSortRecentlyPlayed()
{
    SongListModel model(m_doc);

    Show *s1 = new Show(m_doc); s1->setName("First"); s1->setPath(kSongFolderPath);
    m_doc->addFunction(s1);
    Show *s2 = new Show(m_doc); s2->setName("Second"); s2->setPath(kSongFolderPath);
    m_doc->addFunction(s2);
    Show *s3 = new Show(m_doc); s3->setName("Never Played"); s3->setPath(kSongFolderPath);
    m_doc->addFunction(s3);

    model.addSong("/1.mp3", s1->id());
    model.addSong("/2.mp3", s2->id());
    model.addSong("/3.mp3", s3->id());

    model.markPlayed("/1.mp3");
    QThread::msleep(50);
    model.markPlayed("/2.mp3");

    SongSortFilterProxyModel proxy;
    proxy.setSourceModel(&model);
    proxy.setSortMode(SongSortFilterProxyModel::RecentlyPlayed);
    proxy.setSortAscending(true);
    proxy.sort(0, Qt::AscendingOrder);

    QCOMPARE(proxy.data(proxy.index(0, 0), SongListModel::TitleRole).toString(), "Second");
    QCOMPARE(proxy.data(proxy.index(1, 0), SongListModel::TitleRole).toString(), "First");
    QCOMPARE(proxy.data(proxy.index(2, 0), SongListModel::TitleRole).toString(), "Never Played");

    m_doc->deleteFunction(s1->id());
    m_doc->deleteFunction(s2->id());
    m_doc->deleteFunction(s3->id());
}

void SongManagerTest::testSortRecentlyEdited()
{
    SongListModel model(m_doc);

    Show *s1 = new Show(m_doc); s1->setName("EditedFirst"); s1->setPath(kSongFolderPath);
    m_doc->addFunction(s1);
    Show *s2 = new Show(m_doc); s2->setName("EditedSecond"); s2->setPath(kSongFolderPath);
    m_doc->addFunction(s2);

    model.addSong("/1.mp3", s1->id());
    model.addSong("/2.mp3", s2->id());

    model.markEdited(s1->id());
    QThread::msleep(50);
    model.markEdited(s2->id());

    SongSortFilterProxyModel proxy;
    proxy.setSourceModel(&model);
    proxy.setSortMode(SongSortFilterProxyModel::RecentlyEdited);
    proxy.setSortAscending(true);
    proxy.sort(0, Qt::AscendingOrder);

    QCOMPARE(proxy.data(proxy.index(0, 0), SongListModel::TitleRole).toString(), "EditedSecond");
    QCOMPARE(proxy.data(proxy.index(1, 0), SongListModel::TitleRole).toString(), "EditedFirst");

    m_doc->deleteFunction(s1->id());
    m_doc->deleteFunction(s2->id());
}

void SongManagerTest::testSortDescending()
{
    SongListModel model(m_doc);

    Show *s1 = new Show(m_doc); s1->setName("Alpha"); s1->setPath(kSongFolderPath);
    m_doc->addFunction(s1);
    Show *s2 = new Show(m_doc); s2->setName("Zulu"); s2->setPath(kSongFolderPath);
    m_doc->addFunction(s2);

    model.addSong("/a.mp3", s1->id());
    model.addSong("/z.mp3", s2->id());

    SongSortFilterProxyModel proxy;
    proxy.setSourceModel(&model);
    proxy.setSortMode(SongSortFilterProxyModel::Alphabetical);
    proxy.setSortAscending(false);
    proxy.sort(0, Qt::DescendingOrder);

    QCOMPARE(proxy.data(proxy.index(0, 0), SongListModel::TitleRole).toString(), "Zulu");
    QCOMPARE(proxy.data(proxy.index(1, 0), SongListModel::TitleRole).toString(), "Alpha");

    m_doc->deleteFunction(s1->id());
    m_doc->deleteFunction(s2->id());
}

void SongManagerTest::testSearchFilter()
{
    SongListModel model(m_doc);

    Show *s1 = new Show(m_doc); s1->setName("Bohemian Rhapsody"); s1->setPath(kSongFolderPath);
    m_doc->addFunction(s1);
    Show *s2 = new Show(m_doc); s2->setName("Stairway to Heaven"); s2->setPath(kSongFolderPath);
    m_doc->addFunction(s2);
    Show *s3 = new Show(m_doc); s3->setName("Hotel California"); s3->setPath(kSongFolderPath);
    m_doc->addFunction(s3);

    model.addSong("/bohemian.mp3", s1->id());
    model.addSong("/stairway.mp3", s2->id());
    model.addSong("/hotel.mp3", s3->id());

    SongSortFilterProxyModel proxy;
    proxy.setSourceModel(&model);
    proxy.sort(0, Qt::AscendingOrder);

    QCOMPARE(proxy.rowCount(), 3);

    proxy.setSearchFilter("heaven");
    QCOMPARE(proxy.rowCount(), 1);
    QCOMPARE(proxy.data(proxy.index(0, 0), SongListModel::TitleRole).toString(), "Stairway to Heaven");

    proxy.setSearchFilter("hotel");
    QCOMPARE(proxy.rowCount(), 1);
    QCOMPARE(proxy.data(proxy.index(0, 0), SongListModel::TitleRole).toString(), "Hotel California");

    proxy.setSearchFilter("");
    QCOMPARE(proxy.rowCount(), 3);

    // Case insensitive
    proxy.setSearchFilter("BOHEMIAN");
    QCOMPARE(proxy.rowCount(), 1);

    m_doc->deleteFunction(s1->id());
    m_doc->deleteFunction(s2->id());
    m_doc->deleteFunction(s3->id());
}

void SongManagerTest::testSearchAndSort()
{
    SongListModel model(m_doc);

    Show *s1 = new Show(m_doc); s1->setName("Rock Anthem"); s1->setPath(kSongFolderPath);
    m_doc->addFunction(s1);
    Show *s2 = new Show(m_doc); s2->setName("Rock Ballad"); s2->setPath(kSongFolderPath);
    m_doc->addFunction(s2);
    Show *s3 = new Show(m_doc); s3->setName("Jazz Standard"); s3->setPath(kSongFolderPath);
    m_doc->addFunction(s3);

    model.addSong("/anthem.mp3", s1->id());
    model.addSong("/ballad.mp3", s2->id());
    model.addSong("/jazz.mp3", s3->id());

    SongSortFilterProxyModel proxy;
    proxy.setSourceModel(&model);
    proxy.setSortMode(SongSortFilterProxyModel::Alphabetical);
    proxy.setSortAscending(true);
    proxy.sort(0, Qt::AscendingOrder);

    proxy.setSearchFilter("rock");
    QCOMPARE(proxy.rowCount(), 2);
    QCOMPARE(proxy.data(proxy.index(0, 0), SongListModel::TitleRole).toString(), "Rock Anthem");
    QCOMPARE(proxy.data(proxy.index(1, 0), SongListModel::TitleRole).toString(), "Rock Ballad");

    m_doc->deleteFunction(s1->id());
    m_doc->deleteFunction(s2->id());
    m_doc->deleteFunction(s3->id());
}

QTEST_MAIN(SongManagerTest)
