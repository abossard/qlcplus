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
#include <QVariantMap>

class QLCIOPlugin;

/**
 * Qt-facing facade for the OS2L plugin's structured event stream.
 *
 * The OS2L plugin (a shared library loaded via IOPluginCache) emits
 * songReceived(QVariantMap) and beatInfoReceived(double,double,bool) signals.
 * VdjBridge is the qmlui-side QObject that consumes those signals and exposes
 * the live VDJ state (current song, BPM, beat position, connection status)
 * as Q_PROPERTYs that QML can bind to and that the Song Manager can react to.
 *
 * Connections from the plugin are made by App using Qt's string-based
 * signal/slot syntax so qmlui does not need to link against the plugin .so.
 *
 * The bridge is intentionally agnostic about which VDJ-side bridge produced
 * the events: a second backend (e.g. a future reverse-engineered DMXDesktop
 * protocol) can feed the same slots without any change to consumers.
 */
class VdjBridge final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(double bpm READ bpm NOTIFY beatChanged)
    Q_PROPERTY(double beatPos READ beatPos NOTIFY beatChanged)
    Q_PROPERTY(int beatCount READ beatCount NOTIFY beatChanged)
    Q_PROPERTY(QVariantMap currentSong READ currentSong NOTIFY songChanged)
    Q_PROPERTY(QString currentSongName READ currentSongName NOTIFY songChanged)
    Q_PROPERTY(QString currentArtist READ currentArtist NOTIFY songChanged)
    Q_PROPERTY(QString currentSongPath READ currentSongPath NOTIFY songChanged)
    Q_PROPERTY(double currentBpm READ currentBpm NOTIFY songChanged)
    Q_PROPERTY(double currentDuration READ currentDuration NOTIFY songChanged)
    Q_PROPERTY(double currentElapsed READ currentElapsed NOTIFY songElapsedChanged)
    Q_PROPERTY(QString currentStatus READ currentStatus NOTIFY songChanged)

public:
    explicit VdjBridge(QObject *parent = nullptr);

    /**
     * Bind to an OS2L plugin instance and start consuming its events.
     * The plugin pointer may be null (no OS2L plugin available); the bridge
     * stays in a disconnected state. Subsequent calls replace any prior binding.
     */
    void attachOS2LPlugin(QLCIOPlugin *plugin);

    bool connected() const { return m_connected; }
    double bpm() const { return m_bpm; }
    double beatPos() const { return m_beatPos; }
    int beatCount() const { return m_beatCount; }
    QVariantMap currentSong() const { return m_currentSong; }
    QString currentSongName() const { return m_currentSong.value("name").toString(); }
    QString currentArtist() const { return m_currentSong.value("artist").toString(); }
    QString currentSongPath() const { return m_currentSong.value("path").toString(); }
    double currentBpm() const { return m_currentSong.value("bpm").toDouble(); }
    double currentDuration() const { return m_currentSong.value("duration").toDouble(); }
    double currentElapsed() const { return m_currentSong.value("elapsed").toDouble(); }
    QString currentStatus() const { return m_currentSong.value("status").toString(); }

public slots:
    /** Connected to OS2LPlugin::songReceived (string-based). */
    void onSongReceived(QVariantMap song);

    /** Connected to OS2LPlugin::beatInfoReceived (string-based). */
    void onBeatInfo(double bpm, double pos, bool change);

    /** Connected to QLCIOPlugin::connectionStatusChanged.
     *  Re-queries the plugin and updates the connected property. */
    void refreshConnectionStatus();

signals:
    void connectedChanged();
    void beatChanged();

    /**
     * Emitted on every received song event. The Song Manager listens to this
     * to look up or create a Show for the track.
     */
    void songChanged();

    /** Separate from songChanged because elapsed often advances without a full
     *  song-event re-broadcast — avoids re-running show creation logic. */
    void songElapsedChanged();

private:
    /** Held as a base-class pointer so qmlui does not need to link the
     *  OS2L plugin's shared library. Becomes null if the plugin is unloaded. */
    QPointer<QLCIOPlugin> m_plugin;

    bool m_connected = false;
    double m_bpm = 0.0;
    double m_beatPos = 0.0;
    int m_beatCount = 0;
    QVariantMap m_currentSong;
};

#endif // VDJBRIDGE_H
