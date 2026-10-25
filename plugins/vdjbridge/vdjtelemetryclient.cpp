/*
  Q Light Controller Plus
  vdjtelemetryclient.cpp

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

#include "vdjtelemetryclient.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

static const char *kLogTag = "[VdjTelemetry]";

// Only what DjFsm, VdjBridge and the DJ Manager read: VDJ sends a trigger on
// every change, so each one more is traffic (a VU meter streams at the tick rate).

// Per deck, as "deck N <trigger>". VDJ sends a tick's changed triggers in this
// order: the song's identity first, so a track change reaches consumers before
// its new position does
static const QStringList kDeckTriggers = {
    "get_filepath", "loaded",
    // Continuous
    "get_time", "get_time elapsed absolute", "get_beatpos",
    // On-load metadata; title_artist, album, genre and firstbeat mark a new file's fields
    "get_title", "get_artist", "get_title_artist",
    "get_album", "get_genre", "get_bpm", "get_key", "get_firstbeat",
    "get_time total", "play", "volume", "masterdeck",
};

// the real deck count; decks above it mirror VDJ's default deck
static const QStringList kGlobalTriggers = { "get_decks" };

VdjTelemetryClient::VdjTelemetryClient(QObject *parent)
    : QObject(parent)
{
    connect(&m_server, &QTcpServer::newConnection,
            this, &VdjTelemetryClient::onNewConnection);
}

VdjTelemetryClient::~VdjTelemetryClient()
{
    stop();
}

bool VdjTelemetryClient::start(quint16 port)
{
    if (m_server.isListening())
        stop();

    if (!m_server.listen(QHostAddress::Any, port))
    {
        qWarning() << kLogTag << "Failed to bind port" << port
                    << ":" << m_server.errorString();
        setStatus(PortInUse);
        return false;
    }

    qDebug() << kLogTag << "Listening on port" << port;
    setStatus(Listening);
    return true;
}

void VdjTelemetryClient::stop()
{
    const bool hadClient = m_client != nullptr;
    closeClient();
    if (m_server.isListening())
    {
        m_server.close();
        qDebug() << kLogTag << "Server stopped";
    }
    setStatus(Idle);
    // closing the input is a disconnect to its consumers, like VDJ going away
    if (hadClient)
        emit clientDisconnected();
}

QString VdjTelemetryClient::clientAddress() const
{
    if (m_client && m_client->state() == QAbstractSocket::ConnectedState)
        return QString("%1:%2").arg(m_client->peerAddress().toString()).arg(m_client->peerPort());
    return QString();
}

void VdjTelemetryClient::setStatus(Status s)
{
    if (m_status != s)
    {
        m_status = s;
        emit statusChanged(s);
    }
}

void VdjTelemetryClient::closeClient()
{
    if (m_client)
    {
        m_client->disconnect(this);
        m_client->abort();
        m_client->deleteLater();
        m_client = nullptr;
        m_buffer.clear();
        m_snapshotSeen = false;
    }
}

void VdjTelemetryClient::onNewConnection()
{
    QTcpSocket *pending = m_server.nextPendingConnection();
    if (!pending)
        return;

    // Replace existing client
    if (m_client)
    {
        qDebug() << kLogTag << "New connection replacing existing client";
        closeClient();
        emit clientDisconnected();
    }

    m_client = pending;
    qDebug() << kLogTag << "Client connected from"
             << m_client->peerAddress().toString()
             << ":" << m_client->peerPort();

    connect(m_client, &QTcpSocket::readyRead,
            this, &VdjTelemetryClient::onReadyRead);
    connect(m_client, &QTcpSocket::disconnected,
            this, &VdjTelemetryClient::onClientDisconnected);

    // Send subscription handshake
    QByteArray sub = buildSubscriptionMessage(kDefaultFrequency);
    m_client->write(sub);
    m_client->flush();

    setStatus(ClientConnected);
    emit clientConnected();
}

void VdjTelemetryClient::onReadyRead()
{
    if (m_client)
        feed(m_client->readAll());
}

void VdjTelemetryClient::feed(const QByteArray &chunk)
{
    m_buffer.append(chunk);
    int pos = 0;
    bool writeOpen = false; // objects dispatched since the last boundary
    while (pos < m_buffer.size())
    {
        const char c = m_buffer.at(pos);
        if (c == '\n' || c == '\r' || c == ' ' || c == '\t')
        {
            pos++;
            continue;
        }
        if (c != '{')
        {
            // not JSON: drop up to the next object
            const int next = m_buffer.indexOf('{', pos);
            qWarning() << kLogTag << "Discarding non-JSON bytes:" << m_buffer.mid(pos, next < 0 ? -1 : next - pos).left(120);
            pos = next < 0 ? m_buffer.size() : next;
            continue;
        }
        // the object ends at its closing brace, braces and quotes inside strings aside
        int end = -1, depth = 0;
        bool inString = false;
        for (int i = pos; i < m_buffer.size() && end < 0; i++)
        {
            const char b = m_buffer.at(i);
            if (inString)
            {
                if (b == '\\')
                    i++;
                else if (b == '"')
                    inString = false;
            }
            else if (b == '"')
                inString = true;
            else if (b == '{')
                depth++;
            else if (b == '}' && --depth == 0)
                end = i + 1;
        }
        if (end < 0)
            break; // the object goes on in the next chunk
        parseLine(m_buffer.mid(pos, end - pos));
        pos = end;
        writeOpen = true;
        // a write ends where the next one starts, adjacent
        if (pos < m_buffer.size() && m_buffer.at(pos) == '{')
        {
            writeOpen = false;
            emit tickEnded();
        }
    }
    m_buffer.remove(0, pos);
    // or where the data ends after its last complete object
    // shortcut: a write split across two reads ends here too (seen on no
    // loopback capture), upgrade to a settle timeout when VDJ runs over the LAN
    if (writeOpen && m_buffer.isEmpty())
        emit tickEnded();
}

void VdjTelemetryClient::onClientDisconnected()
{
    qDebug() << kLogTag << "Client disconnected";
    closeClient();
    setStatus(Listening);
    emit clientDisconnected();
}

QByteArray VdjTelemetryClient::buildSubscriptionMessage(int frequency)
{
    QJsonArray triggers;

    for (int deck = 1; deck <= kDeckCount; ++deck)
    {
        for (const QString &t : kDeckTriggers)
            triggers.append(QString("deck %1 %2").arg(deck).arg(t));
    }

    for (const QString &t : kGlobalTriggers)
        triggers.append(t);

    QJsonObject obj;
    obj["evt"] = "subscribe";
    obj["frequency"] = QString::number(frequency);
    obj["trigger"] = triggers;

    QByteArray json = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    json.append('\n');
    return json;
}

void VdjTelemetryClient::parseLine(const QByteArray &line)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(line, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
    {
        qWarning() << kLogTag << "Malformed JSON:" << err.errorString()
                    << "line:" << line.left(120);
        return;
    }

    QJsonObject obj = doc.object();
    QString evt = obj.value("evt").toString();

    if (evt == "beat")
    {
        // a reconnecting VDJ first drains every beat it queued while
        // disconnected (measured: 22,946 after 3 h), then sends the snapshot
        if (!m_snapshotSeen)
            return;
        int pos = obj.value("pos").toInt();
        qreal bpm = obj.value("bpm").toDouble();
        qreal strength = obj.value("strength").toDouble();
        bool change = obj.value("change").toBool();
        emit beatReceived(pos, bpm, strength, change);
        return;
    }
    m_snapshotSeen = true;

    if (evt == "subscribed")
    {
        QString trigger = obj.value("trigger").toString();
        // as JSON typed it: each consumer reads the trigger's own type
        const QVariant value = obj.value("value").toVariant();

        if (trigger.isEmpty())
            return;

        // Check if this is a per-deck trigger: "deck N <rest>"
        if (trigger.startsWith("deck ") && trigger.length() > 7)
        {
            bool ok = false;
            int deckNum = trigger.mid(5, 1).toInt(&ok);
            if (ok && deckNum >= 1 && deckNum <= kDeckCount)
            {
                // Extract the trigger name after "deck N "
                QString deckTrigger = trigger.mid(7);
                // Log song metadata triggers (infrequent, useful for debugging)
                if (deckTrigger.startsWith("get_title") || deckTrigger.startsWith("get_artist") ||
                    deckTrigger.startsWith("get_filepath") || deckTrigger == "get_bpm" ||
                    deckTrigger == "get_key" || deckTrigger == "loaded" || deckTrigger == "play" ||
                    deckTrigger == "masterdeck")
                    qDebug() << kLogTag << "Deck" << deckNum << deckTrigger << "=" << value;
                emit deckTriggerReceived(deckNum - 1, deckTrigger, value);
                return;
            }
        }

        // Global trigger
        emit globalTriggerReceived(trigger, value);
        return;
    }

    // Unknown event type — log once and ignore
    qDebug() << kLogTag << "Unknown event type:" << evt;
}
