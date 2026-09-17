/*
  Q Light Controller Plus - Unit test stubs

  RGB/HUE matrix editors reference Tardis for undo queueing in setter paths
  that these tests don't execute. Stub the tiny symbol surface to avoid
  linking the full QML application graph.
*/

#include <QVariant>
#include <QString>

class Tardis
{
public:
    static Tardis *instance();
    void beginBatch(const QString &name);
    void endBatch();
    void enqueueAction(int code, unsigned int objID, QVariant oldVal, QVariant newVal);
};

Tardis *Tardis::instance()
{
    static Tardis stub;
    return &stub;
}

void Tardis::enqueueAction(int, unsigned int, QVariant, QVariant)
{
}

void Tardis::beginBatch(const QString &)
{
}

void Tardis::endBatch()
{
}
