/*
  Q Light Controller Plus - Unit test
  songloadtracker_test.cpp
*/

#include <QtTest>
#include <QSignalSpy>

#include "songloadtracker_test.h"
#include "songloadtracker.h"

void SongLoadTracker_Test::initialState()
{
    SongLoadTracker tracker;
    QCOMPARE(tracker.masterDeck(), 1);
    QCOMPARE(tracker.hasEmitted("/any/path.mp3"), false);
}

void SongLoadTracker_Test::normalLoadSequence_data()
{
    QTest::addColumn<int>("deck");
    QTest::addColumn<QString>("filepath");
    QTest::addColumn<QString>("title");
    QTest::addColumn<QString>("artist");
    QTest::addColumn<double>("bpm");

    QTest::newRow("deck1_pop")
        << 1 << "/music/song.mp3" << "My Song" << "Artist A" << 128.0;
    QTest::newRow("deck2_edm")
        << 2 << "/music/track.flac" << "Big Drop" << "DJ X" << 140.5;
    QTest::newRow("deck3_slow")
        << 3 << "/library/ballad.wav" << "Slow Dance" << "Singer B" << 72.0;
}

void SongLoadTracker_Test::normalLoadSequence()
{
    QFETCH(int, deck);
    QFETCH(QString, filepath);
    QFETCH(QString, title);
    QFETCH(QString, artist);
    QFETCH(double, bpm);

    SongLoadTracker tracker;
    QSignalSpy spy(&tracker, &SongLoadTracker::songReady);

    // Feed triggers in VDJ's typical order
    tracker.onTrigger(deck, "get_filepath", filepath);
    tracker.onTrigger(deck, "get_title", title);
    tracker.onTrigger(deck, "get_artist", artist);
    tracker.onTrigger(deck, "get_bpm", bpm);
    QCOMPARE(spy.count(), 0); // not yet — loaded not received

    tracker.onTrigger(deck, "loaded", "on");
    QCOMPARE(spy.count(), 1);

    auto info = spy[0][0].value<SongLoadTracker::SongInfo>();
    QCOMPARE(info.deck, deck);
    QCOMPARE(info.filepath, filepath);
    QCOMPARE(info.title, title);
    QCOMPARE(info.artist, artist);
    QCOMPARE(info.bpm, bpm);
    QVERIFY(tracker.hasEmitted(filepath));
}

void SongLoadTracker_Test::outOfOrderFields()
{
    SongLoadTracker tracker;
    QSignalSpy spy(&tracker, &SongLoadTracker::songReady);

    // Filepath first (anchor), then loaded arrives before title
    // — tests that emission waits until ALL required bits set
    tracker.onTrigger(1, "get_filepath", "/late/file.mp3");
    tracker.onTrigger(1, "get_bpm", 130.0);
    tracker.onTrigger(1, "loaded", "on");
    QCOMPARE(spy.count(), 0); // no title or artist yet

    tracker.onTrigger(1, "get_artist", "Early Artist");
    QCOMPARE(spy.count(), 0); // still no title

    tracker.onTrigger(1, "get_title", "Early Title");
    QCOMPARE(spy.count(), 1); // now all REQUIRED bits present

    auto info = spy[0][0].value<SongLoadTracker::SongInfo>();
    QCOMPARE(info.filepath, QString("/late/file.mp3"));
    QCOMPARE(info.title, QString("Early Title"));
}

void SongLoadTracker_Test::placeholderTitleFiltered()
{
    SongLoadTracker tracker;
    QSignalSpy spy(&tracker, &SongLoadTracker::songReady);

    tracker.onTrigger(1, "get_filepath", "/music/real.mp3");
    tracker.onTrigger(1, "get_title", "Drag a song on this deck to load it");
    tracker.onTrigger(1, "get_artist", "Nobody");
    tracker.onTrigger(1, "get_bpm", 120.0);
    tracker.onTrigger(1, "loaded", "on");

    // Placeholder title is filtered — F_Title bit never set
    QCOMPARE(spy.count(), 0);

    // Provide a real title → should now emit
    tracker.onTrigger(1, "get_title", "Real Song");
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy[0][0].value<SongLoadTracker::SongInfo>().title, QString("Real Song"));
}

void SongLoadTracker_Test::emptyFilepathResets()
{
    SongLoadTracker tracker;
    QSignalSpy spy(&tracker, &SongLoadTracker::songReady);

    tracker.onTrigger(1, "get_filepath", "/music/song.mp3");
    tracker.onTrigger(1, "get_title", "Song");
    // Empty filepath resets the slot
    tracker.onTrigger(1, "get_filepath", "");
    tracker.onTrigger(1, "get_artist", "Artist");
    tracker.onTrigger(1, "get_bpm", 128.0);
    tracker.onTrigger(1, "loaded", "on");

    // Should not emit — filepath was cleared
    QCOMPARE(spy.count(), 0);
}

void SongLoadTracker_Test::linkedDeckDedup()
{
    SongLoadTracker tracker;
    QSignalSpy spy(&tracker, &SongLoadTracker::songReady);

    auto feedDeck = [&](int deck, const QString &path) {
        tracker.onTrigger(deck, "get_filepath", path);
        tracker.onTrigger(deck, "get_title", "Same Song");
        tracker.onTrigger(deck, "get_artist", "Same Artist");
        tracker.onTrigger(deck, "get_bpm", 125.0);
        tracker.onTrigger(deck, "loaded", "on");
    };

    feedDeck(1, "/music/shared.mp3");
    QCOMPARE(spy.count(), 1);

    // Same filepath on deck 2 (linked deck scenario) — should NOT emit again
    feedDeck(2, "/music/shared.mp3");
    QCOMPARE(spy.count(), 1);
}

void SongLoadTracker_Test::disconnectResetsAll()
{
    SongLoadTracker tracker;
    QSignalSpy spy(&tracker, &SongLoadTracker::songReady);

    // Load a song
    tracker.onTrigger(1, "get_filepath", "/music/song.mp3");
    tracker.onTrigger(1, "get_title", "Song");
    tracker.onTrigger(1, "get_artist", "Artist");
    tracker.onTrigger(1, "get_bpm", 128.0);
    tracker.onTrigger(1, "loaded", "on");
    QCOMPARE(spy.count(), 1);

    // Disconnect
    tracker.onDisconnected();
    QCOMPARE(tracker.hasEmitted("/music/song.mp3"), false);

    // After reconnect, same song should emit again
    tracker.onTrigger(1, "get_filepath", "/music/song.mp3");
    tracker.onTrigger(1, "get_title", "Song");
    tracker.onTrigger(1, "get_artist", "Artist");
    tracker.onTrigger(1, "get_bpm", 128.0);
    tracker.onTrigger(1, "loaded", "on");
    QCOMPARE(spy.count(), 2);
}

void SongLoadTracker_Test::loadedOffClearsLoadedBit()
{
    SongLoadTracker tracker;
    QSignalSpy spy(&tracker, &SongLoadTracker::songReady);

    tracker.onTrigger(1, "get_filepath", "/music/song.mp3");
    tracker.onTrigger(1, "get_title", "Song");
    tracker.onTrigger(1, "get_artist", "Artist");
    tracker.onTrigger(1, "get_bpm", 128.0);
    tracker.onTrigger(1, "loaded", "on");
    QCOMPARE(spy.count(), 1);

    // Disconnect and reload — but loaded goes "off" first
    tracker.onDisconnected();
    tracker.onTrigger(1, "get_filepath", "/music/other.mp3");
    tracker.onTrigger(1, "get_title", "Other");
    tracker.onTrigger(1, "get_artist", "Other Artist");
    tracker.onTrigger(1, "get_bpm", 130.0);
    tracker.onTrigger(1, "loaded", "off");
    QCOMPARE(spy.count(), 1); // should not emit yet

    tracker.onTrigger(1, "loaded", "on");
    QCOMPARE(spy.count(), 2);
}

void SongLoadTracker_Test::newFilepathResetsSlot()
{
    SongLoadTracker tracker;
    QSignalSpy spy(&tracker, &SongLoadTracker::songReady);

    // Start loading one song
    tracker.onTrigger(1, "get_filepath", "/music/first.mp3");
    tracker.onTrigger(1, "get_title", "First");
    tracker.onTrigger(1, "get_artist", "Artist");

    // Before completing, a new filepath arrives (user loaded different song)
    tracker.onTrigger(1, "get_filepath", "/music/second.mp3");
    tracker.onTrigger(1, "get_title", "Second");
    tracker.onTrigger(1, "get_artist", "Artist B");
    tracker.onTrigger(1, "get_bpm", 140.0);
    tracker.onTrigger(1, "loaded", "on");
    QCOMPARE(spy.count(), 1);

    auto info = spy[0][0].value<SongLoadTracker::SongInfo>();
    QCOMPARE(info.filepath, QString("/music/second.mp3"));
    QCOMPARE(info.title, QString("Second"));
}

QTEST_MAIN(SongLoadTracker_Test)
