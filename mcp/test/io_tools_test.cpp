/*
  Q Light Controller Plus - Unit test
  io_tools_test.cpp

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

#include "io_tools_test.h"
#include "tool_registry.h"
#include "doc.h"
#include "fixture.h"
#include "inputoutputmap.h"
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

Json configureUniverses(Doc *doc, const Json &items)
{
    fastmcpp::tools::ToolManager tm;
    registerIOTools(tm, doc);
    return parsed(tm.invoke("configure_universes", Json{{"items", items}}));
}

Json deleteUniverses(Doc *doc, const Json &ids)
{
    fastmcpp::tools::ToolManager tm;
    registerIOTools(tm, doc);
    return parsed(tm.invoke("delete_universes", Json{{"ids", ids}}));
}

Fixture *patchFixture(Doc *doc, const QString &name, quint32 universe, quint32 address,
                      quint32 channels)
{
    Fixture *fxi = new Fixture(doc);
    fxi->setName(name);
    fxi->setUniverse(universe);
    fxi->setAddress(address);
    fxi->setChannels(channels);
    doc->addFixture(fxi);
    return fxi;
}

}

void McpIoTools_Test::cleanup()
{
    delete m_doc;
    m_doc = nullptr;
}

// ─── configure_universes: growth ───────────────────────────────────────────

void McpIoTools_Test::configureUniverses_idBeyondCount_createsUniverses_data()
{
    QTest::addColumn<int>("startCount");
    QTest::addColumn<int>("requestedId");
    QTest::addColumn<int>("expectedCount");
    QTest::addColumn<int>("expectedCreated");

    // addUniverse() fills the gap up to the requested id, so a jump creates
    // every missing universe in between, not just the target.
    QTest::newRow("next in line")   << 1 << 1  << 2  << 1;
    QTest::newRow("one gap")        << 1 << 2  << 3  << 2;
    QTest::newRow("wide gap")       << 1 << 5  << 6  << 5;
    QTest::newRow("from default 4") << 4 << 7  << 8  << 4;
    QTest::newRow("top of range")   << 4 << 127 << 128 << 124;
}

void McpIoTools_Test::configureUniverses_idBeyondCount_createsUniverses()
{
    QFETCH(int, startCount);
    QFETCH(int, requestedId);
    QFETCH(int, expectedCount);
    QFETCH(int, expectedCreated);

    m_doc = new Doc(this, startCount);
    QCOMPARE((int)m_doc->inputOutputMap()->universesCount(), startCount);

    Json result = configureUniverses(m_doc, Json::array({{{"universeID", requestedId}}}));

    QVERIFY2(result.is_array(), result.dump().c_str());
    QCOMPARE(result.size(), (size_t)1);
    QVERIFY2(!result[0].contains("error"), result[0].dump().c_str());
    QCOMPARE((int)m_doc->inputOutputMap()->universesCount(), expectedCount);
    QCOMPARE(result[0].value("universesCreated", 0), expectedCreated);
}

void McpIoTools_Test::configureUniverses_createdUniverseIsUsable()
{
    m_doc = new Doc(this, 1);

    Json result = configureUniverses(m_doc, Json::array({
        {{"universeID", 3}, {"name", "Stage Right"}, {"passthrough", true}}
    }));

    QVERIFY2(!result[0].contains("error"), result[0].dump().c_str());
    QCOMPARE(result[0].value("status", std::string()), std::string("ok"));

    Universe *uni = m_doc->inputOutputMap()->universe(3);
    QVERIFY(uni != NULL);
    QCOMPARE(uni->name(), QString("Stage Right"));
    QCOMPARE(uni->passthrough(), true);

    // The gap-filling universes exist too and are left at their defaults.
    QVERIFY(m_doc->inputOutputMap()->universe(1) != NULL);
    QVERIFY(m_doc->inputOutputMap()->universe(2) != NULL);
}

void McpIoTools_Test::configureUniverses_createdUniversesAreStarted()
{
    m_doc = new Doc(this, 1);

    configureUniverses(m_doc, Json::array({{{"universeID", 2}}}));

    // addUniverse() builds the Universe but leaves its writer thread stopped,
    // so a universe added at runtime emits no DMX until the project reloads.
    // Every universe must be running once the tool returns.
    for (Universe *uni : m_doc->inputOutputMap()->universes())
        QVERIFY2(uni->isRunning(), qPrintable(uni->name()));
}

void McpIoTools_Test::configureUniverses_existingId_doesNotGrow()
{
    m_doc = new Doc(this, 4);

    Json result = configureUniverses(m_doc, Json::array({{{"universeID", 2}, {"name", "Front"}}}));

    QCOMPARE((int)m_doc->inputOutputMap()->universesCount(), 4);
    QVERIFY2(!result[0].contains("universesCreated"), result[0].dump().c_str());
    QCOMPARE(m_doc->inputOutputMap()->universe(2)->name(), QString("Front"));
}

void McpIoTools_Test::configureUniverses_invalidId_rejected_data()
{
    QTest::addColumn<QString>("item");

    QTest::newRow("negative")   << QStringLiteral(R"({"universeID": -1})");
    QTest::newRow("above 127")  << QStringLiteral(R"({"universeID": 128})");
    QTest::newRow("not an int") << QStringLiteral(R"({"universeID": "two"})");
    QTest::newRow("fractional") << QStringLiteral(R"({"universeID": 1.5})");
}

void McpIoTools_Test::configureUniverses_invalidId_rejected()
{
    QFETCH(QString, item);

    m_doc = new Doc(this, 4);
    Json result = configureUniverses(m_doc, Json::array({Json::parse(item.toStdString())}));

    QVERIFY2(result[0].contains("error"), result[0].dump().c_str());
    QCOMPARE((int)m_doc->inputOutputMap()->universesCount(), 4);
}

// ─── delete_universes ──────────────────────────────────────────────────────

void McpIoTools_Test::deleteUniverses_trailing_removesAndReports()
{
    m_doc = new Doc(this, 4);

    Json result = deleteUniverses(m_doc, Json::array({3}));

    QCOMPARE(result.size(), (size_t)1);
    QCOMPARE(result[0].value("status", std::string()), std::string("deleted"));
    QCOMPARE((int)m_doc->inputOutputMap()->universesCount(), 3);
}

void McpIoTools_Test::deleteUniverses_batchOutOfOrder_removesBoth()
{
    m_doc = new Doc(this, 4);

    // Ascending input: the tool must reorder, since the engine only ever lets
    // the current last universe go.
    Json result = deleteUniverses(m_doc, Json::array({2, 3}));

    QCOMPARE(result.size(), (size_t)2);
    for (auto &entry : result)
        QVERIFY2(entry.value("status", std::string()) == "deleted", entry.dump().c_str());
    QCOMPARE((int)m_doc->inputOutputMap()->universesCount(), 2);
}

void McpIoTools_Test::deleteUniverses_wouldLeaveGap_rejected()
{
    m_doc = new Doc(this, 4);

    Json result = deleteUniverses(m_doc, Json::array({1}));

    // Assert the tool's own guard fired, not just that something failed —
    // removeUniverse() would also refuse and produce a generic error.
    const std::string error = result[0].value("error", std::string());
    QVERIFY2(error.find("only the last universe") != std::string::npos,
             result[0].dump().c_str());
    QCOMPARE((int)m_doc->inputOutputMap()->universesCount(), 4);
}

void McpIoTools_Test::deleteUniverses_duplicateIds_deletedOnce()
{
    m_doc = new Doc(this, 4);

    Json result = deleteUniverses(m_doc, Json::array({3, 3}));

    QCOMPARE(result.size(), (size_t)1);
    QCOMPARE(result[0].value("status", std::string()), std::string("deleted"));
    QCOMPARE((int)m_doc->inputOutputMap()->universesCount(), 3);
}

void McpIoTools_Test::deleteUniverses_crossUniverseFixture_rejected()
{
    m_doc = new Doc(this, 4);

    // 24 channels from universe 0 address 500 spill into universe 1, so
    // universe 1 is occupied even though Fixture::universe() reports 0.
    Fixture *fxi = new Fixture(m_doc);
    fxi->setName("Wide");
    fxi->setUniverse(0);
    fxi->setAddress(500);
    fxi->setChannels(24);
    fxi->setCrossUniverse(true);
    m_doc->addFixture(fxi, Fixture::invalidId(), true);
    QVERIFY(fxi->crossUniverse());

    Json result = deleteUniverses(m_doc, Json::array({1}));

    const std::string error = result[0].value("error", std::string());
    QVERIFY2(error.find("patched fixtures") != std::string::npos, result[0].dump().c_str());
    QCOMPARE((int)m_doc->inputOutputMap()->universesCount(), 4);
}

void McpIoTools_Test::deleteUniverses_withPatchedFixtures_rejected()
{
    m_doc = new Doc(this, 4);
    Fixture *fxi = patchFixture(m_doc, "Par 1", 3, 0, 6);

    Json result = deleteUniverses(m_doc, Json::array({3}));

    QVERIFY2(result[0].contains("error"), result[0].dump().c_str());
    QCOMPARE(result[0]["fixtureIDs"].size(), (size_t)1);
    QCOMPARE(result[0]["fixtureIDs"][0].get<int>(), (int)fxi->id());
    QCOMPARE((int)m_doc->inputOutputMap()->universesCount(), 4);
}

void McpIoTools_Test::deleteUniverses_lastRemaining_rejected()
{
    m_doc = new Doc(this, 1);

    Json result = deleteUniverses(m_doc, Json::array({0}));

    QVERIFY2(result[0].contains("error"), result[0].dump().c_str());
    QCOMPARE((int)m_doc->inputOutputMap()->universesCount(), 1);
}

// ─── persistence ───────────────────────────────────────────────────────────

void McpIoTools_Test::createdUniverses_surviveXmlRoundTrip()
{
    m_doc = new Doc(this, 1);
    configureUniverses(m_doc, Json::array({{{"universeID", 3}, {"name", "Stage Right"},
                                            {"passthrough", true}}}));
    QCOMPARE((int)m_doc->inputOutputMap()->universesCount(), 4);

    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly);
    QXmlStreamWriter writer(&buffer);
    writer.writeStartDocument();
    QVERIFY(m_doc->inputOutputMap()->saveXML(&writer));
    writer.writeEndDocument();
    buffer.close();

    Doc reloaded(this, 1);
    QXmlStreamReader reader(data);
    reader.readNextStartElement();
    QVERIFY(reloaded.inputOutputMap()->loadXML(reader));

    QCOMPARE((int)reloaded.inputOutputMap()->universesCount(), 4);
    QCOMPARE(reloaded.inputOutputMap()->universe(3)->name(), QString("Stage Right"));
    QCOMPARE(reloaded.inputOutputMap()->universe(3)->passthrough(), true);
}

void McpIoTools_Test::deleteUniverses_unknownId_notFound()
{
    m_doc = new Doc(this, 4);

    Json result = deleteUniverses(m_doc, Json::array({9}));

    QCOMPARE(result[0].value("status", std::string()), std::string("not found"));
    QCOMPARE((int)m_doc->inputOutputMap()->universesCount(), 4);
}

QTEST_MAIN(McpIoTools_Test)
