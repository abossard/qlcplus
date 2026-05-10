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
#include <QElapsedTimer>
#include <QMutex>
#include <QMap>
#include <atomic>

#include "ddppacketizer.h"

/** Per-universe output configuration */
struct DDPUniverseInfo
{
    QHostAddress destAddress;
    quint16 destPort;
    quint8 destId;
    quint32 ddpOffset;        // byte offset into the device's pixel buffer
    int transmissionMode;     // Stored for XML compat but currently unused in packet output
    int components;           // 0 = RGB (3 bytes/pixel), 1 = RGBW (4 bytes/pixel)
    qint64 lastSendElapsed = 0;      // per-universe rate limit timestamp
    qint64 lastSendDataElapsed = 0;  // per-universe keepalive timestamp
};

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

    /** Send DMX data for a specific QLC+ universe.
     *  When dataChanged is false, the send is suppressed unless the
     *  keep-alive interval (kKeepAliveMs) has elapsed since the last send. */
    void sendDmx(quint32 universe, const QByteArray &data, bool dataChanged = true);

    /** Return the controller IP address */
    QString getNetworkIP() const;

    /** Add a universe to this controller's output map */
    void addUniverse(quint32 universe);

    /** Remove a universe from this controller's output map */
    void removeUniverse(quint32 universe);

    /** Return the list of universes handled by this controller */
    QList<quint32> universesList() const;

    /** Return a thread-safe copy of per-universe info. `found` (if non-null)
     *  is set to true if the universe exists, false otherwise. */
    DDPUniverseInfo getUniverseInfo(quint32 universe, bool *found = nullptr) const;

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

    /** Maximum frames per second sent over the wire (0 = no limit). */
    void setMaxFps(int fps);
    int maxFps() const;

    /** Explicit pixel count (0 = auto: send exactly what the engine produced). */
    void setPixelCount(int pixels);
    int pixelCount() const;

    /** Skip sending when data is unchanged (saves bandwidth on Wi-Fi). */
    void setSkipUnchanged(bool skip);
    bool skipUnchanged() const;

    /** Keep-alive interval: re-send unchanged data at least every kKeepAliveMs. */
    static constexpr qint64 kKeepAliveMs = 1000;

private:
    QHostAddress m_ipAddr;
    quint32 m_line;

    QSharedPointer<QUdpSocket> m_udpSocket;
    QMap<quint32, DDPUniverseInfo> m_universeMap;
    mutable QMutex m_dataMutex;

    std::atomic<quint64> m_packetSent{0};
    std::atomic<quint64> m_frameCount{0};

    // Default 20 FPS: DDP over Wi-Fi (e.g. WLED) doesn't benefit from more
    // than ~20–30 FPS. Users can raise this in the plugin config dialog.
    // The throttle is enforced per-universe inside DDPController::sendDmx via
    // DDPUniverseInfo::lastSendElapsed; the universe thread, MasterTimer, and
    // other output plugins are unaffected.
    static constexpr int kMaxFpsLimit = 50;
    static constexpr int kDefaultFps = 20;
public:
    static constexpr int maxFpsLimit() { return kMaxFpsLimit; }
    static constexpr int defaultFps() { return kDefaultFps; }
private:
    int m_maxFps = kDefaultFps;
    bool m_skipUnchanged = false;
    QElapsedTimer m_sendTimer;

    // Explicit pixel framing — 0 means "auto/legacy" (send what the engine produced)
    int m_pixelCount = 0;
};

#endif // DDPCONTROLLER_H
