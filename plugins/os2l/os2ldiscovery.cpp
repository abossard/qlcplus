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

#include <QDebug>

#ifdef Q_OS_MACOS
#include <QSocketNotifier>
#include <arpa/inet.h>   // htons
#endif

/*********************************************************************
 * Construction / destruction
 *********************************************************************/

OS2LBonjour::OS2LBonjour(QObject *parent)
    : QObject(parent)
#ifdef Q_OS_MACOS
    , m_dnssRef(nullptr)
    , m_notifier(nullptr)
#endif
    , m_registered(false)
    , m_port(0)
{
}

OS2LBonjour::~OS2LBonjour()
{
    unregisterService();
}

/*********************************************************************
 * macOS — native Bonjour via dns_sd.h
 *
 * DNSServiceRegister() advertises QLC+ as "_os2l._tcp" so that
 * VirtualDJ's Auto mode discovers it on the local network.
 *
 * References:
 *   - Apple DNS-SD API: https://developer.apple.com/documentation/dnssd
 *   - dns_sd.h header: DNSServiceRegister, DNSServiceRefSockFD,
 *     DNSServiceProcessResult, DNSServiceRefDeallocate
 *   - RFC 6763 §7 (service naming): https://tools.ietf.org/html/rfc6763#section-7
 *   - OS2L service type "_os2l._tcp": https://os2l.org
 *********************************************************************/

#ifdef Q_OS_MACOS

bool OS2LBonjour::registerService(const QString &serviceName, quint16 port)
{
    if (m_registered)
        return true;

    m_port = port;

    // DNSServiceRegister() — register a named service of type "_os2l._tcp"
    // on the given port.  The Bonjour daemon will respond to mDNS queries
    // for this service type, allowing VirtualDJ Auto mode to find QLC+.
    //
    // See: https://developer.apple.com/documentation/dnssd/1804733-dnsserviceregister
    DNSServiceErrorType err = DNSServiceRegister(
        &m_dnssRef,                          // DNSServiceRef output
        0,                                   // no flags
        kDNSServiceInterfaceIndexAny,        // all interfaces
        serviceName.toUtf8().constData(),     // human-readable instance name
        "_os2l._tcp",                        // service type (OS2L spec)
        "",                                  // domain (default = "local.")
        NULL,                                // host   (this machine)
        htons(port),                         // port in network byte order
        0,                                   // TXT record length
        NULL,                                // TXT record data
        registerCallback,                    // async reply callback
        this);                               // context pointer

    if (err != kDNSServiceErr_NoError)
    {
        QString msg = QString("DNSServiceRegister failed with error %1").arg(err);
        qWarning() << "[OS2L Bonjour]" << msg;
        emit serviceRegistrationFailed(msg);
        return false;
    }

    // Integrate the dns_sd file descriptor with the Qt event loop so the
    // callback fires without blocking.
    int fd = DNSServiceRefSockFD(m_dnssRef);
    if (fd != -1)
    {
        m_notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
        connect(m_notifier, &QSocketNotifier::activated,
                this, &OS2LBonjour::bonjourSocketReadyRead);
    }

    qDebug() << "[OS2L Bonjour] Registering service" << serviceName
             << "(_os2l._tcp) on port" << port << "...";
    return true;
}

void OS2LBonjour::unregisterService()
{
    if (m_notifier)
    {
        m_notifier->setEnabled(false);
        delete m_notifier;
        m_notifier = nullptr;
    }

    if (m_dnssRef)
    {
        DNSServiceRefDeallocate(m_dnssRef);
        m_dnssRef = nullptr;
        qDebug() << "[OS2L Bonjour] Service unregistered";
    }

    m_registered = false;
}

void OS2LBonjour::bonjourSocketReadyRead()
{
    // Let the dns_sd daemon process its socket data and invoke our callback.
    if (m_dnssRef)
    {
        DNSServiceErrorType err = DNSServiceProcessResult(m_dnssRef);
        if (err != kDNSServiceErr_NoError)
            qWarning() << "[OS2L Bonjour] DNSServiceProcessResult error:" << err;
    }
}

/* static */
void DNSSD_API OS2LBonjour::registerCallback(
    DNSServiceRef /* sdRef */,
    DNSServiceFlags /* flags */,
    DNSServiceErrorType errorCode,
    const char *name,
    const char *regtype,
    const char *domain,
    void *context)
{
    OS2LBonjour *self = static_cast<OS2LBonjour *>(context);

    if (errorCode == kDNSServiceErr_NoError)
    {
        self->m_registered = true;
        QString svcName = QString::fromUtf8(name);
        qDebug() << "[OS2L Bonjour] *** Service registered successfully ***";
        qDebug() << "[OS2L Bonjour]   Name:" << svcName;
        qDebug() << "[OS2L Bonjour]   Type:" << regtype;
        qDebug() << "[OS2L Bonjour]   Domain:" << domain;
        qDebug() << "[OS2L Bonjour]   Port:" << self->m_port;
        qDebug() << "[OS2L Bonjour]   VirtualDJ can now discover QLC+ in Auto mode";
        emit self->serviceRegistered(svcName, self->m_port);
    }
    else
    {
        QString msg = QString("Registration callback error %1").arg(errorCode);
        qWarning() << "[OS2L Bonjour]" << msg;
        emit self->serviceRegistrationFailed(msg);
    }
}

#else // !Q_OS_MACOS — stub implementation for non-macOS platforms

bool OS2LBonjour::registerService(const QString &serviceName, quint16 port)
{
    Q_UNUSED(serviceName);
    Q_UNUSED(port);
    qDebug() << "[OS2L Bonjour] Bonjour service registration is only available on macOS.";
    qDebug() << "[OS2L Bonjour] On this platform, configure VirtualDJ's os2lDirectIp manually.";
    return false;
}

void OS2LBonjour::unregisterService()
{
    m_registered = false;
}

#endif // Q_OS_MACOS
