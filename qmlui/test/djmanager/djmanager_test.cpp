/*
  Q Light Controller Plus - Unit test
  djmanager_test.cpp

  Licensed under the Apache License, Version 2.0
*/

#include "djmanager_test.h"
#include "djmanager.h"
#include "djfsm.h"
#include "showfactory.h"
#include "vdjbridge.h"

#include "doc.h"
#include "show.h"
#include "scene.h"
#include "track.h"
#include "showfunction.h"
#include "audio.h"
#include "function.h"
#include "mastertimer.h"

#include <QQuickView>
#include <QSettings>
#include <QSignalSpy>

static Doc *createDoc()
{
    return new Doc(nullptr, 4);
}

// Helper to feed a full metadata burst into an FSM for a deck (1-based).
static void feedSong(DjFsm *fsm, int deck, const QString &fp,
                     const QString &title, const QString &artist, double bpm)
{
    fsm->onDeckTrigger(deck, "get_filepath", fp);
    fsm->onDeckTrigger(deck, "get_title", title);
    fsm->onDeckTrigger(deck, "get_artist", artist);
    fsm->onDeckTrigger(deck, "get_bpm", bpm);
    fsm->onDeckTrigger(deck, "loaded", "on");
    fsm->onTickEnded();
}

/** A handover as VirtualDJ streams it: every other deck's master flag off, this one's on, one write */
static void master(VdjBridge &bridge, int index0)
{
    for (int d = 0; d < DjFsm::MaxDecks; d++)
        QMetaObject::invokeMethod(&bridge, "onDeckTrigger", Q_ARG(int, d), Q_ARG(QString, "masterdeck"),
                                  Q_ARG(QVariant, QVariant(d == index0 ? "on" : "off")));
    QMetaObject::invokeMethod(&bridge, "onTickEnded");
}

/** The deferred Show creation of a song announced since, shortened for tests */
static void waitForShowCreation()
{
    QTest::qWait(DjManager::showCreationDelayMs + 200);
}

void DjManager_Test::initTestCase()
{
    // Isolate QSettings so the persisted deck count doesn't leak between runs.
    QCoreApplication::setOrganizationName("qlcplus-test");
    QCoreApplication::setApplicationName("djmanager_test");
    QSettings().remove("vdj/deckCount");
    DjManager::showCreationDelayMs = 300;

    m_doc = createDoc();
}

void DjManager_Test::cleanupTestCase()
{
    delete m_doc;
    m_doc = nullptr;
}

// ── DjSongModel ──

void DjManager_Test::modelUpsertAddsAndDedupsByFilepath()
{
    ShowFactory index(m_doc);
    DjSongModel model(m_doc, &index);
    QCOMPARE(model.rowCount(), 0);

    DjFsm::DeckSong a;
    a.filepath = "/m/a.mp3"; a.title = "A"; a.artist = "X"; a.bpm = 120.0;
    QVERIFY(model.upsertSong(a));
    QCOMPARE(model.rowCount(), 1);

    // Same filepath updates metadata but does not add a row.
    DjFsm::DeckSong a2 = a; a2.title = "A (remix)";
    QVERIFY(!model.upsertSong(a2));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0), DjSongModel::TitleRole).toString(), QString("A (remix)"));

    // Different filepath adds a new row.
    DjFsm::DeckSong b;
    b.filepath = "/m/b.mp3"; b.title = "B"; b.artist = "Y"; b.bpm = 128.0;
    QVERIFY(model.upsertSong(b));
    QCOMPARE(model.rowCount(), 2);
}

void DjManager_Test::modelReadsTheShowFromTheIndex()
{
    ShowFactory index(m_doc);
    DjSongModel model(m_doc, &index);
    DjFsm::DeckSong a;
    a.filepath = "/m/a.mp3"; a.title = "A";
    model.upsertSong(a);
    QCOMPARE(model.data(model.index(0), DjSongModel::HasShowRole).toBool(), false);

    // the row has no Show of its own: the index answers
    index.registerMapping("/m/a.mp3", 42);
    QCOMPARE(model.data(model.index(0), DjSongModel::ShowIdRole).toUInt(), 42u);
    QCOMPARE(model.data(model.index(0), DjSongModel::HasShowRole).toBool(), true);
    index.registerMapping("/m/a.mp3", Function::invalidId());
    QCOMPARE(model.data(model.index(0), DjSongModel::HasShowRole).toBool(), false);
}

void DjManager_Test::modelActiveAndPlayingFlags()
{
    ShowFactory index(m_doc);
    DjSongModel model(m_doc, &index);
    DjFsm::DeckSong a; a.filepath = "/m/a.mp3"; a.title = "A"; a.artist = "X"; a.bpm = 1.0;
    DjFsm::DeckSong b; b.filepath = "/m/b.mp3"; b.title = "B"; b.artist = "Y"; b.bpm = 1.0;
    model.upsertSong(a);
    model.upsertSong(b);

    model.setActiveFilepath("/m/b.mp3");
    QCOMPARE(model.data(model.index(0), DjSongModel::IsActiveRole).toBool(), false);
    QCOMPARE(model.data(model.index(1), DjSongModel::IsActiveRole).toBool(), true);

    model.setPlayingFilepaths(QSet<QString>{ "/m/a.mp3" });
    QCOMPARE(model.data(model.index(0), DjSongModel::IsPlayingRole).toBool(), true);
    QCOMPARE(model.data(model.index(1), DjSongModel::IsPlayingRole).toBool(), false);
}

// ── DjManager orchestration ──
//
// DjManager needs a QQuickView (PreviewContext base). We construct one
// without showing it.

void DjManager_Test::songChangeAddsOneEntryAndAssignsExistingShow()
{
    Doc *doc = createDoc();
    QQuickView view;
    VdjBridge bridge;
    bridge.setDoc(doc);
    ShowFactory *factory = bridge.showFactory();
    DjFsm *fsm = bridge.djFsm();

    // Pre-seed the factory so the "show already exists" branch is synchronous.
    DjFsm::DeckSong seed; seed.filepath = "/m/a.mp3"; seed.title = "A"; seed.artist = "X"; seed.bpm = 120.0;
    quint32 showId = factory->buildShow(seed);
    QVERIFY(showId != Function::invalidId());

    DjManager mgr(&view, doc, &bridge, factory);

    // Decks 1 and 2 (both real, within deckCount=2) load the SAME song —
    // one DJ entry expected (filepath dedup across decks).
    feedSong(fsm, 1, "/m/a.mp3", "A", "X", 120.0);
    feedSong(fsm, 2, "/m/a.mp3", "A", "X", 120.0);

    QCOMPARE(mgr.songListModel()->rowCount(QModelIndex()), 1);
    QCOMPARE(mgr.songListModel()->data(mgr.songListModel()->index(0, 0),
                                       DjSongModel::ShowIdRole).toUInt(), showId);

    delete doc;
}

void DjManager_Test::repeatedSongChangeDoesNotRecreateShow()
{
    Doc *doc = createDoc();
    QQuickView view;
    VdjBridge bridge;
    bridge.setDoc(doc);
    ShowFactory *factory = bridge.showFactory();
    DjFsm *fsm = bridge.djFsm();

    DjFsm::DeckSong seed; seed.filepath = "/m/a.mp3"; seed.title = "A"; seed.artist = "X"; seed.bpm = 120.0;
    factory->buildShow(seed);

    DjManager mgr(&view, doc, &bridge, factory);

    QSignalSpy createSpy(factory, &ShowFactory::showCreatedForSong);

    feedSong(fsm, 1, "/m/a.mp3", "A", "X", 120.0);
    // Re-load the same song on the same deck and on another deck.
    feedSong(fsm, 2, "/m/a.mp3", "A", "X", 120.0);
    fsm->onDeckTrigger(1, "get_filepath", "/m/a.mp3"); // duplicate, no change

    // The song was already known (existing show) — no new show was created.
    QCOMPARE(createSpy.count(), 0);
    QCOMPARE(mgr.songListModel()->rowCount(QModelIndex()), 1);

    delete doc;
}

void DjManager_Test::songChangeCreatesShowWhenNoneExists()
{
    Doc *doc = createDoc();
    QQuickView view;
    VdjBridge bridge;
    bridge.setDoc(doc);
    ShowFactory *factory = bridge.showFactory();
    DjFsm *fsm = bridge.djFsm();

    DjManager mgr(&view, doc, &bridge, factory);

    QSignalSpy createSpy(factory, &ShowFactory::showCreatedForSong);

    // Use a real audio file from the test resources is not required — the
    // ShowFactory falls back gracefully, but creation is deferred 3s.
    feedSong(fsm, 1, "/m/newsong.mp3", "New Song", "DJ", 130.0);

    // the Show is built once the song's burst is complete
    QVERIFY(createSpy.wait(DjManager::showCreationDelayMs + 2000));
    QCOMPARE(createSpy.count(), 1);
    QCOMPARE(createSpy[0][0].toString(), QString("/m/newsong.mp3"));

    quint32 showId = createSpy[0][1].toUInt();

    // Exactly ONE row for the song (no phantom row from the Doc rebuild
    // path), with the FSM-sourced title preserved and the show assigned.
    QCOMPARE(mgr.songListModel()->rowCount(QModelIndex()), 1);
    QModelIndex i0 = mgr.songListModel()->index(0, 0);
    QCOMPARE(mgr.songListModel()->data(i0, DjSongModel::FilepathRole).toString(),
             QString("/m/newsong.mp3"));
    QCOMPARE(mgr.songListModel()->data(i0, DjSongModel::TitleRole).toString(),
             QString("New Song"));
    QCOMPARE(mgr.songListModel()->data(i0, DjSongModel::ShowIdRole).toUInt(), showId);
    QCOMPARE(mgr.songListModel()->data(i0, DjSongModel::HasShowRole).toBool(), true);

    delete doc;
}

void DjManager_Test::metadataFirstReloadDoesNotCorruptPreviousRow()
{
    // Regression for the live-snapshot corruption: load song A, then reload
    // song B on the SAME deck in metadata-first order (get_title B arrives
    // while the FSM still holds filepath A). Song A's persistent list row must
    // NOT be overwritten with B's title; B must become its own row.
    Doc *doc = createDoc();
    QQuickView view;
    VdjBridge bridge;
    bridge.setDoc(doc);
    DjFsm *fsm = bridge.djFsm();

    DjManager mgr(&view, doc, &bridge, bridge.showFactory());
    mgr.setDeckCount(2);
    QAbstractItemModel *songs = mgr.songListModel();

    // Song A (filepath-first), committed.
    feedSong(fsm, 1, "/m/a.mp3", "Song A", "Artist A", 120.0);
    QCOMPARE(songs->rowCount(QModelIndex()), 1);

    // Reload Song B on deck 1, METADATA-FIRST: title/artist/bpm before filepath.
    fsm->onDeckTrigger(1, "get_title", "Song B");
    fsm->onDeckTrigger(1, "get_artist", "Artist B");
    fsm->onDeckTrigger(1, "get_bpm", 128.0);
    fsm->onTickEnded();
    // While filepath is still A, song A's row must be untouched.
    int rowA = -1;
    for (int i = 0; i < songs->rowCount(QModelIndex()); ++i)
        if (songs->data(songs->index(i, 0), DjSongModel::FilepathRole).toString() == "/m/a.mp3")
            rowA = i;
    QVERIFY(rowA >= 0);
    QCOMPARE(songs->data(songs->index(rowA, 0), DjSongModel::TitleRole).toString(),
             QString("Song A")); // NOT "Song B"

    // Complete B's burst.
    fsm->onDeckTrigger(1, "get_filepath", "/m/b.mp3");
    fsm->onTickEnded();

    // Two distinct rows, each with its own correct title.
    QCOMPARE(songs->rowCount(QModelIndex()), 2);
    QString aTitle, bTitle;
    for (int i = 0; i < 2; ++i)
    {
        QString fp = songs->data(songs->index(i, 0), DjSongModel::FilepathRole).toString();
        QString t  = songs->data(songs->index(i, 0), DjSongModel::TitleRole).toString();
        if (fp == "/m/a.mp3") aTitle = t;
        if (fp == "/m/b.mp3") bTitle = t;
    }
    QCOMPARE(aTitle, QString("Song A"));
    QCOMPARE(bTitle, QString("Song B"));

    delete doc;
}

void DjManager_Test::clearShowSurvivesUnrelatedFunctionAdd()
{
    Doc *doc = createDoc();
    QQuickView view;
    VdjBridge bridge;
    bridge.setDoc(doc);
    ShowFactory *factory = bridge.showFactory();
    DjFsm *fsm = bridge.djFsm();

    DjManager mgr(&view, doc, &bridge, factory);

    feedSong(fsm, 1, "/m/clear.mp3", "Clear", "DJ", 120.0);
    mgr.createShow("/m/clear.mp3");
    QCOMPARE(mgr.songListModel()->data(mgr.songListModel()->index(0, 0),
                                       DjSongModel::HasShowRole).toBool(), true);

    mgr.clearShow("/m/clear.mp3");
    QCOMPARE(mgr.songListModel()->data(mgr.songListModel()->index(0, 0),
                                       DjSongModel::HasShowRole).toBool(), false);

    // An unrelated Doc change must NOT revert the clear or drop the song row.
    Scene *scene = new Scene(doc);
    doc->addFunction(scene);

    QCOMPARE(mgr.songListModel()->rowCount(QModelIndex()), 1);
    QCOMPARE(mgr.songListModel()->data(mgr.songListModel()->index(0, 0),
                                       DjSongModel::HasShowRole).toBool(), false);

    delete doc;
}

void DjManager_Test::removedShowUnassignsButKeepsRow()
{
    Doc *doc = createDoc();
    QQuickView view;
    VdjBridge bridge;
    bridge.setDoc(doc);
    ShowFactory *factory = bridge.showFactory();
    DjFsm *fsm = bridge.djFsm();

    DjManager mgr(&view, doc, &bridge, factory);

    feedSong(fsm, 1, "/m/remove.mp3", "Remove", "DJ", 120.0);
    mgr.createShow("/m/remove.mp3");
    quint32 showId = mgr.songListModel()->data(mgr.songListModel()->index(0, 0),
                                               DjSongModel::ShowIdRole).toUInt();
    QVERIFY(showId != Function::invalidId());

    doc->deleteFunction(showId);

    // The song row remains; only the show assignment is cleared.
    QCOMPARE(mgr.songListModel()->rowCount(QModelIndex()), 1);
    QCOMPARE(mgr.songListModel()->data(mgr.songListModel()->index(0, 0),
                                       DjSongModel::HasShowRole).toBool(), false);

    delete doc;
}

// ── Per-song actions ──

void DjManager_Test::createShowAction()
{
    Doc *doc = createDoc();
    QQuickView view;
    VdjBridge bridge;
    bridge.setDoc(doc);
    ShowFactory *factory = bridge.showFactory();
    DjFsm *fsm = bridge.djFsm();

    DjManager mgr(&view, doc, &bridge, factory);

    feedSong(fsm, 1, "/m/manual.mp3", "Manual", "DJ", 124.0);
    // No show yet (deferred path hasn't fired).
    QCOMPARE(mgr.songListModel()->data(mgr.songListModel()->index(0, 0),
                                       DjSongModel::HasShowRole).toBool(), false);

    int showsBefore = doc->functionsByType(Function::ShowType).count();
    mgr.createShow("/m/manual.mp3");
    int showsAfter = doc->functionsByType(Function::ShowType).count();

    QCOMPARE(showsAfter, showsBefore + 1);
    QCOMPARE(mgr.songListModel()->data(mgr.songListModel()->index(0, 0),
                                       DjSongModel::HasShowRole).toBool(), true);

    delete doc;
}

void DjManager_Test::newShowWinsAfterAWorkspaceLoad()
{
    // a loaded workspace keeps its ids and the Doc hands out the holes below
    // them: the Show "New" builds gets a lower id than the song's old one
    Doc *doc = createDoc();
    QQuickView view;
    VdjBridge bridge;
    bridge.setDoc(doc);
    ShowFactory *factory = bridge.showFactory();
    DjManager mgr(&view, doc, &bridge, factory);

    Audio *audio = new Audio(doc);
    audio->setSourceFileName("/m/old.mp3");
    doc->addFunction(audio, 6);
    Show *old = new Show(doc);
    old->setName("Old");
    doc->addFunction(old, 7);
    Track *track = new Track(Function::invalidId(), old);
    old->addTrack(track);
    track->createShowFunction(audio->id());
    feedSong(bridge.djFsm(), 1, "/m/old.mp3", "Old", "DJ", 120.0);
    QCOMPARE(factory->showIdForFilepath("/m/old.mp3"), 7u);

    mgr.createShow("/m/old.mp3");
    QCOMPARE(doc->functionsByType(Function::ShowType).count(), 2);
    const quint32 created = factory->showIdForFilepath("/m/old.mp3");
    QVERIFY(created != 7u);
    QVERIFY(created < 7u);
    QCOMPARE(doc->function(created)->name(), QString("DJ - Old"));

    delete doc;
}

void DjManager_Test::assignAndClearShowAction()
{
    Doc *doc = createDoc();
    QQuickView view;
    VdjBridge bridge;
    bridge.setDoc(doc);
    ShowFactory *factory = bridge.showFactory();
    DjFsm *fsm = bridge.djFsm();

    DjManager mgr(&view, doc, &bridge, factory);

    // An existing show to assign.
    Show *show = new Show(doc);
    show->setName("Existing Show");
    show->setPath(kSongFolderPath);
    doc->addFunction(show);

    feedSong(fsm, 1, "/m/assign.mp3", "Assign", "DJ", 122.0);

    mgr.assignShow("/m/assign.mp3", static_cast<int>(show->id()));
    QCOMPARE(mgr.songListModel()->data(mgr.songListModel()->index(0, 0),
                                       DjSongModel::ShowIdRole).toUInt(), show->id());

    mgr.clearShow("/m/assign.mp3");
    QCOMPARE(mgr.songListModel()->data(mgr.songListModel()->index(0, 0),
                                       DjSongModel::HasShowRole).toBool(), false);

    delete doc;
}

void DjManager_Test::loadShowEmitsRequest()
{
    Doc *doc = createDoc();
    QQuickView view;
    VdjBridge bridge;
    bridge.setDoc(doc);
    ShowFactory *factory = bridge.showFactory();
    DjFsm *fsm = bridge.djFsm();

    DjManager mgr(&view, doc, &bridge, factory);

    Show *show = new Show(doc);
    show->setName("Loadable");
    show->setPath(kSongFolderPath);
    doc->addFunction(show);

    feedSong(fsm, 1, "/m/load.mp3", "Load", "DJ", 121.0);
    mgr.assignShow("/m/load.mp3", static_cast<int>(show->id()));

    QSignalSpy spy(&mgr, &DjManager::showLoadRequested);
    mgr.loadShow("/m/load.mp3");
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy[0][0].toInt(), static_cast<int>(show->id()));

    // Loading a song with no assigned show is a no-op.
    feedSong(fsm, 2, "/m/noshow.mp3", "NoShow", "DJ", 121.0);
    mgr.loadShow("/m/noshow.mp3");
    QCOMPARE(spy.count(), 1);

    delete doc;
}

// ── Perform mode ──

void DjManager_Test::performModeDelegatesToBridge()
{
    Doc *doc = createDoc();
    QQuickView view;
    VdjBridge bridge;
    bridge.setDoc(doc);
    DjManager mgr(&view, doc, &bridge, bridge.showFactory());

    QCOMPARE(mgr.performMode(), false);
    QCOMPARE(bridge.performMode(), false);

    QSignalSpy spy(&mgr, &DjManager::performModeChanged);
    mgr.setPerformMode(true);
    QCOMPARE(bridge.performMode(), true);
    QCOMPARE(mgr.performMode(), true);
    QVERIFY(spy.count() >= 1);

    delete doc;
}

void DjManager_Test::performModeGatesAutoPlay()
{
    Doc *doc = createDoc();
    QQuickView view;
    VdjBridge bridge;
    bridge.setDoc(doc);
    ShowFactory *factory = bridge.showFactory();

    DjManager mgr(&view, doc, &bridge, factory);

    // Build a long-duration External-sync Show so it keeps running once
    // started, and register the filepath→show mapping the auto-play path uses.
    Show *show = new Show(doc);
    show->setName("Perform Show");
    show->setSyncSource(1); // External
    doc->addFunction(show);
    Scene *scene = new Scene(doc);
    doc->addFunction(scene);
    Track *track = new Track(Function::invalidId(), show);
    track->setName("Audio");
    show->addTrack(track);
    ShowFunction *sf = track->createShowFunction(scene->id());
    sf->setStartTime(0);
    sf->setDuration(300000); // 5 minutes — won't auto-stop
    factory->registerMapping("/m/perform.mp3", show->id());

    // Feed the song THROUGH the bridge (deckIndex 0 == deck 1) so the
    // VdjDeckModel filepath the auto-play path reads is populated.
    auto deckTrigger = [&](const QString &t, const QVariant &v) {
        QMetaObject::invokeMethod(&bridge, "onDeckTrigger",
            Q_ARG(int, 0), Q_ARG(QString, t), Q_ARG(QVariant, v));
        QMetaObject::invokeMethod(&bridge, "onTickEnded");
    };
    deckTrigger("get_filepath", QVariant("/m/perform.mp3"));
    deckTrigger("loaded", QVariant("on"));
    deckTrigger("get_title", QVariant("Perform"));
    deckTrigger("get_artist", QVariant("DJ"));
    deckTrigger("get_bpm", QVariant(120.0));

    // Master deck = 1 (drives both FSM active deck and VdjBridge auto-play).
    QMetaObject::invokeMethod(&bridge, "onDeckTrigger",
        Q_ARG(int, 0), Q_ARG(QString, "masterdeck"), Q_ARG(QVariant, QVariant("on")));

    doc->masterTimer()->start();

    // Perform OFF: playing the master deck must NOT start the show.
    deckTrigger("play", QVariant("on"));
    QTest::qWait(60);
    QVERIFY(!show->isRunning());

    // Reset deck play state, enable Perform, replay.
    deckTrigger("play", QVariant("off"));
    mgr.setPerformMode(true);
    deckTrigger("play", QVariant("on"));
    QTest::qWait(60);
    QVERIFY(show->isRunning());

    doc->masterTimer()->stop();
    delete doc;
}

void DjManager_Test::performOffStopsRunningShow()
{
    Doc *doc = createDoc();
    QQuickView view;
    VdjBridge bridge;
    bridge.setDoc(doc);
    ShowFactory *factory = bridge.showFactory();

    DjManager mgr(&view, doc, &bridge, factory);

    Show *show = new Show(doc);
    show->setName("Perform Off Show");
    show->setSyncSource(1); // External
    doc->addFunction(show);
    Scene *scene = new Scene(doc);
    doc->addFunction(scene);
    Track *track = new Track(Function::invalidId(), show);
    track->setName("Audio");
    show->addTrack(track);
    ShowFunction *sf = track->createShowFunction(scene->id());
    sf->setStartTime(0);
    sf->setDuration(300000);
    factory->registerMapping("/m/po.mp3", show->id());

    auto deckTrigger = [&](const QString &t, const QVariant &v) {
        QMetaObject::invokeMethod(&bridge, "onDeckTrigger",
            Q_ARG(int, 0), Q_ARG(QString, t), Q_ARG(QVariant, v));
        QMetaObject::invokeMethod(&bridge, "onTickEnded");
    };
    deckTrigger("get_filepath", QVariant("/m/po.mp3"));
    deckTrigger("loaded", QVariant("on"));
    QMetaObject::invokeMethod(&bridge, "onDeckTrigger",
        Q_ARG(int, 0), Q_ARG(QString, "masterdeck"), Q_ARG(QVariant, QVariant("on")));

    doc->masterTimer()->start();
    mgr.setPerformMode(true);
    deckTrigger("play", QVariant("on"));
    QTest::qWait(60);
    QVERIFY(show->isRunning());
    QVERIFY(!show->isPaused());

    // Turning Perform OFF stops the running External-sync show.
    mgr.setPerformMode(false);
    QTRY_VERIFY(!show->isRunning());

    doc->masterTimer()->stop();
    delete doc;
}

void DjManager_Test::performLoadsAndSwitchesActiveShow()
{
    Doc *doc = createDoc();
    QQuickView view;
    VdjBridge bridge;
    bridge.setDoc(doc);
    ShowFactory *factory = bridge.showFactory();

    DjManager mgr(&view, doc, &bridge, factory);
    mgr.setDeckCount(2);

    auto buildShow = [&](const QString &name) {
        Show *s = new Show(doc); s->setName(name); s->setSyncSource(1); doc->addFunction(s);
        Scene *sc = new Scene(doc); doc->addFunction(sc);
        Track *t = new Track(Function::invalidId(), s); s->addTrack(t);
        ShowFunction *sf = t->createShowFunction(sc->id());
        sf->setStartTime(0); sf->setDuration(300000);
        return s;
    };
    Show *showA = buildShow("Show A");
    Show *showB = buildShow("Show B");
    factory->registerMapping("/m/a.mp3", showA->id());
    factory->registerMapping("/m/b.mp3", showB->id());

    auto deckTrigger = [&](int idx, const QString &t, const QVariant &v) {
        QMetaObject::invokeMethod(&bridge, "onDeckTrigger",
            Q_ARG(int, idx), Q_ARG(QString, t), Q_ARG(QVariant, v));
        QMetaObject::invokeMethod(&bridge, "onTickEnded");
    };
    deckTrigger(0, "get_filepath", QVariant("/m/a.mp3"));
    deckTrigger(0, "loaded", QVariant("on"));
    deckTrigger(1, "get_filepath", QVariant("/m/b.mp3"));
    deckTrigger(1, "loaded", QVariant("on"));
    deckTrigger(0, "play", QVariant("on"));
    deckTrigger(1, "play", QVariant("on"));

    doc->masterTimer()->start();

    // Active deck 1 + Perform ON → show A loads/runs; show B is not the active
    // show so it is not actively running.
    master(bridge, 0);
    mgr.setPerformMode(true);
    QTest::qWait(60);
    QVERIFY(showA->isRunning() && !showA->isPaused());
    QVERIFY(!(showB->isRunning() && !showB->isPaused()));

    // Active deck changes to deck 2 → show B is loaded immediately, show A stopped.
    master(bridge, 1);
    QTest::qWait(60);
    QVERIFY(showB->isRunning() && !showB->isPaused());
    QVERIFY(!showA->isRunning());

    doc->masterTimer()->stop();
    delete doc;
}

void DjManager_Test::externalPositionSyncDrivesShowWhenPerforming()
{
    Doc *doc = createDoc();
    QQuickView view;
    VdjBridge bridge;
    bridge.setDoc(doc);
    ShowFactory *factory = bridge.showFactory();

    DjManager mgr(&view, doc, &bridge, factory);

    // Show with External sync containing a Scene item that starts at 2000ms.
    // While performing, pushing the deck's elapsed position past 2000ms must
    // drive the show runner so the inner Scene starts.
    Show *show = new Show(doc);
    show->setName("Sync Show");
    show->setSyncSource(1); // External
    doc->addFunction(show);
    Scene *scene = new Scene(doc);
    doc->addFunction(scene);
    Track *track = new Track(Function::invalidId(), show);
    track->setName("Audio");
    show->addTrack(track);
    ShowFunction *sf = track->createShowFunction(scene->id());
    sf->setStartTime(2000);
    sf->setDuration(60000);
    factory->registerMapping("/m/sync.mp3", show->id());

    auto deckTrigger = [&](const QString &t, const QVariant &v) {
        QMetaObject::invokeMethod(&bridge, "onDeckTrigger",
            Q_ARG(int, 0), Q_ARG(QString, t), Q_ARG(QVariant, v));
        QMetaObject::invokeMethod(&bridge, "onTickEnded");
    };
    deckTrigger("get_filepath", QVariant("/m/sync.mp3"));
    deckTrigger("loaded", QVariant("on"));
    master(bridge, 0);

    doc->masterTimer()->start();
    mgr.setPerformMode(true);
    deckTrigger("play", QVariant("on"));
    QTest::qWait(60);
    QVERIFY(show->isRunning());

    // The inner Scene (start 2000ms) must NOT have started yet (elapsed 0).
    QSignalSpy runSpy(scene, &Function::running);
    QCOMPARE(runSpy.count(), 0);

    // Push the deck position to 3000ms THROUGH the bridge — past the inner
    // item's 2000ms start. The perform-gated sync feeds the show runner,
    // which then starts the inner Scene. (VDJ time fields are milliseconds.)
    deckTrigger("get_time elapsed absolute", QVariant(3000.0));

    bool started = false;
    for (int i = 0; i < 40 && !started; ++i)
    {
        QTest::qWait(50);
        started = runSpy.count() > 0;
    }
    QVERIFY(started); // external position (via bridge) advanced the show runner

    doc->masterTimer()->stop();
    delete doc;
}

void DjManager_Test::performShowsActiveShowInShowManager()
{
    Doc *doc = createDoc();
    QQuickView view;
    VdjBridge bridge;
    bridge.setDoc(doc);
    DjFsm *fsm = bridge.djFsm();
    DjManager mgr(&view, doc, &bridge, bridge.showFactory());

    // two songs with assigned shows on decks 1 and 2
    Show *show1 = new Show(doc);
    show1->setName("Perform One");
    show1->setPath(kSongFolderPath);
    doc->addFunction(show1);
    Show *show2 = new Show(doc);
    show2->setName("Perform Two");
    show2->setPath(kSongFolderPath);
    doc->addFunction(show2);

    feedSong(fsm, 1, "/m/one.mp3", "One", "DJ", 120.0);
    feedSong(fsm, 2, "/m/two.mp3", "Two", "DJ", 124.0);
    mgr.assignShow("/m/one.mp3", static_cast<int>(show1->id()));
    mgr.assignShow("/m/two.mp3", static_cast<int>(show2->id()));

    auto setMasterDeck = [&bridge](int deck) { master(bridge, deck - 1); };

    QSignalSpy spy(&bridge, &VdjBridge::performShowSelected);

    // Perform OFF: active deck changes must not touch the Show Manager
    setMasterDeck(1);
    QCOMPARE(spy.count(), 0);

    // enabling Perform loads the active deck's show right away
    mgr.setPerformMode(true);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy[0][0].toInt(), static_cast<int>(show1->id()));

    // switching the master deck follows with the new deck's show
    setMasterDeck(2);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy[1][0].toInt(), static_cast<int>(show2->id()));

    // Perform OFF again: deck changes no longer drive the Show Manager
    mgr.setPerformMode(false);
    setMasterDeck(1);
    QCOMPARE(spy.count(), 2);

    delete doc;
}

void DjManager_Test::performResolvesShowAfterWorkspaceReload()
{
    Doc *doc = createDoc();
    QQuickView view;
    VdjBridge bridge;
    bridge.setDoc(doc);

    // Simulate a REOPENED workspace: the Show exists in the Doc with its
    // Audio child, but the session-level factory mapping is empty.
    Show *show = new Show(doc);
    show->setName("Reloaded");
    show->setPath(kSongFolderPath);
    doc->addFunction(show);

    Audio *audio = new Audio(doc);
    audio->setSourceFileName("/m/reload.mp3");
    doc->addFunction(audio);

    Track *track = new Track(Function::invalidId(), show);
    show->addTrack(track);
    ShowFunction *sf = track->createShowFunction(audio->id());
    sf->setStartTime(0);
    sf->setDuration(300000);

    // the index reads the Doc: the Show is the song's before any manager exists
    QCOMPARE(bridge.showFactory()->showIdForFilepath("/m/reload.mp3"), show->id());

    DjManager mgr(&view, doc, &bridge, bridge.showFactory());
    QCOMPARE(bridge.showFactory()->showIdForFilepath("/m/reload.mp3"), show->id());

    // VDJ loads the song: the existing show is reused, no duplicate created
    const int showsBefore = doc->functionsByType(Function::ShowType).count();
    feedSong(bridge.djFsm(), 1, "/m/reload.mp3", "Reloaded", "DJ", 120.0);
    master(bridge, 0);

    // Enabling Perform resolves the reloaded show immediately and the Show
    // Manager follow fires — the previously distrusted case.
    QSignalSpy loadSpy(&bridge, &VdjBridge::performShowSelected);
    mgr.setPerformMode(true);
    QCOMPARE(bridge.performFsm()->state(), PerformFsm::PerformState::Suspended);
    QCOMPARE(bridge.performFsm()->activeShowId(), show->id());
    QCOMPARE(loadSpy.count(), 1);
    QCOMPARE(loadSpy[0][0].toInt(), static_cast<int>(show->id()));
    QCOMPARE(doc->functionsByType(Function::ShowType).count(), showsBefore);

    delete doc;
}

void DjManager_Test::deckTableReflectsFsmStateAndUpdates()
{
    Doc *doc = createDoc();
    QQuickView view;
    VdjBridge bridge;
    bridge.setDoc(doc);
    DjFsm *fsm = bridge.djFsm();

    DjManager mgr(&view, doc, &bridge, bridge.showFactory());
    mgr.setDeckCount(2);

    QAbstractItemModel *deckModel = mgr.deckModel();
    QCOMPARE(deckModel->rowCount(QModelIndex()), 2);

    QSignalSpy dataSpy(deckModel, &QAbstractItemModel::dataChanged);

    // Loading a song on deck 1 must be reflected in the deck table row 0.
    feedSong(fsm, 1, "/m/a.mp3", "Song A", "Artist A", 120.0);
    QModelIndex i0 = deckModel->index(0, 0);
    QCOMPARE(deckModel->data(i0, DjDeckModel::TitleRole).toString(), QString("Song A"));
    QCOMPARE(deckModel->data(i0, DjDeckModel::ArtistRole).toString(), QString("Artist A"));
    QCOMPARE(deckModel->data(i0, DjDeckModel::StateRole).toString(), QString("Loaded"));
    QVERIFY(dataSpy.count() > 0); // QML is notified of the change

    // Playing deck 1 updates its state to Playing.
    fsm->onDeckTrigger(1, "play", "on");
    fsm->onTickEnded();
    QCOMPARE(deckModel->data(i0, DjDeckModel::StateRole).toString(), QString("Playing"));

    delete doc;
}

void DjManager_Test::deckTablePositionReflectsActiveDeck()
{
    Doc *doc = createDoc();
    QQuickView view;
    VdjBridge bridge;
    bridge.setDoc(doc);
    DjFsm *fsm = bridge.djFsm();

    DjManager mgr(&view, doc, &bridge, bridge.showFactory());
    mgr.setDeckCount(2);

    feedSong(fsm, 1, "/m/a.mp3", "Song A", "Artist A", 120.0);

    // Make deck 1 the active deck so the active-position properties track it.
    master(bridge, 0);
    QCOMPARE(mgr.activeDeck(), 1);

    QAbstractItemModel *deckModel = mgr.deckModel();
    QSignalSpy dataSpy(deckModel, &QAbstractItemModel::dataChanged);
    QSignalSpy posSpy(&mgr, &DjManager::activePositionChanged);

    // Drive a play position into the FSM for deck 1.
    // VDJ sends time fields in MILLISECONDS (verified from live telemetry).
    fsm->onDeckTrigger(1, "get_beatpos", 16.05);
    fsm->onDeckTrigger(1, "get_time elapsed absolute", 7602.0); // ms elapsed
    fsm->onDeckTrigger(1, "get_time", 351883.0);                // ms remaining

    // The deck table row exposes beat + elapsed/remaining for the QML display.
    QModelIndex i0 = deckModel->index(0, 0);
    QVERIFY(qFuzzyCompare(deckModel->data(i0, DjDeckModel::BeatPosRole).toDouble(), 16.05));
    QCOMPARE(deckModel->data(i0, DjDeckModel::ElapsedRole).toInt(), 7602);
    QCOMPARE(deckModel->data(i0, DjDeckModel::RemainingRole).toInt(), 351883);
    QVERIFY(dataSpy.count() > 0); // QML is notified of the position change

    // The DjManager active-position properties mirror the active deck and
    // notify QML via activePositionChanged.
    QVERIFY(qFuzzyCompare(mgr.activeBeatPos(), 16.05));
    QCOMPARE(mgr.activeElapsedMs(), 7602);
    QVERIFY(posSpy.count() > 0);

    // A non-active deck's position must NOT move the active-position display.
    int activeEmits = posSpy.count();
    feedSong(fsm, 2, "/m/b.mp3", "Song B", "Artist B", 124.0);
    fsm->onDeckTrigger(2, "get_time elapsed absolute", 42.0);
    QCOMPARE(mgr.activeElapsedMs(), 7602);      // still deck 1's position
    QCOMPARE(posSpy.count(), activeEmits);      // no active-position emission

    delete doc;
}

void DjManager_Test::deckCountSettingLimitsTable()
{
    Doc *doc = createDoc();
    QQuickView view;
    VdjBridge bridge;
    bridge.setDoc(doc);

    DjManager mgr(&view, doc, &bridge, bridge.showFactory());
    mgr.setDeckCount(2);
    QCOMPARE(mgr.deckCount(), 2);
    QCOMPARE(mgr.deckModel()->rowCount(QModelIndex()), 2);

    QSignalSpy spy(&mgr, &DjManager::deckCountChanged);
    mgr.setDeckCount(4);
    QCOMPARE(mgr.deckCount(), 4);
    QCOMPARE(mgr.deckModel()->rowCount(QModelIndex()), 4);
    QVERIFY(spy.count() >= 1);

    mgr.setDeckCount(2); // restore for other runs
    delete doc;
}

void DjManager_Test::getDecksDrivesDeckCount()
{
    Doc *doc = createDoc();
    QQuickView view;
    VdjBridge bridge;
    bridge.setDoc(doc);
    DjFsm *fsm = bridge.djFsm();

    DjManager mgr(&view, doc, &bridge, bridge.showFactory());
    mgr.setDeckCount(4); // start at 4 (e.g. a persisted setting) → phantom decks would show
    QCOMPARE(mgr.deckModel()->rowCount(QModelIndex()), 4);

    auto sendDecks = [&](const QVariant &v) {
        QMetaObject::invokeMethod(&bridge, "onGlobalTrigger",
            Q_ARG(QString, "get_decks"), Q_ARG(QVariant, v));
    };

    // VDJ reports 2 real decks → tracked count drops to 2 and the table shrinks.
    // The telemetry client coerces JSON numbers to double, so feed double to
    // exercise the real wire type (not just int).
    sendDecks(QVariant(2.0));
    QCOMPARE(fsm->deckCount(), 2);
    QCOMPARE(mgr.deckModel()->rowCount(QModelIndex()), 2);

    // A phantom deck-3 trigger is now ignored (deck 3 is beyond the count).
    feedSong(fsm, 3, "/m/phantom.mp3", "Phantom", "Ghost", 120.0);
    QVERIFY(!fsm->deckAt(2).song.isValid());

    // Bounds: out-of-range values are clamped, non-numeric is ignored, no crash.
    sendDecks(QVariant(9.0));          // → clamp to MaxDecks (4)
    QCOMPARE(fsm->deckCount(), 4);
    sendDecks(QVariant(0.0));          // → clamp to 1
    QCOMPARE(fsm->deckCount(), 1);
    sendDecks(QVariant("not a number")); // → ignored, count unchanged
    QCOMPARE(fsm->deckCount(), 1);

    delete doc;
}


void DjManager_Test::perDeckMasterdeckSetsActiveDeck()
{
    Doc *doc = createDoc();
    QQuickView view;
    VdjBridge bridge;
    bridge.setDoc(doc);
    DjFsm *fsm = bridge.djFsm();

    DjManager mgr(&view, doc, &bridge, bridge.showFactory());
    mgr.setDeckCount(2);

    feedSong(fsm, 1, "/m/d1.mp3", "One", "A1", 120.0);
    feedSong(fsm, 2, "/m/d2.mp3", "Two", "A2", 124.0);

    // the per-deck masterdeck flag names the active deck
    master(bridge, 0);
    QCOMPARE(mgr.activeDeck(), 1);
    QCOMPARE(mgr.activeTitle(), QString("One"));

    master(bridge, 1);
    QCOMPARE(mgr.activeDeck(), 2);
    QCOMPARE(mgr.activeTitle(), QString("Two"));
    QCOMPARE(mgr.activeArtist(), QString("A2"));

    // an eject empties the header too, though the deck keeps VDJ's last artist for the next song
    fsm->onDeckTrigger(2, "loaded", "off");
    fsm->onTickEnded();
    QCOMPARE(mgr.activeDeck(), 2);
    QCOMPARE(mgr.activeTitle(), QString());
    QCOMPARE(mgr.activeArtist(), QString());

    delete doc;
}

void DjManager_Test::deferredShowCreationRevalidates_data()
{
    QTest::addColumn<QString>("meanwhile");
    QTest::addColumn<bool>("created");
    QTest::addColumn<QString>("showName");

    QTest::newRow("nothing: built from the completed burst") << "" << true << "DJ - Late";
    QTest::newRow("assigned by the user") << "assign" << false << "Chosen";
    QTest::newRow("cleared by the user") << "cleared" << false << "";
    QTest::newRow("a Show of that name exists: a moved file is a new song") << "named" << true << "DJ - Late";
    QTest::newRow("the workspace was replaced: the deck's song gets a Show in the new one")
        << "clear" << true << "DJ - Late";
}

void DjManager_Test::deferredShowCreationRevalidates()
{
    QFETCH(QString, meanwhile);
    QFETCH(bool, created);
    QFETCH(QString, showName);

    Doc *doc = createDoc();
    QQuickView view;
    VdjBridge bridge;
    bridge.setDoc(doc);
    ShowFactory *factory = bridge.showFactory();
    DjFsm *fsm = bridge.djFsm();
    DjManager mgr(&view, doc, &bridge, factory);
    QSignalSpy createSpy(factory, &ShowFactory::showCreatedForSong);

    // a manual load: the artist and bpm in the first write, the filepath in the second
    fsm->onDeckTrigger(1, "get_title", "Late");
    fsm->onDeckTrigger(1, "get_artist", "DJ");
    fsm->onDeckTrigger(1, "get_bpm", 120.0);
    fsm->onDeckTrigger(1, "loaded", "on");
    fsm->onTickEnded();
    fsm->onDeckTrigger(1, "get_filepath", "/m/late.mp3");
    fsm->onTickEnded();
    QCOMPARE(mgr.songListModel()->rowCount(QModelIndex()), 1);

    Show *named = nullptr;
    if (meanwhile == QLatin1String("assign") || meanwhile == QLatin1String("named")
        || meanwhile == QLatin1String("cleared"))
    {
        named = new Show(doc);
        named->setName(meanwhile == QLatin1String("assign") ? "Chosen" : "DJ - Late");
        doc->addFunction(named);
        if (meanwhile != QLatin1String("named"))
            mgr.assignShow("/m/late.mp3", int(named->id()));
        if (meanwhile == QLatin1String("cleared"))
            mgr.clearShow("/m/late.mp3");
    }
    else if (meanwhile == QLatin1String("clear"))
    {
        doc->clearContents();
    }

    waitForShowCreation();
    QCOMPARE(createSpy.count(), created ? 1 : 0);
    const quint32 showId = mgr.songListModel()->rowCount(QModelIndex()) > 0
        ? mgr.songListModel()->data(mgr.songListModel()->index(0, 0), DjSongModel::ShowIdRole).toUInt()
        : quint32(Function::invalidId());
    if (meanwhile == QLatin1String("cleared"))
    {
        QCOMPARE(showId, Function::invalidId());
        QCOMPARE(doc->functionsByType(Function::ShowType).count(), 1);
    }
    else
    {
        QVERIFY(showId != Function::invalidId());
        QCOMPARE(doc->function(showId)->name(), showName);
        if (meanwhile == QLatin1String("named"))
            QVERIFY(showId != named->id());
        QCOMPARE(doc->functionsByType(Function::ShowType).count(), meanwhile == QLatin1String("named") ? 2 : 1);
    }

    delete doc;
}

void DjManager_Test::playingASongModifiesTheWorkspace()
{
    Doc *doc = createDoc();
    QQuickView view;
    VdjBridge bridge;
    bridge.setDoc(doc);
    ShowFactory *factory = bridge.showFactory();
    DjFsm *fsm = bridge.djFsm();
    DjManager mgr(&view, doc, &bridge, factory);

    DjFsm::DeckSong seed; seed.filepath = "/m/played.mp3"; seed.title = "Played"; seed.artist = "DJ"; seed.bpm = 120.0;
    const quint32 showId = factory->buildShow(seed);
    feedSong(fsm, 1, "/m/played.mp3", "Played", "DJ", 120.0);
    doc->resetModified();
    QVERIFY(!doc->function(showId)->lastPlayed().isValid());

    // lastPlayed is saved with the workspace: playing dirties it
    fsm->onDeckTrigger(1, "play", "on");
    fsm->onTickEnded();
    QVERIFY(doc->function(showId)->lastPlayed().isValid());
    QVERIFY(doc->isModified());

    delete doc;
}

// row index in the song list by filepath, -1 if absent
static int rowOf(QAbstractItemModel *songs, const QString &fp)
{
    for (int i = 0; i < songs->rowCount(QModelIndex()); ++i)
        if (songs->data(songs->index(i, 0), DjSongModel::FilepathRole).toString() == fp)
            return i;
    return -1;
}

void DjManager_Test::emptyArtistReachesTheRow()
{
    // B is an untagged file after a tagged one: its artist "" is a value the row takes
    Doc *doc = createDoc();
    QQuickView view;
    VdjBridge bridge;
    bridge.setDoc(doc);
    DjFsm *fsm = bridge.djFsm();
    DjManager mgr(&view, doc, &bridge, bridge.showFactory());
    QAbstractItemModel *songs = mgr.songListModel();

    feedSong(fsm, 1, "/m/a.mp3", "A", "X", 125.0);
    feedSong(fsm, 1, "/m/b.mp3", "B", "", 125.0);
    const int b = rowOf(songs, "/m/b.mp3");
    QVERIFY(b >= 0);
    QCOMPARE(songs->data(songs->index(b, 0), DjSongModel::ArtistRole).toString(), QString());
    // and a later change to "" too
    fsm->onDeckTrigger(1, "get_artist", "");
    fsm->onTickEnded();
    fsm->onDeckTrigger(1, "get_time elapsed absolute", 100.0);
    fsm->onTickEnded();
    QCOMPARE(songs->data(songs->index(rowOf(songs, "/m/a.mp3"), 0), DjSongModel::ArtistRole).toString(), QString("X"));

    delete doc;
}

void DjManager_Test::newShowBecomesTheSongsShow()
{
    // the song has a Show; "New" builds another, which is the song's from then on
    Doc *doc = createDoc();
    QQuickView view;
    VdjBridge bridge;
    bridge.setDoc(doc);
    ShowFactory *factory = bridge.showFactory();
    DjFsm *fsm = bridge.djFsm();
    DjManager mgr(&view, doc, &bridge, factory);
    QAbstractItemModel *songs = mgr.songListModel();

    DjFsm::DeckSong seed; seed.filepath = "/m/a.mp3"; seed.title = "A"; seed.artist = "X"; seed.bpm = 120.0;
    const quint32 first = factory->buildShow(seed);
    feedSong(fsm, 1, "/m/a.mp3", "A", "X", 120.0);
    QCOMPARE(songs->data(songs->index(0, 0), DjSongModel::ShowIdRole).toUInt(), first);

    QSignalSpy created(factory, &ShowFactory::showCreatedForSong);
    mgr.createShow("/m/a.mp3");
    QCOMPARE(created.count(), 1);
    const quint32 fresh = created[0][1].toUInt();
    QVERIFY(fresh != first);
    QCOMPARE(songs->data(songs->index(0, 0), DjSongModel::ShowIdRole).toUInt(), fresh);

    delete doc;
}

void DjManager_Test::deletedAssignedShowUnassignsTheRow()
{
    Doc *doc = createDoc();
    QQuickView view;
    VdjBridge bridge;
    bridge.setDoc(doc);
    DjFsm *fsm = bridge.djFsm();
    DjManager mgr(&view, doc, &bridge, bridge.showFactory());
    QAbstractItemModel *songs = mgr.songListModel();

    Show *show = new Show(doc);
    show->setName("Chosen");
    doc->addFunction(show);
    feedSong(fsm, 1, "/m/a.mp3", "A", "X", 120.0);
    mgr.assignShow("/m/a.mp3", int(show->id()));
    const quint32 chosenId = show->id();
    QCOMPARE(songs->data(songs->index(0, 0), DjSongModel::ShowIdRole).toUInt(), chosenId);

    doc->deleteFunction(chosenId);
    QSignalSpy load(&mgr, &DjManager::showLoadRequested);
    mgr.loadShow("/m/a.mp3");
    QCOMPARE(load.count(), 0);
    QCOMPARE(songs->data(songs->index(0, 0), DjSongModel::HasShowRole).toBool(), false);

    delete doc;
}

void DjManager_Test::staleDeferredCreationFiresForNothing()
{
    // the workspace is replaced and VDJ reconnects with the same song before
    // the first deadline: the first callback belongs to the old workspace
    Doc *doc = createDoc();
    QQuickView view;
    VdjBridge bridge;
    bridge.setDoc(doc);
    ShowFactory *factory = bridge.showFactory();
    DjFsm *fsm = bridge.djFsm();
    DjManager mgr(&view, doc, &bridge, factory);
    QSignalSpy created(factory, &ShowFactory::showCreatedForSong);

    feedSong(fsm, 1, "/m/a.mp3", "A", "X", 120.0);
    QTest::qWait(DjManager::showCreationDelayMs * 2 / 3);
    doc->clearContents();
    fsm->onDisconnected();
    feedSong(fsm, 1, "/m/a.mp3", "A", "X", 120.0);
    QTest::qWait(DjManager::showCreationDelayMs / 2);
    QCOMPARE(created.count(), 0);
    waitForShowCreation();
    QCOMPARE(created.count(), 1);

    delete doc;
}

void DjManager_Test::workspaceLoadKeepsThePlayingSong()
{
    // a workspace without a Show for the playing song is loaded mid-set
    Doc *doc = createDoc();
    QQuickView view;
    VdjBridge bridge;
    bridge.setDoc(doc);
    ShowFactory *factory = bridge.showFactory();
    DjFsm *fsm = bridge.djFsm();
    DjManager mgr(&view, doc, &bridge, factory);
    QAbstractItemModel *songs = mgr.songListModel();
    QSignalSpy created(factory, &ShowFactory::showCreatedForSong);

    feedSong(fsm, 1, "/m/a.mp3", "A", "X", 120.0);
    fsm->onDeckTrigger(1, "play", "on");
    fsm->onTickEnded();
    doc->clearContents();
    QCOMPARE(fsm->deckAt(0).state, DjFsm::DeckState::Playing);
    QVERIFY(rowOf(songs, "/m/a.mp3") >= 0);
    QCOMPARE(songs->data(songs->index(rowOf(songs, "/m/a.mp3"), 0), DjSongModel::IsPlayingRole).toBool(), true);
    waitForShowCreation();
    QCOMPARE(created.count(), 1);
    QCOMPARE(songs->data(songs->index(rowOf(songs, "/m/a.mp3"), 0), DjSongModel::HasShowRole).toBool(), true);

    delete doc;
}

void DjManager_Test::twoDecksWithOneSongBuildOneShow()
{
    Doc *doc = createDoc();
    QQuickView view;
    VdjBridge bridge;
    bridge.setDoc(doc);
    ShowFactory *factory = bridge.showFactory();
    DjFsm *fsm = bridge.djFsm();
    DjManager mgr(&view, doc, &bridge, factory);
    QSignalSpy created(factory, &ShowFactory::showCreatedForSong);

    feedSong(fsm, 1, "/m/same.mp3", "Same", "X", 120.0);
    feedSong(fsm, 2, "/m/same.mp3", "Same", "X", 120.0);
    waitForShowCreation();
    QCOMPARE(created.count(), 1);
    QCOMPARE(mgr.songListModel()->rowCount(QModelIndex()), 1);

    delete doc;
}

void DjManager_Test::liveWorkspaceClearKeepsPerformOffTheRecycledId()
{
    // Perform is live on a Show; the workspace is replaced and the Show's id
    // goes to a Show of another song: Perform must not adopt it
    Doc *doc = createDoc();
    QQuickView view;
    VdjBridge bridge;
    bridge.setDoc(doc);
    ShowFactory *factory = bridge.showFactory();
    DjFsm *fsm = bridge.djFsm();
    DjManager mgr(&view, doc, &bridge, factory);

    DjFsm::DeckSong seed; seed.filepath = "/m/a.mp3"; seed.title = "A"; seed.artist = "X"; seed.bpm = 120.0;
    const quint32 oldId = factory->buildShow(seed);
    feedSong(fsm, 1, "/m/a.mp3", "A", "X", 120.0);
    fsm->onDeckTrigger(1, "masterdeck", "on");
    fsm->onDeckTrigger(1, "play", "on");
    fsm->onTickEnded();
    mgr.setPerformMode(true);
    QCOMPARE(bridge.performFsm()->state(), PerformFsm::PerformState::Live);
    QCOMPARE(bridge.performFsm()->activeShowId(), oldId);

    doc->clearContents();
    QCOMPARE(bridge.performFsm()->state(), PerformFsm::PerformState::Armed);
    DjFsm::DeckSong other; other.filepath = "/m/other.mp3"; other.title = "Other"; other.artist = "Y"; other.bpm = 120.0;
    const quint32 recycled = factory->buildShow(other);
    QVERIFY(recycled <= oldId);
    QCOMPARE(bridge.performFsm()->state(), PerformFsm::PerformState::Armed);

    // the playing song gets a Show of its own in the new workspace
    waitForShowCreation();
    QCOMPARE(bridge.performFsm()->state(), PerformFsm::PerformState::Live);
    QVERIFY(bridge.performFsm()->activeShowId() != recycled);
    mgr.setPerformMode(false);

    delete doc;
}

QTEST_MAIN(DjManager_Test)
