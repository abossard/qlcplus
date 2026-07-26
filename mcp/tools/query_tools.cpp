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
#include "rgbmatrix.h"
#include "rgbalgorithm.h"
#include "rgbscriptv4.h"
#include "rgbtext.h"
#include "rgbimage.h"
#include "fixturegroup.h"
#include "inputoutputmap.h"
#include "inputpatch.h"
#include "outputpatch.h"
#include "universe.h"

#include <fastmcpp/tools/manager.hpp>
#include <fastmcpp/tools/tool.hpp>

#include <algorithm>

using Json = nlohmann::json;

void registerQueryTools(fastmcpp::tools::ToolManager &tm, Doc *doc, VCBridge *vcBridge)
{
    using Tool = fastmcpp::tools::Tool;

    // query_fixtures — list all patched fixtures
    tm.register_tool(Tool(
        "query_fixtures",
        Json{{"type", "object"}, {"properties", {
            {"id", {{"type", "integer"}, {"minimum", 0}, {"description", "Exact fixture ID"}}},
            {"name", {{"type", "string"}, {"description", "Case-insensitive name substring"}}},
            {"universe", {{"type", "integer"}, {"minimum", 0}, {"description", "Exact zero-based universe"}}},
            {"page", {{"type", "object"}, {"properties", {
                {"limit", {{"type", "integer"}, {"minimum", 1}, {"maximum", 100}}},
                {"cursor", {{"type", "string"}, {"description", "Opaque cursor returned by the previous page"}}}
            }}, {"additionalProperties", false}}}
        }}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto err = validateFields(args, {"id", "name", "universe", "page"});
            if (!err.empty()) return err;
            if (args.contains("id") && (!args["id"].is_number_integer() || args["id"].get<int>() < 0))
                return Json({{"error", "id must be a non-negative integer"}}).dump();
            if (args.contains("name") && !args["name"].is_string())
                return Json({{"error", "name must be a string"}}).dump();
            if (args.contains("universe") &&
                (!args["universe"].is_number_integer() || args["universe"].get<int>() < 0))
                return Json({{"error", "universe must be a non-negative integer"}}).dump();
            if (args.contains("page") && !args["page"].is_object())
                return Json({{"error", "page must be an object"}}).dump();

            QList<Fixture *> fixtures = doc->fixtures();
            std::sort(fixtures.begin(), fixtures.end(),
                [](const Fixture *left, const Fixture *right) { return left->id() < right->id(); });

            Json results = Json::array();
            for (Fixture *fxi : fixtures)
            {
                if (args.contains("id") && fxi->id() != args["id"].get<quint32>())
                    continue;
                if (args.contains("name") &&
                    !fxi->name().contains(QString::fromStdString(args["name"].get<std::string>()),
                                          Qt::CaseInsensitive))
                    continue;
                if (args.contains("universe") && fxi->universe() != args["universe"].get<quint32>())
                    continue;
                results.push_back(mcp::fixtureToJson(fxi));
            }

            if (!args.contains("page"))
                return results.dump();

            const Json &page = args["page"];
            err = validateFields(page, {"limit", "cursor"});
            if (!err.empty()) return err;
            int limit = 50;
            if (page.contains("limit"))
            {
                if (!page["limit"].is_number_integer())
                    return Json({{"error", "page.limit must be an integer from 1 to 100"}}).dump();
                limit = page["limit"].get<int>();
                if (limit < 1 || limit > 100)
                    return Json({{"error", "page.limit must be from 1 to 100"}}).dump();
            }

            int offset = 0;
            if (page.contains("cursor"))
            {
                if (!page["cursor"].is_string())
                    return Json({{"error", "page.cursor must be an opaque string"}}).dump();
                QByteArray encoded = QByteArray::fromStdString(page["cursor"].get<std::string>());
                QByteArray decoded = QByteArray::fromBase64(encoded, QByteArray::Base64UrlEncoding);
                const QByteArray prefix("fixture-offset:");
                bool validOffset = false;
                if (decoded.startsWith(prefix))
                    offset = decoded.mid(prefix.size()).toInt(&validOffset);
                if (!validOffset || offset < 0 || offset >= int(results.size()))
                    return Json({{"error", "page.cursor is invalid or no longer points into this filtered result"}}).dump();
            }

            Json items = Json::array();
            int end = std::min(offset + limit, int(results.size()));
            for (int index = offset; index < end; ++index)
                items.push_back(results[index]);

            Json envelope = {{"items", items}, {"total", results.size()}, {"nextCursor", nullptr}};
            if (end < int(results.size()))
            {
                QByteArray cursor = QByteArray("fixture-offset:") + QByteArray::number(end);
                envelope["nextCursor"] = cursor.toBase64(
                    QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals).toStdString();
            }
            return envelope.dump();
            });
        },
        std::nullopt,
        std::string("List patched fixtures in fixture-ID order, optionally filtered by id, case-insensitive name, and universe. "
                    "Without page, returns the legacy bare array. With page, returns a bounded cursor envelope. "
                    "Returns IDs needed for other tools. "
                    "Includes type (Moving Head, Dimmer, etc.), capabilities (RGBW, ContinuousTiltRotation, Pan/Tilt, UV, Amber), "
                    "headMap with per-head channel indices and rgbChannels for multi-head fixtures, and physical properties."),
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
        std::string("Search the fixture definition library by manufacturer/model. Returns available fixtures with their modes. Batch. "
                     "Wrap multiple operations in {\"items\": [...]}. Each item is processed independently."),
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
                {"universe", {{"type", "integer"}, {"minimum", 0}, {"maximum", 127}}},
                {"address", {{"type", "integer"}, {"minimum", 0}, {"maximum", 511},
                             {"description", "DMX start address (0-based)"}}},
                {"quantity", {{"type", "integer"}, {"minimum", 1}, {"maximum", 512},
                              {"description", "Number of fixtures to patch (default 1)"}}}
            }}, {"required", {"manufacturer", "model", "mode", "name", "universe", "address"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto itemsErr = validateItemsArray(args);
            if (itemsErr) return *itemsErr;
            Json results = Json::array();
            for (auto &item : args.at("items"))
            {
                auto err = validateFields(item, {"manufacturer", "model", "mode", "name", "universe", "address", "quantity"});
                if (!err.empty()) { results.push_back(Json::parse(err)); continue; }
                if (!item["universe"].is_number_integer() ||
                    item["universe"].get<int>() < 0 || item["universe"].get<int>() > 127)
                {
                    results.push_back({{"error", "universe must be an integer from 0 to 127"}});
                    continue;
                }
                if (!item["address"].is_number_integer() ||
                    item["address"].get<int>() < 0 || item["address"].get<int>() > 511)
                {
                    results.push_back({{"error", "address must be an integer from 0 to 511"}});
                    continue;
                }
                if (item.contains("quantity") &&
                    (!item["quantity"].is_number_integer() ||
                     item["quantity"].get<int>() < 1 || item["quantity"].get<int>() > 512))
                {
                    results.push_back({{"error", "quantity must be an integer from 1 to 512"}});
                    continue;
                }

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
                if (address + quantity * mode->channels().size() > 512)
                {
                    results.push_back({
                        {"error", "fixture quantity and footprint exceed DMX address 511"},
                        {"address", address}, {"quantity", quantity},
                        {"channelsPerFixture", mode->channels().size()}
                    });
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
                    if (!doc->addFixture(fxi))
                    {
                        delete fxi;
                        results.push_back({
                            {"name", fxName.toStdString()},
                            {"address", fxAddr},
                            {"universe", universe},
                            {"error", "address overlap with existing fixture"}
                        });
                        continue;
                    }

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
        std::string("Create fixtures in the project. Returns status 'existing' only for an exact name, universe, and address match; "
                    "a changed address creates a separate fixture rather than updating an existing one. Batch."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotIdempotent));

    // update_fixture — atomically repair one fixture's setup properties
    tm.register_tool(Tool(
        "update_fixture",
        Json{{"type", "object"}, {"properties", {
            {"id", {{"type", "integer"}, {"minimum", 0}, {"description", "Existing fixture ID"}}},
            {"name", {{"type", "string"}, {"description", "New fixture name"}}},
            {"universe", {{"type", "integer"}, {"minimum", 0}, {"maximum", 127},
                          {"description", "New zero-based universe"}}},
            {"address", {{"type", "integer"}, {"minimum", 0}, {"maximum", 511},
                         {"description", "New zero-based DMX start address"}}}
        }}, {"required", {"id"}}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto err = validateFields(args, {"id", "name", "universe", "address"});
            if (!err.empty()) return err;
            if (!args.contains("id") || !args["id"].is_number_integer() || args["id"].get<int>() < 0)
                return Json({{"error", "id is required and must be a non-negative integer"}}).dump();
            if (!args.contains("name") && !args.contains("universe") && !args.contains("address"))
                return Json({{"error", "provide at least one update field: name, universe, or address"}}).dump();
            if (args.contains("name") &&
                (!args["name"].is_string() || args["name"].get<std::string>().empty()))
                return Json({{"error", "name must be a non-empty string"}}).dump();
            if (args.contains("universe") &&
                (!args["universe"].is_number_integer() ||
                 args["universe"].get<int>() < 0 || args["universe"].get<int>() > 127))
                return Json({{"error", "universe must be an integer from 0 to 127"}}).dump();
            if (args.contains("address") &&
                (!args["address"].is_number_integer() ||
                 args["address"].get<int>() < 0 || args["address"].get<int>() > 511))
                return Json({{"error", "address must be an integer from 0 to 511"}}).dump();

            const quint32 id = args["id"].get<quint32>();
            Fixture *fixture = doc->fixture(id);
            if (!fixture)
                return Json({{"error", "fixture ID " + std::to_string(id) + " was not found"},
                             {"id", id}}).dump();

            const QString finalName = args.contains("name")
                ? QString::fromStdString(args["name"].get<std::string>()) : fixture->name();
            const quint32 finalUniverse = args.contains("universe")
                ? args["universe"].get<quint32>() : fixture->universe();
            const quint32 finalAddress = args.contains("address")
                ? args["address"].get<quint32>() : fixture->address();
            const quint32 channels = fixture->channels();

            if (!fixture->crossUniverse() && finalAddress + channels > 512)
            {
                return Json({
                    {"error", "fixture footprint exceeds address 511; choose address at most " +
                              std::to_string(512 - channels)},
                    {"id", id}, {"address", finalAddress}, {"channels", channels}
                }).dump();
            }

            const quint32 finalUniverseAddress = (finalUniverse << 9) | finalAddress;
            for (quint32 offset = 0; offset < channels; ++offset)
            {
                const quint32 owner = doc->fixtureForAddress(finalUniverseAddress + offset);
                if (owner != Fixture::invalidId() && owner != id)
                {
                    return Json({
                        {"error", "target footprint conflicts with fixture ID " +
                                  std::to_string(owner) + " at universe " +
                                  std::to_string(finalUniverse) + ", address " +
                                  std::to_string(finalAddress + offset)},
                        {"id", id}, {"conflictingFixtureID", owner},
                        {"universe", finalUniverse}, {"address", finalAddress + offset}
                    }).dump();
                }
            }

            auto setupState = [](const Fixture *entry) {
                return Json{
                    {"id", entry->id()},
                    {"name", entry->name().toStdString()},
                    {"universe", entry->universe()},
                    {"address", entry->address()},
                    {"channels", entry->channels()}
                };
            };

            const Json before = setupState(fixture);
            if (fixture->name() == finalName && fixture->universe() == finalUniverse &&
                fixture->address() == finalAddress)
                return Json({{"status", "unchanged"}, {"before", before}, {"after", before}}).dump();

            const bool signalsWereBlocked = fixture->blockSignals(true);
            if (fixture->name() != finalName) fixture->setName(finalName);
            if (fixture->universe() != finalUniverse) fixture->setUniverse(finalUniverse);
            if (fixture->address() != finalAddress) fixture->setAddress(finalAddress);
            fixture->blockSignals(signalsWereBlocked);
            if (!signalsWereBlocked)
                fixture->setID(id);

            return Json({
                {"status", "updated"},
                {"before", before},
                {"after", setupState(fixture)}
            }).dump();
            });
        },
        std::nullopt,
        std::string("Atomically rename or repatch one existing fixture by ID after validating its complete final footprint."),
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
                     "Frame: multipageMode, totalPages, currentPage, collapsed, grid. "
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
                    pageJson["externalInputMode"] = page.externalInputMode.toStdString();
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
            std::string("Query full details of Virtual Console widgets. Batch. "
                         "Pass widget IDs in {\"widgetIDs\": [...]}."),
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
            {"typeFilter", {{"type", "string"}, {"enum", {"Dimmer", "Color", "Pan", "Tilt", "PanTilt", "Position3D", "Shutter", "Gobo", "Zoom"}},
                {"description", "Filter by type: Dimmer, Color, Pan, Tilt, PanTilt, Position3D, Shutter, Gobo, Zoom (omit for all)"}}}
        }}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto err = validateFields(args, {"typeFilter"});
            if (!err.empty()) return err;

            static const Json kEnums = {
                {"typeFilter", {{"enum", {"Dimmer", "Color", "Pan", "Tilt", "PanTilt", "Position3D", "Shutter", "Gobo", "Zoom"}}}}
            };
            err = validateEnums(args, kEnums);
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
                    case QLCPalette::Position3D:
                        entry["x"] = p->floatValue1();
                        entry["y"] = p->floatValue2();
                        entry["z"] = p->floatValue3();
                        break;
                    case QLCPalette::Shutter:
                        entry["value"] = p->intValue1();
                        entry["value2"] = p->intValue2();
                        break;
                    case QLCPalette::Gobo:
                    case QLCPalette::Zoom:
                        entry["value"] = p->intValue1();
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
                     "Optional typeFilter: Dimmer, Color, Pan, Tilt, PanTilt, Position3D, Shutter, Gobo, Zoom."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotReadOnly));

    // query_rgb_algorithms — list available RGB algorithms
    tm.register_tool(Tool(
        "query_rgb_algorithms",
        Json{{"type", "object"}, {"properties", {
            {"type", {{"type", "string"}, {"enum", {"Script", "Text", "Image", "Audio", "Plain"}}, {"description", "Filter by type: Script, Text, Image, Audio, Plain"}}},
            {"name", {{"type", "string"}, {"description", "Filter by name (substring, case-insensitive)"}}}
        }}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto err = validateFields(args, {"type", "name"});
            if (!err.empty()) return err;

            static const Json kEnums = {
                {"type", {{"enum", {"Script", "Text", "Image", "Audio", "Plain"}}}}
            };
            err = validateEnums(args, kEnums);
            if (!err.empty()) return err;

            QString typeFilter = args.contains("type")
                ? QString::fromStdString(args.at("type").get<std::string>()) : "";
            QString nameFilter = args.contains("name")
                ? QString::fromStdString(args.at("name").get<std::string>()).toLower() : "";

            Json results = Json::array();
            QStringList algoNames = RGBAlgorithm::algorithms(doc);
            for (const QString &algoName : algoNames)
            {
                RGBAlgorithm *algo = RGBAlgorithm::algorithm(doc, algoName);
                if (!algo) continue;

                std::string typeStr = mcp::rgbAlgorithmTypeToString(algo->type());

                // Apply filters
                if (!typeFilter.isEmpty() && QString::fromStdString(typeStr).compare(typeFilter, Qt::CaseInsensitive) != 0)
                { delete algo; continue; }
                if (!nameFilter.isEmpty() && !algoName.toLower().contains(nameFilter))
                { delete algo; continue; }

                Json entry;
                entry["name"] = algoName.toStdString();
                entry["type"] = typeStr;
                entry["acceptColors"] = algo->acceptColors();
                entry["audioReactive"] = algo->usesAudio();

                // Script properties
                if (algo->type() == RGBAlgorithm::Script)
                {
                    RGBScript *script = static_cast<RGBScript*>(algo);
                    QList<RGBScriptProperty> props = script->properties();
                    if (!props.isEmpty())
                    {
                        Json propsJson = Json::array();
                        for (const RGBScriptProperty &prop : props)
                        {
                            Json p;
                            p["name"] = prop.m_name.toStdString();
                            p["displayName"] = prop.m_displayName.toStdString();
                            switch (prop.m_type)
                            {
                                case RGBScriptProperty::List:
                                {
                                    p["type"] = "list";
                                    Json vals = Json::array();
                                    for (const QString &v : prop.m_listValues)
                                        vals.push_back(v.toStdString());
                                    p["values"] = vals;
                                    break;
                                }
                                case RGBScriptProperty::Range:
                                    p["type"] = "range";
                                    p["min"] = prop.m_rangeMinValue;
                                    p["max"] = prop.m_rangeMaxValue;
                                    break;
                                case RGBScriptProperty::Float:
                                    p["type"] = "float";
                                    break;
                                case RGBScriptProperty::String:
                                    p["type"] = "string";
                                    break;
                                default:
                                    p["type"] = "unknown";
                                    break;
                            }
                            // Include current default value
                            QString val = script->property(prop.m_name);
                            if (!val.isEmpty())
                                p["default"] = val.toStdString();
                            propsJson.push_back(p);
                        }
                        entry["properties"] = propsJson;
                    }
                }

                results.push_back(entry);
                delete algo;
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("List available RGB algorithms (Plain, Script, Text, Image, Audio) with their types, "
                     "accepted color count, audio-reactivity flag, and configurable properties (for scripts). "
                     "Use to discover algorithms before creating RGB matrices. "
                     "Beat durations supported: 1/8, 1/4, 1/2, 1, 2, 3, 4 beats."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotReadOnly));

    // query_fixture_groups — list fixture groups
    tm.register_tool(Tool(
        "query_fixture_groups",
        Json{{"type", "object"}, {"properties", {
            {"name", {{"type", "string"}, {"description", "Filter by name (substring, case-insensitive)"}}},
            {"minFixtures", {{"type", "integer"}, {"description", "Only groups with at least N fixtures"}}}
        }}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto err = validateFields(args, {"name", "minFixtures"});
            if (!err.empty()) return err;

            QString nameFilter = args.contains("name")
                ? QString::fromStdString(args.at("name").get<std::string>()).toLower() : "";
            int minFixtures = args.value("minFixtures", 0);

            Json results = Json::array();
            for (FixtureGroup *group : doc->fixtureGroups())
            {
                if (!nameFilter.isEmpty() && !group->name().toLower().contains(nameFilter))
                    continue;
                QList<quint32> fxList = group->fixtureList();
                if ((int)fxList.size() < minFixtures)
                    continue;

                Json entry;
                entry["id"] = (int)group->id();
                entry["name"] = group->name().toStdString();
                entry["columns"] = group->size().width();
                entry["rows"] = group->size().height();
                Json fxIds = Json::array();
                for (quint32 fid : fxList)
                    fxIds.push_back((int)fid);
                entry["fixtureIDs"] = fxIds;
                entry["fixtureCount"] = (int)fxList.size();
                results.push_back(entry);
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("List fixture groups with grid dimensions and fixture IDs. "
                     "Fixture groups define the pixel layout for RGB matrices."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotReadOnly));

    // query_rgb_matrices — list RGB matrix functions with full details
    tm.register_tool(Tool(
        "query_rgb_matrices",
        Json{{"type", "object"}, {"properties", {
            {"name", {{"type", "string"}, {"description", "Filter by name (substring, case-insensitive)"}}},
            {"algorithm", {{"type", "string"}, {"description", "Filter by algorithm name (substring)"}}},
            {"fixtureGroupID", {{"type", "integer"}, {"description", "Filter by fixture group ID"}}}
        }}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto err = validateFields(args, {"name", "algorithm", "fixtureGroupID"});
            if (!err.empty()) return err;

            QString nameFilter = args.contains("name")
                ? QString::fromStdString(args.at("name").get<std::string>()).toLower() : "";
            QString algoFilter = args.contains("algorithm")
                ? QString::fromStdString(args.at("algorithm").get<std::string>()).toLower() : "";
            int groupFilter = args.contains("fixtureGroupID")
                ? args.at("fixtureGroupID").get<int>() : -1;

            Json results = Json::array();
            for (Function *fn : doc->functions())
            {
                if (fn->type() != Function::RGBMatrixType) continue;
                RGBMatrix *matrix = qobject_cast<RGBMatrix*>(fn);
                if (!matrix) continue;

                if (!nameFilter.isEmpty() && !matrix->name().toLower().contains(nameFilter))
                    continue;
                if (!algoFilter.isEmpty())
                {
                    RGBAlgorithm *algo = matrix->algorithm();
                    if (!algo || !algo->name().toLower().contains(algoFilter))
                        continue;
                }
                if (groupFilter >= 0 && (int)matrix->fixtureGroup() != groupFilter)
                    continue;

                Json entry = mcp::rgbMatrixToJson(matrix);

                // Also include algorithm properties if set
                RGBAlgorithm *algo = matrix->algorithm();
                if (algo && algo->type() == RGBAlgorithm::Script)
                {
                    RGBScript *script = static_cast<RGBScript*>(algo);
                    QHash<QString, QString> props = script->propertiesAsStrings();
                    if (!props.isEmpty())
                    {
                        Json propsJson = Json::object();
                        for (auto it = props.constBegin(); it != props.constEnd(); ++it)
                            propsJson[it.key().toStdString()] = it.value().toStdString();
                        entry["properties"] = propsJson;
                    }
                }

                results.push_back(entry);
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("List all RGB matrix functions with full details: algorithm, colors, timing (beat strings when in Beats mode), "
                     "control mode, blend mode, run order, direction, and script properties."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotReadOnly));

    // query_workspace_summary — lightweight counts of major entities
    tm.register_tool(Tool(
        "query_workspace_summary",
        Json{{"type", "object"}, {"properties", Json::object()}},
        Json{},
        [doc, vcBridge](const Json &) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            int scenes = 0, chasers = 0, sequences = 0, collections = 0;
            int rgbMatrices = 0, efx = 0, scripts = 0, shows = 0;
            int audio = 0, video = 0;
            for (Function *fn : doc->functions())
            {
                if (!fn) continue;
                switch (fn->type())
                {
                    case Function::SceneType:      scenes++; break;
                    case Function::ChaserType:     chasers++; break;
                    case Function::SequenceType:   sequences++; break;
                    case Function::CollectionType: collections++; break;
                    case Function::RGBMatrixType:  rgbMatrices++; break;
                    case Function::EFXType:        efx++; break;
                    case Function::ScriptType:     scripts++; break;
                    case Function::ShowType:       shows++; break;
                    case Function::AudioType:      audio++; break;
                    case Function::VideoType:      video++; break;
                    default: break;
                }
            }

            int universes = 0;
            if (InputOutputMap *ioMap = doc->inputOutputMap())
                universes = ioMap->universes().size();

            int vcPages = 0;
            if (vcBridge)
                vcPages = (int)vcBridge->pages().size();

            int runningFunctions = 0;
            if (MasterTimer *mt = doc->masterTimer())
                runningFunctions = mt->runningFunctions();

            Json result;
            result["fixtures"] = (int)doc->fixtures().size();
            result["fixtureGroups"] = (int)doc->fixtureGroups().size();
            result["universes"] = universes;
            Json fns;
            fns["total"] = (int)doc->functions().size();
            fns["scenes"] = scenes;
            fns["chasers"] = chasers;
            fns["collections"] = collections;
            fns["rgbMatrices"] = rgbMatrices;
            fns["efx"] = efx;
            fns["scripts"] = scripts;
            fns["shows"] = shows;
            fns["sequences"] = sequences;
            fns["audio"] = audio;
            fns["video"] = video;
            result["functions"] = fns;
            result["palettes"] = (int)doc->palettes().size();
            result["vcPages"] = vcPages;
            result["runningFunctions"] = runningFunctions;
            return result.dump();
            });
        },
        std::nullopt,
        std::string("Lightweight workspace overview. Returns counts of fixtures, fixture groups, universes, "
                     "functions (grouped by type: scenes, chasers, collections, rgbMatrices, efx, scripts, "
                     "shows, sequences, audio, video), palettes, VC pages, and currently running functions. "
                     "Use as a fast first call before drilling into specific entities."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotReadOnly));
}
