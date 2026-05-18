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
 * Qt-facing facade for the OS2L plugin's beat tick stream.
 *
 * Exposes ONLY what stock VirtualDJ verifiably broadcasts via OS2L:
 *  - A bare `evt:"beat"` message at the beat rate (no payload).
 *  - TCP connection state from the OS2L plugin.
 *
 * The bridge does NOT expose BPM, beat position, song info, or any
 * other field, because VDJ does not send them. Computing BPM from
 * inter-arrival times is possible but is a measurement task that
 * belongs elsewhere — not in this passive facade.
 *
 * Connections from the plugin are made by App using Qt's string-based
 * signal/slot syntax so qmlui does not need to link against the plugin .so.
 */
class VdjBridge final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(int beatCount READ beatCount NOTIFY beatReceived)

public:
    explicit VdjBridge(QObject *parent = nullptr);

    /**
     * Bind to an OS2L plugin instance and start consuming its events.
     * The plugin pointer may be null (no OS2L plugin available); the bridge
     * stays in a disconnected state. Subsequent calls replace any prior binding.
     */
    void attachOS2LPlugin(QLCIOPlugin *plugin);

    bool connected() const { return m_connected; }
    int beatCount() const { return m_beatCount; }

public slots:
    /** Connected to OS2LPlugin::beatReceived (string-based). */
    void onBeat();

    /** Connected to QLCIOPlugin::connectionStatusChanged.
     *  Re-queries the plugin and updates the connected property. */
    void refreshConnectionStatus();

signals:
    void connectedChanged();
    void beatReceived();

private:
    /** Held as a base-class pointer so qmlui does not need to link the
     *  OS2L plugin's shared library. Becomes null if the plugin is unloaded. */
    QPointer<QLCIOPlugin> m_plugin;

    bool m_connected = false;
    int m_beatCount = 0;
};

#endif // VDJBRIDGE_H
