/*
  Q Light Controller Plus - Unit test
  function_authoring_test.cpp

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

#include "function_authoring_test.h"
#include "tool_registry.h"
#include "doc.h"
#include "scene.h"
#include "chaser.h"
#include "chaserstep.h"
#include "sequence.h"
#include "efx.h"
#include "efxfixture.h"
#include "collection.h"
#include "script.h"
#include "qlcpalette.h"
#include "fixturegroup.h"
#include "channelsgroup.h"
#include "rgbmatrix.h"
#include "huematrix.h"
#include "mastertimer.h"
#include "fixture.h"
#include "scenevalue.h"
#include "inputoutputmap.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcfixturedefcache.h"
#include "qlcchannel.h"

#include <fastmcpp/tools/manager.hpp>
#include <fastmcpp/mcp/handler.hpp>
#include <fastmcpp/server/server.hpp>
#include <fastmcpp/resources/manager.hpp>
#include <fastmcpp/prompts/manager.hpp>

using Json = nlohmann::json;

namespace {

Fixture *patchDimmers(Doc *doc, quint32 address, quint32 channels)
{
    Fixture *fxi = new Fixture(doc);
    fxi->setAddress(address);
    fxi->setUniverse(0);
    fxi->setChannels(channels);
    doc->addFixture(fxi);
    return fxi;
}

// A pan/tilt head: the Doc's definition cache owns the definition
Fixture *patchMover(Doc *doc, quint32 address)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel("Mover");
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("2ch");
    const QLCChannel::Group groups[] = { QLCChannel::Pan, QLCChannel::Tilt };
    for (int i = 0; i < 2; ++i)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(i == 0 ? "Pan" : "Tilt");
        ch->setGroup(groups[i]);
        ch->setControlByte(QLCChannel::MSB);
        def->addChannel(ch);
        mode->insertChannel(ch, i);
    }
    def->addMode(mode);
    doc->fixtureDefCache()->addFixtureDef(def);

    Fixture *fxi = new Fixture(doc);
    fxi->setAddress(address);
    fxi->setUniverse(0);
    fxi->setFixtureDefinition(def, mode);
    doc->addFixture(fxi);
    return fxi;
}

} // namespace

Json McpFunctionAuthoring_Test::call(const std::string &tool, const Json &args)
{
    Json raw = m_tm->invoke(tool, args);
    return raw.is_string() ? Json::parse(raw.get<std::string>()) : raw;
}

void McpFunctionAuthoring_Test::init()
{
    m_doc = new Doc(this);
    m_tm = new fastmcpp::tools::ToolManager();
    registerFunctionTools(*m_tm, m_doc);
}

void McpFunctionAuthoring_Test::cleanup()
{
    // Every tool call, admitted or refused, must leave no edit admission behind
    const bool leaked = !m_doc->masterTimer()->beginFunctionEdit(QList<Function*>());
    m_doc->masterTimer()->endFunctionEdit();
    m_doc->masterTimer()->stop();
    delete m_tm;
    m_tm = nullptr;
    delete m_doc;
    m_doc = nullptr;
    QVERIFY2(!leaked, "an MCP call left a MasterTimer edit admission held");
}

void McpFunctionAuthoring_Test::deleteFunctions_admission_data()
{
    QTest::addColumn<QString>("otherState");
    QTest::addColumn<QString>("status");
    QTest::addColumn<QString>("code");
    QTest::addColumn<bool>("deleted");

    QTest::newRow("all idle") << "idle" << "ok" << "" << true;
    QTest::newRow("unrelated function queued") << "queued" << "error" << "functions_running" << false;
    QTest::newRow("unrelated function running") << "running" << "error" << "functions_running" << false;
}

void McpFunctionAuthoring_Test::deleteFunctions_admission()
{
    QFETCH(QString, otherState);
    QFETCH(QString, status);
    QFETCH(QString, code);
    QFETCH(bool, deleted);

    Scene *target = new Scene(m_doc);
    QVERIFY(m_doc->addFunction(target));
    const quint32 targetId = target->id();
    Scene *other = new Scene(m_doc);
    other->setValue(SceneValue(patchDimmers(m_doc, 0, 1)->id(), 0, 200));
    QVERIFY(m_doc->addFunction(other));
    if (otherState != "idle")
        other->start(m_doc->masterTimer(), FunctionParent::master());
    if (otherState == "running")
    {
        // Universes must run or the MasterTimer thread blocks on its first tick
        m_doc->inputOutputMap()->startUniverses();
        m_doc->masterTimer()->start();
        QTRY_VERIFY(other->isRunning());
    }

    Json res = call("delete_functions", {{"ids", {targetId}}});

    QVERIFY2(res.is_array() && res.size() == 1, res.dump().c_str());
    QCOMPARE(res[0].value("index", -1), 0);
    QVERIFY2(QString::fromStdString(res[0].value("status", "")) == status, res.dump().c_str());
    QCOMPARE(QString::fromStdString(res[0].value("code", "")), code);
    QCOMPARE(m_doc->function(targetId) == nullptr, deleted);
}

void McpFunctionAuthoring_Test::deleteFunctions_itemsIndependent()
{
    Scene *flashing = new Scene(m_doc);
    QVERIFY(m_doc->addFunction(flashing));
    flashing->flash(m_doc->masterTimer(), false, false);
    Scene *idle = new Scene(m_doc);
    QVERIFY(m_doc->addFunction(idle));
    const quint32 idleId = idle->id();

    Json res = call("delete_functions", {{"ids", {4242, idleId, "x", flashing->id(), -1}}});

    QVERIFY2(res.is_array() && res.size() == 5, res.dump().c_str());
    const char *codes[] = { "not_found", "", "invalid", "flashing", "invalid" };
    for (size_t i = 0; i < res.size(); ++i)
    {
        QCOMPARE(res[i].value("index", -1), int(i));
        QCOMPARE(res[i].value("code", std::string()), std::string(codes[i]));
        QCOMPARE(res[i].value("status", std::string()), std::string(i == 1 ? "ok" : "error"));
    }
    QVERIFY(m_doc->function(idleId) == nullptr);
    QVERIFY(m_doc->function(flashing->id()) == flashing);
    flashing->unFlash(m_doc->masterTimer());
}

void McpFunctionAuthoring_Test::deleteFunctions_boundScene_data()
{
    // Functions: 0 Bound (Scene), 1 First and 2 Second (Sequences bound to 0), 3 Free (Scene)
    QTest::addColumn<QString>("ids");
    QTest::addColumn<QString>("expected");
    QTest::addColumn<QList<int>>("survivors");

    QTest::newRow("bound scene names every referrer") << "[0]"
        << R"([{"index":0,"status":"error","code":"bound_scene","referrers":[1,2]}])" << QList<int>({ 0, 1, 2, 3 });
    QTest::newRow("unbound scene deleted") << "[3]"
        << R"([{"index":0,"status":"ok","id":3,"outcome":"deleted"}])" << QList<int>({ 0, 1, 2 });
    QTest::newRow("referrers deleted first release the scene") << "[1,2,0]"
        << R"([{"index":0,"status":"ok"},{"index":1,"status":"ok"},{"index":2,"status":"ok","id":0}])" << QList<int>({ 3 });
    QTest::newRow("scene before its last referrer") << "[1,0,2]"
        << R"([{"index":0,"status":"ok"},{"index":1,"status":"error","code":"bound_scene","referrers":[2]},{"index":2,"status":"ok"}])"
        << QList<int>({ 0, 3 });
}

void McpFunctionAuthoring_Test::deleteFunctions_boundScene()
{
    QFETCH(QString, ids);
    QFETCH(QString, expected);
    QFETCH(QList<int>, survivors);

    patchDimmers(m_doc, 0, 2);
    Scene *bound = new Scene(m_doc);
    bound->setName("Bound");
    bound->setValue(SceneValue(0, 0, 1));
    bound->setValue(SceneValue(0, 1, 2));
    QVERIFY(m_doc->addFunction(bound));
    for (const char *name : { "First", "Second" })
    {
        Sequence *seq = new Sequence(m_doc);
        seq->setName(name);
        seq->setBoundSceneID(bound->id());
        ChaserStep step(bound->id(), 0, 500, 0);
        step.values << SceneValue(0, 0, 10) << SceneValue(0, 1, 20);
        QVERIFY(seq->addStep(step));
        QVERIFY(m_doc->addFunction(seq));
    }
    Scene *free = new Scene(m_doc);
    free->setName("Free");
    QVERIFY(m_doc->addFunction(free));
    QCOMPARE(free->id(), quint32(3));

    const Json allTargets = {{{"id", 0}}, {{"id", 1}}, {{"id", 2}}, {{"id", 3}}};
    const Json before = call("query_function_details", {{"targets", allTargets}})["items"];
    m_doc->resetModified();

    const Json res = call("delete_functions", {{"ids", Json::parse(ids.toStdString())}});
    const Json want = Json::parse(expected.toStdString());
    QVERIFY2(res.is_array() && res.size() == want.size(), res.dump().c_str());
    for (size_t i = 0; i < want.size(); ++i)
        for (auto it = want[i].begin(); it != want[i].end(); ++it)
            QVERIFY2(res[i].value(it.key(), Json()) == it.value(), (it.key() + " in " + res.dump()).c_str());

    QList<int> present;
    for (int id = 0; id < 4; ++id)
        if (m_doc->function(quint32(id)) != nullptr)
            present << id;
    QCOMPARE(present, survivors);
    // A survivor keeps its binding, steps and values exactly
    for (int id : survivors)
    {
        const Json now = call("query_function_details", {{"targets", {{{"id", id}}}}})["items"][0];
        Json was = before[id];
        was["index"] = 0;
        if (id == 0 && !survivors.contains(1) && !survivors.contains(2))
            continue;
        QCOMPARE(now, was);
    }
    if (present.size() == 4)
        QVERIFY(!m_doc->isModified());
}

void McpFunctionAuthoring_Test::deleteFixtureGroups_indexedOutcomes_data()
{
    // Groups: 0 Free, 1 Bound (used by matrix "Rainbow"), 2 Other
    QTest::addColumn<QString>("ids");
    QTest::addColumn<QString>("expected");
    QTest::addColumn<QList<int>>("survivors");
    QTest::addColumn<bool>("isError");

    QTest::newRow("deleted") << "[0]"
        << R"([{"index":0,"status":"ok","id":0,"name":"Free","outcome":"deleted"}])" << QList<int>({ 1, 2 }) << false;
    QTest::newRow("not found") << "[9]" << R"([{"index":0,"status":"error","code":"not_found"}])"
        << QList<int>({ 0, 1, 2 }) << true;
    QTest::newRow("not an id") << R"(["x",-1])"
        << R"([{"index":0,"status":"error","code":"invalid"},{"index":1,"status":"error","code":"invalid"}])"
        << QList<int>({ 0, 1, 2 }) << true;
    QTest::newRow("bound to a matrix") << "[1]"
        << R"([{"index":0,"status":"error","code":"bound_matrix","id":1,"boundMatrices":[{"id":0,"name":"Rainbow","running":false}]}])"
        << QList<int>({ 0, 1, 2 }) << true;
    QTest::newRow("repeat of a deletion") << "[0,0]"
        << R"([{"index":0,"status":"ok","outcome":"deleted"},{"index":1,"status":"ok","id":0,"outcome":"duplicate","duplicateOf":0}])"
        << QList<int>({ 1, 2 }) << false;
    QTest::newRow("repeat of a refusal") << "[1,1]"
        << R"([{"index":0,"status":"error","code":"bound_matrix"},{"index":1,"status":"error","code":"bound_matrix","duplicateOf":0}])"
        << QList<int>({ 0, 1, 2 }) << true;
    QTest::newRow("repeat of a missing id") << "[9,9]"
        << R"([{"index":0,"status":"error","code":"not_found"},{"index":1,"status":"error","code":"not_found","duplicateOf":0}])"
        << QList<int>({ 0, 1, 2 }) << true;
    QTest::newRow("items independent") << R"([9,"x",2,0])"
        << R"([{"index":0,"code":"not_found"},{"index":1,"code":"invalid"},{"index":2,"status":"ok","id":2},{"index":3,"status":"ok","id":0}])"
        << QList<int>({ 1 }) << false;
}

void McpFunctionAuthoring_Test::deleteFixtureGroups_indexedOutcomes()
{
    QFETCH(QString, ids);
    QFETCH(QString, expected);
    QFETCH(QList<int>, survivors);
    QFETCH(bool, isError);

    patchDimmers(m_doc, 0, 1);
    for (const char *name : { "Free", "Bound", "Other" })
    {
        FixtureGroup *group = new FixtureGroup(m_doc);
        group->setName(name);
        QVERIFY(m_doc->addFixtureGroup(group));
    }
    RGBMatrix *matrix = new RGBMatrix(m_doc);
    matrix->setName("Rainbow");
    matrix->setFixtureGroup(1);
    QVERIFY(m_doc->addFunction(matrix));

    fastmcpp::server::Server server("qlcplus", "5.0.0");
    fastmcpp::resources::ResourceManager rm;
    fastmcpp::prompts::PromptManager pm;
    auto handler = mcp::withToolResultEncoding(
        fastmcpp::mcp::make_mcp_handler("qlcplus", "5.0.0", server, *m_tm, rm, pm));
    const Json response = handler({{"jsonrpc", "2.0"}, {"id", 2}, {"method", "tools/call"},
        {"params", {{"name", "delete_fixture_groups"}, {"arguments", {{"ids", Json::parse(ids.toStdString())}}}}}});
    const Json &result = response["result"];
    QVERIFY2(result.value("isError", false) == isError, response.dump().c_str());
    const Json res = Json::parse(result["content"][0]["text"].get<std::string>());

    const Json want = Json::parse(expected.toStdString());
    QVERIFY2(res.is_array() && res.size() == want.size(), res.dump().c_str());
    for (size_t i = 0; i < want.size(); ++i)
    {
        for (auto it = want[i].begin(); it != want[i].end(); ++it)
            QVERIFY2(res[i].value(it.key(), Json()) == it.value(), (it.key() + " in " + res.dump()).c_str());
        if (res[i].value("status", "") == "error")
            QVERIFY2(res[i]["error"].is_string(), res.dump().c_str());
    }

    QList<int> present;
    for (FixtureGroup *group : m_doc->fixtureGroups())
        present << int(group->id());
    std::sort(present.begin(), present.end());
    QCOMPARE(present, survivors);
}

void McpFunctionAuthoring_Test::createScenes_preflightAndAdmission_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<int>("value");
    QTest::addColumn<QString>("running");
    QTest::addColumn<QString>("status");
    QTest::addColumn<QString>("code");
    QTest::addColumn<QString>("outcome");

    QTest::newRow("new idle") << "Fresh" << 10 << "none" << "ok" << "" << "created";
    QTest::newRow("upsert idle") << "Existing" << 10 << "none" << "ok" << "" << "updated";
    QTest::newRow("upsert invalid value") << "Existing" << 300 << "none" << "error" << "invalid" << "";
    QTest::newRow("upsert while target running") << "Existing" << 10 << "existing" << "error" << "running" << "";
    QTest::newRow("upsert while unrelated running") << "Existing" << 10 << "other" << "ok" << "" << "updated";
    QTest::newRow("new while unrelated running") << "Fresh" << 10 << "other" << "error" << "functions_running" << "";
}

void McpFunctionAuthoring_Test::createScenes_preflightAndAdmission()
{
    QFETCH(QString, name);
    QFETCH(int, value);
    QFETCH(QString, running);
    QFETCH(QString, status);
    QFETCH(QString, code);
    QFETCH(QString, outcome);

    const quint32 fxId = patchDimmers(m_doc, 0, 2)->id();
    Scene *existing = new Scene(m_doc);
    existing->setName("Existing");
    existing->setPath("Keep");
    existing->setValue(SceneValue(fxId, 0, 77));
    QVERIFY(m_doc->addFunction(existing));
    Scene *other = new Scene(m_doc);
    other->setValue(SceneValue(fxId, 1, 50));
    QVERIFY(m_doc->addFunction(other));
    if (running != "none")
    {
        Scene *live = running == "existing" ? existing : other;
        live->start(m_doc->masterTimer(), FunctionParent::master());
        m_doc->inputOutputMap()->startUniverses();
        m_doc->masterTimer()->start();
        QTRY_VERIFY(live->isRunning());
    }
    m_doc->resetModified();
    const int functionCount = m_doc->functions().count();

    Json res = call("create_scenes", {{"items", {{
        {"name", name.toStdString()}, {"path", "New"},
        {"channelValues", {{{"fixtureID", fxId}, {"channel", 1}, {"value", value}}}}}}}});

    QVERIFY2(res.is_array() && res.size() == 1, res.dump().c_str());
    QCOMPARE(res[0].value("index", -1), 0);
    QVERIFY2(QString::fromStdString(res[0].value("status", "")) == status, res.dump().c_str());
    QCOMPARE(QString::fromStdString(res[0].value("code", "")), code);
    QCOMPARE(QString::fromStdString(res[0].value("outcome", "")), outcome);
    if (status == "error")
    {
        QCOMPARE(m_doc->functions().count(), functionCount);
        QCOMPARE(existing->path(true), QStringLiteral("Keep"));
        QCOMPARE(existing->values().count(), 1);
        QCOMPARE(existing->value(fxId, 0), uchar(77));
        QVERIFY(!m_doc->isModified());
    }
    else
    {
        Scene *s = qobject_cast<Scene*>(m_doc->function(res[0].value("id", 0u)));
        QVERIFY(s != nullptr);
        QCOMPARE(s->path(true), QStringLiteral("New"));
        QCOMPARE(s->values().count(), 1);
        QCOMPARE(s->value(fxId, 1), uchar(value));
    }
}

void McpFunctionAuthoring_Test::createChasers_preflightAndAdmission_data()
{
    QTest::addColumn<QString>("step");
    QTest::addColumn<bool>("queueTarget");
    QTest::addColumn<QString>("code");

    QTest::newRow("valid upsert") << "valid" << false << "";
    QTest::newRow("unknown step function id") << "unknownId" << false << "invalid";
    QTest::newRow("unresolved step function name") << "unknownName" << false << "invalid";
    QTest::newRow("step references the chaser itself") << "self" << false << "invalid";
    QTest::newRow("negative hold") << "negativeHold" << false << "invalid";
    QTest::newRow("target queued") << "valid" << true << "running";
}

void McpFunctionAuthoring_Test::createChasers_preflightAndAdmission()
{
    QFETCH(QString, step);
    QFETCH(bool, queueTarget);
    QFETCH(QString, code);

    Scene *kept = new Scene(m_doc);
    QVERIFY(m_doc->addFunction(kept));
    Scene *replacement = new Scene(m_doc);
    replacement->setName("Replacement");
    QVERIFY(m_doc->addFunction(replacement));
    Chaser *existing = new Chaser(m_doc);
    existing->setName("Existing");
    existing->setPath("Keep");
    QVERIFY(existing->addStep(ChaserStep(kept->id(), 0, 100, 0)));
    QVERIFY(m_doc->addFunction(existing));
    if (queueTarget)
        existing->start(m_doc->masterTimer(), FunctionParent::master());
    m_doc->resetModified();

    Json stepJson = {{"functionID", replacement->id()}, {"hold", 500}};
    if (step == "unknownId") stepJson = {{"functionID", 4242}};
    if (step == "unknownName") stepJson = {{"functionName", "Nope"}};
    if (step == "self") stepJson = {{"functionID", existing->id()}};
    if (step == "negativeHold") stepJson = {{"functionID", replacement->id()}, {"hold", -5}};

    Json res = call("create_chasers", {{"items", {{
        {"name", "Existing"}, {"path", "New"}, {"steps", {stepJson}}}}}});

    QVERIFY2(res.is_array() && res.size() == 1, res.dump().c_str());
    QCOMPARE(res[0].value("index", -1), 0);
    QVERIFY2(QString::fromStdString(res[0].value("code", "")) == code, res.dump().c_str());
    if (code.isEmpty())
    {
        QCOMPARE(QString::fromStdString(res[0].value("outcome", "")), QStringLiteral("updated"));
        QCOMPARE(existing->path(true), QStringLiteral("New"));
        QCOMPARE(existing->steps().count(), 1);
        QCOMPARE(existing->steps().at(0).fid, replacement->id());
        QCOMPARE(existing->steps().at(0).hold, 500u);
    }
    else
    {
        QCOMPARE(QString::fromStdString(res[0].value("status", "")), QStringLiteral("error"));
        QCOMPARE(existing->path(true), QStringLiteral("Keep"));
        QCOMPARE(existing->steps().count(), 1);
        QCOMPARE(existing->steps().at(0).fid, kept->id());
        QVERIFY(!m_doc->isModified());
    }
}

void McpFunctionAuthoring_Test::createSequences_binding_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<QString>("bind");
    QTest::addColumn<QString>("code");

    QTest::newRow("new bound to scene") << "Fresh" << "otherScene" << "";
    QTest::newRow("new bound to unknown id") << "Fresh" << "unknown" << "invalid";
    QTest::newRow("new bound to a chaser") << "Fresh" << "chaser" << "invalid";
    QTest::newRow("upsert keeps binding and steps") << "Existing" << "boundScene" << "";
    QTest::newRow("upsert rebinding a sequence with steps") << "Existing" << "otherScene" << "invalid";
}

void McpFunctionAuthoring_Test::createSequences_binding()
{
    QFETCH(QString, name);
    QFETCH(QString, bind);
    QFETCH(QString, code);

    const quint32 fxId = patchDimmers(m_doc, 0, 1)->id();
    Scene *bound = new Scene(m_doc);
    bound->setValue(SceneValue(fxId, 0, 0));
    QVERIFY(m_doc->addFunction(bound));
    Scene *otherScene = new Scene(m_doc);
    QVERIFY(m_doc->addFunction(otherScene));
    Chaser *chaser = new Chaser(m_doc);
    QVERIFY(m_doc->addFunction(chaser));
    Sequence *existing = new Sequence(m_doc);
    existing->setName("Existing");
    existing->setBoundSceneID(bound->id());
    ChaserStep step(bound->id());
    step.values.append(SceneValue(fxId, 0, 99));
    QVERIFY(existing->addStep(step));
    QVERIFY(m_doc->addFunction(existing));
    m_doc->resetModified();
    const int functionCount = m_doc->functions().count();

    quint32 bindId = 4242;
    if (bind == "otherScene") bindId = otherScene->id();
    if (bind == "chaser") bindId = chaser->id();
    if (bind == "boundScene") bindId = bound->id();

    Json res = call("create_sequences", {{"items", {{{"name", name.toStdString()}, {"boundSceneID", bindId}}}}});

    QVERIFY2(res.is_array() && res.size() == 1, res.dump().c_str());
    QCOMPARE(res[0].value("index", -1), 0);
    QVERIFY2(QString::fromStdString(res[0].value("code", "")) == code, res.dump().c_str());
    QCOMPARE(existing->boundSceneID(), bound->id());
    QCOMPARE(existing->steps().count(), 1);
    QCOMPARE(existing->steps().at(0).values.at(0).value, uchar(99));
    if (code.isEmpty())
    {
        Sequence *seq = qobject_cast<Sequence*>(m_doc->function(res[0].value("id", 0u)));
        QVERIFY(seq != nullptr);
        QCOMPARE(seq->boundSceneID(), bindId);
    }
    else
    {
        QCOMPARE(m_doc->functions().count(), functionCount);
        QVERIFY(!m_doc->isModified());
    }
}

void McpFunctionAuthoring_Test::createScenes_boundSceneChannelSet_data()
{
    QTest::addColumn<QList<int>>("channels");
    QTest::addColumn<QString>("code");

    QTest::newRow("same channel set, new values") << QList<int>({ 0, 1 }) << "";
    QTest::newRow("channel removed") << QList<int>({ 0 }) << "bound_scene";
    QTest::newRow("channel added") << QList<int>({ 0, 1, 2 }) << "bound_scene";
}

void McpFunctionAuthoring_Test::createScenes_boundSceneChannelSet()
{
    QFETCH(QList<int>, channels);
    QFETCH(QString, code);

    const quint32 fxId = patchDimmers(m_doc, 0, 3)->id();
    Scene *bound = new Scene(m_doc);
    bound->setName("Bound");
    bound->setValue(SceneValue(fxId, 0, 1));
    bound->setValue(SceneValue(fxId, 1, 2));
    QVERIFY(m_doc->addFunction(bound));
    Sequence *seq = new Sequence(m_doc);
    seq->setBoundSceneID(bound->id());
    QVERIFY(m_doc->addFunction(seq));
    m_doc->resetModified();

    Json values = Json::array();
    for (int ch : channels)
        values.push_back({{"fixtureID", fxId}, {"channel", ch}, {"value", 200}});
    Json res = call("create_scenes", {{"items", {{{"name", "Bound"}, {"channelValues", values}}}}});

    QVERIFY2(res.is_array() && res.size() == 1, res.dump().c_str());
    QVERIFY2(QString::fromStdString(res[0].value("code", "")) == code, res.dump().c_str());
    if (code.isEmpty())
        QCOMPARE(bound->value(fxId, 1), uchar(200));
    else
    {
        QCOMPARE(bound->values().count(), 2);
        QCOMPARE(bound->value(fxId, 1), uchar(2));
        QVERIFY(!m_doc->isModified());
    }
}

void McpFunctionAuthoring_Test::createEfxs_preflightAndAdmission_data()
{
    QTest::addColumn<QString>("field");
    QTest::addColumn<QVariant>("value");
    QTest::addColumn<bool>("queueTarget");
    QTest::addColumn<QString>("code");

    QTest::newRow("valid upsert") << "width" << QVariant(100) << false << "";
    QTest::newRow("width above engine range") << "width" << QVariant(200) << false << "invalid";
    QTest::newRow("width not an integer") << "width" << QVariant("wide") << false << "invalid";
    QTest::newRow("rotation fractional") << "rotation" << QVariant(1.5) << false << "invalid";
    QTest::newRow("head beyond fixture heads") << "head" << QVariant(3) << false << "invalid";
    QTest::newRow("unknown fixture") << "fixtureIDs" << QVariant(4242) << false << "invalid";
    QTest::newRow("target queued") << "width" << QVariant(100) << true << "running";
}

void McpFunctionAuthoring_Test::createEfxs_preflightAndAdmission()
{
    QFETCH(QString, field);
    QFETCH(QVariant, value);
    QFETCH(bool, queueTarget);
    QFETCH(QString, code);

    const quint32 keptFx = patchDimmers(m_doc, 0, 1)->id();
    const quint32 newFx = patchDimmers(m_doc, 1, 1)->id();
    EFX *existing = new EFX(m_doc);
    existing->setName("Existing");
    existing->setPath("Keep");
    existing->setWidth(50);
    EFXFixture *ef = new EFXFixture(existing);
    ef->setHead(GroupHead(keptFx, 0));
    QVERIFY(existing->addFixture(ef));
    QVERIFY(m_doc->addFunction(existing));
    if (queueTarget)
        existing->start(m_doc->masterTimer(), FunctionParent::master());
    m_doc->resetModified();

    Json item = {{"name", "Existing"}, {"algorithm", "Line"}, {"path", "New"}, {"fixtureIDs", {newFx}}};
    if (field == "fixtureIDs")
        item["fixtureIDs"] = {value.toInt()};
    else if (value.typeId() == QMetaType::QString)
        item[field.toStdString()] = value.toString().toStdString();
    else if (value.typeId() == QMetaType::Double)
        item[field.toStdString()] = value.toDouble();
    else
        item[field.toStdString()] = value.toInt();

    Json res = call("create_efxs", {{"items", {item}}});

    QVERIFY2(res.is_array() && res.size() == 1, res.dump().c_str());
    QCOMPARE(res[0].value("index", -1), 0);
    QVERIFY2(QString::fromStdString(res[0].value("code", "")) == code, res.dump().c_str());
    if (code.isEmpty())
    {
        QCOMPARE(existing->width(), 100);
        QCOMPARE(existing->path(true), QStringLiteral("New"));
        QCOMPARE(existing->fixtures().count(), 1);
        QCOMPARE(existing->fixtures().at(0)->head().fxi, newFx);
    }
    else
    {
        QCOMPARE(existing->width(), 50);
        QCOMPARE(existing->path(true), QStringLiteral("Keep"));
        QCOMPARE(existing->fixtures().count(), 1);
        QCOMPARE(existing->fixtures().at(0)->head().fxi, keptFx);
        QVERIFY(!m_doc->isModified());
    }
}

void McpFunctionAuthoring_Test::createCollections_preflightAndAdmission_data()
{
    QTest::addColumn<QString>("member");
    QTest::addColumn<bool>("queueTarget");
    QTest::addColumn<QString>("code");

    QTest::newRow("valid upsert") << "valid" << false << "";
    QTest::newRow("unknown member id") << "unknownId" << false << "invalid";
    QTest::newRow("unresolved member name") << "unknownName" << false << "invalid";
    QTest::newRow("member is the collection itself") << "self" << false << "invalid";
    QTest::newRow("target queued") << "valid" << true << "running";
}

void McpFunctionAuthoring_Test::createCollections_preflightAndAdmission()
{
    QFETCH(QString, member);
    QFETCH(bool, queueTarget);
    QFETCH(QString, code);

    Scene *kept = new Scene(m_doc);
    QVERIFY(m_doc->addFunction(kept));
    Scene *replacement = new Scene(m_doc);
    QVERIFY(m_doc->addFunction(replacement));
    Collection *existing = new Collection(m_doc);
    existing->setName("Existing");
    existing->setPath("Keep");
    QVERIFY(existing->addFunction(kept->id()));
    QVERIFY(m_doc->addFunction(existing));
    if (queueTarget)
        existing->start(m_doc->masterTimer(), FunctionParent::master());
    m_doc->resetModified();

    Json item = {{"name", "Existing"}, {"path", "New"}, {"functionIDs", {replacement->id()}}};
    if (member == "unknownId") item["functionIDs"] = {4242};
    if (member == "unknownName") item["functionNames"] = {"Nope"};
    if (member == "self") item["functionIDs"] = {existing->id()};

    Json res = call("create_collections", {{"items", {item}}});

    QVERIFY2(res.is_array() && res.size() == 1, res.dump().c_str());
    QCOMPARE(res[0].value("index", -1), 0);
    QVERIFY2(QString::fromStdString(res[0].value("code", "")) == code, res.dump().c_str());
    const QList<quint32> expected = code.isEmpty() ? QList<quint32>({ replacement->id() })
                                                   : QList<quint32>({ kept->id() });
    QCOMPARE(existing->functions(), expected);
    QCOMPARE(existing->path(true), code.isEmpty() ? QStringLiteral("New") : QStringLiteral("Keep"));
    if (!code.isEmpty())
        QVERIFY(!m_doc->isModified());
}

void McpFunctionAuthoring_Test::createScripts_preflightAndAdmission_data()
{
    QTest::addColumn<QString>("content");
    QTest::addColumn<bool>("queueTarget");
    QTest::addColumn<QString>("code");

    QTest::newRow("valid upsert") << "var a = 1;" << false << "";
    QTest::newRow("syntax error keeps path") << "var = ;" << false << "invalid";
    QTest::newRow("target queued") << "var a = 1;" << true << "running";
}

void McpFunctionAuthoring_Test::createScripts_preflightAndAdmission()
{
    QFETCH(QString, content);
    QFETCH(bool, queueTarget);
    QFETCH(QString, code);

    Script *existing = new Script(m_doc);
    existing->setName("Existing");
    existing->setPath("Keep");
    existing->setData("var old = 0;");
    QVERIFY(m_doc->addFunction(existing));
    if (queueTarget)
        existing->start(m_doc->masterTimer(), FunctionParent::master());
    m_doc->resetModified();

    Json res = call("create_scripts", {{"items", {{{"name", "Existing"}, {"path", "New"},
                                                   {"content", content.toStdString()}}}}});

    QVERIFY2(res.is_array() && res.size() == 1, res.dump().c_str());
    QCOMPARE(res[0].value("index", -1), 0);
    QVERIFY2(QString::fromStdString(res[0].value("code", "")) == code, res.dump().c_str());
    QCOMPARE(existing->data(), code.isEmpty() ? content : QStringLiteral("var old = 0;"));
    QCOMPARE(existing->path(true), code.isEmpty() ? QStringLiteral("New") : QStringLiteral("Keep"));
    if (!code.isEmpty())
        QVERIFY(!m_doc->isModified());
}

void McpFunctionAuthoring_Test::createFixtureGroups_preflightAndAdmission_data()
{
    QTest::addColumn<QString>("groupName");
    QTest::addColumn<QString>("member");
    QTest::addColumn<QString>("queuedMatrix");
    QTest::addColumn<QString>("code");

    QTest::newRow("idle bound matrix, valid upsert") << "Existing" << "valid" << "" << "";
    QTest::newRow("non-integer fixture id") << "Existing" << "text" << "" << "invalid";
    QTest::newRow("bound RGB matrix queued") << "Existing" << "valid" << "rgb" << "running";
    QTest::newRow("bound HUE matrix queued") << "Existing" << "valid" << "hue" << "running";
    QTest::newRow("new group while a matrix is queued") << "Fresh" << "valid" << "rgb" << "running";
    QTest::newRow("unknown fixture id") << "Existing" << "unknown" << "" << "invalid";
}

void McpFunctionAuthoring_Test::createFixtureGroups_preflightAndAdmission()
{
    QFETCH(QString, groupName);
    QFETCH(QString, member);
    QFETCH(QString, queuedMatrix);
    QFETCH(QString, code);

    Fixture *kept = patchDimmers(m_doc, 0, 1);
    Fixture *replacement = patchDimmers(m_doc, 1, 1);
    FixtureGroup *group = new FixtureGroup(m_doc);
    group->setName("Existing");
    group->setSize(QSize(1, 1));
    QVERIFY(group->assignFixture(kept->id(), QLCPoint(0, 0)));
    QVERIFY(m_doc->addFixtureGroup(group));

    RGBMatrix *matrix = queuedMatrix == "hue" ? new HUEMatrix(m_doc) : new RGBMatrix(m_doc);
    matrix->setFixtureGroup(group->id());
    QVERIFY(m_doc->addFunction(matrix));
    if (!queuedMatrix.isEmpty())
        matrix->start(m_doc->masterTimer(), FunctionParent::master());
    m_doc->resetModified();
    const int groupCount = m_doc->fixtureGroups().size();

    Json ids = {replacement->id(), kept->id()};
    if (member == "text") ids = {replacement->id(), "x"};
    if (member == "unknown") ids = {replacement->id(), 4242};
    Json res = call("create_fixture_groups", {{"items", {{{"name", groupName.toStdString()},
                                                          {"fixtureIDs", ids}, {"columns", 2}}}}});

    QVERIFY2(res.is_array() && res.size() == 1, res.dump().c_str());
    QCOMPARE(res[0].value("index", -1), 0);
    QVERIFY2(QString::fromStdString(res[0].value("code", "")) == code, res.dump().c_str());
    if (code.isEmpty())
    {
        QCOMPARE(res[0].value("outcome", ""), std::string("updated"));
        QCOMPARE(group->fixtureList(), QList<quint32>({ replacement->id(), kept->id() }));
        QCOMPARE(group->size(), QSize(2, 1));
        QVERIFY(m_doc->isModified());
        return;
    }
    QCOMPARE(group->fixtureList(), QList<quint32>({ kept->id() }));
    QCOMPARE(group->size(), QSize(1, 1));
    QCOMPARE(m_doc->fixtureGroups().size(), groupCount);
    QVERIFY(!m_doc->isModified());
}

void McpFunctionAuthoring_Test::createRgbMatrices_preflightAndAdmission_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<QString>("defect");
    QTest::addColumn<bool>("queueOther");
    QTest::addColumn<QString>("code");

    QTest::newRow("valid upsert keeps script properties") << "Existing" << "" << false << "";
    QTest::newRow("non-integer fixture group") << "Existing" << "group" << false << "invalid";
    QTest::newRow("non-string color") << "Existing" << "color" << false << "invalid";
    QTest::newRow("bad timing") << "Existing" << "timing" << false << "invalid";
    QTest::newRow("target queued") << "Existing" << "queued" << false << "running";
    QTest::newRow("create while another function queued") << "Fresh" << "" << true << "functions_running";
}

void McpFunctionAuthoring_Test::createRgbMatrices_preflightAndAdmission()
{
    QFETCH(QString, name);
    QFETCH(QString, defect);
    QFETCH(bool, queueOther);
    QFETCH(QString, code);

    RGBMatrix *existing = new RGBMatrix(m_doc);
    existing->setName("Existing");
    existing->setPath("Keep");
    existing->setProperty("orientation", "Vertical");
    existing->setFixtureGroup(7);
    QVERIFY(m_doc->addFunction(existing));
    if (defect == "queued")
        existing->start(m_doc->masterTimer(), FunctionParent::master());
    if (queueOther)
    {
        Scene *other = new Scene(m_doc);
        QVERIFY(m_doc->addFunction(other));
        other->start(m_doc->masterTimer(), FunctionParent::master());
    }
    m_doc->resetModified();
    const int functionCount = m_doc->functions().size();

    Json item = {{"name", name.toStdString()}, {"type", "RGBMatrix"}, {"path", "New"}, {"fixtureGroupID", 3}};
    if (defect == "group") item["fixtureGroupID"] = "3";
    if (defect == "color") item["colors"] = {"#ff0000", 5};
    if (defect == "timing") item["duration"] = "soon";

    Json res = call("create_rgb_matrices", {{"items", {item}}});

    QVERIFY2(res.is_array() && res.size() == 1, res.dump().c_str());
    QCOMPARE(res[0].value("index", -1), 0);
    QVERIFY2(QString::fromStdString(res[0].value("code", "")) == code, res.dump().c_str());
    if (code.isEmpty())
    {
        QCOMPARE(existing->path(true), QStringLiteral("New"));
        QCOMPARE(existing->fixtureGroup(), quint32(3));
        QCOMPARE(existing->property("orientation"), QStringLiteral("Vertical"));
        return;
    }
    QCOMPARE(existing->path(true), QStringLiteral("Keep"));
    QCOMPARE(existing->fixtureGroup(), quint32(7));
    QCOMPARE(m_doc->functions().size(), functionCount);
    QVERIFY(!m_doc->isModified());
}

void McpFunctionAuthoring_Test::queryFunctionDetails_targets_data()
{
    QTest::addColumn<QString>("selector");
    QTest::addColumn<QString>("code");
    QTest::addColumn<QString>("type");

    QTest::newRow("by id") << "id:Look" << "" << "Scene";
    QTest::newRow("unique name") << "name:Look" << "" << "Scene";
    QTest::newRow("ambiguous name") << "name:Twin" << "ambiguous" << "";
    QTest::newRow("name narrowed by type") << "name:Twin,type:Chaser" << "" << "Chaser";
    QTest::newRow("unknown id") << "id:4242" << "not_found" << "";
    QTest::newRow("unknown name") << "name:Nope" << "not_found" << "";
    QTest::newRow("id and name") << "both" << "invalid" << "";
    QTest::newRow("empty selector") << "empty" << "invalid" << "";
    QTest::newRow("matrix refers to its tool") << "name:Mat" << "" << "RGBMatrix";
}

void McpFunctionAuthoring_Test::queryFunctionDetails_targets()
{
    QFETCH(QString, selector);
    QFETCH(QString, code);
    QFETCH(QString, type);

    Scene *look = new Scene(m_doc);
    look->setName("Look");
    QVERIFY(m_doc->addFunction(look));
    Scene *twinScene = new Scene(m_doc);
    twinScene->setName("Twin");
    QVERIFY(m_doc->addFunction(twinScene));
    Chaser *twinChaser = new Chaser(m_doc);
    twinChaser->setName("Twin");
    QVERIFY(m_doc->addFunction(twinChaser));
    RGBMatrix *mat = new RGBMatrix(m_doc);
    mat->setName("Mat");
    QVERIFY(m_doc->addFunction(mat));

    Json target = Json::object();
    if (selector == "both")
        target = {{"id", look->id()}, {"name", "Look"}};
    else if (selector == "id:Look")
        target = {{"id", look->id()}};
    else if (selector.startsWith("id:"))
        target = {{"id", selector.mid(3).toInt()}};
    else if (selector.startsWith("name:"))
    {
        const QStringList parts = selector.split(',');
        target["name"] = parts[0].mid(5).toStdString();
        if (parts.size() > 1)
            target["type"] = parts[1].mid(5).toStdString();
    }

    // The probed target sits between two valid ones: indices follow the targets list
    Json res = call("query_function_details",
                    {{"targets", Json::array({{{"id", look->id()}}, target, {{"name", "Look"}}})}})["items"];

    QVERIFY2(res.is_array() && res.size() == 3, res.dump().c_str());
    for (int i = 0; i < 3; ++i)
        QCOMPARE(res[i].value("index", -1), i);
    QCOMPARE(res[0].value("status", std::string()), std::string("ok"));
    QCOMPARE(res[2].value("status", std::string()), std::string("ok"));
    const Json &probed = res[1];
    QVERIFY2(QString::fromStdString(probed.value("code", "")) == code, probed.dump().c_str());
    QCOMPARE(probed.value("status", std::string()), std::string(code.isEmpty() ? "ok" : "error"));
    if (!code.isEmpty())
        return;
    QCOMPARE(QString::fromStdString(probed.value("type", "")), type);
    if (type == "RGBMatrix")
        QCOMPARE(probed.value("detailTool", std::string()), std::string("query_rgb_matrices"));
}

void McpFunctionAuthoring_Test::queryFunctionDetails_perType_data()
{
    QTest::addColumn<QString>("kind");
    QTest::addColumn<QString>("expected");

    QTest::newRow("scene, ms timing, default fade out") << "scene" << R"({"type":"Scene","path":"Looks",
        "tempoType":"time","fadeIn":{"unit":"ms","value":300},"fadeOut":{"unit":"default"},
        "values":[{"fixtureID":0,"channel":0,"value":10},{"fixtureID":0,"channel":1,"value":255}],
        "paletteIDs":[],"valuesAuthoritative":true})";
    QTest::newRow("scene, beats timing") << "beatScene" << R"({"tempoType":"beats",
        "fadeIn":{"unit":"beats","value":1.5},"fadeOut":{"unit":"beats","value":0}})";
    QTest::newRow("chaser steps and modes") << "chaser" << R"({"type":"Chaser","runOrder":"pingpong",
        "direction":"backward","fadeInMode":"common","fadeOutMode":"default","durationMode":"default",
        "steps":[{"functionID":0,"fadeIn":{"unit":"ms","value":100},"hold":{"unit":"infinite"},
                  "fadeOut":{"unit":"ms","value":200},"note":"cue"}]})";
    QTest::newRow("sequence binds scene, bound values not authoritative") << "sequence" << R"({"type":"Sequence",
        "boundSceneID":0,"runOrder":"loop","direction":"forward",
        "steps":[{"fadeIn":{"unit":"ms","value":0},"hold":{"unit":"ms","value":500},"fadeOut":{"unit":"ms","value":0},
                  "note":"","values":[{"fixtureID":0,"channel":0,"value":1},{"fixtureID":0,"channel":1,"value":2}]}],
        "boundScene":{"valuesAuthoritative":false}})";
    // a 2-channel generic dimmer has no pan/tilt, so EFXFixture falls back to Dimmer mode
    QTest::newRow("efx shape and fixtures") << "efx" << R"({"type":"EFX","algorithm":"Lissajous",
        "width":100,"height":50,"rotation":90,"xOffset":10,"yOffset":20,"xFrequency":3,"yFrequency":4,
        "xPhase":45,"yPhase":180,"startOffset":30,"isRelative":true,"propagationMode":"Serial",
        "runOrder":"single","direction":"forward",
        "fixtures":[{"fixtureID":0,"head":0,"direction":"backward","startOffset":15,"mode":"Dimmer"}]})";
    QTest::newRow("collection members") << "collection" << R"({"type":"Collection","functionIDs":[0]})";
    QTest::newRow("script content") << "script" << R"({"type":"Script","content":"Engine.waitTime(10);"})";
}

void McpFunctionAuthoring_Test::queryFunctionDetails_perType()
{
    QFETCH(QString, kind);
    QFETCH(QString, expected);

    Fixture *fxi = patchDimmers(m_doc, 0, 2);
    QCOMPARE(fxi->id(), quint32(0));
    Scene *scene = new Scene(m_doc);
    scene->setValue(SceneValue(0, 1, 255));
    scene->setValue(SceneValue(0, 0, 10));
    scene->setPath("Looks");
    scene->setFadeInSpeed(300);
    scene->setFadeOutSpeed(Function::defaultSpeed());
    QVERIFY(m_doc->addFunction(scene));
    QCOMPARE(scene->id(), quint32(0));

    Function *target = scene;
    if (kind == "beatScene")
    {
        scene->setTempoType(Function::Beats);
        scene->setFadeInSpeed(1500);
        scene->setFadeOutSpeed(0);
    }
    else if (kind == "chaser")
    {
        Chaser *chaser = new Chaser(m_doc);
        chaser->setRunOrder(Function::PingPong);
        chaser->setDirection(Function::Backward);
        chaser->setFadeInMode(Chaser::Common);
        chaser->setDurationMode(Chaser::Default);
        chaser->setFadeOutMode(Chaser::Default);
        ChaserStep step(0, 100, Function::infiniteSpeed(), 200);
        step.note = "cue";
        QVERIFY(chaser->addStep(step));
        QVERIFY(m_doc->addFunction(chaser));
        target = chaser;
    }
    else if (kind == "sequence")
    {
        Sequence *seq = new Sequence(m_doc);
        seq->setBoundSceneID(0);
        ChaserStep step(0, 0, 500, 0);
        step.values << SceneValue(0, 0, 1) << SceneValue(0, 1, 2);
        QVERIFY(seq->addStep(step));
        QVERIFY(m_doc->addFunction(seq));
        target = seq;
    }
    else if (kind == "efx")
    {
        EFX *efx = new EFX(m_doc);
        efx->setAlgorithm(EFX::Lissajous);
        efx->setWidth(100);
        efx->setHeight(50);
        efx->setRotation(90);
        efx->setXOffset(10);
        efx->setYOffset(20);
        efx->setXFrequency(3);
        efx->setYFrequency(4);
        efx->setXPhase(45);
        efx->setYPhase(180);
        efx->setStartOffset(30);
        efx->setIsRelative(true);
        efx->setPropagationMode(EFX::Serial);
        efx->setRunOrder(Function::SingleShot);
        EFXFixture *ef = new EFXFixture(efx);
        ef->setHead(GroupHead(0, 0));
        ef->setDirection(Function::Backward);
        ef->setStartOffset(15);
        QVERIFY(efx->addFixture(ef));
        QVERIFY(m_doc->addFunction(efx));
        target = efx;
    }
    else if (kind == "collection")
    {
        Collection *col = new Collection(m_doc);
        QVERIFY(col->addFunction(0));
        QVERIFY(m_doc->addFunction(col));
        target = col;
    }
    else if (kind == "script")
    {
        Script *script = new Script(m_doc);
        script->setData("Engine.waitTime(10);");
        QVERIFY(m_doc->addFunction(script));
        target = script;
    }

    Json res = call("query_function_details", {{"targets", Json::array({{{"id", target->id()}}})}})["items"];
    QVERIFY2(res.is_array() && res.size() == 1 && res[0].value("status", "") == "ok", res.dump().c_str());
    Json detail = res[0];
    if (kind == "sequence")
    {
        Json scene = call("query_function_details", {{"targets", Json::array({{{"id", 0}}})}})["items"];
        detail["boundScene"] = {{"valuesAuthoritative", scene[0].value("valuesAuthoritative", true)}};
    }
    const Json want = Json::parse(expected.toStdString());
    for (auto it = want.begin(); it != want.end(); ++it)
        QVERIFY2(detail.contains(it.key()) && detail.at(it.key()) == it.value(),
                 (it.key() + " in " + detail.dump()).c_str());
}

void McpFunctionAuthoring_Test::updateFunctions_common_data()
{
    QTest::addColumn<QString>("target");
    QTest::addColumn<QString>("fields");
    QTest::addColumn<QString>("state");
    QTest::addColumn<QString>("code");
    QTest::addColumn<QString>("readback");

    QTest::newRow("rename, path and ms fade") << R"({"id":0})"
        << R"({"name":"Warm","path":"Looks/Warm","fadeIn":{"unit":"ms","value":250}})" << "idle" << ""
        << R"({"name":"Warm","path":"Looks/Warm","fadeIn":{"unit":"ms","value":250},"fadeOut":{"unit":"ms","value":0}})";
    QTest::newRow("sentinels by unique name") << R"({"name":"Run"})"
        << R"({"duration":{"unit":"infinite"},"fadeOut":{"unit":"default"}})" << "idle" << ""
        << R"({"duration":{"unit":"infinite"},"fadeOut":{"unit":"default"}})";
    QTest::newRow("switch to beats with beat fade") << R"({"id":0})"
        << R"({"tempoType":"beats","fadeIn":{"unit":"beats","value":0.5}})" << "idle" << ""
        << R"({"tempoType":"beats","fadeIn":{"unit":"beats","value":0.5}})";
    QTest::newRow("beat unit on time function") << R"({"id":0})"
        << R"({"name":"Lost","fadeIn":{"unit":"beats","value":2}})" << "idle" << "invalid" << "";
    QTest::newRow("negative ms") << R"({"id":0})" << R"({"fadeIn":{"unit":"ms","value":-1}})" << "idle" << "invalid" << "";
    QTest::newRow("value on sentinel") << R"({"id":0})" << R"({"fadeIn":{"unit":"default","value":3}})" << "idle" << "invalid" << "";
    QTest::newRow("unknown field") << R"({"id":0})" << R"({"name":"Lost","colour":"red"})" << "idle" << "invalid" << "";
    QTest::newRow("no fields") << R"({"id":0})" << R"({})" << "idle" << "invalid" << "";
    QTest::newRow("target queued") << R"({"id":0})" << R"({"name":"Lost"})" << "targetQueued" << "running" << "";
    QTest::newRow("referrer queued") << R"({"id":0})" << R"({"name":"Lost"})" << "referrerQueued" << "running" << "";
    QTest::newRow("ambiguous name") << R"({"name":"Twin"})" << R"({"name":"Lost"})" << "idle" << "ambiguous" << "";
    QTest::newRow("matrix uses its own tool") << R"({"name":"Mat"})" << R"({"name":"Lost"})" << "idle" << "unsupported" << "";
}

void McpFunctionAuthoring_Test::updateFunctions_common()
{
    QFETCH(QString, target);
    QFETCH(QString, fields);
    QFETCH(QString, state);
    QFETCH(QString, code);
    QFETCH(QString, readback);

    Scene *scene = new Scene(m_doc);
    scene->setName("Look");
    QVERIFY(m_doc->addFunction(scene));
    QCOMPARE(scene->id(), quint32(0));
    Chaser *run = new Chaser(m_doc);
    run->setName("Run");
    QVERIFY(run->addStep(ChaserStep(0)));
    QVERIFY(m_doc->addFunction(run));
    for (int i = 0; i < 2; ++i)
    {
        Scene *twin = new Scene(m_doc);
        twin->setName("Twin");
        QVERIFY(m_doc->addFunction(twin));
    }
    RGBMatrix *mat = new RGBMatrix(m_doc);
    mat->setName("Mat");
    QVERIFY(m_doc->addFunction(mat));
    if (state == "targetQueued")
        scene->start(m_doc->masterTimer(), FunctionParent::master());
    if (state == "referrerQueued")
        run->start(m_doc->masterTimer(), FunctionParent::master());

    const Json tgt = Json::parse(target.toStdString());
    Function *resolved = tgt.contains("id") ? m_doc->function(tgt.at("id").get<quint32>()) : nullptr;
    const Json before = resolved ? call("query_function_details", {{"targets", Json::array({tgt})}})["items"][0] : Json();
    m_doc->resetModified();

    Json item = Json::parse(fields.toStdString());
    item["target"] = tgt;
    // The probed item sits after an invalid one: items are independent and indexed
    Json res = call("update_functions", {{"items", Json::array({Json{{"target", {{"id", 4242}}}, {"name", "x"}}, item})}})["items"];

    QVERIFY2(res.is_array() && res.size() == 2, res.dump().c_str());
    QCOMPARE(res[0].value("code", std::string()), std::string("not_found"));
    QCOMPARE(res[1].value("index", -1), 1);
    QVERIFY2(QString::fromStdString(res[1].value("code", "")) == code, res.dump().c_str());
    QCOMPARE(res[1].value("status", std::string()), std::string(code.isEmpty() ? "ok" : "error"));
    QCOMPARE(m_doc->isModified(), code.isEmpty());
    if (!code.isEmpty())
    {
        if (resolved)
            QCOMPARE(call("query_function_details", {{"targets", Json::array({tgt})}})["items"][0], before);
        return;
    }
    QCOMPARE(res[1].value("outcome", std::string()), std::string("updated"));
    Json detail = call("query_function_details", {{"targets", Json::array({{{"id", res[1].value("id", -1)}}})}})["items"][0];
    const Json want = Json::parse(readback.toStdString());
    for (auto it = want.begin(); it != want.end(); ++it)
        QVERIFY2(detail.at(it.key()) == it.value(), (it.key() + " in " + detail.dump()).c_str());
}

void McpFunctionAuthoring_Test::updateFunctions_perType_data()
{
    QTest::addColumn<QString>("target");
    QTest::addColumn<QString>("fields");
    QTest::addColumn<QString>("code");
    QTest::addColumn<QString>("readback");

    QTest::newRow("scene values replaced") << "Look" << R"({"values":[{"fixtureID":0,"channel":1,"value":200}]})"
        << "" << R"({"values":[{"fixtureID":0,"channel":1,"value":200}]})";
    QTest::newRow("scene value above 255") << "Look" << R"({"values":[{"fixtureID":0,"channel":0,"value":256}]})" << "invalid" << "";
    QTest::newRow("scene unknown fixture") << "Look" << R"({"values":[{"fixtureID":9,"channel":0,"value":1}]})" << "invalid" << "";
    QTest::newRow("scene channel beyond fixture") << "Look" << R"({"values":[{"fixtureID":0,"channel":2,"value":1}]})" << "invalid" << "";
    QTest::newRow("scene duplicate channel") << "Look"
        << R"({"values":[{"fixtureID":0,"channel":0,"value":1},{"fixtureID":0,"channel":0,"value":2}]})" << "invalid" << "";
    QTest::newRow("scene unknown palette") << "Look" << R"({"paletteIDs":[77]})" << "invalid" << "";
    QTest::newRow("bound scene channel set change") << "Bound" << R"({"values":[{"fixtureID":0,"channel":0,"value":5}]})" << "bound_scene" << "";
    QTest::newRow("bound scene same channel set") << "Bound"
        << R"({"values":[{"fixtureID":0,"channel":0,"value":5},{"fixtureID":0,"channel":1,"value":6}]})"
        << "" << R"({"valuesAuthoritative":false,"values":[{"fixtureID":0,"channel":0,"value":5},{"fixtureID":0,"channel":1,"value":6}]})";
    QTest::newRow("chaser order, modes and steps") << "Run"
        << R"({"runOrder":"random","direction":"backward","fadeInMode":"common",
               "steps":[{"functionID":0,"hold":{"unit":"ms","value":100},"note":"a"},{"functionID":2,"fadeIn":{"unit":"default"}}]})"
        << "" << R"({"runOrder":"random","direction":"backward","fadeInMode":"common",
               "steps":[{"functionID":0,"fadeIn":{"unit":"ms","value":0},"hold":{"unit":"ms","value":100},"fadeOut":{"unit":"ms","value":0},"note":"a"},
                        {"functionID":2,"fadeIn":{"unit":"default"},"hold":{"unit":"ms","value":0},"fadeOut":{"unit":"ms","value":0},"note":""}]})";
    QTest::newRow("chaser step unknown function") << "Run" << R"({"steps":[{"functionID":4242}]})" << "invalid" << "";
    QTest::newRow("chaser step is itself") << "Run" << R"({"steps":[{"functionID":1}]})" << "invalid" << "";
    QTest::newRow("chaser bad run order") << "Run" << R"({"runOrder":"sideways"})" << "invalid" << "";
    QTest::newRow("efx shape") << "Fx" << R"({"algorithm":"Eight","width":50,"isRelative":true,"propagationMode":"Asymmetric"})"
        << "" << R"({"algorithm":"Eight","width":50,"isRelative":true,"propagationMode":"Asymmetric"})";
    QTest::newRow("efx width out of range") << "Fx" << R"({"width":200})" << "invalid" << "";
    QTest::newRow("efx fixtures replaced") << "Fx"
        << R"({"fixtures":[{"fixtureID":0,"head":1,"direction":"backward","startOffset":90,"mode":"Dimmer"},{"fixtureID":0,"head":0,"direction":"forward","startOffset":0,"mode":"Dimmer"}]})"
        << "" << R"({"fixtures":[{"fixtureID":0,"head":1,"direction":"backward","startOffset":90,"mode":"Dimmer"},{"fixtureID":0,"head":0,"direction":"forward","startOffset":0,"mode":"Dimmer"}]})";
    QTest::newRow("efx fixtures cleared") << "Fx" << R"({"fixtures":[]})" << "" << R"({"fixtures":[]})";
    QTest::newRow("efx fixture unknown") << "Fx" << R"({"fixtures":[{"fixtureID":9,"head":0,"direction":"forward","startOffset":0,"mode":"Dimmer"}]})" << "invalid" << "";
    QTest::newRow("efx fixture head beyond fixture") << "Fx" << R"({"fixtures":[{"fixtureID":0,"head":2,"direction":"forward","startOffset":0,"mode":"Dimmer"}]})" << "invalid" << "";
    QTest::newRow("efx fixture mode unsupported") << "Fx" << R"({"fixtures":[{"fixtureID":0,"head":0,"direction":"forward","startOffset":0,"mode":"Position"}]})" << "invalid" << "";
    QTest::newRow("efx fixture offset 360") << "Fx" << R"({"fixtures":[{"fixtureID":0,"head":0,"direction":"forward","startOffset":360,"mode":"Dimmer"}]})" << "invalid" << "";
    QTest::newRow("efx fixture direction") << "Fx" << R"({"fixtures":[{"fixtureID":0,"head":0,"direction":"up","startOffset":0,"mode":"Dimmer"}]})" << "invalid" << "";
    QTest::newRow("efx fixture missing mode") << "Fx" << R"({"fixtures":[{"fixtureID":0,"head":0,"direction":"forward","startOffset":0}]})" << "invalid" << "";
    QTest::newRow("efx fixture duplicate") << "Fx"
        << R"({"fixtures":[{"fixtureID":0,"head":0,"direction":"forward","startOffset":0,"mode":"Dimmer"},{"fixtureID":0,"head":0,"direction":"backward","startOffset":5,"mode":"Dimmer"}]})" << "invalid" << "";
    QTest::newRow("collection members") << "Col" << R"({"functionIDs":[0,1]})" << "" << R"({"functionIDs":[0,1]})";
    QTest::newRow("collection contains itself") << "Col" << R"({"functionIDs":[5]})" << "invalid" << "";
    QTest::newRow("script content") << "Scr" << R"({"content":"Engine.waitTime(5);"})" << "" << R"({"content":"Engine.waitTime(5);"})";
    QTest::newRow("script syntax error") << "Scr" << R"({"content":"var = ;"})" << "invalid" << "";
    QTest::newRow("script runtime throw is authored, not run") << "Scr" << R"({"content":"throw new Error('x');"})"
        << "" << R"({"content":"throw new Error('x');"})";
    QTest::newRow("script nonterminating is authored, not run") << "Scr" << R"({"content":"while (true) {}"})"
        << "" << R"({"content":"while (true) {}"})";
    QTest::newRow("script closing its wrapper") << "Scr" << R"({"content":"}); Engine.setBPM(1); (function() {"})" << "invalid" << "";
    QTest::newRow("field of another type") << "Look" << R"({"algorithm":"Eight"})" << "invalid" << "";
}

void McpFunctionAuthoring_Test::updateFunctions_perType()
{
    QFETCH(QString, target);
    QFETCH(QString, fields);
    QFETCH(QString, code);
    QFETCH(QString, readback);

    QCOMPARE(patchDimmers(m_doc, 0, 2)->id(), quint32(0));
    Scene *look = new Scene(m_doc);
    look->setName("Look");
    look->setValue(SceneValue(0, 0, 10));
    look->setValue(SceneValue(0, 1, 20));
    QVERIFY(m_doc->addFunction(look));                              // 0
    Chaser *run = new Chaser(m_doc);
    run->setName("Run");
    QVERIFY(run->addStep(ChaserStep(0)));
    QVERIFY(m_doc->addFunction(run));                               // 1
    Scene *bound = new Scene(m_doc);
    bound->setName("Bound");
    bound->setValue(SceneValue(0, 0, 1));
    bound->setValue(SceneValue(0, 1, 2));
    QVERIFY(m_doc->addFunction(bound));                             // 2
    Sequence *seq = new Sequence(m_doc);
    seq->setBoundSceneID(bound->id());
    QVERIFY(m_doc->addFunction(seq));                               // 3
    EFX *fx = new EFX(m_doc);
    fx->setName("Fx");
    EFXFixture *ef = new EFXFixture(fx);
    ef->setHead(GroupHead(0, 1));
    ef->setMode(EFXFixture::Dimmer);
    QVERIFY(fx->addFixture(ef));
    QVERIFY(m_doc->addFunction(fx));                                // 4
    Collection *col = new Collection(m_doc);
    col->setName("Col");
    QVERIFY(m_doc->addFunction(col));                               // 5
    QCOMPARE(col->id(), quint32(5));
    Script *scr = new Script(m_doc);
    scr->setName("Scr");
    scr->setData("Engine.waitTime(1);");
    QVERIFY(m_doc->addFunction(scr));                               // 6

    const Json tgt = {{"name", target.toStdString()}};
    const Json before = call("query_function_details", {{"targets", Json::array({tgt})}})["items"][0];
    m_doc->resetModified();

    Json item = Json::parse(fields.toStdString());
    item["target"] = tgt;
    Json res = call("update_functions", {{"items", Json::array({item})}})["items"];

    QVERIFY2(res.is_array() && res.size() == 1, res.dump().c_str());
    QVERIFY2(QString::fromStdString(res[0].value("code", "")) == code, res.dump().c_str());
    QCOMPARE(m_doc->isModified(), code.isEmpty());
    const Json after = call("query_function_details", {{"targets", Json::array({tgt})}})["items"][0];
    if (!code.isEmpty())
    {
        QCOMPARE(after, before);
        return;
    }
    const Json want = Json::parse(readback.toStdString());
    for (auto it = want.begin(); it != want.end(); ++it)
        QVERIFY2(after.at(it.key()) == it.value(), (it.key() + " in " + after.dump()).c_str());
}

void McpFunctionAuthoring_Test::updateFunctions_extendedFields_data()
{
    QTest::addColumn<QString>("target");
    QTest::addColumn<QString>("fields");
    QTest::addColumn<QString>("code");
    QTest::addColumn<QString>("readback");

    QTest::newRow("scene hidden, additive") << "Look" << R"({"visible":false,"blendMode":"Additive"})"
        << "" << R"({"visible":false,"blendMode":"Additive"})";
    QTest::newRow("chaser hidden keeps blend") << "Run" << R"({"visible":false})"
        << "" << R"({"visible":false,"blendMode":"Normal"})";
    QTest::newRow("efx subtractive") << "Fx" << R"({"blendMode":"Subtractive"})"
        << "" << R"({"visible":true,"blendMode":"Subtractive"})";
    QTest::newRow("unknown blend mode") << "Look" << R"({"blendMode":"Screen"})" << "invalid" << "";
    QTest::newRow("visible not boolean") << "Look" << R"({"visible":"no"})" << "invalid" << "";
    QTest::newRow("scene groups replaced") << "Look"
        << R"({"fixtureGroupIDs":[1],"channelGroups":[{"id":1,"level":200},{"id":0,"level":0}]})"
        << "" << R"({"fixtureGroupIDs":[1],"channelGroups":[{"id":1,"level":200},{"id":0,"level":0}]})";
    QTest::newRow("scene groups cleared") << "Look" << R"({"fixtureGroupIDs":[],"channelGroups":[]})"
        << "" << R"({"fixtureGroupIDs":[],"channelGroups":[]})";
    QTest::newRow("scene groups kept when omitted") << "Look" << R"({"name":"Renamed"})"
        << "" << R"({"fixtureGroupIDs":[0],"channelGroups":[{"id":0,"level":7}]})";
    QTest::newRow("unknown fixture group") << "Look" << R"({"fixtureGroupIDs":[9]})" << "invalid" << "";
    QTest::newRow("repeated fixture group") << "Look" << R"({"fixtureGroupIDs":[1,1]})" << "invalid" << "";
    QTest::newRow("unknown channel group") << "Look" << R"({"channelGroups":[{"id":9,"level":1}]})" << "invalid" << "";
    QTest::newRow("channel group level 256") << "Look" << R"({"channelGroups":[{"id":1,"level":256}]})" << "invalid" << "";
    QTest::newRow("channel group without level") << "Look" << R"({"channelGroups":[{"id":1}]})" << "invalid" << "";
    QTest::newRow("repeated channel group") << "Look"
        << R"({"channelGroups":[{"id":1,"level":1},{"id":1,"level":2}]})" << "invalid" << "";
    QTest::newRow("efx dimmer control on") << "Fx" << R"({"dimmerControl":true})" << "" << R"({"dimmerControl":true})";
    QTest::newRow("efx dimmer control not boolean") << "Fx" << R"({"dimmerControl":1})" << "invalid" << "";
    QTest::newRow("dimmer control on a scene") << "Look" << R"({"dimmerControl":true})" << "invalid" << "";
}

void McpFunctionAuthoring_Test::updateFunctions_extendedFields()
{
    QFETCH(QString, target);
    QFETCH(QString, fields);
    QFETCH(QString, code);
    QFETCH(QString, readback);

    // Fixture 0, fixture groups 0/1 and channel groups 0/1 exist in both Docs
    auto populate = [](Doc *doc) {
        patchDimmers(doc, 0, 2);
        for (int i = 0; i < 2; ++i)
        {
            QVERIFY(doc->addFixtureGroup(new FixtureGroup(doc)));
            QVERIFY(doc->addChannelsGroup(new ChannelsGroup(doc)));
        }
    };
    populate(m_doc);
    Scene *look = new Scene(m_doc);
    look->setName("Look");
    look->setValue(SceneValue(0, 0, 10));
    look->addFixtureGroup(0);
    look->addChannelGroup(0);
    look->setChannelGroupLevel(0, 7);
    QVERIFY(m_doc->addFunction(look));
    Chaser *run = new Chaser(m_doc);
    run->setName("Run");
    QVERIFY(m_doc->addFunction(run));
    EFX *fx = new EFX(m_doc);
    fx->setName("Fx");
    QVERIFY(m_doc->addFunction(fx));

    const Json tgt = {{"name", target.toStdString()}};
    const Json before = call("query_function_details", {{"targets", Json::array({tgt})}})["items"][0];
    m_doc->resetModified();

    Json item = Json::parse(fields.toStdString());
    item["target"] = tgt;
    const Json res = call("update_functions", {{"items", Json::array({item})}})["items"];
    QVERIFY2(res.is_array() && res.size() == 1, res.dump().c_str());
    QVERIFY2(QString::fromStdString(res[0].value("code", "")) == code, res.dump().c_str());
    QCOMPARE(m_doc->isModified(), code.isEmpty());
    const Json after = call("query_function_details", {{"targets", Json::array({{{"id", before.at("id")}}})}})["items"][0];
    if (!code.isEmpty())
    {
        QCOMPARE(after, before);
        return;
    }

    // The edit survives save and reload into a fresh Doc, read back through the same tool
    Function *edited = m_doc->function(before.at("id").get<quint32>());
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly);
    QXmlStreamWriter writer(&buffer);
    writer.writeStartElement("Functions");
    QVERIFY(edited->saveXML(&writer));
    writer.writeEndElement();
    buffer.close();
    Doc fresh(this);
    populate(&fresh);
    QXmlStreamReader reader(buffer.data());
    reader.readNextStartElement();
    while (reader.readNextStartElement())
        QVERIFY(Function::loader(reader, &fresh));
    fastmcpp::tools::ToolManager freshTools;
    registerFunctionTools(freshTools, &fresh);
    Json reloaded = freshTools.invoke("query_function_details", {{"targets", Json::array({{{"id", edited->id()}}})}});
    if (reloaded.is_string())
        reloaded = Json::parse(reloaded.get<std::string>());

    const Json want = Json::parse(readback.toStdString());
    for (auto it = want.begin(); it != want.end(); ++it)
    {
        QVERIFY2(after.contains(it.key()) && after.at(it.key()) == it.value(), (it.key() + " in " + after.dump()).c_str());
        QVERIFY2(reloaded["items"][0].value(it.key(), Json()) == it.value(), (it.key() + " reloaded " + reloaded.dump()).c_str());
    }
}

void McpFunctionAuthoring_Test::updateFunctions_sceneMembership_data()
{
    QTest::addColumn<QString>("fields");
    QTest::addColumn<QString>("code");
    QTest::addColumn<QString>("readback");

    QTest::newRow("detail only") << "" << ""
        << R"({"fixtureIDs":[0,1],"paletteIDs":[0],"values":[]})";
    QTest::newRow("rename keeps palette-only members") << R"({"name":"Renamed"})" << ""
        << R"({"name":"Renamed","fixtureIDs":[0,1],"paletteIDs":[0],"values":[]})";
    QTest::newRow("palettes cleared keep members") << R"({"paletteIDs":[]})" << ""
        << R"({"fixtureIDs":[0,1],"paletteIDs":[],"values":[]})";
    QTest::newRow("values keep palette-only member") << R"({"values":[{"fixtureID":1,"channel":0,"value":9}]})" << ""
        << R"({"fixtureIDs":[0,1],"paletteIDs":[0],"values":[{"fixtureID":1,"channel":0,"value":9}]})";
    QTest::newRow("members replaced") << R"({"fixtureIDs":[1]})" << ""
        << R"({"fixtureIDs":[1],"paletteIDs":[0],"values":[]})";
    QTest::newRow("members and values together") << R"({"fixtureIDs":[1],"values":[{"fixtureID":1,"channel":0,"value":3}]})" << ""
        << R"({"fixtureIDs":[1],"paletteIDs":[0],"values":[{"fixtureID":1,"channel":0,"value":3}]})";
    QTest::newRow("member unknown fixture") << R"({"fixtureIDs":[9]})" << "invalid" << "";
    QTest::newRow("member repeated") << R"({"fixtureIDs":[0,0]})" << "invalid" << "";
    QTest::newRow("valued fixture left out of members") << R"({"fixtureIDs":[1],"values":[{"fixtureID":0,"channel":0,"value":3}]})" << "invalid" << "";
}

void McpFunctionAuthoring_Test::updateFunctions_sceneMembership()
{
    QFETCH(QString, fields);
    QFETCH(QString, code);
    QFETCH(QString, readback);

    QCOMPARE(patchDimmers(m_doc, 0, 2)->id(), quint32(0));
    QCOMPARE(patchDimmers(m_doc, 2, 2)->id(), quint32(1));
    QLCPalette *palette = new QLCPalette(QLCPalette::Dimmer);
    palette->setValue(128);
    QVERIFY(m_doc->addPalette(palette, 0));
    Scene *scene = new Scene(m_doc);
    scene->setName("Look");
    scene->addFixture(0);
    scene->addFixture(1);
    scene->addPalette(0);
    QVERIFY(m_doc->addFunction(scene));
    m_doc->resetModified();

    const Json tgt = {{"id", scene->id()}};
    const Json before = call("query_function_details", {{"targets", Json::array({tgt})}})["items"][0];
    if (!fields.isEmpty())
    {
        Json item = Json::parse(fields.toStdString());
        item["target"] = tgt;
        const Json res = call("update_functions", {{"items", Json::array({item})}})["items"][0];
        QVERIFY2(QString::fromStdString(res.value("code", "")) == code, res.dump().c_str());
    }
    const Json after = call("query_function_details", {{"targets", Json::array({tgt})}})["items"][0];
    if (!code.isEmpty())
    {
        QVERIFY2(after == before, after.dump().c_str());
        QVERIFY(!m_doc->isModified());
        return;
    }
    const Json expected = Json::parse(readback.toStdString());
    QVERIFY(!expected.empty());
    for (const auto &[key, value] : expected.items())
        QVERIFY2(after.value(key, Json()) == value, (key + " in " + after.dump()).c_str());

    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly);
    QXmlStreamWriter writer(&buffer);
    QVERIFY(scene->saveXML(&writer));
    buffer.close();
    Doc fresh(this);
    patchDimmers(&fresh, 0, 2);
    patchDimmers(&fresh, 2, 2);
    QXmlStreamReader reader(buffer.data());
    reader.readNextStartElement();
    QVERIFY(Function::loader(reader, &fresh));
    Scene *loaded = qobject_cast<Scene*>(fresh.function(scene->id()));
    QVERIFY(loaded != nullptr);
    QCOMPARE(loaded->fixtures(), scene->fixtures());
    QCOMPARE(loaded->palettes(), scene->palettes());
    QCOMPARE(loaded->values(), scene->values());
}

void McpFunctionAuthoring_Test::updateFunctions_sequenceSteps_data()
{
    QTest::addColumn<bool>("boundIsScene");
    QTest::addColumn<QString>("steps");
    QTest::addColumn<QString>("code");
    QTest::addColumn<bool>("sequenceFirst");

    const char *aligned = R"([{"hold":{"unit":"ms","value":100},"note":"up","values":[{"fixtureID":0,"channel":1,"value":20},{"fixtureID":0,"channel":0,"value":10}]},
               {"fadeIn":{"unit":"infinite"},"values":[{"fixtureID":0,"channel":0,"value":0},{"fixtureID":0,"channel":1,"value":255}]}])";
    QTest::newRow("aligned, sequence saved first") << true << aligned << "" << true;
    QTest::newRow("aligned steps in any order") << true
        << R"([{"hold":{"unit":"ms","value":100},"note":"up","values":[{"fixtureID":0,"channel":1,"value":20},{"fixtureID":0,"channel":0,"value":10}]},
               {"fadeIn":{"unit":"infinite"},"values":[{"fixtureID":0,"channel":0,"value":0},{"fixtureID":0,"channel":1,"value":255}]}])" << "" << false;
    QTest::newRow("missing channel") << true << R"([{"values":[{"fixtureID":0,"channel":0,"value":10}]}])" << "invalid" << false;
    QTest::newRow("extra channel") << true
        << R"([{"values":[{"fixtureID":0,"channel":0,"value":1},{"fixtureID":0,"channel":1,"value":2},{"fixtureID":0,"channel":2,"value":3}]}])" << "invalid" << false;
    QTest::newRow("repeated channel") << true
        << R"([{"values":[{"fixtureID":0,"channel":0,"value":1},{"fixtureID":0,"channel":0,"value":2}]}])" << "invalid" << false;
    QTest::newRow("value above 255") << true
        << R"([{"values":[{"fixtureID":0,"channel":0,"value":1},{"fixtureID":0,"channel":1,"value":256}]}])" << "invalid" << false;
    QTest::newRow("step names a function") << true
        << R"([{"functionID":0,"values":[{"fixtureID":0,"channel":0,"value":1},{"fixtureID":0,"channel":1,"value":2}]}])" << "invalid" << false;
    QTest::newRow("step without values") << true << R"([{"hold":{"unit":"ms","value":5}}])" << "invalid" << false;
    QTest::newRow("bound function is not a Scene") << false
        << R"([{"values":[{"fixtureID":0,"channel":0,"value":1},{"fixtureID":0,"channel":1,"value":2}]}])" << "invalid" << false;
}

void McpFunctionAuthoring_Test::updateFunctions_sequenceSteps()
{
    QFETCH(bool, boundIsScene);
    QFETCH(QString, steps);
    QFETCH(QString, code);
    QFETCH(bool, sequenceFirst);

    QCOMPARE(patchDimmers(m_doc, 0, 3)->id(), quint32(0));
    Scene *bound = new Scene(m_doc);
    bound->setName("Bound");
    bound->setValue(SceneValue(0, 1, 2));
    bound->setValue(SceneValue(0, 0, 1));
    QVERIFY(m_doc->addFunction(bound));
    Chaser *notScene = new Chaser(m_doc);
    QVERIFY(m_doc->addFunction(notScene));
    Sequence *seq = new Sequence(m_doc);
    seq->setName("Seq");
    seq->setBoundSceneID(boundIsScene ? bound->id() : notScene->id());
    QVERIFY(m_doc->addFunction(seq));

    const Json tgt = {{"id", seq->id()}};
    const Json before = call("query_function_details", {{"targets", Json::array({tgt})}})["items"][0];
    m_doc->resetModified();

    Json res = call("update_functions", {{"items", Json::array({{{"target", tgt}, {"steps", Json::parse(steps.toStdString())}}})}})["items"];
    QVERIFY2(res.is_array() && res.size() == 1, res.dump().c_str());
    QVERIFY2(QString::fromStdString(res[0].value("code", "")) == code, res.dump().c_str());
    QCOMPARE(m_doc->isModified(), code.isEmpty());
    const Json after = call("query_function_details", {{"targets", Json::array({tgt})}})["items"][0];
    if (!code.isEmpty())
    {
        QCOMPARE(after, before);
        return;
    }

    const Json want = Json::parse(R"([
        {"fadeIn":{"unit":"ms","value":0},"hold":{"unit":"ms","value":100},"fadeOut":{"unit":"ms","value":0},"note":"up",
         "values":[{"fixtureID":0,"channel":0,"value":10},{"fixtureID":0,"channel":1,"value":20}]},
        {"fadeIn":{"unit":"infinite"},"hold":{"unit":"ms","value":0},"fadeOut":{"unit":"ms","value":0},"note":"",
         "values":[{"fixtureID":0,"channel":0,"value":0},{"fixtureID":0,"channel":1,"value":255}]}])");
    QVERIFY2(after.at("steps") == want, after.dump().c_str());
    for (const ChaserStep &step : seq->steps())
        QCOMPARE(step.fid, bound->id());

    // Persistence: the authored steps survive save and reload into a fresh Doc
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly);
    QXmlStreamWriter writer(&buffer);
    writer.writeStartElement("Functions");
    if (sequenceFirst)
        QVERIFY(seq->saveXML(&writer));
    QVERIFY(bound->saveXML(&writer));
    if (!sequenceFirst)
        QVERIFY(seq->saveXML(&writer));
    writer.writeEndElement();
    buffer.close();

    Doc fresh(this);
    patchDimmers(&fresh, 0, 3);
    QXmlStreamReader reader(buffer.data());
    reader.readNextStartElement();
    while (reader.readNextStartElement())
        QVERIFY(Function::loader(reader, &fresh));
    for (Function *f : fresh.functions())
        f->postLoad();
    Sequence *loaded = qobject_cast<Sequence*>(fresh.function(seq->id()));
    QVERIFY(loaded != nullptr);
    QCOMPARE(loaded->stepsCount(), 2);
    for (int i = 0; i < 2; ++i)
    {
        QCOMPARE(loaded->steps().at(i).values, seq->steps().at(i).values);
        QCOMPARE(loaded->steps().at(i).hold, seq->steps().at(i).hold);
        QCOMPARE(loaded->steps().at(i).fadeIn, seq->steps().at(i).fadeIn);
        QCOMPARE(loaded->steps().at(i).note, seq->steps().at(i).note);
    }
    QStringList lost;
    for (int i = 0; i < 2; ++i)
        for (int v = 0; v < 2; ++v)
            if (loaded->steps().at(i).values.at(v).value != seq->steps().at(i).values.at(v).value)
                lost << QString("step %1 channel %2").arg(i).arg(v);
    QCOMPARE(lost, QStringList());
}

void McpFunctionAuthoring_Test::functionReferences_containmentCycles_data()
{
    // 0 Leaf (Scene), 1 Outer (Chaser: 2), 2 Inner (Collection: 0), 3 LoopA (Chaser: 4) and
    // 4 LoopB (Collection: 3) already form a cycle, 5 Free (Collection), 6 Diamond (Collection: 1, 2)
    QTest::addColumn<QString>("tool");
    QTest::addColumn<QString>("args");
    QTest::addColumn<QString>("code");

    QTest::newRow("update: two-function cycle") << "update_functions"
        << R"({"items":[{"target":{"id":2},"functionIDs":[1]}]})" << "invalid";
    QTest::newRow("update: three-function cycle") << "update_functions"
        << R"({"items":[{"target":{"id":2},"functionIDs":[6]}]})" << "invalid";
    QTest::newRow("update: chaser step closes a cycle") << "update_functions"
        << R"({"items":[{"target":{"id":1},"steps":[{"functionID":6}]}]})" << "invalid";
    QTest::newRow("update: reference into an existing cycle") << "update_functions"
        << R"({"items":[{"target":{"id":5},"functionIDs":[3]}]})" << "invalid";
    QTest::newRow("update: diamond stays accepted") << "update_functions"
        << R"({"items":[{"target":{"id":5},"functionIDs":[1,2,6]}]})" << "";
    QTest::newRow("update: rename inside an existing cycle") << "update_functions"
        << R"({"items":[{"target":{"id":3},"name":"Renamed"}]})" << "";
    QTest::newRow("update: new list breaks an existing cycle") << "update_functions"
        << R"({"items":[{"target":{"id":3},"steps":[{"functionID":0}]}]})" << "";
    QTest::newRow("create collection over an existing cycle") << "create_collections"
        << R"({"items":[{"name":"Wrap","functionIDs":[4]}]})" << "invalid";
    QTest::newRow("upsert collection closes a cycle") << "create_collections"
        << R"({"items":[{"name":"Inner","functionIDs":[1]}]})" << "invalid";
    QTest::newRow("upsert chaser closes a cycle") << "create_chasers"
        << R"({"items":[{"name":"Outer","steps":[{"functionID":6}]}]})" << "invalid";
    QTest::newRow("create chaser over a diamond") << "create_chasers"
        << R"({"items":[{"name":"Fresh","steps":[{"functionID":6},{"functionID":1}]}]})" << "";
}

void McpFunctionAuthoring_Test::functionReferences_containmentCycles()
{
    QFETCH(QString, tool);
    QFETCH(QString, args);
    QFETCH(QString, code);

    Scene *leaf = new Scene(m_doc);
    leaf->setName("Leaf");
    QVERIFY(m_doc->addFunction(leaf));
    Chaser *outer = new Chaser(m_doc);
    outer->setName("Outer");
    QVERIFY(m_doc->addFunction(outer));
    Collection *inner = new Collection(m_doc);
    inner->setName("Inner");
    QVERIFY(inner->addFunction(0));
    QVERIFY(m_doc->addFunction(inner));
    QVERIFY(outer->addStep(ChaserStep(inner->id())));
    Chaser *loopA = new Chaser(m_doc);
    loopA->setName("LoopA");
    QVERIFY(m_doc->addFunction(loopA));
    Collection *loopB = new Collection(m_doc);
    loopB->setName("LoopB");
    QVERIFY(m_doc->addFunction(loopB));
    QVERIFY(loopA->addStep(ChaserStep(loopB->id())));
    QVERIFY(loopB->addFunction(loopA->id()));
    Collection *free = new Collection(m_doc);
    free->setName("Free");
    QVERIFY(m_doc->addFunction(free));
    Collection *diamond = new Collection(m_doc);
    diamond->setName("Diamond");
    QVERIFY(diamond->addFunction(outer->id()));
    QVERIFY(diamond->addFunction(inner->id()));
    QVERIFY(m_doc->addFunction(diamond));
    QCOMPARE(diamond->id(), quint32(6));

    const Json all = {{{"id", 0}}, {{"id", 1}}, {{"id", 2}}, {{"id", 3}}, {{"id", 4}}, {{"id", 5}}, {{"id", 6}}};
    const Json before = call("query_function_details", {{"targets", all}})["items"];
    const int countBefore = m_doc->functions().size();
    m_doc->resetModified();

    const Json raw = call(tool.toStdString(), Json::parse(args.toStdString()));
    const Json records = raw.is_object() ? raw.value("items", Json()) : raw;
    QVERIFY2(records.is_array() && records.size() == 1, raw.dump().c_str());
    QVERIFY2(QString::fromStdString(records[0].value("code", "")) == code, raw.dump().c_str());
    QCOMPARE(records[0].value("status", ""), std::string(code.isEmpty() ? "ok" : "error"));
    if (!code.isEmpty())
    {
        QVERIFY2(records[0].value("error", "").find("cycle") != std::string::npos ||
                 records[0].value("error", "").find("contain") != std::string::npos, raw.dump().c_str());
        QCOMPARE(call("query_function_details", {{"targets", all}})["items"], before);
        QCOMPARE(m_doc->functions().size(), countBefore);
        QVERIFY(!m_doc->isModified());
    }
}

void McpFunctionAuthoring_Test::transport_typedObjectOutput_data()
{
    QTest::addColumn<QString>("tool");
    QTest::addColumn<QString>("args");
    QTest::addColumn<bool>("isError");
    QTest::addColumn<QString>("firstStatus");

    QTest::newRow("detail found") << "query_function_details" << R"({"targets":[{"id":0}]})" << false << "ok";
    QTest::newRow("detail all failed") << "query_function_details" << R"({"targets":[{"id":7},{"id":8}]})" << true << "error";
    QTest::newRow("detail mixed") << "query_function_details" << R"({"targets":[{"id":0},{"id":7}]})" << false << "ok";
    QTest::newRow("detail request error") << "query_function_details" << R"({"targets":[]})" << true << "";
    QTest::newRow("update applied") << "update_functions" << R"({"items":[{"target":{"id":0},"name":"Renamed"}]})" << false << "ok";
    QTest::newRow("update all failed") << "update_functions" << R"({"items":[{"target":{"id":0},"bogus":1},{"target":{"id":7},"name":"X"}]})" << true << "error";
    QTest::newRow("update mixed") << "update_functions" << R"({"items":[{"target":{"id":0},"name":"Renamed"},{"target":{"id":7},"name":"X"}]})" << false << "ok";
    QTest::newRow("update request error") << "update_functions" << R"({"nope":1})" << true << "";
}

void McpFunctionAuthoring_Test::transport_typedObjectOutput()
{
    QFETCH(QString, tool);
    QFETCH(QString, args);
    QFETCH(bool, isError);
    QFETCH(QString, firstStatus);

    Scene *scene = new Scene(m_doc);
    scene->setName("Look");
    QVERIFY(m_doc->addFunction(scene));

    fastmcpp::server::Server server("qlcplus", "5.0.0");
    fastmcpp::resources::ResourceManager rm;
    fastmcpp::prompts::PromptManager pm;
    auto handler = mcp::withToolResultEncoding(
        fastmcpp::mcp::make_mcp_handler("qlcplus", "5.0.0", server, *m_tm, rm, pm));

    const Json list = handler({{"jsonrpc", "2.0"}, {"id", 1}, {"method", "tools/list"}});
    Json schema;
    for (const Json &t : list["result"]["tools"])
        if (t["name"] == tool.toStdString())
            schema = t.value("outputSchema", Json());
    QVERIFY2(schema.is_object(), list.dump().c_str());
    QCOMPARE(schema["type"], Json("object"));
    QCOMPARE(schema["properties"]["items"]["type"], Json("array"));
    const std::string dumped = schema.dump();
    for (const char *combinator : { "oneOf", "anyOf", "allOf", "x-fastmcp-wrap-result" })
        QVERIFY2(dumped.find(combinator) == std::string::npos, dumped.c_str());

    const Json response = handler({{"jsonrpc", "2.0"}, {"id", 2}, {"method", "tools/call"},
        {"params", {{"name", tool.toStdString()}, {"arguments", Json::parse(args.toStdString())}}}});
    const Json &result = response["result"];
    QVERIFY2(result["content"].is_array() && result["content"].size() == 1, response.dump().c_str());
    const Json payload = Json::parse(result["content"][0]["text"].get<std::string>());
    QVERIFY2(result.value("isError", !isError) == isError, response.dump().c_str());
    QVERIFY2(result.contains("structuredContent"), response.dump().c_str());
    QCOMPARE(result["structuredContent"], payload);
    QVERIFY2(payload.is_object(), payload.dump().c_str());
    if (firstStatus.isEmpty())
    {
        QVERIFY2(payload["error"].is_string(), payload.dump().c_str());
        return;
    }
    QVERIFY2(payload["items"].is_array() && !payload["items"].empty(), payload.dump().c_str());
    QCOMPARE(payload["items"][0]["status"], Json(firstStatus.toStdString()));
    QCOMPARE(payload["items"][0]["index"], Json(0));
}

void McpFunctionAuthoring_Test::transport_efxModeAdvertisedAndResubmittable_data()
{
    QTest::addColumn<bool>("mover");
    QTest::addColumn<int>("mode");
    QTest::addColumn<QString>("expectedMode");

    QTest::newRow("pan/tilt head") << true << (int)EFXFixture::PanTilt << "Position";
    QTest::newRow("dimmer head") << false << (int)EFXFixture::Dimmer << "Dimmer";
}

void McpFunctionAuthoring_Test::transport_efxModeAdvertisedAndResubmittable()
{
    QFETCH(bool, mover);
    QFETCH(int, mode);
    QFETCH(QString, expectedMode);

    Fixture *fxi = mover ? patchMover(m_doc, 0) : patchDimmers(m_doc, 0, 2);
    EFX *efx = new EFX(m_doc);
    efx->setName("Fx");
    EFXFixture *ef = new EFXFixture(efx);
    ef->setHead(GroupHead(fxi->id(), 0));
    ef->setMode(EFXFixture::Mode(mode));
    efx->addFixture(ef);
    QVERIFY(m_doc->addFunction(efx));

    fastmcpp::server::Server server("qlcplus", "5.0.0");
    fastmcpp::resources::ResourceManager rm;
    fastmcpp::prompts::PromptManager pm;
    auto handler = mcp::withToolResultEncoding(
        fastmcpp::mcp::make_mcp_handler("qlcplus", "5.0.0", server, *m_tm, rm, pm));
    auto callTool = [&](const char *name, const Json &arguments) {
        const Json response = handler({{"jsonrpc", "2.0"}, {"id", 2}, {"method", "tools/call"},
            {"params", {{"name", name}, {"arguments", arguments}}}});
        return response["result"]["structuredContent"];
    };

    const Json list = handler({{"jsonrpc", "2.0"}, {"id", 1}, {"method", "tools/list"}});
    Json detailEnum, updateEnum;
    for (const Json &t : list["result"]["tools"])
    {
        if (t["name"] == "query_function_details")
            detailEnum = t["outputSchema"]["properties"]["items"]["items"]["properties"]["fixtures"]["items"]["properties"]["mode"]["enum"];
        if (t["name"] == "update_functions")
            updateEnum = t["inputSchema"]["properties"]["items"]["items"]["properties"]["fixtures"]["items"]["properties"]["mode"]["enum"];
    }
    QVERIFY2(detailEnum.is_array() && updateEnum.is_array(), list.dump().c_str());

    const Json detail = callTool("query_function_details", {{"targets", {{{"id", efx->id()}}}}});
    const Json fixtures = detail["items"][0]["fixtures"];
    QCOMPARE(fixtures[0]["mode"], Json(expectedMode.toStdString()));
    const Json actualMode = fixtures[0]["mode"];
    QVERIFY2(std::find(detailEnum.begin(), detailEnum.end(), actualMode) != detailEnum.end(), detailEnum.dump().c_str());
    QVERIFY2(std::find(updateEnum.begin(), updateEnum.end(), actualMode) != updateEnum.end(), updateEnum.dump().c_str());

    const Json updated = callTool("update_functions", {{"items", {{{"target", {{"id", efx->id()}}}, {"fixtures", fixtures}}}}});
    QCOMPARE(updated["items"][0]["status"], Json("ok"));
    const Json reread = callTool("query_function_details", {{"targets", {{{"id", efx->id()}}}}});
    QCOMPARE(reread["items"][0]["fixtures"], fixtures);
}

QTEST_MAIN(McpFunctionAuthoring_Test)
