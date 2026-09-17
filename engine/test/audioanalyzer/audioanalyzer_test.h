#pragma once

#include <QObject>

class AudioAnalyzer_Test : public QObject
{
    Q_OBJECT
private slots:
    void canonicalProfiles_data();
    void canonicalProfiles();
    void rawDetectorBranch();
    void silenceFreezesSpectrum();
    void profileRevisionAndRetirement();
    void powerWindow_data();
    void powerWindow();
    void powerResponse_data();
    void powerResponse();
    void rawDetectorsPreserved_data();
    void rawDetectorsPreserved();
    void powerReconfiguration();
    void dumpCorpus();
};
