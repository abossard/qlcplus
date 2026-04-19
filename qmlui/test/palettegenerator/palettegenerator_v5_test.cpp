/*
  Q Light Controller Plus - Unit test
  palettegenerator_v5_test.cpp

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
#include <QList>

#define protected public
#define private public

#include "palettegenerator_v5_test.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "qlccapability.h"
#include "qlcpalette.h"
#include "fixture.h"
#include "chaser.h"
#include "scene.h"
#include "doc.h"

#include "palettegenerator.h"
#include "qlcchannel.h"
#include "qlcfile.h"

#undef private
#undef protected

#include "../../../engine/test/common/resource_paths.h"

static QLCFixtureDef *createRGBPanTiltDef()
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel("RGB-PT");

    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("Standard");

    // ch0: Red
    QLCChannel *red = new QLCChannel();
    red->setName("Red");
    red->setGroup(QLCChannel::Intensity);
    red->setColour(QLCChannel::Red);
    def->addChannel(red);
    mode->insertChannel(red, 0);

    // ch1: Green
    QLCChannel *green = new QLCChannel();
    green->setName("Green");
    green->setGroup(QLCChannel::Intensity);
    green->setColour(QLCChannel::Green);
    def->addChannel(green);
    mode->insertChannel(green, 1);

    // ch2: Blue
    QLCChannel *blue = new QLCChannel();
    blue->setName("Blue");
    blue->setGroup(QLCChannel::Intensity);
    blue->setColour(QLCChannel::Blue);
    def->addChannel(blue);
    mode->insertChannel(blue, 2);

    // ch3: Dimmer
    QLCChannel *dimmer = new QLCChannel();
    dimmer->setName("Dimmer");
    dimmer->setGroup(QLCChannel::Intensity);
    dimmer->setColour(QLCChannel::NoColour);
    def->addChannel(dimmer);
    mode->insertChannel(dimmer, 3);

    // ch4: Pan
    QLCChannel *pan = new QLCChannel();
    pan->setName("Pan");
    pan->setGroup(QLCChannel::Pan);
    def->addChannel(pan);
    mode->insertChannel(pan, 4);

    // ch5: Tilt
    QLCChannel *tilt = new QLCChannel();
    tilt->setName("Tilt");
    tilt->setGroup(QLCChannel::Tilt);
    def->addChannel(tilt);
    mode->insertChannel(tilt, 5);

    // Set up fixture head with RGB channels
    QLCFixtureHead head;
    head.addChannel(0);
    head.addChannel(1);
    head.addChannel(2);
    head.addChannel(3);
    head.addChannel(4);
    head.addChannel(5);
    mode->insertHead(-1, head);

    def->addMode(mode);
    return def;
}

static QLCFixtureDef *createPrismDef()
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel("Prism-FX");

    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("Standard");

    // ch0: Prism with capabilities
    QLCChannel *prism = new QLCChannel();
    prism->setName("Prism");
    prism->setGroup(QLCChannel::Prism);
    prism->addCapability(new QLCCapability(0, 10, "Off"));
    prism->addCapability(new QLCCapability(11, 127, "Prism On"));
    prism->addCapability(new QLCCapability(128, 191, "CW Rotation"));
    prism->addCapability(new QLCCapability(192, 255, "CCW Rotation"));
    def->addChannel(prism);
    mode->insertChannel(prism, 0);

    // ch1: Dimmer
    QLCChannel *dimmer = new QLCChannel();
    dimmer->setName("Dimmer");
    dimmer->setGroup(QLCChannel::Intensity);
    dimmer->setColour(QLCChannel::NoColour);
    def->addChannel(dimmer);
    mode->insertChannel(dimmer, 1);

    QLCFixtureHead head;
    head.addChannel(0);
    head.addChannel(1);
    mode->insertHead(-1, head);
    def->addMode(mode);
    return def;
}

void PaletteGenerator_V5_Test::initTestCase()
{
    // Try loading from multiple relative paths since the binary
    // may be in different locations depending on build directory structure
    QStringList searchPaths = {
        INTERNAL_FIXTUREDIR,
        "../../../../resources/fixtures/",
        "../../../../../resources/fixtures/"
    };
    for (const QString &path : searchPaths)
    {
        QDir dir(path);
        dir.setFilter(QDir::Files);
        dir.setNameFilters(QStringList() << QString("*%1").arg(KExtFixture));
        if (m_fixtureDefCache.loadMap(dir))
            return;
    }
    qWarning() << "Fixture cache not loaded - some tests will be skipped";
}

void PaletteGenerator_V5_Test::capabilities()
{
    Doc doc(this);
    QLCFixtureDef *def = createRGBPanTiltDef();
    QLCFixtureMode *mode = def->modes().at(0);

    Fixture *fxi = new Fixture(&doc);
    fxi->setFixtureDefinition(def, mode);

    QStringList caps = PaletteGenerator::getCapabilities(fxi);
    QVERIFY(caps.contains(KQLCChannelMovement));
    QVERIFY(caps.contains(KQLCChannelRGB));
    QVERIFY(caps.contains(QStringLiteral("Dimmer")));
}

void PaletteGenerator_V5_Test::capabilitiesPrism()
{
    Doc doc(this);
    QLCFixtureDef *def = createPrismDef();
    QLCFixtureMode *mode = def->modes().at(0);

    Fixture *fxi = new Fixture(&doc);
    fxi->setFixtureDefinition(def, mode);

    QStringList caps = PaletteGenerator::getCapabilities(fxi);
    QVERIFY(caps.contains(QLCChannel::groupToString(QLCChannel::Prism)));
    QVERIFY(caps.contains(QStringLiteral("Dimmer")));
}

void PaletteGenerator_V5_Test::createPrimaryColors()
{
    Doc doc(this);
    QLCFixtureDef *def = createRGBPanTiltDef();
    QLCFixtureMode *mode = def->modes().at(0);

    Fixture *fxi = new Fixture(&doc);
    fxi->setFixtureDefinition(def, mode);
    doc.addFixture(fxi);

    QList<Fixture *> list;
    list << fxi;

    PaletteGenerator pg(&doc, list, PaletteGenerator::PrimaryColors);
    QVERIFY(pg.scenes().count() > 0);
    QVERIFY(pg.chasers().count() == 1);

    // Verify chaser is beat-synced
    Chaser *chaser = pg.chasers().at(0);
    QCOMPARE(chaser->tempoType(), Function::Beats);
    QCOMPARE(chaser->fadeInSpeed(), uint(2000));
    QCOMPARE(chaser->duration(), uint(4000));

    // Verify palettes generated
    QVERIFY(pg.palettes().count() > 0);
}

void PaletteGenerator_V5_Test::createGobos()
{
    Doc doc(this);

    QLCFixtureDef *fixtureDef;
    fixtureDef = m_fixtureDefCache.fixtureDef("Futurelight", "DJScan250");
    if (fixtureDef == nullptr)
    {
        QSKIP("Futurelight DJScan250 not found in fixture cache");
        return;
    }
    QLCFixtureMode *fixtureMode = fixtureDef->modes().at(0);

    Fixture *fxi = new Fixture(&doc);
    fxi->setFixtureDefinition(fixtureDef, fixtureMode);
    doc.addFixture(fxi);

    QList<Fixture *> list;
    list << fxi;

    PaletteGenerator pg(&doc, list, PaletteGenerator::Gobos);
    QVERIFY(pg.scenes().count() > 0);
    QVERIFY(pg.chasers().count() == 1);

    // Verify chaser is beat-synced
    Chaser *chaser = pg.chasers().at(0);
    QCOMPARE(chaser->tempoType(), Function::Beats);
}

void PaletteGenerator_V5_Test::createRainbow()
{
    Doc doc(this);
    QLCFixtureDef *def = createRGBPanTiltDef();
    QLCFixtureMode *mode = def->modes().at(0);

    Fixture *fxi = new Fixture(&doc);
    fxi->setFixtureDefinition(def, mode);
    doc.addFixture(fxi);

    QList<Fixture *> list;
    list << fxi;

    PaletteGenerator pg(&doc, list, PaletteGenerator::RainbowChaser);

    // Rainbow should create 8 scenes (R, O, Y, G, C, B, P, M)
    QCOMPARE(pg.scenes().count(), 8);
    QVERIFY(pg.chasers().count() >= 1);

    // Verify first scene is Red (255, 0, 0)
    Scene *redScene = pg.scenes().at(0);
    QVERIFY(redScene->values().count() >= 3);

    // Check the values: for a single fixture with R,G,B channels
    bool foundRed = false;
    for (const SceneValue &sv : redScene->values())
    {
        if (sv.channel == 0) // Red channel
        {
            QCOMPARE(sv.value, uchar(255));
            foundRed = true;
        }
        if (sv.channel == 1) // Green channel
            QCOMPARE(sv.value, uchar(0));
        if (sv.channel == 2) // Blue channel
            QCOMPARE(sv.value, uchar(0));
    }
    QVERIFY(foundRed);

    // Verify chaser timing
    Chaser *chaser = pg.chasers().at(0);
    QCOMPARE(chaser->tempoType(), Function::Beats);
    QCOMPARE(chaser->fadeInSpeed(), uint(2000)); // 2 beats
    QCOMPARE(chaser->duration(), uint(4000));     // 4 beats
    QCOMPARE(chaser->runOrder(), Function::Loop);

    // Verify color palettes generated
    QCOMPARE(pg.palettes().count(), 8);
}

void PaletteGenerator_V5_Test::createWarmColors()
{
    Doc doc(this);
    QLCFixtureDef *def = createRGBPanTiltDef();
    QLCFixtureMode *mode = def->modes().at(0);

    Fixture *fxi = new Fixture(&doc);
    fxi->setFixtureDefinition(def, mode);
    doc.addFixture(fxi);

    QList<Fixture *> list;
    list << fxi;

    PaletteGenerator pg(&doc, list, PaletteGenerator::WarmColors);

    // Warm should create 5 scenes
    QCOMPARE(pg.scenes().count(), 5);
    QVERIFY(pg.chasers().count() >= 1);

    // Warm chaser should use PingPong
    Chaser *chaser = pg.chasers().at(0);
    QCOMPARE(chaser->runOrder(), Function::PingPong);
    QCOMPARE(chaser->tempoType(), Function::Beats);
}

void PaletteGenerator_V5_Test::createCoolColors()
{
    Doc doc(this);
    QLCFixtureDef *def = createRGBPanTiltDef();
    QLCFixtureMode *mode = def->modes().at(0);

    Fixture *fxi = new Fixture(&doc);
    fxi->setFixtureDefinition(def, mode);
    doc.addFixture(fxi);

    QList<Fixture *> list;
    list << fxi;

    PaletteGenerator pg(&doc, list, PaletteGenerator::CoolColors);

    // Cool should create 5 scenes
    QCOMPARE(pg.scenes().count(), 5);
    QVERIFY(pg.chasers().count() >= 1);

    Chaser *chaser = pg.chasers().at(0);
    QCOMPARE(chaser->runOrder(), Function::PingPong);
    QCOMPARE(chaser->tempoType(), Function::Beats);
}

void PaletteGenerator_V5_Test::beatSyncedChasers()
{
    Doc doc(this);
    QLCFixtureDef *def = createRGBPanTiltDef();
    QLCFixtureMode *mode = def->modes().at(0);

    Fixture *fxi = new Fixture(&doc);
    fxi->setFixtureDefinition(def, mode);
    doc.addFixture(fxi);

    QList<Fixture *> list;
    list << fxi;

    // Primary Colors chaser
    PaletteGenerator pg1(&doc, list, PaletteGenerator::PrimaryColors);
    QVERIFY(pg1.chasers().count() >= 1);
    Chaser *c1 = pg1.chasers().at(0);
    QCOMPARE(c1->tempoType(), Function::Beats);
    QCOMPARE(c1->fadeInSpeed(), uint(2000));  // 2 beats
    QCOMPARE(c1->duration(), uint(4000));      // 4 beats

    // Pan/Tilt chaser (main cycle)
    PaletteGenerator pg2(&doc, list, PaletteGenerator::PanTilt);
    QVERIFY(pg2.chasers().count() >= 1);
    Chaser *c2 = pg2.chasers().at(0);
    QCOMPARE(c2->tempoType(), Function::Beats);
    QCOMPARE(c2->fadeInSpeed(), uint(4000));  // 4 beats
    QCOMPARE(c2->duration(), uint(4000));      // 4 beats
}

void PaletteGenerator_V5_Test::panTiltMovementChasers()
{
    Doc doc(this);
    QLCFixtureDef *def = createRGBPanTiltDef();
    QLCFixtureMode *mode = def->modes().at(0);

    Fixture *fxi = new Fixture(&doc);
    fxi->setFixtureDefinition(def, mode);
    doc.addFixture(fxi);

    QList<Fixture *> list;
    list << fxi;

    PaletteGenerator pg(&doc, list, PaletteGenerator::PanTilt);

    // Should have the main position cycle chaser + 4 movement chasers
    // (Pan Sweep, Tilt Sweep, Nod, Shake)
    QVERIFY(pg.chasers().count() >= 5);

    // Verify movement chasers have correct properties
    // Movement chasers start at index 1 (index 0 is the main cycle)
    bool foundPingPong = false;
    bool foundLoop = false;
    for (int i = 1; i < pg.chasers().count(); i++)
    {
        Chaser *c = pg.chasers().at(i);
        QCOMPARE(c->tempoType(), Function::Beats);

        if (c->runOrder() == Function::PingPong)
            foundPingPong = true;
        if (c->runOrder() == Function::Loop)
            foundLoop = true;
    }
    QVERIFY(foundPingPong); // Nod and Shake use PingPong
    QVERIFY(foundLoop);     // Pan Sweep and Tilt Sweep use Loop

    // Verify chaser scene mapping exists for movement chasers
    QVERIFY(pg.m_chaserSceneMap.count() >= 4);

    // Verify addToDoc works correctly with scene mapping
    pg.addToDoc();

    // Check that a movement chaser doesn't have all scenes
    for (auto it = pg.m_chaserSceneMap.begin(); it != pg.m_chaserSceneMap.end(); ++it)
    {
        Chaser *c = it.key();
        QVERIFY(c->stepsCount() < pg.scenes().count()); // Should have fewer steps than total scenes
        QVERIFY(c->stepsCount() >= 2); // At least 2 steps per movement
    }
}

void PaletteGenerator_V5_Test::createPrism()
{
    Doc doc(this);
    QLCFixtureDef *def = createPrismDef();
    QLCFixtureMode *mode = def->modes().at(0);

    Fixture *fxi = new Fixture(&doc);
    fxi->setFixtureDefinition(def, mode);
    doc.addFixture(fxi);

    QList<Fixture *> list;
    list << fxi;

    PaletteGenerator pg(&doc, list, PaletteGenerator::Prism);

    // Should create scenes for each prism capability
    QCOMPARE(pg.scenes().count(), 4); // Off, On, CW, CCW
    QVERIFY(pg.chasers().count() >= 1);

    Chaser *chaser = pg.chasers().at(0);
    QCOMPARE(chaser->tempoType(), Function::Beats);
}

void PaletteGenerator_V5_Test::colorPalettes()
{
    Doc doc(this);
    QLCFixtureDef *def = createRGBPanTiltDef();
    QLCFixtureMode *mode = def->modes().at(0);

    Fixture *fxi = new Fixture(&doc);
    fxi->setFixtureDefinition(def, mode);
    doc.addFixture(fxi);

    QList<Fixture *> list;
    list << fxi;

    PaletteGenerator pg(&doc, list, PaletteGenerator::RainbowChaser);

    // Rainbow generates 8 color palettes
    QCOMPARE(pg.palettes().count(), 8);

    // Verify palette types
    for (QLCPalette *p : pg.palettes())
    {
        QCOMPARE(p->type(), QLCPalette::Color);
        QVERIFY(!p->name().isEmpty());
        // Color palettes should have a valid QColor
        QColor c = p->rgbValue();
        QVERIFY(c.isValid());
    }
}

void PaletteGenerator_V5_Test::positionPalettes()
{
    Doc doc(this);
    QLCFixtureDef *def = createRGBPanTiltDef();
    QLCFixtureMode *mode = def->modes().at(0);

    Fixture *fxi = new Fixture(&doc);
    fxi->setFixtureDefinition(def, mode);
    doc.addFixture(fxi);

    QList<Fixture *> list;
    list << fxi;

    PaletteGenerator pg(&doc, list, PaletteGenerator::PanTilt);

    // PanTilt generates 9 position palettes
    QCOMPARE(pg.palettes().count(), 9);

    for (QLCPalette *p : pg.palettes())
    {
        QCOMPARE(p->type(), QLCPalette::PanTilt);
        QVERIFY(!p->name().isEmpty());
    }
}

void PaletteGenerator_V5_Test::dimmerPalettes()
{
    Doc doc(this);
    QLCFixtureDef *def = createRGBPanTiltDef();
    QLCFixtureMode *mode = def->modes().at(0);

    Fixture *fxi = new Fixture(&doc);
    fxi->setFixtureDefinition(def, mode);
    doc.addFixture(fxi);

    QList<Fixture *> list;
    list << fxi;

    PaletteGenerator pg(&doc, list, PaletteGenerator::Dimmer);

    // Dimmer generates 5 palettes (Full, 75%, 50%, 25%, Off)
    QCOMPARE(pg.palettes().count(), 5);

    for (QLCPalette *p : pg.palettes())
    {
        QCOMPARE(p->type(), QLCPalette::Dimmer);
        QVERIFY(!p->name().isEmpty());
    }
}

QTEST_APPLESS_MAIN(PaletteGenerator_V5_Test)
