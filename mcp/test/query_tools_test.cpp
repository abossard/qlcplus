/*
  Q Light Controller Plus - Unit test
*/

#include <QtTest>
#include <QSignalSpy>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "query_tools_test.h"
#include "tool_registry.h"
#include "doc.h"
#include "fixture.h"
#include "scene.h"
#include "chaser.h"
#include "scriptv4.h"
#include "qlcpalette.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcfixturedefcache.h"
#include "qlcchannel.h"
#include "qlcmodifierscache.h"
#include "channelmodifier.h"
#include "rgbscriptscache.h"
#include "huescriptscache.h"

#include <fastmcpp/tools/manager.hpp>
#include <fastmcpp/mcp/handler.hpp>
#include <fastmcpp/server/server.hpp>
#include <fastmcpp/resources/manager.hpp>
#include <fastmcpp/prompts/manager.hpp>

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

QList<quint32> addFixtureLayout(Doc *doc)
{
    const struct FixtureSpec {
        const char *name;
        quint32 universe;
        quint32 address;
        quint32 channels;
    } specs[] = {
        {"Alpha Wash", 0, 10, 4},
        {"beta Spot", 1, 20, 8},
        {"Alpha Bar", 0, 30, 3},
        {"Gamma", 2, 5, 1},
        {"alpha Accent", 1, 100, 6},
    };

    QList<quint32> ids;
    for (const FixtureSpec &spec : specs)
    {
        auto *fixture = new Fixture(doc);
        fixture->setName(spec.name);
        fixture->setUniverse(spec.universe);
        fixture->setAddress(spec.address);
        fixture->setChannels(spec.channels);
        if (!doc->addFixture(fixture))
        {
            delete fixture;
            return {};
        }
        ids.append(fixture->id());
    }
    return ids;
}

QList<int> fixtureIds(const Json &fixtures)
{
    QList<int> ids;
    if (!fixtures.is_array())
        return ids;
    for (const Json &fixture : fixtures)
    {
        if (!fixture.is_object() || !fixture.contains("id") || !fixture["id"].is_number_integer())
            return {};
        ids.append(fixture["id"].get<int>());
    }
    return ids;
}

Json fixtureState(const Fixture *fixture)
{
    return {
        {"id", fixture->id()},
        {"name", fixture->name().toStdString()},
        {"universe", fixture->universe()},
        {"address", fixture->address()},
        {"channels", fixture->channels()}
    };
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

void QueryTools_Test::palettes_zoomValue_data()
{
    QTest::addColumn<QByteArray>("valueJson");
    QTest::addColumn<QByteArray>("expectedJson"); // null == rejected

    QTest::newRow("fractional") << QByteArray("22.5") << QByteArray("22.5");
    QTest::newRow("integer stays integer") << QByteArray("40") << QByteArray("40");
    QTest::newRow("negative") << QByteArray("-1") << QByteArray("null");
    QTest::newRow("above range") << QByteArray("360.5") << QByteArray("null");
    QTest::newRow("string") << QByteArray("\"40\"") << QByteArray("null");
}

void QueryTools_Test::deletePalettes_perSelectorRecords()
{
    fastmcpp::tools::ToolManager tm;
    registerPaletteTools(tm, m_doc);
    QList<quint32> ids;
    for (const char *name : {"Warm A", "Warm B", "Cold"})
    {
        QLCPalette *p = new QLCPalette(QLCPalette::Dimmer);
        p->setName(name);
        p->setValue(10);
        QVERIFY(m_doc->addPalette(p));
        ids << p->id();
    }

    // ids first (indices 0..), then names; one record per selector.
    const Json res = parsedToolResult(tm.invoke("delete_palettes", Json{
        {"ids", Json::array({int(ids[0]), 9999, int(ids[0]), 0.5})},
        {"names", Json::array({"warm*", "Nothing*", 7})}}));
    QVERIFY2(res.is_array() && res.size() == 7, res.dump().c_str());
    for (int i = 0; i < 7; i++)
        QCOMPARE(res[i].value("index", -1), i);
    QCOMPARE(res[0].value("outcome", std::string()), std::string("deleted"));
    QCOMPARE(res[1].value("status", std::string()), std::string("error"));
    QCOMPARE(res[2].value("outcome", std::string()), std::string("duplicate"));
    QCOMPARE(res[2].value("duplicateOf", -1), 0);
    QCOMPARE(res[3].value("status", std::string()), std::string("error"));
    // The glob reports both matches: Warm A already gone via index 0, Warm B deleted here.
    QCOMPARE(res[4].value("outcome", std::string()), std::string("deleted"));
    QVERIFY2(res[4]["palettes"].size() == 2, res.dump().c_str());
    QCOMPARE(res[4]["palettes"][0].value("duplicateOf", -1), 0);
    QCOMPARE(res[4]["palettes"][1].value("outcome", std::string()), std::string("deleted"));
    QCOMPARE(res[5].value("status", std::string()), std::string("ok"));
    QCOMPARE(res[5].value("outcome", std::string()), std::string("noMatch"));
    QCOMPARE(res[6].value("status", std::string()), std::string("error"));
    QCOMPARE(m_doc->palettes().size(), 1);
    QCOMPARE(m_doc->palettes().first()->name(), QString("Cold"));
}

void QueryTools_Test::palettes_zoomValue()
{
    QFETCH(QByteArray, valueJson);
    QFETCH(QByteArray, expectedJson);
    fastmcpp::tools::ToolManager tm;
    registerPaletteTools(tm, m_doc);
    registerQueryTools(tm, m_doc, nullptr);

    const Json item = {{"name", "Zoom"}, {"type", "Zoom"}, {"value", Json::parse(valueJson.constData())}};
    const Json created = parsedToolResult(tm.invoke("create_palettes", Json{{"items", Json::array({item})}}));
    const Json expected = Json::parse(expectedJson.constData());
    const Json list = parsedToolResult(tm.invoke("query_palettes", Json{{"typeFilter", "Zoom"}}));
    if (expected.is_null())
    {
        QVERIFY2(created[0].value("status", std::string()) == "error", created.dump().c_str());
        QVERIFY2(QString::fromStdString(created[0].value("error", std::string())).contains("value"), created.dump().c_str());
        QVERIFY2(list.is_array() && list.empty(), list.dump().c_str());
        return;
    }
    QVERIFY2(list.is_array() && list.size() == 1, list.dump().c_str());
    QCOMPARE(QByteArray(list[0]["value"].dump().c_str()), expectedJson);
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
    QCOMPARE(createRes[0]["outcome"].get<std::string>(), std::string("created"));
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

void QueryTools_Test::palettes_updateModifiedAndFloatReadback()
{
    fastmcpp::tools::ToolManager tm;
    registerPaletteTools(tm, m_doc);
    registerQueryTools(tm, m_doc, nullptr);

    Json item = {{"name", "PT"}, {"type", "PanTilt"}, {"panDegrees", 10.0}, {"tiltDegrees", 20.0}};
    parsedToolResult(tm.invoke("create_palettes", Json{{"items", Json::array({item})}}));
    m_doc->resetModified();

    item["panDegrees"] = 10.5;
    item["tiltDegrees"] = 20.5;
    Json res = parsedToolResult(tm.invoke("create_palettes", Json{{"items", Json::array({item})}}));
    QCOMPARE(res[0]["outcome"].get<std::string>(), std::string("updated"));
    QVERIFY2(m_doc->isModified(), "updating a palette must mark the workspace modified");

    Json list = parsedToolResult(tm.invoke("query_palettes", Json{{"typeFilter", "PanTilt"}}));
    QCOMPARE(list[0]["panDegrees"].get<double>(), 10.5);
    QCOMPARE(list[0]["tiltDegrees"].get<double>(), 20.5);
}

void QueryTools_Test::palettes_invalidValueKind_data()
{
    QTest::addColumn<QString>("json");
    QTest::newRow("fractional value") << R"({"name":"D","type":"Dimmer","value":1.5})";
    QTest::newRow("oversized value") << R"({"name":"D","type":"Dimmer","value":256})";
    QTest::newRow("bool value") << R"({"name":"G","type":"Gobo","value":true})";
    QTest::newRow("string degrees") << R"({"name":"P","type":"Pan","panDegrees":"10"})";
    QTest::newRow("shutter pct 101") << R"({"name":"S","type":"Shutter","value":1,"value2":101})";
    QTest::newRow("rgb not color") << R"({"name":"C","type":"Color","rgb":"nope"})";
    QTest::newRow("name not string") << R"({"name":5,"type":"Dimmer"})";
}

void QueryTools_Test::palettes_invalidValueKind()
{
    QFETCH(QString, json);
    fastmcpp::tools::ToolManager tm;
    registerPaletteTools(tm, m_doc);

    Json item = Json::parse(json.toStdString());
    Json res = parsedToolResult(tm.invoke("create_palettes", Json{{"items", Json::array({item})}}));
    QVERIFY2(res.is_array() && res.size() == 1, res.dump().c_str());
    QCOMPARE(res[0]["index"].get<int>(), 0);
    QCOMPARE(res[0]["status"].get<std::string>(), std::string("error"));
    QVERIFY(res[0].contains("error"));
    QCOMPARE(m_doc->palettes().size(), 0);
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

void QueryTools_Test::queryRgbAlgorithms_matrixTypeSchema()
{
    auto tm = makeQueryToolManager(m_doc);
    const Json schema = tm.input_schema_for("query_rgb_algorithms");
    const Json &props = schema.at("properties");
    QVERIFY(props.contains("matrixType"));
    QVERIFY(props.at("matrixType").at("enum") == Json::array({"RGBMatrix", "HUEMatrix"}));
    QVERIFY(props.at("type").at("enum") == Json::array({"Script", "Text", "Image", "Audio", "Plain"}));
    QVERIFY(!schema.contains("required") || schema.at("required").empty());
}

void QueryTools_Test::queryRgbAlgorithms_matrixType_data()
{
    QTest::addColumn<QString>("matrixType");
    QTest::newRow("default-stock") << QString();
    QTest::newRow("explicit-stock") << QStringLiteral("RGBMatrix");
    QTest::newRow("hsv-only") << QStringLiteral("HUEMatrix");
}

void QueryTools_Test::queryRgbAlgorithms_matrixType()
{
    QFETCH(QString, matrixType);
    const QDir rgbDir(QFINDTESTDATA("../../resources/rgbscripts"));
    const QDir hueDir(QFINDTESTDATA("../../resources/huescripts"));
    QVERIFY(m_doc->rgbScriptsCache()->load(rgbDir));
    QVERIFY(m_doc->hueScriptsCache()->load(rgbDir, false));
    QVERIFY(m_doc->hueScriptsCache()->load(hueDir, true));
    auto tm = makeQueryToolManager(m_doc);
    Json args = Json::object();
    if (!matrixType.isEmpty())
        args["matrixType"] = matrixType.toStdString();

    const Json result = parsedToolResult(tm.invoke("query_rgb_algorithms", args));
    QVERIFY2(result.is_array(), result.dump().c_str());
    QStringList names;
    for (const auto &entry : result)
        names.append(QString::fromStdString(entry.at("name").get<std::string>()));

    const bool hue = matrixType == QStringLiteral("HUEMatrix");
    QCOMPARE(names.contains("Audio Spectrum Bars"), hue);
    QCOMPARE(names.contains("Stripes"), !hue);
    QCOMPARE(names.contains("Audio Spectrum"), !hue);
    QCOMPARE(names.contains("Audio Fire"), hue);
    if (hue)
    {
        QCOMPARE(result.size(), size_t(70));
        QVERIFY(names.contains("Hue Fade"));
        QVERIFY(names.contains("Audio Waterfall"));
        for (const auto &entry : result)
            QCOMPARE(entry.at("type").get<std::string>(), std::string("Script"));
    }
    else
    {
        const Json stock = parsedToolResult(tm.invoke("query_rgb_algorithms", Json::object()));
        QVERIFY(result == stock);
    }

    // Kind and case-insensitive name filters still compose with matrixType.
    args["name"] = "aUdIo SpEcTrUm";
    args["type"] = hue ? "Script" : "Audio";
    const Json filtered = parsedToolResult(tm.invoke("query_rgb_algorithms", args));
    QVERIFY2(filtered.is_array(), filtered.dump().c_str());
    QCOMPARE(filtered.size(), size_t(1));
    QCOMPARE(filtered[0].at("name").get<std::string>(),
             std::string(hue ? "Audio Spectrum Bars" : "Audio Spectrum"));
    QCOMPARE(filtered[0].at("type").get<std::string>(), std::string(hue ? "Script" : "Audio"));
    QCOMPARE(filtered[0].at("audioReactive").get<bool>(), true);
    if (hue)
    {
        QVERIFY(filtered[0].contains("properties"));
        bool found = false;
        for (const auto &prop : filtered[0].at("properties"))
        {
            if (prop.at("name") != "rgb_mix")
                continue;
            found = true;
            QCOMPARE(prop.at("type").get<std::string>(), std::string("range"));
            QCOMPARE(prop.at("min").get<int>(), 0);
            QCOMPARE(prop.at("max").get<int>(), 5);
            QCOMPARE(prop.at("default").get<std::string>(), std::string("0"));
        }
        QVERIFY(found);
    }
    args["type"] = hue ? "Audio" : "Script";
    const Json wrongKind = parsedToolResult(tm.invoke("query_rgb_algorithms", args));
    QVERIFY(wrongKind.is_array());
    QVERIFY(wrongKind.empty());
}

void QueryTools_Test::queryRgbAlgorithms_floatBoundsMetadata()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    QFile scriptFile(temp.filePath("boundedfloatmetadata.js"));
    QVERIFY(scriptFile.open(QIODevice::WriteOnly | QIODevice::Text));
    scriptFile.write(R"(
        (function() {
          var algo = new Object;
          var gain = 0.00001;
          algo.apiVersion = 2;
          algo.name = "Bounded Float Metadata";
          algo.author = "QLC+ Unit Test";
          algo.acceptColors = 0;
          algo.properties = [
            "name:gain|display:Gain|type:float|values:0.00001,1|write:setGain|read:getGain"
          ];
          algo.rgbMap = function(width, height, rgb, step) {
            var out = [];
            for (var y = 0; y < height; y++) {
              var row = [];
              for (var x = 0; x < width; x++) row.push(0);
              out.push(row);
            }
            return out;
          };
          algo.rgbMapStepCount = function(width, height) { return 1; };
          algo.setGain = function(value) { gain = parseFloat(value); };
          algo.getGain = function() { return gain; };
          return algo;
        })();
    )");
    scriptFile.close();

    QVERIFY(m_doc->rgbScriptsCache()->load(QDir(temp.path())));

    auto tm = makeQueryToolManager(m_doc);
    const Json result = parsedToolResult(tm.invoke("query_rgb_algorithms", Json{
        {"matrixType", "RGBMatrix"},
        {"type", "Script"},
        {"name", "bounded float metadata"}
    }));

    QVERIFY2(result.is_array(), result.dump().c_str());
    QCOMPARE(result.size(), size_t(1));
    const Json &algo = result.at(0);
    QVERIFY(algo.contains("properties"));
    QVERIFY(algo.at("properties").is_array());

    bool found = false;
    for (const auto &prop : algo.at("properties"))
    {
        if (prop.at("name").get<std::string>() != std::string("gain"))
            continue;
        found = true;
        QCOMPARE(prop.at("type").get<std::string>(), std::string("float"));
        QVERIFY(prop.contains("min"));
        QVERIFY(prop.contains("max"));
        QCOMPARE(prop.at("min").get<double>(), 0.00001);
        QCOMPARE(prop.at("max").get<double>(), 1.0);
        break;
    }
    QVERIFY2(found, "gain float property not returned");
}

void QueryTools_Test::queryRgbAlgorithms_invalidMatrixTypeReturnsError()
{
    auto tm = makeQueryToolManager(m_doc);
    const Json result = parsedToolResult(tm.invoke("query_rgb_algorithms", Json{{"matrixType", "Bogus"}}));
    QVERIFY(result.is_object());
    QVERIFY(result.contains("error"));
    const QString message = QString::fromStdString(result.at("error").get<std::string>());
    QVERIFY2(message.contains("invalid value for 'matrixType'"), qPrintable(message));
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

void QueryTools_Test::queryFixtures_legacyShapeAndFilters()
{
    const QList<quint32> ids = addFixtureLayout(m_doc);
    QCOMPARE(ids.size(), 5);
    auto tm = makeQueryToolManager(m_doc);

    const Json all = parsedToolResult(tm.invoke("query_fixtures", Json::object()));
    QVERIFY(all.is_array());
    QCOMPARE(fixtureIds(all), QList<int>({int(ids[0]), int(ids[1]), int(ids[2]), int(ids[3]), int(ids[4])}));

    const Json byId = parsedToolResult(tm.invoke("query_fixtures", {{"id", ids[2]}}));
    QVERIFY2(byId.is_array(), byId.dump().c_str());
    QCOMPARE(fixtureIds(byId), QList<int>({int(ids[2])}));

    const Json byName = parsedToolResult(tm.invoke("query_fixtures", {{"name", "ALPHA"}}));
    QVERIFY2(byName.is_array(), byName.dump().c_str());
    QCOMPARE(fixtureIds(byName), QList<int>({int(ids[0]), int(ids[2]), int(ids[4])}));

    const Json byUniverse = parsedToolResult(tm.invoke("query_fixtures", {{"universe", 1}}));
    QVERIFY2(byUniverse.is_array(), byUniverse.dump().c_str());
    QCOMPARE(fixtureIds(byUniverse), QList<int>({int(ids[1]), int(ids[4])}));

    const Json combined = parsedToolResult(tm.invoke(
        "query_fixtures", {{"name", "alpha"}, {"universe", 1}}));
    QVERIFY2(combined.is_array(), combined.dump().c_str());
    QCOMPARE(fixtureIds(combined), QList<int>({int(ids[4])}));
}

void QueryTools_Test::queryFixtures_cursorPagination()
{
    const QList<quint32> ids = addFixtureLayout(m_doc);
    QCOMPARE(ids.size(), 5);
    auto tm = makeQueryToolManager(m_doc);

    Json first = parsedToolResult(tm.invoke("query_fixtures", {{"page", {{"limit", 2}}}}));
    QVERIFY2(first.is_object(), first.dump().c_str());
    QVERIFY(first.contains("total") && first["total"].is_number_integer());
    QVERIFY(first.contains("items") && first["items"].is_array());
    QVERIFY(first.contains("nextCursor"));
    QCOMPARE(first["total"].get<int>(), 5);
    QCOMPARE(fixtureIds(first["items"]), QList<int>({int(ids[0]), int(ids[1])}));
    QVERIFY(first["nextCursor"].is_string());

    Json second = parsedToolResult(tm.invoke("query_fixtures",
        {{"page", {{"limit", 2}, {"cursor", first["nextCursor"]}}}}));
    QVERIFY2(second.is_object(), second.dump().c_str());
    QVERIFY(second.contains("total") && second["total"].is_number_integer());
    QVERIFY(second.contains("items") && second["items"].is_array());
    QVERIFY(second.contains("nextCursor"));
    QCOMPARE(second["total"].get<int>(), 5);
    QCOMPARE(fixtureIds(second["items"]), QList<int>({int(ids[2]), int(ids[3])}));

    Json third = parsedToolResult(tm.invoke("query_fixtures",
        {{"page", {{"limit", 2}, {"cursor", second["nextCursor"]}}}}));
    QVERIFY2(third.is_object(), third.dump().c_str());
    QVERIFY(third.contains("total") && third["total"].is_number_integer());
    QVERIFY(third.contains("items") && third["items"].is_array());
    QVERIFY(third.contains("nextCursor"));
    QCOMPARE(third["total"].get<int>(), 5);
    QCOMPARE(fixtureIds(third["items"]), QList<int>({int(ids[4])}));
    QVERIFY(third["nextCursor"].is_null());
}

void QueryTools_Test::queryFixtures_invalidPage_data()
{
    QTest::addColumn<QByteArray>("arguments");
    QTest::newRow("zero-limit") << QByteArray(R"({"page":{"limit":0}})");
    QTest::newRow("excessive-limit") << QByteArray(R"({"page":{"limit":101}})");
    QTest::newRow("wrong-limit-type") << QByteArray(R"({"page":{"limit":"two"}})");
    QTest::newRow("invalid-cursor") << QByteArray(R"({"page":{"limit":2,"cursor":"not-a-cursor"}})");
}

void QueryTools_Test::queryFixtures_invalidPage()
{
    QFETCH(QByteArray, arguments);
    addFixtureLayout(m_doc);
    auto tm = makeQueryToolManager(m_doc);

    const Json result = parsedToolResult(
        tm.invoke("query_fixtures", Json::parse(arguments.constData())));
    QVERIFY(result.is_object());
    QVERIFY2(result.contains("error"), result.dump().c_str());
}

void QueryTools_Test::updateFixture_atomicAndIdempotent()
{
    const QList<quint32> ids = addFixtureLayout(m_doc);
    QCOMPARE(ids.size(), 5);
    Fixture *target = m_doc->fixture(ids[0]);
    const quint32 oldUniverseAddress = target->universeAddress();
    const quint32 channels = target->channels();
    auto tm = makeQueryToolManager(m_doc);
    QVERIFY(tm.has("update_fixture"));
    QSignalSpy changedSpy(m_doc, &Doc::fixtureChanged);

    const Json request = {
        {"id", ids[0]}, {"name", "Renamed Wash"}, {"universe", 3}, {"address", 40}
    };
    const Json updated = parsedToolResult(tm.invoke("update_fixture", request));
    QVERIFY2(updated.is_object(), updated.dump().c_str());
    QVERIFY(updated.contains("status") && updated["status"].is_string());
    QVERIFY(updated.contains("before") && updated["before"].is_object());
    QVERIFY(updated.contains("after") && updated["after"].is_object());
    QVERIFY(updated["before"].contains("id") && updated["before"]["id"].is_number_integer());
    QVERIFY(updated["after"].contains("id") && updated["after"]["id"].is_number_integer());
    QCOMPARE(updated["status"].get<std::string>(), std::string("updated"));
    QCOMPARE(updated["before"]["id"].get<quint32>(), ids[0]);
    QCOMPARE(updated["after"]["id"].get<quint32>(), ids[0]);
    QCOMPARE(target->name(), QString("Renamed Wash"));
    QCOMPARE(target->universe(), quint32(3));
    QCOMPARE(target->address(), quint32(40));
    QCOMPARE(changedSpy.size(), 1);
    for (quint32 offset = 0; offset < channels; ++offset)
    {
        QCOMPARE(m_doc->fixtureForAddress(oldUniverseAddress + offset), Fixture::invalidId());
        QCOMPARE(m_doc->fixtureForAddress(target->universeAddress() + offset), ids[0]);
    }

    changedSpy.clear();
    const Json unchanged = parsedToolResult(tm.invoke("update_fixture", request));
    QVERIFY2(unchanged.is_object(), unchanged.dump().c_str());
    QVERIFY(unchanged.contains("status") && unchanged["status"].is_string());
    QCOMPARE(unchanged["status"].get<std::string>(), std::string("unchanged"));
    QCOMPARE(fixtureState(target), updated["after"]);
    QCOMPARE(changedSpy.size(), 0);
}

void QueryTools_Test::updateFixture_invalidRequest_data()
{
    QTest::addColumn<QByteArray>("requestTemplate");

    QTest::newRow("unknown-id") << QByteArray(R"({"id":9999,"name":"Nope"})");
    QTest::newRow("unknown-field") << QByteArray(R"({"id":0,"bogus":1})");
    QTest::newRow("missing-update") << QByteArray(R"({"id":0})");
    QTest::newRow("wrong-id-type") << QByteArray(R"({"id":"zero","name":"Nope"})");
    QTest::newRow("wrong-name-type") << QByteArray(R"({"id":0,"name":42})");
    QTest::newRow("negative-universe") << QByteArray(R"({"id":0,"universe":-1})");
    QTest::newRow("address-outside") << QByteArray(R"({"id":0,"address":512})");
    QTest::newRow("footprint-overflow") << QByteArray(R"({"id":0,"address":510})");
    QTest::newRow("occupied-range") << QByteArray(R"({"id":0,"universe":1,"address":100})");
}

void QueryTools_Test::updateFixture_invalidRequest()
{
    QFETCH(QByteArray, requestTemplate);
    const QList<quint32> ids = addFixtureLayout(m_doc);
    QCOMPARE(ids.size(), 5);
    Fixture *target = m_doc->fixture(ids[0]);
    const Json before = fixtureState(target);
    const quint32 oldUniverseAddress = target->universeAddress();
    const quint32 blockerAddress = m_doc->fixture(ids[4])->universeAddress();
    const quint32 oldOwner = m_doc->fixtureForAddress(oldUniverseAddress);
    const quint32 blockerOwner = m_doc->fixtureForAddress(blockerAddress);
    QSignalSpy changedSpy(m_doc, &Doc::fixtureChanged);
    auto tm = makeQueryToolManager(m_doc);
    QVERIFY(tm.has("update_fixture"));

    Json request = Json::parse(requestTemplate.constData());
    if (request.contains("id") && request["id"].is_number_integer() &&
        request["id"].get<int>() == 0)
        request["id"] = ids[0];

    const Json result = parsedToolResult(tm.invoke("update_fixture", request));
    QVERIFY(result.is_object());
    QVERIFY2(result.contains("error"), result.dump().c_str());
    QCOMPARE(fixtureState(target), before);
    QCOMPARE(m_doc->fixtureForAddress(oldUniverseAddress), oldOwner);
    QCOMPARE(m_doc->fixtureForAddress(blockerAddress), blockerOwner);
    QCOMPARE(changedSpy.size(), 0);
}

void QueryTools_Test::patchFixtures_schemaDescribesExactMatch()
{
    auto tm = makeQueryToolManager(m_doc);
    const Json schema = tm.input_schema_for("patch_fixtures");
    QVERIFY2(schema.is_object(), schema.dump().c_str());
    QVERIFY(schema.contains("properties") && schema["properties"].is_object());
    const Json &rootProperties = schema["properties"];
    QVERIFY(rootProperties.contains("items") && rootProperties["items"].is_object());
    const Json &itemsSchema = rootProperties["items"];
    QVERIFY(itemsSchema.contains("items") && itemsSchema["items"].is_object());
    const Json &itemSchema = itemsSchema["items"];
    QVERIFY(itemSchema.contains("properties") && itemSchema["properties"].is_object());
    const Json &properties = itemSchema["properties"];
    for (const char *field : {"universe", "address", "quantity"})
        QVERIFY(properties.contains(field) && properties[field].is_object());
    QVERIFY(properties["universe"].contains("minimum") &&
            properties["universe"]["minimum"].is_number_integer());
    QVERIFY(properties["address"].contains("minimum") &&
            properties["address"]["minimum"].is_number_integer());
    QVERIFY(properties["address"].contains("maximum") &&
            properties["address"]["maximum"].is_number_integer());
    QVERIFY(properties["quantity"].contains("minimum") &&
            properties["quantity"]["minimum"].is_number_integer());
    QCOMPARE(properties["universe"]["minimum"].get<int>(), 0);
    QCOMPARE(properties["address"]["minimum"].get<int>(), 0);
    QCOMPARE(properties["address"]["maximum"].get<int>(), 511);
    QCOMPARE(properties["quantity"]["minimum"].get<int>(), 1);

    const std::string description = tm.get("patch_fixtures").description().value_or("");
    QVERIFY(QString::fromStdString(description).contains("exact", Qt::CaseInsensitive));
    QVERIFY(!QString::fromStdString(description).contains("upsert", Qt::CaseInsensitive));
}

void QueryTools_Test::patchFixtures_invalidBounds_data()
{
    QTest::addColumn<QByteArray>("itemJson");
    QTest::addColumn<QString>("field");

    QTest::newRow("negative-universe")
        << QByteArray(R"({"manufacturer":"Missing","model":"Missing","mode":"Missing","name":"Bad","universe":-1,"address":0})")
        << QString("universe");
    QTest::newRow("excessive-universe")
        << QByteArray(R"({"manufacturer":"Missing","model":"Missing","mode":"Missing","name":"Bad","universe":128,"address":0})")
        << QString("universe");
    QTest::newRow("negative-address")
        << QByteArray(R"({"manufacturer":"Missing","model":"Missing","mode":"Missing","name":"Bad","universe":0,"address":-1})")
        << QString("address");
    QTest::newRow("excessive-address")
        << QByteArray(R"({"manufacturer":"Missing","model":"Missing","mode":"Missing","name":"Bad","universe":0,"address":512})")
        << QString("address");
    QTest::newRow("zero-quantity")
        << QByteArray(R"({"manufacturer":"Missing","model":"Missing","mode":"Missing","name":"Bad","universe":0,"address":0,"quantity":0})")
        << QString("quantity");
}

void QueryTools_Test::patchFixtures_invalidBounds()
{
    QFETCH(QByteArray, itemJson);
    QFETCH(QString, field);
    auto tm = makeQueryToolManager(m_doc);
    QVERIFY(tm.has("patch_fixtures"));

    const Json result = parsedToolResult(tm.invoke(
        "patch_fixtures", {{"items", Json::array({Json::parse(itemJson.constData())})}}));
    QVERIFY(result.is_array());
    QCOMPARE(result.size(), size_t(1));
    QVERIFY2(result[0].contains("error"), result.dump().c_str());
    QVERIFY(QString::fromStdString(result[0]["error"].get<std::string>())
                .contains(field, Qt::CaseInsensitive));
    QCOMPARE(m_doc->fixturesCount(), 0);
}

void QueryTools_Test::patchFixtures_quantityIsAtomicPerItem()
{
    auto *def = new QLCFixtureDef();
    def->setManufacturer("McpTest");
    def->setModel("Par4");
    auto *mode = new QLCFixtureMode(def);
    mode->setName("4ch");
    for (int i = 0; i < 4; i++)
    {
        auto *ch = new QLCChannel();
        ch->setName(QString("Ch%1").arg(i));
        def->addChannel(ch);
        mode->insertChannel(ch, i);
    }
    def->addMode(mode);
    QVERIFY(m_doc->fixtureDefCache()->addFixtureDef(def));
    addFixtureLayout(m_doc); // "Alpha Wash" occupies universe 0, 10-13
    const int before = m_doc->fixturesCount();
    m_doc->resetModified();
    auto tm = makeQueryToolManager(m_doc);

    const Json base = {{"manufacturer", "McpTest"}, {"model", "Par4"}, {"mode", "4ch"}, {"universe", 0}};
    Json overlapping = base; overlapping["name"] = "Clash"; overlapping["address"] = 0; overlapping["quantity"] = 4;
    Json valid = base; valid["name"] = "Row"; valid["address"] = 200; valid["quantity"] = 2;
    Json badName = base; badName["name"] = 5; badName["address"] = 300;

    const Json result = parsedToolResult(tm.invoke("patch_fixtures",
        {{"items", Json::array({overlapping, valid, badName})}}));
    QVERIFY2(result.is_array() && result.size() == 3, result.dump().c_str());

    QCOMPARE(result[0].value("index", -1), 0);
    QCOMPARE(result[0].value("status", std::string()), std::string("error"));
    QVERIFY2(QString::fromStdString(result[0].value("error", "")).contains("overlap"), result.dump().c_str());

    QCOMPARE(result[1].value("index", -1), 1);
    QCOMPARE(result[1].value("status", std::string()), std::string("ok"));
    QVERIFY2(result[1].contains("fixtures") && result[1]["fixtures"].size() == 2, result.dump().c_str());
    QCOMPARE(result[1]["fixtures"][1].value("address", -1), 204);
    QCOMPARE(result[1]["fixtures"][1].value("outcome", std::string()), std::string("created"));

    QCOMPARE(result[2].value("index", -1), 2);
    QCOMPARE(result[2].value("status", std::string()), std::string("error"));
    QVERIFY(QString::fromStdString(result[2].value("error", "")).contains("name"));

    QCOMPARE(m_doc->fixturesCount(), before + 2);
    QVERIFY(m_doc->isModified());
}

void QueryTools_Test::queryFixtureChannels_invalidFixtureID_data()
{
    QTest::addColumn<QString>("tool");
    QTest::addColumn<QByteArray>("idJson");

    for (const char *tool : {"query_fixture_channels", "read_dmx_values", "convert_degrees_to_dmx"})
    {
        QTest::newRow(qPrintable(QString("%1 fractional").arg(tool))) << tool << QByteArray("0.5");
        QTest::newRow(qPrintable(QString("%1 above uint32").arg(tool))) << tool << QByteArray("4294967296");
        QTest::newRow(qPrintable(QString("%1 negative").arg(tool))) << tool << QByteArray("-1");
        QTest::newRow(qPrintable(QString("%1 boolean").arg(tool))) << tool << QByteArray("true");
        QTest::newRow(qPrintable(QString("%1 string").arg(tool))) << tool << QByteArray("\"0\"");
    }
}

void QueryTools_Test::queryFixtureChannels_invalidFixtureID()
{
    QFETCH(QString, tool);
    QFETCH(QByteArray, idJson);
    addFixtureLayout(m_doc);
    fastmcpp::tools::ToolManager tm;
    registerChannelTools(tm, m_doc);

    const Json id = Json::parse(idJson.constData());
    if (tool == "convert_degrees_to_dmx")
    {
        // Batch tool: one indexed error record, no conversion for fixture 0/1.
        const Json result = parsedToolResult(tm.invoke(tool.toStdString(),
            {{"items", Json::array({{{"fixtureID", id}, {"panDegrees", 10}}})}}));
        QVERIFY2(result.is_array() && result.size() == 1, result.dump().c_str());
        QCOMPARE(result[0].value("status", std::string()), std::string("error"));
        QCOMPARE(result[0].value("index", -1), 0);
        QVERIFY(QString::fromStdString(result[0].value("error", "")).contains("fixtureID"));
        return;
    }
    const Json result = parsedToolResult(tm.invoke(
        tool.toStdString(), {{"fixtureIDs", Json::array({id})}}));
    QVERIFY2(result.is_object() && result.contains("error"), result.dump().c_str());
    QVERIFY(QString::fromStdString(result["error"].get<std::string>()).contains("fixtureIDs"));
}

void QueryTools_Test::configureChannels_invalidReference_data()
{
    QTest::addColumn<QString>("tool");
    QTest::addColumn<QByteArray>("itemJson");
    QTest::addColumn<QString>("field");

    // Fixture 0 ("Alpha Wash") has 4 channels.
    for (const char *tool : {"configure_channels", "set_channel_modifiers"})
    {
        const QByteArray tail = QByteArray(tool) == "configure_channels"
            ? QByteArray(R"("precedence":"htp")") : QByteArray(R"("modifierName":"none")");
        QTest::newRow(qPrintable(QString("%1 fractional fixture").arg(tool)))
            << tool << (R"({"fixtureID":0.5,"channel":0,)" + tail + "}") << "fixtureID";
        QTest::newRow(qPrintable(QString("%1 oversized fixture").arg(tool)))
            << tool << (R"({"fixtureID":4294967296,"channel":0,)" + tail + "}") << "fixtureID";
        QTest::newRow(qPrintable(QString("%1 fractional channel").arg(tool)))
            << tool << (R"({"fixtureID":0,"channel":1.5,)" + tail + "}") << "channel";
        QTest::newRow(qPrintable(QString("%1 channel beyond fixture").arg(tool)))
            << tool << (R"({"fixtureID":0,"channel":4,)" + tail + "}") << "channel";
        QTest::newRow(qPrintable(QString("%1 negative channel").arg(tool)))
            << tool << (R"({"fixtureID":0,"channel":-1,)" + tail + "}") << "channel";
    }
    QTest::newRow("configure_channels precedence not string")
        << "configure_channels" << QByteArray(R"({"fixtureID":0,"channel":0,"precedence":5})") << "precedence";
    QTest::newRow("configure_channels precedence unknown")
        << "configure_channels" << QByteArray(R"({"fixtureID":0,"channel":0,"precedence":"loud"})") << "precedence";
}

void QueryTools_Test::configureChannels_invalidReference()
{
    QFETCH(QString, tool);
    QFETCH(QByteArray, itemJson);
    QFETCH(QString, field);
    addFixtureLayout(m_doc);
    m_doc->resetModified();
    fastmcpp::tools::ToolManager tm;
    registerChannelTools(tm, m_doc);

    const Json result = parsedToolResult(tm.invoke(
        tool.toStdString(), {{"items", Json::array({Json::parse(itemJson.constData())})}}));
    QVERIFY2(result.is_array() && result.size() == 1, result.dump().c_str());
    QVERIFY2(result[0].value("status", "") == "error", result.dump().c_str());
    QCOMPARE(result[0].value("index", -1), 0);
    QVERIFY2(QString::fromStdString(result[0].value("error", "")).contains(field), result.dump().c_str());
    Fixture *fxi = m_doc->fixture(0);
    QVERIFY(fxi->forcedHTPChannels().isEmpty());
    QVERIFY(!m_doc->isModified());
}

void QueryTools_Test::configureChannels_precedenceAndModified_data()
{
    QTest::addColumn<QString>("precedence");
    QTest::addColumn<bool>("htp");
    QTest::addColumn<bool>("ltp");

    QTest::newRow("htp") << "htp" << true << false;
    QTest::newRow("HTP") << "HTP" << true << false;
    QTest::newRow("Ltp") << "Ltp" << false << true;
}

void QueryTools_Test::configureChannels_precedenceAndModified()
{
    QFETCH(QString, precedence);
    QFETCH(bool, htp);
    QFETCH(bool, ltp);
    addFixtureLayout(m_doc);
    m_doc->resetModified();
    fastmcpp::tools::ToolManager tm;
    registerChannelTools(tm, m_doc);

    const Json result = parsedToolResult(tm.invoke("configure_channels", {{"items", Json::array({
        {{"fixtureID", 0}, {"channel", 2}, {"precedence", precedence.toStdString()}}})}}));
    QVERIFY2(result.is_array() && result[0].value("status", "") == "ok", result.dump().c_str());
    Fixture *fxi = m_doc->fixture(0);
    QCOMPARE(fxi->forcedHTPChannels().contains(2), htp);
    QCOMPARE(fxi->forcedLTPChannels().contains(2), ltp);
    QVERIFY2(m_doc->isModified(), "channel configuration must mark the workspace modified");
}

void QueryTools_Test::setChannelModifiers_marksModified()
{
    addFixtureLayout(m_doc);
    m_doc->resetModified();
    fastmcpp::tools::ToolManager tm;
    registerChannelTools(tm, m_doc);

    const Json result = parsedToolResult(tm.invoke("set_channel_modifiers", {{"items", Json::array({
        {{"fixtureID", 0}, {"channel", 1}, {"modifierName", "none"}}})}}));
    QVERIFY2(result.is_array() && result[0].value("status", "") == "ok", result.dump().c_str());
    QVERIFY2(m_doc->isModified(), "modifier changes must mark the workspace modified");
}

void QueryTools_Test::channelConfig_survivesSaveReload_data()
{
    QTest::addColumn<QString>("tool");
    QTest::addColumn<QByteArray>("itemJson");
    QTest::addColumn<bool>("canFade");
    QTest::addColumn<bool>("htp");
    QTest::addColumn<bool>("ltp");
    QTest::addColumn<QString>("modifier");

    QTest::newRow("canFade false") << "configure_channels"
        << QByteArray(R"({"fixtureID":0,"channel":2,"canFade":false})") << false << false << false << QString();
    QTest::newRow("precedence htp") << "configure_channels"
        << QByteArray(R"({"fixtureID":0,"channel":2,"precedence":"htp"})") << true << true << false << QString();
    QTest::newRow("precedence ltp + canFade false") << "configure_channels"
        << QByteArray(R"({"fixtureID":0,"channel":2,"precedence":"ltp","canFade":false})") << false << false << true << QString();
    QTest::newRow("modifier") << "set_channel_modifiers"
        << QByteArray(R"({"fixtureID":0,"channel":2,"modifierName":"MCP Test Invert"})") << true << false << false
        << QString("MCP Test Invert");
}

void QueryTools_Test::channelConfig_survivesSaveReload()
{
    QFETCH(QString, tool);
    QFETCH(QByteArray, itemJson);
    QFETCH(bool, canFade);
    QFETCH(bool, htp);
    QFETCH(bool, ltp);
    QFETCH(QString, modifier);

    auto addTemplate = [](Doc *doc) {
        auto *mod = new ChannelModifier();
        mod->setName("MCP Test Invert");
        mod->setModifierMap({{0, 255}, {255, 0}});
        doc->modifiersCache()->addModifier(mod);
    };
    addTemplate(m_doc);
    addFixtureLayout(m_doc);
    fastmcpp::tools::ToolManager tm;
    registerChannelTools(tm, m_doc);
    const Json result = parsedToolResult(tm.invoke(tool.toStdString(),
        {{"items", Json::array({Json::parse(itemJson.constData())})}}));
    QVERIFY2(result.is_array() && result[0].value("status", "") == "ok", result.dump().c_str());

    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly);
    QXmlStreamWriter writer(&buffer);
    writer.writeStartDocument();
    QVERIFY(m_doc->saveXML(&writer));
    writer.writeEndDocument();
    buffer.close();

    Doc reloaded(this);
    addTemplate(&reloaded);
    QXmlStreamReader reader(data);
    reader.readNextStartElement();
    QVERIFY(reloaded.loadXML(reader));

    Fixture *fxi = reloaded.fixture(0);
    QVERIFY(fxi != nullptr);
    QCOMPARE(fxi->channelCanFade(2), canFade);
    QCOMPARE(fxi->forcedHTPChannels().contains(2), htp);
    QCOMPARE(fxi->forcedLTPChannels().contains(2), ltp);
    ChannelModifier *mod = fxi->channelModifier(2);
    QCOMPARE(mod ? mod->name() : QString(), modifier);
}

void QueryTools_Test::transport_toolResultEncoding_data()
{
    QTest::addColumn<QString>("tool");
    QTest::addColumn<QByteArray>("argsJson");
    QTest::addColumn<bool>("isError");
    QTest::addColumn<bool>("structured");

    QTest::newRow("request error object") << "query_fixture_channels"
        << QByteArray(R"({"fixtureIDs":[0.5]})") << true << true;
    QTest::newRow("query array success") << "query_fixture_channels"
        << QByteArray(R"({"fixtureIDs":[0]})") << false << false;
    QTest::newRow("batch all failed") << "configure_channels"
        << QByteArray(R"({"items":[{"fixtureID":0,"channel":99}]})") << true << false;
    QTest::newRow("batch mixed") << "configure_channels"
        << QByteArray(R"({"items":[{"fixtureID":0,"channel":0,"canFade":false},{"fixtureID":0,"channel":99}]})")
        << false << false;
    QTest::newRow("batch all ok") << "configure_channels"
        << QByteArray(R"({"items":[{"fixtureID":0,"channel":0,"canFade":true}]})") << false << false;
}

void QueryTools_Test::transport_toolResultEncoding()
{
    QFETCH(QString, tool);
    QFETCH(QByteArray, argsJson);
    QFETCH(bool, isError);
    QFETCH(bool, structured);
    addFixtureLayout(m_doc);

    fastmcpp::server::Server server("qlcplus", "5.0.0");
    fastmcpp::tools::ToolManager tm;
    fastmcpp::resources::ResourceManager rm;
    fastmcpp::prompts::PromptManager pm;
    registerChannelTools(tm, m_doc);
    auto handler = mcp::withToolResultEncoding(
        fastmcpp::mcp::make_mcp_handler("qlcplus", "5.0.0", server, tm, rm, pm));

    const Json response = handler({{"jsonrpc", "2.0"}, {"id", 7}, {"method", "tools/call"},
        {"params", {{"name", tool.toStdString()}, {"arguments", Json::parse(argsJson.constData())}}}});
    QVERIFY2(response.contains("result"), response.dump().c_str());
    const Json &result = response["result"];
    QVERIFY2(result["content"].is_array() && result["content"].size() == 1, response.dump().c_str());
    const Json payload = Json::parse(result["content"][0]["text"].get<std::string>());
    QVERIFY2(result.contains("isError") && result["isError"] == isError, response.dump().c_str());
    QCOMPARE(result.contains("structuredContent"), structured);
    if (structured)
        QCOMPARE(result["structuredContent"], payload);
}

QTEST_MAIN(QueryTools_Test)