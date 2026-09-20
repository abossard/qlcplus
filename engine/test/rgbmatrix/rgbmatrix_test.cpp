/*
  Q Light Controller Plus - Unit tests
  rgbmatrix_test.cpp

  Copyright (C) Heikki Junnila
                Massimo Callegari

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
#include <QSet>
#include <QScopeGuard>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#define protected public
#define private public
#include "rgbscriptscache.h"
#include "rgbmatrix_test.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "fixturegroup.h"
#include "mastertimer.h"
#include "inputoutputmap.h"
#include "universe.h"
#include "rgbmatrix.h"
#include "fixture.h"
#include "qlcfile.h"
#include "doc.h"
#undef private
#undef protected

#include "../common/resource_paths.h"

#include "timingdiagnostics.h"
#include "fadechannel.h"
#include "genericfader.h"
#include "rgbplain.h"

namespace
{
    QStringList g_rgbMsgs;

    void rgbCaptureHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
    {
        Q_UNUSED(type);
        Q_UNUSED(ctx);
        g_rgbMsgs << msg;
    }
}

void RGBMatrix_Test::initTestCase()
{
    m_doc = new Doc(this);

    QDir fxiDir(INTERNAL_FIXTUREDIR);
    fxiDir.setFilter(QDir::Files);
    fxiDir.setNameFilters(QStringList() << QString("*%1").arg(KExtFixture));
    QVERIFY(m_doc->fixtureDefCache()->loadMap(fxiDir) == true);

    QLCFixtureDef* def = m_doc->fixtureDefCache()->fixtureDef("Stairville", "LED PAR56");
    QVERIFY(def != NULL);
    QLCFixtureMode* mode = def->modes().first();
    QVERIFY(mode != NULL);

    FixtureGroup* grp = new FixtureGroup(m_doc);
    grp->setName("Test Group");
    grp->setSize(QSize(5, 5));
    m_doc->addFixtureGroup(grp);

    for (int i = 0; i < 25; i++)
    {
        Fixture* fxi = new Fixture(m_doc);
        fxi->setFixtureDefinition(def, mode);
        fxi->setAddress(i * fxi->channels());
        m_doc->addFixture(fxi);

        grp->assignFixture(fxi->id());
    }

    QVERIFY(m_doc->rgbScriptsCache()->load(QDir(INTERNAL_SCRIPTDIR)));
    QVERIFY(m_doc->rgbScriptsCache()->names().size() != 0);
}

void RGBMatrix_Test::cleanupTestCase()
{
    delete m_doc;
}

void RGBMatrix_Test::initial()
{
    RGBMatrix mtx(m_doc);
    QCOMPARE(mtx.type(), Function::RGBMatrixType);
    QCOMPARE(mtx.fixtureGroup(), FixtureGroup::invalidId());
    QCOMPARE(mtx.getColor(0), QColor(Qt::red));
    QCOMPARE(mtx.getColor(1), QColor());
    QCOMPARE(mtx.m_fadersMap.count(), 0);
    QCOMPARE(mtx.m_stepHandler->currentStepIndex(), 0);
    QCOMPARE(mtx.name(), tr("New RGB Matrix"));
    QCOMPARE(mtx.duration(), uint(500));
    QCOMPARE(mtx.totalDuration(), uint(0));
    QVERIFY(mtx.algorithm() != NULL);
    QCOMPARE(mtx.algorithm()->name(), QString("Stripes"));
    QCOMPARE(mtx.components().size(), 0);
}

void RGBMatrix_Test::group()
{
    RGBMatrix mtx(m_doc);
    mtx.setFixtureGroup(0);
    QCOMPARE(mtx.fixtureGroup(), uint(0));

    mtx.setFixtureGroup(15);
    QCOMPARE(mtx.fixtureGroup(), uint(15));

    mtx.setFixtureGroup(FixtureGroup::invalidId());
    QCOMPARE(mtx.fixtureGroup(), FixtureGroup::invalidId());
}

void RGBMatrix_Test::color()
{
    RGBMatrix mtx(m_doc);
    mtx.setColor(0, Qt::blue);
    QCOMPARE(mtx.getColor(0), QColor(Qt::blue));

    mtx.setColor(0, QColor());
    QCOMPARE(mtx.getColor(0), QColor());

    mtx.setColor(1, Qt::green);
    QCOMPARE(mtx.getColor(1), QColor(Qt::green));

    mtx.setColor(1, QColor());
    QCOMPARE(mtx.getColor(1), QColor());
}

void RGBMatrix_Test::copy()
{
    RGBMatrix mtx(m_doc);
    mtx.setColor(0, Qt::magenta);
    mtx.setColor(1, Qt::yellow);
    mtx.setFixtureGroup(0);
    mtx.setAlgorithm(RGBAlgorithm::algorithm(m_doc, "Stripes"));
    QVERIFY(mtx.algorithm() != NULL);

    RGBMatrix* copyMtx = qobject_cast<RGBMatrix*> (mtx.createCopy(m_doc));
    QVERIFY(copyMtx != NULL);
    QCOMPARE(copyMtx->getColor(0), QColor(Qt::magenta));
    QCOMPARE(copyMtx->getColor(1), QColor(Qt::yellow));
    QCOMPARE(copyMtx->fixtureGroup(), uint(0));
    QVERIFY(copyMtx->algorithm() != NULL);
    QVERIFY(copyMtx->algorithm() != mtx.algorithm()); // Different object pointer!
    QCOMPARE(copyMtx->algorithm()->name(), QString("Stripes"));
}

void RGBMatrix_Test::previewMaps()
{
    RGBMatrix mtx(m_doc);
    RGBMatrixStep handler;
    QVERIFY(mtx.algorithm() != NULL);
    QCOMPARE(mtx.algorithm()->name(), QString("Stripes"));

    int steps = mtx.stepsCount();
    QCOMPARE(steps, 0);

    mtx.previewMap(0, &handler);
    QCOMPARE(handler.m_map.size(), 0); // No fixture group

    mtx.setFixtureGroup(0);
    steps = mtx.stepsCount();
    QCOMPARE(steps, 5);
    QCOMPARE(mtx.components().size(), 25);
    QCOMPARE(mtx.totalDuration(), uint(2500));

    mtx.setTotalDuration(8000);
    QCOMPARE(mtx.totalDuration(), uint(8000));

    mtx.previewMap(0, &handler);
    QCOMPARE(handler.m_map.size(), 5);

    for (int z = 0; z < steps; z++)
    {
        mtx.previewMap(z, &handler);
        for (int y = 0; y < 5; y++)
        {
            for (int x = 0; x < 5; x++)
            {
                if (x == z)
                    QCOMPARE(handler.m_map[y][x], QColor(Qt::black).rgb());
                else
                    QCOMPARE(handler.m_map[y][x], uint(0));
            }
        }
    }
}

void RGBMatrix_Test::property()
{
    RGBMatrix mtx(m_doc);
    QVERIFY(mtx.algorithm() != NULL);
    QCOMPARE(mtx.algorithm()->name(), QString("Stripes"));

    // check on invalid property
    QCOMPARE(mtx.property("foo"), QString());

    // check a valid property
    QCOMPARE(mtx.property("orientation"), QString("Horizontal"));

    mtx.setProperty("orientation", "Vertical");

    QCOMPARE(mtx.property("orientation"), QString("Vertical"));
}

void RGBMatrix_Test::loadSave()
{
    RGBMatrix* mtx = new RGBMatrix(m_doc);
    mtx->setColor(0, Qt::magenta);
    mtx->setColor(1, Qt::blue);
    mtx->setColor(2, Qt::green);
    mtx->setColor(3, Qt::red);
    mtx->setColor(4, Qt::yellow);
    mtx->setControlMode(RGBMatrix::ControlModeRgb);
    mtx->setFixtureGroup(42);
    mtx->setAlgorithm(RGBAlgorithm::algorithm(m_doc, "Stripes"));
    QVERIFY(mtx->algorithm() != NULL);
    QCOMPARE(mtx->algorithm()->name(), QString("Stripes"));

    mtx->setName("Xyzzy");
    mtx->setDirection(Function::Backward);
    mtx->setRunOrder(Function::PingPong);
    mtx->setDuration(1200);
    mtx->setFadeInSpeed(10);
    mtx->setFadeOutSpeed(20);
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
    QCOMPARE(xmlReader.attributes().value("Type").toString(), QString("RGBMatrix"));
    QCOMPARE(xmlReader.attributes().value("ID").toString(), QString::number(mtx->id()));
    QCOMPARE(xmlReader.attributes().value("Name").toString(), QString("Xyzzy"));

    int speed = 0, dir = 0, run = 0, algo = 0, grp = 0, color1 = 0, color2 = 0, color3 = 0, color4 = 0, color5 = 0, colormode = 0;

    while (xmlReader.readNextStartElement())
    {
        if (xmlReader.name().toString() == "Speed")
        {
            QCOMPARE(xmlReader.attributes().value("FadeIn").toString(), QString("10"));
            QCOMPARE(xmlReader.attributes().value("FadeOut").toString(), QString("20"));
            QCOMPARE(xmlReader.attributes().value("Duration").toString(), QString("1200"));
            speed++;
            xmlReader.skipCurrentElement();
        }
        else if (xmlReader.name().toString() == "Direction")
        {
            QCOMPARE(xmlReader.readElementText(), QString("Backward"));
            dir++;
        }
        else if (xmlReader.name().toString() == "RunOrder")
        {
            QCOMPARE(xmlReader.readElementText(), QString("PingPong"));
            run++;
        }
        else if (xmlReader.name().toString() == "Algorithm")
        {
            // RGBAlgorithms take care of Algorithm tag's contents
            algo++;
            xmlReader.skipCurrentElement();
        }
        else if (xmlReader.name().toString() == "Color")
        {
            bool ok = false;
            int colorNum = xmlReader.attributes().value("Index").toInt(&ok);
            QVERIFY(ok);

            switch (colorNum)
            {
                case 0:
                    QCOMPARE(xmlReader.readElementText().toUInt(), QColor(Qt::magenta).rgb());
                    color1++;
                break;
                case 1:
                    QCOMPARE(xmlReader.readElementText().toUInt(), QColor(Qt::blue).rgb());
                    color2++;
                break;
                case 2:
                    QCOMPARE(xmlReader.readElementText().toUInt(), QColor(Qt::green).rgb());
                    color3++;
                break;
                case 3:
                    QCOMPARE(xmlReader.readElementText().toUInt(), QColor(Qt::red).rgb());
                    color4++;
                break;
                case 4:
                    QCOMPARE(xmlReader.readElementText().toUInt(), QColor(Qt::yellow).rgb());
                    color5++;
                break;
                default:
                    // The color number can be between 1 and MAXINT, but here we expect only 5.
                    QVERIFY(colorNum > 0 && colorNum <= 5);
                break;
            }
        }
        else if (xmlReader.name().toString() == "FixtureGroup")
        {
            QCOMPARE(xmlReader.readElementText(), QString("42"));
            grp++;
        }
        else if (xmlReader.name().toString() == "ControlMode")
        {
            QCOMPARE(xmlReader.readElementText(), QString("RGB"));
            colormode++;
        }
        else
        {
            QFAIL(QString("Unexpected tag: %1").arg(xmlReader.name().toString()).toUtf8().constData());
        }
    }

    QCOMPARE(speed, 1);
    QCOMPARE(dir, 1);
    QCOMPARE(run, 1);
    QCOMPARE(algo, 1);
    QCOMPARE(color1, 1);
    QCOMPARE(color2, 1);
    QCOMPARE(color3, 1);
    QCOMPARE(color4, 1);
    QCOMPARE(color5, 1);
    QCOMPARE(grp, 1);
    QCOMPARE(colormode, 1);

    xmlReader.setDevice(NULL);
    buffer.seek(0);
    xmlReader.setDevice(&buffer);
    xmlReader.readNextStartElement();

    RGBMatrix mtx2(m_doc);
    QVERIFY(mtx2.loadXML(xmlReader) == true);
    QCOMPARE(mtx2.direction(), Function::Backward);
    QCOMPARE(mtx2.runOrder(), Function::PingPong);
    QCOMPARE(mtx2.getColor(0), QColor(Qt::magenta));
    QCOMPARE(mtx2.getColor(1), QColor(Qt::blue));
    QCOMPARE(mtx2.controlMode(), RGBMatrix::ControlModeRgb);
    QCOMPARE(mtx2.fixtureGroup(), uint(42));
    QVERIFY(mtx2.algorithm() != NULL);
    QCOMPARE(mtx2.algorithm()->name(), mtx->algorithm()->name());
    QCOMPARE(mtx2.duration(), uint(1200));
    QCOMPARE(mtx2.fadeInSpeed(), uint(10));
    QCOMPARE(mtx2.fadeOutSpeed(), uint(20));

    buffer.close();
    buffer.setData(QByteArray());

    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    xmlWriter.setDevice(&buffer);

    // Put some extra garbage in
    xmlWriter.writeStartElement("Foo");
    xmlWriter.writeEndElement();

    buffer.close();
    buffer.open(QIODevice::ReadOnly | QIODevice::Text);

    xmlReader.setDevice(&buffer);
    xmlReader.readNextStartElement();

    QVERIFY(mtx2.loadXML(xmlReader) == false); // Not a function node

    xmlReader.setDevice(NULL);
    buffer.close();
    buffer.setData(QByteArray());

    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    xmlWriter.setDevice(&buffer);

    // Put some extra garbage in
    xmlWriter.writeStartElement("Function");
    xmlWriter.writeAttribute("Type", "Scene");
    xmlWriter.writeEndElement();

    buffer.close();
    buffer.open(QIODevice::ReadOnly | QIODevice::Text);

    xmlReader.setDevice(&buffer);
    xmlReader.readNextStartElement();
    QVERIFY(mtx2.loadXML(xmlReader) == false); // Not an RGBMatrix node

}

void RGBMatrix_Test::repeatedFadeTarget_data()
{
    QTest::addColumn<int>("target");
    QTest::addColumn<int>("nextTarget");
    QTest::newRow("continue-fade-out") << 0 << 0;
    QTest::newRow("continue-fade-in") << 220 << 220;
    QTest::newRow("change-to-fade-out") << 220 << 0;
    QTest::newRow("change-to-fade-in") << 0 << 180;
}

void RGBMatrix_Test::repeatedFadeTarget()
{
    QFETCH(int, target);
    QFETCH(int, nextTarget);
    RGBMatrix matrix(m_doc);
    matrix.setFadeOutSpeed(1200);
    FadeChannel channel;
    channel.setStart(30);
    channel.setCurrent(90);
    channel.setTarget(target);
    channel.setElapsed(200);
    channel.setFadeTime(800);
    channel.setReady(false);

    matrix.updateFaderValues(channel, nextTarget, 600);

    const bool unchanged = target == nextTarget;
    QCOMPARE(channel.start(), quint32(unchanged ? 30 : 90));
    QCOMPARE(channel.current(), quint32(90));
    QCOMPARE(channel.target(), quint32(nextTarget));
    QCOMPARE(channel.elapsed(), uint(unchanged ? 200 : 0));
    QCOMPARE(channel.fadeTime(), uint(unchanged ? 800 : nextTarget == 0 ? 1200 : 600));
    QVERIFY(!channel.isReady());
}

void RGBMatrix_Test::initialFadePlayback_data()
{
    QTest::addColumn<int>("current");
    QTest::addColumn<int>("target");
    QTest::addColumn<uint>("fadeOut");
    QTest::addColumn<int>("firstOutput");
    QTest::newRow("cold-black-fades-from-live") << 200 << 0 << uint(1200) << 197;
    QTest::newRow("cold-nonzero-target") << 40 << 220 << uint(1200) << 44;
    QTest::newRow("already-dark") << 0 << 0 << uint(1200) << 0;
    QTest::newRow("instant-black") << 200 << 0 << uint(0) << 0;
}

void RGBMatrix_Test::initialFadePlayback()
{
    QFETCH(int, current);
    QFETCH(int, target);
    QFETCH(uint, fadeOut);
    QFETCH(int, firstOutput);
    Doc doc(nullptr);
    auto *fixture = new Fixture(&doc);
    fixture->setChannels(1);
    QVERIFY(doc.addFixture(fixture));
    auto *group = new FixtureGroup(&doc);
    group->setSize(QSize(1, 1));
    QVERIFY(doc.addFixtureGroup(group));
    group->assignFixture(fixture->id());
    const auto universes = doc.inputOutputMap()->universes();
    Universe *universe = universes.first();
    universe->write(0, current);

    RGBMatrix matrix(&doc);
    matrix.setFixtureGroup(group->id());
    matrix.setControlMode(RGBMatrix::ControlModeDimmer);
    matrix.setAlgorithm(new RGBPlain(&doc));
    matrix.setColor(0, QColor(target, target, target));
    matrix.setDuration(MasterTimer::tick());
    matrix.setFadeInSpeed(800);
    matrix.setFadeOutSpeed(fadeOut);
    matrix.preRun(doc.masterTimer());
    const auto cleanup = qScopeGuard([&]() { matrix.postRun(doc.masterTimer(), universes); });

    for (int elapsed = 20; elapsed <= 1200; elapsed += 20)
    {
        matrix.write(doc.masterTimer(), universes);
        auto fader = matrix.m_fadersMap.value(universe->id());
        QVERIFY(fader);
        universe->zeroIntensityChannels();
        fader->write(universe, 20);
        if (elapsed == 20)
            QCOMPARE(universe->preGMValue(0), uchar(firstOutput));
    }
    QCOMPARE(universe->preGMValue(0), uchar(target));
}

/* C2-3: the ordinary between-beat step advance must advance the step the same
   way whether diagnostics are on or off, must never emit the old unbounded
   "Elapsed exceeded" line, and when enabled must produce one bounded, identified
   summary carrying the advance count for the interval.

   Each iteration renders a frame, then forces one between-beat transition
   through the real write()/roundCheck() path. */
void RGBMatrix_Test::beatStepAdvanceDiagnostic()
{
    MasterTimer *mt = m_doc->masterTimer();
    QList<Universe*> u = m_doc->inputOutputMap()->claimUniverses();
    const int N = 6;

    auto driveWrites = [&](RGBMatrix &mtx, int n) -> QVector<RGBMap>
    {
        QVector<RGBMap> frames;
        for (int i = 0; i < n; i++)
        {
            mtx.m_elapsed = 0;
            mtx.write(mt, u);
            frames.append(mtx.m_stepHandler->m_map);
            mtx.m_stepBeatDuration = 40;
            mtx.m_elapsed = 40;
            mtx.write(mt, u);
        }
        return frames;
    };

    auto setup = [&](RGBMatrix &mtx)
    {
        mtx.setID(17);
        mtx.setFixtureGroup(0);
        mtx.setTempoType(Function::Beats);
        mtx.setDuration(1000);
        mt->requestBpmNumber(6);        // beatTimeDuration 10000ms -> beat far away
        mtx.preRun(mt);
    };

    g_rgbMsgs.clear();
    QtMessageHandler prev = qInstallMessageHandler(rgbCaptureHandler);

    // Disabled baseline: advancement happens, nothing is logged.
    TimingDiag::resetForTest();
    TimingDiag::setEnabledForTest(false);
    RGBMatrix disabledMtx(m_doc);
    setup(disabledMtx);
    const QVector<RGBMap> framesDisabled = driveWrites(disabledMtx, N);
    const int idxDisabled = disabledMtx.m_stepHandler->currentStepIndex();

    // Enabled: same advancement, one bounded identified summary.
    qint64 now = 0;
    TimingDiag::resetForTest();
    TimingDiag::setClockForTest([&now]() { return now; });
    TimingDiag::setIntervalMsForTest(1000);
    TimingDiag::setEnabledForTest(true);
    RGBMatrix enabledMtx(m_doc);
    setup(enabledMtx);
    const QVector<RGBMap> framesEnabled = driveWrites(enabledMtx, N);
    const int idxEnabled = enabledMtx.m_stepHandler->currentStepIndex();
    now = 1000;                                          // interval elapsed
    TimingDiag::flushExpired();                          // periodic flush emits the window

    TimingDiag::setEnabledForTest(false);
    TimingDiag::setClockForTest(nullptr);
    TimingDiag::resetForTest();
    qInstallMessageHandler(prev);
    m_doc->inputOutputMap()->releaseUniverses();

    QStringList stepLines, elapsedLines;
    foreach (const QString &m, g_rgbMsgs)
    {
        if (m.contains(QStringLiteral("[timing] RGBMatrix step advance")))
            stepLines << m;
        if (m.contains(QStringLiteral("Elapsed exceeded")))
            elapsedLines << m;
    }

    QCOMPARE(idxEnabled, idxDisabled);          // advancement unchanged by diagnostics
    QVERIFY(disabledMtx.m_stepsCount > 0);
    QCOMPARE(idxDisabled, N % disabledMtx.m_stepsCount);
    QCOMPARE(framesEnabled, framesDisabled);
    QVERIFY(framesDisabled.first() != framesDisabled.at(1));
    QSet<uint> colors;
    for (const auto &row : framesDisabled.first())
        for (uint color : row)
            colors.insert(color);
    QVERIFY(colors.size() > 1);
    QCOMPARE(elapsedLines.size(), 0);           // ambiguous flood is gone in both modes
    QCOMPARE(stepLines.size(), 1);              // exactly one bounded summary
    QVERIFY2(stepLines.first().contains("advances=" + QString::number(N)),
             qPrintable(stepLines.first()));
    QVERIFY2(stepLines.first().contains("id=17"), qPrintable(stepLines.first()));
}

QTEST_MAIN(RGBMatrix_Test)
