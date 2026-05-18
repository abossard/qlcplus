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

class QLCIOPlugin;

/**
 * Qt-facing facade for the OS2L plugin's beat event stream.
 *
 * The OS2L plugin (a shared library loaded via IOPluginCache) emits
 * beatInfoReceived(double,double,bool) for every received OS2L beat
 * event. VdjBridge is the qmlui-side QObject that consumes those signals
 * and exposes the live VDJ beat / BPM / connection state as Q_PROPERTYs
 * that QML can bind to.
 *
 * The bridge currently covers ONLY what stock VirtualDJ broadcasts over
 * OS2L without any user-side scripting: continuous `evt:beat` messages
 * with optional bpm/pos/change fields. Song-event handling and any
 * file-path / waveform features are deliberately out of scope until a
 * second backend (e.g. a reverse-engineered DMXDesktop protocol) can
 * actually supply that data — see
 * docs/VDJ_DMXDESKTOP_REVERSE_ENGINEERING_PROMPT.md.
 *
 * Connections from the plugin are made by App using Qt's string-based
 * signal/slot syntax so qmlui does not need to link against the plugin .so.
 */
class VdjBridge final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(double bpm READ bpm NOTIFY beatChanged)
    Q_PROPERTY(double beatPos READ beatPos NOTIFY beatChanged)
    Q_PROPERTY(int beatCount READ beatCount NOTIFY beatChanged)

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

public slots:
    /** Connected to OS2LPlugin::beatInfoReceived (string-based). */
    void onBeatInfo(double bpm, double pos, bool change);

    /** Connected to QLCIOPlugin::connectionStatusChanged.
     *  Re-queries the plugin and updates the connected property. */
    void refreshConnectionStatus();

signals:
    void connectedChanged();
    void beatChanged();

private:
    /** Held as a base-class pointer so qmlui does not need to link the
     *  OS2L plugin's shared library. Becomes null if the plugin is unloaded. */
    QPointer<QLCIOPlugin> m_plugin;

    bool m_connected = false;
    double m_bpm = 0.0;
    double m_beatPos = 0.0;
    int m_beatCount = 0;
};

#endif // VDJBRIDGE_H
