/*
  Q Light Controller Plus
  vdjbonjour.cpp

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

#include "vdjbonjour.h"

#include <QDebug>

#ifdef Q_OS_MACOS
#include <QSocketNotifier>
#include <arpa/inet.h>
#endif

static const char *kLogTag = "[VDJ Bonjour]";

VdjBonjour::VdjBonjour(QObject *parent)
    : QObject(parent)
#ifdef Q_OS_MACOS
    , m_dnssRef(nullptr)
    , m_notifier(nullptr)
#endif
    , m_registered(false)
    , m_port(0)
{
}

VdjBonjour::~VdjBonjour()
{
    unregisterService();
}

#ifdef Q_OS_MACOS

bool VdjBonjour::registerService(const QString &serviceName, quint16 port)
{
    if (m_registered)
        return true;

    m_port = port;

    DNSServiceErrorType err = DNSServiceRegister(
        &m_dnssRef,
        0,
        kDNSServiceInterfaceIndexAny,
        serviceName.toUtf8().constData(),
        "_dmxdesktop._tcp",
        "",
        NULL,
        htons(port),
        0,
        NULL,
        registerCallback,
        this);

    if (err != kDNSServiceErr_NoError)
    {
        QString msg = QString("DNSServiceRegister failed with error %1").arg(err);
        qWarning() << kLogTag << msg;
        emit serviceRegistrationFailed(msg);
        return false;
    }

    int fd = DNSServiceRefSockFD(m_dnssRef);
    if (fd != -1)
    {
        m_notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
        connect(m_notifier, &QSocketNotifier::activated,
                this, &VdjBonjour::bonjourSocketReadyRead);
    }

    qDebug() << kLogTag << "Registering service" << serviceName
             << "(_dmxdesktop._tcp) on port" << port << "...";
    return true;
}

void VdjBonjour::unregisterService()
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
        qDebug() << kLogTag << "Service unregistered";
    }

    m_registered = false;
}

void VdjBonjour::bonjourSocketReadyRead()
{
    if (m_dnssRef)
    {
        DNSServiceErrorType err = DNSServiceProcessResult(m_dnssRef);
        if (err != kDNSServiceErr_NoError)
            qWarning() << kLogTag << "DNSServiceProcessResult error:" << err;
    }
}

/* static */
void DNSSD_API VdjBonjour::registerCallback(
    DNSServiceRef /* sdRef */,
    DNSServiceFlags /* flags */,
    DNSServiceErrorType errorCode,
    const char *name,
    const char *regtype,
    const char *domain,
    void *context)
{
    VdjBonjour *self = static_cast<VdjBonjour *>(context);

    if (errorCode == kDNSServiceErr_NoError)
    {
        self->m_registered = true;
        QString svcName = QString::fromUtf8(name);
        qDebug() << kLogTag << "*** Service registered ***";
        qDebug() << kLogTag << "  Name:" << svcName;
        qDebug() << kLogTag << "  Type:" << regtype;
        qDebug() << kLogTag << "  Domain:" << domain;
        qDebug() << kLogTag << "  Port:" << self->m_port;
        emit self->serviceRegistered(svcName, self->m_port);
    }
    else
    {
        QString msg = QString("Registration callback error %1").arg(errorCode);
        qWarning() << kLogTag << msg;
        emit self->serviceRegistrationFailed(msg);
    }
}

#else // Non-macOS stub

bool VdjBonjour::registerService(const QString &serviceName, quint16 port)
{
    Q_UNUSED(serviceName);
    Q_UNUSED(port);
    qDebug() << kLogTag << "Bonjour registration only available on macOS.";
    qDebug() << kLogTag << "Configure VirtualDJ to connect to this IP:port manually.";
    return false;
}

void VdjBonjour::unregisterService()
{
    m_registered = false;
}

#endif // Q_OS_MACOS
