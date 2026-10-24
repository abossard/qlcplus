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
#include <QList>

/**
 * Finite state machine for VDJ Perform mode (control plane).
 *
 * Two inputs: the user's Perform toggle and the active deck, taken as one
 * fact (its Show, its transport, whether its position is past that Show's
 * content). Every input change recomputes once and emits stateChanged()
 * at most once, so consumers never see a half-applied deck.
 *
 * plan() is the pure planner: the effects that take the engine, the
 * recorder and the Show Manager from one state to the next, in handover
 * order. VdjBridge is the executor.
 *
 * The 25 Hz playback-position stream is deliberately NOT routed through
 * this FSM: that is the data plane; only control decisions live here.
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
        Armed,     //!< Perform on, no Show resolved for the active deck
        Live,      //!< Show resolved, the deck plays inside its content
        Suspended, //!< Show resolved, the deck is paused: playhead frozen
        Ended,     //!< Show resolved, the deck plays past its content: dark
    };
    Q_ENUM(PerformState)

    /** Matches Function::invalidId() without depending on the engine */
    static constexpr quint32 InvalidShowId = 0xFFFFFFFFU;

    /** The active deck as one fact. No deck: the defaults. */
    struct Inputs
    {
        quint32 showId = InvalidShowId; //!< the Show resolved for the deck's song
        bool playing = false;
        bool pastEnd = false;           //!< deck position at or past the Show's content

        bool operator==(const Inputs &o) const
        {
            return showId == o.showId && playing == o.playing && pastEnd == o.pastEnd;
        }
    };

    /** What the executor does on a transition, in this order */
    enum class Effect
    {
        ResolveRecorder, //!< the recorder follows Perform to showId (InvalidShowId: armed under Perform)
        ReleaseRecorder, //!< Perform disengaged: the recorder's selection resolves again
        StopShow,        //!< Function stop, adoption kept (a start not run yet, or a Show past its content)
        ReleaseShow,     //!< Autonomous sync, Audio audible again, no longer adopted
        AdoptShow,       //!< External sync, Audio suppressed, the deck position pushed
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

    /** Runtime facts of the next state's Show, as the engine reports them */
    struct ShowFacts
    {
        bool running = false;
        bool paused = false;
        bool startQueued = false; //!< started and not cancelled, its first tick still ahead
    };

    explicit PerformFsm(QObject *parent = nullptr);

    PerformState state() const { return m_state; }

    /** The Show Perform drives: InvalidShowId while Idle or Armed */
    quint32 activeShowId() const;
    bool enabled() const { return m_enabled; }

    /** The Show Manager is read-only whenever Perform is engaged */
    bool readOnly() const { return m_state != PerformState::Idle; }

    static QString stateToString(PerformState state);

    /** The ordered effects from (prevState, prevShow) to (state, show), the
     *  latter's Show having facts. Pure: a table, tested as one. */
    static QList<Step> plan(PerformState prevState, quint32 prevShow,
                            PerformState state, quint32 show, const ShowFacts &facts);

public slots:
    /** The user's Perform toggle */
    void setPerformEnabled(bool on);

    /** The active deck changed in any way: one recompute */
    void setInputs(const PerformFsm::Inputs &inputs);

signals:
    /** state() or activeShowId() changed, once per real change */
    void stateChanged();

private:
    void recomputeState(PerformState prevState, quint32 prevShow);

    bool m_enabled = false;
    Inputs m_inputs;
    PerformState m_state = PerformState::Idle;
};

#endif // PERFORMFSM_H
