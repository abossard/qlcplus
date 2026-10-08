/*
  Q Light Controller Plus - Unit test
  delete_tools_test.cpp

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
#include <QBuffer>
#include <QXmlStreamWriter>
#include <nlohmann/json.hpp>

#include "delete_tools_test.h"
#include "tool_registry.h"
#include "vcbridge.h"
#include "doc.h"
#include "fixture.h"
#include "fixturegroup.h"
#include "scene.h"
#include "scenevalue.h"
#include "rgbmatrix.h"
#include "mastertimer.h"
#include "functionparent.h"
#include "sequence.h"
#include "inputoutputmap.h"

#include <fastmcpp/tools/manager.hpp>

using Json = nlohmann::json;

namespace {

/** Bridge fake that tracks only what vc_delete_pages needs: a list of pages. */
class PageVCBridge final : public VCBridge
{
public:
    QStringList pageNames;

    int addPage(const QString &name) override
    {
        pageNames.append(name);
        return pageNames.count() - 1;
    }
    QList<PageInfo> pages() const override
    {
        QList<PageInfo> out;
        for (const QString &name : pageNames)
        {
            PageInfo info;
            info.name = name;
            out.append(info);
        }
        return out;
    }
    int pagesCount() const override { return pageNames.count(); }
    bool deletePage(int pageIndex) override
    {
        // Mirrors VirtualConsole::deletePage: declines an invalid index and
        // never removes the last remaining page.
        if (pageIndex < 0 || pageIndex >= pageNames.count()) return false;
        if (pageNames.count() == 1) return false;
        pageNames.removeAt(pageIndex);
        return true;
    }

    int addFrame(int, const QRect &, const QString &, bool) override { return 1; }
    int addButton(int, const QRect &, quint32, const QString &, const QString &, int) override { return 2; }
    int addSlider(int, const QRect &, const QString &, const QString &, quint32,
                  const QList<QPair<quint32, quint32>> &) override { return 3; }
    int addXYPad(int, const QRect &, const QList<quint32> &) override { return 4; }
    int addCueList(int, const QRect &, quint32, const QString &) override { return 5; }
    int addLabel(int, const QRect &, const QString &) override { return 6; }
    bool mapWidgetInput(int, quint32, quint32) override { return true; }
    bool setWidgetFeedback(int, int, int, int, int, int, int) override { return true; }
    bool setWidgetColors(int, const QColor &, const QColor &) override { return true; }
    int addSpeedDial(int, const QRect &, const QList<quint32> &) override { return 7; }
    int addAudioTriggers(int, const QRect &) override { return 8; }
    int addClock(int, const QRect &, const QString &) override { return 9; }
    int addRecordPanel(int, const QRect &) override { return 10; }
};

Json parsed(const Json &value)
{
    if (value.is_string())
        return Json::parse(value.get<std::string>());
    return value;
}

Json deleteFixtures(Doc *doc, const Json &ids)
{
    fastmcpp::tools::ToolManager tm;
    registerQueryTools(tm, doc, nullptr);
    return parsed(tm.invoke("delete_fixtures", Json{{"ids", ids}}));
}

Json deleteFixtureGroups(Doc *doc, const Json &ids)
{
    fastmcpp::tools::ToolManager tm;
    registerFunctionTools(tm, doc);
    return parsed(tm.invoke("delete_fixture_groups", Json{{"ids", ids}}));
}

Json deletePages(Doc *doc, VCBridge *bridge, const Json &indexes)
{
    fastmcpp::tools::ToolManager tm;
    registerVCLayoutTools(tm, doc, bridge);
    return parsed(tm.invoke("vc_delete_pages", Json{{"pageIndexes", indexes}}));
}

Fixture *patchFixture(Doc *doc, const QString &name, quint32 address, quint32 channels)
{
    Fixture *fxi = new Fixture(doc);
    fxi->setName(name);
    fxi->setUniverse(0);
    fxi->setAddress(address);
    fxi->setChannels(channels);
    doc->addFixture(fxi);
    return fxi;
}

}

void McpDeleteTools_Test::init()
{
    m_doc = new Doc(this);
}

void McpDeleteTools_Test::cleanup()
{
    m_doc->masterTimer()->stop();
    delete m_doc;
    m_doc = nullptr;
}

// ─── delete_fixtures ───────────────────────────────────────────────────────

void McpDeleteTools_Test::deleteFixtures_removesFromDocAndFreesAddress()
{
    Fixture *keep = patchFixture(m_doc, "Keep", 0, 6);
    Fixture *drop = patchFixture(m_doc, "Drop", 10, 6);
    const quint32 dropID = drop->id();
    QCOMPARE(m_doc->fixtures().count(), 2);

    Json result = deleteFixtures(m_doc, Json::array({(int)dropID}));

    QCOMPARE(result.size(), (size_t)1);
    QCOMPARE(result[0].value("outcome", std::string()), std::string("deleted"));
    QCOMPARE(result[0].value("name", std::string()), std::string("Drop"));
    QCOMPARE(m_doc->fixtures().count(), 1);
    QVERIFY(m_doc->fixture(dropID) == NULL);
    QVERIFY(m_doc->fixture(keep->id()) != NULL);

    // The freed address range is claimable again — the whole point of unpatching.
    Fixture *reuse = patchFixture(m_doc, "Reuse", 10, 6);
    QCOMPARE(m_doc->fixtureForAddress((0 << 9) | 10), reuse->id());
}

void McpDeleteTools_Test::deleteFixtures_referencedByScene_scrubsSceneValues()
{
    Fixture *keep = patchFixture(m_doc, "Keep", 0, 4);
    Fixture *drop = patchFixture(m_doc, "Drop", 10, 4);

    Scene *scene = new Scene(m_doc);
    scene->setName("Wash");
    scene->setValue(SceneValue(keep->id(), 0, 255));
    scene->setValue(SceneValue(keep->id(), 1, 128));
    scene->setValue(SceneValue(drop->id(), 0, 64));
    scene->setValue(SceneValue(drop->id(), 2, 32));
    m_doc->addFunction(scene);
    QCOMPARE(scene->values().count(), 4);

    deleteFixtures(m_doc, Json::array({(int)drop->id()}));

    // Doc::deleteFixture signals fixtureRemoved and Scene scrubs its own values,
    // so no orphan SceneValue may survive.
    QCOMPARE(scene->values().count(), 2);
    for (const SceneValue &sv : scene->values())
        QVERIFY2(sv.fxi == keep->id(), "orphan SceneValue left behind");
}

void McpDeleteTools_Test::deleteFixtures_batchWithUnknownId_reportsPerItemError()
{
    Fixture *real = patchFixture(m_doc, "Real", 0, 6);

    Json result = deleteFixtures(m_doc, Json::array({(int)real->id(), 9999}));

    QCOMPARE(result.size(), (size_t)2);
    QCOMPARE(result[0].value("outcome", std::string()), std::string("deleted"));
    QCOMPARE(result[1].value("status", std::string()), std::string("error"));
    QCOMPARE(m_doc->fixtures().count(), 0);
}

void McpDeleteTools_Test::deleteFixtures_wrappingId_deletesNothing()
{
    Fixture *real = patchFixture(m_doc, "Real", 0, 6);

    // 2^32 + id would wrap onto the real fixture if narrowed unchecked.
    Json result = deleteFixtures(m_doc, Json::array({4294967296LL + real->id(), -1, 0.5}));

    QCOMPARE(result.size(), (size_t)3);
    for (const Json &r : result)
        QVERIFY2(r.value("status", std::string()) == "error" && r.contains("error"), result.dump().c_str());
    QCOMPARE(m_doc->fixtures().count(), 1);
}

void McpDeleteTools_Test::deleteFixtures_perInputRecords()
{
    Fixture *real = patchFixture(m_doc, "Real", 0, 6);
    const int id = int(real->id());

    Json result = deleteFixtures(m_doc, Json::array({9999, id, id, 0.5}));

    QVERIFY2(result.size() == 4, result.dump().c_str());
    const char *expected[][2] = {{"error", "not found"}, {"ok", "deleted"}, {"ok", "duplicate"}, {"error", "integer"}};
    for (int i = 0; i < 4; i++)
    {
        QCOMPARE(result[i].value("index", -1), i);
        QCOMPARE(result[i].value("status", std::string()), std::string(expected[i][0]));
        const std::string got = result[i].value(expected[i][0] == std::string("ok") ? "outcome" : "error", std::string());
        QVERIFY2(got.find(expected[i][1]) != std::string::npos, result.dump().c_str());
    }
    QCOMPARE(result[2].value("duplicateOf", -1), 1);
    QCOMPARE(m_doc->fixtures().count(), 0);
}

void McpDeleteTools_Test::deleteFixtures_savedXmlHasNoOrphanReference()
{
    Fixture *keep = patchFixture(m_doc, "Keep", 0, 4);
    Fixture *drop = patchFixture(m_doc, "Drop", 10, 4);
    const quint32 dropID = drop->id();

    Scene *scene = new Scene(m_doc);
    scene->setName("Wash");
    scene->setValue(SceneValue(keep->id(), 0, 255));
    scene->setValue(SceneValue(dropID, 0, 64));
    m_doc->addFunction(scene);

    deleteFixtures(m_doc, Json::array({(int)dropID}));

    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly);
    QXmlStreamWriter writer(&buffer);
    writer.writeStartDocument();
    QVERIFY(m_doc->saveXML(&writer));
    writer.writeEndDocument();
    buffer.close();

    // Nothing in the saved project may still point at the deleted fixture:
    // neither a <Fixture> block nor a scene value referencing its ID.
    const QString xml = QString::fromUtf8(data);
    QVERIFY2(!xml.contains(QString("FixtureVal ID=\"%1\"").arg(dropID)), qPrintable(xml));
    QVERIFY2(!xml.contains("Drop"), qPrintable(xml));

    // ...while the surviving fixture and its scene value are still there.
    QVERIFY2(xml.contains(QString("FixtureVal ID=\"%1\"").arg(keep->id())), qPrintable(xml));
    QVERIFY(xml.contains("Wash"));
    QVERIFY(xml.contains("Keep"));
}

void McpDeleteTools_Test::deleteFixtures_admission_data()
{
    QTest::addColumn<QString>("state");
    QTest::addColumn<QString>("targetCode");
    QTest::addColumn<QString>("freeCode");

    // Input is always [target, free]; empty code means outcome deleted.
    QTest::newRow("stopped unreferenced") << "idle" << "" << "";
    QTest::newRow("user scene queued") << "queued" << "functions_running" << "functions_running";
    QTest::newRow("user scene running") << "running" << "functions_running" << "functions_running";
    QTest::newRow("unrelated running") << "otherRunning" << "functions_running" << "functions_running";
    QTest::newRow("scene bound by sequence") << "bound" << "bound_scene" << "";
}

void McpDeleteTools_Test::deleteFixtures_admission()
{
    QFETCH(QString, state);
    QFETCH(QString, targetCode);
    QFETCH(QString, freeCode);

    Fixture *target = patchFixture(m_doc, "Target", 0, 4);
    Fixture *free = patchFixture(m_doc, "Free", 10, 4);
    Fixture *other = patchFixture(m_doc, "Other", 20, 4);
    const quint32 targetId = target->id(), freeId = free->id();

    Scene *user = new Scene(m_doc);
    user->setValue(SceneValue(targetId, 0, 200));
    QVERIFY(m_doc->addFunction(user));
    Scene *unrelated = new Scene(m_doc);
    unrelated->setValue(SceneValue(other->id(), 0, 100));
    QVERIFY(m_doc->addFunction(unrelated));
    FixtureGroup *group = new FixtureGroup(m_doc);
    group->setName("Pair");
    group->assignFixture(targetId);
    group->assignFixture(other->id());
    m_doc->addFixtureGroup(group);

    if (state == "bound")
    {
        Sequence *seq = new Sequence(m_doc);
        seq->setBoundSceneID(user->id());
        QVERIFY(m_doc->addFunction(seq));
    }
    Function *started = state == "otherRunning" ? unrelated
                      : (state == "queued" || state == "running") ? user : nullptr;
    if (started)
        started->start(m_doc->masterTimer(), FunctionParent::master());
    if (state == "running" || state == "otherRunning")
    {
        // Universes must run or the MasterTimer thread blocks on its first tick
        m_doc->inputOutputMap()->startUniverses();
        m_doc->masterTimer()->start();
        QTRY_VERIFY(started->isRunning());
    }
    m_doc->resetModified();

    Json result = deleteFixtures(m_doc, Json::array({(int)targetId, (int)freeId}));

    QVERIFY2(result.is_array() && result.size() == 2, result.dump().c_str());
    const QString codes[] = { targetCode, freeCode };
    const quint32 ids[] = { targetId, freeId };
    bool anyDeleted = false;
    for (int i = 0; i < 2; i++)
    {
        const Json &r = result[i];
        QCOMPARE(r.value("index", -1), i);
        if (codes[i].isEmpty())
        {
            QVERIFY2(r.value("outcome", "") == "deleted", r.dump().c_str());
            QVERIFY(m_doc->fixture(ids[i]) == nullptr);
            anyDeleted = true;
        }
        else
        {
            QVERIFY2(r.value("status", "") == "error", r.dump().c_str());
            QCOMPARE(QString::fromStdString(r.value("code", "")), codes[i]);
            QVERIFY(m_doc->fixture(ids[i]) != nullptr);
        }
    }
    const bool targetKept = !targetCode.isEmpty();
    QCOMPARE(user->values().count(), targetKept ? 1 : 0);
    QCOMPARE(group->fixtureList().contains(targetId), targetKept);
    QCOMPARE(m_doc->isModified(), anyDeleted);
    if (!started)
    {
        // Every admitted item released its registry edit
        QVERIFY(m_doc->masterTimer()->beginRegistryEdit());
        m_doc->masterTimer()->endFunctionEdit();
    }
}

// ─── delete_fixture_groups ─────────────────────────────────────────────────

void McpDeleteTools_Test::deleteFixtureGroups_removesGroupButKeepsFixtures()
{
    Fixture *f1 = patchFixture(m_doc, "F1", 0, 3);
    Fixture *f2 = patchFixture(m_doc, "F2", 3, 3);

    FixtureGroup *group = new FixtureGroup(m_doc);
    group->setName("Backline");
    group->assignFixture(f1->id());
    group->assignFixture(f2->id());
    m_doc->addFixtureGroup(group);
    const quint32 groupID = group->id();
    QCOMPARE(m_doc->fixtureGroups().count(), 1);

    Json result = deleteFixtureGroups(m_doc, Json::array({(int)groupID}));

    QCOMPARE(result[0].value("status", std::string()), std::string("ok"));
    QCOMPARE(result[0].value("outcome", std::string()), std::string("deleted"));
    QCOMPARE(result[0].value("name", std::string()), std::string("Backline"));
    QCOMPARE(m_doc->fixtureGroups().count(), 0);

    // Grouping is removed, the fixtures stay patched.
    QCOMPARE(m_doc->fixtures().count(), 2);
    QVERIFY(m_doc->fixture(f1->id()) != NULL);
    QVERIFY(m_doc->fixture(f2->id()) != NULL);
}

void McpDeleteTools_Test::deleteFixtureGroups_boundMatrix_rejected()
{
    Fixture *f1 = patchFixture(m_doc, "F1", 0, 3);

    FixtureGroup *group = new FixtureGroup(m_doc);
    group->setName("Matrix");
    group->setSize(QSize(1, 1));
    group->assignFixture(f1->id());
    m_doc->addFixtureGroup(group);

    RGBMatrix *matrix = new RGBMatrix(m_doc);
    matrix->setName("Rainbow");
    matrix->setFixtureGroup(group->id());
    m_doc->addFunction(matrix);

    Json result = deleteFixtureGroups(m_doc, Json::array({(int)group->id()}));

    // An RGBMatrix never drops its group reference, so deleting the group would
    // either dangle a raw pointer (running) or leave a dead id in the project.
    const std::string error = result[0].value("error", std::string());
    QVERIFY2(error.find("bound to one or more matrices") != std::string::npos,
             result[0].dump().c_str());
    QCOMPARE(result[0]["boundMatrices"].size(), (size_t)1);
    QCOMPARE(result[0]["boundMatrices"][0].value("name", std::string()), std::string("Rainbow"));
    QCOMPARE(result[0]["boundMatrices"][0].value("running", true), false);
    QCOMPARE(m_doc->fixtureGroups().count(), 1);
}

void McpDeleteTools_Test::deleteFixtureGroups_unboundGroup_deleted()
{
    Fixture *f1 = patchFixture(m_doc, "F1", 0, 3);

    FixtureGroup *bound = new FixtureGroup(m_doc);
    bound->setName("Bound");
    bound->assignFixture(f1->id());
    m_doc->addFixtureGroup(bound);

    FixtureGroup *free_ = new FixtureGroup(m_doc);
    free_->setName("Free");
    free_->assignFixture(f1->id());
    m_doc->addFixtureGroup(free_);

    RGBMatrix *matrix = new RGBMatrix(m_doc);
    matrix->setName("Rainbow");
    matrix->setFixtureGroup(bound->id());
    m_doc->addFunction(matrix);

    // The guard must key on the specific group, not refuse whenever any
    // matrix exists.
    Json result = deleteFixtureGroups(m_doc, Json::array({(int)free_->id()}));

    QCOMPARE(result[0].value("status", std::string()), std::string("ok"));
    QCOMPARE(result[0].value("outcome", std::string()), std::string("deleted"));
    QCOMPARE(m_doc->fixtureGroups().count(), 1);
    QCOMPARE(m_doc->fixtureGroups().first()->name(), QString("Bound"));
}

void McpDeleteTools_Test::deleteFixtures_emptiedGroup_isRemoved()
{
    Fixture *f1 = patchFixture(m_doc, "F1", 0, 3);
    Fixture *f2 = patchFixture(m_doc, "F2", 3, 3);

    FixtureGroup *emptied = new FixtureGroup(m_doc);
    emptied->setName("Solo");
    emptied->assignFixture(f1->id());
    m_doc->addFixtureGroup(emptied);

    FixtureGroup *survivor = new FixtureGroup(m_doc);
    survivor->setName("Keeps One");
    survivor->assignFixture(f1->id());
    survivor->assignFixture(f2->id());
    m_doc->addFixtureGroup(survivor);

    Json result = deleteFixtures(m_doc, Json::array({(int)f1->id()}));

    // A group left with no fixtures blocks reusing its name (#2063), so the
    // tool drops it and says so; the group that still holds F2 stays.
    QCOMPARE(m_doc->fixtureGroups().count(), 1);
    QCOMPARE(m_doc->fixtureGroups().first()->name(), QString("Keeps One"));

    // Nested in the item that emptied the group, not an extra top-level record.
    QCOMPARE(result.size(), (size_t)1);
    const Json &last = result[0];
    QVERIFY2(last.contains("removedEmptyGroups"), result.dump().c_str());
    QCOMPARE(last["removedEmptyGroups"].size(), (size_t)1);
    QCOMPARE(last["removedEmptyGroups"][0].value("name", std::string()), std::string("Solo"));
}

void McpDeleteTools_Test::deleteFixtureGroups_unknownId_notFound()
{
    Json result = deleteFixtureGroups(m_doc, Json::array({4242}));

    QCOMPARE(result[0].value("status", std::string()), std::string("error"));
    QCOMPARE(result[0].value("code", std::string()), std::string("not_found"));
}

// ─── vc_delete_pages ───────────────────────────────────────────────────────

void McpDeleteTools_Test::vcDeletePages_removesPage()
{
    PageVCBridge bridge;
    bridge.addPage("Intro");
    bridge.addPage("Drop");

    Json result = deletePages(m_doc, &bridge, Json::array({1}));

    QCOMPARE(result[0].value("status", std::string()), std::string("ok"));
    QCOMPARE(result[0].value("outcome", std::string()), std::string("deleted"));
    QCOMPARE(bridge.pageNames, QStringList{"Intro"});
}

void McpDeleteTools_Test::vcDeletePages_batchAscending_deletesHighestFirst()
{
    PageVCBridge bridge;
    bridge.addPage("Intro");
    bridge.addPage("Build");
    bridge.addPage("Drop");
    bridge.addPage("Outro");

    // Ascending input: without highest-first ordering the second removal would
    // land on a shifted page and take the wrong one.
    Json result = deletePages(m_doc, &bridge, Json::array({1, 2}));

    QCOMPARE(result.size(), (size_t)2);
    for (auto &entry : result)
        QVERIFY2(entry.value("outcome", std::string()) == "deleted", entry.dump().c_str());
    QCOMPARE(bridge.pageNames, (QStringList{"Intro", "Outro"}));
}

void McpDeleteTools_Test::vcDeletePages_lastRemaining_rejected()
{
    PageVCBridge bridge;
    bridge.addPage("Only");

    Json result = deletePages(m_doc, &bridge, Json::array({0}));

    // Assert the tool's own guard fired: the bridge also declines the last page,
    // so a bare "has an error key" check would pass without the guard.
    const std::string error = result[0].value("error", std::string());
    QVERIFY2(error.find("last remaining page") != std::string::npos, result[0].dump().c_str());
    QCOMPARE(bridge.pagesCount(), 1);
}

void McpDeleteTools_Test::vcDeletePages_batchDownToLastPage_keepsOne()
{
    PageVCBridge bridge;
    bridge.addPage("Intro");
    bridge.addPage("Drop");

    Json result = deletePages(m_doc, &bridge, Json::array({0, 1}));

    // Page 1 goes, page 0 is refused once it is the only one left; outcomes
    // keep the caller's order.
    QCOMPARE(result.size(), (size_t)2);
    QVERIFY2(result[0].contains("error"), result[0].dump().c_str());
    QCOMPARE(result[1].value("outcome", std::string()), std::string("deleted"));
    QCOMPARE(bridge.pageNames, QStringList{"Intro"});
}

void McpDeleteTools_Test::vcDeletePages_outOfRange_notFound()
{
    PageVCBridge bridge;
    bridge.addPage("Intro");
    bridge.addPage("Drop");

    Json result = deletePages(m_doc, &bridge, Json::array({7, -1}));

    QCOMPARE(result.size(), (size_t)2);
    for (auto &entry : result)
        QVERIFY2(entry.value("status", std::string()) == "error", entry.dump().c_str());
    QCOMPARE(bridge.pagesCount(), 2);
}

QTEST_MAIN(McpDeleteTools_Test)
