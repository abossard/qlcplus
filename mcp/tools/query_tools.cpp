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
#include "vcbridge.h"
#include "doc.h"
#include "fixture.h"
#include "function.h"
#include "qlcchannel.h"
#include "qlcfixturedef.h"
#include "qlcfixturedefcache.h"
#include "qlcfixturemode.h"
#include "inputoutputmap.h"
#include "universe.h"

#include <fastmcpp/tools/manager.hpp>
#include <fastmcpp/tools/tool.hpp>

using Json = nlohmann::json;

static Json fixtureCapabilities(const Fixture *fxi)
{
    Json caps = Json::array();
    bool hasPan = false, hasTilt = false;
    bool hasR = false, hasG = false, hasB = false;
    bool hasC = false, hasM = false, hasY = false;

    for (quint32 ch = 0; ch < fxi->channels(); ch++)
    {
        const QLCChannel *channel = fxi->channel(ch);
        if (!channel) continue;
        switch (channel->group())
        {
            case QLCChannel::Pan: hasPan = true; break;
            case QLCChannel::Tilt: hasTilt = true; break;
            case QLCChannel::Colour: caps.push_back("Colour"); break;
            case QLCChannel::Gobo: caps.push_back("Gobo"); break;
            case QLCChannel::Shutter: caps.push_back("Shutter"); break;
            case QLCChannel::Intensity:
                switch (channel->colour())
                {
                    case QLCChannel::Red: hasR = true; break;
                    case QLCChannel::Green: hasG = true; break;
                    case QLCChannel::Blue: hasB = true; break;
                    case QLCChannel::Cyan: hasC = true; break;
                    case QLCChannel::Magenta: hasM = true; break;
                    case QLCChannel::Yellow: hasY = true; break;
                    default: break;
                }
                break;
            default: break;
        }
    }
    if (hasPan && hasTilt) caps.push_back("Pan/Tilt");
    if (hasR && hasG && hasB) caps.push_back("RGB");
    if (hasC && hasM && hasY) caps.push_back("CMY");
    return caps;
}

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
            {
                Json entry;
                entry["id"] = fxi->id();
                entry["name"] = fxi->name().toStdString();
                entry["channels"] = fxi->channels();
                entry["heads"] = fxi->heads();
                if (fxi->fixtureDef())
                {
                    entry["manufacturer"] = fxi->fixtureDef()->manufacturer().toStdString();
                    entry["model"] = fxi->fixtureDef()->model().toStdString();
                }
                entry["capabilities"] = fixtureCapabilities(fxi);
                results.push_back(entry);
            }
            return results;
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
                QString mfgFilter = filter.contains("manufacturer")
                    ? QString::fromStdString(filter["manufacturer"].get<std::string>()) : "";
                QString modelFilter = filter.contains("model")
                    ? QString::fromStdString(filter["model"].get<std::string>()) : "";

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
            return results;
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
                {"mode", {{"type", "string"}, {"description", "Fixture mode name (optional, uses first mode if omitted)"}}},
                {"name", {{"type", "string"}, {"description", "Base name for the fixture(s)"}}},
                {"universe", {{"type", "integer"}}},
                {"address", {{"type", "integer"}, {"description", "DMX start address (0-based)"}}},
                {"quantity", {{"type", "integer"}, {"description", "Number of fixtures to patch (default 1)"}}}
            }}, {"required", {"manufacturer", "model", "name", "universe", "address"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            for (auto &item : args["items"])
            {
                QString mfg = QString::fromStdString(item["manufacturer"].get<std::string>());
                QString model = QString::fromStdString(item["model"].get<std::string>());
                QString name = QString::fromStdString(item["name"].get<std::string>());
                int universe = item["universe"].get<int>();
                int address = item["address"].get<int>();
                int quantity = item.value("quantity", 1);
                QString modeName = item.contains("mode")
                    ? QString::fromStdString(item["mode"].get<std::string>()) : "";

                const QLCFixtureDef *def = doc->fixtureDefCache()->fixtureDef(mfg, model);
                if (!def)
                {
                    results.push_back({{"error", "Fixture not found: " + mfg.toStdString() + " " + model.toStdString()}});
                    continue;
                }

                QLCFixtureDef *mutableDef = const_cast<QLCFixtureDef*>(def);
                QLCFixtureMode *mode = nullptr;
                if (!modeName.isEmpty())
                    mode = mutableDef->mode(modeName);
                if (!mode && !mutableDef->modes().isEmpty())
                    mode = mutableDef->modes().first();

                for (int i = 0; i < quantity; i++)
                {
                    Fixture *fxi = new Fixture(doc);
                    fxi->setFixtureDefinition(mutableDef, mode);
                    fxi->setName(quantity > 1 ? QString("%1 %2").arg(name).arg(i + 1) : name);
                    fxi->setUniverse(universe);
                    fxi->setAddress(address + (mode ? i * mode->channels().size() : i));
                    doc->addFixture(fxi);

                    results.push_back({
                        {"id", (int)fxi->id()},
                        {"name", fxi->name().toStdString()},
                        {"address", (int)fxi->address()},
                        {"universe", universe}
                    });
                }
            }
            return results;
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
            {
                results.push_back({
                    {"id", (int)func->id()},
                    {"name", func->name().toStdString()},
                    {"type", Function::typeToString(func->type()).toStdString()},
                    {"duration", (int)func->totalDuration()}
                });
            }
            return results;
            });
        },
        std::nullopt,
        std::string("List all existing functions (scenes, chasers, collections, etc.)"),
        std::nullopt
    ));

    // query_vc_pages — list Virtual Console pages
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
                        widgets.push_back({
                            {"id", w.id},
                            {"type", w.type.toStdString()},
                            {"caption", w.caption.toStdString()},
                            {"x", w.geometry.x()}, {"y", w.geometry.y()},
                            {"width", w.geometry.width()}, {"height", w.geometry.height()},
                            {"functionID", (int)w.functionID}
                        });
                    }
                    pageJson["widgets"] = widgets;
                    results.push_back(pageJson);
                }
                return results;
                });
            },
            std::nullopt,
            std::string("List all Virtual Console pages and their widgets."),
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
                results.push_back(entry);
            }
            return results;
            });
        },
        std::nullopt,
        std::string("List all configured DMX universes."),
        std::nullopt
    ));
}
