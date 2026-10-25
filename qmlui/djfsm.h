/*
  Q Light Controller Plus
  djfsm.h

  Copyright (c) Massimo Callegari

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#ifndef DJFSM_H
#define DJFSM_H

#include <QObject>
#include <QString>
#include <QVariant>

#include <array>
#include <optional>

/**
 * DjFsm mirrors VirtualDJ's per-deck telemetry.
 *
 * The wire is a delta: VDJ sends each trigger when its value changes, against
 * the value it last sent for that deck, across ejects. Every slot keeps exactly
 * those last-sent values (song fields, loaded, play, master flag, fader,
 * position). A deck is loaded when `loaded` is on, a filepath is known and the
 * title is not the empty deck's placeholder; its song is then the mirror.
 *
 * Song fields (filepath, title, artist, bpm, key, length) of a loaded deck are
 * held until the deck's first write that carries a filepath or a position
 * sample; an empty deck mirrors them at once, and `loaded` on brings the
 * ejected file back unless the write carries a per-file field other than
 * title and bpm: a manual load sends its metadata one write before the
 * filepath, a reload of the ejected file resends only what the eject changed,
 * and VDJ resends per-file fields only when the file changed. Position and
 * fader apply as they arrive. No timers, no debounce: synchronous and testable.
 *
 * The active deck is the lowest tracked deck whose master flag is on,
 * recomputed at every write boundary and when the deck count moves.
 */
class DjFsm : public QObject
{
    Q_OBJECT

public:
    enum class DeckState
    {
        Empty,    //!< No song loaded on the deck
        Loading,  //!< Filepath known, metadata still incomplete
        Loaded,   //!< Required metadata present, not playing
        Playing,  //!< Currently playing
        Paused,   //!< Was playing, now paused
    };
    Q_ENUM(DeckState)

    /** Typed snapshot of the song currently on a deck. */
    struct DeckSong
    {
        QString filepath;
        QString title;
        QString artist;
        QString key;
        double  bpm       = 0.0;
        int     totalMs   = 0;

        bool isValid() const { return !filepath.isEmpty(); }
    };

    /** Typed snapshot of a single deck. */
    struct Deck
    {
        int       number  = 0;                   //!< 1-based deck number
        DeckState state   = DeckState::Empty;
        DeckSong  song;
        bool      playing = false;
        double    volume  = 0.0;                 //!< current fader volume

        // --- Play position (continuous) ---
        double    beatPos = 0.0;                 //!< current beat position (get_beatpos)
        int       elapsedMs = 0;                 //!< elapsed time, ms (get_time elapsed absolute)
        int       remainingMs = 0;               //!< remaining time, ms (get_time)
        bool      positionStale = false;         //!< a new song's first sample is still ahead
    };

    static constexpr int MaxDecks     = 4;
    static constexpr int DefaultDecks = 2;

    explicit DjFsm(QObject *parent = nullptr);

    // --- Topology ---

    /** Number of tracked decks (1..MaxDecks). Defaults to DefaultDecks. */
    int deckCount() const { return m_deckCount; }
    void setDeckCount(int count);

    // --- Event inputs (deck is 1-based) ---

    /** Mirror a per-deck telemetry trigger. Ignored when the deck number
     *  exceeds deckCount, except "masterdeck", kept for every deck so get_decks
     *  finds the master whichever way the count moves. */
    void onDeckTrigger(int deck, const QString &trigger, const QVariant &value);

    /** The objects of one VDJ write are in: every deck settles its view, then the
     *  master, then the write's notifications go out, the master's first. */
    void onTickEnded();

    /** VDJ socket dropped: no active deck first, then every deck empty. */
    void onDisconnected();

    // --- State access ---

    /** Read a deck by 0-based index (0..MaxDecks-1). */
    const Deck &deckAt(int index0) const;

    /** Active/master deck (1-based); 0 if none. */
    int activeDeck() const { return m_activeDeck; }

    /** Song currently on the active deck (invalid if none). */
    DeckSong activeSong() const;

    /** Human-readable name for a deck state. */
    static QString stateName(DeckState s);

signals:
    void deckCountChanged(int count);
    void deckChanged(int deck);
    void positionChanged(int deck);
    /** A new song on a deck, once per filepath, with every field its load brought */
    void songChanged(int deck, const DjFsm::DeckSong &song);
    /** Title, artist or bpm of the announced song changed after it */
    void songUpdated(int deck, const DjFsm::DeckSong &song);
    void activeDeckChanged(int deck);
    /** The active deck's song, title or artist changed */
    void activeSongChanged();

private:
    /** A deck's song fields VDJ sent since their last commit */
    struct Pending
    {
        std::optional<QString> filepath;
        std::optional<QString> title;
        std::optional<QString> artist;
        std::optional<QString> key;
        std::optional<double>  bpm;
        std::optional<int>     totalMs;
    };

    struct Slot
    {
        Deck     deck;                   //!< the view: the song while loaded, transport, position
        DeckSong last;                   //!< VDJ's last-sent song fields
        Pending  pending;
        bool     loaded = false;         //!< VDJ's "loaded"
        bool     play = false;           //!< VDJ's "play"
        bool     master = false;         //!< VDJ's per-deck "masterdeck"
        bool     positionSeen = false;   //!< the write under way carried an elapsed sample
        /** the write under way carried "loaded" on: a per-write fact, since a VU write
         *  between the two halves of a load would otherwise flush the held title onto the old filepath */
        bool     loadedOnSeen = false;
        bool     perFileSeen = false;    //!< the write under way carried a per-file field other than title and bpm
        bool     wasPlaying = false;     //!< distinguishes Paused from Loaded
        int      posEmitSecond = -1;     //!< last elapsed second a positionChanged was emitted for
        int      posEmitBeatQ  = -1000;  //!< last quarter-beat a positionChanged was emitted for
    };

    static bool indexValid(int index0) { return index0 >= 0 && index0 < MaxDecks; }
    /** A deck's view of the write under way: the mirror, the song, transport */
    void settle(Slot &s);
    void recomputeActiveDeck();
    void maybeEmitPosition(Slot &s, int deck1);
    void recomputeActiveSong();

    std::array<Slot, MaxDecks> m_slots;
    int     m_deckCount  = DefaultDecks;
    int     m_activeDeck = 0;       //!< 1-based; 0 = none
    QString m_activeSongKey;        //!< the active song as announced: filepath, title and artist
};

Q_DECLARE_METATYPE(DjFsm::DeckSong)

#endif // DJFSM_H
