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

static const char *kLogTag = "[VDJ Bonjour]";
#endif

VdjBonjour::VdjBonjour(QObject *parent)
    : QObject(parent)
#ifdef Q_OS_MACOS
    , m_dnssRef(nullptr)
    , m_notifier(nullptr)
#endif
{
}

VdjBonjour::~VdjBonjour()
{
    unregisterService();
}

#ifdef Q_OS_MACOS

bool VdjBonjour::registerService(const QString &serviceName, quint16 port)
{
    if (m_dnssRef)
        return true;

    DNSServiceErrorType err = DNSServiceRegister(
        &m_dnssRef,
        0,
        kDNSServiceInterfaceIndexAny,
        serviceName.toUtf8().constData(),
        "_os2l._tcp",
        "",
        NULL,
        htons(port),
        0,
        NULL,
        registerCallback,
        this);

    if (err != kDNSServiceErr_NoError)
    {
        m_dnssRef = nullptr;
        fail(QString("DNSServiceRegister failed with error %1").arg(err));
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
             << "(_os2l._tcp) on port" << port << "...";
    return true;
}

void VdjBonjour::unregisterService()
{
    if (m_notifier)
    {
        m_notifier->setEnabled(false);
        // this may run inside the notifier's own activated()
        m_notifier->deleteLater();
        m_notifier = nullptr;
    }

    if (m_dnssRef)
    {
        DNSServiceRefDeallocate(m_dnssRef);
        m_dnssRef = nullptr;
        qDebug() << kLogTag << "Service unregistered";
    }
}

void VdjBonjour::fail(const QString &message)
{
    // a ref left allocated keeps a readable fd: the notifier would fire forever
    qWarning() << kLogTag << message;
    unregisterService();
    emit serviceRegistrationFailed(message);
}

void VdjBonjour::bonjourSocketReadyRead()
{
    if (m_dnssRef)
    {
        DNSServiceErrorType err = DNSServiceProcessResult(m_dnssRef);
        if (err != kDNSServiceErr_NoError)
            fail(QString("DNSServiceProcessResult error %1").arg(err));
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
        qDebug() << kLogTag << "Service registered:" << name << regtype << domain;
    else
        self->fail(QString("Registration callback error %1").arg(errorCode)); // dns_sd allows deallocating here
}

#else // Non-macOS stub

bool VdjBonjour::registerService(const QString &serviceName, quint16 port)
{
    Q_UNUSED(serviceName);
    Q_UNUSED(port);
    emit serviceRegistrationFailed(tr("Bonjour registration is only available on macOS; "
                                      "connect VirtualDJ to this IP:port manually"));
    return false;
}

void VdjBonjour::unregisterService()
{
}

#endif // Q_OS_MACOS
