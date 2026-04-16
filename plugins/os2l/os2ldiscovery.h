/*
  Q Light Controller Plus
  os2ldiscovery.h

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

#ifndef OS2LDISCOVERY_H
#define OS2LDISCOVERY_H

#include <QObject>
#include <QUdpSocket>
#include <QTimer>
#include <QHostAddress>

/**
 * @brief OS2L Service Discovery using mDNS/Bonjour
 *
 * This class implements mDNS-based service discovery for OS2L hosts.
 * It discovers VirtualDJ or other OS2L-compatible software on the local network
 * by querying and listening for _os2l._tcp.local. service announcements.
 *
 * Protocol references:
 *  - OS2L service type "_os2l._tcp": https://os2l.org
 *  - mDNS (RFC 6762): https://tools.ietf.org/html/rfc6762
 *    Defines UDP port 5353 and multicast group 224.0.0.251 used here.
 *  - DNS-SD (RFC 6763): https://tools.ietf.org/html/rfc6763
 *    Defines the PTR query pattern and "_service._tcp.local." naming convention.
 *  - DNS wire format (RFC 1035): https://tools.ietf.org/html/rfc1035
 *    Binary packet structure used in sendQuery().
 *  - Apple Bonjour (native macOS): https://developer.apple.com/bonjour/
 *    For production macOS use, consider replacing this implementation with
 *    DNSServiceBrowse / DNSServiceResolve from <dns_sd.h>.
 */
class OS2LDiscovery : public QObject
{
    Q_OBJECT

public:
    struct ServiceInfo
    {
        QString name;           // Service instance name
        QHostAddress address;   // IP address
        quint16 port;          // Port number
        QString hostName;       // Hostname
        QMap<QString, QString> txtRecords; // TXT record key-value pairs
    };

    explicit OS2LDiscovery(QObject *parent = nullptr);
    virtual ~OS2LDiscovery();

    /** Start mDNS service discovery */
    bool startDiscovery();

    /** Stop mDNS service discovery */
    void stopDiscovery();

    /** Check if discovery is active */
    bool isActive() const { return m_active; }

    /** Get list of discovered services */
    QList<ServiceInfo> discoveredServices() const { return m_services; }

signals:
    /** Emitted when a new OS2L service is discovered */
    void serviceDiscovered(const OS2LDiscovery::ServiceInfo &service);

    /** Emitted when an OS2L service is no longer available */
    void serviceRemoved(const QString &serviceName);

private slots:
    void processPendingDatagrams();
    void sendQuery();
    void checkServiceTimeout();

private:
    void parseResponse(const QByteArray &data, const QHostAddress &sender);
    QString extractServiceName(const QByteArray &data, int offset, int &newOffset);

    QUdpSocket *m_socket;
    QTimer *m_queryTimer;
    QTimer *m_timeoutTimer;
    bool m_active;

    QList<ServiceInfo> m_services;
    QMap<QString, qint64> m_serviceLastSeen; // Service name -> timestamp

    static const quint16 MDNS_PORT = 5353;
    static const char* MDNS_ADDR;
    static const int SERVICE_TIMEOUT_MS = 30000; // 30 seconds
    static const int QUERY_INTERVAL_MS = 5000;   // 5 seconds
};

#endif // OS2LDISCOVERY_H
