/*
  Q Light Controller Plus - Unit test
  live_tools_test.cpp

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
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <nlohmann/json.hpp>

#include "live_tools_test.h"
#include "tool_registry.h"
#include "doc.h"
#include "fixture.h"
#include "function.h"
#include "functionparent.h"
#include "grandmaster.h"
#include "inputoutputmap.h"
#include "mastertimer.h"
#include "scene.h"
#include "scenevalue.h"
#include "universe.h"

#include <fastmcpp/tools/manager.hpp>

using Json = nlohmann::json;

namespace {

Json parsed(const Json &value)
{
    if (value.is_string())
        return Json::parse(value.get<std::string>());
    return value;
}

/**
 * The DMX source lives as long as the ToolManager, so a single manager is kept
 * per call chain where held values matter. Tools that only read or set engine
 * state can use a throwaway manager.
 */
Json invokeOn(fastmcpp::tools::ToolManager &tm, const char *tool, const Json &args)
{
    return parsed(tm.invoke(tool, args));
}

Json invoke(Doc *doc, const char *tool, const Json &args)
{
    fastmcpp::tools::ToolManager tm;
    registerLiveTools(tm, doc);
    return invokeOn(tm, tool, args);
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

/** Universes must run or the MasterTimer thread blocks on its first tick. */
void startEngine(Doc *doc)
{
    doc->inputOutputMap()->startUniverses();
    doc->masterTimer()->start();
}

uchar postGmValue(Doc *doc, int channel)
{
    QList<Universe*> universes = doc->inputOutputMap()->claimUniverses();
    const uchar value = (uchar)universes.at(0)->postGMValues()->at(channel);
    doc->inputOutputMap()->releaseUniverses(false);
    return value;
}

uchar preGmValue(Doc *doc, int channel)
{
    QList<Universe*> universes = doc->inputOutputMap()->claimUniverses();
    const uchar value = (uchar)universes.at(0)->preGMValues().at(channel);
    doc->inputOutputMap()->releaseUniverses(false);
    return value;
}

}

void McpLiveTools_Test::init()
{
    m_doc = new Doc(this);
}

void McpLiveTools_Test::cleanup()
{
    if (m_doc != nullptr)
        m_doc->masterTimer()->stop();
    delete m_doc;
    m_doc = nullptr;
}

// ─── grand master ──────────────────────────────────────────────────────────

void McpLiveTools_Test::setGrandMaster_roundTripsThroughQuery()
{
    Json set = invoke(m_doc, "set_grand_master", Json{{"value", 200}});
    QVERIFY2(!set.contains("error"), set.dump().c_str());

    Json read = invoke(m_doc, "query_grand_master", Json::object());
    QCOMPARE(read.value("value", -1), 200);
}

void McpLiveTools_Test::setGrandMaster_modes_data()
{
    QTest::addColumn<QString>("valueMode");
    QTest::addColumn<QString>("channelMode");
    QTest::addColumn<int>("expectedValueMode");
    QTest::addColumn<int>("expectedChannelMode");

    QTest::newRow("limit + intensity")
        << QStringLiteral("limit") << QStringLiteral("intensity")
        << (int)GrandMaster::Limit << (int)GrandMaster::Intensity;
    QTest::newRow("reduce + all")
        << QStringLiteral("reduce") << QStringLiteral("allChannels")
        << (int)GrandMaster::Reduce << (int)GrandMaster::AllChannels;
    QTest::newRow("case insensitive")
        << QStringLiteral("REDUCE") << QStringLiteral("ALLCHANNELS")
        << (int)GrandMaster::Reduce << (int)GrandMaster::AllChannels;
}

void McpLiveTools_Test::setGrandMaster_modes()
{
    QFETCH(QString, valueMode);
    QFETCH(QString, channelMode);
    QFETCH(int, expectedValueMode);
    QFETCH(int, expectedChannelMode);

    Json result = invoke(m_doc, "set_grand_master", Json{
        {"valueMode", valueMode.toStdString()}, {"channelMode", channelMode.toStdString()}
    });

    QVERIFY2(!result.contains("error"), result.dump().c_str());
    QCOMPARE((int)m_doc->inputOutputMap()->grandMasterValueMode(), expectedValueMode);
    QCOMPARE((int)m_doc->inputOutputMap()->grandMasterChannelMode(), expectedChannelMode);
}

void McpLiveTools_Test::setGrandMaster_valueOutOfRange_rejected()
{
    invoke(m_doc, "set_grand_master", Json{{"value", 128}});

    Json result = invoke(m_doc, "set_grand_master", Json{{"value", 300}});

    QVERIFY2(result.contains("error"), result.dump().c_str());
    QCOMPARE((int)m_doc->inputOutputMap()->grandMasterValue(), 128);
}

void McpLiveTools_Test::setGrandMaster_scalesUniverseOutputNotPreGmValues()
{
    Fixture *fxi = patchFixture(m_doc, "Par 1", 0, 4);
    startEngine(m_doc);

    Scene *scene = new Scene(m_doc);
    scene->setName("Full");
    scene->setValue(SceneValue(fxi->id(), 0, 255));
    m_doc->addFunction(scene);

    invoke(m_doc, "set_grand_master", Json{{"value", 128}, {"valueMode", "reduce"},
                                           {"channelMode", "allChannels"}});
    scene->start(m_doc->masterTimer(), FunctionParent::master());

    QTRY_COMPARE(preGmValue(m_doc, 0), (uchar)255);

    // The Grand Master scales what leaves the desk; the stored value stays full.
    // Getting these two the wrong way round is the classic GM bug. Assert the
    // exact scaled level, so a wrong scaling factor cannot hide inside a range.
    QTRY_COMPARE(postGmValue(m_doc, 0), (uchar)128);
    QCOMPARE(preGmValue(m_doc, 0), (uchar)255);

    scene->stop(FunctionParent::master());
}

void McpLiveTools_Test::grandMaster_isSessionStateNotSaved()
{
    invoke(m_doc, "set_grand_master", Json{{"value", 77}, {"valueMode", "limit"},
                                           {"channelMode", "allChannels"}});

    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly);
    QXmlStreamWriter writer(&buffer);
    writer.writeStartDocument();
    QVERIFY(m_doc->inputOutputMap()->saveXML(&writer));
    writer.writeEndDocument();
    buffer.close();

    // QLC+ writes no Grand Master state into the project. The tool documents
    // this, and must not mark the document modified for a change that cannot be
    // saved — otherwise load_workspace would refuse on a phantom edit.
    QVERIFY2(!QString::fromUtf8(data).contains("GrandMaster", Qt::CaseInsensitive),
             data.constData());
    QCOMPARE(m_doc->isModified(), false);
}

// ─── blackout ──────────────────────────────────────────────────────────────

void McpLiveTools_Test::setBlackout_togglesAndReports()
{
    Json on = invoke(m_doc, "set_blackout", Json{{"enabled", true}});
    QCOMPARE(on.value("blackout", false), true);
    QCOMPARE(m_doc->inputOutputMap()->blackout(), true);

    Json off = invoke(m_doc, "set_blackout", Json{{"enabled", false}});
    QCOMPARE(off.value("blackout", true), false);
    QCOMPARE(m_doc->inputOutputMap()->blackout(), false);
}

void McpLiveTools_Test::setBlackout_suppressesAtOutputKeepingValues()
{
    Fixture *fxi = patchFixture(m_doc, "Par 1", 0, 4);
    startEngine(m_doc);

    Scene *scene = new Scene(m_doc);
    scene->setName("Full");
    scene->setValue(SceneValue(fxi->id(), 0, 255));
    m_doc->addFunction(scene);
    scene->start(m_doc->masterTimer(), FunctionParent::master());

    QTRY_COMPARE(postGmValue(m_doc, 0), (uchar)255);

    invoke(m_doc, "set_blackout", Json{{"enabled", true}});

    // Blackout is applied by the output patch on its way to the plugin, not by
    // zeroing the desk buffers: both pre- and post-GM values stay where they
    // were, so releasing blackout restores the look instantly.
    QCOMPARE(m_doc->inputOutputMap()->blackout(), true);
    QCOMPARE(postGmValue(m_doc, 0), (uchar)255);
    QCOMPARE(preGmValue(m_doc, 0), (uchar)255);

    invoke(m_doc, "set_blackout", Json{{"enabled", false}});
    QCOMPARE(m_doc->inputOutputMap()->blackout(), false);
    QCOMPARE(postGmValue(m_doc, 0), (uchar)255);

    scene->stop(FunctionParent::master());
}

// ─── write_dmx ─────────────────────────────────────────────────────────────

void McpLiveTools_Test::writeDmx_setsChannelOnOutput()
{
    Fixture *fxi = patchFixture(m_doc, "Par 1", 0, 4);
    startEngine(m_doc);

    fastmcpp::tools::ToolManager tm;
    registerLiveTools(tm, m_doc);

    Json result = invokeOn(tm, "write_dmx", Json{{"items", Json::array({
        {{"fixtureID", (int)fxi->id()}, {"channel", 2}, {"value", 180}}
    })}});

    QCOMPARE(result[0].value("status", std::string()), std::string("ok"));
    QTRY_COMPARE(postGmValue(m_doc, 2), (uchar)180);
}

void McpLiveTools_Test::writeDmx_channelOutOfRange_rejected()
{
    Fixture *fxi = patchFixture(m_doc, "Par 1", 0, 4);

    Json result = invoke(m_doc, "write_dmx", Json{{"items", Json::array({
        {{"fixtureID", (int)fxi->id()}, {"channel", 9}, {"value", 100}}
    })}});

    QVERIFY2(result[0].contains("error"), result[0].dump().c_str());
}

void McpLiveTools_Test::writeDmx_unknownFixture_rejected()
{
    Json result = invoke(m_doc, "write_dmx", Json{{"items", Json::array({
        {{"fixtureID", 4242}, {"channel", 0}, {"value", 100}}
    })}});

    QVERIFY2(result[0].contains("error"), result[0].dump().c_str());
}

void McpLiveTools_Test::writeDmx_release_clearsHeldChannels()
{
    Fixture *fxi = patchFixture(m_doc, "Par 1", 0, 4);
    startEngine(m_doc);

    fastmcpp::tools::ToolManager tm;
    registerLiveTools(tm, m_doc);

    invokeOn(tm, "write_dmx", Json{{"items", Json::array({
        {{"fixtureID", (int)fxi->id()}, {"channel", 1}, {"value", 255}}
    })}});
    QTRY_COMPARE(postGmValue(m_doc, 1), (uchar)255);

    Json released = invokeOn(tm, "write_dmx", Json{{"release", true}});

    QCOMPARE(released.value("status", std::string()), std::string("released"));
    QTRY_COMPARE(postGmValue(m_doc, 1), (uchar)0);
}

// ─── run_functions ─────────────────────────────────────────────────────────

void McpLiveTools_Test::runFunctions_startThenStop()
{
    Fixture *fxi = patchFixture(m_doc, "Par 1", 0, 4);
    startEngine(m_doc);

    Scene *scene = new Scene(m_doc);
    scene->setName("Wash");
    scene->setValue(SceneValue(fxi->id(), 0, 200));
    m_doc->addFunction(scene);

    Json started = invoke(m_doc, "run_functions", Json{{"items", Json::array({
        {{"functionID", (int)scene->id()}, {"action", "start"}}
    })}});
    QCOMPARE(started[0].value("status", std::string()), std::string("ok"));
    QTRY_VERIFY(scene->isRunning());

    Json stopped = invoke(m_doc, "run_functions", Json{{"items", Json::array({
        {{"functionID", (int)scene->id()}, {"action", "stop"}}
    })}});
    QCOMPARE(stopped[0].value("status", std::string()), std::string("ok"));
    QTRY_VERIFY(!scene->isRunning());
}

void McpLiveTools_Test::runFunctions_unknownFunction_notFound()
{
    Json result = invoke(m_doc, "run_functions", Json{{"items", Json::array({
        {{"functionID", 4242}, {"action", "start"}}
    })}});

    // Must carry an error key: an agent scanning batch results for "error"
    // would otherwise read a typo'd ID as success.
    QVERIFY2(result[0].contains("error"), result[0].dump().c_str());
}

void McpLiveTools_Test::runFunctions_missingAction_isPerItemError()
{
    Fixture *fxi = patchFixture(m_doc, "Par 1", 0, 4);
    startEngine(m_doc);

    Scene *scene = new Scene(m_doc);
    scene->setName("Wash");
    scene->setValue(SceneValue(fxi->id(), 0, 200));
    m_doc->addFunction(scene);

    // A malformed second item must not abort the batch after the first has
    // already taken effect.
    Json result = invoke(m_doc, "run_functions", Json{{"items", Json::array({
        {{"functionID", (int)scene->id()}, {"action", "start"}},
        {{"functionID", (int)scene->id()}}
    })}});

    QCOMPARE(result.size(), (size_t)2);
    QVERIFY2(!result[0].contains("error"), result[0].dump().c_str());
    QVERIFY2(result[1].contains("error"), result[1].dump().c_str());

    scene->stop(FunctionParent::master());
}

void McpLiveTools_Test::writeDmx_releaseWithItems_rejected()
{
    Fixture *fxi = patchFixture(m_doc, "Par 1", 0, 4);

    Json result = invoke(m_doc, "write_dmx", Json{
        {"release", true},
        {"items", Json::array({{{"fixtureID", (int)fxi->id()}, {"channel", 0}, {"value", 10}}})}
    });

    // Silently ignoring the items would look like a successful write.
    QVERIFY2(result.contains("error"), result.dump().c_str());
}

void McpLiveTools_Test::writeDmx_heldValuesClearedWhenProjectCleared()
{
    Fixture *fxi = patchFixture(m_doc, "Par 1", 0, 4);
    startEngine(m_doc);

    fastmcpp::tools::ToolManager tm;
    registerLiveTools(tm, m_doc);

    invokeOn(tm, "write_dmx", Json{{"items", Json::array({
        {{"fixtureID", (int)fxi->id()}, {"channel", 0}, {"value", 255}}
    })}});
    QTRY_COMPARE(postGmValue(m_doc, 0), (uchar)255);

    // Holds are keyed by fixture ID, and a new project reassigns those IDs to
    // different fixtures — so they must not survive a project change.
    m_doc->clearContents();
    QTRY_COMPARE(postGmValue(m_doc, 0), (uchar)0);
}

void McpLiveTools_Test::queryRunningFunctions_listsStarted()
{
    Fixture *fxi = patchFixture(m_doc, "Par 1", 0, 4);
    startEngine(m_doc);

    Scene *scene = new Scene(m_doc);
    scene->setName("Wash");
    scene->setValue(SceneValue(fxi->id(), 0, 200));
    m_doc->addFunction(scene);

    QCOMPARE(invoke(m_doc, "query_running_functions", Json::object()).size(), (size_t)0);

    invoke(m_doc, "run_functions", Json{{"items", Json::array({
        {{"functionID", (int)scene->id()}, {"action", "start"}}
    })}});
    QTRY_VERIFY(scene->isRunning());

    Json running = invoke(m_doc, "query_running_functions", Json::object());
    QCOMPARE(running.size(), (size_t)1);
    QCOMPARE(running[0].value("name", std::string()), std::string("Wash"));
    QCOMPARE(running[0].value("functionID", -1), (int)scene->id());

    scene->stop(FunctionParent::master());
}

QTEST_MAIN(McpLiveTools_Test)
