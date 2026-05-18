/*
  Q Light Controller Plus
  os2lplugin.cpp

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

#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCoreApplication>

#include "utils.h"
#include "os2lplugin.h"
#include "os2lconfiguration.h"
#include "os2lbonjour.h"

/*****************************************************************************
 * Initialization
 *****************************************************************************/

OS2LPlugin::~OS2LPlugin()
{
    enableTCPServer(false);
}

/*********************************************************************
 * Diagnostics
 *********************************************************************/

bool OS2LPlugin::isDebugMode() const
{
    QCoreApplication *app = QCoreApplication::instance();
    return app && app->property("debugMode").toBool();
}

void OS2LPlugin::diagLog(const QString &type, const QString &detail)
{
    if (!isDiagnosticsEnabled())
        return;

    QMutexLocker locker(&m_diagMutex);

    OS2LDiagEvent evt;
    evt.timestamp = QDateTime::currentMSecsSinceEpoch();
    evt.type = type;
    evt.detail = detail;
    m_diagEvents.append(evt);

    // Cap at OS2L_DIAG_MAX_EVENTS
    while (m_diagEvents.size() > OS2L_DIAG_MAX_EVENTS)
        m_diagEvents.removeFirst();

    // Update stats counter
    m_diagStats[type] = m_diagStats.value(type, 0) + 1;
}

QByteArray OS2LPlugin::pluginDiagnostics() const
{
    if (!isDiagnosticsEnabled())
        return QByteArray();

    QMutexLocker locker(&m_diagMutex);

    QJsonObject root;

    // Status
    QString statusStr = "idle";
    if (m_clientConnected)
        statusStr = "connected";
    else if (m_bonjourRegistered)
        statusStr = "advertising";
    root["status"] = statusStr;

    root["bonjourEnabled"] = m_bonjourEnabled;
    root["bonjourRegistered"] = m_bonjourRegistered;
    root["port"] = m_hostPort;
    root["clientConnected"] = m_clientConnected;
    root["clientAddress"] = m_clientAddress;
    root["universe"] = m_inputUniverse == UINT_MAX ? -1 : (int)m_inputUniverse;

    // Events (newest first)
    QJsonArray eventsArr;
    for (int i = m_diagEvents.size() - 1; i >= 0; --i)
    {
        const OS2LDiagEvent &e = m_diagEvents[i];
        QJsonObject obj;
        obj["ts"] = e.timestamp;
        obj["type"] = e.type;
        obj["detail"] = e.detail;
        eventsArr.append(obj);
    }
    root["events"] = eventsArr;

    // Stats
    QJsonObject statsObj;
    for (auto it = m_diagStats.constBegin(); it != m_diagStats.constEnd(); ++it)
        statsObj[it.key()] = it.value();
    root["stats"] = statsObj;

    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

void OS2LPlugin::init()
{
    m_inputUniverse = UINT_MAX;
    m_hostPort = OS2L_DEFAULT_PORT;
    m_tcpServer = NULL;
    m_bonjour = NULL;
    m_bonjourEnabled = true;
    m_bonjourRegistered = false;
    m_clientConnected = false;
    m_clientSocket = NULL;
    m_clientAddress.clear();
}

QString OS2LPlugin::name() const
{
    return QString("OS2L");
}

int OS2LPlugin::capabilities() const
{
    return QLCIOPlugin::Input | QLCIOPlugin::Feedback | QLCIOPlugin::Beats;
}

QString OS2LPlugin::pluginInfo() const
{
    /** Return a description of the purpose of this plugin
     *  in HTML format */
    QString str;

    str += QString("<HTML>");
    str += QString("<HEAD>");
    str += QString("<TITLE>%1</TITLE>").arg(name());
    str += QString("</HEAD>");
    str += QString("<BODY>");

    str += QString("<P>");
    str += QString("<H3>%1</H3>").arg(name());
    str += tr("This plugin provides support for one OS2L host.");
    str += QString("<BR>");
    str += tr("On macOS, the plugin registers itself via Bonjour so VirtualDJ "
              "can discover QLC+ automatically (OS2L set to Auto).");
    str += QString("</P>");

    return str;
}

/*************************************************************************
 * Inputs
 *************************************************************************/

bool OS2LPlugin::openInput(quint32 input, quint32 universe)
{
    if (input != 0)
        return false;

    m_inputUniverse = universe;

    addToMap(universe, input, Input);

    if (!enableTCPServer(true))
    {
        qWarning() << "[OS2L] Failed to start TCP server — not advertising via Bonjour";
        diagLog("error", "Failed to start TCP server — not advertising via Bonjour");
        return false;
    }

    // Register as a Bonjour service so VirtualDJ Auto mode discovers QLC+.
    // Only if user has enabled Bonjour and the TCP server started successfully.
    if (m_bonjourEnabled && m_bonjour == NULL)
    {
        m_bonjour = new OS2LBonjour(this);
        connect(m_bonjour, &OS2LBonjour::serviceRegistered,
                this, [this](const QString &name, quint16 port) {
                    qDebug() << "[OS2L] Bonjour: registered as" << name << "on port" << port;
                    diagLog("bonjour", QString("Registered as %1 on port %2").arg(name).arg(port));
                    m_bonjourRegistered = true;
                    emit connectionStatusChanged(m_inputUniverse, 0);
                });
        connect(m_bonjour, &OS2LBonjour::serviceRegistrationFailed,
                this, [this](const QString &err) {
                    qWarning() << "[OS2L] Bonjour registration failed:" << err;
                    diagLog("bonjour", QString("Registration failed: %1").arg(err));
                    m_bonjourRegistered = false;
                    emit connectionStatusChanged(m_inputUniverse, 0);
                });
        m_bonjour->registerService("QLC+", m_hostPort);
        diagLog("bonjour", QString("Registering service on port %1...").arg(m_hostPort));
    }

    return true;
}

void OS2LPlugin::closeInput(quint32 input, quint32 universe)
{
    m_clientConnected = false;
    m_clientSocket = NULL;

    enableTCPServer(false);

    if (m_bonjour != NULL)
    {
        m_bonjour->unregisterService();
        delete m_bonjour;
        m_bonjour = NULL;
        m_bonjourRegistered = false;
        diagLog("bonjour", "Service unregistered");
    }

    removeFromMap(input, universe, Input);

    m_inputUniverse = UINT_MAX;

    emit connectionStatusChanged(universe, input);
}

QStringList OS2LPlugin::inputs()
{
    /**
     * Build a list of input line names. The names must be always in the
     * same order i.e. the first name is the name of input line number 0,
     * the next one is output line number 1, etc..
     */
    QStringList list;
    list << QString("OS2L line");
    return list;
}

QString OS2LPlugin::inputInfo(quint32 input)
{
    /**
     * Provide an informational text regarding the specified input line.
     * This text is in HTML format and it is shown to the user.
     */
    QString str;

    if (input != QLCIOPlugin::invalidLine())
        str += QString("<H3>%1</H3>").arg(inputs()[input]);

    str += QString("</BODY>");
    str += QString("</HTML>");

    return str;
}

quint32 OS2LPlugin::universe() const
{
    return m_inputUniverse;
}

bool OS2LPlugin::bonjourEnabled() const
{
    return m_bonjourEnabled;
}

int OS2LPlugin::connectionStatus(quint32 input)
{
    Q_UNUSED(input)

    // Connected takes priority (works even with Bonjour disabled via direct IP)
    if (m_clientConnected)
        return Connected;

    // Bonjour advertising but no client yet
    if (m_bonjourRegistered)
        return Advertising;

    return Idle;
}

bool OS2LPlugin::enableTCPServer(bool enable)
{
    if (enable)
    {
        m_tcpServer = new QTcpServer(this);

        if (m_tcpServer->listen(QHostAddress::Any, m_hostPort) == false)
        {
            qDebug() << "[OS2L] Error listening TCP socket on" << m_hostPort;
            diagLog("error", QString("Error listening TCP socket on port %1").arg(m_hostPort));
            return false;
        }
        connect(m_tcpServer, SIGNAL(newConnection()), this, SLOT(slotProcessNewTCPConnection()));
        qDebug() << "[OS2L] listening on TCP port" << m_hostPort;
        diagLog("connection", QString("TCP server listening on port %1").arg(m_hostPort));
    }
    else
    {
        if (m_tcpServer == NULL)
            return true;

        disconnect(m_tcpServer, SIGNAL(newConnection()), this, SLOT(slotProcessNewTCPConnection()));
        m_tcpServer->close();
        delete m_tcpServer;
        m_tcpServer = NULL;
        qDebug() << "[OS2L] stop listening on TCP";
        diagLog("connection", "TCP server stopped");
    }

    return true;
}

quint16 OS2LPlugin::getHash(QString channel)
{
    quint16 hash;
    if (m_hashMap.contains(channel))
        hash = m_hashMap[channel];
    else
    {
        /** No existing hash found. Add a new key to the table */
        hash = Utils::getChecksum(channel.toUtf8());
        m_hashMap[channel] = hash;
    }

    return hash;
}

void OS2LPlugin::slotProcessNewTCPConnection()
{
    qDebug() << Q_FUNC_INFO;
    QTcpSocket *clientConnection = m_tcpServer->nextPendingConnection();
    if (clientConnection == NULL)
        return;

    // Enforce single client: close any existing connection
    if (m_clientSocket != NULL && m_clientSocket != clientConnection)
    {
        qDebug() << "[OS2L] Closing previous client connection";
        m_clientSocket->disconnectFromHost();
        m_clientSocket->deleteLater();
    }

    m_clientSocket = clientConnection;
    m_clientConnected = true;

    QHostAddress senderAddress = clientConnection->peerAddress();
    m_clientAddress = senderAddress.toString();
    qDebug() << "[OS2L] Host connected:" << m_clientAddress;
    diagLog("connection", QString("Host connected: %1").arg(m_clientAddress));
    connect(clientConnection, SIGNAL(readyRead()), this, SLOT(slotProcessTCPPackets()));
    connect(clientConnection, SIGNAL(disconnected()), this, SLOT(slotHostDisconnected()));

    emit connectionStatusChanged(m_inputUniverse, 0);
}

void OS2LPlugin::slotHostDisconnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (socket != NULL)
    {
        QHostAddress senderAddress = socket->peerAddress();
        qDebug() << "[OS2L] Host disconnected:" << senderAddress.toString();
        diagLog("connection", QString("Host disconnected: %1").arg(senderAddress.toString()));
        socket->deleteLater();
    }

    if (socket == m_clientSocket)
    {
        m_clientSocket = NULL;
        m_clientConnected = false;
        m_clientAddress.clear();
        m_packetLeftOver.clear();
        emit connectionStatusChanged(m_inputUniverse, 0);
    }
}

void OS2LPlugin::slotProcessTCPPackets()
{
    QTcpSocket *socket = (QTcpSocket *)sender();
    if (socket == NULL)
        return;

    QHostAddress senderAddress = QHostAddress(socket->peerAddress().toIPv4Address());

    while (1)
    {
        m_packetLeftOver.append(socket->readAll());

        int endIndex = m_packetLeftOver.indexOf("}");
        if (endIndex == -1)
        {
            if (socket->bytesAvailable())
                continue;
            else
                break;
        }

        QByteArray message = m_packetLeftOver.left(endIndex + 1);
        m_packetLeftOver.remove(0, endIndex + 1);
        QJsonDocument json = QJsonDocument::fromJson(message);

        // Enhanced logging: show raw message and all parsed fields.
        // Useful for debugging the OS2L protocol (https://os2l.org).
        qDebug() << "[OS2L] Received" << message.length() << "bytes from" << senderAddress.toString();
        qDebug() << "[OS2L] Raw message:" << message;

        QJsonObject jsonObj = json.object();

        // Log every JSON key/value so the user can inspect all OS2L data.
        for (const QString &key : jsonObj.keys())
        {
            QJsonValue val = jsonObj.value(key);
            if (val.isString())
                qDebug() << "[OS2L]  " << key << "=" << val.toString();
            else if (val.isDouble())
                qDebug() << "[OS2L]  " << key << "=" << val.toDouble();
            else if (val.isBool())
                qDebug() << "[OS2L]  " << key << "=" << val.toBool();
        }

        // OS2L protocol: every message carries an "evt" field.
        // Source: OS2L specification — https://os2l.org
        QJsonValue jEvent = jsonObj.value("evt");
        if (jEvent.isUndefined())
            return;

        QString event = jEvent.toString();

        if (event == "btn")
        {
            // "btn" event — button press/release.
            QJsonValue jName = jsonObj.value("name");
            QJsonValue jState = jsonObj.value("state");
            qDebug() << "[OS2L] Button:" << jName.toString() << "state:" << jState.toString();
            diagLog("message", QString("btn: %1 = %2").arg(jName.toString(), jState.toString()));
            uchar value = jState.toString() == "off" ? 0 : 255;
            emit valueChanged(m_inputUniverse, 0, getHash(jName.toString()), value, jName.toString());
        }
        else if (event == "cmd")
        {
            // "cmd" event — numeric command with value.
            QJsonValue jId = jsonObj.value("id");
            QJsonValue jParam = jsonObj.value("param");
            qDebug() << "[OS2L] CMD id:" << jId.toInt() << "param:" << jParam.toDouble();
            diagLog("message", QString("cmd: id=%1 param=%2").arg(jId.toInt()).arg(jParam.toDouble()));
            quint32 channel = quint32(jId.toInt());
            QString cmd = QString("cmd%1").arg(channel);
            emit valueChanged(m_inputUniverse, 0, quint32(jId.toInt()), uchar(jParam.toDouble()), cmd);
        }
        else if (event == "beat")
        {
           // "beat" event — BPM synchronization. Stock VDJ sends a bare
           // `{"evt":"beat"}`; the spec allows bpm/pos/change but VDJ
           // does not include them, so we do not parse them.
           qDebug() << "[OS2L] Beat message received";
           diagLog("message", "beat");
           emit valueChanged(m_inputUniverse, 0, 8341, 255, "beat");
           emit beatReceived();
        }
        else if (event == "song")
        {
            // "song" event — track metadata
            QString songName = jsonObj.value("name").toString();
            QString artist   = jsonObj.value("artist").toString();
            QString album    = jsonObj.value("album").toString();
            QString genre    = jsonObj.value("genre").toString();
            QString year     = jsonObj.value("year").toString();
            QString status   = jsonObj.value("status").toString();
            double  bpm      = jsonObj.value("bpm").toDouble();
            QString key      = jsonObj.value("key").toString();
            double  elapsed  = jsonObj.value("elapsed").toDouble();
            double  duration = jsonObj.value("duration").toDouble();
            QString remix    = jsonObj.value("remix").toString();
            int     deck     = jsonObj.value("deck").toInt();

            qDebug() << "[OS2L] ==================== SONG METADATA ====================";
            if (!songName.isEmpty()) qDebug() << "[OS2L] Song Name:" << songName;
            if (!artist.isEmpty())   qDebug() << "[OS2L] Artist:" << artist;
            if (!album.isEmpty())    qDebug() << "[OS2L] Album:" << album;
            if (!genre.isEmpty())    qDebug() << "[OS2L] Genre:" << genre;
            if (!year.isEmpty())     qDebug() << "[OS2L] Year:" << year;
            if (!remix.isEmpty())    qDebug() << "[OS2L] Remix:" << remix;
            if (!status.isEmpty())   qDebug() << "[OS2L] Status:" << status;
            if (bpm > 0)             qDebug() << "[OS2L] BPM:" << bpm;
            if (!key.isEmpty())      qDebug() << "[OS2L] Key:" << key;
            if (elapsed > 0)         qDebug() << "[OS2L] Elapsed:" << elapsed << "seconds";
            if (duration > 0)        qDebug() << "[OS2L] Duration:" << duration << "seconds";
            if (deck > 0)            qDebug() << "[OS2L] Deck:" << deck;
            qDebug() << "[OS2L] ======================================================";

            // Build a rich diagnostic string for the ring buffer
            QStringList parts;
            if (!artist.isEmpty()) parts << QString("artist=%1").arg(artist);
            if (!songName.isEmpty()) parts << QString("title=%1").arg(songName);
            if (!album.isEmpty()) parts << QString("album=%1").arg(album);
            if (!status.isEmpty()) parts << QString("status=%1").arg(status);
            if (bpm > 0) parts << QString("bpm=%1").arg(bpm);
            if (!key.isEmpty()) parts << QString("key=%1").arg(key);
            if (deck > 0) parts << QString("deck=%1").arg(deck);
            if (duration > 0) parts << QString("duration=%1s").arg(duration);
            diagLog("message", QString("song: %1").arg(parts.join(", ")));

            if (!songName.isEmpty())
            {
                QString songId = QString("%1 - %2").arg(artist, songName);
                emit valueChanged(m_inputUniverse, 0, getHash(songId), 255, songId);
            }
        }
        else
        {
            qDebug() << "[OS2L] Unknown event type:" << event;
            diagLog("message", QString("unknown: %1").arg(event));
        }
    }
}

/*****************************************************************************
 * Configuration
 *****************************************************************************/

void OS2LPlugin::configure()
{
    OS2LConfiguration conf(this);
    conf.exec();
}

bool OS2LPlugin::canConfigure() const
{
    return true;
}

void OS2LPlugin::setParameter(quint32 universe, quint32 line, Capability type,
                             QString name, QVariant value)
{
    if (name == OS2L_HOST_ADDRESS)
    {
        // Placeholder — host address is not used (we listen on all interfaces)
    }
    else if (name == OS2L_HOST_PORT)
    {
        if (value.toInt() != m_hostPort)
        {
            m_hostPort = quint16(value.toUInt());

            // Only restart if the input is currently active
            if (m_inputUniverse != UINT_MAX)
            {
                enableTCPServer(false);
                enableTCPServer(true);

                // Re-register Bonjour on the new port
                if (m_bonjour != NULL)
                {
                    m_bonjour->unregisterService();
                    m_bonjourRegistered = false;
                    m_bonjour->registerService("QLC+", m_hostPort);
                }
            }
        }
    }
    else if (name == OS2L_BONJOUR_ENABLED)
    {
        m_bonjourEnabled = value.toBool();

        // Only do runtime changes if the input is currently active
        if (m_inputUniverse != UINT_MAX)
        {
            if (m_bonjourEnabled && m_bonjour == NULL)
            {
                m_bonjour = new OS2LBonjour(this);
                connect(m_bonjour, &OS2LBonjour::serviceRegistered,
                        this, [this](const QString &name, quint16 port) {
                            qDebug() << "[OS2L] Bonjour: registered as" << name << "on port" << port;
                            diagLog("bonjour", QString("Registered as %1 on port %2").arg(name).arg(port));
                            m_bonjourRegistered = true;
                            emit connectionStatusChanged(m_inputUniverse, 0);
                        });
                connect(m_bonjour, &OS2LBonjour::serviceRegistrationFailed,
                        this, [this](const QString &err) {
                            qWarning() << "[OS2L] Bonjour registration failed:" << err;
                            diagLog("bonjour", QString("Registration failed: %1").arg(err));
                            m_bonjourRegistered = false;
                            emit connectionStatusChanged(m_inputUniverse, 0);
                        });
                m_bonjour->registerService("QLC+", m_hostPort);
                diagLog("bonjour", "Bonjour enabled at runtime");
            }
            else if (!m_bonjourEnabled && m_bonjour != NULL)
            {
                m_bonjour->unregisterService();
                delete m_bonjour;
                m_bonjour = NULL;
                m_bonjourRegistered = false;
                diagLog("bonjour", "Bonjour disabled at runtime");
                emit connectionStatusChanged(m_inputUniverse, 0);
            }
        }
    }

    QLCIOPlugin::setParameter(universe, line, type, name, value);
}
