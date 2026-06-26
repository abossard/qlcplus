/*
  Q Light Controller Plus - Unit test
  djfsm_test.cpp

  Licensed under the Apache License, Version 2.0
*/

#include "djfsm_test.h"
#include "djfsm.h"

#include <QSignalSpy>

// Helper: feed a song burst in FILEPATH-FIRST order (filepath then metadata).
static void loadSong(DjFsm &fsm, int deck, const QString &filepath,
                     const QString &title, const QString &artist, double bpm)
{
    fsm.onDeckTrigger(deck, "get_filepath", filepath);
    fsm.onDeckTrigger(deck, "get_title", title);
    fsm.onDeckTrigger(deck, "get_artist", artist);
    fsm.onDeckTrigger(deck, "get_bpm", bpm);
}

// Helper: feed a song burst in METADATA-FIRST order (metadata then filepath),
// matching VirtualDJ's observed ordering where get_filepath arrives last.
static void loadSongMetaFirst(DjFsm &fsm, int deck, const QString &filepath,
                              const QString &title, const QString &artist, double bpm)
{
    fsm.onDeckTrigger(deck, "get_title", title);
    fsm.onDeckTrigger(deck, "get_artist", artist);
    fsm.onDeckTrigger(deck, "get_bpm", bpm);
    fsm.onDeckTrigger(deck, "get_filepath", filepath);
}

void DjFsm_Test::initialState()
{
    DjFsm fsm;
    QCOMPARE(fsm.deckCount(), DjFsm::DefaultDecks); // strictly 2 by default
    QCOMPARE(fsm.activeDeck(), 0);
    QVERIFY(!fsm.activeSong().isValid());
    for (int i = 0; i < DjFsm::MaxDecks; ++i)
    {
        QCOMPARE(fsm.deckAt(i).number, i + 1);
        QCOMPARE(fsm.deckAt(i).state, DjFsm::DeckState::Empty);
        QVERIFY(!fsm.deckAt(i).song.isValid());
    }
}

void DjFsm_Test::loadSequenceAnnouncesOnceAndReachesLoaded()
{
    DjFsm fsm;
    QSignalSpy songSpy(&fsm, &DjFsm::songChanged);

    loadSong(fsm, 1, "/music/a.mp3", "Song A", "Artist A", 120.0);

    QCOMPARE(songSpy.count(), 1);
    QCOMPARE(songSpy[0][0].toInt(), 1);
    auto song = songSpy[0][1].value<DjFsm::DeckSong>();
    QCOMPARE(song.filepath, QString("/music/a.mp3"));
    QCOMPARE(song.title, QString("Song A"));
    QCOMPARE(song.artist, QString("Artist A"));
    QCOMPARE(song.bpm, 120.0);

    QCOMPARE(fsm.deckAt(0).state, DjFsm::DeckState::Loaded);
}

void DjFsm_Test::repeatedMetadataDoesNotReannounce()
{
    DjFsm fsm;
    QSignalSpy songSpy(&fsm, &DjFsm::songChanged);

    loadSong(fsm, 1, "/music/a.mp3", "Song A", "Artist A", 120.0);
    // Same song keeps streaming metadata + bpm jitter.
    fsm.onDeckTrigger(1, "get_bpm", 120.01);
    fsm.onDeckTrigger(1, "get_title", "Song A");
    fsm.onDeckTrigger(1, "get_filepath", "/music/a.mp3");

    QCOMPARE(songSpy.count(), 1);
}

void DjFsm_Test::newFilepathOnSameDeckReannounces()
{
    DjFsm fsm;
    QSignalSpy songSpy(&fsm, &DjFsm::songChanged);

    loadSong(fsm, 2, "/music/a.mp3", "Song A", "Artist A", 120.0);
    loadSong(fsm, 2, "/music/b.mp3", "Song B", "Artist B", 128.0);

    QCOMPARE(songSpy.count(), 2);
    QCOMPARE(songSpy[1][1].value<DjFsm::DeckSong>().filepath, QString("/music/b.mp3"));
    QCOMPARE(fsm.deckAt(1).song.title, QString("Song B"));
}

void DjFsm_Test::placeholderTitleIsIgnored()
{
    DjFsm fsm;
    QSignalSpy songSpy(&fsm, &DjFsm::songChanged);

    fsm.onDeckTrigger(1, "get_filepath", "/music/empty.mp3");
    fsm.onDeckTrigger(1, "get_title", "Drag a song on this deck to load it");
    fsm.onDeckTrigger(1, "get_artist", "Artist");
    fsm.onDeckTrigger(1, "get_bpm", 120.0);

    // Title never satisfied → no announcement, deck stays Loading.
    QCOMPARE(songSpy.count(), 0);
    QCOMPARE(fsm.deckAt(0).state, DjFsm::DeckState::Loading);

    // A real title completes it.
    fsm.onDeckTrigger(1, "get_title", "Real Song");
    QCOMPARE(songSpy.count(), 1);
    QCOMPARE(songSpy[0][1].value<DjFsm::DeckSong>().title, QString("Real Song"));
}

void DjFsm_Test::playPauseTransitions()
{
    DjFsm fsm;
    loadSong(fsm, 1, "/music/a.mp3", "Song A", "Artist A", 120.0);
    QCOMPARE(fsm.deckAt(0).state, DjFsm::DeckState::Loaded);

    fsm.onDeckTrigger(1, "play", "on");
    QCOMPARE(fsm.deckAt(0).state, DjFsm::DeckState::Playing);
    QVERIFY(fsm.deckAt(0).playing);

    fsm.onDeckTrigger(1, "play", "off");
    QCOMPARE(fsm.deckAt(0).state, DjFsm::DeckState::Paused);
    QVERIFY(!fsm.deckAt(0).playing);

    fsm.onDeckTrigger(1, "play", "on");
    QCOMPARE(fsm.deckAt(0).state, DjFsm::DeckState::Playing);
}

void DjFsm_Test::masterDeckDrivesActiveDeckAndSong()
{
    DjFsm fsm;
    QSignalSpy activeDeckSpy(&fsm, &DjFsm::activeDeckChanged);
    QSignalSpy activeSongSpy(&fsm, &DjFsm::activeSongChanged);

    loadSong(fsm, 1, "/music/a.mp3", "Song A", "Artist A", 120.0);
    loadSong(fsm, 2, "/music/b.mp3", "Song B", "Artist B", 128.0);

    fsm.onMasterDeck(2);
    QCOMPARE(fsm.activeDeck(), 2);
    QCOMPARE(activeDeckSpy.count(), 1);
    QCOMPARE(fsm.activeSong().filepath, QString("/music/b.mp3"));
    QVERIFY(activeSongSpy.count() >= 1);

    // Re-asserting the same master deck does nothing.
    fsm.onMasterDeck(2);
    QCOMPARE(activeDeckSpy.count(), 1);
}

void DjFsm_Test::emptyFilepathResetsDeck()
{
    DjFsm fsm;
    loadSong(fsm, 1, "/music/a.mp3", "Song A", "Artist A", 120.0);
    QVERIFY(fsm.deckAt(0).song.isValid());

    fsm.onDeckTrigger(1, "get_filepath", "");
    QCOMPARE(fsm.deckAt(0).state, DjFsm::DeckState::Empty);
    QVERIFY(!fsm.deckAt(0).song.isValid());
}

void DjFsm_Test::disconnectResetsEverything()
{
    DjFsm fsm;
    loadSong(fsm, 1, "/music/a.mp3", "Song A", "Artist A", 120.0);
    fsm.onMasterDeck(1);

    fsm.onDisconnected();
    QCOMPARE(fsm.activeDeck(), 0);
    QVERIFY(!fsm.activeSong().isValid());
    QCOMPARE(fsm.deckAt(0).state, DjFsm::DeckState::Empty);
}

void DjFsm_Test::defaultTwoDecksIgnoresPhantomDeck()
{
    DjFsm fsm; // deckCount defaults to 2
    QSignalSpy songSpy(&fsm, &DjFsm::songChanged);

    // VDJ phantom decks 3 & 4 mirror the active deck — they must be ignored.
    loadSong(fsm, 3, "/music/phantom.mp3", "Phantom", "Ghost", 120.0);
    fsm.onDeckTrigger(4, "play", "on");

    QCOMPARE(songSpy.count(), 0);
    QCOMPARE(fsm.deckAt(2).state, DjFsm::DeckState::Empty);
    QCOMPARE(fsm.deckAt(3).state, DjFsm::DeckState::Empty);

    // Real decks 1 & 2 still work.
    loadSong(fsm, 2, "/music/real.mp3", "Real", "DJ", 124.0);
    QCOMPARE(songSpy.count(), 1);
    QCOMPARE(fsm.deckAt(1).state, DjFsm::DeckState::Loaded);
}

void DjFsm_Test::emptyDeckIsNeverPlaying()
{
    DjFsm fsm;
    // A play trigger on a deck with no song must NOT make it Playing
    // (VDJ broadcasts these to mirror decks).
    fsm.onDeckTrigger(1, "play", "on");
    QCOMPARE(fsm.deckAt(0).state, DjFsm::DeckState::Empty);
    QVERIFY(!fsm.deckAt(0).playing);

    // Once a real song is on the deck, play makes it Playing.
    loadSong(fsm, 1, "/music/a.mp3", "Song A", "Artist A", 120.0);
    fsm.onDeckTrigger(1, "play", "on");
    QCOMPARE(fsm.deckAt(0).state, DjFsm::DeckState::Playing);
}

void DjFsm_Test::setDeckCountResetsDroppedDecks()
{
    DjFsm fsm;
    fsm.setDeckCount(4);
    QCOMPARE(fsm.deckCount(), 4);
    loadSong(fsm, 4, "/music/d4.mp3", "Deck4", "DJ", 120.0);
    QCOMPARE(fsm.deckAt(3).state, DjFsm::DeckState::Loaded);

    // Shrinking the count back to 2 resets the now-untracked deck 4.
    fsm.setDeckCount(2);
    QCOMPARE(fsm.deckCount(), 2);
    QCOMPARE(fsm.deckAt(3).state, DjFsm::DeckState::Empty);
    QVERIFY(!fsm.deckAt(3).song.isValid());

    // Clamped to valid range.
    fsm.setDeckCount(99);
    QCOMPARE(fsm.deckCount(), DjFsm::MaxDecks);
    fsm.setDeckCount(0);
    QCOMPARE(fsm.deckCount(), 1);
}

void DjFsm_Test::metadataFirstOrderAnnouncesCorrectSong()
{
    // VDJ frequently sends title/artist/bpm BEFORE get_filepath. The song must
    // still be announced once with the correct metadata.
    DjFsm fsm;
    QSignalSpy songSpy(&fsm, &DjFsm::songChanged);

    loadSongMetaFirst(fsm, 1, "/music/a.mp3", "Song A", "Artist A", 120.0);

    QCOMPARE(songSpy.count(), 1);
    auto song = songSpy[0][1].value<DjFsm::DeckSong>();
    QCOMPARE(song.filepath, QString("/music/a.mp3"));
    QCOMPARE(song.title, QString("Song A"));
    QCOMPARE(song.artist, QString("Artist A"));
    QCOMPARE(song.bpm, 120.0);
    QCOMPARE(fsm.deckAt(0).song.title, QString("Song A"));
    QCOMPARE(fsm.deckAt(0).state, DjFsm::DeckState::Loaded);
}

void DjFsm_Test::filepathFirstAfterPreviousSongIsNotStale()
{
    // The critical regression: after song A is committed, song B arrives
    // FILEPATH-FIRST. When B's filepath lands, A's title/artist are still
    // buffered — B must NOT be announced with A's stale metadata.
    DjFsm fsm;
    QSignalSpy songSpy(&fsm, &DjFsm::songChanged);

    loadSongMetaFirst(fsm, 1, "/music/a.mp3", "Song A", "Artist A", 120.0);
    QCOMPARE(songSpy.count(), 1);

    // Song B, filepath first (B's metadata not yet arrived).
    fsm.onDeckTrigger(1, "get_filepath", "/music/b.mp3");
    // No announce yet — B has no fresh title.
    QCOMPARE(songSpy.count(), 1);
    fsm.onDeckTrigger(1, "get_title", "Song B");
    fsm.onDeckTrigger(1, "get_artist", "Artist B");
    fsm.onDeckTrigger(1, "get_bpm", 128.0);

    QCOMPARE(songSpy.count(), 2);
    auto b = songSpy[1][1].value<DjFsm::DeckSong>();
    QCOMPARE(b.filepath, QString("/music/b.mp3"));
    QCOMPARE(b.title, QString("Song B"));     // NOT "Song A"
    QCOMPARE(b.artist, QString("Artist B"));
    QCOMPARE(fsm.deckAt(0).song.title, QString("Song B"));
}

void DjFsm_Test::splitFieldOrderDoesNotAnnounceStale()
{
    // Robustness: a new song where get_filepath arrives BETWEEN the fresh title
    // and the fresh artist must not be announced carrying the previous song's
    // artist. Per-field freshness keeps the fresh title, clears the stale
    // artist/bpm, and only announces once B's own artist + bpm arrive.
    DjFsm fsm;
    QSignalSpy songSpy(&fsm, &DjFsm::songChanged);

    loadSong(fsm, 1, "/music/a.mp3", "Song A", "Artist A", 120.0);
    QCOMPARE(songSpy.count(), 1);

    // Song B in a split order: title, filepath, (stale artist A still buffered)
    fsm.onDeckTrigger(1, "get_title", "Song B");
    fsm.onDeckTrigger(1, "get_filepath", "/music/b.mp3");
    // Must NOT have announced B yet (B's own artist/bpm not present).
    QCOMPARE(songSpy.count(), 1);

    fsm.onDeckTrigger(1, "get_artist", "Artist B");
    fsm.onDeckTrigger(1, "get_bpm", 128.0);

    QCOMPARE(songSpy.count(), 2);
    auto b = songSpy[1][1].value<DjFsm::DeckSong>();
    QCOMPARE(b.title, QString("Song B"));
    QCOMPARE(b.artist, QString("Artist B"));   // NOT "Artist A"
    QCOMPARE(b.bpm, 128.0);                      // NOT 120.0
}

void DjFsm_Test::volumeIsTracked()
{
    DjFsm fsm;
    loadSong(fsm, 1, "/music/a.mp3", "Song A", "Artist A", 120.0);

    fsm.onDeckTrigger(1, "volume", 0.75);
    QVERIFY(qFuzzyCompare(fsm.deckAt(0).volume, 0.75));

    fsm.onDeckTrigger(1, "volume", 0.20);
    QVERIFY(qFuzzyCompare(fsm.deckAt(0).volume, 0.20));
}

void DjFsm_Test::positionIsTrackedAndThrottled()
{
    DjFsm fsm;
    loadSong(fsm, 1, "/music/a.mp3", "Song A", "Artist A", 120.0);

    QSignalSpy posSpy(&fsm, &DjFsm::positionChanged);

    // Beat position + elapsed/remaining time are tracked at full resolution.
    // VDJ sends time fields in MILLISECONDS (verified from live telemetry).
    fsm.onDeckTrigger(1, "get_beatpos", 16.05);
    fsm.onDeckTrigger(1, "get_time elapsed absolute", 7602.0);  // ms elapsed
    fsm.onDeckTrigger(1, "get_time", 351883.0);                 // ms remaining
    QVERIFY(qFuzzyCompare(fsm.deckAt(0).beatPos, 16.05));
    QCOMPARE(fsm.deckAt(0).elapsedMs, 7602);
    QCOMPARE(fsm.deckAt(0).remainingMs, 351883);

    int emitsSoFar = posSpy.count();
    QVERIFY(emitsSoFar >= 1); // at least one position notification

    // Sub-threshold updates within the same second / quarter-beat do NOT
    // emit another positionChanged (throttling).
    fsm.onDeckTrigger(1, "get_time elapsed absolute", 7610.0); // still second 7
    fsm.onDeckTrigger(1, "get_beatpos", 16.06);                // still beatQ 64
    QCOMPARE(posSpy.count(), emitsSoFar);

    // Crossing into the next second emits again.
    fsm.onDeckTrigger(1, "get_time elapsed absolute", 8010.0);
    QVERIFY(posSpy.count() > emitsSoFar);
}

void DjFsm_Test::playBeforeFilepathIsRecovered()
{
    // VDJ's field order is unreliable: play=on can arrive BEFORE the new
    // track's metadata. The play flag must survive and become effective once
    // the song is valid (otherwise the deck is stuck "not playing").
    DjFsm fsm;

    // play=on arrives first, while the deck has no valid song yet.
    fsm.onDeckTrigger(1, "play", "on");
    QVERIFY(!fsm.deckAt(0).playing);                          // gated: no song yet
    QCOMPARE(fsm.deckAt(0).state, DjFsm::DeckState::Empty);

    QSignalSpy deckSpy(&fsm, &DjFsm::deckChanged);

    // Now the metadata burst arrives (metadata-first order, filepath last).
    loadSongMetaFirst(fsm, 1, "/music/a.mp3", "Song A", "Artist A", 120.0);

    // The earlier play=on is recovered: the deck is Playing, not Loaded/Paused.
    QVERIFY(fsm.deckAt(0).playing);
    QCOMPARE(fsm.deckAt(0).state, DjFsm::DeckState::Playing);
    QVERIFY(deckSpy.count() > 0); // consumers were notified of the play state
}

void DjFsm_Test::playAcceptsBoolAndNumericForms()
{
    // The protocol documents play as int/bool; the telemetry client coerces
    // JSON true→bool and 1→double. All truthy forms must mean "playing".
    DjFsm fsm;
    loadSong(fsm, 1, "/music/a.mp3", "Song A", "Artist A", 120.0);

    fsm.onDeckTrigger(1, "play", true);                 // JSON bool
    QVERIFY(fsm.deckAt(0).playing);

    fsm.onDeckTrigger(1, "play", false);
    QVERIFY(!fsm.deckAt(0).playing);

    fsm.onDeckTrigger(1, "play", 1.0);                  // JSON number
    QVERIFY(fsm.deckAt(0).playing);

    fsm.onDeckTrigger(1, "play", 0.0);
    QVERIFY(!fsm.deckAt(0).playing);

    fsm.onDeckTrigger(1, "play", "on");                 // legacy string
    QVERIFY(fsm.deckAt(0).playing);
}

QTEST_MAIN(DjFsm_Test)
