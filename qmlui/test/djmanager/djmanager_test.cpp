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
}

void DjManager_Test::initTestCase()
{
    // Isolate QSettings so the persisted deck count doesn't leak between runs.
    QCoreApplication::setOrganizationName("qlcplus-test");
    QCoreApplication::setApplicationName("djmanager_test");
    QSettings().remove("vdj/deckCount");

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
    DjSongModel model(m_doc);
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

void DjManager_Test::modelSetAndClearShow()
{
    DjSongModel model(m_doc);
    DjFsm::DeckSong a;
    a.filepath = "/m/a.mp3"; a.title = "A"; a.artist = "X"; a.bpm = 120.0;
    model.upsertSong(a);

    QCOMPARE(model.data(model.index(0), DjSongModel::HasShowRole).toBool(), false);

    model.setShow("/m/a.mp3", 42);
    QCOMPARE(model.data(model.index(0), DjSongModel::ShowIdRole).toUInt(), 42u);
    QCOMPARE(model.data(model.index(0), DjSongModel::HasShowRole).toBool(), true);

    model.setShow("/m/a.mp3", Function::invalidId());
    QCOMPARE(model.data(model.index(0), DjSongModel::HasShowRole).toBool(), false);
}

void DjManager_Test::modelActiveAndPlayingFlags()
{
    DjSongModel model(m_doc);
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

    // ShowFactory defers creation by 3s; wait for the signal.
    QVERIFY(createSpy.wait(5000));
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
    };
    deckTrigger("get_filepath", QVariant("/m/perform.mp3"));
    deckTrigger("get_title", QVariant("Perform"));
    deckTrigger("get_artist", QVariant("DJ"));
    deckTrigger("get_bpm", QVariant(120.0));

    // Master deck = 1 (drives both FSM active deck and VdjBridge auto-play).
    QMetaObject::invokeMethod(&bridge, "onGlobalTrigger",
        Q_ARG(QString, "masterdeck"), Q_ARG(QVariant, QVariant(1)));

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

void DjManager_Test::performOffPausesRunningShow()
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
    };
    deckTrigger("get_filepath", QVariant("/m/po.mp3"));
    QMetaObject::invokeMethod(&bridge, "onGlobalTrigger",
        Q_ARG(QString, "masterdeck"), Q_ARG(QVariant, QVariant(1)));

    doc->masterTimer()->start();
    mgr.setPerformMode(true);
    deckTrigger("play", QVariant("on"));
    QTest::qWait(60);
    QVERIFY(show->isRunning());
    QVERIFY(!show->isPaused());

    // Turning Perform OFF pauses the running External-sync show.
    mgr.setPerformMode(false);
    QTest::qWait(20);
    QVERIFY(show->isPaused());

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
    };
    deckTrigger(0, "get_filepath", QVariant("/m/a.mp3"));
    deckTrigger(1, "get_filepath", QVariant("/m/b.mp3"));
    deckTrigger(0, "play", QVariant("on"));
    deckTrigger(1, "play", QVariant("on"));

    doc->masterTimer()->start();

    // Active deck 1 + Perform ON → show A loads/runs; show B is not the active
    // show so it is not actively running.
    QMetaObject::invokeMethod(&bridge, "onGlobalTrigger",
        Q_ARG(QString, "masterdeck"), Q_ARG(QVariant, QVariant("on")));
    mgr.setPerformMode(true);
    QTest::qWait(60);
    QVERIFY(showA->isRunning() && !showA->isPaused());
    QVERIFY(!(showB->isRunning() && !showB->isPaused()));

    // Active deck changes to deck 2 → show B is loaded immediately, show A paused.
    QMetaObject::invokeMethod(&bridge, "onGlobalTrigger",
        Q_ARG(QString, "masterdeck"), Q_ARG(QVariant, QVariant("off")));
    QTest::qWait(60);
    QVERIFY(showB->isRunning() && !showB->isPaused());
    QVERIFY(!(showA->isRunning() && !showA->isPaused())); // A paused (or stopped)

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
    };
    deckTrigger("get_filepath", QVariant("/m/sync.mp3"));
    QMetaObject::invokeMethod(&bridge, "onGlobalTrigger",
        Q_ARG(QString, "masterdeck"), Q_ARG(QVariant, QVariant(1)));

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

    auto setMasterDeck = [&bridge](int deck) {
        QMetaObject::invokeMethod(&bridge, "onGlobalTrigger",
            Q_ARG(QString, "masterdeck"), Q_ARG(QVariant, QVariant(deck)));
    };

    QSignalSpy spy(&mgr, &DjManager::showLoadRequested);

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

    QCOMPARE(bridge.showFactory()->showIdForFilepath("/m/reload.mp3"),
             Function::invalidId());

    // DjManager construction rebuilds the model from the Doc and must
    // restore the factory mapping (previously it stayed empty until a
    // 3s deferred name-match, which also duplicated renamed shows).
    DjManager mgr(&view, doc, &bridge, bridge.showFactory());
    QCOMPARE(bridge.showFactory()->showIdForFilepath("/m/reload.mp3"), show->id());

    // VDJ loads the song: the existing show is reused, no duplicate created
    const int showsBefore = doc->functionsByType(Function::ShowType).count();
    feedSong(bridge.djFsm(), 1, "/m/reload.mp3", "Reloaded", "DJ", 120.0);
    QMetaObject::invokeMethod(&bridge, "onGlobalTrigger",
        Q_ARG(QString, "masterdeck"), Q_ARG(QVariant, QVariant(1)));

    // Enabling Perform resolves the reloaded show immediately and the Show
    // Manager follow fires — the previously distrusted case.
    QSignalSpy loadSpy(&mgr, &DjManager::showLoadRequested);
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
    QMetaObject::invokeMethod(&bridge, "onGlobalTrigger",
        Q_ARG(QString, "masterdeck"), Q_ARG(QVariant, QVariant("on")));
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
    QCOMPARE(mgr.activeRemainingMs(), 351883);
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


void DjManager_Test::masterDeckOnOffMapsToActiveDeck()
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

    // masterdeck "on" => deck 1 active; "off" => deck 2 active.
    QMetaObject::invokeMethod(&bridge, "onGlobalTrigger",
        Q_ARG(QString, "masterdeck"), Q_ARG(QVariant, QVariant("on")));
    QCOMPARE(mgr.activeDeck(), 1);
    QCOMPARE(mgr.activeTitle(), QString("One"));

    QMetaObject::invokeMethod(&bridge, "onGlobalTrigger",
        Q_ARG(QString, "masterdeck"), Q_ARG(QVariant, QVariant("off")));
    QCOMPARE(mgr.activeDeck(), 2);
    QCOMPARE(mgr.activeTitle(), QString("Two"));

    delete doc;
}

QTEST_MAIN(DjManager_Test)
