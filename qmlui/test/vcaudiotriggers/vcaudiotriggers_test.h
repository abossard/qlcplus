#pragma once

#include <QObject>

class VCAudioTriggers_Test : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void realPanel_data();
    void realPanel();
    void referenceDefaults_data();
    void referenceDefaults();
    void bankUpdate_data();
    void bankUpdate();
    void bankControlBounds_data();
    void bankControlBounds();
    void coherentPublication();
    void visualCadence_data();
    void visualCadence();
    void sharedProfileDemand();
    void latencyBinding_data();
    void latencyBinding();
    void inputLatencyStall_data();
    void inputLatencyStall();
    void sparklineRecovery_data();
    void sparklineRecovery();
    void diagnosticsControl_data();
    void diagnosticsControl();
    void editorLongStepPreview();
    void nativeConsumers_data();
    void nativeConsumers();
    void eventDelivery_data();
    void eventDelivery();
    void mappedActions_data();
    void mappedActions();
    void mappingRoundTrip_data();
    void mappingRoundTrip();
    void mappedSubmaster_data();
    void mappedSubmaster();
    void invalidMappingSource_data();
    void invalidMappingSource();
};
