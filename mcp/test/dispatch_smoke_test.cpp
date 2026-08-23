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

void DispatchSmoke_Test::dispatchSmoke_liveControlTools_notRegistered()
{
    fastmcpp::tools::ToolManager tm;
    registerFunctionTools(tm, m_doc);
    registerIOTools(tm, m_doc);

    QVERIFY(!tm.has("set_grand_master"));
    QVERIFY(!tm.has("update_scene_from_dmx"));
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
