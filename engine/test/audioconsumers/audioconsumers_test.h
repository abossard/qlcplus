#ifndef AUDIOCONSUMERS_TEST_H
#define AUDIOCONSUMERS_TEST_H

#include <QObject>

class AudioConsumers_Test : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void mapping();
    void eventCadence_data();
    void eventCadence();
    void eventCadenceTransientLoss();
    void packedScriptContract_data();
    void packedScriptContract();
    void paletteRoutingRegression_data();
    void paletteRoutingRegression();
    void nativeExecutionContractSweep_data();
    void nativeExecutionContractSweep();
    void scriptRunnerStop_data();
    void scriptRunnerStop();
    void scriptRunnerQueue_data();
    void scriptRunnerQueue();
    void scriptRunnerDispatch_data();
    void scriptRunnerDispatch();
    void scriptRunnerWait_data();
    void scriptRunnerWait();
    void scriptRunnerRandom_data();
    void scriptRunnerRandom();
    void scriptRunnerCompletion_data();
    void scriptRunnerCompletion();
    void pcmToPixels_data();
    void pcmToPixels();
    void powerEcho_data();
    void powerEcho();
    void exportEffects();
    void runtimeReplay();
};

#endif
