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
#include "os2lbonjour.h"

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
    m_hostPort = OS2L_DEFAULT_PORT;
    m_tcpServer = NULL;
    m_bonjour = NULL;
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

    enableTCPServer(true);

    // Register as a Bonjour service so VirtualDJ Auto mode discovers QLC+.
    // On macOS this uses the native dns_sd API; on other platforms it is a no-op.
    // Reference: https://www.virtualdj.com/wiki/OS2L.html (Auto mode)
    if (m_bonjour == NULL)
    {
        m_bonjour = new OS2LBonjour(this);
        connect(m_bonjour, &OS2LBonjour::serviceRegistered,
                this, [](const QString &name, quint16 port) {
                    qDebug() << "[OS2L] Bonjour: registered as" << name << "on port" << port;
                    qDebug() << "[OS2L] VirtualDJ can now discover QLC+ in Auto mode";
                });
        connect(m_bonjour, &OS2LBonjour::serviceRegistrationFailed,
                this, [](const QString &err) {
                    qWarning() << "[OS2L] Bonjour registration failed:" << err;
                });
        m_bonjour->registerService("QLC+", m_hostPort);
    }

    return true;
}

void OS2LPlugin::closeInput(quint32 input, quint32 universe)
{
    enableTCPServer(false);

    if (m_bonjour != NULL)
    {
        m_bonjour->unregisterService();
        delete m_bonjour;
        m_bonjour = NULL;
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
            // Fields: "name" (string), "state" ("on"/"off").
            // Source: https://os2l.org
            QJsonValue jName = jsonObj.value("name");
            QJsonValue jState = jsonObj.value("state");
            qDebug() << "[OS2L] Button:" << jName.toString() << "state:" << jState.toString();
            uchar value = jState.toString() == "off" ? 0 : 255;
            emit valueChanged(m_inputUniverse, 0, getHash(jName.toString()), value, jName.toString());
        }
        else if (event == "cmd")
        {
            // "cmd" event — numeric command with value.
            // Fields: "id" (int), "param" (float 0.0–1.0).
            // Source: https://os2l.org
            QJsonValue jId = jsonObj.value("id");
            QJsonValue jParam = jsonObj.value("param");
            qDebug() << "[OS2L] CMD id:" << jId.toInt() << "param:" << jParam.toDouble();
            quint32 channel = quint32(jId.toInt());
            QString cmd = QString("cmd%1").arg(channel);
            emit valueChanged(m_inputUniverse, 0, quint32(jId.toInt()), uchar(jParam.toDouble()), cmd);
        }
        else if (event == "beat")
        {
           // "beat" event — BPM synchronization.
           // Source: https://os2l.org
           qDebug() << "[OS2L] Beat message received";
           emit valueChanged(m_inputUniverse, 0, 8341, 255, "beat");
        }
        else if (event == "song")
        {
            // "song" event — track metadata sent when a new song loads or info changes.
            // Field names sourced from:
            //   - OS2L specification: https://os2l.org
            //   - VirtualDJ OS2L wiki: https://www.virtualdj.com/wiki/OS2L.html
            QString songName = jsonObj.value("name").toString();    // Track title
            QString artist   = jsonObj.value("artist").toString();  // Artist name
            QString album    = jsonObj.value("album").toString();   // Album name
            QString genre    = jsonObj.value("genre").toString();   // Music genre
            QString year     = jsonObj.value("year").toString();    // Release year
            QString status   = jsonObj.value("status").toString();  // "play" / "pause" / "stop"
            double  bpm      = jsonObj.value("bpm").toDouble();     // Beats per minute
            QString key      = jsonObj.value("key").toString();     // Camelot key (e.g. "8B")
            double  elapsed  = jsonObj.value("elapsed").toDouble(); // Elapsed time (seconds)
            double  duration = jsonObj.value("duration").toDouble();// Total duration (seconds)
            QString remix    = jsonObj.value("remix").toString();   // Remix/edit version
            int     deck     = jsonObj.value("deck").toInt();       // Deck number (1 or 2)

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

            // Re-register Bonjour on the new port
            if (m_bonjour != NULL)
            {
                m_bonjour->unregisterService();
                m_bonjour->registerService("QLC+", m_hostPort);
            }
        }
    }

    /** Remember to call the base QLCIOPlugin method to actually inform
     *  QLC+ to store the parameter in the project workspace XML */
    QLCIOPlugin::setParameter(universe, line, type, name, value);
}
