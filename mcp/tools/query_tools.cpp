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
#include "doc.h"
#include "qlcfixturedefcache.h"
#include "qlcfixturemode.h"
#include "inputoutputmap.h"
#include "inputpatch.h"
#include "outputpatch.h"
#include "universe.h"
#include "inputoutputmap.h"
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
        std::string("List all patched fixtures with their capabilities (RGB, Pan/Tilt, Gobo, etc.)"),
        std::nullopt
    ));

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
        std::string("Search the fixture definition library by manufacturer/model. Returns available fixtures with their modes."),
        std::nullopt
    ));

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
        std::string("Patch fixtures into the project. Supports batching: pass multiple fixtures in 'items' array."),
        std::nullopt
    ));

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
        std::string("List all existing functions (scenes, chasers, collections, etc.)"),
        std::nullopt
    ));

    // query_vc_pages — list Virtual Console pages with widget details
    if (vcBridge)
    {
        tm.register_tool(Tool(
            "query_vc_pages",
            Json{{"type", "object"}, {"properties", Json::object()}},
            Json{},
            [doc, vcBridge](const Json &) -> Json {
                return execOnMainThread(doc, [&]() -> Json {
                Json results = Json::array();
                for (const auto &page : vcBridge->pages())
                {
                    Json pageJson;
                    pageJson["index"] = page.index;
                    pageJson["name"] = page.name.toStdString();
                    Json widgets = Json::array();
                    for (const auto &w : page.widgets)
                    {
                        Json wJson;
                        wJson["id"] = w.id;
                        wJson["type"] = w.type.toStdString();
                        wJson["caption"] = w.caption.toStdString();
                        wJson["x"] = w.geometry.x();
                        wJson["y"] = w.geometry.y();
                        wJson["width"] = w.geometry.width();
                        wJson["height"] = w.geometry.height();

                        // Enrich with details from getWidgetDetails
                        auto d = vcBridge->getWidgetDetails(w.id);
                        if (d.id >= 0)
                        {
                            if (d.functionID != 0 && d.functionID != (quint32)-1)
                                wJson["functionID"] = (int)d.functionID;
                            if (!d.action.isEmpty())
                                wJson["action"] = d.action.toStdString();
                            if (!d.sliderMode.isEmpty())
                                wJson["sliderMode"] = d.sliderMode.toStdString();
                            if (d.parentID >= 0)
                                wJson["parentID"] = d.parentID;

                            if (!d.inputMappings.isEmpty())
                            {
                                Json inputs = Json::array();
                                for (auto &m : d.inputMappings)
                                    inputs.push_back({{"universe", (int)m.universe}, {"channel", (int)m.channel}});
                                wJson["inputMappings"] = inputs;
                            }
                        }
                        widgets.push_back(wJson);
                    }
                    pageJson["widgets"] = widgets;
                    results.push_back(pageJson);
                }
                return results.dump();
                });
            },
            std::nullopt,
            std::string("List all Virtual Console pages and their widgets with details (type, caption, geometry, "
                         "function, action, slider mode, parent, input mappings)."),
        std::nullopt
        ));
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
        std::string("List all configured DMX universes."),
        std::nullopt
    ));
}
