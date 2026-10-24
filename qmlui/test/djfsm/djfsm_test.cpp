/*
  Q Light Controller Plus - Unit test
  djfsm_test.cpp

  Licensed under the Apache License, Version 2.0
*/

#include "djfsm_test.h"
#include "djfsm.h"
#include "vdjcapturereplay.h"

#include <QSignalSpy>

// One line per announced or updated song: "d<deck> <filepath> | <title> | <artist> | <bpm>"
static QStringList songs(const QSignalSpy &spy)
{
    QStringList out;
    for (const QList<QVariant> &args : spy)
    {
        const auto song = args[1].value<DjFsm::DeckSong>();
        out << QStringLiteral("d%1 %2 | %3 | %4 | %5").arg(args[0].toInt()).arg(song.filepath, song.title, song.artist)
                   .arg(song.bpm, 0, 'f', 3);
    }
    return out;
}

// Feed VDJ writes to a deck: "trigger=value;..." objects of one write, '|'
// ends the write (onTickEnded). fp, title, artist (empty allowed), bpm, play,
// elapsed, loaded, vu (get_vu_meter), eq (eq_high).
static void feed(DjFsm &fsm, int deck, const QString &writes)
{
    for (const QString &write : writes.split('|'))
    {
        for (const QString &step : write.split(';', Qt::SkipEmptyParts))
        {
            const QString key = step.section('=', 0, 0), value = step.section('=', 1);
            if (key == "fp")
                fsm.onDeckTrigger(deck, "get_filepath", value);
            else if (key == "title")
                fsm.onDeckTrigger(deck, "get_title", value);
            else if (key == "artist")
                fsm.onDeckTrigger(deck, "get_artist", value);
            else if (key == "bpm")
                fsm.onDeckTrigger(deck, "get_bpm", value.toDouble());
            else if (key == "play")
                fsm.onDeckTrigger(deck, "play", value);
            else if (key == "elapsed")
                fsm.onDeckTrigger(deck, "get_time elapsed absolute", value.toDouble());
            else if (key == "loaded")
                fsm.onDeckTrigger(deck, "loaded", value);
            else if (key == "vu")
                fsm.onDeckTrigger(deck, "get_vu_meter", value.toDouble());
            else if (key == "eq")
                fsm.onDeckTrigger(deck, "eq_high", value.toDouble());
        }
        if (&write != &writes.split('|').last() || writes.endsWith('|'))
            fsm.onTickEnded();
    }
}

// Helper: an Automix load, one write in FILEPATH-FIRST order.
static void loadSong(DjFsm &fsm, int deck, const QString &filepath,
                     const QString &title, const QString &artist, double bpm)
{
    fsm.onDeckTrigger(deck, "get_filepath", filepath);
    fsm.onDeckTrigger(deck, "get_title", title);
    fsm.onDeckTrigger(deck, "get_artist", artist);
    fsm.onDeckTrigger(deck, "get_bpm", bpm);
    fsm.onTickEnded();
}

// Helper: a manual load as VDJ sends it, two writes: metadata, then the filepath.
static void loadSongMetaFirst(DjFsm &fsm, int deck, const QString &filepath,
                              const QString &title, const QString &artist, double bpm)
{
    fsm.onDeckTrigger(deck, "get_title", title);
    fsm.onDeckTrigger(deck, "get_artist", artist);
    fsm.onDeckTrigger(deck, "get_bpm", bpm);
    fsm.onTickEnded();
    fsm.onDeckTrigger(deck, "get_filepath", filepath);
    fsm.onTickEnded();
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
    QSignalSpy updateSpy(&fsm, &DjFsm::songUpdated);

    loadSong(fsm, 1, "/music/a.mp3", "Song A", "Artist A", 120.0);

    // one write, one announcement with everything it brought
    QCOMPARE(songs(songSpy), QStringList{ "d1 /music/a.mp3 | Song A | Artist A | 120.000" });
    QCOMPARE(updateSpy.count(), 0);
    QCOMPARE(fsm.deckAt(0).state, DjFsm::DeckState::Loaded);
}

void DjFsm_Test::repeatedMetadataDoesNotReannounce()
{
    DjFsm fsm;
    QSignalSpy songSpy(&fsm, &DjFsm::songChanged);

    loadSong(fsm, 1, "/music/a.mp3", "Song A", "Artist A", 120.0);
    QSignalSpy updateSpy(&fsm, &DjFsm::songUpdated);
    // a resubscribe snapshot resends the song with bpm jitter: nothing moves
    feed(fsm, 1, "bpm=120.01;title=Song A;fp=/music/a.mp3|elapsed=100|");

    QCOMPARE(songSpy.count(), 1);
    QCOMPARE(updateSpy.count(), 0);
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

void DjFsm_Test::placeholderTitleEmptiesDeck()
{
    // VDJ's eject tick: the placeholder title, get_bpm 120, loaded off, and no
    // empty filepath; the placeholder is the first word of it
    DjFsm fsm;
    loadSong(fsm, 1, "/music/a.mp3", "Song A", "Artist A", 120.0);
    QSignalSpy songSpy(&fsm, &DjFsm::songChanged);
    QSignalSpy updateSpy(&fsm, &DjFsm::songUpdated);

    fsm.onDeckTrigger(1, "get_title", "Drag a song on this deck to load it");
    QCOMPARE(fsm.deckAt(0).state, DjFsm::DeckState::Empty);
    fsm.onDeckTrigger(1, "get_bpm", 120.0);
    fsm.onDeckTrigger(1, "loaded", "off");
    fsm.onTickEnded();
    fsm.onDeckTrigger(1, "volume", 0.5);
    fsm.onTickEnded();
    QCOMPARE(fsm.deckAt(0).state, DjFsm::DeckState::Empty);
    QVERIFY(!fsm.deckAt(0).song.isValid());
    QCOMPARE(songSpy.count(), 0);
    QCOMPARE(updateSpy.count(), 0);
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

void DjFsm_Test::loadWrites_data()
{
    QTest::addColumn<QString>("writes");
    QTest::addColumn<QStringList>("announced");
    QTest::addColumn<QStringList>("updated");

    // measured on VDJ 8.5: Automix loads are one write, manual loads two
    // (metadata, then the filepath 33 to 167 ms later), one in eight starts
    // with bpm and firstbeat alone; autoBPMMatch corrects the bpm 35 ms after
    const QString b = "d1 /b | B | Y | 128.000";
    QTest::newRow("Automix: one write") << "fp=/b;title=B;artist=Y;bpm=128|" << QStringList{ b } << QStringList{};
    QTest::newRow("manual: metadata, then the filepath") << "title=B;artist=Y;bpm=128|fp=/b|" << QStringList{ b }
                                                          << QStringList{};
    QTest::newRow("manual: bpm alone, then the rest") << "bpm=128|title=B;artist=Y;fp=/b|" << QStringList{ b }
                                                       << QStringList{};
    QTest::newRow("bpm correction after the load")
        << "title=B;artist=Y;bpm=128|fp=/b|bpm=126|elapsed=100|" << QStringList{ b }
        << QStringList{ "d1 /b | B | Y | 126.000" };
    QTest::newRow("a playing deck's tempo follows at its next tick")
        << "fp=/b;title=B;artist=Y;bpm=128;play=on|bpm=130;elapsed=40|elapsed=80|" << QStringList{ b }
        << QStringList{ "d1 /b | B | Y | 130.000" };
    QTest::newRow("a paused deck's correction waits for its next position sample")
        << "title=B;artist=Y;bpm=128|fp=/b|bpm=126|vu=0|eq=0.5||" << QStringList{ b } << QStringList{};
}

void DjFsm_Test::loadWrites()
{
    QFETCH(QString, writes);
    QFETCH(QStringList, announced);
    QFETCH(QStringList, updated);

    DjFsm fsm;
    QSignalSpy songSpy(&fsm, &DjFsm::songChanged);
    QSignalSpy updateSpy(&fsm, &DjFsm::songUpdated);
    feed(fsm, 1, writes);
    QCOMPARE(songs(songSpy), announced);
    QCOMPARE(songs(updateSpy), updated);
}

void DjFsm_Test::twoDecksLoadInInterleavedWrites()
{
    DjFsm fsm;
    QSignalSpy songSpy(&fsm, &DjFsm::songChanged);
    QSignalSpy updateSpy(&fsm, &DjFsm::songUpdated);
    loadSong(fsm, 1, "/music/old.mp3", "Old", "Z", 100.0);
    songSpy.clear();

    // deck 1's metadata, deck 2's metadata, deck 1's filepath, deck 2's filepath
    feed(fsm, 1, "title=A;artist=X;bpm=125|");
    feed(fsm, 2, "title=B;artist=Y;bpm=128|");
    feed(fsm, 1, "fp=/a|");
    feed(fsm, 2, "fp=/b|");
    QCOMPARE(songs(songSpy), QStringList({ "d1 /a | A | X | 125.000", "d2 /b | B | Y | 128.000" }));
    QCOMPARE(updateSpy.count(), 0);
}

void DjFsm_Test::onChangeSemantics_data()
{
    QTest::addColumn<QString>("first");     // deck 1 steps
    QTest::addColumn<QString>("then");      // deck 1 steps
    QTest::addColumn<QStringList>("announced");
    QTest::addColumn<QStringList>("updated");

    // VDJ sends only changes: what the new song's load does not bring, it shares
    const QString a = "title=A;artist=X;bpm=125;fp=/a|";
    const QString aLine = "d1 /a | A | X | 125.000";
    QTest::newRow("same bpm not resent: the last one is the new song's")
        << a << "title=B;artist=Y;fp=/b|" << QStringList({ aLine, "d1 /b | B | Y | 125.000" }) << QStringList{};
    QTest::newRow("same artist not resent")
        << a << "title=B;bpm=128;fp=/b|" << QStringList({ aLine, "d1 /b | B | X | 128.000" }) << QStringList{};
    QTest::newRow("an empty artist is a value")
        << a << "title=B;artist=;fp=/b|" << QStringList({ aLine, "d1 /b | B |  | 125.000" }) << QStringList{};
    QTest::newRow("an unchanged bpm resent is nothing, and it stays for the next song")
        << a << "bpm=125|fp=/b;title=B|" << QStringList({ aLine, "d1 /b | B | X | 125.000" }) << QStringList{};
    QTest::newRow("a jittered bpm is no update") << a << "bpm=125.01|elapsed=50|" << QStringList{ aLine } << QStringList{};
    QTest::newRow("a tempo change is an update")
        << a << "bpm=130|elapsed=50|" << QStringList{ aLine } << QStringList{ "d1 /a | A | X | 130.000" };
    QTest::newRow("a manual load's first write is no update of A")
        << a << "title=C;artist=Z;bpm=99|fp=/c|" << QStringList({ aLine, "d1 /c | C | Z | 99.000" }) << QStringList{};
    // the same title across files: VDJ never resends it, the held title is the new song's
    QTest::newRow("same title, filepath first: announced with the held title")
        << a << "fp=/b;artist=Y;bpm=128|" << QStringList({ aLine, "d1 /b | A | Y | 128.000" }) << QStringList{};
    QTest::newRow("same title, metadata first: B's artist and bpm are not updates of A")
        << a << "artist=Y;bpm=128|fp=/b|" << QStringList({ aLine, "d1 /b | A | Y | 128.000" }) << QStringList{};
    QTest::newRow("same title and artist (the extended mix): announced at its filepath")
        << a << "bpm=128|fp=/b|" << QStringList({ aLine, "d1 /b | A | X | 128.000" }) << QStringList{};
    QTest::newRow("same title, metadata first, no tags: the empty artist is not an update of A")
        << a << "artist=;bpm=128|fp=/b|" << QStringList({ aLine, "d1 /b | A |  | 128.000" }) << QStringList{};
    // VDJ sends VU zeros every write for a second after a stop, EQ moves any time
    QTest::newRow("a VU write between a manual load's writes is not B's filepath")
        << a << "title=B;artist=Y;bpm=128|vu=0|fp=/b|" << QStringList({ aLine, "d1 /b | B | Y | 128.000" }) << QStringList{};
    QTest::newRow("an EQ write between a manual load's writes is not B's filepath")
        << a << "title=B;artist=Y;bpm=128|eq=0.6|fp=/b|" << QStringList({ aLine, "d1 /b | B | Y | 128.000" }) << QStringList{};
    QTest::newRow("a tempo ramp flushes per write")
        << a << "play=on;elapsed=0|bpm=126;elapsed=40|bpm=127;elapsed=80|bpm=128;elapsed=120|" << QStringList{ aLine }
        << QStringList({ "d1 /a | A | X | 126.000", "d1 /a | A | X | 127.000", "d1 /a | A | X | 128.000" });
}

void DjFsm_Test::onChangeSemantics()
{
    QFETCH(QString, first);
    QFETCH(QString, then);
    QFETCH(QStringList, announced);
    QFETCH(QStringList, updated);

    DjFsm fsm;
    QSignalSpy songSpy(&fsm, &DjFsm::songChanged);
    feed(fsm, 1, first);
    QCOMPARE(songSpy.count(), 1);
    QSignalSpy updateSpy(&fsm, &DjFsm::songUpdated);
    feed(fsm, 1, then);
    QCOMPARE(songs(songSpy), announced);
    QCOMPARE(songs(updateSpy), updated);
    QVERIFY(fsm.deckAt(0).state != DjFsm::DeckState::Loading);
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

void DjFsm_Test::perDeckMasterdeckDrivesActiveDeck_data()
{
    QTest::addColumn<QString>("flags");    // "deck:on|off;..." as VDJ streams them in one tick
    QTest::addColumn<int>("activeDeck");
    QTest::addColumn<int>("changes");

    // measured on VDJ 8.5: at a handover the old master sends off and the new one on
    QTest::newRow("snapshot: deck 1 master") << "1:on;2:off" << 1 << 1;
    QTest::newRow("handover to deck 2") << "1:on;1:off;2:on" << 2 << 2;
    QTest::newRow("back to deck 1") << "1:on;1:off;2:on;2:off;1:on" << 1 << 3;
    QTest::newRow("off alone keeps the master") << "1:on;1:off" << 1 << 1;
    QTest::newRow("phantom deck 3 is ignored") << "1:on;3:on" << 1 << 1;
    QTest::newRow("numeric and boolean forms") << "2:1;1:true" << 1 << 2;
}

void DjFsm_Test::perDeckMasterdeckDrivesActiveDeck()
{
    QFETCH(QString, flags);
    QFETCH(int, activeDeck);
    QFETCH(int, changes);

    DjFsm fsm;
    loadSong(fsm, 1, "/music/a.mp3", "Song A", "Artist A", 120.0);
    loadSong(fsm, 2, "/music/b.mp3", "Song B", "Artist B", 128.0);
    QSignalSpy activeSpy(&fsm, &DjFsm::activeDeckChanged);

    for (const QString &flag : flags.split(';'))
    {
        const QStringList parts = flag.split(':');
        const QString value = parts.at(1);
        fsm.onDeckTrigger(parts.at(0).toInt(), "masterdeck",
                          value == "1" ? QVariant(1.0) : value == "true" ? QVariant(true) : QVariant(value));
    }
    QCOMPARE(fsm.activeDeck(), activeDeck);
    QCOMPARE(fsm.activeSong().filepath, activeDeck == 1 ? QString("/music/a.mp3") : QString("/music/b.mp3"));
    QCOMPARE(activeSpy.count(), changes);
}

void DjFsm_Test::loadedOffEmptiesDeck()
{
    // VDJ's eject sends no empty get_filepath: only loaded off, a placeholder
    // title and get_bpm 120
    DjFsm fsm;
    loadSong(fsm, 1, "/music/a.mp3", "Song A", "Artist A", 120.0);
    fsm.onDeckTrigger(1, "play", "on");
    fsm.onDeckTrigger(1, "get_time elapsed absolute", 50000.0);
    QSignalSpy deckSpy(&fsm, &DjFsm::deckChanged);

    fsm.onDeckTrigger(1, "loaded", "off");
    QCOMPARE(fsm.deckAt(0).state, DjFsm::DeckState::Empty);
    QVERIFY(!fsm.deckAt(0).song.isValid());
    QVERIFY(!fsm.deckAt(0).playing);
    QCOMPARE(deckSpy.count(), 1);
    fsm.onDeckTrigger(1, "get_title", "Drag a song on this deck to load it");
    fsm.onDeckTrigger(1, "get_bpm", 120.0);
    fsm.onTickEnded();
    QCOMPARE(fsm.deckAt(0).state, DjFsm::DeckState::Empty);

    // loaded off on an empty deck (the snapshot of an empty deck) is silent,
    // loaded on changes nothing by itself
    const int notified = deckSpy.count();
    fsm.onDeckTrigger(1, "loaded", "off");
    fsm.onDeckTrigger(1, "loaded", "on");
    QCOMPARE(deckSpy.count(), notified);
    QCOMPARE(fsm.deckAt(0).state, DjFsm::DeckState::Empty);
}

void DjFsm_Test::disconnectDropsActiveDeckFirst()
{
    DjFsm fsm;
    loadSong(fsm, 1, "/music/a.mp3", "Song A", "Artist A", 120.0);
    fsm.onMasterDeck(1);

    // whoever hears a deck change sees no active deck any more
    QList<int> activeAtDeckChange;
    connect(&fsm, &DjFsm::deckChanged, &fsm, [&]() { activeAtDeckChange << fsm.activeDeck(); });
    fsm.onDisconnected();
    QCOMPARE(activeAtDeckChange, QList<int>({ 0, 0, 0, 0 }));
}

void DjFsm_Test::titleAfterFilepathChangesActiveSong()
{
    DjFsm fsm;
    fsm.onMasterDeck(1);
    QSignalSpy activeSongSpy(&fsm, &DjFsm::activeSongChanged);

    // the header follows the committed song, and its later artist
    feed(fsm, 1, "fp=/music/a.mp3;title=Song A|");
    QCOMPARE(activeSongSpy.count(), 1);
    QCOMPARE(fsm.activeSong().title, QString("Song A"));
    feed(fsm, 1, "artist=Artist A|elapsed=10|");
    QCOMPARE(activeSongSpy.count(), 2);
    QCOMPARE(fsm.activeSong().artist, QString("Artist A"));
    feed(fsm, 1, "bpm=120|elapsed=20|");
    QCOMPARE(activeSongSpy.count(), 2);
}

void DjFsm_Test::masterFlagSurvivesTheDeckCount_data()
{
    QTest::addColumn<int>("countBefore");
    QTest::addColumn<QString>("flags");
    QTest::addColumn<int>("activeBefore");
    QTest::addColumn<int>("countAfter");
    QTest::addColumn<int>("activeAfter");

    // the snapshot streams every deck's master flag before get_decks: with a
    // persisted count of 4 a phantom deck 3 (a copy of deck 1) takes it, with
    // the default 2 a real deck 3 of a 4-deck VDJ is hidden
    QTest::newRow("shrink: the real master keeps its flag") << 4 << "1:on;3:on" << 3 << 2 << 1;
    QTest::newRow("shrink: no real deck holds it") << 4 << "1:off;3:on" << 3 << 2 << 0;
    QTest::newRow("grow: the hidden master appears") << 2 << "3:on" << 0 << 4 << 3;
    QTest::newRow("grow: nobody holds it") << 2 << "1:off;3:off" << 0 << 4 << 0;
    QTest::newRow("grow: a tracked master stays") << 2 << "1:on;3:on" << 1 << 4 << 1;
}

void DjFsm_Test::masterFlagSurvivesTheDeckCount()
{
    QFETCH(int, countBefore);
    QFETCH(QString, flags);
    QFETCH(int, activeBefore);
    QFETCH(int, countAfter);
    QFETCH(int, activeAfter);

    DjFsm fsm;
    fsm.setDeckCount(countBefore);
    for (const QString &flag : flags.split(';'))
        fsm.onDeckTrigger(flag.section(':', 0, 0).toInt(), "masterdeck", flag.section(':', 1));
    QCOMPARE(fsm.activeDeck(), activeBefore);
    fsm.setDeckCount(countAfter);
    QCOMPARE(fsm.activeDeck(), activeAfter);
}

void DjFsm_Test::ejectKeepsTheFaderAndTheLastValues()
{
    // VDJ resends neither an unchanged fader nor an unchanged artist after an eject
    DjFsm fsm;
    QSignalSpy songSpy(&fsm, &DjFsm::songChanged);
    loadSong(fsm, 1, "/music/a.mp3", "Song A", "Artist X", 120.0);
    fsm.onDeckTrigger(1, "masterdeck", "on");
    fsm.onDeckTrigger(1, "volume", 1.0);
    fsm.onTickEnded();
    fsm.onDeckTrigger(1, "get_title", "Drag a song on this deck to load it");
    fsm.onDeckTrigger(1, "get_bpm", 120.0);
    fsm.onDeckTrigger(1, "loaded", "off");
    fsm.onTickEnded();
    QCOMPARE(fsm.deckAt(0).state, DjFsm::DeckState::Empty);
    QCOMPARE(fsm.deckAt(0).volume, 1.0);
    QCOMPARE(fsm.activeDeck(), 1);

    // same artist, different bpm: filepath, title, bpm and no get_artist
    feed(fsm, 1, "fp=/music/b.mp3;title=Song B;bpm=125|");
    QCOMPARE(fsm.deckAt(0).state, DjFsm::DeckState::Loaded);
    QCOMPARE(songs(songSpy).last(), QString("d1 /music/b.mp3 | Song B | Artist X | 125.000"));
    QCOMPARE(fsm.deckAt(0).volume, 1.0);
    QCOMPARE(fsm.activeDeck(), 1);
}

void DjFsm_Test::ejectAndReload_data()
{
    QTest::addColumn<QString>("then");  // deck 1 writes while A plays
    QTest::addColumn<QStringList>("announced");
    QTest::addColumn<int>("state");

    // VDJ's eject (measured): the placeholder title, get_bpm 120, loaded off, and
    // play off after it when the deck played. The same file back resends only
    // what the eject changed: title, bpm, loaded on, never the filepath.
    const QString eject = "title=Drag a song on this deck to load it;bpm=120;loaded=off";
    const QString aLine = "d1 /a | A | X | 125.000";
    const int empty = int(DjFsm::DeckState::Empty), loaded = int(DjFsm::DeckState::Loaded);
    QTest::newRow("loaded off then play off") << eject + ";play=off|" << QStringList{ aLine } << empty;
    QTest::newRow("play off then loaded off") << "play=off;" + eject + "|" << QStringList{ aLine } << empty;
    QTest::newRow("the eject's bpm 120 is no tempo of A") << "play=off|" + eject + "|" << QStringList{ aLine } << empty;
    QTest::newRow("the same file back without a bpm: the tempo before the eject")
        << "play=off|" + eject + "|title=A;loaded=on;elapsed=0|" << QStringList({ aLine, aLine }) << loaded;
    QTest::newRow("the same file back with its bpm")
        << "play=off|" + eject + "|title=A;bpm=125;loaded=on;elapsed=0|" << QStringList({ aLine, aLine }) << loaded;
    QTest::newRow("the same file back at another tempo")
        << "play=off|" + eject + "|title=A;bpm=128;loaded=on|" << QStringList({ aLine, "d1 /a | A | X | 128.000" }) << loaded;
    QTest::newRow("loaded on alone brings the file back")
        << "play=off|" + eject + "|loaded=on|" << QStringList({ aLine, aLine }) << loaded;
    QTest::newRow("another song without a bpm: 120 is real")
        << "play=off|" + eject + "|title=B;artist=Y;loaded=on|fp=/b;elapsed=0|"
        << QStringList({ aLine, "d1 /b | B | Y | 120.000" }) << loaded;
    QTest::newRow("another song of the same artist: not resent")
        << "play=off|" + eject + "|title=B;bpm=131;loaded=on|fp=/b|" << QStringList({ aLine, "d1 /b | B | X | 131.000" }) << loaded;
    QTest::newRow("another song's first write announces nothing until its filepath")
        << "play=off|" + eject + "|title=B;bpm=131;loaded=on|elapsed=0|fp=/b|"
        << QStringList({ aLine, "d1 /b | B | X | 131.000" }) << loaded;
    QTest::newRow("a held first write dies with the eject")
        << "play=off|title=B;artist=Y;bpm=128|" + eject + "|loaded=on|" << QStringList({ aLine, aLine }) << loaded;
}

void DjFsm_Test::ejectAndReload()
{
    QFETCH(QString, then);
    QFETCH(QStringList, announced);
    QFETCH(int, state);

    DjFsm fsm;
    QSignalSpy songSpy(&fsm, &DjFsm::songChanged);
    QSignalSpy updateSpy(&fsm, &DjFsm::songUpdated);
    feed(fsm, 1, "title=A;artist=X;bpm=125;fp=/a;loaded=on|play=on;elapsed=3000|");
    QCOMPARE(fsm.deckAt(0).state, DjFsm::DeckState::Playing);
    feed(fsm, 1, then);
    QCOMPARE(songs(songSpy), announced);
    QCOMPARE(updateSpy.count(), 0);
    QCOMPARE(int(fsm.deckAt(0).state), state);
    QVERIFY(!fsm.deckAt(0).playing);
}

void DjFsm_Test::artistAfterTitleChangesActiveSong()
{
    // automix order: filepath, title, artist; the header shows the artist once it arrives
    DjFsm fsm;
    fsm.onMasterDeck(1);
    QString artistAtNotify;
    connect(&fsm, &DjFsm::activeSongChanged, &fsm, [&]() { artistAtNotify = fsm.activeSong().artist; });
    loadSong(fsm, 1, "/music/a.mp3", "Song A", "Artist A", 120.0);
    QCOMPARE(artistAtNotify, QString("Artist A"));
}

void DjFsm_Test::capture_trackChangeSameBpm()
{
    // Automix replaces the track: get_bpm is not resent, the artist follows the title
    DjFsm fsm;
    VdjTelemetryClient client;
    VdjCapture::wire(client, fsm);
    QSignalSpy songSpy(&fsm, &DjFsm::songChanged);
    QSignalSpy updateSpy(&fsm, &DjFsm::songUpdated);
    QVERIFY(VdjCapture::replay(client, "track_change_same_deck_same_bpm") > 0);

    // the snapshot and the Automix change are one write each: complete snapshots,
    // the bpm not resent is the new song's too
    QCOMPARE(songs(songSpy), QStringList({ "d1 /music/track-1.mp3 | Title 1 | Artist 1 | 126.000",
                                           "d1 /music/track-2.mp3 | Title 2 | Artist 2 | 126.000" }));
    QCOMPARE(updateSpy.count(), 0);
    QCOMPARE(fsm.deckAt(0).song.bpm, 126.000122);
    QCOMPARE(fsm.deckAt(0).state, DjFsm::DeckState::Playing);
    QCOMPARE(fsm.deckAt(1).state, DjFsm::DeckState::Empty);
    QCOMPARE(fsm.deckCount(), 2);
}

void DjFsm_Test::capture_consecutiveLoads()
{
    // four manual loads on deck 1 (same bpm, same artist, empty artist), one on deck 2
    DjFsm fsm;
    VdjTelemetryClient client;
    VdjCapture::wire(client, fsm);
    QSignalSpy songSpy(&fsm, &DjFsm::songChanged);
    QSignalSpy updateSpy(&fsm, &DjFsm::songUpdated);
    QVERIFY(VdjCapture::replay(client, "consecutive_loads_same_bpm_artist_empty_quote") > 0);

    QCOMPARE(updateSpy.count(), 0);
    QCOMPARE(songs(songSpy), QStringList({ "d1 /music/track-3.mp3 | Title 3 | Artist 3 | 125.000",
                                           "d1 /music/track-5.mp3 | Title 4 | Artist 5 | 125.000",
                                           "d1 /music/track-6.mp3 | Title 5 | Artist 6 | 125.000",
                                           "d1 /music/track-7.mp3 | Title 6 | Artist 6 | 131.000",
                                           "d1 /music/track-8.mp3 | Title 7 |  | 83.488",
                                           "d2 /music/track-9.mp3 | Title 8 | Artist 7 | 127.999" }));
}

void DjFsm_Test::capture_eject()
{
    DjFsm fsm;
    VdjTelemetryClient client;
    VdjCapture::wire(client, fsm);
    QSignalSpy songSpy(&fsm, &DjFsm::songChanged);
    QSignalSpy updateSpy(&fsm, &DjFsm::songUpdated);
    QVERIFY(VdjCapture::replay(client, "eject") > 0);

    QCOMPARE(songs(songSpy), QStringList({ "d1 /music/track-3.mp3 | Title 3 | Artist 3 | 125.000",
                                           "d2 /music/track-4.mp3 | Title 9 | Artist 4 | 125.000" }));
    // the ejected deck is empty; its placeholder title and bpm 120 reach no song
    QCOMPARE(fsm.deckAt(1).state, DjFsm::DeckState::Empty);
    QVERIFY(!fsm.deckAt(1).song.isValid());
    QCOMPARE(updateSpy.count(), 0);
    QCOMPARE(fsm.deckAt(0).state, DjFsm::DeckState::Paused);
    QCOMPARE(fsm.deckAt(0).song.filepath, QString("/music/track-3.mp3"));
}

void DjFsm_Test::capture_playPauseLoad()
{
    DjFsm fsm;
    VdjTelemetryClient client;
    VdjCapture::wire(client, fsm);
    QSignalSpy songSpy(&fsm, &DjFsm::songChanged);
    int playStarts = 0;
    bool wasPlaying = false;
    connect(&fsm, &DjFsm::deckChanged, &fsm, [&](int deck) {
        const bool playing = fsm.deckAt(deck - 1).playing;
        if (deck == 1 && playing && !wasPlaying)
            playStarts++;
        wasPlaying = playing;
    });
    QVERIFY(VdjCapture::replay(client, "play_pause_load") > 0);

    // the snapshot's song in one write, the manual load in two: complete either way
    QCOMPARE(songs(songSpy), QStringList({ "d1 /music/track-10.mp3 | Title 10 | Artist 8 | 125.000",
                                           "d1 /music/track-11.mp3 | Title 11 | Artist 9 | 126.000" }));
    // playing at the snapshot, then play, pause, play
    QCOMPARE(playStarts, 3);
    QCOMPARE(fsm.deckAt(0).state, DjFsm::DeckState::Playing);
}

void DjFsm_Test::capture_masterHandover()
{
    DjFsm fsm;
    VdjTelemetryClient client;
    VdjCapture::wire(client, fsm);
    QSignalSpy activeSpy(&fsm, &DjFsm::activeDeckChanged);
    QVERIFY(VdjCapture::replay(client, "master_handover") > 0);

    // deck 1 at the snapshot, deck 2 when deck 1's volume drops, deck 1 when deck 2 pauses;
    // the bare masterdeck flag changes in between name nothing
    QList<int> active;
    for (const QList<QVariant> &args : activeSpy)
        active << args[0].toInt();
    QCOMPARE(active, QList<int>({ 1, 2, 1 }));
}

void DjFsm_Test::capture_connectSnapshot()
{
    DjFsm fsm;
    VdjTelemetryClient client;
    VdjCapture::wire(client, fsm);
    QSignalSpy songSpy(&fsm, &DjFsm::songChanged);
    QVERIFY(VdjCapture::replay(client, "connect_snapshot_playing") > 0);

    QCOMPARE(songs(songSpy), QStringList{ "d1 /music/track-12.mp3 | Title 12 | Artist 10 | 120.000" });
    QCOMPARE(fsm.deckAt(0).state, DjFsm::DeckState::Playing);
    QCOMPARE(fsm.deckAt(1).state, DjFsm::DeckState::Empty);
    QCOMPARE(fsm.deckCount(), 2);
    // this capture predates the per-deck master subscription: no active deck
    QCOMPARE(fsm.activeDeck(), 0);
}

void DjFsm_Test::capture_ejectReload_data()
{
    QTest::addColumn<QString>("capture");
    QTest::addColumn<int>("deck");  // 1-based
    QTest::addColumn<QStringList>("announced");
    QTest::addColumn<int>("ejects");
    QTest::addColumn<int>("state");

    const QString a = "d1 /music/track-6.mp3 | Title 5 | Artist 6 | 125.000";
    const QString b = "d1 /music/track-7.mp3 | Title 6 | Artist 6 | 131.000";
    const QString c = "d1 /music/track-5.mp3 | Title 4 | Artist 5 | 125.000";
    const int empty = int(DjFsm::DeckState::Empty), loaded = int(DjFsm::DeckState::Loaded),
              paused = int(DjFsm::DeckState::Paused);
    QTest::newRow("the same file again: announced twice at its bpm")
        << "eject_reload_same_file" << 1 << QStringList({ a, a }) << 1 << paused;
    QTest::newRow("the same artist is not resent after the eject")
        << "eject_load_same_artist" << 1 << QStringList({ a, a, b }) << 2 << loaded;
    QTest::newRow("the same bpm is resent after the eject")
        << "eject_load_same_bpm" << 1 << QStringList({ b, a, c }) << 1 << loaded;
    QTest::newRow("a 120 bpm track after the eject: no get_bpm, 120 is real")
        << "eject_load_bpm120" << 1 << QStringList({ a, "d1 /music/track-13.mp3 | Title 13 | Artist 11 | 120.000" })
        << 2 << empty;
    QTest::newRow("eject while playing: loaded off before play off")
        << "eject_while_playing" << 1 << QStringList({ c, a }) << 1 << empty;
    QTest::newRow("eject paused at zero: no position at all")
        << "eject_paused_at_zero" << 2 << QStringList{ "d2 /music/track-9.mp3 | Title 8 | Artist 7 | 127.999" }
        << 1 << empty;
}

void DjFsm_Test::capture_ejectReload()
{
    QFETCH(QString, capture);
    QFETCH(int, deck);
    QFETCH(QStringList, announced);
    QFETCH(int, ejects);
    QFETCH(int, state);

    DjFsm fsm;
    VdjTelemetryClient client;
    VdjCapture::wire(client, fsm);
    QSignalSpy songSpy(&fsm, &DjFsm::songChanged);
    QSignalSpy updateSpy(&fsm, &DjFsm::songUpdated);
    int emptied = 0;
    bool wasEmpty = true;
    connect(&fsm, &DjFsm::deckChanged, &fsm, [&](int changed) {
        if (changed != deck)
            return;
        const bool empty = fsm.deckAt(deck - 1).state == DjFsm::DeckState::Empty;
        if (empty && !wasEmpty)
            emptied++;
        wasEmpty = empty;
    });
    QVERIFY(VdjCapture::replay(client, capture.toLatin1().constData()) > 0);

    QCOMPARE(songs(songSpy), announced);
    // the eject's placeholder title and bpm 120 reach no song
    QCOMPARE(updateSpy.count(), 0);
    QCOMPARE(emptied, ejects);
    QCOMPARE(int(fsm.deckAt(deck - 1).state), state);
    QVERIFY(!fsm.deckAt(deck - 1).playing);
}

QTEST_MAIN(DjFsm_Test)
