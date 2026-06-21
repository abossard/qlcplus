/*
  Q Light Controller Plus
  vdjbridgeplugin.cpp

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

#include "vdjbridgeplugin.h"
#include "configurevdjbridge.h"
#include "vdjtelemetryclient.h"
#include "vdjbonjour.h"

#include <QDebug>

VdjBridgePlugin::~VdjBridgePlugin()
{
    if (m_open)
        closeInput(0, m_inputUniverse);
}

void VdjBridgePlugin::init()
{
}

QString VdjBridgePlugin::name() const
{
    return QStringLiteral("VDJ Bridge");
}

int VdjBridgePlugin::capabilities() const
{
    return QLCIOPlugin::Input | QLCIOPlugin::Beats;
}

QString VdjBridgePlugin::pluginInfo() const
{
    QString str;
    str  = QString("<HTML><HEAD><TITLE>%1</TITLE></HEAD><BODY>").arg(name());
    str += QString("<H3>%1</H3>").arg(name());
    str += tr("This plugin exposes VirtualDJ DMXDesktop telemetry "
              "(TCP, default port 8050, Bonjour service "
              "<tt>_os2l._tcp</tt>) as a QLC+ input. "
              "It emits beat events and per-deck metadata that the "
              "qmlui VdjBridge facade consumes to drive the Song Manager.");
    str += QString("</BODY></HTML>");
    return str;
}

bool VdjBridgePlugin::openInput(quint32 input, quint32 universe)
{
    Q_UNUSED(input)

    if (m_open)
        return true;

    m_inputUniverse = universe;
    m_telemetry = new VdjTelemetryClient(this);

    connect(m_telemetry, &VdjTelemetryClient::deckTriggerReceived,
            this, &VdjBridgePlugin::deckTriggerReceived);
    connect(m_telemetry, &VdjTelemetryClient::globalTriggerReceived,
            this, &VdjBridgePlugin::globalTriggerReceived);
    connect(m_telemetry, &VdjTelemetryClient::beatReceived,
            this, [this](int pos, qreal bpm, qreal strength, bool change) {
        // Mirror OS2L's pattern: emit valueChanged on a fixed beat
        // channel (8341 / 0xff) so the engine beat-source machinery
        // picks it up, then emit both the standard zero-arg beat and
        // the rich telemetry-payload beat.
        emit valueChanged(m_inputUniverse, 0, 8341, 255, QStringLiteral("beat"));
        emit beatReceived();
        emit telemetryBeatReceived(pos, bpm, strength, change);
    });
    connect(m_telemetry, &VdjTelemetryClient::clientConnected,
            this, [this, input, universe]() {
        emit clientConnected();
        emit connectionStatusChanged(universe, input);
    });
    connect(m_telemetry, &VdjTelemetryClient::clientDisconnected,
            this, [this, input, universe]() {
        emit clientDisconnected();
        emit connectionStatusChanged(universe, input);
    });

    if (!m_telemetry->start(m_hostPort))
    {
        qWarning() << "[VdjBridge] failed to bind TCP port" << m_hostPort;
        delete m_telemetry;
        m_telemetry = nullptr;
        return false;
    }

    if (m_bonjourEnabled)
    {
        m_bonjour = new VdjBonjour(this);
        m_bonjour->registerService(m_serviceName, m_hostPort);
    }

    addToMap(universe, input, Input);

    m_open = true;
    emit connectionStatusChanged(universe, input);
    return true;
}

void VdjBridgePlugin::closeInput(quint32 input, quint32 universe)
{
    if (!m_open)
        return;

    removeFromMap(input, universe, Input);

    if (m_bonjour)
    {
        m_bonjour->unregisterService();
        m_bonjour->deleteLater();
        m_bonjour = nullptr;
    }

    if (m_telemetry)
    {
        m_telemetry->stop();
        m_telemetry->deleteLater();
        m_telemetry = nullptr;
    }

    m_open = false;
    m_inputUniverse = QLCIOPlugin::invalidLine();
    emit connectionStatusChanged(universe, input);
}

QStringList VdjBridgePlugin::inputs()
{
    return QStringList() << QStringLiteral("1: VDJ Telemetry");
}

QString VdjBridgePlugin::inputInfo(quint32 input)
{
    Q_UNUSED(input)
    QString str;
    str  = QString("<H3>%1</H3>").arg(QStringLiteral("VDJ Telemetry"));
    str += QString("<P><B>%1:</B> %2</P>")
            .arg(tr("Listen port")).arg(m_hostPort);
    str += QString("<P><B>%1:</B> %2</P>")
            .arg(tr("Bonjour service"))
            .arg(m_bonjourEnabled
                 ? QStringLiteral("_os2l._tcp (%1)").arg(m_serviceName)
                 : tr("disabled"));
    return str;
}

int VdjBridgePlugin::connectionStatus(quint32 input)
{
    Q_UNUSED(input)
    if (!m_telemetry)
        return QLCIOPlugin::Idle;
    if (m_telemetry->status() == VdjTelemetryClient::ClientConnected)
        return QLCIOPlugin::Connected;
    return QLCIOPlugin::Advertising;
}

void VdjBridgePlugin::configure()
{
    ConfigureVdjBridge dialog(this);
    dialog.exec();
}

bool VdjBridgePlugin::canConfigure() const
{
    return true;
}

QString VdjBridgePlugin::clientAddress() const
{
    if (m_telemetry && m_telemetry->clientAddress().isEmpty() == false)
        return m_telemetry->clientAddress();
    return QString();
}

void VdjBridgePlugin::setParameter(quint32 universe, quint32 line, Capability type,
                                   QString name, QVariant value)
{
    if (name == QLatin1String(VDJ_HOST_PORT))
    {
        quint16 newPort = static_cast<quint16>(value.toUInt());
        if (newPort == m_hostPort)
            return;
        m_hostPort = newPort;
        if (m_open && m_telemetry)
        {
            m_telemetry->stop();
            m_telemetry->start(m_hostPort);
            if (m_bonjour)
            {
                m_bonjour->unregisterService();
                m_bonjour->registerService(m_serviceName, m_hostPort);
            }
        }
    }
    else if (name == QLatin1String(VDJ_BONJOUR_ENABLED))
    {
        m_bonjourEnabled = value.toBool();
        if (m_open)
        {
            if (m_bonjourEnabled && !m_bonjour)
            {
                m_bonjour = new VdjBonjour(this);
                m_bonjour->registerService(m_serviceName, m_hostPort);
            }
            else if (!m_bonjourEnabled && m_bonjour)
            {
                m_bonjour->unregisterService();
                m_bonjour->deleteLater();
                m_bonjour = nullptr;
            }
        }
    }
    else if (name == QLatin1String(VDJ_SERVICE_NAME))
    {
        m_serviceName = value.toString();
        if (m_open && m_bonjour)
        {
            m_bonjour->unregisterService();
            m_bonjour->registerService(m_serviceName, m_hostPort);
        }
    }
    else
    {
        QLCIOPlugin::setParameter(universe, line, type, name, value);
    }
}
