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

    // query_vc_pages — list Virtual Console pages with widget details
    if (vcBridge)
    {
        tm.register_tool(Tool(
            "vc_query_pages",
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
                            // Slider extended properties (only emit non-defaults)
                            if (!d.clickAndGoType.isEmpty() && d.clickAndGoType != "none")
                                wJson["clickAndGoType"] = d.clickAndGoType.toStdString();
                            if (!d.valueDisplayStyle.isEmpty() && d.valueDisplayStyle != "dmx")
                                wJson["valueDisplayStyle"] = d.valueDisplayStyle.toStdString();
                            if (d.sliderInvertedAppearance)
                                wJson["invertedAppearance"] = true;
                            if (d.rangeLowLimit > 0)
                                wJson["rangeLowLimit"] = d.rangeLowLimit;
                            if (d.rangeHighLimit < 255)
                                wJson["rangeHighLimit"] = d.rangeHighLimit;
                            if (d.monitorEnabled)
                                wJson["monitorEnabled"] = true;
                            if (!d.gmValueMode.isEmpty())
                                wJson["gmValueMode"] = d.gmValueMode.toStdString();
                            if (!d.gmChannelMode.isEmpty())
                                wJson["gmChannelMode"] = d.gmChannelMode.toStdString();
                            if (d.parentID >= 0)
                                wJson["parentID"] = d.parentID;

                            if (!d.inputMappings.isEmpty())
                            {
                                Json inputs = Json::array();
                                for (const auto &m : d.inputMappings)
                                {
                                    Json inp = {{"universe", (int)m.universe}, {"channel", (int)m.channel},
                                                {"sourceId", m.sourceId}};
                                    if (!m.sourceName.isEmpty())
                                        inp["sourceName"] = m.sourceName.toStdString();
                                    inp["feedback"] = {
                                        {"idleValue", m.feedback.idleValue},
                                        {"activeValue", m.feedback.activeValue},
                                        {"monitorValue", m.feedback.monitorValue},
                                        {"idleChannel", m.feedback.idleMidiCh},
                                        {"activeChannel", m.feedback.activeMidiCh},
                                        {"monitorChannel", m.feedback.monitorMidiCh}
                                    };
                                    inputs.push_back(inp);
                                }
                                wJson["inputMappings"] = inputs;
                            }

                            if (!d.validSources.isEmpty())
                            {
                                Json vs = Json::array();
                                for (const auto &s : d.validSources)
                                    vs.push_back({{"name", s.name.toStdString()},
                                                  {"id", s.id},
                                                  {"description", s.description.toStdString()}});
                                wJson["validSources"] = vs;
                            }

                            // Button extended
                            if (!d.iconPath.isEmpty())
                                wJson["iconPath"] = d.iconPath.toStdString();
                            if (d.startupIntensityEnabled)
                            {
                                wJson["startupIntensityEnabled"] = true;
                                wJson["startupIntensity"] = d.startupIntensity;
                            }
                            if (d.flashOverride)
                                wJson["flashOverride"] = true;
                            if (d.flashForceLTP)
                                wJson["flashForceLTP"] = true;
                            if (d.stopAllFadeTime > 0)
                                wJson["stopAllFadeTime"] = d.stopAllFadeTime;

                            // Slider extended
                            if (!d.widgetStyle.isEmpty())
                                wJson["widgetStyle"] = d.widgetStyle.toStdString();
                            if (d.catchValues)
                                wJson["catchValues"] = true;

                            // Frame extended
                            if (d.multipageMode)
                            {
                                wJson["multipageMode"] = true;
                                wJson["totalPages"] = d.totalPages;
                                wJson["currentPage"] = d.currentPage;
                                wJson["pagesLoop"] = d.pagesLoop;
                                if (!d.pageLabels.isEmpty())
                                {
                                    auto arr = nlohmann::json::array();
                                    for (const auto &lbl : d.pageLabels)
                                        arr.push_back(lbl.toStdString());
                                    wJson["pageLabels"] = arr;
                                }
                            }
                            if (!d.headerVisible)
                                wJson["headerVisible"] = false;
                            if (d.enableButtonVisible)
                                wJson["enableButtonVisible"] = true;
                            if (d.collapsed)
                                wJson["collapsed"] = true;
                            if (d.soloframeMixing)
                                wJson["soloframeMixing"] = true;
                            if (d.excludeMonitoredFunctions)
                                wJson["excludeMonitoredFunctions"] = true;

                            // CueList extended
                            if (!d.nextPrevBehavior.isEmpty())
                                wJson["nextPrevBehavior"] = d.nextPrevBehavior.toStdString();
                            if (!d.playbackLayout.isEmpty())
                                wJson["playbackLayout"] = d.playbackLayout.toStdString();
                            if (!d.sideFaderMode.isEmpty())
                                wJson["sideFaderMode"] = d.sideFaderMode.toStdString();

                            // Clock extended
                            if (!d.clockType.isEmpty())
                                wJson["clockType"] = d.clockType.toStdString();
                            if (d.countdownH > 0 || d.countdownM > 0 || d.countdownS > 0)
                            {
                                wJson["countdownHours"] = d.countdownH;
                                wJson["countdownMinutes"] = d.countdownM;
                                wJson["countdownSeconds"] = d.countdownS;
                            }
                            if (!d.clockSchedules.isEmpty())
                            {
                                Json schedArr = Json::array();
                                for (auto &sch : d.clockSchedules)
                                    schedArr.push_back({{"functionID", (int)sch.functionID},
                                                        {"hour", sch.hour}, {"minute", sch.minute}, {"second", sch.second}});
                                wJson["schedules"] = schedArr;
                            }

                            // SpeedDial extended
                            if (!d.speedDialFunctions.isEmpty())
                            {
                                Json funcArr = Json::array();
                                for (auto &f : d.speedDialFunctions)
                                    funcArr.push_back({{"functionID", (int)f.functionID},
                                                       {"fadeInMultiplier", f.fadeInMultiplier.toStdString()},
                                                       {"fadeOutMultiplier", f.fadeOutMultiplier.toStdString()},
                                                       {"durationMultiplier", f.durationMultiplier.toStdString()}});
                                wJson["speedDialFunctions"] = funcArr;
                            }
                            if (!d.speedDialPresets.isEmpty())
                            {
                                Json presetArr = Json::array();
                                for (auto &p : d.speedDialPresets)
                                    presetArr.push_back({{"name", p.name.toStdString()}, {"value", p.value}});
                                wJson["speedDialPresets"] = presetArr;
                            }

                            // Matrix extended
                            if (d.matrixVisibilityMask > 0)
                                wJson["visibilityMask"] = (int)d.matrixVisibilityMask;
                            if (d.matrixInstantApply)
                                wJson["instantApply"] = true;
                            if (d.matrixColor1.isValid())
                                wJson["color1"] = d.matrixColor1.name().toStdString();
                            if (d.matrixColor2.isValid())
                                wJson["color2"] = d.matrixColor2.name().toStdString();
                            if (d.matrixColor3.isValid())
                                wJson["color3"] = d.matrixColor3.name().toStdString();
                            if (d.matrixColor4.isValid())
                                wJson["color4"] = d.matrixColor4.name().toStdString();
                            if (d.matrixColor5.isValid())
                                wJson["color5"] = d.matrixColor5.name().toStdString();
                            if (!d.matrixAnimation.isEmpty())
                                wJson["animation"] = d.matrixAnimation.toStdString();

                            // XY Pad presets
                            if (!d.xyPadPresets.isEmpty())
                            {
                                Json presetArr = Json::array();
                                for (auto &p : d.xyPadPresets)
                                {
                                    Json pj = {{"name", p.name.toStdString()}, {"type", p.type.toStdString()}};
                                    if (p.type == "position")
                                        pj["position"] = {{"x", p.position.x()}, {"y", p.position.y()}};
                                    if (p.functionID != (quint32)-1)
                                        pj["functionID"] = (int)p.functionID;
                                    presetArr.push_back(pj);
                                }
                                wJson["presets"] = presetArr;
                            }

                            // Base widget extended
                            if (!d.backgroundImage.isEmpty())
                                wJson["backgroundImage"] = d.backgroundImage.toStdString();
                            if (d.disabled)
                                wJson["disabled"] = true;
                            if (d.fontConfig.family.has_value() || d.fontConfig.pointSize.has_value())
                            {
                                Json fontJson;
                                if (d.fontConfig.family.has_value())
                                    fontJson["family"] = d.fontConfig.family->toStdString();
                                if (d.fontConfig.pointSize.has_value())
                                    fontJson["size"] = *d.fontConfig.pointSize;
                                if (d.fontConfig.bold.has_value())
                                    fontJson["bold"] = *d.fontConfig.bold;
                                if (d.fontConfig.italic.has_value())
                                    fontJson["italic"] = *d.fontConfig.italic;
                                wJson["font"] = fontJson;
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
            std::string("List all Virtual Console pages and their widgets with details."),
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
                for (auto &wid : args.at("widgetIDs"))
                {
                    int id = wid.get<int>();
                    auto d = vcBridge->getWidgetDetails(id);
                    if (d.id < 0)
                    {
                        results.push_back({{"id", id}, {"error", "not found"}});
                        continue;
                    }
                    Json entry;
                    entry["id"] = d.id;
                    entry["type"] = d.type.toStdString();
                    entry["caption"] = d.caption.toStdString();
                    entry["geometry"] = {{"x", d.geometry.x()}, {"y", d.geometry.y()},
                                         {"width", d.geometry.width()}, {"height", d.geometry.height()}};
                    entry["parentID"] = d.parentID;

                    if (d.functionID != 0 && d.functionID != (quint32)-1)
                        entry["functionID"] = (int)d.functionID;
                    if (!d.action.isEmpty())
                        entry["action"] = d.action.toStdString();
                    if (!d.sliderMode.isEmpty())
                        entry["sliderMode"] = d.sliderMode.toStdString();

                    if (!d.channels.isEmpty())
                    {
                        Json chArr = Json::array();
                        for (auto &ch : d.channels)
                            chArr.push_back({{"fixtureID", (int)ch.first}, {"channel", (int)ch.second}});
                        entry["channels"] = chArr;
                    }

                    Json inputs = Json::array();
                    for (const auto &m : d.inputMappings)
                    {
                        Json inp = {{"universe", (int)m.universe}, {"channel", (int)m.channel},
                                    {"sourceId", m.sourceId}};
                        if (!m.sourceName.isEmpty())
                            inp["sourceName"] = m.sourceName.toStdString();
                        inp["feedback"] = {
                            {"idleValue", m.feedback.idleValue},
                            {"activeValue", m.feedback.activeValue},
                            {"monitorValue", m.feedback.monitorValue},
                            {"idleChannel", m.feedback.idleMidiCh},
                            {"activeChannel", m.feedback.activeMidiCh},
                            {"monitorChannel", m.feedback.monitorMidiCh}
                        };
                        inputs.push_back(inp);
                    }
                    if (!inputs.empty())
                        entry["inputMappings"] = inputs;

                    if (d.bgColor.isValid())
                        entry["bgColor"] = d.bgColor.name().toStdString();
                    if (d.fgColor.isValid())
                        entry["fgColor"] = d.fgColor.name().toStdString();

                    if (!d.validSources.isEmpty())
                    {
                        Json vs = Json::array();
                        for (const auto &s : d.validSources)
                            vs.push_back({{"name", s.name.toStdString()},
                                          {"id", s.id},
                                          {"description", s.description.toStdString()}});
                        entry["validSources"] = vs;
                    }

                    // XY Pad specific fields
                    if (!d.displayMode.isEmpty())
                    {
                        entry["displayMode"] = d.displayMode.toStdString();
                        entry["invertedAppearance"] = d.invertedAppearance;
                        entry["position"] = {{"x", d.xyPadPosition.x()}, {"y", d.xyPadPosition.y()}};

                        Json fxArr = Json::array();
                        for (const auto &fx : d.xyPadFixtures)
                        {
                            Json fxEntry;
                            fxEntry["fixtureID"] = (int)fx.fixtureID;
                            fxEntry["head"] = fx.head;
                            fxEntry["name"] = fx.name.toStdString();
                            fxEntry["xMin"] = fx.xMin;
                            fxEntry["xMax"] = fx.xMax;
                            fxEntry["xReverse"] = fx.xReverse;
                            fxEntry["yMin"] = fx.yMin;
                            fxEntry["yMax"] = fx.yMax;
                            fxEntry["yReverse"] = fx.yReverse;
                            if (fx.panDegreesMax > 0 || fx.tiltDegreesMax > 0)
                            {
                                fxEntry["panDegreesMax"] = fx.panDegreesMax;
                                fxEntry["tiltDegreesMax"] = fx.tiltDegreesMax;
                            }
                            fxArr.push_back(fxEntry);
                        }
                        entry["fixtures"] = fxArr;
                    }

                    // Audio Triggers specific fields
                    if (!d.audioBars.isEmpty())
                    {
                        entry["captureEnabled"] = d.captureEnabled;
                        entry["volumeLevel"] = d.volumeLevel;
                        entry["barsNumber"] = d.barsNumber;

                        Json barsArr = Json::array();
                        for (const auto &bar : d.audioBars)
                        {
                            Json barEntry;
                            barEntry["barIndex"] = bar.barIndex;
                            barEntry["type"] = bar.type.toStdString();
                            barEntry["minThreshold"] = bar.minThreshold;
                            barEntry["maxThreshold"] = bar.maxThreshold;
                            if (bar.type == "function" && bar.functionID != (quint32)-1)
                            {
                                barEntry["functionID"] = (int)bar.functionID;
                                barEntry["functionName"] = bar.functionName.toStdString();
                            }
                            if (bar.type == "widget" && bar.widgetID != (quint32)-1)
                            {
                                barEntry["widgetID"] = (int)bar.widgetID;
                                barEntry["widgetName"] = bar.widgetName.toStdString();
                            }
                            barsArr.push_back(barEntry);
                        }
                        entry["bars"] = barsArr;
                    }

                    // Button extended
                    if (!d.iconPath.isEmpty())
                        entry["iconPath"] = d.iconPath.toStdString();
                    if (d.startupIntensityEnabled)
                    {
                        entry["startupIntensityEnabled"] = true;
                        entry["startupIntensity"] = d.startupIntensity;
                    }
                    if (d.flashOverride)
                        entry["flashOverride"] = true;
                    if (d.flashForceLTP)
                        entry["flashForceLTP"] = true;
                    if (d.stopAllFadeTime > 0)
                        entry["stopAllFadeTime"] = d.stopAllFadeTime;

                    // Slider extended
                    if (!d.widgetStyle.isEmpty())
                        entry["widgetStyle"] = d.widgetStyle.toStdString();
                    if (d.catchValues)
                        entry["catchValues"] = true;

                    // Frame extended
                    if (d.multipageMode)
                    {
                        entry["multipageMode"] = true;
                        entry["totalPages"] = d.totalPages;
                        entry["currentPage"] = d.currentPage;
                        entry["pagesLoop"] = d.pagesLoop;
                        if (!d.pageLabels.isEmpty())
                        {
                            auto arr = nlohmann::json::array();
                            for (const auto &lbl : d.pageLabels)
                                arr.push_back(lbl.toStdString());
                            entry["pageLabels"] = arr;
                        }
                    }
                    if (!d.headerVisible)
                        entry["headerVisible"] = false;
                    if (d.enableButtonVisible)
                        entry["enableButtonVisible"] = true;
                    if (d.collapsed)
                        entry["collapsed"] = true;
                    if (d.soloframeMixing)
                        entry["soloframeMixing"] = true;
                    if (d.excludeMonitoredFunctions)
                        entry["excludeMonitoredFunctions"] = true;

                    // CueList extended
                    if (!d.nextPrevBehavior.isEmpty())
                        entry["nextPrevBehavior"] = d.nextPrevBehavior.toStdString();
                    if (!d.playbackLayout.isEmpty())
                        entry["playbackLayout"] = d.playbackLayout.toStdString();
                    if (!d.sideFaderMode.isEmpty())
                        entry["sideFaderMode"] = d.sideFaderMode.toStdString();

                    // Clock extended
                    if (!d.clockType.isEmpty())
                        entry["clockType"] = d.clockType.toStdString();
                    if (d.countdownH > 0 || d.countdownM > 0 || d.countdownS > 0)
                    {
                        entry["countdownHours"] = d.countdownH;
                        entry["countdownMinutes"] = d.countdownM;
                        entry["countdownSeconds"] = d.countdownS;
                    }
                    if (!d.clockSchedules.isEmpty())
                    {
                        Json schedArr = Json::array();
                        for (auto &sch : d.clockSchedules)
                            schedArr.push_back({{"functionID", (int)sch.functionID},
                                                {"hour", sch.hour}, {"minute", sch.minute}, {"second", sch.second}});
                        entry["schedules"] = schedArr;
                    }

                    // SpeedDial extended
                    if (!d.speedDialFunctions.isEmpty())
                    {
                        Json funcArr = Json::array();
                        for (auto &f : d.speedDialFunctions)
                            funcArr.push_back({{"functionID", (int)f.functionID},
                                               {"fadeInMultiplier", f.fadeInMultiplier.toStdString()},
                                               {"fadeOutMultiplier", f.fadeOutMultiplier.toStdString()},
                                               {"durationMultiplier", f.durationMultiplier.toStdString()}});
                        entry["speedDialFunctions"] = funcArr;
                    }
                    if (!d.speedDialPresets.isEmpty())
                    {
                        Json presetArr = Json::array();
                        for (auto &p : d.speedDialPresets)
                            presetArr.push_back({{"name", p.name.toStdString()}, {"value", p.value}});
                        entry["speedDialPresets"] = presetArr;
                    }

                    // Matrix extended
                    if (d.matrixVisibilityMask > 0)
                        entry["visibilityMask"] = (int)d.matrixVisibilityMask;
                    if (d.matrixInstantApply)
                        entry["instantApply"] = true;
                    if (d.matrixColor1.isValid())
                        entry["color1"] = d.matrixColor1.name().toStdString();
                    if (d.matrixColor2.isValid())
                        entry["color2"] = d.matrixColor2.name().toStdString();
                    if (d.matrixColor3.isValid())
                        entry["color3"] = d.matrixColor3.name().toStdString();
                    if (d.matrixColor4.isValid())
                        entry["color4"] = d.matrixColor4.name().toStdString();
                    if (d.matrixColor5.isValid())
                        entry["color5"] = d.matrixColor5.name().toStdString();
                    if (!d.matrixAnimation.isEmpty())
                        entry["animation"] = d.matrixAnimation.toStdString();

                    // XY Pad presets
                    if (!d.xyPadPresets.isEmpty())
                    {
                        Json presetArr = Json::array();
                        for (auto &p : d.xyPadPresets)
                        {
                            Json pj = {{"name", p.name.toStdString()}, {"type", p.type.toStdString()}};
                            if (p.type == "position")
                                pj["position"] = {{"x", p.position.x()}, {"y", p.position.y()}};
                            if (p.functionID != (quint32)-1)
                                pj["functionID"] = (int)p.functionID;
                            presetArr.push_back(pj);
                        }
                        entry["presets"] = presetArr;
                    }

                    // Base widget extended
                    if (!d.backgroundImage.isEmpty())
                        entry["backgroundImage"] = d.backgroundImage.toStdString();
                    if (d.disabled)
                        entry["disabled"] = true;
                    if (d.fontConfig.family.has_value() || d.fontConfig.pointSize.has_value())
                    {
                        Json fontJson;
                        if (d.fontConfig.family.has_value())
                            fontJson["family"] = d.fontConfig.family->toStdString();
                        if (d.fontConfig.pointSize.has_value())
                            fontJson["size"] = *d.fontConfig.pointSize;
                        if (d.fontConfig.bold.has_value())
                            fontJson["bold"] = *d.fontConfig.bold;
                        if (d.fontConfig.italic.has_value())
                            fontJson["italic"] = *d.fontConfig.italic;
                        entry["font"] = fontJson;
                    }

                    results.push_back(entry);
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
}
