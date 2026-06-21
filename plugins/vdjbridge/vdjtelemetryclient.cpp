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

// --- Per-deck triggers ---
// These are subscribed for each of 4 decks as "deck N <trigger>".

static const QStringList kDeckTriggers = {
    // Continuous
    "get_position", "get_time", "get_time elapsed absolute",
    "get_beatpos", "get_vu_meter", "level",
    // On-load metadata
    "get_filepath", "get_title", "get_artist", "get_title_artist",
    "get_album", "get_genre", "get_bpm", "get_key", "get_firstbeat",
    "get_time total", "loaded", "play", "volume",
    // EQ
    "eq_high", "eq_med", "eq_low", "gain",
    // Loop
    "loop", "get_loop",
};

// --- Global triggers ---
static const QStringList kGlobalTriggers = {
    "master_volume", "get_decks", "crossfader", "headphone_volume",
    "masterdeck", "get_crossfader_result full", "mixer_order", "get_vu_meter",
};

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
    closeClient();
    if (m_server.isListening())
    {
        m_server.close();
        qDebug() << kLogTag << "Server stopped";
    }
    setStatus(Idle);
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
        m_lineBuffer.clear();
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
    if (!m_client)
        return;

    m_lineBuffer.append(m_client->readAll());

    // VDJ sends JSON objects concatenated without separators: {...}{...}{...}
    // Also sometimes newline-delimited. Split on both patterns.
    // Strategy: replace }{ with }\n{ then split on \n.
    m_lineBuffer.replace("}{", "}\n{");

    int newlinePos;
    while ((newlinePos = m_lineBuffer.indexOf('\n')) >= 0)
    {
        QByteArray line = m_lineBuffer.left(newlinePos).trimmed();
        m_lineBuffer.remove(0, newlinePos + 1);

        if (!line.isEmpty())
            parseLine(line);
    }

    // Whatever remains in the buffer is a partial JSON object — keep it
    // for the next readyRead call.
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

    // Per-deck triggers for decks 1–4
    for (int deck = 1; deck <= kDeckCount; ++deck)
    {
        for (const QString &t : kDeckTriggers)
            triggers.append(QString("deck %1 %2").arg(deck).arg(t));
    }

    // Global triggers
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

// Helper: coerce a QJsonValue to a QVariant defensively
static QVariant coerceValue(const QJsonValue &v)
{
    if (v.isDouble())
        return v.toDouble();
    if (v.isBool())
        return v.toBool();
    if (v.isString())
    {
        const QString s = v.toString();
        // Try numeric coercion
        bool ok = false;
        double d = s.toDouble(&ok);
        if (ok) return d;
        // Try bool coercion
        if (s.compare("true", Qt::CaseInsensitive) == 0) return true;
        if (s.compare("false", Qt::CaseInsensitive) == 0) return false;
        return s;
    }
    return QVariant();
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
        int pos = obj.value("pos").toInt();
        qreal bpm = obj.value("bpm").toDouble();
        qreal strength = obj.value("strength").toDouble();
        bool change = obj.value("change").toBool();
        emit beatReceived(pos, bpm, strength, change);
        return;
    }

    if (evt == "subscribed")
    {
        QString trigger = obj.value("trigger").toString();
        QVariant value = coerceValue(obj.value("value"));

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
                    deckTrigger == "get_key" || deckTrigger == "loaded" || deckTrigger == "play")
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
