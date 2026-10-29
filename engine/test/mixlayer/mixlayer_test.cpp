/*
  Q Light Controller Plus - Unit test
  mixlayer_test.cpp

  Copyright (c) QLC+ contributors

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

#define protected public
#define private public
#include "mastertimer.h"
#include "universe.h"
#undef private
#undef protected

#include "mixlayer_test.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "genericfader.h"
#include "fadechannel.h"
#include "collection.h"
#include "mixlayer.h"
#include "qlcfile.h"
#include "chaser.h"
#include "scene.h"
#include "doc.h"

#include "../common/resource_paths.h"

/* Futurelight DJScan250 Mode 1 at address 0: Pan is LTP, Shutter is HTP */
static const quint32 kLtp = 0;
static const quint32 kHtp = 5;

void MixLayer_Test::initTestCase()
{
    m_doc = new Doc(this);
    QDir dir(INTERNAL_FIXTUREDIR);
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtFixture));
    QVERIFY(m_doc->fixtureDefCache()->loadMap(dir) == true);
}

void MixLayer_Test::init()
{
    Fixture *fxi = new Fixture(m_doc);
    QLCFixtureDef *def = m_doc->fixtureDefCache()->fixtureDef("Futurelight", "DJScan250");
    QVERIFY(def != NULL);

    QLCFixtureMode *mode = def->mode("Mode 1");
    QVERIFY(mode != NULL);

    fxi->setFixtureDefinition(def, mode);
    fxi->setAddress(0);
    m_doc->addFixture(fxi);
    m_fixtureId = fxi->id();
}

void MixLayer_Test::cleanup()
{
    m_doc->clearContents();

    Universe *universe = m_doc->inputOutputMap()->universes().at(0);
    foreach (QSharedPointer<GenericFader> fader, universe->faders())
        universe->dismissFader(fader);
    universe->reset();
}

Scene *MixLayer_Test::createScene(uchar ltp, uchar htp)
{
    Scene *scene = new Scene(m_doc);
    scene->setValue(m_fixtureId, kLtp, ltp);
    scene->setValue(m_fixtureId, kHtp, htp);
    m_doc->addFunction(scene);
    return scene;
}

Function *MixLayer_Test::createParent(const QString &type, quint32 childId)
{
    // "Collection > Chaser" is a Collection running a Chaser running the child
    QStringList chain = type.split(" > ");
    Function *parent = NULL;
    quint32 memberId = childId;

    while (chain.isEmpty() == false)
    {
        if (chain.takeLast() == "Chaser")
        {
            Chaser *chaser = new Chaser(m_doc);
            chaser->setDurationMode(Chaser::Common);
            chaser->setDuration(Function::infiniteSpeed());
            chaser->addStep(ChaserStep(memberId));
            parent = chaser;
        }
        else
        {
            Collection *collection = new Collection(m_doc);
            collection->addFunction(memberId);
            parent = collection;
        }
        m_doc->addFunction(parent);
        memberId = parent->id();
    }

    return parent;
}

QByteArray MixLayer_Test::writeUniverse()
{
    QList<Universe*> ua = m_doc->inputOutputMap()->claimUniverses();
    ua[0]->processFaders(MasterTimer::tick());
    QByteArray values = ua[0]->preGMValues();
    m_doc->inputOutputMap()->releaseUniverses(false);
    return values;
}

void MixLayer_Test::faderLayerDecidesLtp_data()
{
    QTest::addColumn<int>("olderPriority");
    QTest::addColumn<int>("olderLayer");
    QTest::addColumn<int>("newerPriority");
    QTest::addColumn<int>("newerLayer");
    QTest::addColumn<int>("expectedLtp");

    const int base = Universe::Auto;
    const int over = Universe::Override;

    // The older fader writes LTP 100 and HTP 150, the newer one LTP 50 and HTP 200
    QTest::newRow("same layer, newest wins") << base << 0 << base << 0 << 50;
    QTest::newRow("older on a higher layer wins") << base << 1 << base << 0 << 100;
    QTest::newRow("newer on a lower layer loses") << base << 0 << base << -1 << 100;
    QTest::newRow("newer on a higher layer wins") << base << -1 << base << 0 << 50;
    QTest::newRow("layers order the Override band") << over << 1 << over << 0 << 100;
    QTest::newRow("older band beats layer") << over << -5 << base << 5 << 100;
    QTest::newRow("newer band beats layer") << base << 5 << over << -5 << 50;
}

void MixLayer_Test::faderLayerDecidesLtp()
{
    QFETCH(int, olderPriority);
    QFETCH(int, olderLayer);
    QFETCH(int, newerPriority);
    QFETCH(int, newerLayer);
    QFETCH(int, expectedLtp);

    Universe *universe = m_doc->inputOutputMap()->universes().at(0);
    auto setValue = [this](QSharedPointer<GenericFader> fader, quint32 channel, uchar value)
    {
        FadeChannel fc(m_doc, m_fixtureId, channel);
        fc.setStart(0);
        fc.setTarget(value);
        fc.setFadeTime(0);
        fader->add(fc);
    };

    QSharedPointer<GenericFader> older = universe->requestFader(Universe::FaderPriority(olderPriority));
    older->mixLayer()->setLayer(olderLayer);
    setValue(older, kLtp, 100);
    setValue(older, kHtp, 150);

    QSharedPointer<GenericFader> newer = universe->requestFader(Universe::FaderPriority(newerPriority));
    newer->mixLayer()->setLayer(newerLayer);
    setValue(newer, kLtp, 50);
    setValue(newer, kHtp, 200);

    universe->processFaders(MasterTimer::tick());
    QCOMPARE(uchar(universe->preGMValues().at(kLtp)), uchar(expectedLtp));
    QCOMPARE(uchar(universe->preGMValues().at(kHtp)), uchar(200));
}

void MixLayer_Test::sceneLayerFlipMidRun_data()
{
    QTest::addColumn<int>("olderLayer");
    QTest::addColumn<int>("newerLayer");
    QTest::addColumn<int>("expectedLtp");
    QTest::addColumn<int>("flippedOlderLayer");
    QTest::addColumn<int>("flippedNewerLayer");
    QTest::addColumn<int>("flippedLtp");

    // The older Scene sets LTP 100 and HTP 150, the newer one LTP 50 and HTP 200
    QTest::newRow("older raised above") << 0 << 0 << 50 << 1 << 0 << 100;
    QTest::newRow("older dropped below") << 1 << 0 << 100 << -1 << 0 << 50;
    QTest::newRow("newer back to the same layer") << 0 << -1 << 100 << 0 << 0 << 50;
    QTest::newRow("layers swapped") << 1 << 0 << 100 << 0 << 1 << 50;
}

void MixLayer_Test::sceneLayerFlipMidRun()
{
    QFETCH(int, olderLayer);
    QFETCH(int, newerLayer);
    QFETCH(int, expectedLtp);
    QFETCH(int, flippedOlderLayer);
    QFETCH(int, flippedNewerLayer);
    QFETCH(int, flippedLtp);

    MasterTimer timer(m_doc);
    Scene *older = createScene(100, 150);
    Scene *newer = createScene(50, 200);
    older->mixLayer()->setLayer(olderLayer);
    newer->mixLayer()->setLayer(newerLayer);

    older->start(&timer, FunctionParent::master());
    timer.timerTick();
    newer->start(&timer, FunctionParent::master());
    timer.timerTick();

    QByteArray values = writeUniverse();
    QCOMPARE(uchar(values.at(kLtp)), uchar(expectedLtp));
    QCOMPARE(uchar(values.at(kHtp)), uchar(200));

    older->mixLayer()->setLayer(flippedOlderLayer);
    newer->mixLayer()->setLayer(flippedNewerLayer);
    timer.timerTick();

    values = writeUniverse();
    QCOMPARE(uchar(values.at(kLtp)), uchar(flippedLtp));
    QCOMPARE(uchar(values.at(kHtp)), uchar(200));

    older->stop(FunctionParent::master());
    newer->stop(FunctionParent::master());
    timer.timerTick();
}

void MixLayer_Test::childFollowsParent_data()
{
    QTest::addColumn<QString>("parentType");
    QTest::addColumn<int>("parentLayer");
    QTest::addColumn<int>("expectedLtp");
    QTest::addColumn<int>("flippedParentLayer");
    QTest::addColumn<int>("flippedLtp");

    // The child Scene sets LTP 100 and HTP 150, a newer standalone Scene LTP 50 and HTP 200
    QTest::newRow("Chaser step above") << "Chaser" << 1 << 100 << 0 << 50;
    QTest::newRow("Chaser step below") << "Chaser" << -1 << 50 << 1 << 100;
    QTest::newRow("Collection member above") << "Collection" << 1 << 100 << -1 << 50;
    QTest::newRow("Collection member below") << "Collection" << -1 << 50 << 1 << 100;
    QTest::newRow("Chaser in a Collection") << "Collection > Chaser" << 1 << 100 << 0 << 50;
}

void MixLayer_Test::childFollowsParent()
{
    QFETCH(QString, parentType);
    QFETCH(int, parentLayer);
    QFETCH(int, expectedLtp);
    QFETCH(int, flippedParentLayer);
    QFETCH(int, flippedLtp);

    MasterTimer timer(m_doc);
    Scene *child = createScene(100, 150);
    Scene *other = createScene(50, 200);
    Function *parent = createParent(parentType, child->id());
    parent->mixLayer()->setLayer(parentLayer);

    parent->start(&timer, FunctionParent::master());
    for (int i = 0; i < 3; i++)
        timer.timerTick();
    QVERIFY(child->isRunning());

    other->start(&timer, FunctionParent::master());
    timer.timerTick();

    QByteArray values = writeUniverse();
    QCOMPARE(uchar(values.at(kLtp)), uchar(expectedLtp));
    QCOMPARE(uchar(values.at(kHtp)), uchar(200));

    parent->mixLayer()->setLayer(flippedParentLayer);
    timer.timerTick();

    values = writeUniverse();
    QCOMPARE(uchar(values.at(kLtp)), uchar(flippedLtp));
    QCOMPARE(uchar(values.at(kHtp)), uchar(200));
    QCOMPARE(child->mixLayer()->effective(), flippedParentLayer);

    // Started on its own, the child no longer follows the parent
    parent->stop(FunctionParent::master());
    for (int i = 0; i < 3; i++)
        timer.timerTick();
    QVERIFY(child->isRunning() == false);

    child->start(&timer, FunctionParent::master());
    QCOMPARE(child->mixLayer()->effective(), 0);

    child->stop(FunctionParent::master());
    other->stop(FunctionParent::master());
    timer.timerTick();
}

QTEST_MAIN(MixLayer_Test)
