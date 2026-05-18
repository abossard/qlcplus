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

class Doc;
class QLCIOPlugin;
class VdjDeckModel;
class VdjTelemetryClient;
class VdjBonjour;
class SongLoadTracker;
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
 * Exposes 4 VdjDeckModel instances (one per VDJ deck) plus global mixer
 * state to QML.
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
    Q_PROPERTY(QList<QObject*> decks READ decks CONSTANT)
    Q_PROPERTY(int masterDeck READ masterDeck NOTIFY masterDeckChanged)
    Q_PROPERTY(qreal masterVolume READ masterVolume NOTIFY globalMixerChanged)
    Q_PROPERTY(qreal crossfader READ crossfader NOTIFY globalMixerChanged)
    Q_PROPERTY(qreal headphoneVolume READ headphoneVolume NOTIFY globalMixerChanged)
    Q_PROPERTY(qreal masterVu READ masterVu NOTIFY globalMixerChanged)

public:
    explicit VdjBridge(QObject *parent = nullptr);

    /** Set the Doc pointer for auto-show creation. */
    void setDoc(Doc *doc);

    /**
     * Bind to an OS2L plugin instance and start consuming its events.
     */
    void attachOS2LPlugin(QLCIOPlugin *plugin);

    /**
     * Start the DMXDesktop telemetry TCP server.
     * @param port TCP port (default 8050, 0 = disabled).
     */
    void startTelemetry(quint16 port = 8050);

    /** Stop the telemetry server and Bonjour registration. */
    void stopTelemetry();

    /** Access the ShowFactory for UI components that observe show creation. */
    ShowFactory *showFactory() const { return m_showFactory; }

    // --- Legacy getters ---
    bool connected() const { return m_connected; }
    int beatCount() const { return m_beatCount; }

    // --- Telemetry getters ---
    QString telemetryStatus() const;
    bool telemetryConnected() const;
    QList<QObject*> decks() const;
    int masterDeck() const { return m_masterDeck; }
    qreal masterVolume() const { return m_masterVolume; }
    qreal crossfader() const { return m_crossfader; }
    qreal headphoneVolume() const { return m_headphoneVolume; }
    qreal masterVu() const { return m_masterVu; }

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

private slots:
    void onTelemetryBeat(int pos, qreal bpm, qreal strength, bool change);
    void onDeckTrigger(int deckIndex, const QString &trigger, const QVariant &value);
    void onGlobalTrigger(const QString &trigger, const QVariant &value);
    void onTelemetryClientConnected();
    void onTelemetryClientDisconnected();

private:
    void applyDeckTrigger(VdjDeckModel *deck, int deckIndex,
                          const QString &trigger, const QVariant &value);

    // Doc (for auto-show creation via ShowFactory)
    Doc *m_doc = nullptr;
    SongLoadTracker *m_tracker = nullptr;
    ShowFactory *m_showFactory = nullptr;

    // OS2L
    QPointer<QLCIOPlugin> m_plugin;
    bool m_connected = false;
    int m_beatCount = 0;

    // Telemetry
    VdjTelemetryClient *m_telemetry = nullptr;
    VdjBonjour *m_bonjour = nullptr;
    quint16 m_telemetryPort = 0;
    VdjDeckModel *m_deckModels[4] = {};
    int m_masterDeck = 0;
    qreal m_masterVolume = 0.0;
    qreal m_crossfader = 0.0;
    qreal m_headphoneVolume = 0.0;
    qreal m_masterVu = 0.0;
};

#endif // VDJBRIDGE_H
