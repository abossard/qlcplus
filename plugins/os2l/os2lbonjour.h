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

#ifndef OS2LBONJOUR_H
#define OS2LBONJOUR_H

#include <QObject>

#ifdef Q_OS_MACOS
#include <dns_sd.h>
class QSocketNotifier;
#endif

/**
 * @brief Bonjour service registration for the OS2L plugin (macOS only).
 *
 * Registers QLC+ as a "_os2l._tcp" Bonjour/DNS-SD service so that
 * VirtualDJ (and other OS2L hosts) can discover it automatically when
 * their OS2L mode is set to "Auto".
 *
 * On macOS the native dns_sd.h API is used (DNSServiceRegister).
 * On all other platforms the class compiles but is a no-op
 * (native Bonjour would require platform-specific implementations
 * or external libraries such as Avahi on Linux).
 *
 * Protocol / API references:
 *   - OS2L service type "_os2l._tcp": https://os2l.org
 *   - VirtualDJ OS2L Auto mode: https://www.virtualdj.com/wiki/OS2L.html
 *   - Apple DNS-SD C API: https://developer.apple.com/documentation/dnssd
 *   - RFC 6762 (mDNS): https://tools.ietf.org/html/rfc6762
 *   - RFC 6763 (DNS-SD): https://tools.ietf.org/html/rfc6763
 */
class OS2LBonjour : public QObject
{
    Q_OBJECT

public:
    explicit OS2LBonjour(QObject *parent = nullptr);
    ~OS2LBonjour();

    /**
     * Register QLC+ as a "_os2l._tcp" Bonjour service on @p port.
     * On macOS this calls DNSServiceRegister(); on other platforms it
     * returns false immediately.
     */
    bool registerService(const QString &serviceName, quint16 port);

    /** Remove the Bonjour registration. */
    void unregisterService();

    /** @return true when a registration is active. */
    bool isRegistered() const { return m_registered; }

signals:
    /** Emitted after Bonjour confirms the registration. */
    void serviceRegistered(const QString &name, quint16 port);

    /** Emitted if registration fails. */
    void serviceRegistrationFailed(const QString &errorMessage);

#ifdef Q_OS_MACOS
private slots:
    /** Called by QSocketNotifier when the dns_sd socket is readable. */
    void bonjourSocketReadyRead();

private:
    /**
     * Callback invoked by the dns_sd daemon when registration completes.
     * See DNSServiceRegisterReply in dns_sd.h.
     */
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

#endif // OS2LBONJOUR_H
