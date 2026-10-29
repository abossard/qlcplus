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
#include <QTimer>

/**
 * TCP server implementing the DMXDesktop telemetry protocol for VirtualDJ.
 *
 * Listens on a configurable port. When VDJ connects, sends
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

    static const int kDefaultFrequency = 25;
    static const int kDeckCount = 4;
    /** Quiet after a complete object that ends a write (ms): a read can end
     *  inside a write, a LAN splits one; the next write's start ends it sooner */
    static const int kSettleMs = 10;

    explicit VdjTelemetryClient(QObject *parent = nullptr);
    ~VdjTelemetryClient() override;

    /** Start listening on the given port. Returns false if port bind fails. */
    bool start(quint16 port);

    /** Stop listening; a connected client is dropped and clientDisconnected() emitted. */
    void stop();

    Status status() const { return m_status; }

    /** Return the connected client's address:port string, or empty. */
    QString clientAddress() const;

    /** The subscription: the triggers QLC+ reads, for each of kDeckCount decks */
    static QByteArray buildSubscriptionMessage(int frequency = kDefaultFrequency);

    /** Parse a single NDJSON line. Exposed for testing. */
    void parseLine(const QByteArray &line);

    /** One chunk as the socket delivers it. VDJ makes one write per tick: its
     *  changed triggers joined by newlines; writes sit adjacent ("}{"), also
     *  across chunks; a chunk may hold several writes and may end inside a
     *  write or an object. Objects are cut by brace depth, so a "}{" inside a
     *  title is text. tickEnded() follows the objects of each write: at the
     *  next write's adjacent "{", or kSettleMs after data that ended after a
     *  complete object. Exposed so tests replay captures without a socket. */
    void feed(const QByteArray &chunk);

    /** Ends the write whose data ended after a complete object now, as
     *  kSettleMs without data does; replays call it where the socket went quiet */
    void settle();

signals:
    /** Per-deck trigger update. deckIndex is 0-based (0–3). */
    void deckTriggerReceived(int deckIndex, const QString &trigger, const QVariant &value);

    /** Global trigger update. */
    void globalTriggerReceived(const QString &trigger, const QVariant &value);

    /** Beat event from VDJ. */
    void beatReceived(int pos, qreal bpm, qreal strength, bool change);

    /** Client connected / disconnected. */
    void clientConnected();
    void clientDisconnected();

    /** The objects of one VDJ write (one tick) have all been dispatched */
    void tickEnded();

private slots:
    void onNewConnection();
    void onReadyRead();
    void onClientDisconnected();

private:
    void closeClient();

    QTcpServer m_server;
    QTcpSocket *m_client = nullptr;
    QByteArray m_buffer;
    /** Objects dispatched since the last write boundary */
    bool m_writeOpen = false;
    /** The buffer starts right after the last of them: a "{" there is the next write */
    bool m_afterObject = false;
    QTimer m_settleTimer;
    /** The connection's first "subscribed" object (the snapshot) is in: its
     *  beats are live, the ones before it are VDJ's backlog */
    bool m_subscriptionAnswered = false;
    Status m_status = Idle;
};

#endif // VDJTELEMETRYCLIENT_H
