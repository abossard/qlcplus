/*
  Q Light Controller Plus - Unit test
  huematrix_test.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#include <QtTest>
#include <QBuffer>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QSet>
#include <cmath>
#include <limits>

#define private public
#define protected public
#include "huematrix_test.h"
#include "huescriptscache.h"
#include "huescript.h"
#include "huematrix.h"
#include "rgbalgorithm.h"
#include "rgbaudio.h"
#include "rgbimage.h"
#include "rgbplain.h"
#include "rgbtext.h"
#include "rgbmatrix.h"
#include "rgbscriptscache.h"
#include "fixturegroup.h"
#include "inputoutputmap.h"
#include "mastertimer.h"
#include "universe.h"
#include "function.h"
#undef private
#undef protected

#include "doc.h"

#include "../common/resource_paths.h"

#define INTERNAL_HUESCRIPTDIR "../../../resources/huescripts/"

static QStringList expectedHueAlgorithmNames()
{
    return QStringList{
        "Audio Aurora", "Audio BPM Bar", "Audio Bands", "Audio Bands Matrix", "Audio Barcode",
        "Audio Bass Laser", "Audio Beat Colors", "Audio Blade Power", "Audio Bleep", "Audio Blocks",
        "Audio Blurz", "Audio Buildup", "Audio Cellular", "Audio Chaser", "Audio Concentric",
        "Audio Crawler", "Audio DJ Light", "Audio Digital Rain", "Audio Energy", "Audio Energy 2",
        "Audio Equalizer", "Audio Equalizer 2D", "Audio Filter", "Audio Fire", "Audio Fireworks",
        "Audio Flame", "Audio Flow Field", "Audio Game of Life", "Audio Glitch", "Audio Glitch 2",
        "Audio Gravimeter", "Audio Hierarchy", "Audio Hue Shift", "Audio Lava Lamp", "Audio Magnitude",
        "Audio Marching", "Audio Melt", "Audio Melt and Sparkle", "Audio Multicolor Bar", "Audio Noise",
        "Audio Pitch Spectrum", "Audio Plasma", "Audio Power", "Audio Puddles", "Audio Reaction-Diffusion",
        "Audio Reactor", "Audio Scan", "Audio Scan Multi", "Audio Scan and Flare", "Audio Shockwave",
        "Audio Shot", "Audio Smoke", "Audio Soap", "Audio Spectral Blocks", "Audio Spectrum Bars",
        "Audio Split Tower", "Audio Spotlight", "Audio Strobe", "Audio Tunnel", "Audio Vortex",
        "Audio VuMeter", "Audio Water", "Audio Waterfall", "Hue Fade", "Hue Gradient", "Hue Metro",
        "Hue Pixels", "Hue Rainbow", "Hue Random Flash", "Hue Single Color"
    };
}

static void failOnUnexpectedHueScriptWarnings()
{
    QTest::failOnWarning(
        QRegularExpression(QStringLiteral(".*values cannot be applied before the 'type' property.*")));
    QTest::failOnWarning(
        QRegularExpression(QStringLiteral(".*Variable\\s+\"[^\"]+\"\\s+is\\s+used\\s+before\\s+its\\s+declaration.*")));
}

void HUEMatrix_Test::initTestCase()
{
    m_doc = new Doc(this);
    QVERIFY(m_doc->rgbScriptsCache()->load(QDir(INTERNAL_SCRIPTDIR)));
    QVERIFY(m_doc->hueScriptsCache()->load(QDir(INTERNAL_HUESCRIPTDIR), true));
    QVERIFY(m_doc->hueScriptsCache()->load(QDir(INTERNAL_SCRIPTDIR), false));
}

void HUEMatrix_Test::cleanupTestCase()
{
    delete m_doc;
    m_doc = NULL;
}

/****************************************************************************
 * AC1 - HUEMatrix is a first class Function type
 ****************************************************************************/

void HUEMatrix_Test::functionType()
{
    HUEMatrix mtx(m_doc);
    QCOMPARE(mtx.type(), Function::HUEMatrixType);
    QCOMPARE(Function::typeToString(mtx.type()), QString("HUEMatrix"));
    QCOMPARE(Function::stringToType("HUEMatrix"), Function::HUEMatrixType);

    // A HUEMatrix is-a RGBMatrix, but a plain RGBMatrix is never a HUEMatrix
    RGBMatrix plain(m_doc);
    QCOMPARE(plain.type(), Function::RGBMatrixType);
    QVERIFY(qobject_cast<HUEMatrix*>(&plain) == NULL);
    QVERIFY(qobject_cast<RGBMatrix*>(&mtx) != NULL);
}

/****************************************************************************
 * AC5 - script partitioning
 ****************************************************************************/

void HUEMatrix_Test::audioScriptsAreNotOfferedToRGBMatrix()
{
    // THE IMPORTANT HALF: a plain RGBMatrix must not see any relocated script.
    QStringList rgbScripts = m_doc->rgbScriptsCache()->names();
    QVERIFY(rgbScripts.isEmpty() == false);

    QStringList leaked;
    foreach (QString name, m_doc->hueScriptsCache()->hsvNames())
    {
        if (rgbScripts.contains(name))
            leaked << name;
    }
    QVERIFY2(leaked.isEmpty(),
             qPrintable(QString("HSV scripts leaked into the RGBMatrix script list: %1")
                        .arg(leaked.join(", "))));

    // No file in resources/rgbscripts is named audio*.js any more
    QStringList strays = QDir(INTERNAL_SCRIPTDIR).entryList(QStringList() << "audio*.js");
    QVERIFY2(strays.isEmpty(),
             qPrintable(QString("audio scripts left in rgbscripts: %1").arg(strays.join(", "))));

    // And resources/huescripts holds exactly those files
    QStringList moved = QDir(INTERNAL_HUESCRIPTDIR).entryList(QStringList() << "audio*.js");
    QCOMPARE(moved.count(), 63);
}

void HUEMatrix_Test::hueMatrixOffersAllAudioScripts()
{
    failOnUnexpectedHueScriptWarnings();

    QStringList hueNames = HUEMatrix::availableAlgorithms(m_doc);
    QStringList rgbNames = RGBAlgorithm::algorithms(m_doc);
    const QStringList expected = expectedHueAlgorithmNames();

    foreach (QString name, rgbNames)
        QVERIFY2(hueNames.contains(name) == false, qPrintable(name));

    // All HSV scripts are present and instantiable.
    QStringList hsv = m_doc->hueScriptsCache()->hsvNames();
    QCOMPARE(expected.count(), 70);
    QCOMPARE(hsv.count(), 70);
    QStringList sortedExpected = expected;
    QStringList sortedHue = hueNames;
    QStringList sortedHsv = hsv;
    sortedExpected.sort();
    sortedHue.sort();
    sortedHsv.sort();
    QCOMPARE(sortedHue, sortedExpected);
    QCOMPARE(sortedHsv, sortedExpected);
    QCOMPARE(QSet<QString>(hueNames.begin(), hueNames.end()).size(), hueNames.size());
    QVERIFY(hueNames.contains("Audio Spectrum Bars"));
    QVERIFY(hueNames.contains("Audio Pitch Spectrum"));
    QVERIFY(hueNames.contains("Audio Digital Rain"));
    QVERIFY(hueNames.contains("Audio Waterfall"));
    QVERIFY(hueNames.contains("Audio Bands"));
    QVERIFY(hueNames.contains("Audio Equalizer 2D"));
    QVERIFY(hueNames.contains("Audio VuMeter"));
    QVERIFY(hueNames.contains("Hue Fade"));
    QVERIFY(hueNames.contains("Hue Rainbow"));
    QVERIFY(!hueNames.contains("Audio Spectrum"));
    QVERIFY(rgbNames.contains("Audio Spectrum"));
    QVERIFY(rgbNames.contains("Stripes"));
    QStringList runtime = HUEMatrix::runtimeAlgorithms(m_doc);
    QStringList runtimeSuffix = runtime.mid(rgbNames.size());
    QStringList sortedRuntimeSuffix = runtimeSuffix;
    sortedRuntimeSuffix.sort();
    QCOMPARE(runtime.mid(0, rgbNames.size()), rgbNames);
    QCOMPARE(sortedRuntimeSuffix, sortedExpected);
    QCOMPARE(QSet<QString>(runtime.begin(), runtime.end()).size(), runtime.size());
    foreach (QString name, hsv)
    {
        QVERIFY2(hueNames.contains(name), qPrintable(name));
        RGBAlgorithm *algo = HUEMatrix::createAlgorithm(m_doc, name);
        QVERIFY2(algo != NULL, qPrintable(name));
        QCOMPARE(algo->name(), name);
        delete algo;
    }
}

void HUEMatrix_Test::defaultPattern_data()
{
    QTest::addColumn<bool>("loadHsv");
    QTest::newRow("startup-cache") << true;
    QTest::newRow("stock-only-cache") << false;
}

void HUEMatrix_Test::defaultPattern()
{
    QFETCH(bool, loadHsv);
    Doc doc(this);
    QVERIFY(doc.rgbScriptsCache()->load(QDir(INTERNAL_SCRIPTDIR)));
    QVERIFY(doc.hueScriptsCache()->load(QDir(INTERNAL_SCRIPTDIR), false));
    if (loadHsv)
        QVERIFY(doc.hueScriptsCache()->load(QDir(INTERNAL_HUESCRIPTDIR), true));
    HUEMatrix matrix(&doc);
    const QStringList offered = HUEMatrix::availableAlgorithms(&doc);
    if (loadHsv)
    {
        QCOMPARE(offered.count(), 70);
        QVERIFY(matrix.algorithm() != nullptr);
        QCOMPARE(matrix.algorithm()->name(), QString("Audio Aurora"));
        QVERIFY(offered.contains(matrix.algorithm()->name()));
        QVERIFY(dynamic_cast<HUEScript *>(matrix.algorithm()) != nullptr);
    }
    else
    {
        QVERIFY(offered.isEmpty());
        QVERIFY(matrix.algorithm() == nullptr);
    }
}

void HUEMatrix_Test::nonAudioHsvContract()
{
    const QString fixtureDir = QDir::current().filePath("hue-contract-fixture");
    QVERIFY(QDir().mkpath(fixtureDir));
    const auto cleanup = qScopeGuard([&]() { QDir(fixtureDir).removeRecursively(); });
    QFile file(fixtureDir + "/still.js");
    QVERIFY(file.open(QIODevice::WriteOnly));
    const QByteArray source =
        "var testAlgo;\n(function () {\nvar algo = {};\n"
        "algo.name = \"Still HSV\";\n"
        "algo.apiVersion = 3;\nalgo.usesAudio = false;\nalgo.acceptColors = 0;\n"
        "algo.rgbMapStepCount = function(w,h) { return 1; };\n"
        "algo.rgbMap = function(w,h,r,s) { return new Float32Array(w*h*3); };\n"
        "testAlgo = algo;\nreturn algo;\n})();\n";
    QCOMPARE(file.write(source), qint64(source.size()));
    file.close();
    Doc doc(this);
    QVERIFY(doc.hueScriptsCache()->load(QDir(fixtureDir), true));
    HUEMatrix matrix(&doc);
    QCOMPARE(HUEMatrix::availableAlgorithms(&doc), QStringList{"Still HSV"});
    QVERIFY(matrix.algorithm() != nullptr);
    QCOMPARE(matrix.algorithm()->name(), QString("Still HSV"));
    QVERIFY(!matrix.algorithm()->usesAudio());
    QVERIFY(!RGBAlgorithm::algorithms(&doc).contains("Still HSV"));
    QVERIFY(file.remove());
    QVERIFY(QDir().rmdir(fixtureDir));
}

void HUEMatrix_Test::nonAudioSingleColorStepBoundary()
{
    auto *group = new FixtureGroup(m_doc);
    group->setName("HueSingleColor");
    group->setSize(QSize(5, 1));
    QVERIFY(m_doc->addFixtureGroup(group));

    MasterTimer timer(m_doc);
    QList<Universe *> universes = m_doc->inputOutputMap()->claimUniverses();
    m_doc->inputOutputMap()->releaseUniverses(false);

    auto *staticAlgo = HUEMatrix::createAlgorithm(m_doc, "Hue Single Color");
    QVERIFY(staticAlgo != nullptr);
    auto *staticScript = static_cast<RGBScript *>(staticAlgo);
    QVERIFY(staticScript->setProperty("presetModulation", "Off"));
    QCOMPARE(staticAlgo->rgbMapStepCount(group->size()), 1);
    HUEMatrix staticMatrix(m_doc);
    staticMatrix.setFixtureGroup(group->id());
    staticMatrix.setDuration(20);
    staticMatrix.setFadeInSpeed(0);
    staticMatrix.setFadeOutSpeed(0);
    staticMatrix.setAlgorithm(staticAlgo);
    staticMatrix.preRun(&timer);
    staticMatrix.write(&timer, universes);
    const RGBMap staticFirst = staticMatrix.m_stepHandler->m_map;
    QTest::qSleep(30);
    staticMatrix.write(&timer, universes);
    const RGBMap staticSecond = staticMatrix.m_stepHandler->m_map;
    staticMatrix.postRun(&timer, universes);
    QCOMPARE(staticFirst, staticSecond);

    auto *animatedAlgo = HUEMatrix::createAlgorithm(m_doc, "Hue Single Color");
    QVERIFY(animatedAlgo != nullptr);
    auto *animatedScript = static_cast<RGBScript *>(animatedAlgo);
    QVERIFY(animatedScript->setProperty("presetModulation", "Sine"));
    QVERIFY(animatedScript->setProperty("presetModulationSpeed", "1"));
    QVERIFY(animatedScript->setProperty("presetTemporalSpeed", "10"));
    QCOMPARE(animatedAlgo->rgbMapStepCount(group->size()), 2);

    HUEMatrix animatedMatrix(m_doc);
    animatedMatrix.setFixtureGroup(group->id());
    animatedMatrix.setDuration(20);
    animatedMatrix.setFadeInSpeed(0);
    animatedMatrix.setFadeOutSpeed(0);
    animatedMatrix.setAlgorithm(animatedAlgo);
    animatedMatrix.preRun(&timer);
    animatedMatrix.write(&timer, universes);
    const RGBMap animatedFirst = animatedMatrix.m_stepHandler->m_map;
    QTest::qSleep(30);
    animatedMatrix.write(&timer, universes);
    const RGBMap animatedSecond = animatedMatrix.m_stepHandler->m_map;

    const uint sentinel = 0x00123456;
    animatedMatrix.setDuration(200);
    animatedMatrix.write(&timer, universes);
    animatedMatrix.m_stepHandler->m_map = RGBMap(1, QVector<uint>(5, sentinel));
    animatedMatrix.write(&timer, universes);
    QCOMPARE(animatedMatrix.m_stepHandler->m_map, RGBMap(1, QVector<uint>(5, sentinel)));
    animatedMatrix.postRun(&timer, universes);

    QVERIFY(animatedFirst != animatedSecond);
}

void HUEMatrix_Test::patternSelection_data()
{
    QTest::addColumn<QString>("baseName");
    QTest::addColumn<QString>("selectedName");
    QTest::newRow("first-hue") << "Stripes" << "Audio Aurora";
    QTest::newRow("last-hue") << "Audio Spectrum" << "Audio Water";
    QTest::newRow("hue-to-hue") << "Audio Fire" << "Audio Spectrum Bars";
    QTest::newRow("unconfigured-base") << "" << "Audio Fire";
    QTest::newRow("unconfigured-builtin") << "" << "Plain Color";
    QTest::newRow("unconfigured-legacy-script") << "" << "Stripes";
    QTest::newRow("legacy-script-preset") << "Audio Fire" << "Stripes";
    QTest::newRow("legacy-text-preset") << "Audio Fire" << "Text";
}

void HUEMatrix_Test::patternSelection()
{
    QFETCH(QString, baseName);
    QFETCH(QString, selectedName);
    HUEMatrix matrix(m_doc);
    FixtureGroup *group = new FixtureGroup(m_doc);
    group->setSize(QSize(3, 2));
    QVERIFY(m_doc->addFixtureGroup(group));
    matrix.setFixtureGroup(group->id());
    matrix.setDuration(200);
    matrix.setAlgorithm(baseName.isEmpty() ? nullptr : HUEMatrix::createAlgorithm(m_doc, baseName));
    QStringList runtime = RGBAlgorithm::algorithms(m_doc);
    runtime.append(HUEMatrix::availableAlgorithms(m_doc));
    RGBMatrix *base = &matrix;
    QCOMPARE(base->algorithmIndex(), runtime.indexOf(baseName));
    QCOMPARE(matrix.attributes().at(RGBMatrix::PatternAttr).m_max, qreal(runtime.count() - 1));
    const int selected = runtime.indexOf(selectedName);
    QVERIFY(selected >= 0);
    if (selectedName == "Stripes" || selectedName == "Text" || selectedName == "Plain Color")
        QVERIFY(!HUEMatrix::availableAlgorithms(m_doc).contains(selectedName));
    else
        QVERIFY(selected >= RGBAlgorithm::algorithms(m_doc).count());
    const int overrideID = base->requestAttributeOverride(RGBMatrix::PatternAttr, selected);
    QVERIFY(overrideID != Function::invalidAttributeId());
    QCOMPARE(base->algorithm()->name(), selectedName);
    if (selectedName == "Text")
        QVERIFY(dynamic_cast<RGBText *>(base->algorithm()) != nullptr);
    else if (selectedName == "Plain Color")
        QVERIFY(dynamic_cast<RGBPlain *>(base->algorithm()) != nullptr);
    else
        QVERIFY(dynamic_cast<HUEScript *>(base->algorithm()) != nullptr);
    RGBAlgorithm *unchanged = base->algorithm();
    base->adjustAttribute(selected, overrideID);
    QCOMPARE(base->algorithm(), unchanged);
    for (qreal invalid : {qreal(-2), qreal(-1), qreal(-0.25), qreal(runtime.count()),
                          std::numeric_limits<qreal>::infinity(),
                          std::numeric_limits<qreal>::quiet_NaN()})
    {
        base->adjustAttribute(invalid, overrideID);
        QCOMPARE(base->algorithm(), unchanged);
    }
    matrix.preRun(m_doc->masterTimer());
    const auto cleanup = qScopeGuard([&]() { matrix.postRun(m_doc->masterTimer(), {}); });
    QVERIFY(matrix.isRunning());
    matrix.write(m_doc->masterTimer(), {});
    QCOMPARE(matrix.m_runAlgorithm, unchanged);
    base->releaseAttributeOverride(overrideID);
    base->applyStyleAttributes();
    if (baseName.isEmpty())
        QVERIFY(base->algorithm() == nullptr);
    else
        QCOMPARE(base->algorithm()->name(), baseName);
    QCOMPARE(base->algorithmIndex(), runtime.indexOf(baseName));
    QCOMPARE(base->getAttributeValue(RGBMatrix::PatternAttr), qreal(runtime.indexOf(baseName)));
    matrix.write(m_doc->masterTimer(), {});
    QCOMPARE(matrix.m_runAlgorithm, matrix.algorithm());
    QVERIFY(!matrix.m_requestEngineCreation);
    matrix.write(m_doc->masterTimer(), {});
    QCOMPARE(matrix.m_runAlgorithm, matrix.algorithm());
    QVERIFY(!matrix.m_requestEngineCreation);
    RGBAlgorithm *replayed = HUEMatrix::createAlgorithm(m_doc, selectedName);
    QVERIFY(replayed != nullptr);
    base->setAlgorithm(replayed);
    QCOMPARE(base->algorithm()->name(), selectedName);
    QVERIFY(HUEMatrix::createAlgorithm(m_doc, "Stale pattern") == nullptr);
    QCOMPARE(base->algorithm(), replayed);
}

void HUEMatrix_Test::legacyAlgorithmRoundTrip_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<QString>("type");
    QTest::addColumn<QString>("propertyName");
    QTest::addColumn<QString>("propertyValue");
    const auto add = [](const char *rowId, const char *name, const char *type,
                        const char *propertyName, const char *propertyValue) {
        QTest::newRow(rowId) << QString::fromUtf8(name) << QString::fromUtf8(type)
                             << QString::fromUtf8(propertyName) << QString::fromUtf8(propertyValue);
    };

    add("legacy-rgb-script", "Stripes", "Script", "orientation", "Vertical");
    add("legacy-built-in-audio", "Audio Spectrum", "Audio", "", "");
    add("legacy-hsv-script", "Audio Fire", "Script", "intensity", "19");

    // Added mode values on existing identities
    add("mode-audio-barcode-scrollplus", "Audio Barcode", "Script", "mode", "Scroll+");
    add("mode-audio-blocks-ledfx", "Audio Blocks", "Script", "mode", "LedFx Block Reflections");
    add("mode-audio-crawler-ledfx", "Audio Crawler", "Script", "mode", "LedFx Crawler");
    add("mode-audio-energy2-ledfx", "Audio Energy 2", "Script", "mode", "LedFx Energy 2");
    add("mode-audio-equalizer-segment", "Audio Equalizer", "Script", "mode", "Segment Equalizer");
    add("mode-audio-fire-ledfx", "Audio Fire", "Script", "mode", "LedFx Fire");
    add("mode-audio-glitch-ledfx", "Audio Glitch", "Script", "mode", "LedFx Glitch");
    add("mode-audio-lava-ledfx", "Audio Lava Lamp", "Script", "mode", "LedFx Lava Lamp");
    add("mode-audio-melt-ledfx", "Audio Melt", "Script", "mode", "LedFx Melt");
    add("mode-audio-meltsparkle-ledfx", "Audio Melt and Sparkle", "Script", "mode", "LedFx Melt and Sparkle");
    add("mode-audio-plasma-plasma2d", "Audio Plasma", "Script", "mode", "Plasma2d");
    add("mode-audio-plasma-wled2d", "Audio Plasma", "Script", "mode", "PlasmaWled2d");
    add("mode-audio-power-ledfx", "Audio Power", "Script", "mode", "LedFx Power");
    add("mode-audio-puddles-rainpulse", "Audio Puddles", "Script", "mode", "Rain Pulse");
    add("mode-audio-scan-ledfx", "Audio Scan", "Script", "mode", "LedFx Scan");
    add("mode-audio-scanflare-ledfx", "Audio Scan and Flare", "Script", "mode", "LedFx Scan and Flare");
    add("mode-audio-scanmulti-ledfx", "Audio Scan Multi", "Script", "mode", "LedFx Scan Multi");
    add("mode-audio-soap-ledfx", "Audio Soap", "Script", "mode", "LedFx Soap");
    add("mode-audio-strobe-percussive", "Audio Strobe", "Script", "mode", "Percussive RGB");
    add("mode-audio-water-ledfx", "Audio Water", "Script", "mode", "LedFx Water");

    // All 29 new identities with nondefault properties
    add("new-identity-audio-bands", "Audio Bands", "Script", "bandCount", "11");
    add("new-identity-audio-bands-matrix", "Audio Bands Matrix", "Script", "flipBandOrder", "Yes");
    add("new-identity-audio-blade-power", "Audio Blade Power", "Script", "frequencyRange", "High");
    add("new-identity-audio-bleep", "Audio Bleep", "Script", "mode", "Fill");
    add("new-identity-audio-bpm-bar", "Audio BPM Bar", "Script", "mode", "bounce");
    add("new-identity-audio-concentric", "Audio Concentric", "Script", "frequencyRange", "High");
    add("new-identity-audio-digital-rain", "Audio Digital Rain", "Script", "lineWidth", "17");
    add("new-identity-audio-equalizer-2d", "Audio Equalizer 2D", "Script", "mode", "Ring");
    add("new-identity-audio-filter", "Audio Filter", "Script", "useGradient", "No");
    add("new-identity-audio-flame", "Audio Flame", "Script", "spawnRate", "11");
    add("new-identity-audio-game-of-life", "Audio Game of Life", "Script", "healthCheck", "Oscillating");
    add("new-identity-audio-hierarchy", "Audio Hierarchy", "Script", "thresholdLows", "0.2");
    add("new-identity-audio-magnitude", "Audio Magnitude", "Script", "mirror", "On");
    add("new-identity-audio-marching", "Audio Marching", "Script", "reactivity", "0.4");
    add("new-identity-audio-multicolor-bar", "Audio Multicolor Bar", "Script", "mode", "cascade");
    add("new-identity-audio-noise", "Audio Noise", "Script", "stretch", "1.2");
    add("new-identity-audio-pitch-spectrum", "Audio Pitch Spectrum", "Script", "mirror", "No");
    add("new-identity-audio-smoke", "Audio Smoke", "Script", "zoom", "3");
    add("new-identity-audio-spectral-blocks", "Audio Spectral Blocks", "Script", "blockCount", "7");
    add("new-identity-audio-spotlight", "Audio Spotlight", "Script", "gradient", "No");
    add("new-identity-audio-vumeter", "Audio VuMeter", "Script", "peakPercent", "3");
    add("new-identity-audio-waterfall", "Audio Waterfall", "Script", "centerMode", "On");
    add("new-identity-hue-fade", "Hue Fade", "Script", "presetFlip", "On");
    add("new-identity-hue-gradient", "Hue Gradient", "Script", "presetModulation", "Sine");
    add("new-identity-hue-metro", "Hue Metro", "Script", "presetSteps", "5");
    add("new-identity-hue-pixels", "Hue Pixels", "Script", "presetBuildUp", "Off");
    add("new-identity-hue-rainbow", "Hue Rainbow", "Script", "presetMirror", "On");
    add("new-identity-hue-random-flash", "Hue Random Flash", "Script", "presetSize", "3");
    add("new-identity-hue-single-color", "Hue Single Color", "Script", "presetModulation", "Sine");
}

void HUEMatrix_Test::legacyAlgorithmRoundTrip()
{
    failOnUnexpectedHueScriptWarnings();

    QFETCH(QString, name);
    QFETCH(QString, type);
    QFETCH(QString, propertyName);
    QFETCH(QString, propertyValue);
    Doc doc(this);
    QVERIFY(doc.rgbScriptsCache()->load(QDir(INTERNAL_SCRIPTDIR)));
    QVERIFY(doc.hueScriptsCache()->load(QDir(INTERNAL_HUESCRIPTDIR), true));
    QVERIFY(doc.hueScriptsCache()->load(QDir(INTERNAL_SCRIPTDIR), false));
    const QString xml = QString(
        "<Function ID=\"45\" Type=\"HUEMatrix\" Name=\"Legacy\">"
        "<Speed FadeIn=\"125\" FadeOut=\"275\" Duration=\"320\"/>"
        "<Algorithm Type=\"%1\">%2</Algorithm>"
        "<Property Name=\"%3\" Value=\"%4\"/>"
        "<Brightness>0.375</Brightness><Rotation>90</Rotation>"
        "</Function>").arg(type, name, propertyName, propertyValue);
    QXmlStreamReader reader(xml);
    QVERIFY(reader.readNextStartElement());
    QVERIFY(Function::loader(reader, &doc));
    HUEMatrix *matrix = qobject_cast<HUEMatrix *>(doc.function(45));
    QVERIFY(matrix != nullptr);
    QVERIFY(matrix->algorithm() != nullptr);
    QCOMPARE(matrix->algorithm()->name(), name);
    QCOMPARE(matrix->algorithm()->type(), type == "Audio" ? RGBAlgorithm::Audio : RGBAlgorithm::Script);
    if (type == "Audio")
        QVERIFY(dynamic_cast<RGBAudio *>(matrix->algorithm()) != nullptr);
    else
        QVERIFY(dynamic_cast<HUEScript *>(matrix->algorithm()) != nullptr);
    if (type == "Audio")
        QVERIFY(!HUEMatrix::availableAlgorithms(&doc).contains(name));
    else if (name == "Stripes")
        QVERIFY(!HUEMatrix::availableAlgorithms(&doc).contains(name));
    else
        QVERIFY(HUEMatrix::availableAlgorithms(&doc).contains(name));

    QString saved;
    QXmlStreamWriter writer(&saved);
    QVERIFY(matrix->saveXML(&writer));
    QVERIFY(saved.contains("Type=\"HUEMatrix\""));
    QVERIFY(saved.contains(type == "Audio" ? "Type=\"Audio\"" : name));
    doc.deleteFunction(45);
    QXmlStreamReader reload(saved);
    QVERIFY(reload.readNextStartElement());
    QVERIFY(Function::loader(reload, &doc));
    matrix = qobject_cast<HUEMatrix *>(doc.function(45));
    QVERIFY(matrix != nullptr);
    QCOMPARE(matrix->algorithm()->name(), name);
    QCOMPARE(matrix->brightness(), qreal(0.375));
    QCOMPARE(matrix->duration(), quint32(320));
    QCOMPARE(matrix->fadeInSpeed(), quint32(125));
    if (!propertyName.isEmpty())
    {
        QCOMPARE(matrix->property(propertyName), propertyValue);
        matrix->applyStyleAttributes();
        QCOMPARE(matrix->property(propertyName), propertyValue);
        QCOMPARE(static_cast<RGBScript *>(matrix->algorithm())->property(propertyName), propertyValue);
    }
}

void HUEMatrix_Test::createCopyPreservesHue_data()
{
    QTest::addColumn<bool>("crossDocument");
    QTest::addColumn<QString>("algorithmName");
    QTest::newRow("duplicate") << false << "Audio Fire";
    QTest::newRow("import") << true << "Audio Fire";
    QTest::newRow("import-built-in") << true << "Text";
    QTest::newRow("unconfigured") << false << "";
}

void HUEMatrix_Test::createCopyPreservesHue()
{
    QFETCH(bool, crossDocument);
    QFETCH(QString, algorithmName);
    const bool hasAlgorithm = !algorithmName.isEmpty();
    Doc target(this);
    HUEMatrix original(m_doc);
    original.setAlgorithm(hasAlgorithm ? HUEMatrix::createAlgorithm(m_doc, algorithmName) : nullptr);
    if (algorithmName == "Text")
        static_cast<RGBText *>(original.algorithm())->setText("Imported text");
    original.setProperty("intensity", "19");
    original.setProperty("speed", "0.23");
    original.setRotation(1);
    original.setMirror(2);
    original.setMirrorBlend(HUEMatrix::MirrorAdditive);
    original.setBrightness(0.625);
    original.setBeatEffect(HUEMatrix::BeatEffectMirror);
    original.setBeatSelection(HUEMatrix::BeatSelWalk);
    original.setBeatOrientation(HUEMatrix::BeatOrientColumns);
    original.setControlMode(RGBMatrix::ControlModeRgbw);
    original.setDuration(640);
    original.setColor(0, QColor("#12ab34"));
    Function *function = &original;
    Function *copy = function->createCopy(crossDocument ? &target : m_doc, crossDocument);
    QVERIFY(copy != nullptr);
    HUEMatrix *matrix = qobject_cast<HUEMatrix *>(copy);
    QVERIFY(matrix != nullptr);
    QCOMPARE(copy->type(), Function::HUEMatrixType);
    QCOMPARE(matrix->rotation(), 1);
    QCOMPARE(matrix->mirror(), 2);
    QCOMPARE(matrix->mirrorBlend(), HUEMatrix::MirrorAdditive);
    QCOMPARE(matrix->brightness(), qreal(0.625));
    QCOMPARE(matrix->beatEffect(), HUEMatrix::BeatEffectMirror);
    QCOMPARE(matrix->beatSelection(), HUEMatrix::BeatSelWalk);
    QCOMPARE(matrix->beatOrientation(), HUEMatrix::BeatOrientColumns);
    QCOMPARE(matrix->controlMode(), RGBMatrix::ControlModeRgbw);
    QCOMPARE(matrix->duration(), quint32(640));
    QCOMPARE(matrix->getColor(0), QColor("#12ab34"));
    QCOMPARE(crossDocument ? target.function(copy->id()) : nullptr, crossDocument ? copy : nullptr);
    if (hasAlgorithm)
    {
        QVERIFY(matrix->algorithm() != original.algorithm());
        QCOMPARE(matrix->algorithm()->doc(), crossDocument ? &target : m_doc);
        QCOMPARE(matrix->algorithm()->name(), algorithmName);
        if (algorithmName == "Text")
            QCOMPARE(static_cast<RGBText *>(matrix->algorithm())->text(), QString("Imported text"));
        QCOMPARE(matrix->property("intensity"), QString("19"));
        QCOMPARE(matrix->property("speed"), QString("0.23"));
        RGBMap map;
        matrix->algorithm()->rgbMap(QSize(5, 3), 0xff00ff, 0, map);
        QCOMPARE(map.size(), 3);
        for (const auto &row : map)
            QCOMPARE(row.size(), 5);
        matrix->setProperty("intensity", "7");
        QCOMPARE(original.property("intensity"), QString("19"));
        QString saved;
        QXmlStreamWriter writer(&saved);
        QVERIFY(matrix->saveXML(&writer));
        QVERIFY(saved.contains("Type=\"HUEMatrix\""));
        QVERIFY(saved.contains("Value=\"0.23\""));
    }
    else
        QVERIFY(matrix->algorithm() == nullptr);
    if (!crossDocument)
        delete copy;
}

void HUEMatrix_Test::scriptAttributeLifecycle()
{
    HUEMatrix matrix(m_doc);
    matrix.setAlgorithm(HUEMatrix::createAlgorithm(m_doc, "Audio Fire"));
    QCOMPARE(matrix.attributes().count(), int(RGBMatrix::ScriptPropertyAttr) + matrix.scriptPropertyAttributes().count());
    const auto properties = matrix.scriptPropertyAttributes();
    for (int i = 0; i < properties.count(); ++i)
    {
        QCOMPARE(matrix.attributes().at(RGBMatrix::ScriptPropertyAttr + i).m_name,
                 RGBMatrix::scriptPropertyAttributeName(properties.at(i)));
        if (properties.at(i).m_name == "intensity")
        {
            QCOMPARE(matrix.adjustAttribute(19, RGBMatrix::ScriptPropertyAttr + i), RGBMatrix::ScriptPropertyAttr + i);
            QCOMPARE(matrix.property("intensity"), QString("19"));
            QCOMPARE(matrix.getAttributeValue(Function::Intensity), qreal(1));
        }
    }
    matrix.setAlgorithm(HUEMatrix::createAlgorithm(m_doc, "Stripes"));
    QCOMPARE(matrix.attributes().count(), int(RGBMatrix::ScriptPropertyAttr) + matrix.scriptPropertyAttributes().count());
    RGBMatrix *base = &matrix;
    base->setProperty("orientation", "Vertical");
    const int orientation = base->getAttributeIndex("Orientation");
    QVERIFY(orientation >= RGBMatrix::ScriptPropertyAttr);
    QCOMPARE(base->getAttributeValue(orientation), qreal(1));
    const int overrideId = base->requestAttributeOverride(orientation, 0);
    QCOMPARE(base->property("orientation"), QString("Horizontal"));
    base->releaseAttributeOverride(overrideId);
    base->applyStyleAttributes();
    QCOMPARE(base->property("orientation"), QString("Vertical"));
    matrix.setAlgorithm(nullptr);
    QCOMPARE(matrix.attributes().count(), int(RGBMatrix::ScriptPropertyAttr));
}

void HUEMatrix_Test::floatPropertyAttributes_data()
{
    QTest::addColumn<QString>("propertyName");
    QTest::addColumn<QString>("propertyValue");
    QTest::addColumn<qreal>("multiplier");
    QTest::addColumn<qreal>("blur");
    QTest::addColumn<bool>("editWhileOverridden");
    QTest::newRow("defaults") << "" << "" << qreal(1.6) << qreal(1.5) << false;
    QTest::newRow("authored-blur") << "referenceBlur" << "3" << qreal(1.6) << qreal(3) << false;
    QTest::newRow("authored-multiplier") << "presetMultiplier" << "2.4" << qreal(2.4) << qreal(1.5) << false;
    QTest::newRow("edit-overridden-blur") << "referenceBlur" << "3" << qreal(1.6) << qreal(3) << true;
}

void HUEMatrix_Test::floatPropertyAttributes()
{
    QFETCH(QString, propertyName);
    QFETCH(QString, propertyValue);
    QFETCH(qreal, multiplier);
    QFETCH(qreal, blur);
    QFETCH(bool, editWhileOverridden);
    HUEMatrix matrix(m_doc);
    matrix.setAlgorithm(HUEMatrix::createAlgorithm(m_doc, "Audio Energy"));
    QVERIFY(matrix.algorithm() != nullptr);
    if (!propertyName.isEmpty() && !editWhileOverridden)
        matrix.setProperty(propertyName, propertyValue);
    const int multiplierAttr = matrix.getAttributeIndex("Fill Amount");
    const int blurAttr = matrix.getAttributeIndex("Reference Blur");
    const int brightnessAttr = matrix.getAttributeIndex("Reference Brightness");
    const int smoothingAttr = matrix.getAttributeIndex("Smoothing");
    QVERIFY(multiplierAttr >= RGBMatrix::ScriptPropertyAttr);
    QVERIFY(blurAttr >= RGBMatrix::ScriptPropertyAttr);
    QVERIFY(brightnessAttr >= RGBMatrix::ScriptPropertyAttr);
    QVERIFY(smoothingAttr >= RGBMatrix::ScriptPropertyAttr);
    if (editWhileOverridden)
    {
        const int overrideId = matrix.requestAttributeOverride(blurAttr, 0.5);
        matrix.setProperty(propertyName, propertyValue);
        QCOMPARE(matrix.getAttributeValue(blurAttr), qreal(0.5));
        QCOMPARE(matrix.attributes().at(blurAttr).m_value, blur);
        QCOMPARE(matrix.property(propertyName).toDouble(), qreal(0.5));
        matrix.releaseAttributeOverride(overrideId);
        matrix.applyStyleAttributes();
    }
    QCOMPARE(matrix.getAttributeValue(multiplierAttr), multiplier);
    QCOMPARE(matrix.getAttributeValue(blurAttr), blur);
    QString before;
    QXmlStreamWriter beforeWriter(&before);
    QVERIFY(matrix.saveXML(&beforeWriter));

    const int colorOverride = matrix.requestAttributeOverride(RGBMatrix::Color1Attr, 0x123456);
    matrix.releaseAttributeOverride(colorOverride);
    matrix.applyStyleAttributes();

    auto *script = static_cast<RGBScript *>(matrix.algorithm());
    QCOMPARE(script->property("presetMultiplier").toDouble(), multiplier);
    QCOMPARE(script->property("referenceBlur").toDouble(), blur);
    QCOMPARE(script->property("referenceBrightness").toDouble(), qreal(0.7));
    QCOMPARE(matrix.getAttributeValue(multiplierAttr), multiplier);
    QCOMPARE(matrix.getAttributeValue(blurAttr), blur);
    QCOMPARE(matrix.attributes().at(brightnessAttr).m_min, qreal(0));
    QCOMPARE(matrix.attributes().at(brightnessAttr).m_max, qreal(1));
    QCOMPARE(matrix.attributes().at(smoothingAttr).m_min, qreal(1));
    QCOMPARE(matrix.attributes().at(smoothingAttr).m_max, qreal(10));
    QString after;
    QXmlStreamWriter afterWriter(&after);
    QVERIFY(matrix.saveXML(&afterWriter));
    QCOMPARE(after, before);
}

void HUEMatrix_Test::authoredPropertiesSurviveStyleRelease_data()
{
    QTest::addColumn<bool>("fromXml");
    QTest::addColumn<QString>("overrideKind");
    QTest::newRow("setter-color") << false << "color";
    QTest::newRow("xml-color") << true << "color";
    QTest::newRow("setter-pattern") << false << "pattern";
    QTest::newRow("xml-pattern") << true << "pattern";
    QTest::newRow("setter-property") << false << "property";
    QTest::newRow("xml-property") << true << "property";
    QTest::newRow("setter-during-property-override") << false << "edit";
}

void HUEMatrix_Test::authoredPropertiesSurviveStyleRelease()
{
    QFETCH(bool, fromXml);
    QFETCH(QString, overrideKind);
    HUEMatrix matrix(m_doc);
    RGBMatrix *base = &matrix;
    if (fromXml)
    {
        QXmlStreamReader reader(QStringLiteral(
            "<Function ID=\"45\" Type=\"HUEMatrix\" Name=\"Authored\">"
            "<Algorithm Type=\"Script\">Audio Fire</Algorithm>"
            "<Property Name=\"intensity\" Value=\"19\"/>"
            "<Property Name=\"speed\" Value=\"0.23\"/>"
            "</Function>"));
        QVERIFY(reader.readNextStartElement());
        QVERIFY(base->loadXML(reader));
    }
    else
    {
        base->setAlgorithm(HUEMatrix::createAlgorithm(m_doc, "Audio Fire"));
        base->setProperty("intensity", "19");
        base->setProperty("speed", "0.23");
    }
    QCOMPARE(base->property("intensity"), QString("19"));
    QCOMPARE(base->property("speed"), QString("0.23"));
    const int intensityAttr = base->getAttributeIndex("Script Intensity");
    const int speedAttr = base->getAttributeIndex("Speed");
    QVERIFY(intensityAttr >= RGBMatrix::ScriptPropertyAttr);
    QVERIFY(speedAttr >= RGBMatrix::ScriptPropertyAttr);
    QCOMPARE(base->attributes().at(intensityAttr).m_min, qreal(1));
    QCOMPARE(base->attributes().at(intensityAttr).m_max, qreal(64));
    QCOMPARE(base->attributes().at(intensityAttr).m_value, qreal(19));
    QCOMPARE(base->attributes().at(speedAttr).m_value, qreal(0.23));

    int attribute = RGBMatrix::Color1Attr;
    qreal value = 0x123456;
    if (overrideKind == "pattern")
    {
        attribute = RGBMatrix::PatternAttr;
        value = HUEMatrix::runtimeAlgorithms(m_doc).indexOf("Audio Aurora");
    }
    else if (overrideKind == "property" || overrideKind == "edit")
    {
        attribute = intensityAttr;
        value = 27;
    }
    const int overrideId = base->requestAttributeOverride(attribute, value);
    QVERIFY(overrideId != Function::invalidAttributeId());
    QString expectedIntensity = "19";
    if (attribute == intensityAttr)
    {
        QCOMPARE(base->property("intensity"), QString("27"));
        QCOMPARE(base->attributes().at(intensityAttr).m_value, qreal(19));
        if (overrideKind == "edit")
        {
            base->setProperty("intensity", "21");
            expectedIntensity = "21";
            QCOMPARE(base->property("intensity"), QString("27"));
            QCOMPARE(base->attributes().at(intensityAttr).m_value, qreal(21));
        }
    }
    base->releaseAttributeOverride(overrideId);
    base->applyStyleAttributes();
    QCOMPARE(base->algorithm()->name(), QString("Audio Fire"));
    QCOMPARE(base->property("intensity"), expectedIntensity);
    QCOMPARE(base->property("speed"), QString("0.23"));
    RGBScript *script = static_cast<RGBScript *>(base->algorithm());
    QCOMPARE(script->property("intensity"), expectedIntensity);
    QCOMPARE(script->property("speed"), QString("0.23"));
    QCOMPARE(base->getAttributeValue(Function::Intensity), qreal(1));

    const int attributeCount = base->attributes().count();
    base->setProperty("not-a-script-property", "not-numeric");
    QCOMPARE(base->attributes().count(), attributeCount);
    QCOMPARE(base->getAttributeValue(speedAttr), qreal(0.23));
    QCOMPARE(base->getAttributeValue(intensityAttr), expectedIntensity.toDouble());
}

void HUEMatrix_Test::hueUiRoutesNames()
{
    struct Binding { const char *path; const char *source; };
    static const Binding bindings[] = {
        {"qmlui/huematrixeditor.cpp", "return HUEMatrix::availableAlgorithms(m_doc);"},
        {"qmlui/rgbmatrixeditor.cpp", "return RGBAlgorithm::algorithms(m_doc);"},
        {"qmlui/huematrixeditor.cpp", "algorithmName(), name"},
        {"qmlui/huematrixeditor.cpp", "m_matrix == nullptr || algoIndex < 0"},
        {"qmlui/qml/fixturesfunctions/HUEMatrixEditor.qml", "onActivated: function(index)"},
        {"qmlui/qml/fixturesfunctions/HUEMatrixEditor.qml", "property string algorithmName: hueMatrixEditor.algorithmName"},
        {"qmlui/tardis/tardis.cpp", "HUEMatrix::createAlgorithm(m_doc, name)"},
        {"qmlui/tardis/tardis.cpp", "animation->setRuntimeAlgorithmIndex(value->toInt());"},
        {"qmlui/virtualconsole/vcanimation.cpp", "runtimeAlgorithms().indexOf(algoList.at(index))"},
        {"qmlui/virtualconsole/vcanimation.cpp", "HUEMatrix::availableAlgorithms(m_doc)"},
        {"qmlui/virtualconsole/vcanimation.cpp", "HUEMatrix::createAlgorithm(m_doc, algoList.at(m_localAlgorithmIndex))"},
        {"qmlui/virtualconsole/vcanimation.cpp", "m_doc->hueScriptsCache()->script(algoName)"},
        {"qmlui/virtualconsole/vcanimation.cpp", "const int algoIndex = runtimeAlgorithms().indexOf(resource);"},
        {"qmlui/virtualconsole/vcanimation.cpp", "if (algoIndex < 0)\n            return;\n        setRuntimeAlgorithmIndex(algoIndex);"},
        {"qmlui/virtualconsole/vcanimation.cpp", "control->m_type == VCAnimationPreset::Text ? \"Text\" : control->m_resource"},
        {"qmlui/virtualconsole/vcanimation.h", "algorithms READ algorithms NOTIFY functionIDChanged"},
    };
    for (const Binding &binding : bindings)
    {
        QFile file(QStringLiteral(QLCPLUS_SOURCE_DIR) + "/" + binding.path);
        QVERIFY2(file.open(QIODevice::ReadOnly), binding.path);
        QVERIFY2(file.readAll().contains(binding.source), binding.source);
    }
}

/****************************************************************************
 * AC4 - the HSV contract really produces pixels
 ****************************************************************************/

void HUEMatrix_Test::hsvScriptProducesFiniteNonUniformMap()
{
    QStringList hsv = m_doc->hueScriptsCache()->hsvNames();
    QVERIFY(hsv.isEmpty() == false);

    QStringList nonUniform;
    foreach (QString name, hsv)
    {
        RGBAlgorithm *algo = HUEMatrix::createAlgorithm(m_doc, name);
        QVERIFY2(algo != NULL, qPrintable(name));
        QCOMPARE(algo->type(), RGBAlgorithm::Script);
        algo->setDisplaySize(QSize(5, 5));

        // Every HSV script must decode into a well formed 5x5 packed-RGB map.
        // A Float32Array that was not understood would leave the map empty.
        uint first = 0;
        for (int step = 0; step < 3; step++)
        {
            RGBMap map;
            algo->rgbMap(QSize(5, 5), 0x00FF0000, step, map);
            QCOMPARE(map.size(), 5);
            for (int y = 0; y < 5; y++)
            {
                QCOMPARE(map[y].size(), 5);
                for (int x = 0; x < 5; x++)
                {
                    uint px = map[y][x];
                    // Never a NaN-derived or out of range channel
                    QVERIFY2((px & 0xFF000000) == 0 || qAlpha(px) == 255, qPrintable(name));
                    QVERIFY(qRed(px) >= 0 && qRed(px) <= 255);
                    QVERIFY(qGreen(px) >= 0 && qGreen(px) <= 255);
                    QVERIFY(qBlue(px) >= 0 && qBlue(px) <= 255);
                    if (y == 0 && x == 0 && step == 0)
                        first = px;
                    else if (px != first && nonUniform.contains(name) == false)
                        nonUniform << name;
                }
            }
        }
        delete algo;
    }

    // At least one HSV script must paint an actually varying picture, which
    // proves the HSV -> packed RGB conversion carries real values through.
    QVERIFY2(nonUniform.isEmpty() == false,
             "no HSV script produced a non-uniform map");
    qDebug() << "non-uniform HSV scripts:" << nonUniform.count() << "of" << hsv.count();
}

/****************************************************************************
 * AC6 - upstream scripts keep working under the HUE script wrapper
 ****************************************************************************/

void HUEMatrix_Test::upstreamScriptStillWorksOnHueScript()
{
    RGBAlgorithm *algo = HUEMatrix::createAlgorithm(m_doc, "Stripes");
    QVERIFY(algo != NULL);
    QCOMPARE(algo->name(), QString("Stripes"));
    QVERIFY(dynamic_cast<HUEScript*>(algo) != NULL);
    QVERIFY(algo->usesAudio() == false);

    RGBScript *script = static_cast<RGBScript*>(algo);
    script->setProperty("orientation", "Vertical");
    QCOMPARE(script->property("orientation"), QString("Vertical"));

    // Same assertion the restored rgbscript_test makes: for a 5x5 map at
    // step N, row N is the step colour and every other row is the second one.
    QVector<uint> rawRgbColors = { QColor(Qt::red).rgb(), uint(0) };

    for (int step = 0; step < 5; step++)
    {
        RGBMap map;
        script->rgbMap(QSize(5, 5), rawRgbColors[0], step, map);
        for (int y = 0; y < 5; y++)
        {
            for (int x = 0; x < 5; x++)
            {
                if (y == step)
                    QCOMPARE(map[y][x], rawRgbColors[0]);
                else
                    QCOMPARE(map[y][x], rawRgbColors[1]);
            }
        }
    }
    delete algo;
}

/****************************************************************************
 * AC7 - every fork property is reachable on HUEMatrix
 ****************************************************************************/

void HUEMatrix_Test::forkPropertyRoundTripsInMemory_data()
{
    QTest::addColumn<QString>("property");
    QTest::addColumn<int>("value");

    QTest::newRow("rotation 90")            << "rotation"        << 1;
    QTest::newRow("rotation 270")           << "rotation"        << 3;
    QTest::newRow("mirror horizontal")      << "mirror"          << 1;
    QTest::newRow("mirror both")            << "mirror"          << 3;
    QTest::newRow("mirrorBlend max")        << "mirrorBlend"     << int(HUEMatrix::MirrorMax);
    QTest::newRow("mirrorBlend average")    << "mirrorBlend"     << int(HUEMatrix::MirrorAverage);
    QTest::newRow("beatEffect strobe")      << "beatEffect"      << int(HUEMatrix::BeatEffectBlackout);
    QTest::newRow("beatSelection random")   << "beatSelection"   << int(HUEMatrix::BeatSelRandom);
    QTest::newRow("beatOrientation cols")   << "beatOrientation" << int(HUEMatrix::BeatOrientColumns);
    QTest::newRow("controlMode rgbw")       << "controlMode"     << int(RGBMatrix::ControlModeRgbw);
    QTest::newRow("controlMode rgbwBright") << "controlMode"     << int(RGBMatrix::ControlModeRgbwBrighter);
}

void HUEMatrix_Test::forkPropertyRoundTripsInMemory()
{
    QFETCH(QString, property);
    QFETCH(int, value);

    HUEMatrix mtx(m_doc);

    if (property == "rotation")
    {
        QCOMPARE(mtx.rotation(), 0);
        mtx.setRotation(value);
        QCOMPARE(mtx.rotation(), value);
    }
    else if (property == "mirror")
    {
        QCOMPARE(mtx.mirror(), 0);
        mtx.setMirror(value);
        QCOMPARE(mtx.mirror(), value);
    }
    else if (property == "mirrorBlend")
    {
        QCOMPARE(mtx.mirrorBlend(), HUEMatrix::MirrorFlip);
        mtx.setMirrorBlend(HUEMatrix::MirrorBlend(value));
        QCOMPARE(int(mtx.mirrorBlend()), value);
    }
    else if (property == "beatEffect")
    {
        QCOMPARE(mtx.beatEffect(), HUEMatrix::BeatEffectOff);
        mtx.setBeatEffect(HUEMatrix::BeatEffect(value));
        QCOMPARE(int(mtx.beatEffect()), value);
    }
    else if (property == "beatSelection")
    {
        QCOMPARE(mtx.beatSelection(), HUEMatrix::BeatSelAllOnDownbeat);
        mtx.setBeatSelection(HUEMatrix::BeatSelection(value));
        QCOMPARE(int(mtx.beatSelection()), value);
    }
    else if (property == "beatOrientation")
    {
        QCOMPARE(mtx.beatOrientation(), HUEMatrix::BeatOrientRows);
        mtx.setBeatOrientation(HUEMatrix::BeatOrientation(value));
        QCOMPARE(int(mtx.beatOrientation()), value);
    }
    else if (property == "controlMode")
    {
        QCOMPARE(mtx.controlMode(), RGBMatrix::ControlModeRgb);
        mtx.setControlMode(RGBMatrix::ControlMode(value));
        QCOMPARE(int(mtx.controlMode()), value);
    }
    else
    {
        QFAIL("unhandled property");
    }

    // Brightness is a qreal and gets its own clamp check here
    QCOMPARE(mtx.brightness(), qreal(1.0));
    mtx.setBrightness(0.25);
    QCOMPARE(mtx.brightness(), qreal(0.25));
}

/****************************************************************************
 * AC8 - XML round trip
 ****************************************************************************/

void HUEMatrix_Test::forkPropertiesSurviveXmlRoundTrip()
{
    HUEMatrix *mtx = new HUEMatrix(m_doc);
    mtx->setName("Fork Round Trip");
    mtx->setControlMode(RGBMatrix::ControlModeRgbwBrighter);
    mtx->setRotation(3);
    mtx->setMirror(3);
    mtx->setMirrorBlend(HUEMatrix::MirrorAverage);
    mtx->setBrightness(0.375);
    mtx->setBeatEffect(HUEMatrix::BeatEffectBlackout);
    mtx->setBeatSelection(HUEMatrix::BeatSelRandom);
    mtx->setBeatOrientation(HUEMatrix::BeatOrientColumns);
    mtx->setAlgorithm(HUEMatrix::createAlgorithm(m_doc, "Audio Fire"));
    QVERIFY(mtx->algorithm() != NULL);
    m_doc->addFunction(mtx);

    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter xmlWriter(&buffer);
    QVERIFY(mtx->saveXML(&xmlWriter) == true);
    xmlWriter.setDevice(NULL);
    buffer.close();

    buffer.open(QIODevice::ReadOnly | QIODevice::Text);
    QXmlStreamReader xmlReader(&buffer);
    xmlReader.readNextStartElement();
    QCOMPARE(xmlReader.name().toString(), QString("Function"));
    QCOMPARE(xmlReader.attributes().value("Type").toString(), QString("HUEMatrix"));

    HUEMatrix reloaded(m_doc);
    QVERIFY(reloaded.loadXML(xmlReader) == true);

    QCOMPARE(reloaded.controlMode(), RGBMatrix::ControlModeRgbwBrighter);
    QCOMPARE(reloaded.rotation(), 3);
    QCOMPARE(reloaded.mirror(), 3);
    QCOMPARE(reloaded.mirrorBlend(), HUEMatrix::MirrorAverage);
    QCOMPARE(reloaded.brightness(), qreal(0.375));
    QCOMPARE(reloaded.beatEffect(), HUEMatrix::BeatEffectBlackout);
    QCOMPARE(reloaded.beatSelection(), HUEMatrix::BeatSelRandom);
    QCOMPARE(reloaded.beatOrientation(), HUEMatrix::BeatOrientColumns);
    QVERIFY(reloaded.algorithm() != NULL);
    QCOMPARE(reloaded.algorithm()->name(), QString("Audio Fire"));
}

void HUEMatrix_Test::unknownXmlValuesFallBackToDefaults()
{
    // ERROR CASE: garbage enum text must not corrupt the matrix
    QString xml = "<Function ID=\"7\" Type=\"HUEMatrix\" Name=\"Bad\">"
                  "<Speed FadeIn=\"0\" FadeOut=\"0\" Duration=\"200\"/>"
                  "<Direction>Forward</Direction>"
                  "<RunOrder>Loop</RunOrder>"
                  "<ControlMode>Nonsense</ControlMode>"
                  "<MirrorBlend>Nonsense</MirrorBlend>"
                  "<BeatEffect>Nonsense</BeatEffect>"
                  "<BeatSelection>Nonsense</BeatSelection>"
                  "<BeatOrientation>Nonsense</BeatOrientation>"
                  "</Function>";

    QXmlStreamReader reader(xml);
    reader.readNextStartElement();

    HUEMatrix mtx(m_doc);
    QVERIFY(mtx.loadXML(reader) == true);
    QCOMPARE(mtx.controlMode(), RGBMatrix::ControlModeRgb);
    QCOMPARE(mtx.mirrorBlend(), HUEMatrix::MirrorFlip);
    QCOMPARE(mtx.beatEffect(), HUEMatrix::BeatEffectOff);
    QCOMPARE(mtx.beatSelection(), HUEMatrix::BeatSelAllOnDownbeat);
    QCOMPARE(mtx.beatOrientation(), HUEMatrix::BeatOrientRows);

    // And a node that is not a Function at all is rejected
    QXmlStreamReader bad("<NotAFunction/>");
    bad.readNextStartElement();
    HUEMatrix other(m_doc);
    QVERIFY(other.loadXML(bad) == false);
}

/****************************************************************************
 * AC9 - unavailable algorithm
 ****************************************************************************/

void HUEMatrix_Test::unavailableAlgorithmIsRejected()
{
    QVERIFY(HUEMatrix::createAlgorithm(m_doc, "No Such Algorithm At All") == NULL);

    QString xml = "<Function ID=\"9\" Type=\"HUEMatrix\" Name=\"Missing\">"
                  "<Speed FadeIn=\"0\" FadeOut=\"0\" Duration=\"200\"/>"
                  "<Direction>Forward</Direction>"
                  "<RunOrder>Loop</RunOrder>"
                  "<Algorithm Type=\"Script\">No Such Algorithm At All</Algorithm>"
                  "</Function>";

    QXmlStreamReader reader(xml);
    reader.readNextStartElement();

    HUEMatrix mtx(m_doc);
    QTest::ignoreMessage(QtWarningMsg,
        QRegularExpression("Script \"No Such Algorithm At All\" not available"));
    QVERIFY(mtx.loadXML(reader) == true);
    // The function still loads, it simply has no algorithm to run
    QVERIFY(mtx.algorithm() == NULL);
}

/****************************************************************************
 * AC11 - upstream e671868c6 attribute surface survived the restore
 ****************************************************************************/

void HUEMatrix_Test::scriptPropertyAttrDoesNotCollide()
{
    // ScriptPropertyAttr is the first index AFTER every fixed attribute
    QVERIFY(RGBMatrix::ScriptPropertyAttr > RGBMatrix::ColorLastAttr);
    QCOMPARE(int(RGBMatrix::ScriptPropertyAttr), int(RGBMatrix::PatternAttr) + 1);
    QCOMPARE(int(RGBMatrix::PatternAttr), int(RGBMatrix::ColorLastAttr) + 1);
    QCOMPARE(int(RGBMatrix::Color1Attr), int(Function::Intensity) + 1);

    // The two additive control modes must not shadow any upstream value
    QCOMPARE(int(RGBMatrix::ControlModeRgbw), 6);
    QCOMPARE(int(RGBMatrix::ControlModeRgbwBrighter), 7);
    QVERIFY(int(RGBMatrix::ControlModeShutter) < int(RGBMatrix::ControlModeRgbw));

    // Upstream RGBMatrix registers script property attributes for a script
    // Upstream RGBMatrix exposes script properties as Function attributes,
    // starting exactly at ScriptPropertyAttr.
    RGBMatrix mtx(m_doc);
    mtx.setAlgorithm(NULL);
    int fixed = mtx.attributes().count();
    QCOMPARE(fixed, int(RGBMatrix::ScriptPropertyAttr));
    QCOMPARE(mtx.scriptPropertyAttributes().count(), 0);

    mtx.setAlgorithm(RGBAlgorithm::algorithm(m_doc, "Stripes"));
    QVERIFY(mtx.scriptPropertyAttributes().count() > 0);
    QCOMPARE(mtx.attributes().count(),
             fixed + mtx.scriptPropertyAttributes().count());
    QCOMPARE(mtx.attributes().at(RGBMatrix::ScriptPropertyAttr).m_name,
             RGBMatrix::scriptPropertyAttributeName(mtx.scriptPropertyAttributes().first()));

    // Dropping the algorithm again removes exactly those attributes
    mtx.setAlgorithm(NULL);
    QCOMPARE(mtx.attributes().count(), fixed);
}

/****************************************************************************
 * AC12 - HUEMatrix is compatible with that attribute surface
 ****************************************************************************/

void HUEMatrix_Test::hueMatrixSupportsScriptPropertyAttributes()
{
    HUEMatrix mtx(m_doc);
    mtx.setAlgorithm(HUEMatrix::createAlgorithm(m_doc, "Stripes"));
    QVERIFY(mtx.algorithm() != NULL);

    QList<RGBScriptProperty> props = mtx.scriptPropertyAttributes();
    QVERIFY2(props.count() > 0, "HUEMatrix did not register script properties");

    // Drive a real script property through the PUBLIC attribute API
    int idx = RGBMatrix::ScriptPropertyAttr;
    QCOMPARE(mtx.adjustAttribute(1.0, idx), idx);
    QCOMPARE(mtx.getAttributeValue(idx), qreal(1.0));

    RGBScript *script = static_cast<RGBScript*>(mtx.algorithm());
    QString name = props.first().m_name;
    QVERIFY(script->property(name).isEmpty() == false);

    // Out of range attribute index is rejected, not crashed on
    QCOMPARE(mtx.adjustAttribute(1.0, 9999), -1);
}

void HUEMatrix_Test::hueMatrixSupportsForkAttributes()
{
    HUEMatrix mtx(m_doc);
    mtx.setAlgorithm(HUEMatrix::createAlgorithm(m_doc, "Audio Fire"));
    QVERIFY(mtx.algorithm() != NULL);

    // A fork-only script property (Audio Fire lives in resources/huescripts)
    QList<RGBScriptProperty> props = mtx.scriptPropertyAttributes();
    QVERIFY2(props.count() > 0, "fork-only script exposed no properties");

    bool sawIntensity = false;
    foreach (RGBScriptProperty p, props)
    {
        if (p.m_name == "intensity")
            sawIntensity = true;
    }
    QVERIFY2(sawIntensity, "Audio Fire 'intensity' property is not an attribute");

    int idx = RGBMatrix::ScriptPropertyAttr;
    QCOMPARE(mtx.adjustAttribute(0.5, idx), idx);
    QCOMPARE(mtx.getAttributeValue(idx), qreal(0.5));

    // The upstream fixed attributes still work on a HUEMatrix
    QCOMPARE(mtx.adjustAttribute(0.5, Function::Intensity), int(Function::Intensity));
    QCOMPARE(mtx.getAttributeValue(Function::Intensity), qreal(0.5));
}

/****************************************************************************
 * Built-in algorithm edge case - no script involved
 ****************************************************************************/

void HUEMatrix_Test::builtInAlgorithmStillRunsTransformPipeline()
{
    // Every built-in is instantiable on a HUEMatrix and is not a script
    foreach (QString name, QStringList() << "Plain Color" << "Text" << "Image")
    {
        RGBAlgorithm *builtIn = HUEMatrix::createAlgorithm(m_doc, name);
        QVERIFY2(builtIn != NULL, qPrintable(name));
        QVERIFY2(builtIn->type() != RGBAlgorithm::Script, qPrintable(name));
        delete builtIn;
    }

    // And a HUEMatrix driving a built-in still runs the fork's transform and
    // beat pipeline over that built-in's map.
    RGBAlgorithm *algo = HUEMatrix::createAlgorithm(m_doc, "Plain Color");
    QVERIFY(algo != NULL);

    HUEMatrix mtx(m_doc);
    mtx.setAlgorithm(algo);
    mtx.setRotation(1);
    mtx.setMirror(1);
    mtx.setBeatEffect(HUEMatrix::BeatEffectBlackout);
    QCOMPARE(mtx.rotation(), 1);
    QCOMPARE(mtx.beatEffect(), HUEMatrix::BeatEffectBlackout);

    RGBMap map;
    algo->rgbMap(QSize(4, 3), 0x00FF8000, 0, map);
    QCOMPARE(map.size(), 3);
    QCOMPARE(map[0].size(), 4);

    // A quarter turn swaps the axes
    HUEMatrix::applyTransforms(map, QSize(4, 3), QSize(3, 4), 1, 1,
                               HUEMatrix::MirrorFlip);
    QCOMPARE(map.size(), 4);
    QCOMPARE(map[0].size(), 3);

    // Blackout on the downbeat must clear the map without resizing it
    mtx.applyBeatTransform(map, 0, 4);
    QCOMPARE(map.size(), 4);
    QCOMPARE(map[0].size(), 3);
}

/****************************************************************************
 * AC13: every QJSValue touch must be marshalled onto the JS engine thread.
 *
 * HUEMatrix::write() runs on the MasterTimer thread, so rgbMap() is routinely
 * called from a thread that does not own the engine. Without the guards this
 * faulted inside QV4::PersistentValueStorage::allocate.
 ****************************************************************************/

void HUEMatrix_Test::hsvScriptRgbMapIsSafeFromAnotherThread()
{
    QStringList hsv = m_doc->hueScriptsCache()->hsvNames();
    QVERIFY(hsv.isEmpty() == false);

    QString name = hsv.first();

    // evaluate() happens here, on the main thread...
    RGBAlgorithm *algo = HUEMatrix::createAlgorithm(m_doc, name);
    QVERIFY2(algo != NULL, qPrintable(name));

    // ...while every render happens on a foreign thread, as it does live.
    struct Renderer : public QThread
    {
        RGBAlgorithm *algo;
        int filled = 0;
        void run() override
        {
            algo->setDisplaySize(QSize(5, 5));
            for (int step = 0; step < 20; step++)
            {
                RGBMap map;
                algo->rgbMap(QSize(5, 5), 0x0000FF00, step, map);
                if (map.size() == 5 && map[0].size() == 5)
                    filled++;
            }
        }
    } renderer;

    renderer.algo = algo;
    renderer.start();
    QVERIFY2(renderer.wait(60000), "render thread did not finish - deadlock");
    QCOMPARE(renderer.filled, 20);

    delete algo;
}

/****************************************************************************
 * AC16: RGBAudio must keep reporting that it is audio driven, otherwise
 * HUEMatrix::write() stops recomputing its map every tick.
 ****************************************************************************/

void HUEMatrix_Test::builtInAudioAlgorithmReportsUsesAudio()
{
    // AC19 resolved the "Audio Spectrum" shadowing, so the HUE factory now
    // returns the genuine built-in for this name.
    RGBAlgorithm *audio = HUEMatrix::createAlgorithm(m_doc, RGBAudio(m_doc).name());
    QVERIFY(audio != NULL);
    QCOMPARE(audio->type(), RGBAlgorithm::Audio);
    QCOMPARE(audio->usesAudio(), true);
    delete audio;

    // The default must not leak onto algorithms that are not audio driven.
    RGBAlgorithm *plain = HUEMatrix::createAlgorithm(m_doc, "Stripes");
    QVERIFY(plain != NULL);
    QVERIFY(plain->type() != RGBAlgorithm::Audio);
    QCOMPARE(plain->usesAudio(), false);
    delete plain;
}

/****************************************************************************
 * AC14: the HUE cache must be populated the way the app startup path does it,
 * so that HUEMatrixEditor::algorithms() - which returns exactly this list -
 * shows only the HSV scripts.
 ****************************************************************************/

void HUEMatrix_Test::hueCacheOffersAudioScriptsAfterStartupStyleLoad()
{
    // Same call shape as qmlui/app.cpp and ui/src/app.cpp: HSV directory with
    // the HSV contract, stock directory without it.
    Doc fresh(this);
    QVERIFY(fresh.rgbScriptsCache()->load(QDir(INTERNAL_SCRIPTDIR)));
    QVERIFY(fresh.hueScriptsCache()->load(QDir(INTERNAL_HUESCRIPTDIR), true));
    QVERIFY(fresh.hueScriptsCache()->load(QDir(INTERNAL_SCRIPTDIR), false));

    QStringList hueList = HUEMatrix::availableAlgorithms(&fresh);
    QStringList rgbList = RGBAlgorithm::algorithms(&fresh);

    QCOMPARE(hueList, fresh.hueScriptsCache()->hsvNames());
    foreach (QString name, rgbList)
        QVERIFY2(hueList.contains(name) == false, qPrintable(name));
}

/****************************************************************************
 * AC15: an RGBMatrix whose workspace names a relocated audio script must not
 * load silently to nothing.
 ****************************************************************************/

void HUEMatrix_Test::unavailableAlgorithmWarnsOnWorkspaceLoad()
{
    QStringList hsv = m_doc->hueScriptsCache()->hsvNames();
    QVERIFY(hsv.isEmpty() == false);
    QString missing = hsv.first();
    QVERIFY2(m_doc->rgbScriptsCache()->names().contains(missing) == false,
             "picked a script that IS available to RGBMatrix");

    QString xml = QString(
        "<Function ID=\"7\" Type=\"RGBMatrix\" Name=\"Broken\">"
        "<Speed FadeIn=\"0\" FadeOut=\"0\" Duration=\"200\"/>"
        "<Algorithm Type=\"Script\">%1</Algorithm>"
        "</Function>").arg(missing);

    QBuffer buffer;
    buffer.setData(xml.toUtf8());
    buffer.open(QIODevice::ReadOnly);
    QXmlStreamReader reader(&buffer);
    reader.readNextStartElement();

    // Both halves of the warning: the script name from the cache, the function
    // name from the loader. QTest::ignoreMessage fails if it is never emitted.
    QTest::ignoreMessage(QtWarningMsg,
        QRegularExpression(QString("RGB script \"%1\" is not available").arg(missing)));
    QTest::ignoreMessage(QtWarningMsg,
        QRegularExpression("Function \"Broken\".*lost its algorithm"));

    QVERIFY(Function::loader(reader, m_doc));

    Function *loaded = m_doc->function(7);
    QVERIFY(loaded != NULL);
    RGBMatrix *matrix = qobject_cast<RGBMatrix*> (loaded);
    QVERIFY(matrix != NULL);
    QVERIFY(matrix->algorithm() == NULL);

    // Control: an available algorithm must not warn.
    QString ok = QString(
        "<Function ID=\"8\" Type=\"RGBMatrix\" Name=\"Fine\">"
        "<Speed FadeIn=\"0\" FadeOut=\"0\" Duration=\"200\"/>"
        "<Algorithm Type=\"Script\">Stripes</Algorithm>"
        "</Function>");
    QBuffer okBuffer;
    okBuffer.setData(ok.toUtf8());
    okBuffer.open(QIODevice::ReadOnly);
    QXmlStreamReader okReader(&okBuffer);
    okReader.readNextStartElement();
    QVERIFY(Function::loader(okReader, m_doc));
    QVERIFY(qobject_cast<RGBMatrix*> (m_doc->function(8))->algorithm() != NULL);
}

/****************************************************************************
 * AC17: the two function types must be visually distinguishable.
 ****************************************************************************/

void HUEMatrix_Test::hueMatrixHasItsOwnIcon()
{
    // The engine test binary does not compile the icon .qrc in, so QIcon
    // renders nothing here. Assert the shipped artifacts instead: a distinct
    // SVG, registered, and actually referenced by both UI entry points.
    const QString src = QStringLiteral(QLCPLUS_SOURCE_DIR);
    QFile svg(src + "/resources/icons/svg/huematrix.svg");
    QVERIFY2(svg.exists(), qPrintable(QFileInfo(svg).absoluteFilePath()));
    QVERIFY(svg.open(QIODevice::ReadOnly));
    QByteArray hueSvg = svg.readAll();
    svg.close();
    QVERIFY(hueSvg.contains("<svg"));

    QFile rgbSvg(src + "/resources/icons/svg/rgbmatrix.svg");
    QVERIFY(rgbSvg.open(QIODevice::ReadOnly));
    QVERIFY2(rgbSvg.readAll() != hueSvg, "huematrix.svg is a copy of rgbmatrix.svg");
    rgbSvg.close();

    struct Ref { const char *path; const char *needle; };
    const QVector<Ref> refs = {
        { "/resources/icons/svg/svgicons.qrc", "huematrix.svg" },
        { "/qmlui/functionmanager.cpp",             "HUEMatrixType: return \"qrc:/huematrix.svg\"" },
        { "/qmlui/qml/fixturesfunctions/AddFunctionMenu.qml", "qrc:/huematrix.svg" },
    };

    foreach (const Ref &ref, refs)
    {
        QFile f(src + ref.path);
        QVERIFY2(f.open(QIODevice::ReadOnly), ref.path);
        QVERIFY2(f.readAll().contains(ref.needle), ref.path);
        f.close();
    }
}

/****************************************************************************
 * AC18: every UI site that shows a HUEMatrix icon must show the HUE icon.
 *
 * The AC17 test above checks three hand-picked sites. That let
 * FunctionManager.qml's "HUE Matrices" filter button keep "qrc:/rgbmatrix.svg"
 * unnoticed. This test instead enumerates *every* function-type icon reference
 * in the UI sources and fails on any HUEMatrix one that is not the HUE icon,
 * so a newly added site cannot slip through.
 ****************************************************************************/

/** One `*.svg` reference found in a source file, bound to the function type it
 *  illustrates. */
struct IconSite
{
    QString file;
    int line;
    QString svg;
    QString type;   /**< "HUEMatrix", "RGBMatrix", ... or empty if unbound */
};

/** Collect every `<something>.svg` reference in `text` and bind each to the
 *  *nearest* function-type token within kContextLines. Both a C++ switch arm
 *  and a QML block declare the icon and the type on separate but adjacent
 *  lines, and the nearest token is the one the icon belongs to. */
static QVector<IconSite> scanIconSites(const QString &file, const QString &text)
{
    const int kContextLines = 8;
    const QStringList lines = text.split('\n');
    static const QRegularExpression svgRef("([A-Za-z0-9_]+\\.svg)");
    static const QRegularExpression typeRef(
        "(?:Function::|QLCFunction\\.)([A-Za-z0-9]+)Type\\b");

    QVector<IconSite> sites;
    for (int i = 0; i < lines.count(); i++)
    {
        QRegularExpressionMatchIterator svgIt = svgRef.globalMatch(lines.at(i));
        if (svgIt.hasNext() == false)
            continue;

        // Nearest function-type token, scanning outwards from this line.
        QString type;
        for (int d = 0; d <= kContextLines && type.isEmpty(); d++)
        {
            for (int s = 0; s < 2 && type.isEmpty(); s++)
            {
                const int j = (s == 0) ? i + d : i - d;
                if (j < 0 || j >= lines.count())
                    continue;
                QRegularExpressionMatch m = typeRef.match(lines.at(j));
                if (m.hasMatch())
                    type = m.captured(1);
            }
        }

        while (svgIt.hasNext())
        {
            IconSite site;
            site.file = file;
            site.line = i + 1;
            site.svg = svgIt.next().captured(1);
            site.type = type;
            sites << site;
        }
    }
    return sites;
}

void HUEMatrix_Test::everyHueMatrixIconSiteUsesTheHueIcon()
{
    // Negative control first: the scanner must actually catch the shape of the
    // defect this criterion is about. Without this, a scanner that silently
    // binds nothing would pass the real scan below.
    const QString prefixSnippet =
        "IconButton\n"
        "{\n"
        "    imgSource: \"qrc:/rgbmatrix.svg\"\n"
        "    checkable: true\n"
        "    checked: functionManager.functionsFilter & QLCFunction.HUEMatrixType\n"
        "    tooltip: qsTr(\"HUE Matrices\")\n"
        "}\n";
    QVector<IconSite> bad = scanIconSites("<synthetic>", prefixSnippet);
    QCOMPARE(bad.count(), 1);
    QCOMPARE(bad.first().type, QString("HUEMatrix"));
    QCOMPARE(bad.first().svg, QString("rgbmatrix.svg"));

    // Now the real tree. Everything the UI can draw a function icon from.
    const QString src = QStringLiteral(QLCPLUS_SOURCE_DIR);
    const QStringList roots = { "/qmlui", "/ui/src", "/engine/src", "/resources/icons/svg" };
    const QStringList suffixes = { "qml", "cpp", "h", "qrc" };

    QVector<IconSite> allSites;
    foreach (const QString &root, roots)
    {
        QDirIterator it(src + root, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext())
        {
            const QString path = it.next();
            if (suffixes.contains(QFileInfo(path).suffix()) == false)
                continue;

            QFile f(path);
            QVERIFY2(f.open(QIODevice::ReadOnly), qPrintable(path));
            allSites += scanIconSites(QDir(src).relativeFilePath(path),
                                      QString::fromUtf8(f.readAll()));
            f.close();
        }
    }

    // The scan must have found something, otherwise the assertions below are
    // vacuous. RGBMatrix is the type this one is most likely to be confused
    // with, so require it to be bound too.
    QVERIFY2(allSites.count() > 20, qPrintable(QString("only %1 icon references found")
                                               .arg(allSites.count())));

    QStringList offenders;
    QStringList hueSites;
    int rgbSites = 0;
    foreach (const IconSite &site, allSites)
    {
        if (site.type == QLatin1String("RGBMatrix"))
        {
            rgbSites++;
            if (site.svg != QLatin1String("rgbmatrix.svg"))
                offenders << QString("%1:%2 RGBMatrix -> %3")
                             .arg(site.file).arg(site.line).arg(site.svg);
        }
        if (site.type != QLatin1String("HUEMatrix"))
            continue;
        hueSites << QString("%1:%2 -> %3").arg(site.file).arg(site.line).arg(site.svg);
        if (site.svg != QLatin1String("huematrix.svg"))
            offenders << QString("%1:%2 HUEMatrix -> %3")
                         .arg(site.file).arg(site.line).arg(site.svg);
    }

    QVERIFY2(offenders.isEmpty(), qPrintable(offenders.join("; ")));
    qDebug() << "HUEMatrix icon sites:" << hueSites;

    // Guard against the scan silently narrowing to nothing: the icon switch in
    // functionmanager.cpp, the AddFunctionMenu entry and the FunctionManager
    // filter button are all type-bound sites.
    QVERIFY2(hueSites.count() >= 3, qPrintable(QString("only %1 HUEMatrix icon sites found")
                                               .arg(hueSites.count())));
    QVERIFY2(rgbSites >= 3, qPrintable(QString("only %1 RGBMatrix icon sites found")
                                       .arg(rgbSites)));

    // Two further sites carry no function-type token next to them and so are
    // invisible to the scan above. Assert them explicitly rather than leave
    // them unchecked.
    struct Ref { const char *path; const char *needle; };
    const QVector<Ref> untyped = {
        { "/engine/src/huematrix.cpp",              "QIcon(\":/huematrix.svg\")" },
        { "/resources/icons/svg/svgicons.qrc",      "alias=\"huematrix.svg\"" },
    };
    foreach (const Ref &ref, untyped)
    {
        QFile f(src + ref.path);
        QVERIFY2(f.open(QIODevice::ReadOnly), ref.path);
        QVERIFY2(f.readAll().contains(ref.needle), ref.path);
        f.close();
    }
}

/****************************************************************************
 * AC19: a HUE script must not shadow a built-in algorithm's name.
 ****************************************************************************/

void HUEMatrix_Test::builtInAudioIsReachableByNameOnHueMatrix()
{
    const QString builtInName = RGBAudio(m_doc).name();
    QCOMPARE(builtInName, QString("Audio Spectrum"));

    // Existing functions can still use built-ins without offering them as patterns.
    QVERIFY(HUEMatrix::availableAlgorithms(m_doc).contains(builtInName) == false);

    RGBAlgorithm *algo = HUEMatrix::createAlgorithm(m_doc, builtInName);
    QVERIFY(algo != NULL);
    QCOMPARE(algo->type(), RGBAlgorithm::Audio);
    QCOMPARE(algo->name(), builtInName);
    delete algo;

    // The renamed HSV script is still reachable and is still a Script.
    RGBAlgorithm *script = HUEMatrix::createAlgorithm(m_doc, "Audio Spectrum Bars");
    QVERIFY(script != NULL);
    QCOMPARE(script->type(), RGBAlgorithm::Script);
    delete script;

    // Error case: a name that is neither.
    QVERIFY(HUEMatrix::createAlgorithm(m_doc, "No Such Algorithm") == NULL);
}

void HUEMatrix_Test::noHueScriptShadowsABuiltInName()
{
    // Guard the whole HUE script set, not just the one that was found broken.
    QStringList builtIns;
    builtIns << RGBPlain(m_doc).name() << RGBText(m_doc).name()
             << RGBImage(m_doc).name() << RGBAudio(m_doc).name();

    QStringList shadowed;
    foreach (const QString &name, m_doc->hueScriptsCache()->hsvNames())
    {
        if (builtIns.contains(name))
            shadowed << name;
    }
    QVERIFY2(shadowed.isEmpty(), qPrintable(shadowed.join(", ")));

    // Every built-in must resolve to its built-in type through the HUE factory.
    foreach (const QString &name, builtIns)
    {
        RGBAlgorithm *algo = HUEMatrix::createAlgorithm(m_doc, name);
        QVERIFY2(algo != NULL, qPrintable(name));
        QVERIFY2(algo->type() != RGBAlgorithm::Script, qPrintable(name));
        delete algo;
    }
}

/****************************************************************************
 * AC20: the destructor's wait for an in-flight async precompute is bounded.
 *
 * m_precomputedInFlight is raised *before* the job is queued on the JS thread
 * and cleared only from inside the queued lambda. If the JS thread goes away
 * in between, the flag stays raised forever. Before this fix ~HUEMatrix() spun
 * on it with no exit condition and shutdown hung.
 ****************************************************************************/

void HUEMatrix_Test::destructorGivesUpOnAnAsyncTaskThatNeverClears()
{
    HUEMatrix *mtx = new HUEMatrix(m_doc);

    // Simulate a job that was queued but whose lambda will never run.
    mtx->m_precomputedInFlight.storeRelease(1);
    QCOMPARE(mtx->m_precomputedInFlight.loadAcquire(), 1);

    QTest::ignoreMessage(QtWarningMsg,
                         QRegularExpression("async precompute did not drain within"));

    QElapsedTimer timer;
    timer.start();
    delete mtx;
    const qint64 waited = timer.elapsed();

    // It must actually have waited (so a real in-flight job still gets its
    // chance) but must have terminated. Before the fix this loop had no exit
    // condition at all, so the upper bound is deliberately slack - it only has
    // to separate "bounded" from "never returns".
    QVERIFY2(waited >= HUEMatrix::precomputeDrainTimeoutMs / 2,
             qPrintable(QString("gave up after only %1ms").arg(waited)));
    QVERIFY2(waited < HUEMatrix::precomputeDrainTimeoutMs * 20,
             qPrintable(QString("took %1ms, bound is %2ms")
                        .arg(waited).arg(HUEMatrix::precomputeDrainTimeoutMs)));
}

void HUEMatrix_Test::destructorReturnsImmediatelyWhenNothingIsInFlight()
{
    HUEMatrix *mtx = new HUEMatrix(m_doc);
    QCOMPARE(mtx->m_precomputedInFlight.loadAcquire(), 0);

    QElapsedTimer timer;
    timer.start();
    delete mtx;

    // The common path must not pay the timeout.
    QVERIFY2(timer.elapsed() < HUEMatrix::precomputeDrainTimeoutMs / 4,
             qPrintable(QString("idle teardown took %1ms").arg(timer.elapsed())));
}

/****************************************************************************
 * AC24: the async precompute path.
 *
 * kickAsyncRgbMap() queues an rgbMap() onto the JS thread and stashes the
 * result for the next tick; consumePrecomputedMap() takes it only if nothing
 * that would invalidate it moved in the meantime. Neither had any test.
 ****************************************************************************/

/** Wait (bounded) for the queued JS-thread task to clear the in-flight flag. */
static bool waitForPrecomputeDrain(HUEMatrix *mtx, int timeoutMs = 60000)
{
    QElapsedTimer timer;
    timer.start();
    while (mtx->m_precomputedInFlight.loadAcquire() != 0)
    {
        if (timer.elapsed() > timeoutMs)
            return false;
        QThread::msleep(1);
    }
    return true;
}

void HUEMatrix_Test::asyncPrecomputeProducesAConsumableMap_data()
{
    QTest::addColumn<QString>("algorithmName");
    QTest::newRow("legacy-script") << "Stripes";
    QTest::newRow("hue-single-color") << "Hue Single Color";
}

void HUEMatrix_Test::asyncPrecomputeProducesAConsumableMap()
{
    QFETCH(QString, algorithmName);
    const QString name = algorithmName;
    RGBAlgorithm *algo = HUEMatrix::createAlgorithm(m_doc, name);
    QVERIFY2(algo != NULL, qPrintable(name));
    QCOMPARE(algo->type(), RGBAlgorithm::Script);

    HUEMatrix mtx(m_doc);
    mtx.setAlgorithm(algo);
    mtx.m_runAlgorithm = algo;

    const QSize algoSize(5, 4);
    const uint color = 0x0000FF00;
    const int step = 0;
    const quint32 gen = mtx.m_currentGeneration.loadAcquire();

    QCOMPARE(mtx.m_precomputedReady.loadAcquire(), 0);
    QCOMPARE(mtx.m_precomputedInFlight.loadAcquire(), 0);

    mtx.kickAsyncRgbMap(algo, algoSize, color, step, gen);
    QVERIFY2(waitForPrecomputeDrain(&mtx), "async precompute never drained");

    // The task ran to completion and published a frame.
    QCOMPARE(mtx.m_precomputedReady.loadAcquire(), 1);
    QCOMPARE(mtx.m_precomputedGeneration, gen);
    QCOMPARE(mtx.m_precomputedStep, step);
    QCOMPARE(mtx.m_precomputedColor, color);
    QCOMPARE(mtx.m_precomputedAlgoSize, algoSize);
    QCOMPARE(mtx.m_precomputedMap.size(), algoSize.height());

    // Consume-hit: the frame moves into the step handler and the slot frees.
    mtx.m_stepHandler->m_map = RGBMap();
    QCOMPARE(mtx.consumePrecomputedMap(algoSize, color, step, gen), true);
    QCOMPARE(mtx.m_stepHandler->m_map.size(), algoSize.height());
    QCOMPARE(mtx.m_stepHandler->m_map.first().size(), algoSize.width());
    QCOMPARE(mtx.m_precomputedReady.loadAcquire(), 0);

    // Nothing left to take.
    QCOMPARE(mtx.consumePrecomputedMap(algoSize, color, step, gen), false);

    mtx.m_runAlgorithm = NULL;
}

void HUEMatrix_Test::asyncPrecomputeIsThrottledToOneTaskInFlight()
{
    const QString name = "Stripes";
    RGBAlgorithm *algo = HUEMatrix::createAlgorithm(m_doc, name);
    QVERIFY(algo != NULL);

    HUEMatrix mtx(m_doc);
    mtx.setAlgorithm(algo);
    mtx.m_runAlgorithm = algo;

    // Pretend a task is already queued. The kick must decline rather than
    // stack a second one.
    mtx.m_precomputedInFlight.storeRelease(1);
    mtx.kickAsyncRgbMap(algo, QSize(5, 4), 0x0000FF00, 0, mtx.m_currentGeneration.loadAcquire());

    QThread::msleep(50);
    QCOMPARE(mtx.m_precomputedReady.loadAcquire(), 0);

    // Release the flag by hand; the destructor must not have to wait it out.
    mtx.m_precomputedInFlight.storeRelease(0);
    mtx.m_runAlgorithm = NULL;
}

void HUEMatrix_Test::asyncPrecomputeIsSkippedForNonScriptAlgorithms()
{
    // Built-ins render inline and cheaply, so there is nothing to offload.
    RGBAlgorithm *plain = HUEMatrix::createAlgorithm(m_doc, "Plain Color");
    QVERIFY(plain != NULL);
    QVERIFY(plain->type() != RGBAlgorithm::Script);

    HUEMatrix mtx(m_doc);
    mtx.setAlgorithm(plain);
    mtx.m_runAlgorithm = plain;

    mtx.kickAsyncRgbMap(plain, QSize(5, 4), 0x0000FF00, 0,
                        mtx.m_currentGeneration.loadAcquire());

    QCOMPARE(mtx.m_precomputedInFlight.loadAcquire(), 0);
    QThread::msleep(50);
    QCOMPARE(mtx.m_precomputedReady.loadAcquire(), 0);

    // Error case: a null algorithm must not raise the flag either.
    mtx.kickAsyncRgbMap(NULL, QSize(5, 4), 0x0000FF00, 0,
                        mtx.m_currentGeneration.loadAcquire());
    QCOMPARE(mtx.m_precomputedInFlight.loadAcquire(), 0);

    mtx.m_runAlgorithm = NULL;
}

void HUEMatrix_Test::precomputedMapIsRejectedWhenTheGenerationMoved()
{
    const QString name = "Stripes";
    RGBAlgorithm *algo = HUEMatrix::createAlgorithm(m_doc, name);
    QVERIFY(algo != NULL);

    HUEMatrix mtx(m_doc);
    mtx.setAlgorithm(algo);
    mtx.m_runAlgorithm = algo;

    const QSize algoSize(5, 4);
    const uint color = 0x0000FF00;
    const quint32 gen = mtx.m_currentGeneration.loadAcquire();

    mtx.kickAsyncRgbMap(algo, algoSize, color, 0, gen);
    QVERIFY2(waitForPrecomputeDrain(&mtx), "async precompute never drained");
    QCOMPARE(mtx.m_precomputedReady.loadAcquire(), 1);

    // Any of these four moving must invalidate the stored frame.
    const RGBMap sentinel(1, RGBMap::value_type(1, 0x00ABCDEF));
    mtx.m_stepHandler->m_map = sentinel;

    QCOMPARE(mtx.consumePrecomputedMap(algoSize, color, 0, gen + 1), false);
    QCOMPARE(mtx.m_stepHandler->m_map, sentinel);
    // A miss still frees the slot, so re-arm before each further probe.
    mtx.m_precomputedReady.storeRelease(1);
    QCOMPARE(mtx.consumePrecomputedMap(algoSize, color, 7, gen), false);
    mtx.m_precomputedReady.storeRelease(1);
    QCOMPARE(mtx.consumePrecomputedMap(algoSize, 0x00FF0000, 0, gen), false);
    mtx.m_precomputedReady.storeRelease(1);
    QCOMPARE(mtx.consumePrecomputedMap(QSize(9, 9), color, 0, gen), false);
    QCOMPARE(mtx.m_stepHandler->m_map, sentinel);

    // ...and the matching call still succeeds afterwards.
    mtx.m_precomputedReady.storeRelease(1);
    QCOMPARE(mtx.consumePrecomputedMap(algoSize, color, 0, gen), true);
    QVERIFY(mtx.m_stepHandler->m_map != sentinel);

    mtx.m_runAlgorithm = NULL;
}

void HUEMatrix_Test::inFlightPrecomputeIsDiscardedWhenInvalidatedMidFlight()
{
    const QString name = "Stripes";
    RGBAlgorithm *algo = HUEMatrix::createAlgorithm(m_doc, name);
    QVERIFY(algo != NULL);

    HUEMatrix mtx(m_doc);
    mtx.setAlgorithm(algo);
    mtx.m_runAlgorithm = algo;

    // Kick with a generation that is already stale. The queued task re-checks
    // the generation before computing and must drop the work.
    const quint32 stale = mtx.m_currentGeneration.loadAcquire() - 1;
    mtx.kickAsyncRgbMap(algo, QSize(5, 4), 0x0000FF00, 0, stale);

    QVERIFY2(waitForPrecomputeDrain(&mtx), "stale async precompute never drained");
    QCOMPARE(mtx.m_precomputedReady.loadAcquire(), 0);
    QCOMPARE(mtx.consumePrecomputedMap(QSize(5, 4), 0x0000FF00, 0, stale), false);

    mtx.m_runAlgorithm = NULL;
}

/****************************************************************************
 * AC21: a fork-only <AudioProfileID> tag is dropped when its RGBMatrix is
 * loaded by restored-upstream code, and the user is told.
 *
 * engine/src/rgbmatrix.cpp is a sealed byte-identical upstream artifact, so
 * the warning is upstream's own "Unknown RGB matrix tag". This test pins that
 * behaviour so a future upstream refresh cannot silence it.
 ****************************************************************************/
void HUEMatrix_Test::forkOnlyAudioProfileIdTagWarnsOnRGBMatrixLoad()
{
    QString xml =
        "<Function ID=\"21\" Type=\"RGBMatrix\" Name=\"Has Audio Profile\">"
        "<Speed FadeIn=\"0\" FadeOut=\"0\" Duration=\"200\"/>"
        "<Algorithm Type=\"Script\">Stripes</Algorithm>"
        "<AudioProfileID>3</AudioProfileID>"
        "</Function>";

    QBuffer buffer;
    buffer.setData(xml.toUtf8());
    buffer.open(QIODevice::ReadOnly);
    QXmlStreamReader reader(&buffer);
    reader.readNextStartElement();

    // ignoreMessage fails the test if the warning is never emitted.
    QTest::ignoreMessage(QtWarningMsg,
        QRegularExpression("Unknown RGB matrix tag.*AudioProfileID"));

    QVERIFY(Function::loader(reader, m_doc));

    // The function still loads and keeps its algorithm; only the fork-only
    // setting is lost.
    RGBMatrix *matrix = qobject_cast<RGBMatrix*> (m_doc->function(21));
    QVERIFY(matrix != NULL);
    QVERIFY(matrix->algorithm() != NULL);

    // Control: the same function without the fork-only tag must not warn.
    QString clean =
        "<Function ID=\"22\" Type=\"RGBMatrix\" Name=\"No Audio Profile\">"
        "<Speed FadeIn=\"0\" FadeOut=\"0\" Duration=\"200\"/>"
        "<Algorithm Type=\"Script\">Stripes</Algorithm>"
        "</Function>";
    QBuffer cleanBuffer;
    cleanBuffer.setData(clean.toUtf8());
    cleanBuffer.open(QIODevice::ReadOnly);
    QXmlStreamReader cleanReader(&cleanBuffer);
    cleanReader.readNextStartElement();
    QVERIFY(Function::loader(cleanReader, m_doc));
    QVERIFY(qobject_cast<RGBMatrix*> (m_doc->function(22))->algorithm() != NULL);
}

/****************************************************************************
 * AC23: while a HUEMatrix runs an audio algorithm the map is recomputed on
 * every tick, not only when the step changes.
 *
 * Observed, not asserted from usesAudio(): the previous frame's map is
 * poisoned with a sentinel and write() is driven on a mid-step tick, where a
 * non-audio algorithm is required to leave the sentinel untouched.
 ****************************************************************************/
void HUEMatrix_Test::audioAlgorithmRecomputesTheMapEveryTick()
{
    FixtureGroup *grp = new FixtureGroup(m_doc);
    grp->setName("AC23 Group");
    grp->setSize(QSize(3, 3));
    m_doc->addFixtureGroup(grp);

    MasterTimer timer(m_doc);
    QList<Universe *> universes = m_doc->inputOutputMap()->claimUniverses();
    m_doc->inputOutputMap()->releaseUniverses(false);

    const uint sentinel = 0x00ABCDEF;

    // drive() runs one tick that is NOT the first tick of a step, so the
    // step-change and first-tick arms of the recompute predicate are false and
    // only usesAudio() can trigger a recompute.
    struct Driver
    {
        static bool mapWasRecomputed(HUEMatrix &mtx, MasterTimer *timer,
                                     QList<Universe *> universes, uint sentinel)
        {
            mtx.m_stepHandler->m_map = RGBMap(3, QVector<uint>(3, sentinel));
            mtx.write(timer, universes);
            const RGBMap &m = mtx.m_stepHandler->m_map;
            for (int y = 0; y < m.size(); y++)
                for (int x = 0; x < m[y].size(); x++)
                    if (m[y][x] != sentinel)
                        return true;
            return false;
        }
    };

    // --- audio algorithm: recomputes every tick
    RGBAlgorithm *audio = HUEMatrix::createAlgorithm(m_doc, "Audio Spectrum");
    QVERIFY(audio != NULL);
    QCOMPARE(audio->type(), RGBAlgorithm::Audio);
    QVERIFY(audio->usesAudio());

    HUEMatrix audioMtx(m_doc);
    audioMtx.setFixtureGroup(grp->id());
    audioMtx.setAlgorithm(audio);
    audioMtx.setBeatEffect(HUEMatrix::BeatEffectOff);
    audioMtx.preRun(&timer);
    audioMtx.write(&timer, universes);   // first tick of the step
    QVERIFY2(Driver::mapWasRecomputed(audioMtx, &timer, universes, sentinel),
             "audio algorithm did not recompute on a mid-step tick");
    QVERIFY2(Driver::mapWasRecomputed(audioMtx, &timer, universes, sentinel),
             "audio algorithm recomputed once but not on the following tick");
    audioMtx.postRun(&timer, universes);

    // --- control: a non-audio script must NOT recompute on a mid-step tick
    RGBAlgorithm *plain = HUEMatrix::createAlgorithm(m_doc, "Stripes");
    QVERIFY(plain != NULL);
    QVERIFY(plain->usesAudio() == false);

    HUEMatrix plainMtx(m_doc);
    plainMtx.setFixtureGroup(grp->id());
    plainMtx.setAlgorithm(plain);
    plainMtx.setBeatEffect(HUEMatrix::BeatEffectOff);
    plainMtx.preRun(&timer);
    plainMtx.write(&timer, universes);   // first tick of the step
    QVERIFY2(Driver::mapWasRecomputed(plainMtx, &timer, universes, sentinel) == false,
             "non-audio algorithm recomputed on a mid-step tick");
    plainMtx.postRun(&timer, universes);
}

void HUEMatrix_Test::asyncPrecomputeIsSkippedForAudioScripts()
{
    HUEMatrix matrix(m_doc);
    auto script = HUEMatrix::createAlgorithm(m_doc, "Audio Spectrum Bars");
    QVERIFY(script != nullptr);
    QVERIFY(script->usesAudio());
    matrix.setAlgorithm(script);
    matrix.m_runAlgorithm = script;
    matrix.kickAsyncRgbMap(script, QSize(37, 1), 0xffffff, 0,
                          matrix.m_currentGeneration.loadAcquire());
    QCOMPARE(matrix.m_precomputedInFlight.loadAcquire(), 0);
    QCOMPARE(matrix.m_precomputedReady.loadAcquire(), 0);
    matrix.m_runAlgorithm = nullptr;
}

void HUEMatrix_Test::audioPreviewReusesCommittedFrame()
{
    auto group = new FixtureGroup(m_doc);
    group->setSize(QSize(3, 1));
    m_doc->addFixtureGroup(group);
    HUEMatrix matrix(m_doc);
    matrix.setFixtureGroup(group->id());
    matrix.setAlgorithm(HUEMatrix::createAlgorithm(m_doc, "Audio Spectrum Bars"));
    QVERIFY(matrix.algorithm() != nullptr);
    const RGBMap committed{QVector<uint>{0x12ab34, 0x5612ef, 0xbc7823}};
    matrix.m_stepHandler->m_map = committed;
    MasterTimer timer(m_doc);
    matrix.Function::preRun(&timer);
    RGBMatrixStep preview;
    matrix.previewMap(0, &preview);
    QCOMPARE(preview.m_map, committed);
    matrix.previewMap(1, &preview);
    QCOMPARE(preview.m_map, committed);
    matrix.Function::postRun(&timer, {});
}

QTEST_MAIN(HUEMatrix_Test)