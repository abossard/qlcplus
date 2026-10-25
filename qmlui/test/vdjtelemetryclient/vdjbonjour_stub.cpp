/*
  Q Light Controller Plus - Unit test
  vdjbonjour_stub.cpp

  VdjBonjour without mDNS: a test must not advertise _os2l._tcp to a
  VirtualDJ running on the machine. Counts the registrations instead.
*/

#include "vdjbonjour.h"

int vdjBonjourRegistrations = 0;

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
}

bool VdjBonjour::registerService(const QString &, quint16)
{
    vdjBonjourRegistrations++;
    return true;
}

void VdjBonjour::unregisterService()
{
}

#ifdef Q_OS_MACOS
void VdjBonjour::bonjourSocketReadyRead()
{
}
#endif
