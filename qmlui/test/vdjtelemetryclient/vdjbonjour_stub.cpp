/*
  Q Light Controller Plus - Unit test
  vdjbonjour_stub.cpp

  VdjBonjour without mDNS: a test must not advertise _os2l._tcp to a
  VirtualDJ running on the machine. Counts the registrations instead.
*/

#include "vdjbonjour.h"

#include <QSet>

int vdjBonjourRegistrations = 0;
static QSet<const VdjBonjour *> registered;

/** Services registered right now */
int vdjBonjourLive()
{
    return int(registered.size());
}

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

bool VdjBonjour::registerService(const QString &, quint16)
{
    // as the real one: a live registration is kept, not made again
    if (registered.contains(this))
        return true;
    registered.insert(this);
    vdjBonjourRegistrations++;
    return true;
}

void VdjBonjour::unregisterService()
{
    registered.remove(this);
}

#ifdef Q_OS_MACOS
void VdjBonjour::bonjourSocketReadyRead()
{
}
#endif
