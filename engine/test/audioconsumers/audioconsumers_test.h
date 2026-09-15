#ifndef AUDIOCONSUMERS_TEST_H
#define AUDIOCONSUMERS_TEST_H

#include <QObject>

class AudioConsumers_Test : public QObject
{
    Q_OBJECT
private slots:
    void mapping();
    void eventCadence_data();
    void eventCadence();
    void packedScriptContract();
    void scriptRunnerStop_data();
    void scriptRunnerStop();
    void pcmToPixels_data();
    void pcmToPixels();
    void powerEcho_data();
    void powerEcho();
    void exportEffects();
    void runtimeReplay();
};

#endif
