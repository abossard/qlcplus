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

#include "ddpcontroller.h"

DDPController::DDPController(QNetworkInterface const& iface,
                             QNetworkAddressEntry const& address,
                             quint32 line, QObject *parent)
    : QObject(parent)
    , m_interface(iface)
    , m_ipAddr(address.ip())
    , m_line(line)
    , m_packetSent(0)
    , m_frameCount(0)
{
    m_udpSocket.reset(new QUdpSocket(this));
    m_udpSocket->bind(m_ipAddr, 0);

    qDebug() << "[DDP] Controller created on" << m_ipAddr.toString();
}

DDPController::~DDPController()
{
    qDebug() << "[DDP] Controller destroyed on" << m_ipAddr.toString();
    m_udpSocket->close();
}

void DDPController::sendDmx(quint32 universe, const QByteArray &data)
{
    QMutexLocker locker(&m_dataMutex);

    if (!m_universeMap.contains(universe))
    {
        qWarning() << Q_FUNC_INFO << "universe" << universe << "unknown";
        return;
    }

    DDPUniverseInfo const& info = m_universeMap[universe];

    // Deep-copy immediately: the engine passes a fromRawData() reference
    // that can be overwritten by the next timer tick while we queue it.
    QByteArray owned(data.constData(), data.size());
    QByteArray txData;

    if (info.transmissionMode == Full)
    {
        txData = QByteArray(512, 0);
        txData.replace(0, owned.length(), owned);
    }
    else
    {
        txData = owned;
    }

    // Find the first frame in the queue that doesn't have this universe yet
    int slot = -1;
    for (int i = 0; i < m_frameQueue.size(); i++)
    {
        if (!m_frameQueue[i].contains(universe))
        {
            slot = i;
            break;
        }
    }
    if (slot == -1)
    {
        m_frameQueue.append(QMap<quint32, QByteArray>());
        slot = m_frameQueue.size() - 1;
    }

    m_frameQueue[slot][universe] = txData;

    // Flush all complete frames from the front of the queue
    while (!m_frameQueue.isEmpty()
           && m_frameQueue.first().size() >= m_universeMap.size())
    {
        flushFrame(m_frameQueue.takeFirst());
    }
}

void DDPController::flushFrame(QMap<quint32, QByteArray> frame)
{
    // Collect universes sorted by DDP offset (ascending)
    QList<quint32> sortedUniverses = frame.keys();
    std::sort(sortedUniverses.begin(), sortedUniverses.end(),
        [this](quint32 a, quint32 b) {
            return m_universeMap[a].ddpOffset < m_universeMap[b].ddpOffset;
        });

    m_frameCount++;
    quint8 seq = DDPPacketizer::sequenceForFrame(m_frameCount);

    int totalUniverses = sortedUniverses.size();

    for (int u = 0; u < totalUniverses; u++)
    {
        quint32 universe = sortedUniverses[u];
        DDPUniverseInfo const& info = m_universeMap[universe];
        QByteArray const& txData = frame[universe];

        if (txData.isEmpty())
            continue;

        quint8 dataType = (info.components == RGBW)
            ? DDP_DATATYPE_RGBW888
            : DDP_DATATYPE_RGB888;

        bool isLastUniverse = (u == totalUniverses - 1);
        int totalPackets = DDPPacketizer::packetsRequired(txData.size());

        for (int i = 0; i < totalPackets; i++)
        {
            int chunkStart = i * DDP_MAX_DATALEN;
            int chunkLen = qMin(DDP_MAX_DATALEN, txData.size() - chunkStart);
            bool isLastChunk = (i == totalPackets - 1);
            bool push = isLastUniverse && isLastChunk;

            QByteArray chunk = txData.mid(chunkStart, chunkLen);
            QByteArray packet = DDPPacketizer::buildPacket(
                chunk,
                info.ddpOffset + static_cast<quint32>(chunkStart),
                seq, push, dataType, info.destId);

            qint64 sent = m_udpSocket->writeDatagram(
                packet.data(), packet.size(),
                info.destAddress, info.destPort);

            if (sent < 0)
            {
                qWarning() << "[DDP] flushFrame failed:" << m_udpSocket->errorString();
                break;
            }
            m_packetSent++;
        }
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
        info.destAddress = QHostAddress::Broadcast;
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
    return m_universeMap.keys();
}

DDPUniverseInfo *DDPController::getUniverseInfo(quint32 universe)
{
    if (m_universeMap.contains(universe))
        return &m_universeMap[universe];
    return nullptr;
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
