/*
  Q Light Controller Plus - Unit test stub

  Minimal stub for FunctionManager::deleteFunction so tests linking
  qlcplusmcp::registerFunctionTools (which references this symbol via a
  lambda) don't pull in the full QML FunctionManager translation unit.

  registerFunctionTools is invoked with funcMgr=nullptr in tests, so this
  body is never executed — only the symbol must resolve at link time.
*/

#include <QObject>
#include "doc.h"

// Forward-declare a class that matches FunctionManager's signature for the
// one member we need. We avoid #include "functionmanager.h" (and its heavy
// QML deps) by mirroring just enough of the class to provide the symbol.
//
// IMPORTANT: this matches the real class layout only in name; tests must
// never construct it or call any other method.
class FunctionManager : public QObject
{
public:
    void deleteFunction(unsigned int fid);
};

void FunctionManager::deleteFunction(unsigned int)
{
    // Never invoked in tests (funcMgr is always nullptr).
}
