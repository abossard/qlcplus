/*
  Q Light Controller Plus
  ddpcontroller.h

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

#ifndef DDPCONTROLLER_H
#define DDPCONTROLLER_H

#include <QNetworkInterface>
#include <QHostAddress>
#include <QUdpSocket>
#include <QByteArray>
#include <QMutex>
#include <QMap>

#include "ddppacketizer.h"

/** Per-universe output configuration */
typedef struct
{
    QHostAddress destAddress;
    quint16 destPort;
    quint8 destId;
    quint32 ddpOffset;        // byte offset into the device's pixel buffer
    int transmissionMode;     // 0 = Full (512 ch), 1 = Partial
    int components;           // 0 = RGB (3 bytes/pixel), 1 = RGBW (4 bytes/pixel)
} DDPUniverseInfo;

class DDPController final : public QObject
{
    Q_OBJECT

public:
    enum TransmissionMode { Full, Partial };
    enum Components { RGB, RGBW };

    explicit DDPController(QNetworkInterface const& iface,
                           QNetworkAddressEntry const& address,
                           quint32 line, QObject *parent = nullptr);
    ~DDPController();

    /** Send DMX data for a specific QLC+ universe */
    void sendDmx(quint32 universe, const QByteArray &data);

    /** Return the controller IP address */
    QString getNetworkIP() const;

    /** Add a universe to this controller's output map */
    void addUniverse(quint32 universe);

    /** Remove a universe from this controller's output map */
    void removeUniverse(quint32 universe);

    /** Return the list of universes handled by this controller */
    QList<quint32> universesList() const;

    /** Return per-universe info (or nullptr if not found) */
    DDPUniverseInfo *getUniverseInfo(quint32 universe);

    // Per-universe setters
    void setDestAddress(quint32 universe, const QString &address);
    void setDestPort(quint32 universe, quint16 port);
    void setDestId(quint32 universe, quint8 id);
    void setDDPOffset(quint32 universe, quint32 offset);
    void setTransmissionMode(quint32 universe, TransmissionMode mode);
    void setComponents(quint32 universe, Components components);

    static QString transmissionModeToString(TransmissionMode mode);
    static TransmissionMode stringToTransmissionMode(const QString &mode);
    static QString componentsToString(Components components);
    static Components stringToComponents(const QString &str);

    /** Return the plugin line associated to this controller */
    quint32 line() const;

    /** Get the number of packets sent by this controller */
    quint64 getPacketSentNumber() const;

private:
    QNetworkInterface m_interface;
    QHostAddress m_ipAddr;
    quint32 m_line;

    QSharedPointer<QUdpSocket> m_udpSocket;
    QMap<quint32, DDPUniverseInfo> m_universeMap;
    QMutex m_dataMutex;

    quint64 m_packetSent;
    quint64 m_frameCount;
};

#endif // DDPCONTROLLER_H
