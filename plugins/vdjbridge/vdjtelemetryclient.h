/*
  Q Light Controller Plus
  vdjtelemetryclient.h

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

#ifndef VDJTELEMETRYCLIENT_H
#define VDJTELEMETRYCLIENT_H

#include <QTcpServer>
#include <QTcpSocket>
#include <QByteArray>
#include <QObject>

class VdjDeckModel;

/**
 * TCP server implementing the DMXDesktop telemetry protocol for VirtualDJ.
 *
 * Listens on a configurable port (default 8050). When VDJ connects, sends
 * a subscription handshake then parses incoming NDJSON frames and dispatches
 * typed signals for deck updates, beat events, and global mixer state.
 *
 * Only one client at a time — a new connection replaces the old one.
 * Partial NDJSON lines across TCP reads are handled via a line buffer.
 */
class VdjTelemetryClient : public QObject
{
    Q_OBJECT

public:
    enum Status {
        Idle,           ///< Server not started
        Listening,      ///< Listening for connections
        PortInUse,      ///< Port bind failed
        ClientConnected ///< VDJ client connected
    };
    Q_ENUM(Status)

    static const quint16 kDefaultPort = 8050;
    static const int kDefaultFrequency = 25;
    static const int kDeckCount = 4;

    explicit VdjTelemetryClient(QObject *parent = nullptr);
    ~VdjTelemetryClient() override;

    /** Start listening on the given port. Returns false if port bind fails. */
    bool start(quint16 port = kDefaultPort);

    /** Stop listening and disconnect any client. */
    void stop();

    Status status() const { return m_status; }

    /** Return the connected client's address:port string, or empty. */
    QString clientAddress() const;

    /** Build the 112-trigger subscription JSON payload. */
    static QByteArray buildSubscriptionMessage(int frequency = kDefaultFrequency);

    /** Parse a single NDJSON line. Exposed for testing. */
    void parseLine(const QByteArray &line);

signals:
    void statusChanged(VdjTelemetryClient::Status status);

    /** Per-deck trigger update. deckIndex is 0-based (0–3). */
    void deckTriggerReceived(int deckIndex, const QString &trigger, const QVariant &value);

    /** Global trigger update. */
    void globalTriggerReceived(const QString &trigger, const QVariant &value);

    /** Beat event from VDJ. */
    void beatReceived(int pos, qreal bpm, qreal strength, bool change);

    /** Client connected / disconnected. */
    void clientConnected();
    void clientDisconnected();

private slots:
    void onNewConnection();
    void onReadyRead();
    void onClientDisconnected();

private:
    void setStatus(Status s);
    void closeClient();

    QTcpServer m_server;
    QTcpSocket *m_client = nullptr;
    QByteArray m_lineBuffer;
    Status m_status = Idle;
};

#endif // VDJTELEMETRYCLIENT_H
