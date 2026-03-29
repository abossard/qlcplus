/*
  Q Light Controller Plus - Unit test
  idempotency_test.cpp

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

#include "idempotency_test.h"

#include "idempotency.h"
#include "doc.h"
#include "scene.h"
#include "chaser.h"
#include "efx.h"
#include "collection.h"
#include "rgbmatrix.h"
#include "script.h"
#include "fixture.h"
#include "fixturegroup.h"

void McpIdempotency_Test::initTestCase()
{
    m_doc = new Doc(this);
}

void McpIdempotency_Test::cleanup()
{
    m_doc->clearContents();
}

// ========== mcp::findFunction ==========

void McpIdempotency_Test::findFunction_notFoundInEmptyDoc()
{
    QVERIFY(mcp::findFunction(m_doc, "nonexistent", Function::SceneType) == nullptr);
    QVERIFY(mcp::findFunction(m_doc, "nonexistent", Function::ChaserType) == nullptr);
}

void McpIdempotency_Test::findFunction_findsSceneByName()
{
    Scene *s = new Scene(m_doc);
    s->setName("Red Wash");
    m_doc->addFunction(s);

    Function *found = mcp::findFunction(m_doc, "Red Wash", Function::SceneType);
    QVERIFY(found != nullptr);
    QCOMPARE(found->id(), s->id());
    QCOMPARE(found->name(), QString("Red Wash"));
}

void McpIdempotency_Test::findFunction_findsChaserByName()
{
    Chaser *c = new Chaser(m_doc);
    c->setName("Color Chase");
    m_doc->addFunction(c);

    Function *found = mcp::findFunction(m_doc, "Color Chase", Function::ChaserType);
    QVERIFY(found != nullptr);
    QCOMPARE(found->id(), c->id());
}

void McpIdempotency_Test::findFunction_findsEfxByName()
{
    EFX *e = new EFX(m_doc);
    e->setName("Circle Motion");
    m_doc->addFunction(e);

    Function *found = mcp::findFunction(m_doc, "Circle Motion", Function::EFXType);
    QVERIFY(found != nullptr);
    QCOMPARE(found->id(), e->id());
}

void McpIdempotency_Test::findFunction_findsCollectionByName()
{
    Collection *col = new Collection(m_doc);
    col->setName("Mood: Party");
    m_doc->addFunction(col);

    Function *found = mcp::findFunction(m_doc, "Mood: Party", Function::CollectionType);
    QVERIFY(found != nullptr);
    QCOMPARE(found->id(), col->id());
}

void McpIdempotency_Test::findFunction_findsRgbMatrixByName()
{
    RGBMatrix *m = new RGBMatrix(m_doc);
    m->setName("Rainbow Wave");
    m_doc->addFunction(m);

    Function *found = mcp::findFunction(m_doc, "Rainbow Wave", Function::RGBMatrixType);
    QVERIFY(found != nullptr);
    QCOMPARE(found->id(), m->id());
}

void McpIdempotency_Test::findFunction_findsScriptByName()
{
    Script *sc = new Script(m_doc);
    sc->setName("Show Opener");
    m_doc->addFunction(sc);

    Function *found = mcp::findFunction(m_doc, "Show Opener", Function::ScriptType);
    QVERIFY(found != nullptr);
    QCOMPARE(found->id(), sc->id());
}

void McpIdempotency_Test::findFunction_discriminatesByType()
{
    // Create a Scene and a Chaser with the SAME name
    Scene *s = new Scene(m_doc);
    s->setName("Ambiguous");
    m_doc->addFunction(s);

    Chaser *c = new Chaser(m_doc);
    c->setName("Ambiguous");
    m_doc->addFunction(c);

    // findFunction should return the correct type
    Function *foundScene = mcp::findFunction(m_doc, "Ambiguous", Function::SceneType);
    QVERIFY(foundScene != nullptr);
    QCOMPARE(foundScene->id(), s->id());
    QCOMPARE(foundScene->type(), Function::SceneType);

    Function *foundChaser = mcp::findFunction(m_doc, "Ambiguous", Function::ChaserType);
    QVERIFY(foundChaser != nullptr);
    QCOMPARE(foundChaser->id(), c->id());
    QCOMPARE(foundChaser->type(), Function::ChaserType);

    // A type that doesn't exist should return nullptr
    QVERIFY(mcp::findFunction(m_doc, "Ambiguous", Function::EFXType) == nullptr);
}

void McpIdempotency_Test::findFunction_returnsNullForWrongName()
{
    Scene *s = new Scene(m_doc);
    s->setName("Exists");
    m_doc->addFunction(s);

    QVERIFY(mcp::findFunction(m_doc, "DoesNotExist", Function::SceneType) == nullptr);
    QVERIFY(mcp::findFunction(m_doc, "exists", Function::SceneType) == nullptr); // case-sensitive
}

// ========== mcp::findFixture ==========

void McpIdempotency_Test::findFixture_notFoundInEmptyDoc()
{
    QVERIFY(mcp::findFixture(m_doc, "nonexistent", 0, 0) == nullptr);
}

void McpIdempotency_Test::findFixture_findsExactMatch()
{
    Fixture *fxi = new Fixture(m_doc);
    fxi->setName("Par LED 1");
    fxi->setUniverse(0);
    fxi->setAddress(10);
    fxi->setChannels(6);
    m_doc->addFixture(fxi);

    Fixture *found = mcp::findFixture(m_doc, "Par LED 1", 0, 10);
    QVERIFY(found != nullptr);
    QCOMPARE(found->id(), fxi->id());
}

void McpIdempotency_Test::findFixture_noMatchDifferentName()
{
    Fixture *fxi = new Fixture(m_doc);
    fxi->setName("Par LED 1");
    fxi->setUniverse(0);
    fxi->setAddress(10);
    fxi->setChannels(6);
    m_doc->addFixture(fxi);

    QVERIFY(mcp::findFixture(m_doc, "Par LED 2", 0, 10) == nullptr);
}

void McpIdempotency_Test::findFixture_noMatchDifferentUniverse()
{
    Fixture *fxi = new Fixture(m_doc);
    fxi->setName("Par LED 1");
    fxi->setUniverse(0);
    fxi->setAddress(10);
    fxi->setChannels(6);
    m_doc->addFixture(fxi);

    QVERIFY(mcp::findFixture(m_doc, "Par LED 1", 1, 10) == nullptr);
}

void McpIdempotency_Test::findFixture_noMatchDifferentAddress()
{
    Fixture *fxi = new Fixture(m_doc);
    fxi->setName("Par LED 1");
    fxi->setUniverse(0);
    fxi->setAddress(10);
    fxi->setChannels(6);
    m_doc->addFixture(fxi);

    QVERIFY(mcp::findFixture(m_doc, "Par LED 1", 0, 20) == nullptr);
}

// ========== mcp::findFixtureGroup ==========

void McpIdempotency_Test::findFixtureGroup_notFoundInEmptyDoc()
{
    QVERIFY(mcp::findFixtureGroup(m_doc, "nonexistent") == nullptr);
}

void McpIdempotency_Test::findFixtureGroup_findsExactMatch()
{
    FixtureGroup *group = new FixtureGroup(m_doc);
    group->setName("Front Wash");
    m_doc->addFixtureGroup(group);

    FixtureGroup *found = mcp::findFixtureGroup(m_doc, "Front Wash");
    QVERIFY(found != nullptr);
    QCOMPARE(found->id(), group->id());
}

void McpIdempotency_Test::findFixtureGroup_noMatchDifferentName()
{
    FixtureGroup *group = new FixtureGroup(m_doc);
    group->setName("Front Wash");
    m_doc->addFixtureGroup(group);

    QVERIFY(mcp::findFixtureGroup(m_doc, "Back Wash") == nullptr);
}

QTEST_MAIN(McpIdempotency_Test)
