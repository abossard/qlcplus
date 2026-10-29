/*
  Q Light Controller Plus
  performfsm.h

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

#ifndef PERFORMFSM_H
#define PERFORMFSM_H

#include <QObject>
#include <QHash>
#include <QList>

#include <array>

/**
 * Finite state machine for VDJ Perform mode (control plane).
 *
 * Two inputs: the user's Perform toggle and every deck, taken as one fact
 * (each deck's Show, its transport, whether that Show's runner ended where
 * the deck still is, and which deck VirtualDJ makes active). Every input
 * change recomputes once and emits stateChanged() at most once, so consumers
 * never see a half-applied deck.
 *
 * Every deck whose song has a Show drives it from its own clock; the active
 * deck's Show is dominant (it replays its command track, wins LTP, holds REC
 * and the Show Manager). A Show runs once: the active deck drives a Show two
 * decks hold, else the lower deck.
 *
 * plan() is the pure planner: the effects that take the engine, the
 * recorder and the Show Manager from one snapshot to the next, in handover
 * order. VdjBridge is the executor.
 *
 * The 25 Hz playback-position stream and the decks' mix weights are
 * deliberately NOT routed through this FSM: that is the data plane; only
 * control decisions live here.
 */
class PerformFsm final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(PerformFsm::PerformState state READ state NOTIFY stateChanged)
    Q_PROPERTY(quint32 activeShowId READ activeShowId NOTIFY stateChanged)
    Q_PROPERTY(bool readOnly READ readOnly NOTIFY stateChanged)

public:
    enum class PerformState
    {
        Idle,      //!< Perform off: the Show Manager is editable
        Armed,     //!< Perform on, no Show resolved for the deck
        Live,      //!< Show resolved, the deck plays
        Suspended, //!< Show resolved, the deck is paused: playhead frozen
        Ended,     //!< Show resolved, the deck plays on past where its runner ended: dark
    };
    Q_ENUM(PerformState)

    /** Matches Function::invalidId() without depending on the engine */
    static constexpr quint32 InvalidShowId = 0xFFFFFFFFU;
    /** Matches DjFsm::MaxDecks without depending on it */
    static constexpr int MaxDecks = 4;

    /** One deck as one fact. No Show for its song: the defaults. */
    struct DeckInputs
    {
        quint32 showId = InvalidShowId; //!< the Show resolved for the deck's song
        bool playing = false;
        bool ended = false;             //!< the Show's runner ended and the deck has not moved back since

        bool operator==(const DeckInputs &o) const
        {
            return showId == o.showId && playing == o.playing && ended == o.ended;
        }
    };

    /** Every deck and the active one */
    struct Inputs
    {
        int activeDeck = 0;                     //!< 1-based, 0: none
        std::array<DeckInputs, MaxDecks> decks; //!< deck 1 first

        bool operator==(const Inputs &o) const { return activeDeck == o.activeDeck && decks == o.decks; }
    };

    /** A Show Perform drives, from one deck's clock */
    struct Drive
    {
        quint32 showId = InvalidShowId;
        int deck = 0;                            //!< 1-based: its position and its weight
        PerformState state = PerformState::Live; //!< Live, Suspended or Ended
        bool dominant = false;                   //!< the active deck's

        bool operator==(const Drive &o) const
        {
            return showId == o.showId && deck == o.deck && state == o.state && dominant == o.dominant;
        }
    };

    /** What Perform drives: the planner's state */
    struct Snapshot
    {
        bool engaged = false;  //!< Perform on
        QList<Drive> drives;   //!< at most one per Show, in deck order

        bool operator==(const Snapshot &o) const { return engaged == o.engaged && drives == o.drives; }
    };

    /** What the executor does on a transition, in this order */
    enum class Effect
    {
        ResolveRecorder, //!< the recorder follows Perform to showId (InvalidShowId: armed under Perform)
        ReleaseRecorder, //!< Perform disengaged: the recorder's selection resolves again
        StopShow,        //!< Function stop, adoption kept (a start not run yet, or a Show past its content)
        ReleaseShow,     //!< Autonomous sync, Audio audible, master intensity 1, dominant, no longer adopted
        DemoteShow,      //!< no longer the active deck's: no command replay, loses LTP
        FollowDeck,      //!< another deck drives the Show: its position and weight from there
        PromoteShow,     //!< the active deck's: its command track's state comes back, wins LTP
        AdoptShow,       //!< External sync, Audio suppressed, its deck's position and weight, its dominance
        StartShow,       //!< start at the deck position
        ResumeShow,      //!< a paused run goes on
        PauseShow,
        SelectShow,      //!< the Show Manager displays showId and re-reads its transport
    };
    Q_ENUM(Effect)

    struct Step
    {
        Effect effect;
        quint32 showId = InvalidShowId;

        bool operator==(const Step &o) const { return effect == o.effect && showId == o.showId; }
    };

    /** Runtime facts of a driven Show, as the engine reports them */
    struct ShowFacts
    {
        bool running = false;
        bool paused = false;
        bool startQueued = false; //!< started and not cancelled, its first tick still ahead
    };

    explicit PerformFsm(QObject *parent = nullptr);

    /** The active deck's: Idle while Perform is off, Armed without its Show */
    PerformState state() const;

    /** The active deck's Show: InvalidShowId while Idle or Armed */
    quint32 activeShowId() const;
    bool enabled() const { return m_enabled; }

    /** Every Show Perform drives; no drive while Idle */
    Snapshot snapshot() const { return m_snapshot; }

    /** The Show Manager is read-only whenever Perform is engaged */
    bool readOnly() const { return m_snapshot.engaged; }

    static QString stateToString(PerformState state);

    /** The ordered effects from prev to next, the next drives' Shows having
     *  facts (none: stopped). Pure: a table, tested as one. */
    static QList<Step> plan(const Snapshot &prev, const Snapshot &next,
                            const QHash<quint32, ShowFacts> &facts);

public slots:
    /** The user's Perform toggle */
    void setPerformEnabled(bool on);

    /** Any deck changed in any way: one recompute */
    void setInputs(const PerformFsm::Inputs &inputs);

signals:
    /** snapshot() changed, and with it state() or activeShowId(): once per real change */
    void stateChanged();

private:
    void recompute();

    bool m_enabled = false;
    Inputs m_inputs;
    Snapshot m_snapshot;
};

#endif // PERFORMFSM_H
