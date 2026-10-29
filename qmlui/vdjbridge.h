/*
  Q Light Controller Plus
  vdjbridge.h

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

#ifndef VDJBRIDGE_H
#define VDJBRIDGE_H

#include <QObject>
#include <QPointer>
#include <QSet>
#include <array>
#include <optional>

#include "performfsm.h"

class Doc;
class Show;
class QLCIOPlugin;
class DjFsm;
class ShowFactory;

/**
 * Facade for VirtualDJ integration.
 *
 * Routes the VDJ Bridge plugin's telemetry (DMXDesktop TCP protocol, port
 * 8050) into the DjFsm, the single source of truth for deck/song state, and
 * feeds the PerformFsm every deck as one fact. On every PerformFsm change
 * it executes the planned effects: Show adoption, dominance and transport
 * in the engine, the recorder and the Show Manager through the perform*
 * signals, in handover order. Each driven Show takes its deck's position
 * and mix weight (master intensity) as they arrive.
 */
class VdjBridge final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString telemetryStatus READ telemetryStatus NOTIFY telemetryStatusChanged)
    Q_PROPERTY(bool telemetryConnected READ telemetryConnected NOTIFY telemetryStatusChanged)
    Q_PROPERTY(bool performMode READ performMode WRITE setPerformMode NOTIFY performModeChanged)
    Q_PROPERTY(PerformFsm::PerformState performState READ performState NOTIFY performStatusChanged)
    Q_PROPERTY(QString performShowName READ performShowName NOTIFY performStatusChanged)

public:
    explicit VdjBridge(QObject *parent = nullptr);

    /** Set the Doc pointer for auto-show creation. */
    void setDoc(Doc *doc);

    /**
     * Bind to the VDJ Bridge plugin instance. The facade only knows the
     * plugin via QLCIOPlugin*; the plugin's custom telemetry signals are
     * reached through string-based SIGNAL/SLOT via QMetaObject.
     */
    void attachVdjPlugin(QLCIOPlugin *plugin);

    /** Access the ShowFactory for UI components that observe show creation. */
    ShowFactory *showFactory() const { return m_showFactory; }

    /** Access the 4-deck FSM that DJ Manager observes. */
    DjFsm *djFsm() const { return m_fsm; }

    /**
     * Perform mode. When enabled, every deck's Show is auto-started and kept
     * synchronized with its deck's playback position and mix weight; the
     * active deck's Show is dominant. When disabled (default), VDJ playback
     * never drives Show playback.
     */
    bool performMode() const { return m_performFsm->enabled(); }
    void setPerformMode(bool on);

    /** The Perform control-plane FSM (read-only for consumers). */
    PerformFsm *performFsm() const { return m_performFsm; }
    PerformFsm::PerformState performState() const { return m_performFsm->state(); }

    /** Name of the active deck's Show under Perform, or empty. */
    QString performShowName() const;

    QString telemetryStatus() const;
    bool telemetryConnected() const;

signals:
    void telemetryStatusChanged();
    void performModeChanged();
    void performStatusChanged();

    /** Executor steps outside the engine, emitted in handover order; all
     *  of them follow the active deck's Show */
    void performShowResolved(quint32 showId); //!< the recorder follows Perform to showId
    void performReleased();                    //!< Perform disengaged: the recorder's selection resolves again
    void performShowSelected(int showId);      //!< the Show Manager displays showId

private slots:
    void onDeckTrigger(int deckIndex, const QString &trigger, const QVariant &value);
    void onGlobalTrigger(const QString &trigger, const QVariant &value);
    /** One VDJ write is in: the decks commit it, every driven Show takes its deck's position */
    void onTickEnded();
    void onTelemetryClientDisconnected();

private:
    void pushActiveBpm();

    // --- Perform: every deck as the FSM's input, the plan as effects ---
    void refreshPerformInputs();
    /** A deck's (1-based) position; none while it holds no song */
    std::optional<quint32> deckPosition(int deck) const;
    void pushPosition(Show *show, int deck) const;
    /** The deck's mix weight as the Show's master intensity */
    void pushWeight(Show *show, int deck) const;
    void applyPerformPlan();
    void perform(const PerformFsm::Step &step);
    /** The drive of showId the executor last applied, if any */
    const PerformFsm::Drive *appliedDrive(quint32 showId) const;
    Show *lookupShow(quint32 showId) const;

    Doc *m_doc = nullptr;
    DjFsm *m_fsm = nullptr;
    ShowFactory *m_showFactory = nullptr;
    PerformFsm *m_performFsm = nullptr;

    /** What the executor last applied: the planner's previous state */
    PerformFsm::Snapshot m_applied;
    /** A step that plans again waits for the plan under way */
    bool m_planning = false;
    bool m_replanPending = false;
    /** A disconnect empties every deck: the FSM takes it as one fact, the tempo is no deck's */
    bool m_holdInputs = false;
    /** Per deck: the position at which its Show's runner ended; a move
     *  below it plays the Show again */
    struct Ended
    {
        quint32 showId;
        quint32 positionMs;
    };
    std::array<std::optional<Ended>, PerformFsm::MaxDecks> m_ended;
    /** Shows that moved to another deck once their run was over: a finish
     *  still queued is the old deck's, until their next run starts */
    QSet<quint32> m_finishedElsewhere;
    /** The last tempo VirtualDJ gave the leading song */
    int m_lastSongBpm = 0;

    QPointer<QLCIOPlugin> m_vdjPlugin;
};

#endif // VDJBRIDGE_H
