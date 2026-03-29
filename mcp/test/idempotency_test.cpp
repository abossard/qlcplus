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
#include "vcbridge.h"
#include "doc.h"
#include "scene.h"
#include "scenevalue.h"
#include "chaser.h"
#include "chaserstep.h"
#include "efx.h"
#include "collection.h"
#include "rgbmatrix.h"
#include "script.h"
#include "fixture.h"
#include "fixturegroup.h"

#include <set>
#include <vector>

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

// ========== ChaserStep per-step timing ==========

void McpIdempotency_Test::chaserStep_carriesPerStepTiming()
{
    Scene *s1 = new Scene(m_doc);
    s1->setName("Step1");
    m_doc->addFunction(s1);
    Scene *s2 = new Scene(m_doc);
    s2->setName("Step2");
    m_doc->addFunction(s2);

    Chaser *chaser = new Chaser(m_doc);
    chaser->setName("Timed Chaser");
    chaser->setFadeInMode(Chaser::PerStep);
    chaser->setFadeOutMode(Chaser::PerStep);
    chaser->setDurationMode(Chaser::PerStep);

    ChaserStep step1(s1->id(), 100, 2000, 300);
    ChaserStep step2(s2->id(), 0, 500, 0);
    chaser->addStep(step1);
    chaser->addStep(step2);
    m_doc->addFunction(chaser);

    // Verify step timing is preserved
    QCOMPARE(chaser->steps().size(), 2);
    QCOMPARE(chaser->steps().at(0).fadeIn, (uint)100);
    QCOMPARE(chaser->steps().at(0).hold, (uint)2000);
    QCOMPARE(chaser->steps().at(0).fadeOut, (uint)300);
    QCOMPARE(chaser->steps().at(1).fadeIn, (uint)0);
    QCOMPARE(chaser->steps().at(1).hold, (uint)500);
    QCOMPARE(chaser->steps().at(1).fadeOut, (uint)0);
}

// ========== Upsert behavior ==========

void McpIdempotency_Test::upsert_sceneUpdatesValues()
{
    // Create a fixture for the scene
    Fixture *fxi = new Fixture(m_doc);
    fxi->setName("Test Fix");
    fxi->setChannels(3);
    m_doc->addFixture(fxi);

    // Create a scene
    Scene *s = new Scene(m_doc);
    s->setName("Upsert Scene");
    s->setValue(SceneValue(fxi->id(), 0, 100));
    m_doc->addFunction(s);
    quint32 originalId = s->id();

    // Simulate upsert: find existing, clear, set new values
    Function *found = mcp::findFunction(m_doc, "Upsert Scene", Function::SceneType);
    QVERIFY(found != nullptr);
    Scene *existing = qobject_cast<Scene*>(found);
    QVERIFY(existing != nullptr);
    QCOMPARE(existing->id(), originalId);

    existing->clear();
    existing->setValue(SceneValue(fxi->id(), 0, 200));
    existing->setValue(SceneValue(fxi->id(), 1, 150));

    // Verify update happened in-place (same ID, new values)
    Scene *check = qobject_cast<Scene*>(m_doc->function(originalId));
    QVERIFY(check != nullptr);
    QCOMPARE((int)check->value(fxi->id(), 0), 200);
    QCOMPARE((int)check->value(fxi->id(), 1), 150);
}

void McpIdempotency_Test::upsert_collectionReplacesFunctions()
{
    Scene *s1 = new Scene(m_doc); s1->setName("S1"); m_doc->addFunction(s1);
    Scene *s2 = new Scene(m_doc); s2->setName("S2"); m_doc->addFunction(s2);
    Scene *s3 = new Scene(m_doc); s3->setName("S3"); m_doc->addFunction(s3);

    Collection *col = new Collection(m_doc);
    col->setName("Upsert Collection");
    col->addFunction(s1->id());
    col->addFunction(s2->id());
    m_doc->addFunction(col);
    quint32 originalId = col->id();
    QCOMPARE(col->functions().size(), 2);

    // Simulate upsert: clear and replace
    Function *found = mcp::findFunction(m_doc, "Upsert Collection", Function::CollectionType);
    Collection *existing = qobject_cast<Collection*>(found);
    for (quint32 fid : existing->functions())
        existing->removeFunction(fid);
    existing->addFunction(s2->id());
    existing->addFunction(s3->id());

    // Verify same ID, new members
    QCOMPARE(existing->id(), originalId);
    QCOMPARE(existing->functions().size(), 2);
    QVERIFY(existing->functions().contains(s2->id()));
    QVERIFY(existing->functions().contains(s3->id()));
    QVERIFY(!existing->functions().contains(s1->id()));
}

void McpIdempotency_Test::upsert_fixtureGroupReplacesFixtures()
{
    Fixture *f1 = new Fixture(m_doc); f1->setName("F1"); f1->setChannels(1); f1->setAddress(0); m_doc->addFixture(f1);
    Fixture *f2 = new Fixture(m_doc); f2->setName("F2"); f2->setChannels(1); f2->setAddress(1); m_doc->addFixture(f2);
    Fixture *f3 = new Fixture(m_doc); f3->setName("F3"); f3->setChannels(1); f3->setAddress(2); m_doc->addFixture(f3);

    FixtureGroup *group = new FixtureGroup(m_doc);
    group->setName("Upsert Group");
    group->setSize(QSize(3, 1));
    group->assignFixture(f1->id(), QLCPoint(0, 0));
    group->assignFixture(f2->id(), QLCPoint(1, 0));
    m_doc->addFixtureGroup(group);
    quint32 originalId = group->id();
    QCOMPARE(group->fixtureList().size(), 2);

    // Simulate upsert: clear and replace
    FixtureGroup *found = mcp::findFixtureGroup(m_doc, "Upsert Group");
    for (quint32 fid : found->fixtureList())
        found->resignFixture(fid);
    found->setSize(QSize(2, 1));
    found->assignFixture(f2->id(), QLCPoint(0, 0));
    found->assignFixture(f3->id(), QLCPoint(1, 0));

    QCOMPARE(found->id(), originalId);
    QCOMPARE(found->fixtureList().size(), 2);
    QVERIFY(found->fixtureList().contains(f2->id()));
    QVERIFY(found->fixtureList().contains(f3->id()));
}

// ========== Name resolution helpers ==========

void McpIdempotency_Test::resolveFixtures_globMatchesStar()
{
    Fixture *f1 = new Fixture(m_doc); f1->setName("Par LED 1"); f1->setChannels(1); f1->setAddress(0); m_doc->addFixture(f1);
    Fixture *f2 = new Fixture(m_doc); f2->setName("Par LED 2"); f2->setChannels(1); f2->setAddress(1); m_doc->addFixture(f2);
    Fixture *f3 = new Fixture(m_doc); f3->setName("Moving Head 1"); f3->setChannels(1); f3->setAddress(2); m_doc->addFixture(f3);

    QList<quint32> result = mcp::resolveFixturesByName(m_doc, "Par LED *");
    QCOMPARE(result.size(), 2);
    QVERIFY(result.contains(f1->id()));
    QVERIFY(result.contains(f2->id()));
    QVERIFY(!result.contains(f3->id()));
}

void McpIdempotency_Test::resolveFixtures_exactMatch()
{
    Fixture *f1 = new Fixture(m_doc); f1->setName("Spot A"); f1->setChannels(1); f1->setAddress(0); m_doc->addFixture(f1);

    QList<quint32> result = mcp::resolveFixturesByName(m_doc, "Spot A");
    QCOMPARE(result.size(), 1);
    QCOMPARE(result.first(), f1->id());
}

void McpIdempotency_Test::resolveFixtures_noMatch()
{
    Fixture *f1 = new Fixture(m_doc); f1->setName("Par LED 1"); f1->setChannels(1); m_doc->addFixture(f1);

    QList<quint32> result = mcp::resolveFixturesByName(m_doc, "Moving *");
    QCOMPARE(result.size(), 0);
}

void McpIdempotency_Test::resolveFunction_findsByName()
{
    Scene *s = new Scene(m_doc); s->setName("Warm Wash"); m_doc->addFunction(s);
    Chaser *c = new Chaser(m_doc); c->setName("Color Chase"); m_doc->addFunction(c);

    quint32 id = mcp::resolveFunctionByName(m_doc, "Warm Wash");
    QCOMPARE(id, s->id());

    id = mcp::resolveFunctionByName(m_doc, "Color Chase");
    QCOMPARE(id, c->id());
}

void McpIdempotency_Test::resolveFunction_findsByNameAndType()
{
    Scene *s = new Scene(m_doc); s->setName("Ambiguous"); m_doc->addFunction(s);
    Chaser *c = new Chaser(m_doc); c->setName("Ambiguous"); m_doc->addFunction(c);

    quint32 id = mcp::resolveFunctionByName(m_doc, "Ambiguous", Function::SceneType);
    QCOMPARE(id, s->id());

    id = mcp::resolveFunctionByName(m_doc, "Ambiguous", Function::ChaserType);
    QCOMPARE(id, c->id());

    id = mcp::resolveFunctionByName(m_doc, "Ambiguous", Function::EFXType);
    QCOMPARE(id, Function::invalidId());
}

void McpIdempotency_Test::resolveFunction_returnsInvalidWhenNotFound()
{
    QCOMPARE(mcp::resolveFunctionByName(m_doc, "Nothing"), Function::invalidId());
    QCOMPARE(mcp::resolveFunctionByName(m_doc, "Nothing", Function::SceneType), Function::invalidId());
}

// ========== Script deduplication ==========

// Mirrors the dedup algorithm from create_scripts tool
QString McpIdempotency_Test::buildDedupedScript(const QVector<QPair<QString,int>> &commands)
{
    QString scriptData;
    int i = 0;
    while (i < commands.size())
    {
        const auto &cmd = commands[i];
        if (cmd.first == "stopfunction" || cmd.first == "startfunction")
        {
            std::set<int> stopIDs;
            std::vector<int> startIDs;
            std::set<int> startIDSet;

            while (i < commands.size())
            {
                const auto &c = commands[i];
                if (c.first == "stopfunction")
                {
                    stopIDs.insert(c.second);
                    i++;
                }
                else if (c.first == "startfunction")
                {
                    if (startIDSet.find(c.second) == startIDSet.end())
                    {
                        startIDs.push_back(c.second);
                        startIDSet.insert(c.second);
                    }
                    i++;
                }
                else
                    break;
            }

            for (int sid : startIDSet)
                stopIDs.erase(sid);

            for (int sid : stopIDs)
                scriptData += QString("stopfunction:%1\n").arg(sid);
            for (int sid : startIDs)
                scriptData += QString("startfunction:%1\n").arg(sid);
        }
        else if (cmd.first == "wait")
        {
            scriptData += QString("wait:%1\n").arg(cmd.second);
            i++;
        }
        else
        {
            i++;
        }
    }
    return scriptData;
}

void McpIdempotency_Test::scriptDedup_removesStopForStartedFunction()
{
    // Stop 10, start 10 → only start 10 (function keeps running)
    QVector<QPair<QString,int>> commands = {
        {"stopfunction", 10},
        {"startfunction", 10}
    };
    QString result = buildDedupedScript(commands);
    QVERIFY(!result.contains("stopfunction:10"));
    QVERIFY(result.contains("startfunction:10"));
}

void McpIdempotency_Test::scriptDedup_removesDuplicateStops()
{
    // Stop 5 twice, stop 7 once → stop 5 once, stop 7 once
    QVector<QPair<QString,int>> commands = {
        {"stopfunction", 5},
        {"stopfunction", 5},
        {"stopfunction", 7}
    };
    QString result = buildDedupedScript(commands);
    QCOMPARE(result.count("stopfunction:5"), 1);
    QCOMPARE(result.count("stopfunction:7"), 1);
}

void McpIdempotency_Test::scriptDedup_preservesAcrossWait()
{
    // Stop 10, wait, start 10 → both stop and start preserved (wait breaks the block)
    QVector<QPair<QString,int>> commands = {
        {"stopfunction", 10},
        {"wait", 1000},
        {"startfunction", 10}
    };
    QString result = buildDedupedScript(commands);
    QVERIFY(result.contains("stopfunction:10"));
    QVERIFY(result.contains("wait:1000"));
    QVERIFY(result.contains("startfunction:10"));
}

/*************************************************************************
 * Flow layout positioning
 *************************************************************************/

void McpIdempotency_Test::flowLayout_singleWidget()
{
    // Single widget in a wide parent: placed at top-left
    QRect r = VCBridge::computeFlowPosition(1910, 40, 0, 100, 60, 0, 5);
    QCOMPARE(r.x(), 5);
    QCOMPARE(r.y(), 40);
    QVERIFY(r.width() > 0);
    QCOMPARE(r.height(), 60);
}

void McpIdempotency_Test::flowLayout_autoColumnsWrap()
{
    // Parent width 220, widget width 100, pad 5 → fits 2 per row
    // (220 - 5) / (100 + 5) = 2.04 → 2 columns
    QRect r0 = VCBridge::computeFlowPosition(220, 40, 0, 100, 60, 0, 5);
    QRect r1 = VCBridge::computeFlowPosition(220, 40, 1, 100, 60, 0, 5);
    QRect r2 = VCBridge::computeFlowPosition(220, 40, 2, 100, 60, 0, 5);

    // First two on same row, different X
    QCOMPARE(r0.y(), r1.y());
    QVERIFY(r1.x() > r0.x());

    // Third wraps to next row
    QCOMPARE(r2.x(), r0.x());
    QCOMPARE(r2.y(), 40 + 60 + 5);
}

void McpIdempotency_Test::flowLayout_explicitColumns()
{
    // Force 3 columns in a 1910-wide parent
    QRect r0 = VCBridge::computeFlowPosition(1910, 40, 0, 100, 60, 3, 5);
    QRect r1 = VCBridge::computeFlowPosition(1910, 40, 1, 100, 60, 3, 5);
    QRect r2 = VCBridge::computeFlowPosition(1910, 40, 2, 100, 60, 3, 5);
    QRect r3 = VCBridge::computeFlowPosition(1910, 40, 3, 100, 60, 3, 5);

    // All three on first row
    QCOMPARE(r0.y(), 40);
    QCOMPARE(r1.y(), 40);
    QCOMPARE(r2.y(), 40);

    // Fourth wraps to second row
    QCOMPARE(r3.y(), 40 + 60 + 5);
    QCOMPARE(r3.x(), r0.x());
}

void McpIdempotency_Test::flowLayout_evenWidthDistribution()
{
    // 4 columns in 1910-wide parent, pad 5
    // effectiveWidth = (1910 - 5*5) / 4 = 1885/4 = 471
    QRect r0 = VCBridge::computeFlowPosition(1910, 40, 0, 100, 60, 4, 5);
    QRect r1 = VCBridge::computeFlowPosition(1910, 40, 1, 100, 60, 4, 5);

    int expectedWidth = (1910 - 5 * 5) / 4;
    QCOMPARE(r0.width(), expectedWidth);
    QCOMPARE(r1.width(), expectedWidth);

    // Buttons fill available space evenly
    QCOMPARE(r0.x(), 5);
    QCOMPARE(r1.x(), 5 + expectedWidth + 5);
}

/*************************************************************************
 * Beat encoding — verifies the ×1000 contract for beat values
 *************************************************************************/

void McpIdempotency_Test::beatEncoding_wholeBeatsMultipliedBy1000()
{
    // MCP tool multiplies beat values by 1000 before storing.
    // Verify the engine interprets these correctly.
    Scene *s = new Scene(m_doc);
    s->setName("BeatStep");
    m_doc->addFunction(s);

    Chaser *chaser = new Chaser(m_doc);
    chaser->setName("Beat Chaser");
    chaser->setFadeInMode(Chaser::PerStep);
    chaser->setDurationMode(Chaser::PerStep);

    // Simulate what MCP does: user passes 7 beats, tool stores 7*1000=7000
    uint userBeats = 7;
    uint encoded = userBeats * 1000;
    chaser->addStep(ChaserStep(s->id(), encoded, encoded, 0));
    m_doc->addFunction(chaser);

    QCOMPARE(chaser->steps().at(0).fadeIn, (uint)7000);
    QCOMPARE(chaser->steps().at(0).hold, (uint)7000);

    // beatsToTime: 7000 beats at 120 BPM (500ms/beat) = 3500ms
    uint timeMs = Function::beatsToTime(7000, 500);
    QCOMPARE(timeMs, (uint)3500);
}

void McpIdempotency_Test::beatEncoding_zeroRemainsZero()
{
    // Zero should stay zero regardless of encoding
    uint encoded = 0 * 1000;
    QCOMPARE(encoded, (uint)0);
    QCOMPARE(Function::beatsToTime(0, 500), (uint)0);
}

QTEST_MAIN(McpIdempotency_Test)
