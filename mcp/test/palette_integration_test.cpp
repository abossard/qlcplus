/*
  Q Light Controller Plus - Unit test
  palette_integration_test.cpp

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

#include "palette_integration_test.h"

#include "conversions.h"
#include "doc.h"
#include "scene.h"
#include "scenevalue.h"
#include "qlcpalette.h"

void PaletteIntegration_Test::initTestCase()
{
    m_doc = new Doc(this);
}

void PaletteIntegration_Test::cleanup()
{
    m_doc->clearContents();
}

// ────────────────────────────────────────────
// QLCPalette CRUD via Doc
// ────────────────────────────────────────────

void PaletteIntegration_Test::palette_createDimmer()
{
    auto *pal = new QLCPalette(QLCPalette::Dimmer);
    pal->setName("Full");
    pal->setValue(QVariant(255));
    pal->setTemporary(false);

    QVERIFY(m_doc->addPalette(pal));
    QVERIFY(pal->id() != QLCPalette::invalidId());
    QCOMPARE(m_doc->palette(pal->id()), pal);
    QCOMPARE(pal->type(), QLCPalette::Dimmer);
    QCOMPARE(pal->name(), QString("Full"));
    QCOMPARE(pal->value().toInt(), 255);
}

void PaletteIntegration_Test::palette_createColor()
{
    auto *pal = new QLCPalette(QLCPalette::Color);
    pal->setName("Deep Blue");
    QColor rgb(0, 0, 255);
    QColor wauv(0, 0, 0);
    pal->setValue(QVariant(QLCPalette::colorToString(rgb, wauv)));
    pal->setTemporary(false);

    QVERIFY(m_doc->addPalette(pal));
    QVERIFY(pal->id() != QLCPalette::invalidId());
    QCOMPARE(pal->type(), QLCPalette::Color);

    // Verify the stored color can be parsed back
    QColor parsedRgb, parsedWauv;
    QVERIFY(QLCPalette::stringToColor(pal->value().toString(), parsedRgb, parsedWauv));
    QCOMPARE(parsedRgb, rgb);
}

void PaletteIntegration_Test::palette_createPan()
{
    auto *pal = new QLCPalette(QLCPalette::Pan);
    pal->setName("Center Pan");
    pal->setValue(QVariant(180.0));
    pal->setTemporary(false);

    QVERIFY(m_doc->addPalette(pal));
    QCOMPARE(pal->type(), QLCPalette::Pan);
    QCOMPARE(pal->value().toDouble(), 180.0);
}

void PaletteIntegration_Test::palette_createTilt()
{
    auto *pal = new QLCPalette(QLCPalette::Tilt);
    pal->setName("Tilt Down");
    pal->setValue(QVariant(45.0));
    pal->setTemporary(false);

    QVERIFY(m_doc->addPalette(pal));
    QCOMPARE(pal->type(), QLCPalette::Tilt);
    QCOMPARE(pal->value().toDouble(), 45.0);
}

void PaletteIntegration_Test::palette_createPanTilt()
{
    auto *pal = new QLCPalette(QLCPalette::PanTilt);
    pal->setName("Center Stage");
    pal->setValue(QVariant(180.0), QVariant(45.0));
    pal->setTemporary(false);

    QVERIFY(m_doc->addPalette(pal));
    QCOMPARE(pal->type(), QLCPalette::PanTilt);
    QVariantList vals = pal->values();
    QCOMPARE(vals.size(), 2);
    QCOMPARE(vals.at(0).toDouble(), 180.0);
    QCOMPARE(vals.at(1).toDouble(), 45.0);
}

void PaletteIntegration_Test::palette_deleteRemovesFromDoc()
{
    auto *pal = new QLCPalette(QLCPalette::Dimmer);
    pal->setName("ToDelete");
    pal->setValue(QVariant(128));
    pal->setTemporary(false);

    QVERIFY(m_doc->addPalette(pal));
    quint32 palId = pal->id();
    QVERIFY(m_doc->palette(palId) != nullptr);

    QVERIFY(m_doc->deletePalette(palId));
    QVERIFY(m_doc->palette(palId) == nullptr);
}

// ────────────────────────────────────────────
// Scene-palette integration
// ────────────────────────────────────────────

void PaletteIntegration_Test::scene_addPaletteRef()
{
    auto *pal = new QLCPalette(QLCPalette::Color);
    pal->setName("Test Color");
    pal->setValue(QVariant(QLCPalette::colorToString(QColor(255, 0, 0), QColor(0, 0, 0))));
    pal->setTemporary(false);
    QVERIFY(m_doc->addPalette(pal));

    auto *scene = new Scene(m_doc);
    scene->setName("Palette Scene");
    m_doc->addFunction(scene);

    scene->addPalette(pal->id());
    QCOMPARE(scene->palettes().size(), 1);
    QCOMPARE(scene->palettes().first(), pal->id());

    // Adding same palette again should not duplicate
    scene->addPalette(pal->id());
    QCOMPARE(scene->palettes().size(), 1);
}

void PaletteIntegration_Test::scene_addFixtureRegistration()
{
    auto *scene = new Scene(m_doc);
    scene->setName("Fixture Registration");
    m_doc->addFunction(scene);

    scene->addFixture(0);
    scene->addFixture(1);
    scene->addFixture(2);

    QCOMPARE(scene->fixtures().size(), 3);
    QVERIFY(scene->fixtures().contains(0));
    QVERIFY(scene->fixtures().contains(1));
    QVERIFY(scene->fixtures().contains(2));

    // Adding same fixture again should not duplicate
    scene->addFixture(0);
    QCOMPARE(scene->fixtures().size(), 3);
}

void PaletteIntegration_Test::scene_clearPalettesOnUpsert()
{
    auto *pal1 = new QLCPalette(QLCPalette::Dimmer);
    pal1->setName("Full");
    pal1->setValue(QVariant(255));
    pal1->setTemporary(false);
    QVERIFY(m_doc->addPalette(pal1));

    auto *pal2 = new QLCPalette(QLCPalette::Color);
    pal2->setName("Red");
    pal2->setValue(QVariant(QLCPalette::colorToString(QColor(255, 0, 0), QColor(0, 0, 0))));
    pal2->setTemporary(false);
    QVERIFY(m_doc->addPalette(pal2));

    auto *scene = new Scene(m_doc);
    scene->setName("Upsert Test");
    m_doc->addFunction(scene);

    scene->addPalette(pal1->id());
    scene->addPalette(pal2->id());
    QCOMPARE(scene->palettes().size(), 2);

    // Simulate upsert: clear palettes then add new ones
    for (quint32 pId : scene->palettes())
        scene->removePalette(pId);
    QCOMPARE(scene->palettes().size(), 0);

    scene->addPalette(pal1->id());
    QCOMPARE(scene->palettes().size(), 1);
}

// ────────────────────────────────────────────
// Fanning
// ────────────────────────────────────────────

void PaletteIntegration_Test::fanning_linearType()
{
    auto *pal = new QLCPalette(QLCPalette::Dimmer);
    pal->setName("Gradient");
    pal->setValue(QVariant(255));
    pal->setFanningType(QLCPalette::Linear);
    pal->setFanningAmount(500);
    pal->setFanningValue(QVariant(0));
    pal->setTemporary(false);
    QVERIFY(m_doc->addPalette(pal));

    QCOMPARE(pal->fanningType(), QLCPalette::Linear);
    QCOMPARE(pal->fanningAmount(), 500);
    QCOMPARE(pal->fanningValue().toInt(), 0);
}

void PaletteIntegration_Test::fanning_sineType()
{
    auto *pal = new QLCPalette(QLCPalette::Color);
    pal->setName("Color Wave");
    pal->setValue(QVariant(QLCPalette::colorToString(QColor(255, 0, 0), QColor(0, 0, 0))));
    pal->setFanningType(QLCPalette::Sine);
    pal->setFanningLayout(QLCPalette::XCentered);
    pal->setFanningAmount(1000);
    pal->setTemporary(false);
    QVERIFY(m_doc->addPalette(pal));

    QCOMPARE(pal->fanningType(), QLCPalette::Sine);
    QCOMPARE(pal->fanningLayout(), QLCPalette::XCentered);
    QCOMPARE(pal->fanningAmount(), 1000);
}

void PaletteIntegration_Test::fanning_layoutVariants()
{
    auto *pal = new QLCPalette(QLCPalette::Pan);
    pal->setName("Layout Test");
    pal->setValue(QVariant(90.0));
    pal->setTemporary(false);
    QVERIFY(m_doc->addPalette(pal));

    // Test all layout variants
    pal->setFanningLayout(QLCPalette::XAscending);
    QCOMPARE(pal->fanningLayout(), QLCPalette::XAscending);

    pal->setFanningLayout(QLCPalette::YDescending);
    QCOMPARE(pal->fanningLayout(), QLCPalette::YDescending);

    pal->setFanningLayout(QLCPalette::ZCentered);
    QCOMPARE(pal->fanningLayout(), QLCPalette::ZCentered);
}

void PaletteIntegration_Test::fanning_amountRange()
{
    auto *pal = new QLCPalette(QLCPalette::Dimmer);
    pal->setName("Amount Range");
    pal->setValue(QVariant(255));
    pal->setTemporary(false);
    QVERIFY(m_doc->addPalette(pal));

    pal->setFanningAmount(0);
    QCOMPARE(pal->fanningAmount(), 0);

    pal->setFanningAmount(1000);
    QCOMPARE(pal->fanningAmount(), 1000);

    pal->setFanningAmount(500);
    QCOMPARE(pal->fanningAmount(), 500);
}

// ────────────────────────────────────────────
// functionToJson with palette refs
// ────────────────────────────────────────────

void PaletteIntegration_Test::functionToJson_sceneWithPalettes()
{
    auto *pal1 = new QLCPalette(QLCPalette::Color);
    pal1->setName("Blue");
    pal1->setValue(QVariant(QLCPalette::colorToString(QColor(0, 0, 255), QColor(0, 0, 0))));
    pal1->setTemporary(false);
    QVERIFY(m_doc->addPalette(pal1));

    auto *pal2 = new QLCPalette(QLCPalette::Dimmer);
    pal2->setName("Full");
    pal2->setValue(QVariant(255));
    pal2->setTemporary(false);
    QVERIFY(m_doc->addPalette(pal2));

    auto *scene = new Scene(m_doc);
    scene->setName("Palette Scene JSON");
    m_doc->addFunction(scene);
    scene->addPalette(pal1->id());
    scene->addPalette(pal2->id());

    auto json = mcp::functionToJson(scene);

    QVERIFY(json.contains("paletteRefs"));
    QVERIFY(json["paletteRefs"].is_array());
    QCOMPARE((int)json["paletteRefs"].size(), 2);

    auto ref0 = json["paletteRefs"][0];
    QCOMPARE(ref0["id"].get<int>(), (int)pal1->id());
    QCOMPARE(ref0["name"].get<std::string>(), std::string("Blue"));
    QCOMPARE(ref0["type"].get<std::string>(), std::string("Color"));

    auto ref1 = json["paletteRefs"][1];
    QCOMPARE(ref1["id"].get<int>(), (int)pal2->id());
    QCOMPARE(ref1["name"].get<std::string>(), std::string("Full"));
    QCOMPARE(ref1["type"].get<std::string>(), std::string("Dimmer"));
}

void PaletteIntegration_Test::functionToJson_sceneWithoutPalettes()
{
    auto *scene = new Scene(m_doc);
    scene->setName("Plain Scene");
    m_doc->addFunction(scene);

    // Add a value so it's not completely empty
    scene->setValue(SceneValue(0, 0, 128));

    auto json = mcp::functionToJson(scene);

    QVERIFY(!json.contains("paletteRefs"));
    QCOMPARE(json["channelCount"].get<int>(), 1);
    QCOMPARE(json["fixtureCount"].get<int>(), 1);
}

// ────────────────────────────────────────────
// Palette value storage
// ────────────────────────────────────────────

void PaletteIntegration_Test::paletteValue_dimmerRange()
{
    auto *pal = new QLCPalette(QLCPalette::Dimmer);
    pal->setTemporary(false);

    // Test min
    pal->setValue(QVariant(0));
    QCOMPARE(pal->value().toInt(), 0);

    // Test max
    pal->setValue(QVariant(255));
    QCOMPARE(pal->value().toInt(), 255);

    // Test mid
    pal->setValue(QVariant(128));
    QCOMPARE(pal->value().toInt(), 128);

    delete pal;
}

void PaletteIntegration_Test::paletteValue_colorWithWAUV()
{
    QColor rgb(255, 128, 0);
    QColor wauv(100, 50, 25);

    QString colorStr = QLCPalette::colorToString(rgb, wauv);
    QVERIFY(!colorStr.isEmpty());

    QColor parsedRgb, parsedWauv;
    QVERIFY(QLCPalette::stringToColor(colorStr, parsedRgb, parsedWauv));
    QCOMPARE(parsedRgb, rgb);
    QCOMPARE(parsedWauv, wauv);
}

void PaletteIntegration_Test::paletteValue_panTiltDegrees()
{
    auto *pal = new QLCPalette(QLCPalette::PanTilt);
    pal->setName("Test PT");
    pal->setTemporary(false);

    // Standard position
    pal->setValue(QVariant(270.0), QVariant(135.0));
    QVariantList vals = pal->values();
    QCOMPARE(vals.size(), 2);
    QCOMPARE(vals.at(0).toDouble(), 270.0);
    QCOMPARE(vals.at(1).toDouble(), 135.0);

    // Zero position
    pal->setValue(QVariant(0.0), QVariant(0.0));
    vals = pal->values();
    QCOMPARE(vals.at(0).toDouble(), 0.0);
    QCOMPARE(vals.at(1).toDouble(), 0.0);

    // Max position (typical moving head)
    pal->setValue(QVariant(540.0), QVariant(270.0));
    vals = pal->values();
    QCOMPARE(vals.at(0).toDouble(), 540.0);
    QCOMPARE(vals.at(1).toDouble(), 270.0);

    delete pal;
}

QTEST_GUILESS_MAIN(PaletteIntegration_Test)
