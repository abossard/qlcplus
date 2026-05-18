/*
  Q Light Controller Plus
  vdjbonjour.h

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

#ifndef VDJBONJOUR_H
#define VDJBONJOUR_H

#include <QObject>

#ifdef Q_OS_MACOS
#include <dns_sd.h>
class QSocketNotifier;
#endif

/**
 * Bonjour/DNS-SD service registration for the DMXDesktop telemetry protocol.
 *
 * Registers QLC+ as a "_dmxdesktop._tcp" service so that VirtualDJ can
 * discover it automatically. Same pattern as OS2LBonjour but for the
 * telemetry port (default 8050).
 *
 * On macOS: native dns_sd.h API.
 * Other platforms: no-op (user configures VDJ manually).
 */
class VdjBonjour : public QObject
{
    Q_OBJECT

public:
    explicit VdjBonjour(QObject *parent = nullptr);
    ~VdjBonjour();

    bool registerService(const QString &serviceName, quint16 port);
    void unregisterService();
    bool isRegistered() const { return m_registered; }

signals:
    void serviceRegistered(const QString &name, quint16 port);
    void serviceRegistrationFailed(const QString &errorMessage);

#ifdef Q_OS_MACOS
private slots:
    void bonjourSocketReadyRead();

private:
    static void DNSSD_API registerCallback(
        DNSServiceRef sdRef,
        DNSServiceFlags flags,
        DNSServiceErrorType errorCode,
        const char *name,
        const char *regtype,
        const char *domain,
        void *context);

    DNSServiceRef m_dnssRef;
    QSocketNotifier *m_notifier;
#endif

private:
    bool m_registered;
    quint16 m_port;
};

#endif // VDJBONJOUR_H
