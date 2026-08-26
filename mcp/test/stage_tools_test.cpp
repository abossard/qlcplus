/*
  Q Light Controller Plus - Unit test
  stage_tools_test.cpp

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

#include "stage_tools_test.h"
#include "tool_registry.h"
#include "doc.h"
#include "fixture.h"
#include "channelsgroup.h"
#include "monitorproperties.h"
#include "scenevalue.h"
#include "qlcinputsource.h"

#include <fastmcpp/tools/manager.hpp>

using Json = nlohmann::json;

namespace {

Json parsed(const Json &value)
{
    if (value.is_string())
        return Json::parse(value.get<std::string>());
    return value;
}

Json invoke(Doc *doc, const char *tool, const Json &args)
{
    fastmcpp::tools::ToolManager tm;
    registerStageTools(tm, doc);
    return parsed(tm.invoke(tool, args));
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

void McpStageTools_Test::init()
{
    m_doc = new Doc(this);
}

void McpStageTools_Test::cleanup()
{
    delete m_doc;
    m_doc = nullptr;
}

// ─── placement ─────────────────────────────────────────────────────────────

void McpStageTools_Test::setPlacement_roundTripsThroughQuery()
{
    Fixture *fxi = patchFixture(m_doc, "Par 1", 0, 6);

    Json set = invoke(m_doc, "set_fixture_placement", Json{{"items", Json::array({
        {{"fixtureID", (int)fxi->id()},
         {"position", {{"x", 1500.0}, {"y", 3200.0}, {"z", -450.0}}},
         {"rotation", {{"x", 0.0}, {"y", 90.0}, {"z", 15.0}}}}
    })}});
    QVERIFY2(!set[0].contains("error"), set[0].dump().c_str());

    Json read = invoke(m_doc, "query_fixture_placement", Json::object());

    QCOMPARE(read.size(), (size_t)1);
    QCOMPARE(read[0].value("fixtureID", -1), (int)fxi->id());
    QCOMPARE(read[0]["position"].value("x", 0.0), 1500.0);
    QCOMPARE(read[0]["position"].value("y", 0.0), 3200.0);
    QCOMPARE(read[0]["position"].value("z", 0.0), -450.0);
    QCOMPARE(read[0]["rotation"].value("y", 0.0), 90.0);
    QCOMPARE(read[0]["rotation"].value("z", 0.0), 15.0);
}

void McpStageTools_Test::setPlacement_millimetreUnitsPreserved()
{
    Fixture *fxi = patchFixture(m_doc, "Par 1", 0, 6);

    invoke(m_doc, "set_fixture_placement", Json{{"items", Json::array({
        {{"fixtureID", (int)fxi->id()}, {"position", {{"x", 2000.0}, {"y", 0.0}, {"z", 0.0}}}}
    })}});

    // The tool documents millimetres; MonitorProperties must receive the number
    // unscaled, or every placed rig lands in the wrong spot by a factor of 1000.
    const QVector3D stored = m_doc->monitorProperties()->fixturePosition(fxi->id(), 0, 0);
    QCOMPARE(stored.x(), 2000.0f);
}

void McpStageTools_Test::setPlacement_omittedAxesKeepValue()
{
    Fixture *fxi = patchFixture(m_doc, "Par 1", 0, 6);

    invoke(m_doc, "set_fixture_placement", Json{{"items", Json::array({
        {{"fixtureID", (int)fxi->id()}, {"position", {{"x", 100.0}, {"y", 200.0}, {"z", 300.0}}}}
    })}});

    // Nudge one axis only.
    Json set = invoke(m_doc, "set_fixture_placement", Json{{"items", Json::array({
        {{"fixtureID", (int)fxi->id()}, {"position", {{"y", 999.0}}}}
    })}});

    QCOMPARE(set[0]["position"].value("x", 0.0), 100.0);
    QCOMPARE(set[0]["position"].value("y", 0.0), 999.0);
    QCOMPARE(set[0]["position"].value("z", 0.0), 300.0);
}

void McpStageTools_Test::setPlacement_multiHead_perHeadIsolation()
{
    // Without a fixture definition, setChannels() builds a generic dimmer mode:
    // one head per channel. 12 channels therefore means 12 addressable heads.
    Fixture *bar = patchFixture(m_doc, "Bar", 0, 12);
    QCOMPARE(bar->heads(), 12);

    invoke(m_doc, "set_fixture_placement", Json{{"items", Json::array({
        {{"fixtureID", (int)bar->id()}, {"head", 2},
         {"position", {{"x", 10.0}, {"y", 20.0}, {"z", 30.0}}}},
        {{"fixtureID", (int)bar->id()}, {"head", 5},
         {"position", {{"x", 70.0}, {"y", 80.0}, {"z", 90.0}}}}
    })}});

    MonitorProperties *props = m_doc->monitorProperties();
    QCOMPARE(props->fixturePosition(bar->id(), 2, 0).x(), 10.0f);
    QCOMPARE(props->fixturePosition(bar->id(), 5, 0).x(), 70.0f);

    // Heads that were never placed keep nothing of their neighbours'.
    QVERIFY(!props->containsItem(bar->id(), 3, 0));
    QVERIFY(!props->containsItem(bar->id(), 4, 0));
}

void McpStageTools_Test::setPlacement_gelColour_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<bool>("valid");
    QTest::addColumn<QString>("expected");

    QTest::newRow("hex")      << QStringLiteral("#ff0000") << true  << QStringLiteral("#ff0000");
    QTest::newRow("name")     << QStringLiteral("red")     << true  << QStringLiteral("#ff0000");
    QTest::newRow("mixed")    << QStringLiteral("#1e90ff") << true  << QStringLiteral("#1e90ff");
    QTest::newRow("nonsense") << QStringLiteral("#GGG")    << false << QString();
    QTest::newRow("empty")    << QStringLiteral("")        << false << QString();
}

void McpStageTools_Test::setPlacement_gelColour()
{
    QFETCH(QString, input);
    QFETCH(bool, valid);
    QFETCH(QString, expected);

    Fixture *fxi = patchFixture(m_doc, "Par 1", 0, 6);

    Json result = invoke(m_doc, "set_fixture_placement", Json{{"items", Json::array({
        {{"fixtureID", (int)fxi->id()}, {"gelColor", input.toStdString()}}
    })}});

    if (valid)
    {
        QVERIFY2(!result[0].contains("error"), result[0].dump().c_str());
        QCOMPARE(result[0].value("gelColor", std::string()), expected.toStdString());
    }
    else
    {
        QVERIFY2(result[0].contains("error"), result[0].dump().c_str());
    }
}

void McpStageTools_Test::setPlacement_unknownFixture_error()
{
    Json result = invoke(m_doc, "set_fixture_placement", Json{{"items", Json::array({
        {{"fixtureID", 4242}, {"position", {{"x", 1.0}}}}
    })}});

    QVERIFY2(result[0].contains("error"), result[0].dump().c_str());
}

void McpStageTools_Test::setPlacement_headOutOfRange_error()
{
    Fixture *fxi = patchFixture(m_doc, "Par 1", 0, 6);

    Json result = invoke(m_doc, "set_fixture_placement", Json{{"items", Json::array({
        {{"fixtureID", (int)fxi->id()}, {"head", 99}, {"position", {{"x", 1.0}}}}
    })}});

    QVERIFY2(result[0].contains("error"), result[0].dump().c_str());
    QVERIFY(!m_doc->monitorProperties()->containsItem(fxi->id(), 99, 0));
}

void McpStageTools_Test::queryPlacement_unplacedFixture_omitted()
{
    Fixture *placed = patchFixture(m_doc, "Placed", 0, 6);
    patchFixture(m_doc, "Unplaced", 10, 6);

    invoke(m_doc, "set_fixture_placement", Json{{"items", Json::array({
        {{"fixtureID", (int)placed->id()}, {"position", {{"x", 1.0}}}}
    })}});

    Json read = invoke(m_doc, "query_fixture_placement", Json::object());

    QCOMPARE(read.size(), (size_t)1);
    QCOMPARE(read[0].value("name", std::string()), std::string("Placed"));
}

void McpStageTools_Test::placement_survivesXmlRoundTrip()
{
    Fixture *fxi = patchFixture(m_doc, "Par 1", 0, 6);
    invoke(m_doc, "set_fixture_placement", Json{{"items", Json::array({
        {{"fixtureID", (int)fxi->id()},
         {"position", {{"x", 1500.0}, {"y", 3200.0}, {"z", -450.0}}},
         {"gelColor", "#00ff00"}}
    })}});

    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly);
    QXmlStreamWriter writer(&buffer);
    writer.writeStartDocument();
    QVERIFY(m_doc->saveXML(&writer));
    writer.writeEndDocument();
    buffer.close();

    Doc reloaded(this);
    QXmlStreamReader reader(data);
    reader.readNextStartElement();
    QVERIFY(reloaded.loadXML(reader));

    const QVector3D pos = reloaded.monitorProperties()->fixturePosition(fxi->id(), 0, 0);
    QCOMPARE(pos.x(), 1500.0f);
    QCOMPARE(pos.z(), -450.0f);
    QCOMPARE(reloaded.monitorProperties()->fixtureGelColor(fxi->id(), 0, 0).name(),
             QString("#00ff00"));
}

// ─── configure_stage ───────────────────────────────────────────────────────

void McpStageTools_Test::configureStage_setsSizeAndUnits()
{
    Json result = invoke(m_doc, "configure_stage", Json{
        {"size", {{"x", 12.0}, {"y", 6.0}, {"z", 8.0}}},
        {"units", "feet"},
        {"stageType", "rock"},
        {"showLabels", true}
    });

    QVERIFY2(!result.contains("error"), result.dump().c_str());
    QCOMPARE(result["size"].value("x", 0.0), 12.0);
    QCOMPARE(result.value("units", std::string()), std::string("feet"));
    QCOMPARE(result.value("showLabels", false), true);
    QCOMPARE(m_doc->monitorProperties()->gridSize().z(), 8.0f);
    QCOMPARE((int)m_doc->monitorProperties()->stageType(), (int)MonitorProperties::StageRock);
}

void McpStageTools_Test::setPlacement_rejectedItem_leavesNoPhantomEntry()
{
    Fixture *fxi = patchFixture(m_doc, "Par 1", 0, 6);
    m_doc->resetModified();   // patching the fixture is what dirtied the Doc

    Json result = invoke(m_doc, "set_fixture_placement", Json{{"items", Json::array({
        {{"fixtureID", (int)fxi->id()}, {"position", {{"x", "abc"}}}}
    })}});

    // A rejected item must not register the fixture: query_fixture_placement
    // documents that fixtures never placed are omitted.
    QVERIFY2(result[0].contains("error"), result[0].dump().c_str());
    QVERIFY(!m_doc->monitorProperties()->containsItem(fxi->id(), 0, 0));
    QCOMPARE(invoke(m_doc, "query_fixture_placement", Json::object()).size(), (size_t)0);
    QCOMPARE(m_doc->isModified(), false);
}

void McpStageTools_Test::setPlacement_missingFixtureId_isPerItemError()
{
    Fixture *fxi = patchFixture(m_doc, "Par 1", 0, 6);

    // A malformed item must not abort the whole batch: the good item still lands.
    Json result = invoke(m_doc, "set_fixture_placement", Json{{"items", Json::array({
        {{"fixtureID", (int)fxi->id()}, {"position", {{"x", 10.0}}}},
        Json::object()
    })}});

    QCOMPARE(result.size(), (size_t)2);
    QVERIFY2(!result[0].contains("error"), result[0].dump().c_str());
    QVERIFY2(result[1].contains("error"), result[1].dump().c_str());
}

void McpStageTools_Test::setPlacement_noGel_omitsGelColor()
{
    Fixture *fxi = patchFixture(m_doc, "Par 1", 0, 6);

    Json result = invoke(m_doc, "set_fixture_placement", Json{{"items", Json::array({
        {{"fixtureID", (int)fxi->id()}, {"position", {{"x", 10.0}}}}
    })}});

    // An unset gel is an invalid QColor whose name() is "#000000"; reporting it
    // would turn "no gel" into a real black gel on the next round trip.
    QVERIFY2(!result[0].contains("gelColor"), result[0].dump().c_str());
    QVERIFY(!invoke(m_doc, "query_fixture_placement", Json::object())[0].contains("gelColor"));
}

void McpStageTools_Test::configureStage_degenerateSize_rejected()
{
    Json result = invoke(m_doc, "configure_stage", Json{{"size", {{"x", -5.0}, {"y", 3.0}, {"z", 2.0}}}});

    QVERIFY2(result.contains("error"), result.dump().c_str());
}

void McpStageTools_Test::configureStage_doesNotMoveFixtures()
{
    Fixture *fxi = patchFixture(m_doc, "Par 1", 0, 6);
    invoke(m_doc, "set_fixture_placement", Json{{"items", Json::array({
        {{"fixtureID", (int)fxi->id()}, {"position", {{"x", 1500.0}, {"y", 3200.0}, {"z", -450.0}}}}
    })}});

    invoke(m_doc, "configure_stage", Json{{"size", {{"x", 12.0}, {"y", 6.0}, {"z", 8.0}}},
                                          {"stageType", "rock"}});

    // Configuring the stage must never rewrite fixture coordinates — that is why
    // pointOfView is deliberately not exposed here.
    const QVector3D pos = m_doc->monitorProperties()->fixturePosition(fxi->id(), 0, 0);
    QCOMPARE(pos.x(), 1500.0f);
    QCOMPARE(pos.y(), 3200.0f);
    QCOMPARE(pos.z(), -450.0f);
}

void McpStageTools_Test::configureStage_pointOfViewNotAccepted()
{
    Json result = invoke(m_doc, "configure_stage", Json{{"pointOfView", "top"}});

    QVERIFY2(result.contains("error"), result.dump().c_str());
}

void McpStageTools_Test::configureStage_unknownEnum_rejected()
{
    Json result = invoke(m_doc, "configure_stage", Json{{"units", "furlongs"}});

    QVERIFY2(result.contains("error"), result.dump().c_str());
}

// ─── channel groups ────────────────────────────────────────────────────────

void McpStageTools_Test::createChannelGroups_upsertsByNameKeepingId()
{
    Fixture *fxi = patchFixture(m_doc, "Par 1", 0, 6);

    Json first = invoke(m_doc, "create_channel_groups", Json{{"items", Json::array({
        {{"name", "House"}, {"channels", Json::array({
            {{"fixtureID", (int)fxi->id()}, {"channel", 0}}
        })}}
    })}});
    QCOMPARE(first[0].value("status", std::string()), std::string("created"));
    const int id = first[0].value("id", -1);

    Json second = invoke(m_doc, "create_channel_groups", Json{{"items", Json::array({
        {{"name", "House"}, {"channels", Json::array({
            {{"fixtureID", (int)fxi->id()}, {"channel", 2}}
        })}}
    })}});

    QCOMPARE(second[0].value("status", std::string()), std::string("updated"));
    QCOMPARE(second[0].value("id", -1), id);
    QCOMPARE(m_doc->channelsGroups().count(), 1);
}

void McpStageTools_Test::createChannelGroups_replacesMembership()
{
    Fixture *fxi = patchFixture(m_doc, "Par 1", 0, 6);

    invoke(m_doc, "create_channel_groups", Json{{"items", Json::array({
        {{"name", "House"}, {"channels", Json::array({
            {{"fixtureID", (int)fxi->id()}, {"channel", 0}},
            {{"fixtureID", (int)fxi->id()}, {"channel", 1}},
            {{"fixtureID", (int)fxi->id()}, {"channel", 2}}
        })}}
    })}});

    // A second call with a smaller set must replace, not append.
    invoke(m_doc, "create_channel_groups", Json{{"items", Json::array({
        {{"name", "House"}, {"channels", Json::array({
            {{"fixtureID", (int)fxi->id()}, {"channel", 5}}
        })}}
    })}});

    ChannelsGroup *group = m_doc->channelsGroups().first();
    QCOMPARE(group->getChannels().count(), 1);
    QCOMPARE((int)group->getChannels().first().channel, 5);
}

void McpStageTools_Test::createChannelGroups_updateKeepsOrderAndInputSource()
{
    Fixture *fxi = patchFixture(m_doc, "Par 1", 0, 6);

    for (const char *name : {"A", "B", "C"})
    {
        invoke(m_doc, "create_channel_groups", Json{{"items", Json::array({
            {{"name", name}, {"channels", Json::array({
                {{"fixtureID", (int)fxi->id()}, {"channel", 0}}
            })}}
        })}});
    }

    ChannelsGroup *a = m_doc->channelsGroups().first();
    QCOMPARE(a->name(), QString("A"));
    QSharedPointer<QLCInputSource> source(new QLCInputSource(0, 42));
    a->setInputSource(source);

    invoke(m_doc, "create_channel_groups", Json{{"items", Json::array({
        {{"name", "A"}, {"channels", Json::array({
            {{"fixtureID", (int)fxi->id()}, {"channel", 4}}
        })}}
    })}});

    // Updating membership must not rebuild the object: that would drop the input
    // mapping a user made and move the group to the end of the ordered list.
    QStringList order;
    for (ChannelsGroup *group : m_doc->channelsGroups())
        order << group->name();
    QCOMPARE(order, (QStringList{"A", "B", "C"}));
    QVERIFY(!m_doc->channelsGroups().first()->inputSource().isNull());
    QCOMPARE((int)m_doc->channelsGroups().first()->inputSource()->channel(), 42);
}

void McpStageTools_Test::createChannelGroups_preservesChannelOrder()
{
    Fixture *a = patchFixture(m_doc, "A", 0, 4);
    Fixture *b = patchFixture(m_doc, "B", 10, 4);
    Fixture *c = patchFixture(m_doc, "C", 20, 4);

    invoke(m_doc, "create_channel_groups", Json{{"items", Json::array({
        {{"name", "Mixed"}, {"channels", Json::array({
            {{"fixtureID", (int)c->id()}, {"channel", 3}},
            {{"fixtureID", (int)a->id()}, {"channel", 1}},
            {{"fixtureID", (int)b->id()}, {"channel", 2}}
        })}}
    })}});

    const QList<SceneValue> channels = m_doc->channelsGroups().first()->getChannels();
    QCOMPARE(channels.count(), 3);
    QCOMPARE(channels.at(0).fxi, c->id());
    QCOMPARE((int)channels.at(0).channel, 3);
    QCOMPARE(channels.at(1).fxi, a->id());
    QCOMPARE(channels.at(2).fxi, b->id());
}

void McpStageTools_Test::createChannelGroups_channelOutOfRange_rejectedWholesale()
{
    Fixture *fxi = patchFixture(m_doc, "Par 1", 0, 4);

    Json result = invoke(m_doc, "create_channel_groups", Json{{"items", Json::array({
        {{"name", "Bad"}, {"channels", Json::array({
            {{"fixtureID", (int)fxi->id()}, {"channel", 0}},
            {{"fixtureID", (int)fxi->id()}, {"channel", 9}}
        })}}
    })}});

    // One bad channel must not leave a half-built group behind.
    QVERIFY2(result[0].contains("error"), result[0].dump().c_str());
    QCOMPARE(m_doc->channelsGroups().count(), 0);
}

void McpStageTools_Test::createChannelGroups_unknownFixture_rejected()
{
    Json result = invoke(m_doc, "create_channel_groups", Json{{"items", Json::array({
        {{"name", "Bad"}, {"channels", Json::array({
            {{"fixtureID", 4242}, {"channel", 0}}
        })}}
    })}});

    QVERIFY2(result[0].contains("error"), result[0].dump().c_str());
    QCOMPARE(m_doc->channelsGroups().count(), 0);
}

void McpStageTools_Test::queryChannelGroups_reportsMembers()
{
    Fixture *a = patchFixture(m_doc, "A", 0, 4);
    Fixture *b = patchFixture(m_doc, "B", 10, 4);

    invoke(m_doc, "create_channel_groups", Json{{"items", Json::array({
        {{"name", "Front"}, {"channels", Json::array({
            {{"fixtureID", (int)a->id()}, {"channel", 0}}
        })}},
        {{"name", "Back"}, {"channels", Json::array({
            {{"fixtureID", (int)b->id()}, {"channel", 1}},
            {{"fixtureID", (int)b->id()}, {"channel", 2}}
        })}}
    })}});

    Json read = invoke(m_doc, "query_channel_groups", Json::object());

    QCOMPARE(read.size(), (size_t)2);
    // Counts differ per group, so this proves membership is read per group
    // rather than shared.
    int front = 0, back = 0;
    for (auto &group : read)
    {
        if (group.value("name", std::string()) == "Front") front = (int)group["channels"].size();
        if (group.value("name", std::string()) == "Back") back = (int)group["channels"].size();
    }
    QCOMPARE(front, 1);
    QCOMPARE(back, 2);
}

void McpStageTools_Test::deleteChannelGroups_removesGroupKeepsFixtures()
{
    Fixture *fxi = patchFixture(m_doc, "Par 1", 0, 4);

    Json created = invoke(m_doc, "create_channel_groups", Json{{"items", Json::array({
        {{"name", "House"}, {"channels", Json::array({
            {{"fixtureID", (int)fxi->id()}, {"channel", 0}}
        })}}
    })}});
    const int id = created[0].value("id", -1);

    Json result = invoke(m_doc, "delete_channel_groups", Json{{"ids", Json::array({id})}});

    QCOMPARE(result[0].value("status", std::string()), std::string("deleted"));
    QCOMPARE(m_doc->channelsGroups().count(), 0);
    QCOMPARE(m_doc->fixtures().count(), 1);
}

void McpStageTools_Test::channelGroups_surviveXmlRoundTrip()
{
    Fixture *fxi = patchFixture(m_doc, "Par 1", 0, 6);
    invoke(m_doc, "create_channel_groups", Json{{"items", Json::array({
        {{"name", "House"}, {"channels", Json::array({
            {{"fixtureID", (int)fxi->id()}, {"channel", 0}},
            {{"fixtureID", (int)fxi->id()}, {"channel", 3}}
        })}}
    })}});

    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly);
    QXmlStreamWriter writer(&buffer);
    writer.writeStartDocument();
    QVERIFY(m_doc->saveXML(&writer));
    writer.writeEndDocument();
    buffer.close();

    Doc reloaded(this);
    QXmlStreamReader reader(data);
    reader.readNextStartElement();
    QVERIFY(reloaded.loadXML(reader));

    QCOMPARE(reloaded.channelsGroups().count(), 1);
    QCOMPARE(reloaded.channelsGroups().first()->name(), QString("House"));
    QCOMPARE(reloaded.channelsGroups().first()->getChannels().count(), 2);
}

QTEST_MAIN(McpStageTools_Test)
