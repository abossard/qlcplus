/*
  Q Light Controller Plus - Unit test
*/

#include <QtTest>

#include "query_tools_test.h"
#include "tool_registry.h"
#include "doc.h"
#include "fixture.h"
#include "scene.h"
#include "chaser.h"
#include "scriptv4.h"
#include "qlcpalette.h"

#include <fastmcpp/tools/manager.hpp>

using Json = nlohmann::json;

namespace {

Json parsedToolResult(const Json &value)
{
    if (value.is_string())
        return Json::parse(value.get<std::string>());
    return value;
}

fastmcpp::tools::ToolManager makeQueryToolManager(Doc *doc)
{
    fastmcpp::tools::ToolManager tm;
    registerQueryTools(tm, doc, nullptr);
    return tm;
}

void verifyNonNegativeInteger(const Json &obj, const char *key)
{
    QVERIFY2(obj.contains(key), qPrintable(QStringLiteral("Missing key: %1").arg(key)));
    QVERIFY2(obj[key].is_number_integer(), qPrintable(QStringLiteral("Key is not an integer: %1").arg(key)));
    QVERIFY2(obj[key].get<int>() >= 0, qPrintable(QStringLiteral("Key is negative: %1").arg(key)));
}

}

void QueryTools_Test::init()
{
    m_doc = new Doc(this);
}

void QueryTools_Test::cleanup()
{
    delete m_doc;
    m_doc = nullptr;
}

void QueryTools_Test::queryPalettes_invalidTypeFilterReturnsError()
{
    auto tm = makeQueryToolManager(m_doc);
    Json result = parsedToolResult(tm.invoke("query_palettes", Json{{"typeFilter", "Bogus"}}));

    QVERIFY2(result.is_object(), "Expected error object");
    QVERIFY2(result.contains("error"), "Invalid typeFilter must return an error");
    QString message = QString::fromStdString(result["error"].get<std::string>());
    QVERIFY2(message.contains(QStringLiteral("typeFilter")), qPrintable(message));
}

void QueryTools_Test::palettes_createQueryRoundTrip_data()
{
    // One rich row per upstream 5.3.0 palette type. Values are DISTINCT (and
    // distinct between value/value2 for Shutter) so a swapped accessor in
    // create_palettes or query_palettes serialization fails the round-trip.
    QTest::addColumn<QString>("type");
    QTest::addColumn<QString>("name");
    QTest::addColumn<double>("x");
    QTest::addColumn<double>("y");
    QTest::addColumn<double>("z");
    QTest::addColumn<int>("value");
    QTest::addColumn<int>("value2"); // -1 == not applicable

    QTest::newRow("Position3D") << QStringLiteral("Position3D") << QStringLiteral("Upstage Center")
                                << 11.0 << 22.0 << 33.0 << -1 << -1;
    QTest::newRow("Gobo")       << QStringLiteral("Gobo")       << QStringLiteral("Gobo Spiral")
                                << 0.0 << 0.0 << 0.0 << 7 << -1;
    // Shutter is a 2-value type: value=preset (at(0)), value2=percentage (at(1)).
    QTest::newRow("Shutter")    << QStringLiteral("Shutter")    << QStringLiteral("Strobe Fast")
                                << 0.0 << 0.0 << 0.0 << 5 << 73;
    QTest::newRow("Zoom")       << QStringLiteral("Zoom")       << QStringLiteral("Zoom Wide")
                                << 0.0 << 0.0 << 0.0 << 88 << -1;
}

void QueryTools_Test::palettes_createQueryRoundTrip()
{
    // Exercise the real MCP boundary for the upstream 5.3.0 palette types:
    // create_palettes (new switch cases) -> query_palettes (new serialize cases).
    QFETCH(QString, type);
    QFETCH(QString, name);
    QFETCH(double, x);
    QFETCH(double, y);
    QFETCH(double, z);
    QFETCH(int, value);
    QFETCH(int, value2);

    fastmcpp::tools::ToolManager tm;
    registerPaletteTools(tm, m_doc);
    registerQueryTools(tm, m_doc, nullptr);

    // Arrange + Act: build a create item shaped for this palette type.
    Json item = {{"name", name.toStdString()}, {"type", type.toStdString()}};
    if (type == QStringLiteral("Position3D"))
    {
        item["x"] = x;
        item["y"] = y;
        item["z"] = z;
    }
    else
    {
        item["value"] = value;
        if (value2 >= 0)
            item["value2"] = value2;
    }

    Json createRes = parsedToolResult(tm.invoke("create_palettes", Json{{"items", Json::array({item})}}));
    QVERIFY2(createRes.is_array(), "create_palettes must return an array");
    QCOMPARE((int)createRes.size(), 1);
    QCOMPARE(createRes[0]["status"].get<std::string>(), std::string("created"));
    QCOMPARE(createRes[0]["type"].get<std::string>(), type.toStdString());

    // Assert: typeFilter returns exactly this palette, with type-specific fields
    // round-tripping through the MCP serialize cases.
    Json list = parsedToolResult(tm.invoke("query_palettes", Json{{"typeFilter", type.toStdString()}}));
    QVERIFY2(list.is_array(), "query_palettes must return an array");
    QCOMPARE((int)list.size(), 1);
    const Json &entry = list[0];
    QCOMPARE(entry["type"].get<std::string>(), type.toStdString());

    if (type == QStringLiteral("Position3D"))
    {
        // x/y/z sourced from floatValue1()/floatValue2()/floatValue3().
        QCOMPARE(entry["x"].get<double>(), x);
        QCOMPARE(entry["y"].get<double>(), y);
        QCOMPARE(entry["z"].get<double>(), z);
    }
    else
    {
        // value sourced from intValue1(); value2 (Shutter pct) from intValue2().
        QCOMPARE(entry["value"].get<int>(), value);
        if (value2 >= 0)
            QCOMPARE(entry["value2"].get<int>(), value2);
    }
}

void QueryTools_Test::palettes_createInvalidTypeReturnsError()
{
    // create_palettes must reject an unknown type via the Undefined->error branch,
    // not create a palette.
    fastmcpp::tools::ToolManager tm;
    registerPaletteTools(tm, m_doc);

    Json createRes = parsedToolResult(tm.invoke("create_palettes",
        Json{{"items", Json::array({Json{{"name", "Nope"}, {"type", "Bogus"}}})}}));

    QVERIFY2(createRes.is_array(), "create_palettes must return an array");
    QCOMPARE((int)createRes.size(), 1);
    const Json &res = createRes[0];
    QVERIFY2(res.contains("error"), "Invalid type must return an error entry");
    QVERIFY2(!res.contains("status") || res["status"].get<std::string>() != std::string("created"),
             "Invalid type must not create a palette");
    QString message = QString::fromStdString(res["error"].get<std::string>());
    QVERIFY2(message.contains(QStringLiteral("invalid"), Qt::CaseInsensitive)
                 && message.contains(QStringLiteral("type"), Qt::CaseInsensitive),
             qPrintable(message));
}

void QueryTools_Test::queryRgbAlgorithms_invalidTypeReturnsError()
{
    auto tm = makeQueryToolManager(m_doc);
    Json result = parsedToolResult(tm.invoke("query_rgb_algorithms", Json{{"type", "Bogus"}}));

    QVERIFY2(result.is_object(), "Expected error object");
    QVERIFY2(result.contains("error"), "Invalid type must return an error");
    QString message = QString::fromStdString(result["error"].get<std::string>());
    QVERIFY2(message.contains(QStringLiteral("'type'")), qPrintable(message));
}

void QueryTools_Test::queryWorkspaceSummary_returnsExpectedCounts()
{
    auto tm = makeQueryToolManager(m_doc);
    Json result = parsedToolResult(tm.invoke("query_workspace_summary", Json::object()));

    QVERIFY2(result.is_object(), "Expected summary object");

    const char *topLevelKeys[] = {
        "fixtures", "fixtureGroups", "universes", "palettes", "vcPages", "runningFunctions"
    };
    for (const char *key : topLevelKeys)
        verifyNonNegativeInteger(result, key);

    QVERIFY2(result.contains("functions"), "Missing functions object");
    QVERIFY2(result["functions"].is_object(), "functions must be an object");

    const char *functionKeys[] = {
        "total", "scenes", "chasers", "collections", "rgbMatrices",
        "efx", "scripts", "shows", "sequences", "audio", "video"
    };
    for (const char *key : functionKeys)
        verifyNonNegativeInteger(result["functions"], key);
}

void QueryTools_Test::queryWorkspaceSummary_populatedDoc_returnsExactCounts()
{
    // Add fixtures (no fixture def needed — bare Fixture works for counting)
    Fixture *f1 = new Fixture(m_doc);
    f1->setName("F1"); f1->setChannels(1); f1->setAddress(0);
    QVERIFY(m_doc->addFixture(f1));

    Fixture *f2 = new Fixture(m_doc);
    f2->setName("F2"); f2->setChannels(1); f2->setAddress(1);
    QVERIFY(m_doc->addFixture(f2));

    // Add functions: 2 scenes, 1 chaser, 1 script  → total 4
    Scene *s1 = new Scene(m_doc); s1->setName("S1"); QVERIFY(m_doc->addFunction(s1));
    Scene *s2 = new Scene(m_doc); s2->setName("S2"); QVERIFY(m_doc->addFunction(s2));
    Chaser *c1 = new Chaser(m_doc); c1->setName("C1"); QVERIFY(m_doc->addFunction(c1));
    Script *sc1 = new Script(m_doc); sc1->setName("Sc1"); QVERIFY(m_doc->addFunction(sc1));

    // Add palettes
    auto *p1 = new QLCPalette(QLCPalette::Dimmer); p1->setName("Full"); p1->setValue(QVariant(255));
    QVERIFY(m_doc->addPalette(p1));
    auto *p2 = new QLCPalette(QLCPalette::Color);  p2->setName("Red");
    p2->setValue(QVariant(QLCPalette::colorToString(QColor(255, 0, 0), QColor(0, 0, 0))));
    QVERIFY(m_doc->addPalette(p2));

    auto tm = makeQueryToolManager(m_doc);
    Json result = parsedToolResult(tm.invoke("query_workspace_summary", Json::object()));

    QVERIFY2(result.is_object(), "Expected summary object");

    QCOMPARE(result["fixtures"].get<int>(), 2);
    QCOMPARE(result["palettes"].get<int>(), 2);
    QCOMPARE(result["runningFunctions"].get<int>(), 0);

    // Universe count is environment-dependent — only assert non-negative.
    QVERIFY(result["universes"].get<int>() >= 0);

    QVERIFY(result.contains("functions") && result["functions"].is_object());
    const Json &fns = result["functions"];
    QCOMPARE(fns["total"].get<int>(), 4);
    QCOMPARE(fns["scenes"].get<int>(), 2);
    QCOMPARE(fns["chasers"].get<int>(), 1);
    QCOMPARE(fns["scripts"].get<int>(), 1);
    QCOMPARE(fns["collections"].get<int>(), 0);
    QCOMPARE(fns["rgbMatrices"].get<int>(), 0);
    QCOMPARE(fns["efx"].get<int>(), 0);
}

QTEST_MAIN(QueryTools_Test)