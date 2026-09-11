#pragma once

#include <QObject>

class AudioChannel_Test : public QObject
{
    Q_OBJECT
private slots:
    void fullBankPowers();
    void postFilterFreeze();
    void postFilterReference_data();
    void postFilterReference();
    void publicationCounters();
};
