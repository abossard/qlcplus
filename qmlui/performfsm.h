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

/**
 * Finite state machine for VDJ Perform mode (control plane).
 *
 * Follows the DjFsm house style: typed inputs in, synchronous
 * recomputeState(), typed signals out. Pure QObject — no Doc/engine/UI
 * dependencies — so the full transition table is unit-testable.
 *
 * Unidirectional flow: VdjBridge is the single producer of events; the
 * consumers (VdjBridge engine effects, DjManager show-follow, ShowManager
 * read-only, QML status) only read state and never write back.
 *
 * The 25 Hz playback-position stream is deliberately NOT routed through
 * this FSM — that is the data plane; only control decisions live here.
 */
class PerformFsm final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(PerformFsm::PerformState state READ state NOTIFY stateChanged)
    Q_PROPERTY(quint32 activeShowId READ activeShowId NOTIFY activeShowChanged)
    Q_PROPERTY(bool readOnly READ readOnly NOTIFY stateChanged)

public:
    enum class PerformState
    {
        Idle,      //!< Perform off — the Show Manager is editable
        Armed,     //!< Perform on, no show resolved for the active deck yet
        Live,      //!< Show resolved and the VDJ deck is playing
        Suspended, //!< Show resolved, VDJ paused/stopped — playhead frozen
    };
    Q_ENUM(PerformState)

    /** Matches Function::invalidId() without depending on the engine */
    static constexpr quint32 InvalidShowId = 0xFFFFFFFFU;

    explicit PerformFsm(QObject *parent = nullptr);

    PerformState state() const { return m_state; }
    quint32 activeShowId() const { return m_activeShowId; }
    bool enabled() const { return m_enabled; }

    /** The Show Manager is read-only whenever Perform is engaged */
    bool readOnly() const { return m_state != PerformState::Idle; }

    static QString stateToString(PerformState state);

public slots:
    /** The user's Perform toggle */
    void setPerformEnabled(bool on);

    /** The show resolved for the active deck's song (InvalidShowId = none).
     *  Emits activeShowChanged on every change, including handovers that
     *  keep the state (Live -> Live deck switch). */
    void setActiveShow(quint32 showId);

    /** Play/pause state of the active deck */
    void setDeckPlaying(bool playing);

    /** VDJ disconnected: drop show + playing, keep the user's toggle */
    void reset();

signals:
    void stateChanged(PerformFsm::PerformState state);
    void activeShowChanged(quint32 showId);

private:
    void recomputeState();

    bool m_enabled = false;
    bool m_deckPlaying = false;
    quint32 m_activeShowId = InvalidShowId;
    PerformState m_state = PerformState::Idle;
};

#endif // PERFORMFSM_H
