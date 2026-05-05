/*
  Q Light Controller Plus
  liveaudioanalyzer_test.h
*/

#ifndef LIVEAUDIOANALYZER_TEST_H
#define LIVEAUDIOANALYZER_TEST_H

#include <QObject>

class LiveAudioAnalyzer_Test : public QObject
{
    Q_OBJECT

private slots:
    void testUniformFrameBasics();
    void testSpikeCentroidAndRolloff();
    void testHalfNormalizationAndDb();
    void testAnalyzeSilenceResetsHistory();
};

#endif // LIVEAUDIOANALYZER_TEST_H
