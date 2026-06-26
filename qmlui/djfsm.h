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

/**
 * Typesafe multi-deck finite state machine for VirtualDJ integration.
 *
 * Telemetry fields arrive as a tight burst of separate triggers whose order is
 * unreliable: VirtualDJ sends a deck's metadata (title/artist/bpm) sometimes
 * BEFORE its get_filepath and sometimes AFTER. The FSM updates each deck's song
 * fields *live* (so the deck shows the song name the instant get_title arrives)
 * and uses the title as the freshness signal to decide, when a new filepath
 * arrives, whether buffered metadata belongs to the new song (keep it,
 * metadata-first order) or is stale from the previous song (clear it,
 * filepath-first order). A song is announced exactly once per filepath, when
 * filepath + title + artist + bpm are all present and consistent. No timers,
 * no debounce — synchronous and unit-testable.
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
        QString album;
        QString genre;
        QString key;
        double  bpm       = 0.0;
        double  firstBeat = 0.0;
        int     totalMs   = 0;

        bool isValid() const { return !filepath.isEmpty(); }
    };

    /** Typed snapshot of a single deck. */
    struct Deck
    {
        int       number  = 0;                   //!< 1-based deck number
        DeckState state   = DeckState::Empty;
        DeckSong  song;
        bool      loaded  = false;
        bool      playing = false;
        double    volume  = 0.0;                 //!< current fader volume
        double    level   = 0.0;                 //!< current output level

        // --- Play position (continuous) ---
        double    beatPos = 0.0;                 //!< current beat position (get_beatpos)
        int       elapsedMs = 0;                 //!< elapsed time, ms (get_time elapsed absolute)
        int       remainingMs = 0;               //!< remaining time, ms (get_time)
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
     *  number exceeds deckCount. */
    void onDeckTrigger(int deck, const QString &trigger, const QVariant &value);

    /** Set the active/master deck (1-based). Ignored when beyond deckCount. */
    void onMasterDeck(int deck);

    /** VDJ socket dropped — reset every deck and active state. */
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
    void songChanged(int deck, const DjFsm::DeckSong &song);
    void activeDeckChanged(int deck);
    void activeSongChanged();

private:
    enum FieldBit : quint16
    {
        F_Filepath = 1 << 0,
        F_Title    = 1 << 1,
        F_Artist   = 1 << 2,
        F_Bpm      = 1 << 3,
    };
    static constexpr quint16 RequiredMeta = F_Filepath | F_Title | F_Artist | F_Bpm;

    struct Slot
    {
        Deck     deck;
        quint16  gotMask = 0;            //!< fields present for the current song
        quint16  freshMask = 0;          //!< fields set since the last commit
        QString  committedFilepath;     //!< last announced filepath
        bool     wasPlaying = false;    //!< distinguishes Paused from Loaded
        bool     rawPlaying = false;    //!< last raw play trigger (pre-validity gating)
        int      posEmitSecond = -1;    //!< last elapsed second a positionChanged was emitted for
        int      posEmitBeatQ  = -1000; //!< last quarter-beat a positionChanged was emitted for
    };

    static bool indexValid(int index0) { return index0 >= 0 && index0 < MaxDecks; }
    bool deckTracked(int deck1) const { return deck1 >= 1 && deck1 <= m_deckCount; }
    Slot &slotForDeck(int deck);     // 1-based; clamps invalid input to deck 1
    void resetSlot(Slot &s, int deck1);
    void clearStaleMetadata(Slot &s);
    void recomputeState(Slot &s);
    static bool isPlaceholderTitle(const QString &title);
    void maybeAnnounceSong(Slot &s);
    void maybeEmitPosition(Slot &s, int deck1);
    void recomputeActiveSong();

    std::array<Slot, MaxDecks> m_slots;
    int     m_deckCount  = DefaultDecks;
    int     m_activeDeck = 0;       //!< 1-based; 0 = none
    QString m_activeFilepath;      //!< tracks active song identity
};

Q_DECLARE_METATYPE(DjFsm::DeckSong)

#endif // DJFSM_H
