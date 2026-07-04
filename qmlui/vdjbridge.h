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
#include <QList>
#include <QSet>

#include "performfsm.h"

class Doc;
class Show;
class QLCIOPlugin;
class DjFsm;
class ShowFactory;

/**
 * Facade for VirtualDJ integration.
 *
 * Fans in two independent data sources:
 *  1. **OS2L plugin** — bare beat events via the OS2L UDP/TCP protocol.
 *  2. **Telemetry client** — rich per-deck metadata + beat events via the
 *     DMXDesktop TCP protocol (port 8050).
 *
 * When the telemetry client is connected, beats from OS2L are suppressed
 * to avoid double-counting (telemetry also delivers beat events with BPM).
 *
 * All per-deck telemetry is routed into the DjFsm, which is the single source
 * of truth for deck/song state; the bridge itself keeps only global mixer
 * state and drives Perform-mode Show playback off the FSM.
 */
class VdjBridge final : public QObject
{
    Q_OBJECT

    // --- Legacy OS2L properties (kept for backwards compat) ---
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(int beatCount READ beatCount NOTIFY beatReceived)

    // --- Telemetry properties ---
    Q_PROPERTY(QString telemetryStatus READ telemetryStatus NOTIFY telemetryStatusChanged)
    Q_PROPERTY(bool telemetryConnected READ telemetryConnected NOTIFY telemetryStatusChanged)
    Q_PROPERTY(int masterDeck READ masterDeck NOTIFY masterDeckChanged)
    Q_PROPERTY(qreal masterVolume READ masterVolume NOTIFY globalMixerChanged)
    Q_PROPERTY(qreal crossfader READ crossfader NOTIFY globalMixerChanged)
    Q_PROPERTY(qreal headphoneVolume READ headphoneVolume NOTIFY globalMixerChanged)
    Q_PROPERTY(qreal masterVu READ masterVu NOTIFY globalMixerChanged)
    Q_PROPERTY(bool performMode READ performMode WRITE setPerformMode NOTIFY performModeChanged)
    Q_PROPERTY(PerformFsm::PerformState performState READ performState NOTIFY performStatusChanged)
    Q_PROPERTY(QString performShowName READ performShowName NOTIFY performStatusChanged)

public:
    explicit VdjBridge(QObject *parent = nullptr);

    /** Set the Doc pointer for auto-show creation. */
    void setDoc(Doc *doc);

    /**
     * Bind to an OS2L plugin instance and start consuming its events.
     */
    void attachOS2LPlugin(QLCIOPlugin *plugin);

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
     * Perform mode. When enabled, the active deck's Show is auto-started and
     * kept synchronized with the deck's playback position. When disabled
     * (default), VDJ playback never drives Show playback.
     *
     * All control decisions flow through the PerformFsm (single source of
     * truth); this bridge feeds its events and executes engine effects on
     * its transitions.
     */
    bool performMode() const { return m_performFsm->enabled(); }
    void setPerformMode(bool on);

    /** The Perform control-plane FSM (read-only for consumers). */
    PerformFsm *performFsm() const { return m_performFsm; }
    PerformFsm::PerformState performState() const { return m_performFsm->state(); }

    /** Name of the show currently resolved for Perform, or empty. */
    QString performShowName() const;

    // --- Legacy getters ---
    bool connected() const { return m_connected; }
    int beatCount() const { return m_beatCount; }

    // --- Telemetry getters ---
    QString telemetryStatus() const;
    bool telemetryConnected() const;
    int masterDeck() const { return m_masterDeck; }
    qreal masterVolume() const { return m_masterVolume; }
    qreal crossfader() const { return m_crossfader; }
    qreal headphoneVolume() const { return m_headphoneVolume; }
    qreal masterVu() const { return m_masterVu; }

    /** BPM pushed to the engine while the active deck is paused (visual "drop
     *  super low" cue). Defaults to 1; configurable for the future. */
    int pausedBpm() const { return m_pausedBpm; }
    void setPausedBpm(int bpm);

public slots:
    /** Connected to OS2LPlugin::beatReceived (string-based). */
    void onBeat();

    /** Connected to QLCIOPlugin::connectionStatusChanged. */
    void refreshConnectionStatus();

signals:
    void connectedChanged();
    void beatReceived();
    void telemetryStatusChanged();
    void masterDeckChanged();
    void globalMixerChanged();
    void performModeChanged();
    void performStatusChanged();

private slots:
    void onTelemetryBeat(int pos, qreal bpm, qreal strength, bool change);
    void onDeckTrigger(int deckIndex, const QString &trigger, const QVariant &value);
    void onGlobalTrigger(const QString &trigger, const QVariant &value);
    void onTelemetryClientConnected();
    void onTelemetryClientDisconnected();

private:
    void applyDeckTrigger(int deckIndex, const QString &trigger, const QVariant &value);
    void driveActiveShow(int deckIndex, const QString &trigger, const QVariant &value);
    void pushActiveBpm();

    // --- Perform: FSM event feeding + transition-driven engine effects ---

    /** Re-resolve the FSM inputs (active deck's show + playing state) */
    void refreshPerformInputs();

    /** Engine effects on FSM transitions */
    void applyPerformState(PerformFsm::PerformState state);
    void applyPerformShowChange(quint32 showId);

    /** Sync-source adoption: while Perform drives a show it runs with
     *  External sync; the previous source is restored on release so
     *  manual playback keeps working (nothing is persisted). */
    void adoptActiveShow();
    void releaseAdoptedShow();
    void startAdoptedShow();
    void pauseAdoptedShow();
    Show *lookupShow(quint32 showId) const;

    // Doc (for auto-show creation via ShowFactory)
    Doc *m_doc = nullptr;
    DjFsm *m_fsm = nullptr;
    ShowFactory *m_showFactory = nullptr;
    PerformFsm *m_performFsm = nullptr;

    /** Show currently adopted by Perform (engine-effect state) */
    quint32 m_adoptedShowId = PerformFsm::InvalidShowId;
    int m_adoptedPrevSyncSource = 0;

    // OS2L
    QPointer<QLCIOPlugin> m_plugin;
    bool m_connected = false;
    int m_beatCount = 0;

    // Telemetry (the DjFsm holds all per-deck state; the bridge keeps only
    // global mixer state).
    QPointer<QLCIOPlugin> m_vdjPlugin;
    int m_masterDeck = -1;          //!< 0-based active deck; -1 = none yet
    qreal m_masterVolume = 0.0;
    qreal m_crossfader = 0.0;
    qreal m_headphoneVolume = 0.0;
    qreal m_masterVu = 0.0;

    int m_pausedBpm = 1;            //!< engine BPM while the active deck is paused
};

#endif // VDJBRIDGE_H
