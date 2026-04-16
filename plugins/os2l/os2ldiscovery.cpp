/*
  Q Light Controller Plus
  os2ldiscovery.cpp

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

#include "os2ldiscovery.h"
#include <QNetworkDatagram>
#include <QDateTime>
#include <QDebug>

const char* OS2LDiscovery::MDNS_ADDR = "224.0.0.251";

OS2LDiscovery::OS2LDiscovery(QObject *parent)
    : QObject(parent)
    , m_socket(nullptr)
    , m_queryTimer(nullptr)
    , m_timeoutTimer(nullptr)
    , m_active(false)
{
}

OS2LDiscovery::~OS2LDiscovery()
{
    stopDiscovery();
}

bool OS2LDiscovery::startDiscovery()
{
    if (m_active)
        return true;

    qDebug() << "[OS2L Discovery] Starting mDNS service discovery for _os2l._tcp.local.";

    m_socket = new QUdpSocket(this);

    // Bind to mDNS port and join multicast group
    if (!m_socket->bind(QHostAddress::AnyIPv4, MDNS_PORT, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint))
    {
        qWarning() << "[OS2L Discovery] Failed to bind to mDNS port" << MDNS_PORT;
        delete m_socket;
        m_socket = nullptr;
        return false;
    }

    if (!m_socket->joinMulticastGroup(QHostAddress(MDNS_ADDR)))
    {
        qWarning() << "[OS2L Discovery] Failed to join mDNS multicast group";
        m_socket->close();
        delete m_socket;
        m_socket = nullptr;
        return false;
    }

    connect(m_socket, &QUdpSocket::readyRead, this, &OS2LDiscovery::processPendingDatagrams);

    // Setup periodic query timer
    m_queryTimer = new QTimer(this);
    connect(m_queryTimer, &QTimer::timeout, this, &OS2LDiscovery::sendQuery);
    m_queryTimer->start(QUERY_INTERVAL_MS);

    // Setup service timeout checker
    m_timeoutTimer = new QTimer(this);
    connect(m_timeoutTimer, &QTimer::timeout, this, &OS2LDiscovery::checkServiceTimeout);
    m_timeoutTimer->start(5000); // Check every 5 seconds

    m_active = true;

    // Send initial query
    sendQuery();

    qDebug() << "[OS2L Discovery] mDNS discovery started successfully";
    return true;
}

void OS2LDiscovery::stopDiscovery()
{
    if (!m_active)
        return;

    qDebug() << "[OS2L Discovery] Stopping mDNS service discovery";

    if (m_queryTimer)
    {
        m_queryTimer->stop();
        delete m_queryTimer;
        m_queryTimer = nullptr;
    }

    if (m_timeoutTimer)
    {
        m_timeoutTimer->stop();
        delete m_timeoutTimer;
        m_timeoutTimer = nullptr;
    }

    if (m_socket)
    {
        m_socket->leaveMulticastGroup(QHostAddress(MDNS_ADDR));
        m_socket->close();
        delete m_socket;
        m_socket = nullptr;
    }

    m_services.clear();
    m_serviceLastSeen.clear();
    m_active = false;
}

void OS2LDiscovery::sendQuery()
{
    if (!m_socket)
        return;

    // Construct mDNS query for _os2l._tcp.local.
    QByteArray query;

    // DNS Header
    query.append('\x00'); query.append('\x00'); // Transaction ID
    query.append('\x00'); query.append('\x00'); // Flags: Standard query
    query.append('\x00'); query.append('\x01'); // Questions: 1
    query.append('\x00'); query.append('\x00'); // Answer RRs: 0
    query.append('\x00'); query.append('\x00'); // Authority RRs: 0
    query.append('\x00'); query.append('\x00'); // Additional RRs: 0

    // Question: _os2l._tcp.local.
    query.append('\x05'); query.append("_os2l");  // Label: _os2l
    query.append('\x04'); query.append("_tcp");   // Label: _tcp
    query.append('\x05'); query.append("local");  // Label: local
    query.append('\x00');                          // End of name

    query.append('\x00'); query.append('\x0c');   // Type: PTR
    query.append('\x00'); query.append('\x01');   // Class: IN

    qint64 sent = m_socket->writeDatagram(query, QHostAddress(MDNS_ADDR), MDNS_PORT);
    if (sent < 0)
    {
        qWarning() << "[OS2L Discovery] Failed to send mDNS query:" << m_socket->errorString();
    }
    else
    {
        qDebug() << "[OS2L Discovery] Sent mDNS query for _os2l._tcp.local. (" << sent << "bytes)";
    }
}

void OS2LDiscovery::processPendingDatagrams()
{
    while (m_socket && m_socket->hasPendingDatagrams())
    {
        QNetworkDatagram datagram = m_socket->receiveDatagram();
        QByteArray data = datagram.data();
        QHostAddress sender = datagram.senderAddress();

        qDebug() << "[OS2L Discovery] Received mDNS packet from" << sender.toString()
                 << "(" << data.size() << "bytes)";

        parseResponse(data, sender);
    }
}

void OS2LDiscovery::parseResponse(const QByteArray &data, const QHostAddress &sender)
{
    if (data.size() < 12)
        return;

    // Parse DNS header
    quint16 flags = (static_cast<quint8>(data[2]) << 8) | static_cast<quint8>(data[3]);
    bool isResponse = (flags & 0x8000) != 0;

    if (!isResponse)
        return; // Not a response

    quint16 questions = (static_cast<quint8>(data[4]) << 8) | static_cast<quint8>(data[5]);
    quint16 answers = (static_cast<quint8>(data[6]) << 8) | static_cast<quint8>(data[7]);

    qDebug() << "[OS2L Discovery] DNS response: questions=" << questions << "answers=" << answers;

    if (answers == 0)
        return;

    // Simple heuristic: if we see _os2l in the packet, assume it's a valid service
    QString dataStr = QString::fromUtf8(data);
    if (dataStr.contains("_os2l", Qt::CaseInsensitive) ||
        dataStr.contains("os2l", Qt::CaseInsensitive))
    {
        qDebug() << "[OS2L Discovery] Found OS2L service announcement from" << sender.toString();

        // Try to extract port information (look for common OS2L port 9996 or parse SRV record)
        // For simplicity, we'll use the default port
        quint16 port = 9996;

        ServiceInfo service;
        service.name = QString("OS2L@%1").arg(sender.toString());
        service.address = sender;
        service.port = port;
        service.hostName = sender.toString();

        // Check if this is a new service
        bool isNew = true;
        for (const ServiceInfo &existing : m_services)
        {
            if (existing.address == service.address)
            {
                isNew = false;
                break;
            }
        }

        if (isNew)
        {
            qDebug() << "[OS2L Discovery] *** NEW OS2L SERVICE DISCOVERED ***";
            qDebug() << "[OS2L Discovery]   Name:" << service.name;
            qDebug() << "[OS2L Discovery]   Address:" << service.address.toString();
            qDebug() << "[OS2L Discovery]   Port:" << service.port;
            m_services.append(service);
            emit serviceDiscovered(service);
        }

        // Update last seen timestamp
        m_serviceLastSeen[service.name] = QDateTime::currentMSecsSinceEpoch();
    }
}

void OS2LDiscovery::checkServiceTimeout()
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    QStringList toRemove;

    for (auto it = m_serviceLastSeen.begin(); it != m_serviceLastSeen.end(); ++it)
    {
        if (now - it.value() > SERVICE_TIMEOUT_MS)
        {
            toRemove.append(it.key());
        }
    }

    for (const QString &serviceName : toRemove)
    {
        qDebug() << "[OS2L Discovery] Service timed out:" << serviceName;
        m_serviceLastSeen.remove(serviceName);

        // Remove from services list
        for (int i = 0; i < m_services.size(); ++i)
        {
            if (m_services[i].name == serviceName)
            {
                m_services.removeAt(i);
                break;
            }
        }

        emit serviceRemoved(serviceName);
    }
}

QString OS2LDiscovery::extractServiceName(const QByteArray &data, int offset, int &newOffset)
{
    QString name;
    int pos = offset;

    while (pos < data.size())
    {
        quint8 len = static_cast<quint8>(data[pos]);

        if (len == 0)
        {
            newOffset = pos + 1;
            break;
        }

        if (len >= 0xc0) // Compressed name pointer
        {
            newOffset = pos + 2;
            break;
        }

        pos++;
        if (pos + len > data.size())
            break;

        if (!name.isEmpty())
            name += ".";

        name += QString::fromUtf8(data.mid(pos, len));
        pos += len;
    }

    return name;
}
