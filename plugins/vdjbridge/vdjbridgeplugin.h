/*
  Q Light Controller Plus
  vdjbridgeplugin.h

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

#ifndef VDJBRIDGEPLUGIN_H
#define VDJBRIDGEPLUGIN_H

#include <QVariant>
#include "qlcioplugin.h"

#define VDJ_HOST_PORT        "hostPort"
#define VDJ_BONJOUR_ENABLED  "bonjourEnabled"
#define VDJ_SERVICE_NAME     "serviceName"
#define VDJ_DEFAULT_PORT     8050

class VdjTelemetryClient;
class VdjBonjour;

/**
 * QLC+ I/O plugin that exposes the VirtualDJ DMXDesktop telemetry
 * protocol (TCP, default port 8050, Bonjour service
 * "_os2l._tcp") as a QLC+ input.
 *
 * Emits the standard QLCIOPlugin::beatReceived() and
 * valueChanged(...) for engine beat-source integration, plus custom
 * signals carrying the rich DMXDesktop payload (per-deck triggers,
 * global mixer state, telemetry-style beats) that the qmlui VdjBridge
 * facade consumes via string-based SIGNAL/SLOT.
 */
class VdjBridgePlugin final : public QLCIOPlugin
{
    Q_OBJECT
    Q_INTERFACES(QLCIOPlugin)
    Q_PLUGIN_METADATA(IID QLCIOPlugin_iid)

public:
    virtual ~VdjBridgePlugin();

    void init() override;
    QString name() const override;
    int capabilities() const override;
    QString pluginInfo() const override;

    bool openInput(quint32 input, quint32 universe) override;
    void closeInput(quint32 input, quint32 universe) override;
    QStringList inputs() override;
    QString inputInfo(quint32 input) override;
    int connectionStatus(quint32 input) override;

    void configure() override;
    bool canConfigure() const override;
    void setParameter(quint32 universe, quint32 line, Capability type,
                      QString name, QVariant value) override;

signals:
    // Standard zero-arg beat signal (matches OS2L convention) — emitted
    // alongside valueChanged(...) on every telemetry beat so the engine
    // beat-source machinery and other listeners that only know the
    // QLCIOPlugin interface can pick it up.
    void beatReceived();

    // Rich beat signal carrying DMXDesktop telemetry payload
    // (position, BPM, strength, deck-change flag).
    void telemetryBeatReceived(int pos, qreal bpm, qreal strength, bool change);

    // Custom signals consumed by the qmlui VdjBridge facade through
    // string-based connect() — the facade only knows the plugin as
    // QLCIOPlugin*.
    void deckTriggerReceived(int deckIndex, const QString &trigger, const QVariant &value);
    void globalTriggerReceived(const QString &trigger, const QVariant &value);
    void clientConnected();
    void clientDisconnected();

private:
    quint16  m_hostPort       = VDJ_DEFAULT_PORT;
    bool     m_bonjourEnabled = true;
    QString  m_serviceName    = QStringLiteral("QLC+ VDJ");
    quint32  m_inputUniverse  = QLCIOPlugin::invalidLine();
    bool     m_open           = false;

    VdjTelemetryClient *m_telemetry = nullptr;
    VdjBonjour         *m_bonjour   = nullptr;
};

#endif // VDJBRIDGEPLUGIN_H
