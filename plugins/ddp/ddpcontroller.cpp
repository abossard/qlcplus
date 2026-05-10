/*
  Q Light Controller Plus
  ddpcontroller.cpp

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

#include <QDebug>
#include <QLoggingCategory>

#include "ddpcontroller.h"

// DDP UDP write failures (interface flap, no listener, blocked datagram) are
// expected at runtime and must NOT spam qWarning. Filter via
// QT_LOGGING_RULES="qlcplus.plugins.ddp.debug=true" to inspect.
Q_LOGGING_CATEGORY(ddpLog, "qlcplus.plugins.ddp")

DDPController::DDPController(QNetworkInterface const& iface,
                             QNetworkAddressEntry const& address,
                             quint32 line, QObject *parent)
    : QObject(parent)
    , m_interface(iface)
    , m_ipAddr(address.ip())
    , m_line(line)
{
    m_udpSocket.reset(new QUdpSocket());  // no QObject parent — QSharedPointer owns it
    m_udpSocket->bind(m_ipAddr, 0);

    // Monotonic clock for FPS throttling and keep-alive (Fix 2/3).
    m_sendTimer.start();

    qDebug() << "[DDP] Controller created on" << m_ipAddr.toString();
}

DDPController::~DDPController()
{
    qDebug() << "[DDP] Controller destroyed on" << m_ipAddr.toString();
    m_udpSocket->close();
}

void DDPController::sendDmx(quint32 universe, const QByteArray &data, bool dataChanged)
{
    // Hold m_dataMutex for the entire send: protects m_universeMap as well as
    // serialising socket writes (multiple universe threads may share this
    // controller via QSharedPointer).
    QMutexLocker locker(&m_dataMutex);

    if (!m_universeMap.contains(universe))
    {
        qWarning() << Q_FUNC_INFO << "universe" << universe << "unknown";
        return;
    }

    DDPUniverseInfo &info = m_universeMap[universe];

    // FPS throttle — per-universe, monotonic clock.
    const qint64 now = m_sendTimer.elapsed();
    const qint64 minInterval = (m_maxFps > 0) ? (1000 / m_maxFps) : 0;
    if (minInterval > 0 && (now - info.lastSendElapsed) < minInterval)
        return;

    // Skip unchanged data to save bandwidth (disabled by default).
    // When enabled, a keep-alive is still sent every kKeepAliveMs so
    // receivers don't lose state on packet loss.
    if (m_skipUnchanged && !dataChanged && (now - info.lastSendDataElapsed) < kKeepAliveMs)
        return;

    info.lastSendElapsed = now;
    info.lastSendDataElapsed = now;

    // Unicast only (Fix 5): skip silently in debug log if no destination IP.
    if (info.destAddress.isNull())
    {
        qCDebug(ddpLog) << "sendDmx: skipping universe" << universe
                        << "no destination IP configured";
        return;
    }

    // Build txData (Fix 6) — exact pixel length, never pad to 512.
    QByteArray txData;
    if (m_pixelCount > 0)
    {
        // Explicit pixel count: send exactly pixelCount * bpp.
        const int bpp = (m_bytesPerPixel > 0) ? m_bytesPerPixel : 3;
        const int targetLen = m_pixelCount * bpp;
        txData = data.left(targetLen);
        if (txData.size() < targetLen)
            txData.append(QByteArray(targetLen - txData.size(), 0));
    }
    else
    {
        // Auto: send exactly what the engine produced.
        txData = data;
    }

    if (txData.isEmpty())
        return;

    // Send this universe immediately. Each universe thread arrives here
    // independently after MasterTimer's per-universe QueuedConnection tick;
    // batching across universes is impossible to do reliably without a
    // cross-thread barrier, and DDP receivers (WLED etc.) handle a PUSH per
    // universe just fine.
    m_frameCount++;
    quint8 seq = DDPPacketizer::sequenceForFrame(m_frameCount);

    quint8 dataType = (info.components == RGBW)
        ? DDP_DATATYPE_RGBW888
        : DDP_DATATYPE_RGB888;

    int totalPackets = DDPPacketizer::packetsRequired(txData.size());

    for (int i = 0; i < totalPackets; i++)
    {
        int chunkStart = i * DDP_MAX_DATALEN;
        int chunkLen = qMin(DDP_MAX_DATALEN, txData.size() - chunkStart);
        bool isLastChunk = (i == totalPackets - 1);
        bool push = isLastChunk;

        QByteArray chunk = txData.mid(chunkStart, chunkLen);
        QByteArray packet = DDPPacketizer::buildPacket(
            chunk,
            info.ddpOffset + static_cast<quint32>(chunkStart),
            seq, push, dataType, info.destId);

        // UDP is fire-and-forget. If the datagram fails, drop the rest of
        // the frame and move on — the next tick will try again. Logged at
        // debug level only; transient failures are expected.
        qint64 sent = m_udpSocket->writeDatagram(
            packet.data(), packet.size(),
            info.destAddress, info.destPort);

        if (sent < 0)
        {
            qCDebug(ddpLog) << "sendDmx: writeDatagram failed for universe"
                            << universe << ":" << m_udpSocket->errorString();
            break;
        }
        m_packetSent++;
    }
}

QString DDPController::getNetworkIP() const
{
    return m_ipAddr.toString();
}

void DDPController::addUniverse(quint32 universe)
{
    QMutexLocker locker(&m_dataMutex);

    if (!m_universeMap.contains(universe))
    {
        DDPUniverseInfo info;
        // Unicast by default (Fix 5): null = "user must configure".
        // sendDmx() will skip silently (logged via qlcplus.plugins.ddp).
        info.destAddress = QHostAddress();
        info.destPort = DDP_PORT;
        info.destId = DDP_DEST_DEFAULT;
        info.ddpOffset = 0;
        info.transmissionMode = Full;
        info.components = RGB;
        m_universeMap[universe] = info;
    }
}

void DDPController::removeUniverse(quint32 universe)
{
    QMutexLocker locker(&m_dataMutex);
    m_universeMap.remove(universe);
}

QList<quint32> DDPController::universesList() const
{
    QMutexLocker locker(&m_dataMutex);
    return m_universeMap.keys();
}

DDPUniverseInfo DDPController::getUniverseInfo(quint32 universe, bool *found) const
{
    QMutexLocker locker(&m_dataMutex);
    auto it = m_universeMap.constFind(universe);
    if (it == m_universeMap.constEnd())
    {
        if (found) *found = false;
        return DDPUniverseInfo{};
    }
    if (found) *found = true;
    return it.value();
}

void DDPController::setDestAddress(quint32 universe, const QString &address)
{
    QMutexLocker locker(&m_dataMutex);
    if (m_universeMap.contains(universe))
        m_universeMap[universe].destAddress = QHostAddress(address);
}

void DDPController::setDestPort(quint32 universe, quint16 port)
{
    QMutexLocker locker(&m_dataMutex);
    if (m_universeMap.contains(universe))
        m_universeMap[universe].destPort = port;
}

void DDPController::setDestId(quint32 universe, quint8 id)
{
    QMutexLocker locker(&m_dataMutex);
    if (m_universeMap.contains(universe))
        m_universeMap[universe].destId = id;
}

void DDPController::setDDPOffset(quint32 universe, quint32 offset)
{
    QMutexLocker locker(&m_dataMutex);
    if (m_universeMap.contains(universe))
        m_universeMap[universe].ddpOffset = offset;
}

void DDPController::setTransmissionMode(quint32 universe, TransmissionMode mode)
{
    QMutexLocker locker(&m_dataMutex);
    if (m_universeMap.contains(universe))
        m_universeMap[universe].transmissionMode = mode;
}

void DDPController::setComponents(quint32 universe, Components components)
{
    QMutexLocker locker(&m_dataMutex);
    if (m_universeMap.contains(universe))
        m_universeMap[universe].components = components;
}

QString DDPController::transmissionModeToString(TransmissionMode mode)
{
    switch (mode)
    {
        case Full: return "Full";
        case Partial: return "Partial";
    }
    return "Full";
}

DDPController::TransmissionMode DDPController::stringToTransmissionMode(const QString &mode)
{
    if (mode == "Partial")
        return Partial;
    return Full;
}

QString DDPController::componentsToString(Components components)
{
    switch (components)
    {
        case RGBW: return "RGBW";
        default: return "RGB";
    }
}

DDPController::Components DDPController::stringToComponents(const QString &str)
{
    if (str == "RGBW")
        return RGBW;
    return RGB;
}

quint32 DDPController::line() const
{
    return m_line;
}

quint64 DDPController::getPacketSentNumber() const
{
    return m_packetSent;
}

void DDPController::setMaxFps(int fps)
{
    QMutexLocker locker(&m_dataMutex);
    m_maxFps = qBound(0, fps, kMaxFpsLimit);
}

int DDPController::maxFps() const
{
    QMutexLocker locker(&m_dataMutex);
    return m_maxFps;
}

void DDPController::setPixelCount(int pixels)
{
    QMutexLocker locker(&m_dataMutex);
    m_pixelCount = qMax(0, pixels);
}

int DDPController::pixelCount() const
{
    QMutexLocker locker(&m_dataMutex);
    return m_pixelCount;
}

void DDPController::setBytesPerPixel(int bpp)
{
    QMutexLocker locker(&m_dataMutex);
    m_bytesPerPixel = (bpp > 0) ? bpp : 3;
}

int DDPController::bytesPerPixel() const
{
    QMutexLocker locker(&m_dataMutex);
    return m_bytesPerPixel;
}

void DDPController::setSkipUnchanged(bool skip)
{
    QMutexLocker locker(&m_dataMutex);
    m_skipUnchanged = skip;
}

bool DDPController::skipUnchanged() const
{
    QMutexLocker locker(&m_dataMutex);
    return m_skipUnchanged;
}
