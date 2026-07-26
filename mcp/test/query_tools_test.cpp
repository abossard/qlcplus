/*
  Q Light Controller Plus - Unit test
*/

#include <QtTest>
#include <QSignalSpy>

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

QTEST_MAIN(QueryTools_Test)