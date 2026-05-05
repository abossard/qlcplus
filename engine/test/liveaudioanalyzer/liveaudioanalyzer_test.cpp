/*
  Q Light Controller Plus
  liveaudioanalyzer_test.cpp
*/

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtMath>
#include <QtTest>

#include "audiocapture.h"
#include "liveaudioanalyzer_test.h"
#include "liveaudioanalyzer.h"

namespace
{
    struct TestVector
    {
        double rms = 0.0;
        double peak = 0.0;
        double maxMagnitude = 0.0;
        std::array<double, AUDIO_FEATURE_BANDS> bands {};
    };

    TestVector loadVector(const char *relativePath)
    {
        const QString path = QFINDTESTDATA(QString::fromLatin1(relativePath));
        QVERIFY2(!path.isEmpty(), "Test data file not found via QFINDTESTDATA");

        QFile file(path);
        QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(QString("Failed to open %1").arg(path)));

        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        QVERIFY2(doc.isObject(), qPrintable(QString("Invalid JSON object in %1").arg(path)));

        const QJsonObject obj = doc.object();
        QVERIFY(obj.contains("rms"));
        QVERIFY(obj.contains("peak"));
        QVERIFY(obj.contains("maxMagnitude"));
        QVERIFY(obj.contains("bands"));

        TestVector vec;
        vec.rms = obj.value("rms").toDouble();
        vec.peak = obj.value("peak").toDouble();
        vec.maxMagnitude = obj.value("maxMagnitude").toDouble();

        const QJsonArray bands = obj.value("bands").toArray();
        QCOMPARE(bands.size(), AUDIO_FEATURE_BANDS);
        for (int i = 0; i < AUDIO_FEATURE_BANDS; i++)
            vec.bands[size_t(i)] = bands.at(i).toDouble();

        return vec;
    }

    double bandCenterHz(int index, int count)
    {
        if (count <= 0)
            return 0.0;

        const double minFreq = double(AudioCapture::minFrequency());
        const double maxFreq = double(AudioCapture::maxFrequency());
        const double logRange = qLn(maxFreq / minFreq);
        const double start = minFreq * qExp(logRange * (double(index) / double(count)));
        const double end = minFreq * qExp(logRange * (double(index + 1) / double(count)));
        return qSqrt(start * end);
    }
}

void LiveAudioAnalyzer_Test::testUniformFrameBasics()
{
    LiveAudioAnalyzer analyzer;
    const TestVector vec = loadVector("data/m1.json");

    const AudioFeatures features = analyzer.analyze(vec.rms, vec.peak, vec.bands, vec.maxMagnitude);

    QVERIFY(qAbs(features.rmsDb - (-20.0f)) < 0.2f);
    QVERIFY(qAbs(features.peakDb - (-13.979f)) < 0.2f);
    QVERIFY(qAbs(features.crestFactor - 2.0f) < 0.001f);

    for (int i = 0; i < AUDIO_FEATURE_BANDS; i++)
    {
        QVERIFY(qAbs(features.bandsLog[size_t(i)] - 1.0f) < 0.0001f);
        QVERIFY(qAbs(features.bandsNormalized[size_t(i)] - 1.0f) < 0.0001f);
        QVERIFY(qAbs(features.bandsDb[size_t(i)] - 0.0f) < 0.05f);
    }

    QVERIFY(qAbs(features.bands.sub - 1.0f) < 0.0001f);
    QVERIFY(qAbs(features.bands.bass - 1.0f) < 0.0001f);
    QVERIFY(qAbs(features.bands.lowMid - 1.0f) < 0.0001f);
    QVERIFY(qAbs(features.bands.mid - 1.0f) < 0.0001f);
    QVERIFY(qAbs(features.bands.high - 1.0f) < 0.0001f);

    QVERIFY(features.spectralCentroidHz > float(AudioCapture::minFrequency()));
    QVERIFY(features.spectralCentroidHz < float(AudioCapture::maxFrequency()));
    QVERIFY(features.spectralRolloffHz > float(AudioCapture::minFrequency()));
    QVERIFY(features.spectralRolloffHz < float(AudioCapture::maxFrequency()));
    QVERIFY(features.spectralFlatness > 0.99f);

    QCOMPARE(features.spectralFlux, 32.0f);
    QVERIFY(features.onset);
}

void LiveAudioAnalyzer_Test::testSpikeCentroidAndRolloff()
{
    LiveAudioAnalyzer analyzer;
    const TestVector vec = loadVector("data/m3.json");

    const AudioFeatures features = analyzer.analyze(vec.rms, vec.peak, vec.bands, vec.maxMagnitude);

    const int spikeIndex = 10;
    const float expectedHz = float(bandCenterHz(spikeIndex, AUDIO_FEATURE_BANDS));
    QVERIFY(qAbs(features.spectralCentroidHz - expectedHz) < 0.01f);
    QVERIFY(qAbs(features.spectralRolloffHz - expectedHz) < 0.01f);
}

void LiveAudioAnalyzer_Test::testHalfNormalizationAndDb()
{
    LiveAudioAnalyzer analyzer;
    const TestVector vec = loadVector("data/m2.json");

    const AudioFeatures features = analyzer.analyze(vec.rms, vec.peak, vec.bands, vec.maxMagnitude);

    // Even indices are 0.5, odd indices are 0.0
    QVERIFY(qAbs(features.bandsNormalized[0] - 0.5f) < 0.0001f);
    QVERIFY(qAbs(features.bandsNormalized[1] - 0.0f) < 0.0001f);
    QVERIFY(qAbs(features.bandsDb[0] - (-6.0206f)) < 0.2f);
    QVERIFY(qAbs(features.bandsDb[1] - (-96.0f)) < 0.01f);
}

void LiveAudioAnalyzer_Test::testAnalyzeSilenceResetsHistory()
{
    LiveAudioAnalyzer analyzer;
    const TestVector uniform = loadVector("data/m1.json");

    const AudioFeatures first = analyzer.analyze(uniform.rms, uniform.peak, uniform.bands, uniform.maxMagnitude);
    QCOMPARE(first.spectralFlux, 32.0f);

    const AudioFeatures second = analyzer.analyze(uniform.rms, uniform.peak, uniform.bands, uniform.maxMagnitude);
    QCOMPARE(second.spectralFlux, 0.0f);
    QVERIFY(!second.onset);

    const AudioFeatures silence = analyzer.analyzeSilence();
    QCOMPARE(silence.rmsDb, -96.0f);
    for (int i = 0; i < AUDIO_FEATURE_BANDS; i++)
        QCOMPARE(silence.bandsDb[size_t(i)], -96.0f);

    const AudioFeatures afterSilence = analyzer.analyze(uniform.rms, uniform.peak, uniform.bands, uniform.maxMagnitude);
    QCOMPARE(afterSilence.spectralFlux, 32.0f);
    QVERIFY(afterSilence.onset);
}

QTEST_MAIN(LiveAudioAnalyzer_Test)

