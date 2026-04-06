/*
  Q Light Controller Plus
  query_tools.cpp

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

#include "tool_registry.h"
#include "conversions.h"
#include "idempotency.h"
#include "vcbridge.h"
#include "vc_query_helpers.h"
#include "doc.h"
#include "qlcfixturedefcache.h"
#include "qlcfixturemode.h"
#include "qlcpalette.h"
#include "scene.h"
#include "inputoutputmap.h"
#include "inputpatch.h"
#include "outputpatch.h"
#include "universe.h"

#include <fastmcpp/tools/manager.hpp>
#include <fastmcpp/tools/tool.hpp>

using Json = nlohmann::json;

void registerQueryTools(fastmcpp::tools::ToolManager &tm, Doc *doc, VCBridge *vcBridge)
{
    using Tool = fastmcpp::tools::Tool;

    // query_fixtures — list all patched fixtures
    tm.register_tool(Tool(
        "query_fixtures",
        Json{{"type", "object"}, {"properties", Json::object()}},
        Json{},
        [doc](const Json &) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            for (Fixture *fxi : doc->fixtures())
                results.push_back(mcp::fixtureToJson(fxi));
            return results.dump();
            });
        },
        std::nullopt,
        std::string("List all patched fixtures with capabilities and physical properties. Returns IDs needed for other tools."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotReadOnly));

    // query_available_fixtures — search fixture definition library
    tm.register_tool(Tool(
        "query_available_fixtures",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"manufacturer", {{"type", "string"}, {"description", "Filter by manufacturer name (substring match)"}}},
                {"model", {{"type", "string"}, {"description", "Filter by model name (substring match)"}}}
            }}}}}}
        }}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            auto items = args.value("items", Json::array());
            if (items.empty())
                items = Json::array({Json::object()});

            for (auto &filter : items)
            {
                auto err = validateFields(filter, {"manufacturer", "model"});
                if (!err.empty()) { results.push_back(Json::parse(err)); continue; }

                QString mfgFilter = filter.contains("manufacturer")
                    ? QString::fromStdString(filter.at("manufacturer").get<std::string>()) : "";
                QString modelFilter = filter.contains("model")
                    ? QString::fromStdString(filter.at("model").get<std::string>()) : "";

                for (const QString &mfg : doc->fixtureDefCache()->manufacturers())
                {
                    if (!mfgFilter.isEmpty() && !mfg.contains(mfgFilter, Qt::CaseInsensitive))
                        continue;
                    for (const QString &model : doc->fixtureDefCache()->models(mfg))
                    {
                        if (!modelFilter.isEmpty() && !model.contains(modelFilter, Qt::CaseInsensitive))
                            continue;
                        const QLCFixtureDef *def = doc->fixtureDefCache()->fixtureDef(mfg, model);
                        if (!def) continue;

                        Json entry;
                        entry["manufacturer"] = mfg.toStdString();
                        entry["model"] = model.toStdString();
                        entry["type"] = QLCFixtureDef::typeToString(def->type()).toStdString();
                        Json modes = Json::array();
                        for (const QLCFixtureMode *mode : const_cast<QLCFixtureDef*>(def)->modes())
                        {
                            modes.push_back({
                                {"name", mode->name().toStdString()},
                                {"channels", (int)mode->channels().size()}
                            });
                        }
                        entry["modes"] = modes;
                        results.push_back(entry);
                    }
                }
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("Search the fixture definition library by manufacturer/model. Returns available fixtures with their modes. Batch."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotReadOnly));

    // patch_fixtures — add fixtures to the project (batch)
    tm.register_tool(Tool(
        "patch_fixtures",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"manufacturer", {{"type", "string"}}},
                {"model", {{"type", "string"}}},
                {"mode", {{"type", "string"}, {"description", "Fixture mode name (use query_available_fixtures to discover modes)"}}},
                {"name", {{"type", "string"}, {"description", "Base name for the fixture(s)"}}},
                {"universe", {{"type", "integer"}}},
                {"address", {{"type", "integer"}, {"description", "DMX start address (0-based)"}}},
                {"quantity", {{"type", "integer"}, {"description", "Number of fixtures to patch (default 1)"}}}
            }}, {"required", {"manufacturer", "model", "mode", "name", "universe", "address"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            for (auto &item : args.at("items"))
            {
                auto err = validateFields(item, {"manufacturer", "model", "mode", "name", "universe", "address", "quantity"});
                if (!err.empty()) { results.push_back(Json::parse(err)); continue; }

                QString mfg = QString::fromStdString(item.at("manufacturer").get<std::string>());
                QString model = QString::fromStdString(item.at("model").get<std::string>());
                QString name = QString::fromStdString(item.at("name").get<std::string>());
                int universe = item.at("universe").get<int>();
                int address = item.at("address").get<int>();
                int quantity = item.value("quantity", 1);

                const QLCFixtureDef *def = doc->fixtureDefCache()->fixtureDef(mfg, model);
                if (!def)
                {
                    results.push_back({{"error", "Fixture not found: " + mfg.toStdString() + " " + model.toStdString()}});
                    continue;
                }

                QLCFixtureDef *mutableDef = const_cast<QLCFixtureDef*>(def);
                QLCFixtureMode *mode = nullptr;
                QString modeName = QString::fromStdString(item.at("mode").get<std::string>());
                mode = mutableDef->mode(modeName);
                if (!mode)
                {
                    results.push_back({{"error", "Mode not found: " + modeName.toStdString() +
                        " for fixture " + mfg.toStdString() + " " + model.toStdString()}});
                    continue;
                }

                for (int i = 0; i < quantity; i++)
                {
                    QString fxName = quantity > 1 ? QString("%1 %2").arg(name).arg(i + 1) : name;
                    int fxAddr = address + (mode ? i * mode->channels().size() : i);

                    Fixture *existing = mcp::findFixture(doc, fxName, universe, fxAddr);
                    if (existing)
                    {
                        results.push_back({
                            {"id", (int)existing->id()},
                            {"name", existing->name().toStdString()},
                            {"address", (int)existing->address()},
                            {"universe", universe},
                            {"status", "existing"}
                        });
                        continue;
                    }

                    Fixture *fxi = new Fixture(doc);
                    fxi->setFixtureDefinition(mutableDef, mode);
                    fxi->setName(fxName);
                    fxi->setUniverse(universe);
                    fxi->setAddress(fxAddr);
                    doc->addFixture(fxi);

                    results.push_back({
                        {"id", (int)fxi->id()},
                        {"name", fxi->name().toStdString()},
                        {"address", (int)fxi->address()},
                        {"universe", universe},
                        {"status", "created"}
                    });
                }
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("Patch fixtures into the project. Upserts: skips if fixture with same name/address exists. Batch."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotIdempotent));

    // query_functions — list existing functions
    tm.register_tool(Tool(
        "query_functions",
        Json{{"type", "object"}, {"properties", Json::object()}},
        Json{},
        [doc](const Json &) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            for (Function *func : doc->functions())
                results.push_back(mcp::functionToJson(func));
            return results.dump();
            });
        },
        std::nullopt,
        std::string("List all existing functions (scenes, chasers, collections, etc.)."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotReadOnly));

    // query_vc_pages — list Virtual Console pages with widget details, filtering, and field selection
    if (vcBridge)
    {
        tm.register_tool(Tool(
            "vc_query_pages",
            Json{{"type", "object"}, {"properties", {
                {"nameFilter", {{"type", "string"},
                    {"description", "Glob pattern to match widget captions (case-insensitive, e.g. 'Wash*', '*left*'). Supports * and ?."}}},
                {"typeFilter", {{"type", "string"},
                    {"description", "Widget type(s) to include. Also accepts a JSON array of strings. "
                     "Valid: button, slider, xypad, frame, soloframe, speedDial, cuelist, label, audioTrigger, matrix, clock."}}},
                {"functionID", {{"type", "integer"},
                    {"description", "Only widgets bound to this function ID."}}},
                {"fixtureID", {{"type", "integer"},
                    {"description", "Only widgets controlling channels on this fixture."}}},
                {"channel", {{"type", "integer"},
                    {"description", "Only widgets controlling this DMX channel number."}}},
                {"pageIndex", {{"type", "integer"},
                    {"description", "Only return widgets from this page (0-based)."}}},
                {"parentID", {{"type", "integer"},
                    {"description", "Only return direct children of this frame widget."}}},
                {"properties", {{"type", "array"}, {"items", {{"type", "string"}}},
                    {"description", "Property names to include per widget (id always included). "
                     "Omit for all. Core: type, caption, pageIndex, parentID, geometry. "
                     "Function: functionID, action. Slider: sliderMode, channels, widgetStyle. "
                     "Frame: multipageMode, totalPages, currentPage, collapsed. "
                     "Input: inputMappings, validSources. Appearance: bgColor, fgColor, font, disabled. "
                     "Groups: buttonConfig, cueListConfig, clockConfig, speedDialConfig, matrixConfig, xyPadConfig."
                    }}}
            }}},
            Json{},
            [doc, vcBridge](const Json &args) -> Json {
                return execOnMainThread(doc, [&]() -> Json {

                // Validate arguments
                auto err = VCQueryPages::validateArgs(args);
                if (!err.empty()) return err;

                // Parse field selection
                auto properties = VCQueryPages::parseProperties(args);

                // Check pageIndex filter
                int filterPageIndex = -1;
                if (args.contains("pageIndex"))
                    filterPageIndex = args["pageIndex"].get<int>();

                Json results = Json::array();
                for (const auto &page : vcBridge->pages())
                {
                    // Skip pages that don't match pageIndex filter
                    if (filterPageIndex >= 0 && page.index != filterPageIndex)
                        continue;

                    Json widgets = Json::array();
                    for (const auto &w : page.widgets)
                    {
                        auto d = vcBridge->getWidgetDetails(w.id);
                        if (d.id < 0)
                            continue;

                        // Apply filters
                        if (!VCQueryPages::filterWidget(d, args))
                            continue;

                        // Serialize with field selection
                        widgets.push_back(VCQueryPages::serializeWidget(d, properties));
                    }

                    // Omit pages with no matching widgets (when filters are active)
                    bool hasFilters = args.contains("nameFilter") || args.contains("typeFilter") ||
                                     args.contains("functionID") || args.contains("fixtureID") ||
                                     args.contains("channel") || args.contains("parentID");
                    if (hasFilters && widgets.empty())
                        continue;

                    Json pageJson;
                    pageJson["index"] = page.index;
                    pageJson["name"] = page.name.toStdString();
                    pageJson["widgets"] = widgets;
                    results.push_back(pageJson);
                }
                return results.dump();
                });
            },
            std::nullopt,
            std::string("Search and list Virtual Console widgets with optional filtering and field selection. "
                        "Filters (AND-combined): nameFilter (glob), typeFilter (string/array), "
                        "functionID, fixtureID, channel, pageIndex, parentID. "
                        "Field selection: properties array (id always included, omit for all). "
                        "No parameters returns everything (backward compatible)."),
        std::nullopt
        )
        .set_annotations(mcp::kAnnotReadOnly));

        // vc_query_widgets (batch) — query full details of Virtual Console widgets
        tm.register_tool(Tool(
            "vc_query_widgets",
            Json{{"type", "object"}, {"properties", {
                {"widgetIDs", {{"type", "array"}, {"items", {{"type", "integer"}}},
                    {"description", "Widget IDs to query. Returns full details for each."}}}
            }}, {"required", {"widgetIDs"}}},
            Json{},
            [doc, vcBridge](const Json &args) -> Json {
                return execOnMainThread(doc, [&]() -> Json {
                auto err = validateFields(args, {"widgetIDs"});
                if (!err.empty()) return err;
                Json results = Json::array();
                std::set<std::string> allProps;  // empty = all properties
                for (auto &wid : args.at("widgetIDs"))
                {
                    int id = wid.get<int>();
                    auto d = vcBridge->getWidgetDetails(id);
                    if (d.id < 0)
                    {
                        results.push_back({{"id", id}, {"error", "not found"}});
                        continue;
                    }
                    results.push_back(VCQueryPages::serializeWidget(d, allProps));
                }
                return results.dump();
                });
            },
            std::nullopt,
            std::string("Query full details of Virtual Console widgets. Batch."),
            std::nullopt
        )
        .set_annotations(mcp::kAnnotReadOnly));
    }

    // query_universes — list universe configuration
    tm.register_tool(Tool(
        "query_universes",
        Json{{"type", "object"}, {"properties", Json::object()}},
        Json{},
        [doc](const Json &) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            InputOutputMap *ioMap = doc->inputOutputMap();
            for (Universe *uni : ioMap->universes())
            {
                Json entry;
                entry["id"] = (int)uni->id();
                entry["name"] = uni->name().toStdString();
                entry["passthrough"] = uni->passthrough();

                InputPatch *inPatch = ioMap->inputPatch(uni->id());
                if (inPatch && inPatch->isPatched())
                {
                    entry["inputPlugin"] = inPatch->pluginName().toStdString();
                    entry["inputLine"] = (int)inPatch->input();
                    entry["inputName"] = inPatch->inputName().toStdString();
                    if (inPatch->profile())
                        entry["inputProfile"] = inPatch->profile()->name().toStdString();
                }

                OutputPatch *outPatch = ioMap->outputPatch(uni->id());
                if (outPatch && outPatch->isPatched())
                {
                    entry["outputPlugin"] = outPatch->pluginName().toStdString();
                    entry["outputLine"] = (int)outPatch->output();
                    entry["outputName"] = outPatch->outputName().toStdString();
                }

                entry["hasFeedback"] = uni->hasFeedback();

                results.push_back(entry);
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("List all configured DMX universes with their I/O plugin assignments."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotReadOnly));

    // query_palettes — list all palettes
    tm.register_tool(Tool(
        "query_palettes",
        Json{{"type", "object"}, {"properties", {
            {"typeFilter", {{"type", "string"},
                {"description", "Filter by type: Dimmer, Color, Pan, Tilt, PanTilt (omit for all)"}}}
        }}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto err = validateFields(args, {"typeFilter"});
            if (!err.empty()) return err;

            // Optional type filter
            QLCPalette::PaletteType filterType = QLCPalette::Undefined;
            if (args.contains("typeFilter"))
            {
                QString tf = QString::fromStdString(args.at("typeFilter").get<std::string>());
                filterType = QLCPalette::stringToType(tf);
            }

            // Build reverse map: palette ID → list of scenes that reference it
            QMap<quint32, QList<QPair<quint32, QString>>> paletteSceneRefs;
            for (Function *fn : doc->functions())
            {
                if (fn->type() != Function::SceneType) continue;
                Scene *scene = qobject_cast<Scene*>(fn);
                if (!scene) continue;
                for (quint32 palId : scene->palettes())
                    paletteSceneRefs[palId].append({scene->id(), scene->name()});
            }

            Json results = Json::array();
            for (QLCPalette *p : doc->palettes())
            {
                if (filterType != QLCPalette::Undefined && p->type() != filterType)
                    continue;

                Json entry;
                entry["id"] = (int)p->id();
                entry["name"] = p->name().toStdString();
                entry["type"] = QLCPalette::typeToString(p->type()).toStdString();

                // Type-specific values
                switch (p->type())
                {
                    case QLCPalette::Dimmer:
                        entry["value"] = p->intValue1();
                        break;
                    case QLCPalette::Color:
                        entry["rgb"] = p->rgbValue().name().toStdString();
                        entry["wauv"] = p->wauvValue().name().toStdString();
                        break;
                    case QLCPalette::Pan:
                        entry["panDegrees"] = p->floatValue1();
                        break;
                    case QLCPalette::Tilt:
                        entry["tiltDegrees"] = p->floatValue1();
                        break;
                    case QLCPalette::PanTilt:
                        entry["panDegrees"] = p->floatValue1();
                        entry["tiltDegrees"] = (double)p->intValue2();
                        break;
                    default:
                        break;
                }

                // Scene references
                if (paletteSceneRefs.contains(p->id()))
                {
                    Json refs = Json::array();
                    for (auto &pair : paletteSceneRefs[p->id()])
                        refs.push_back({{"sceneID", (int)pair.first}, {"sceneName", pair.second.toStdString()}});
                    entry["referencedByScenes"] = refs;
                }

                results.push_back(entry);
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("List all palettes with their type, values, and which scenes reference them. "
                     "Optional typeFilter: Dimmer, Color, Pan, Tilt, PanTilt."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotReadOnly));
}
