/*
  Q Light Controller Plus - Unit test
  dispatch_smoke_test.cpp
*/

#include <QtTest>
#include <nlohmann/json.hpp>

#include "dispatch_smoke_test.h"
#include "tool_registry.h"
#include "doc.h"
#include "scene.h"
#include "qlcpalette.h"
#include "function.h"
#include "fixturegroup.h"
#include "rgbmatrix.h"
#include "huematrix.h"
#include "rgbalgorithm.h"
#include "huescript.h"
#include "rgbscriptscache.h"
#include "huescriptscache.h"

#include <fastmcpp/tools/manager.hpp>

using Json = nlohmann::json;

namespace {

Json parsedToolResult(const Json &value)
{
    if (value.is_string())
        return Json::parse(value.get<std::string>());
    return value;
}

}

void DispatchSmoke_Test::init()
{
    m_doc = new Doc(this);
}

void DispatchSmoke_Test::cleanup()
{
    delete m_doc;
    m_doc = nullptr;
}

// ─── query family ──────────────────────────────────────────────────────────

void DispatchSmoke_Test::dispatchSmoke_queryFixtures_emptyDoc_returnsArray()
{
    fastmcpp::tools::ToolManager tm;
    registerQueryTools(tm, m_doc, nullptr);

    Json result = parsedToolResult(tm.invoke("query_fixtures", Json::object()));
    QVERIFY2(result.is_array(), "query_fixtures must return an array");
    QCOMPARE(result.size(), (size_t)0);
}

// ─── function family — create_scenes ───────────────────────────────────────

void DispatchSmoke_Test::dispatchSmoke_createScenes_validItem_createsInDoc()
{
    fastmcpp::tools::ToolManager tm;
    registerFunctionTools(tm, m_doc);

    Json args = {{"items", Json::array({{{"name", "SmokeScene"}}})}};
    Json result = parsedToolResult(tm.invoke("create_scenes", args));

    QVERIFY2(result.is_array(), "create_scenes must return an array");
    QCOMPARE(result.size(), (size_t)1);
    QVERIFY2(!result[0].contains("error"), result[0].dump().c_str());
    QVERIFY(result[0].contains("id"));

    // Side-effect: scene exists in Doc.
    bool found = false;
    for (Function *fn : m_doc->functions())
    {
        if (fn && fn->type() == Function::SceneType && fn->name() == "SmokeScene")
        { found = true; break; }
    }
    QVERIFY2(found, "SmokeScene was not added to Doc");
}

// ─── palette family — create_palettes ──────────────────────────────────────

void DispatchSmoke_Test::dispatchSmoke_createRgbMatrices_typeSchema()
{
    fastmcpp::tools::ToolManager tm;
    registerFunctionTools(tm, m_doc);
    const Json schema = tm.input_schema_for("create_rgb_matrices");
    const Json &item = schema.at("properties").at("items").at("items");
    QVERIFY(item.at("properties").contains("type"));
    const Json &type = item.at("properties").at("type");
    QVERIFY(type.at("enum") == Json::array({"RGBMatrix", "HUEMatrix"}));
    QCOMPARE(type.at("default").get<std::string>(), std::string("HUEMatrix"));
    QVERIFY(item.at("required") == Json::array({"name"}));
}

void DispatchSmoke_Test::dispatchSmoke_createRgbMatrices_type_data()
{
    QTest::addColumn<QString>("matrixType");
    QTest::addColumn<QString>("algorithm");
    QTest::addColumn<bool>("hue");
    QTest::addColumn<int>("algorithmType");
    QTest::newRow("default-hue") << QString() << QStringLiteral("Audio Spectrum Bars") << true << int(RGBAlgorithm::Script);
    QTest::newRow("explicit-hue") << QStringLiteral("HUEMatrix") << QStringLiteral("Audio Spectrum Bars") << true << int(RGBAlgorithm::Script);
    QTest::newRow("stock-script") << QStringLiteral("RGBMatrix") << QStringLiteral("Stripes") << false << int(RGBAlgorithm::Script);
    QTest::newRow("stock-audio") << QStringLiteral("RGBMatrix") << QStringLiteral("Audio Spectrum") << false << int(RGBAlgorithm::Audio);
    QTest::newRow("legacy-hue-stripes") << QStringLiteral("HUEMatrix") << QStringLiteral("Stripes") << true << int(RGBAlgorithm::Script);
    QTest::newRow("legacy-hue-audio") << QStringLiteral("HUEMatrix") << QStringLiteral("Audio Spectrum") << true << int(RGBAlgorithm::Audio);
    QTest::newRow("hue-fire") << QStringLiteral("HUEMatrix") << QStringLiteral("Audio Fire") << true << int(RGBAlgorithm::Script);
    QTest::newRow("case-insensitive-rgb") << QStringLiteral("rgbmatrix") << QStringLiteral("Stripes") << false << int(RGBAlgorithm::Script);
}

void DispatchSmoke_Test::dispatchSmoke_createRgbMatrices_type()
{
    QFETCH(QString, matrixType);
    QFETCH(QString, algorithm);
    QFETCH(bool, hue);
    QFETCH(int, algorithmType);
    const QDir rgbDir(QFINDTESTDATA("../../resources/rgbscripts"));
    QVERIFY(m_doc->rgbScriptsCache()->load(rgbDir));
    QVERIFY(m_doc->hueScriptsCache()->load(rgbDir, false));
    QVERIFY(m_doc->hueScriptsCache()->load(QDir(QFINDTESTDATA("../../resources/huescripts")), true));
    fastmcpp::tools::ToolManager tm;
    registerFunctionTools(tm, m_doc);
    Json item = {{"name", "Typed matrix"}, {"algorithm", algorithm.toStdString()}};
    if (!matrixType.isEmpty())
        item["type"] = matrixType.toStdString();
    if (algorithm == QStringLiteral("Audio Spectrum Bars"))
        item["properties"] = {{"rgb_mix", 4}};
    const Json result = parsedToolResult(tm.invoke("create_rgb_matrices", Json{{"items", Json::array({item})}}));
    QVERIFY(result.is_array());
    QCOMPARE(result.size(), size_t(1));
    QVERIFY2(!result[0].contains("error"), result[0].dump().c_str());
    auto *matrix = qobject_cast<RGBMatrix *>(m_doc->function(result[0].at("id").get<int>()));
    QVERIFY(matrix);
    QCOMPARE(matrix->type(), hue ? Function::HUEMatrixType : Function::RGBMatrixType);
    QCOMPARE(qobject_cast<HUEMatrix *>(matrix) != nullptr, hue);
    QVERIFY(matrix->algorithm());
    QCOMPARE(matrix->algorithm()->name(), algorithm);
    QCOMPARE(int(matrix->algorithm()->type()), algorithmType);
    if (algorithmType == RGBAlgorithm::Script)
        QCOMPARE(dynamic_cast<HUEScript *>(matrix->algorithm()) != nullptr, hue);
    if (algorithm == QStringLiteral("Audio Spectrum Bars"))
        QCOMPARE(matrix->property("rgb_mix"), QStringLiteral("4"));
}

void DispatchSmoke_Test::dispatchSmoke_createRgbMatrices_rejectedBeforeMutation_data()
{
    QTest::addColumn<QString>("matrixType");
    QTest::addColumn<QString>("algorithm");
    QTest::addColumn<bool>("upsert");
    for (bool upsert : {false, true})
    {
        const QByteArray suffix = upsert ? "-upsert" : "-new";
        QTest::newRow(("unknown-type" + suffix).constData())
            << QStringLiteral("Bogus") << QStringLiteral("Stripes") << upsert;
        QTest::newRow(("hue-only-on-rgb" + suffix).constData())
            << QStringLiteral("RGBMatrix") << QStringLiteral("Audio Spectrum Bars") << upsert;
        QTest::newRow(("unknown-on-rgb" + suffix).constData())
            << QStringLiteral("RGBMatrix") << QStringLiteral("Missing algorithm") << upsert;
        QTest::newRow(("unknown-on-hue" + suffix).constData())
            << QStringLiteral("HUEMatrix") << QStringLiteral("Missing algorithm") << upsert;
    }
}

void DispatchSmoke_Test::dispatchSmoke_createRgbMatrices_rejectedBeforeMutation()
{
    QFETCH(QString, matrixType);
    QFETCH(QString, algorithm);
    QFETCH(bool, upsert);
    const QDir rgbDir(QFINDTESTDATA("../../resources/rgbscripts"));
    QVERIFY(m_doc->rgbScriptsCache()->load(rgbDir));
    QVERIFY(m_doc->hueScriptsCache()->load(rgbDir, false));
    QVERIFY(m_doc->hueScriptsCache()->load(QDir(QFINDTESTDATA("../../resources/huescripts")), true));
    auto *originalGroup = new FixtureGroup(m_doc);
    auto *replacementGroup = new FixtureGroup(m_doc);
    QVERIFY(m_doc->addFixtureGroup(originalGroup));
    QVERIFY(m_doc->addFixtureGroup(replacementGroup));
    fastmcpp::tools::ToolManager tm;
    registerFunctionTools(tm, m_doc);
    RGBMatrix *existing = nullptr;
    RGBAlgorithm *originalAlgorithm = nullptr;
    if (upsert)
    {
        const Json seed = {{"name", "Rejected matrix"},
                           {"type", matrixType == "RGBMatrix" ? "RGBMatrix" : "HUEMatrix"},
                           {"algorithm", "Stripes"}, {"fixtureGroupID", originalGroup->id()},
                           {"path", "Original"}};
        const Json created = parsedToolResult(tm.invoke("create_rgb_matrices", Json{{"items", Json::array({seed})}}));
        QVERIFY2(created.is_array() && !created[0].contains("error"), created.dump().c_str());
        existing = qobject_cast<RGBMatrix *>(m_doc->function(created[0].at("id").get<int>()));
        QVERIFY(existing);
        originalAlgorithm = existing->algorithm();
        QVERIFY(originalAlgorithm);
        QCOMPARE(originalAlgorithm->name(), QStringLiteral("Stripes"));
    }
    const int functionsBefore = m_doc->functions().size();
    const int matricesBefore = m_doc->findChildren<RGBMatrix *>().size();
    const Json rejected = {{"name", "Rejected matrix"}, {"type", matrixType.toStdString()},
                           {"algorithm", algorithm.toStdString()}, {"fixtureGroupID", replacementGroup->id()},
                           {"path", "Must not change"}};
    // A valid sibling proves that a rejected item does not abort the batch.
    const Json valid = {{"name", "Valid sibling"}, {"type", "RGBMatrix"}, {"algorithm", "Stripes"}};
    const Json result = parsedToolResult(tm.invoke("create_rgb_matrices", Json{{"items", Json::array({rejected, valid})}}));
    QVERIFY(result.is_array());
    QCOMPARE(result.size(), size_t(2));
    QVERIFY2(result[0].contains("error"), result[0].dump().c_str());
    const QString error = QString::fromStdString(result[0].at("error").get<std::string>());
    QVERIFY2(error.contains(matrixType == "Bogus" ? "'type'" : algorithm), qPrintable(error));
    QVERIFY2(!result[1].contains("error"), result[1].dump().c_str());
    QCOMPARE(m_doc->functions().size(), functionsBefore + 1);
    // Includes unregistered QObject children, detecting failed-new allocation leaks.
    QCOMPARE(m_doc->findChildren<RGBMatrix *>().size(), matricesBefore + 1);
    if (existing)
    {
        QCOMPARE(existing->algorithm(), originalAlgorithm);
        QCOMPARE(existing->algorithm()->name(), QStringLiteral("Stripes"));
        QCOMPARE(existing->fixtureGroup(), originalGroup->id());
        QCOMPARE(existing->path(true), QStringLiteral("Original"));
    }
}

void DispatchSmoke_Test::dispatchSmoke_createPalettes_validItem_exists()
{
    fastmcpp::tools::ToolManager tm;
    registerPaletteTools(tm, m_doc);

    Json args = {{"items", Json::array({
        {{"name", "SmokeDimmer"}, {"type", "Dimmer"}, {"value", 200}}
    })}};
    Json result = parsedToolResult(tm.invoke("create_palettes", args));

    QVERIFY(result.is_array());
    QCOMPARE(result.size(), (size_t)1);
    QVERIFY2(!result[0].contains("error"), result[0].dump().c_str());
    QCOMPARE(result[0]["status"].get<std::string>(), std::string("created"));

    // Side-effect: palette exists in Doc.
    bool found = false;
    for (QLCPalette *p : m_doc->palettes())
        if (p && p->name() == "SmokeDimmer") { found = true; break; }
    QVERIFY2(found, "SmokeDimmer palette not in Doc");
}

// ─── channel family — configure_channels ───────────────────────────────────

void DispatchSmoke_Test::dispatchSmoke_configureChannels_emptyDoc_returnsArray()
{
    fastmcpp::tools::ToolManager tm;
    registerChannelTools(tm, m_doc);

    // No fixtures patched — fixtureID 0 will produce a per-item "fixture not found" error,
    // but the tool should still return a parseable JSON array.
    Json args = {{"items", Json::array({
        {{"fixtureID", 0}, {"channel", 0}, {"precedence", "auto"}}
    })}};
    Json result = parsedToolResult(tm.invoke("configure_channels", args));

    QVERIFY2(result.is_array(), "configure_channels must return an array");
    QCOMPARE(result.size(), (size_t)1);
    QVERIFY(result[0].contains("error"));
}

// ─── IO family — configure_universes ───────────────────────────────────────

void DispatchSmoke_Test::dispatchSmoke_configureUniverses_validItem_returnsResult()
{
    fastmcpp::tools::ToolManager tm;
    registerIOTools(tm, m_doc);

    // Universe 0 always exists by default in Doc.
    Json args = {{"items", Json::array({
        {{"universeID", 0}, {"name", "SmokeUniverse"}}
    })}};
    Json result = parsedToolResult(tm.invoke("configure_universes", args));

    QVERIFY2(result.is_array(), "configure_universes must return an array");
    QCOMPARE(result.size(), (size_t)1);
    QVERIFY(result[0].contains("universeID"));
    QCOMPARE(result[0]["universeID"].get<int>(), 0);
}

// ─── unknown-field validation ──────────────────────────────────────────────

void DispatchSmoke_Test::dispatchSmoke_unknownField_returnsError()
{
    fastmcpp::tools::ToolManager tm;
    registerPaletteTools(tm, m_doc);

    Json args = {{"items", Json::array({
        {{"name", "Bad"}, {"type", "Dimmer"}, {"bogus_field", 1}}
    })}};
    Json result = parsedToolResult(tm.invoke("create_palettes", args));

    QVERIFY(result.is_array());
    QCOMPARE(result.size(), (size_t)1);
    QVERIFY2(result[0].contains("error"),
             "Unknown fields must produce per-item error");
}

void DispatchSmoke_Test::dispatchSmoke_setupToolsTouchingLiveOutput_registered()
{
    fastmcpp::tools::ToolManager tm;
    registerLiveTools(tm, m_doc);
    registerFunctionTools(tm, m_doc);
    registerIOTools(tm, m_doc);
    registerChannelTools(tm, m_doc);

    // The MCP surface exists for setup and configuration. Some of that cannot
    // be done without the rig responding — the Grand Master is a desk setting,
    // and verifying a patch means lighting the lamp — so these are in. What
    // stays out is show operation: no cue stepping, no timed playback.
    for (const char *name : {"set_grand_master", "query_grand_master", "set_blackout",
                             "write_dmx", "run_functions", "query_running_functions"})
        QVERIFY2(tm.has(name), name);

    // Show-operation shapes stay out. These are the names such tools would most
    // plausibly take, including the one this fork previously removed.
    for (const char *name : {"update_scene_from_dmx", "set_function_speed", "step_chaser",
                             "goto_cue", "fade_function"})
        QVERIFY2(!tm.has(name), name);
}

void DispatchSmoke_Test::dispatchSmoke_setupAndDiagnosticsTools_remainRegistered()
{
    fastmcpp::tools::ToolManager tm;
    registerQueryTools(tm, m_doc, nullptr);
    registerFunctionTools(tm, m_doc);
    registerIOTools(tm, m_doc);
    registerChannelTools(tm, m_doc);

    for (const char *name : {"query_fixtures", "patch_fixtures", "update_fixture", "create_scenes",
                            "configure_universes", "read_dmx_values"})
        QVERIFY2(tm.has(name), name);
}

void DispatchSmoke_Test::dispatchSmoke_workspaceTools_needBridge()
{
    fastmcpp::tools::ToolManager tm;
    registerWorkspaceTools(tm, m_doc, nullptr);

    // v4 has no workspace bridge, so these must stay absent rather than
    // register against a null bridge.
    for (const char *name : {"save_workspace", "load_workspace", "new_workspace",
                             "query_workspace_file"})
        QVERIFY2(!tm.has(name), name);
}

void DispatchSmoke_Test::dispatchSmoke_deleteTools_registered()
{
    fastmcpp::tools::ToolManager tm;
    registerQueryTools(tm, m_doc, nullptr);
    registerFunctionTools(tm, m_doc);
    registerIOTools(tm, m_doc);

    for (const char *name : {"delete_fixtures", "delete_fixture_groups", "delete_universes"})
        QVERIFY2(tm.has(name), name);
}

void DispatchSmoke_Test::dispatchSmoke_setupAndConfigTools_registered()
{
    fastmcpp::tools::ToolManager tm;
    registerStageTools(tm, m_doc);
    registerInputProfileTools(tm, m_doc);
    registerShowTools(tm, m_doc);

    for (const char *name : {"set_fixture_placement", "query_fixture_placement", "configure_stage",
                             "create_channel_groups", "query_channel_groups", "delete_channel_groups",
                             "create_input_profiles", "query_input_profile_channels",
                             "create_shows", "query_shows", "add_show_items", "delete_show_items"})
        QVERIFY2(tm.has(name), name);
}

void DispatchSmoke_Test::dispatchSmoke_deleteTools_emptyDoc_returnArrays()
{
    fastmcpp::tools::ToolManager tm;
    registerQueryTools(tm, m_doc, nullptr);
    registerFunctionTools(tm, m_doc);
    registerIOTools(tm, m_doc);

    for (const char *name : {"delete_fixtures", "delete_fixture_groups", "delete_universes"})
    {
        Json result = parsedToolResult(tm.invoke(name, Json{{"ids", Json::array({7})}}));
        QVERIFY2(result.is_array(), name);
        QCOMPARE(result.size(), (size_t)1);
    }
}

QTEST_MAIN(DispatchSmoke_Test)
