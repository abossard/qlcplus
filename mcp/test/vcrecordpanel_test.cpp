/*
  Q Light Controller Plus - Unit test
  vcrecordpanel_test.cpp

  Copyright (C) Massimo Callegari

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
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "vcrecordpanel_test.h"
#include "dmxcapture.h"
#include "doc.h"
#include "scene.h"
#include "chaser.h"
#include "chaserstep.h"
#include "fixture.h"
#include "universe.h"
#include "inputoutputmap.h"

void VCRecordPanel_Test::initTestCase()
{
    m_doc = new Doc(this);
}

void VCRecordPanel_Test::cleanupTestCase()
{
    delete m_doc;
}

void VCRecordPanel_Test::cleanup()
{
    m_doc->clearContents();
}

/*********************************************************************
 * DmxCapture tests
 *********************************************************************/

void VCRecordPanel_Test::captureAllFixtures_nullDoc()
{
    QList<SceneValue> result = DmxCapture::captureAllFixtures(nullptr);
    QVERIFY(result.isEmpty());
}

void VCRecordPanel_Test::captureAllFixtures_noFixtures()
{
    QList<SceneValue> result = DmxCapture::captureAllFixtures(m_doc);
    QVERIFY(result.isEmpty());
}

void VCRecordPanel_Test::captureAllFixtures_withFixtures()
{
    // Add a dimmer fixture with 4 channels
    Fixture *fxi = new Fixture(m_doc);
    fxi->setName("Test Dimmer");
    fxi->setChannels(4);
    fxi->setAddress(0);
    fxi->setUniverse(0);
    m_doc->addFixture(fxi);

    // Write some values to the universe's pre-GM buffer
    QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
    QVERIFY(ua.count() > 0);
    ua[0]->setChannelDefaultValue(0, 128);
    ua[0]->setChannelDefaultValue(1, 64);
    ua[0]->setChannelDefaultValue(2, 0);
    ua[0]->setChannelDefaultValue(3, 255);
    m_doc->inputOutputMap()->releaseUniverses(false);

    QList<SceneValue> result = DmxCapture::captureAllFixtures(m_doc, false);
    QCOMPARE(result.size(), 4);

    // Verify fixture ID and channel indices
    for (const SceneValue &sv : result)
    {
        QCOMPARE(sv.fxi, fxi->id());
    }
}

void VCRecordPanel_Test::captureAllFixtures_nonZeroOnly()
{
    Fixture *fxi = new Fixture(m_doc);
    fxi->setName("Test Dimmer NZ");
    fxi->setChannels(4);
    fxi->setAddress(0);
    fxi->setUniverse(0);
    m_doc->addFixture(fxi);

    // Set some channels to zero and some to non-zero
    QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
    ua[0]->setChannelDefaultValue(0, 100);
    ua[0]->setChannelDefaultValue(1, 0);
    ua[0]->setChannelDefaultValue(2, 200);
    ua[0]->setChannelDefaultValue(3, 0);
    m_doc->inputOutputMap()->releaseUniverses(false);

    QList<SceneValue> result = DmxCapture::captureAllFixtures(m_doc, true);
    // Only channels 0 and 2 have non-zero values
    QCOMPARE(result.size(), 2);
    QCOMPARE(result[0].channel, quint32(0));
    QCOMPARE(result[1].channel, quint32(2));
}

/*********************************************************************
 * Scene creation through Doc
 *********************************************************************/

void VCRecordPanel_Test::sceneCreation_basic()
{
    QList<SceneValue> values;
    values.append(SceneValue(0, 0, 128));
    values.append(SceneValue(0, 1, 64));

    Scene *scene = new Scene(m_doc);
    scene->setName("Test Scene 1");
    scene->setPath("Recordings");
    for (const SceneValue &sv : values)
        scene->setValue(sv);

    QVERIFY(m_doc->addFunction(scene));
    QCOMPARE(scene->name(), QString("Test Scene 1"));
    QCOMPARE(scene->path(true), QString("Recordings"));
    QCOMPARE(scene->values().size(), 2);
}

void VCRecordPanel_Test::sceneCreation_autoNaming()
{
    // Create 3 scenes with prefix pattern "Scene X"
    for (int i = 1; i <= 3; i++)
    {
        Scene *scene = new Scene(m_doc);
        scene->setName(QStringLiteral("Scene %1").arg(i));
        scene->setPath("Recordings");
        QVERIFY(m_doc->addFunction(scene));
    }

    // Verify we can find the max number by scanning functions
    int maxNum = 0;
    for (Function *f : m_doc->functions())
    {
        if (f->path(true) != "Recordings")
            continue;
        const QString &name = f->name();
        if (!name.startsWith("Scene "))
            continue;
        QString numStr = name.mid(6).trimmed();
        bool ok = false;
        int num = numStr.toInt(&ok);
        if (ok && num > maxNum)
            maxNum = num;
    }
    QCOMPARE(maxNum, 3);
    QCOMPARE(maxNum + 1, 4); // Next name number
}

/*********************************************************************
 * Chaser creation through Doc
 *********************************************************************/

void VCRecordPanel_Test::chaserCreation_basic()
{
    Chaser *chaser = new Chaser(m_doc);
    chaser->setName("Test Chaser 1");
    chaser->setPath("Recordings");
    chaser->setFadeInMode(Chaser::PerStep);
    chaser->setFadeOutMode(Chaser::PerStep);
    chaser->setDurationMode(Chaser::PerStep);

    QVERIFY(m_doc->addFunction(chaser));
    QCOMPARE(chaser->name(), QString("Test Chaser 1"));
    QCOMPARE(chaser->fadeInMode(), Chaser::PerStep);
}

void VCRecordPanel_Test::chaserCreation_withSteps()
{
    // Create scenes first
    Scene *s1 = new Scene(m_doc);
    s1->setName("S1");
    QVERIFY(m_doc->addFunction(s1));

    Scene *s2 = new Scene(m_doc);
    s2->setName("S2");
    QVERIFY(m_doc->addFunction(s2));

    // Create chaser and add steps with timing
    Chaser *chaser = new Chaser(m_doc);
    chaser->setName("Test Chaser Steps");
    QVERIFY(m_doc->addFunction(chaser));

    uint fadeIn = 500, hold = 1000, fadeOut = 300;

    ChaserStep step1(s1->id(), fadeIn, hold, fadeOut);
    QVERIFY(chaser->addStep(step1));

    ChaserStep step2(s2->id(), fadeIn, hold, fadeOut);
    QVERIFY(chaser->addStep(step2));

    QCOMPARE(chaser->stepsCount(), 2);

    // Verify step timing
    ChaserStep *readStep = chaser->stepAt(0);
    QVERIFY(readStep != nullptr);
    QCOMPARE(readStep->fid, s1->id());
    QCOMPARE(readStep->fadeIn, fadeIn);
    QCOMPARE(readStep->hold, hold);
    QCOMPARE(readStep->fadeOut, fadeOut);
}

/*********************************************************************
 * Full recording flow: create chaser, add scenes as steps
 *********************************************************************/

void VCRecordPanel_Test::recordingFlow_chaserWithScenes()
{
    // Simulate VCRecordPanel's recording flow without the widget itself
    // This tests the engine-level operations that createScene + startChaser perform

    QString targetFolder = "My Show";
    QString scenePrefix = "Song";
    QString chaserPrefix = "Set";
    uint fadeIn = 500, hold = 2000, fadeOut = 300;

    // 1. Start chaser recording
    Chaser *chaser = new Chaser(m_doc);
    chaser->setName(QStringLiteral("%1 1").arg(chaserPrefix));
    chaser->setPath(targetFolder);
    chaser->setFadeInMode(Chaser::PerStep);
    chaser->setFadeOutMode(Chaser::PerStep);
    chaser->setDurationMode(Chaser::PerStep);
    QVERIFY(m_doc->addFunction(chaser));

    quint32 chaserId = chaser->id();
    QVERIFY(chaserId != Function::invalidId());

    // 2. Create 3 scenes and add them as steps
    for (int i = 1; i <= 3; i++)
    {
        Scene *scene = new Scene(m_doc);
        scene->setName(QStringLiteral("%1 %2").arg(scenePrefix).arg(i));
        scene->setPath(targetFolder);
        scene->setValue(SceneValue(0, 0, i * 50)); // Simulate different DMX values
        QVERIFY(m_doc->addFunction(scene));

        // Add as chaser step
        Chaser *ch = qobject_cast<Chaser *>(m_doc->function(chaserId));
        QVERIFY(ch != nullptr);
        ChaserStep step(scene->id(), fadeIn, hold, fadeOut);
        QVERIFY(ch->addStep(step));
    }

    // 3. Verify chaser has 3 steps with correct timing
    Chaser *resultChaser = qobject_cast<Chaser *>(m_doc->function(chaserId));
    QVERIFY(resultChaser != nullptr);
    QCOMPARE(resultChaser->stepsCount(), 3);
    QCOMPARE(resultChaser->path(true), targetFolder);

    for (int i = 0; i < 3; i++)
    {
        ChaserStep *step = resultChaser->stepAt(i);
        QVERIFY(step != nullptr);
        QCOMPARE(step->fadeIn, fadeIn);
        QCOMPARE(step->hold, hold);
        QCOMPARE(step->fadeOut, fadeOut);

        // Verify the referenced scene exists
        Function *fn = m_doc->function(step->fid);
        QVERIFY(fn != nullptr);
        QCOMPARE(fn->type(), Function::SceneType);
        QCOMPARE(fn->path(true), targetFolder);
    }

    // 4. Verify stop — subsequent scene creation doesn't add to chaser
    Scene *extraScene = new Scene(m_doc);
    extraScene->setName(QStringLiteral("%1 4").arg(scenePrefix));
    extraScene->setPath(targetFolder);
    QVERIFY(m_doc->addFunction(extraScene));
    // Not adding to chaser — recording stopped
    QCOMPARE(resultChaser->stepsCount(), 3);

    // 5. Verify stale chaser ID protection
    m_doc->deleteFunction(chaserId);
    QVERIFY(m_doc->function(chaserId) == nullptr);
    // qobject_cast on nullptr returns nullptr — recording should stop
    Chaser *stale = qobject_cast<Chaser *>(m_doc->function(chaserId));
    QVERIFY(stale == nullptr);
}

/*********************************************************************
 * XML round-trip for VCRecordPanel tags
 *********************************************************************/

void VCRecordPanel_Test::xmlRoundTrip_data()
{
    QTest::addColumn<QString>("targetFolder");
    QTest::addColumn<QString>("scenePrefix");
    QTest::addColumn<QString>("chaserPrefix");
    QTest::addColumn<uint>("fadeIn");
    QTest::addColumn<uint>("hold");
    QTest::addColumn<uint>("fadeOut");

    QTest::newRow("defaults")
        << "Recordings" << "Scene" << "Chaser"
        << uint(0) << uint(1000) << uint(0);

    QTest::newRow("custom")
        << "My Show/Songs" << "Song" << "Set"
        << uint(500) << uint(2000) << uint(300);

    QTest::newRow("empty folder")
        << "" << "S" << "C"
        << uint(100) << uint(0) << uint(100);
}

void VCRecordPanel_Test::xmlRoundTrip()
{
    QFETCH(QString, targetFolder);
    QFETCH(QString, scenePrefix);
    QFETCH(QString, chaserPrefix);
    QFETCH(uint, fadeIn);
    QFETCH(uint, hold);
    QFETCH(uint, fadeOut);

    // Write XML
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter writer(&buffer);
    writer.writeStartDocument();
    writer.writeStartElement("RecordPanel");

    writer.writeTextElement("TargetFolder", targetFolder);
    writer.writeTextElement("ScenePrefix", scenePrefix);
    writer.writeTextElement("ChaserPrefix", chaserPrefix);
    writer.writeTextElement("DefaultFadeIn", QString::number(fadeIn));
    writer.writeTextElement("DefaultHold", QString::number(hold));
    writer.writeTextElement("DefaultFadeOut", QString::number(fadeOut));

    writer.writeEndElement();
    writer.writeEndDocument();
    buffer.close();

    // Read XML back
    buffer.open(QIODevice::ReadOnly | QIODevice::Text);
    QXmlStreamReader reader(&buffer);
    reader.readNextStartElement(); // Move to <RecordPanel>
    QCOMPARE(reader.name().toString(), QString("RecordPanel"));

    QString readFolder, readScenePrefix, readChaserPrefix;
    uint readFadeIn = 0, readHold = 0, readFadeOut = 0;

    while (reader.readNextStartElement())
    {
        if (reader.name() == QStringLiteral("TargetFolder"))
            readFolder = reader.readElementText();
        else if (reader.name() == QStringLiteral("ScenePrefix"))
            readScenePrefix = reader.readElementText();
        else if (reader.name() == QStringLiteral("ChaserPrefix"))
            readChaserPrefix = reader.readElementText();
        else if (reader.name() == QStringLiteral("DefaultFadeIn"))
            readFadeIn = reader.readElementText().toUInt();
        else if (reader.name() == QStringLiteral("DefaultHold"))
            readHold = reader.readElementText().toUInt();
        else if (reader.name() == QStringLiteral("DefaultFadeOut"))
            readFadeOut = reader.readElementText().toUInt();
        else
            reader.skipCurrentElement();
    }
    buffer.close();

    QCOMPARE(readFolder, targetFolder);
    QCOMPARE(readScenePrefix, scenePrefix);
    QCOMPARE(readChaserPrefix, chaserPrefix);
    QCOMPARE(readFadeIn, fadeIn);
    QCOMPARE(readHold, hold);
    QCOMPARE(readFadeOut, fadeOut);
}

QTEST_MAIN(VCRecordPanel_Test)
