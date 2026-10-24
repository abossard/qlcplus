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
 * Typesafe multi-deck finite state machine for VirtualDJ integration.
 *
 * VirtualDJ sends a trigger only when its value changes, one write per tick,
 * so the last title, artist, bpm or key seen IS the current song's until a
 * change arrives, and an empty artist is a value. A load is one write
 * (Automix: filepath and metadata together) or two (manual: metadata first,
 * filepath 33 to 167 ms later). The FSM buffers a deck's song fields and
 * commits them at the write boundary (onTickEnded): a write with a new
 * filepath commits a load, announced as one complete snapshot (songChanged)
 * once filepath and title are present; song fields without a filepath are
 * held one more write, so a load's first write waits for its filepath, and
 * flush as songUpdated when the deck's next write brings none. The wire is
 * a delta against VDJ's last-sent value per trigger, across ejects: an
 * eject (loaded off, the placeholder title) empties the deck at once but
 * keeps every last value, so a reload of the same file (title, bpm, loaded
 * on, no filepath) or a next song that shares a field is announced
 * complete. Transport (play, master flag) and position apply at once. No
 * timers, no debounce: synchronous and unit-testable.
 *
 * Deck transport state:
 *   Empty   — no song. A song-less deck is never Playing/Paused even if VDJ
 *             broadcasts a play trigger for it (filters phantom/mirror decks).
 *   Loading — filepath known, required metadata still incomplete
 *   Loaded  — required metadata present, not playing
 *   Playing — playing
 *   Paused  — was playing, now paused
 *
 * VirtualDJ can advertise more deck slots than there are real decks; the FSM
 * tracks a configurable number of decks (deckCount, default 2) and ignores
 * triggers for decks beyond the count.
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

    /** Route a per-deck telemetry trigger into the FSM. Ignored when the deck
     *  number exceeds deckCount, except "masterdeck", kept for every deck so
     *  get_decks finds the master whichever way the count moves. Song fields
     *  wait for onTickEnded(); "loaded" off and the placeholder title empty
     *  the deck at once (VDJ sends no empty filepath on eject) and keep VDJ's
     *  last values for the next load, the fader is no song field. */
    void onDeckTrigger(int deck, const QString &trigger, const QVariant &value);

    /** The objects of one VDJ write are in: every deck commits its song fields. */
    void onTickEnded();

    /** Set the active/master deck (1-based). Ignored when beyond deckCount. */
    void onMasterDeck(int deck);

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
    /** A deck's song fields of the write under way, or held from the previous one */
    struct Pending
    {
        std::optional<QString> filepath;
        std::optional<QString> title;
        std::optional<QString> artist;
        std::optional<QString> key;
        std::optional<double>  bpm;
        std::optional<int>     totalMs;
        bool fresh = false;              //!< something arrived in the write under way

        bool any() const { return filepath || title || artist || key || bpm || totalMs; }
    };

    struct Slot
    {
        Deck     deck;
        DeckSong last;                  //!< VDJ's last-sent song fields, kept across ejects
        Pending  pending;
        bool     touched = false;       //!< any trigger of the deck in the write under way
        bool     positionSeen = false;  //!< an elapsed sample in the write under way
        bool     ejectedThisWrite = false;
        bool     loadedOn = false;      //!< "loaded" on arrived since the deck emptied
        bool     ejected = false;       //!< an eject since the last load: bpmBeforeEject applies
        double   bpmBeforeEject = 0.0;  //!< the eject's bpm 120 is an artefact, not this
        QString  titleBeforeEject;
        QString  committedFilepath;     //!< last announced filepath
        bool     master = false;        //!< VDJ's per-deck masterdeck flag
        bool     wasPlaying = false;    //!< distinguishes Paused from Loaded
        bool     rawPlaying = false;    //!< last raw play trigger (pre-validity gating)
        int      posEmitSecond = -1;    //!< last elapsed second a positionChanged was emitted for
        int      posEmitBeatQ  = -1000; //!< last quarter-beat a positionChanged was emitted for
    };

    static bool indexValid(int index0) { return index0 >= 0 && index0 < MaxDecks; }
    bool deckTracked(int deck1) const { return deck1 >= 1 && deck1 <= m_deckCount; }
    Slot &slotForDeck(int deck);     // 1-based; clamps invalid input to deck 1
    void resetSlot(Slot &s, int deck1);
    void ejectDeck(Slot &s, int deck1);
    void emptyDeck(Slot &s);
    void followMasterFlags();
    void recomputeState(Slot &s);
    static bool isPlaceholderTitle(const QString &title);
    bool reloadPending(const Slot &s) const;
    void commitPending(Slot &s, int deck1);
    void maybeEmitPosition(Slot &s, int deck1);
    void recomputeActiveSong();

    std::array<Slot, MaxDecks> m_slots;
    int     m_deckCount  = DefaultDecks;
    int     m_activeDeck = 0;       //!< 1-based; 0 = none
    QString m_activeSongKey;        //!< the active song as announced: filepath, title and artist
};

Q_DECLARE_METATYPE(DjFsm::DeckSong)

#endif // DJFSM_H
