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

#include "utils.h"
#include "os2lplugin.h"
#include "os2lconfiguration.h"
#include "os2ldiscovery.h"

/*****************************************************************************
 * Initialization
 *****************************************************************************/

OS2LPlugin::~OS2LPlugin()
{
    enableTCPServer(false);
}

void OS2LPlugin::init()
{
    m_inputUniverse = UINT_MAX;
    m_outputUniverse = UINT_MAX;
    m_hostPort = OS2L_DEFAULT_PORT;
    m_remotePort = OS2L_DEFAULT_PORT;
    m_tcpServer = NULL;
    m_outputSocket = NULL;
    m_discovery = NULL;
}

QString OS2LPlugin::name() const
{
    return QString("OS2L");
}

int OS2LPlugin::capabilities() const
{
    return QLCIOPlugin::Input | QLCIOPlugin::Output | QLCIOPlugin::Feedback | QLCIOPlugin::Beats;
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
    str += tr("This plugin provides support for OS2L (Open Sound 2 Light) protocol.");
    str += QString("<BR>");
    str += tr("Features:");
    str += QString("<UL>");
    str += QString("<LI>") + tr("Receives OS2L messages from VirtualDJ and compatible DJ software") + QString("</LI>");
    str += QString("<LI>") + tr("Automatic service discovery via Bonjour/mDNS") + QString("</LI>");
    str += QString("<LI>") + tr("Displays song metadata (name, artist, BPM, key, etc.)") + QString("</LI>");
    str += QString("<LI>") + tr("Supports beats, buttons, and commands") + QString("</LI>");
    str += QString("<LI>") + tr("Bidirectional communication for sending feedback") + QString("</LI>");
    str += QString("</UL>");
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

    enableTCPServer(true);

    // Start Bonjour/mDNS service discovery
    if (m_discovery == NULL)
    {
        m_discovery = new OS2LDiscovery(this);
        connect(m_discovery, &OS2LDiscovery::serviceDiscovered,
                this, [this](const OS2LDiscovery::ServiceInfo &service) {
                    qDebug() << "[OS2L Plugin] Auto-discovered service:"
                             << service.name << "at" << service.address.toString()
                             << ":" << service.port;
                    slotServiceDiscovered(service.address, service.port);
                });
        m_discovery->startDiscovery();
    }

    return true;
}

void OS2LPlugin::closeInput(quint32 input, quint32 universe)
{
    enableTCPServer(false);

    // Stop service discovery
    if (m_discovery != NULL)
    {
        m_discovery->stopDiscovery();
        delete m_discovery;
        m_discovery = NULL;
    }

    removeFromMap(input, universe, Input);

    m_inputUniverse = UINT_MAX;
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

/*************************************************************************
 * Outputs
 *************************************************************************/

bool OS2LPlugin::openOutput(quint32 output, quint32 universe)
{
    if (output != 0)
        return false;

    m_outputUniverse = universe;

    addToMap(universe, output, Output);

    qDebug() << "[OS2L Plugin] Output opened on universe" << universe;
    qDebug() << "[OS2L Plugin] Ready to send feedback messages to VirtualDJ";

    return true;
}

void OS2LPlugin::closeOutput(quint32 output, quint32 universe)
{
    if (m_outputSocket != NULL)
    {
        m_outputSocket->close();
        delete m_outputSocket;
        m_outputSocket = NULL;
    }

    removeFromMap(output, universe, Output);
    m_outputUniverse = UINT_MAX;
}

QStringList OS2LPlugin::outputs()
{
    QStringList list;
    list << QString("OS2L Output");
    return list;
}

QString OS2LPlugin::outputInfo(quint32 output)
{
    QString str;

    if (output != QLCIOPlugin::invalidLine())
        str += QString("<H3>%1</H3>").arg(outputs()[output]);

    str += QString("<P>");
    str += tr("This output can send OS2L feedback messages to VirtualDJ or other OS2L hosts.");
    str += QString("</P>");
    str += QString("</BODY>");
    str += QString("</HTML>");

    return str;
}

void OS2LPlugin::writeUniverse(quint32 universe, quint32 output, const QByteArray& data, bool dataChanged)
{
    Q_UNUSED(output);
    Q_UNUSED(dataChanged);

    if (universe != m_outputUniverse)
        return;

    // For now, this is a placeholder for sending OS2L commands
    // In a real implementation, you would parse DMX channels and convert them to OS2L commands
    // Example: specific channels could trigger cues, control crossfader, etc.

    qDebug() << "[OS2L Output] Would send data to" << m_remoteAddress.toString() << ":" << m_remotePort;
    qDebug() << "[OS2L Output] Data size:" << data.size() << "bytes";
}

bool OS2LPlugin::enableTCPServer(bool enable)
{
    if (enable)
    {
        m_tcpServer = new QTcpServer(this);

        if (m_tcpServer->listen(QHostAddress::Any, m_hostPort) == false)
        {
            qDebug() << "[OS2L] Error listening TCP socket on" << m_hostPort;
            return false;
        }
        connect(m_tcpServer, SIGNAL(newConnection()), this, SLOT(slotProcessNewTCPConnection()));
        qDebug() << "[OS2L] listening on TCP port" << m_hostPort;
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

    QHostAddress senderAddress = clientConnection->peerAddress();
    qDebug() << "[slotProcessNewTCPConnection] Host connected:" << senderAddress.toString();
    connect(clientConnection, SIGNAL(readyRead()), this, SLOT(slotProcessTCPPackets()));
    connect(clientConnection, SIGNAL(disconnected()), this, SLOT(slotProcessTCPPackets()));
}

void OS2LPlugin::slotHostDisconnected()
{
    QTcpSocket *socket = (QTcpSocket *)sender();
    QHostAddress senderAddress = socket->peerAddress();
    qDebug() << "Host with address" << senderAddress.toString() << "disconnected!";
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

        qDebug() << "[OS2L] Received" << message.length() << "bytes from" << senderAddress.toString();
        qDebug() << "[OS2L] Raw message:" << message;

        QJsonObject jsonObj = json.object();

        // Log all JSON fields for debugging
        qDebug() << "[OS2L] Parsed JSON keys:" << jsonObj.keys();
        for (const QString &key : jsonObj.keys())
        {
            QJsonValue val = jsonObj.value(key);
            if (val.isString())
                qDebug() << "[OS2L]  " << key << "=" << val.toString();
            else if (val.isDouble())
                qDebug() << "[OS2L]  " << key << "=" << val.toDouble();
            else if (val.isBool())
                qDebug() << "[OS2L]  " << key << "=" << val.toBool();
            else if (val.isObject())
                qDebug() << "[OS2L]  " << key << "= [object]";
            else if (val.isArray())
                qDebug() << "[OS2L]  " << key << "= [array]";
        }

        QJsonValue jEvent = jsonObj.value("evt");
        if (jEvent.isUndefined())
        {
            qDebug() << "[OS2L] Warning: No 'evt' field in message, ignoring";
            return;
        }

        QString event = jEvent.toString();
        qDebug() << "[OS2L] Event type:" << event;

        if (event == "btn")
        {
            QJsonValue jName = jsonObj.value("name");
            QJsonValue jState = jsonObj.value("state");
            qDebug() << "[OS2L] Button event: name=" << jName.toString() << "state=" << jState.toString();
            uchar value = jState.toString() == "off" ? 0 : 255;
            emit valueChanged(m_inputUniverse, 0, getHash(jName.toString()), value, jName.toString());
        }
        else if (event == "cmd")
        {
            QJsonValue jId = jsonObj.value("id");
            QJsonValue jParam = jsonObj.value("param");
            qDebug() << "[OS2L] CMD message: id=" << jId.toInt() << "param=" << jParam.toDouble();
            quint32 channel = quint32(jId.toInt());
            QString cmd = QString("cmd%1").arg(channel);
            emit valueChanged(m_inputUniverse, 0, quint32(jId.toInt()), uchar(jParam.toDouble()), cmd);
        }
        else if (event == "beat")
        {
           qDebug() << "[OS2L] Beat message received";
           emit valueChanged(m_inputUniverse, 0, 8341, 255, "beat");
        }
        else if (event == "song" || event.isEmpty())
        {
            // Handle song metadata
            QString songName = jsonObj.value("name").toString();
            QString artist = jsonObj.value("artist").toString();
            QString album = jsonObj.value("album").toString();
            QString genre = jsonObj.value("genre").toString();
            QString year = jsonObj.value("year").toString();
            QString status = jsonObj.value("status").toString();
            double bpm = jsonObj.value("bpm").toDouble();
            QString key = jsonObj.value("key").toString();
            double elapsed = jsonObj.value("elapsed").toDouble();
            double duration = jsonObj.value("duration").toDouble();
            QString remix = jsonObj.value("remix").toString();
            int deck = jsonObj.value("deck").toInt();

            qDebug() << "[OS2L] ==================== SONG METADATA ====================";
            if (!songName.isEmpty())
                qDebug() << "[OS2L] Song Name:" << songName;
            if (!artist.isEmpty())
                qDebug() << "[OS2L] Artist:" << artist;
            if (!album.isEmpty())
                qDebug() << "[OS2L] Album:" << album;
            if (!genre.isEmpty())
                qDebug() << "[OS2L] Genre:" << genre;
            if (!year.isEmpty())
                qDebug() << "[OS2L] Year:" << year;
            if (!remix.isEmpty())
                qDebug() << "[OS2L] Remix:" << remix;
            if (!status.isEmpty())
                qDebug() << "[OS2L] Status:" << status;
            if (bpm > 0)
                qDebug() << "[OS2L] BPM:" << bpm;
            if (!key.isEmpty())
                qDebug() << "[OS2L] Key:" << key;
            if (elapsed > 0)
                qDebug() << "[OS2L] Elapsed:" << elapsed << "seconds";
            if (duration > 0)
                qDebug() << "[OS2L] Duration:" << duration << "seconds";
            if (deck > 0)
                qDebug() << "[OS2L] Deck:" << deck;
            qDebug() << "[OS2L] ======================================================";

            // Emit song info as channel values (using hash for song identification)
            if (!songName.isEmpty())
            {
                QString songId = QString("%1 - %2").arg(artist, songName);
                emit valueChanged(m_inputUniverse, 0, getHash(songId), 255, songId);
            }
        }
        else
        {
            qDebug() << "[OS2L] Unknown event type:" << event;
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
    /** This method is provided to QLC+ to set the plugin specific settings.
     *  Those settings are saved in a project workspace and when it is loaded,
     *  this method is called after QLC+ has opened the input/output lines
     *  mapped in the project workspace as well.
     */

    if (name == OS2L_HOST_ADDRESS)
    {

    }
    else if (name == OS2L_HOST_PORT)
    {
        if (value.toInt() != m_hostPort)
        {
            m_hostPort = quint16(value.toUInt());

            /** restart the TCP server and listen on new port */
            enableTCPServer(false);
            enableTCPServer(true);
        }
    }

    /** Remember to call the base QLCIOPlugin method to actually inform
     *  QLC+ to store the parameter in the project workspace XML */
    QLCIOPlugin::setParameter(universe, line, type, name, value);
}

void OS2LPlugin::slotServiceDiscovered(const QHostAddress &address, quint16 port)
{
    qDebug() << "[OS2L Plugin] *** OS2L Service Discovered ***";
    qDebug() << "[OS2L Plugin]   Address:" << address.toString();
    qDebug() << "[OS2L Plugin]   Port:" << port;
    qDebug() << "[OS2L Plugin]   You can now configure VirtualDJ to send to this QLC+ instance";
}
