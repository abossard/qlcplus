/*
  Q Light Controller Plus
  vc_create_tools.cpp

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
#include "vc_tools_common.h"
#include "idempotency.h"
#include "vcbridge.h"
#include "doc.h"
#include "function.h"

#include <fastmcpp/tools/manager.hpp>
#include <fastmcpp/tools/tool.hpp>

void registerVCCreateTools(fastmcpp::tools::ToolManager &tm, Doc *doc, VCBridge *vcBridge)
{
    using Json = nlohmann::json;
    using Tool = fastmcpp::tools::Tool;

    if (!vcBridge) return;

    // Helper: parse SliderConfig from JSON (only sets fields that are present)
    auto parseSliderConfig = [](const Json &item) -> VCBridge::SliderConfig {
        VCBridge::SliderConfig cfg;
        if (item.contains("clickAndGoType"))
            cfg.clickAndGoType = QString::fromStdString(item.at("clickAndGoType").get<std::string>());
        if (item.contains("valueDisplayStyle"))
            cfg.valueDisplayStyle = QString::fromStdString(item.at("valueDisplayStyle").get<std::string>());
        if (item.contains("invertedAppearance"))
            cfg.invertedAppearance = item.at("invertedAppearance").get<bool>();
        if (item.contains("rangeLowLimit"))
            cfg.rangeLowLimit = item.at("rangeLowLimit").get<double>();
        if (item.contains("rangeHighLimit"))
            cfg.rangeHighLimit = item.at("rangeHighLimit").get<double>();
        if (item.contains("monitorEnabled"))
            cfg.monitorEnabled = item.at("monitorEnabled").get<bool>();
        if (item.contains("gmValueMode"))
            cfg.gmValueMode = QString::fromStdString(item.at("gmValueMode").get<std::string>());
        if (item.contains("gmChannelMode"))
            cfg.gmChannelMode = QString::fromStdString(item.at("gmChannelMode").get<std::string>());
        return cfg;
    };

    // Helper: check whether SliderConfig has any value set
    auto hasSliderConfig = [](const VCBridge::SliderConfig &cfg) -> bool {
        return cfg.clickAndGoType.has_value() || cfg.valueDisplayStyle.has_value()
            || cfg.invertedAppearance.has_value() || cfg.rangeLowLimit.has_value()
            || cfg.rangeHighLimit.has_value() || cfg.monitorEnabled.has_value()
            || cfg.gmValueMode.has_value() || cfg.gmChannelMode.has_value();
    };

    // vc_create_pages (batch)
    tm.register_tool(Tool(
        "vc_create_pages",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"name", {{"type", "string"}}}
            }}, {"required", {"name"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc, vcBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto itemsErr = validateItemsArray(args);
            if (itemsErr) return *itemsErr;
            Json results = Json::array();
            for (auto &item : args.at("items"))
            {
                auto err = validateFields(item, {"name"});
                if (!err.empty()) { results.push_back(nlohmann::json::parse(err)); continue; }
                QString name = QString::fromStdString(item.at("name").get<std::string>());
                int existingIdx = vcBridge->findPageByName(name);
                if (existingIdx >= 0)
                {
                    results.push_back({{"pageIndex", existingIdx}, {"status", "existing"}});
                    continue;
                }
                int idx = vcBridge->addPage(name);
                results.push_back({{"pageIndex", idx}, {"status", "created"}});
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("Create new Virtual Console pages. Batch. "
                     "Wrap multiple operations in {\"items\": [...]}. Each item is processed independently."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotIdempotent));

    // vc_create_widgets — unified widget creation tool with type discriminator
    tm.register_tool(Tool(
        "vc_create_widgets",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"type", {{"type", "string"}, {"enum", {"frame", "soloframe", "button", "slider", "xypad", "cuelist", "label", "speedDial", "audioTrigger", "matrix", "clock"}}, {"description", "Widget type to create"}}},
                {"parentID", {{"type", "integer"}, {"description", "Parent frame or page widget ID"}}},
                {"childPageIndex", {{"type", "integer"}, {"minimum", 0}, {"description", "Target page within a multipage parent frame (non-container widgets only)"}}},
                {"pageIndex", {{"type", "integer"}, {"description", "Page index (for top-level frames only)"}}},
                {"caption", {{"type", "string"}, {"description", "Widget caption/label"}}},
                {"upsert", {{"type", "boolean"}, {"description", "If true, update existing widget with same caption (default: true for most types)"}}},
                {"x", {{"type", "integer"}}}, {"y", {{"type", "integer"}}},
                {"width", {{"type", "integer"}}}, {"height", {{"type", "integer"}}},
                {"bgColor", {{"type", "string"}, {"description", "Background color hex (e.g. #1a3300)"}}},
                {"fgColor", {{"type", "string"}, {"description", "Foreground/text color hex (e.g. #ffffff)"}}},
                {"functionID", {{"type", "integer"}}},
                {"functionName", {{"type", "string"}, {"description", "Function name. Alternative to functionID."}}},
                {"solo", {{"type", "boolean"}, {"description", "Frame: if true, only one child can be active at a time"}}},
                {"multipageMode", {{"type", "boolean"}, {"description", "Frame: enable multipage mode"}}},
                {"totalPages", {{"type", "integer"}, {"description", "Frame: total number of pages"}}},
                {"pagesLoop", {{"type", "boolean"}, {"description", "Frame: loop pages"}}},
                {"pageLabels", {{"type", "array"}, {"items", {{"type", "string"}}}, {"description", "Frame: page names (one per page, ordered by index)"}}},
                {"headerVisible", {{"type", "boolean"}, {"description", "Frame: show header"}}},
                {"enableButtonVisible", {{"type", "boolean"}, {"description", "Frame: show enable button"}}},
                {"soloframeMixing", {{"type", "boolean"}, {"description", "SoloFrame: allow mixing"}}},
                {"excludeMonitoredFunctions", {{"type", "boolean"}, {"description", "SoloFrame: exclude monitored functions"}}},
                {"action", {{"type", "string"}, {"enum", {"toggle", "flash", "blackout", "stopall"}}, {"description", "Button action type"}}},
                {"stopAllFadeTime", {{"type", "integer"}, {"description", "Button: fade time in ms for stopall action"}}},
                {"iconPath", {{"type", "string"}, {"description", "Button: icon file path"}}},
                {"keySequence", {{"type", "string"}, {"description", "Button: keyboard shortcut (e.g. 'Ctrl+A')"}}},
                {"startupIntensityEnabled", {{"type", "boolean"}, {"description", "Button: enable startup intensity"}}},
                {"startupIntensity", {{"type", "number"}, {"description", "Button: startup intensity 0.0-1.0"}}},
                {"flashOverride", {{"type", "boolean"}, {"description", "Button: flash overrides other values"}}},
                {"flashForceLTP", {{"type", "boolean"}, {"description", "Button: flash forces LTP"}}},
                {"mode", {{"type", "string"}, {"enum", {"level", "playback", "submaster", "grandmaster"}}, {"description", "Slider mode"}}},
                {"widgetStyle", {{"type", "string"}, {"enum", {"slider", "knob"}}, {"description", "Slider: visual style"}}},
                {"channels", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                    {"fixtureID", {{"type", "integer"}}}, {"channel", {{"type", "integer"}}}
                }}}}, {"description", "Slider: fixture channels to control (for level mode)"}}},
                {"clickAndGoType", {{"type", "string"}, {"enum", {"none", "colors", "preset", "rgb", "cmy"}}}},
                {"valueDisplayStyle", {{"type", "string"}, {"enum", {"dmx", "percentage"}}}},
                {"invertedAppearance", {{"type", "boolean"}}},
                {"rangeLowLimit", {{"type", "number"}}},
                {"rangeHighLimit", {{"type", "number"}}},
                {"monitorEnabled", {{"type", "boolean"}}},
                {"gmValueMode", {{"type", "string"}, {"enum", {"limit", "reduce"}}}},
                {"gmChannelMode", {{"type", "string"}, {"enum", {"intensity", "allchannels"}}}},
                {"size", {{"type", "integer"}, {"description", "XY Pad: width and height (square)"}}},
                {"fixtureIDs", {{"type", "array"}, {"items", {{"type", "integer"}}}, {"description", "XY Pad: simple fixture ID list"}}},
                {"fixtures", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                    {"fixtureID", {{"type", "integer"}}},
                    {"head", {{"type", "integer"}}},
                    {"xMin", {{"type", "number"}}}, {"xMax", {{"type", "number"}}}, {"xReverse", {{"type", "boolean"}}},
                    {"yMin", {{"type", "number"}}}, {"yMax", {{"type", "number"}}}, {"yReverse", {{"type", "boolean"}}}
                }}, {"required", {"fixtureID"}}}}, {"description", "XY Pad: per-fixture config with axis ranges"}}},
                {"displayMode", {{"type", "string"}, {"enum", {"degrees", "percentage", "dmx"}}}},
                {"chaserID", {{"type", "integer"}}},
                {"chaserName", {{"type", "string"}, {"description", "Chaser name. Alternative to chaserID."}}},
                {"nextPrevBehavior", {{"type", "string"}, {"enum", {"defaultRunFirst", "runNext", "select", "nothing"}}, {"description", "CueList: next/prev button behavior"}}},
                {"playbackLayout", {{"type", "string"}, {"enum", {"playPauseStop", "playStopPause"}}, {"description", "CueList: playback buttons layout"}}},
                {"sideFaderMode", {{"type", "string"}, {"enum", {"none", "crossfade", "steps"}}, {"description", "CueList: side fader mode"}}},
                {"text", {{"type", "string"}, {"description", "Label text content"}}},
                {"functionIDs", {{"type", "array"}, {"items", {{"type", "integer"}}}, {"description", "SpeedDial: function IDs to control (simple)"}}},
                {"functions", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                    {"functionID", {{"type", "integer"}}},
                    {"fadeInMultiplier", {{"type", "string"}}},
                    {"fadeOutMultiplier", {{"type", "string"}}},
                    {"durationMultiplier", {{"type", "string"}}}
                }}, {"required", {"functionID"}}}}, {"description", "SpeedDial: functions with multipliers"}}},
                {"absoluteValueMin", {{"type", "integer"}, {"description", "SpeedDial: absolute value range min (ms)"}}},
                {"absoluteValueMax", {{"type", "integer"}, {"description", "SpeedDial: absolute value range max (ms)"}}},
                {"visibilityMask", {{"type", "integer"}, {"description", "SpeedDial/Matrix: visibility bitmask"}}},
                {"resetFactorOnDialChange", {{"type", "boolean"}, {"description", "SpeedDial: reset factor on dial change"}}},
                {"volumeLevel", {{"type", "integer"}, {"description", "AudioTrigger: audio input volume 0-255"}}},
                {"barsNumber", {{"type", "integer"}, {"description", "AudioTrigger: legacy compatibility field; QLC+ 5 uses a fixed six-band mapping"}}},
                {"clockType", {{"type", "string"}, {"enum", {"clock", "stopwatch", "countdown"}}, {"description", "Clock type"}}},
                {"countdownHours", {{"type", "integer"}, {"description", "Clock: countdown hours"}}},
                {"countdownMinutes", {{"type", "integer"}, {"description", "Clock: countdown minutes"}}},
                {"countdownSeconds", {{"type", "integer"}, {"description", "Clock: countdown seconds"}}},
                {"schedules", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                    {"functionID", {{"type", "integer"}}},
                    {"functionName", {{"type", "string"}}},
                    {"hour", {{"type", "integer"}}},
                    {"minute", {{"type", "integer"}}},
                    {"second", {{"type", "integer"}}}
                }}}}, {"description", "Clock: scheduled function triggers"}}},
                {"instantApply", {{"type", "boolean"}, {"description", "Matrix: instant apply changes"}}},
                {"color1", {{"type", "string"}, {"description", "Matrix: color 1 hex (e.g. '#ff0000')"}}},
                {"color2", {{"type", "string"}, {"description", "Matrix: color 2 hex"}}},
                {"color3", {{"type", "string"}, {"description", "Matrix: color 3 hex"}}},
                {"color4", {{"type", "string"}, {"description", "Matrix: color 4 hex"}}},
                {"color5", {{"type", "string"}, {"description", "Matrix: color 5 hex"}}},
                {"colors", {{"type", "array"}, {"items", {{"type", "string"}}}, {"maxItems", 5}, {"description", "Matrix: colors array (overrides individual colorN fields)"}}},
                {"animation", {{"type", "string"}, {"description", "Matrix: animation algorithm name"}}},
                {"targetFolder", {{"type", "string"}, {"description", "RecordPanel: function folder path for created scenes/chasers"}}},
                {"scenePrefix", {{"type", "string"}, {"description", "RecordPanel: name prefix for created scenes"}}},
                {"chaserPrefix", {{"type", "string"}, {"description", "RecordPanel: name prefix for created chasers"}}},
                {"defaultFadeIn", {{"type", "integer"}, {"description", "RecordPanel: default fade-in time in ms"}}},
                {"defaultHold", {{"type", "integer"}, {"description", "RecordPanel: default hold time in ms"}}},
                {"defaultFadeOut", {{"type", "integer"}, {"description", "RecordPanel: default fade-out time in ms"}}}
            }}, {"required", {"type"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc, vcBridge, parseSliderConfig, hasSliderConfig](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto itemsErr = validateItemsArray(args);
            if (itemsErr) return *itemsErr;
            Json results = Json::array();
            for (auto &item : args.at("items"))
            {
                // 1. Parse type discriminator
                if (!item.contains("type") || !item.at("type").is_string())
                {
                    results.push_back({{"error", "\"type\" is required and must be a string"}});
                    continue;
                }
                std::string typeStr = item.at("type").get<std::string>();
                int widgetType = VCType::fromString(typeStr);
                if (widgetType == VCType::Unknown)
                {
                    results.push_back({{"error", "invalid widget type '" + typeStr +
                        "'. Must be one of: frame, soloframe, button, slider, xypad, cuelist, label, speedDial, audioTrigger, matrix, clock"}});
                    continue;
                }

                // 2. Validate all fields for this widget type
                // For frames, pageIndex is also allowed
                auto validationErr = VCValidate::validate(item, widgetType, true);
                if (!validationErr.empty()) { results.push_back(nlohmann::json::parse(validationErr)); continue; }

                if (item.contains("childPageIndex"))
                {
                    const int parentID = item.at("parentID").get<int>();
                    const int pageIndex = item.at("childPageIndex").get<int>();
                    const VCBridge::WidgetDetails parent = vcBridge->getWidgetDetails(parentID);
                    if (parent.id < 0 || pageIndex >= parent.totalPages)
                    {
                        const std::string range = parent.totalPages > 0
                            ? "0-" + std::to_string(parent.totalPages - 1)
                            : "empty";
                        results.push_back({{"error", "childPageIndex " + std::to_string(pageIndex) +
                            " is outside the parent frame page range " + range +
                            " (parentID " + std::to_string(parentID) + ")"}});
                        continue;
                    }
                }

                // 3. Dispatch to the appropriate creation logic based on type
                switch (widgetType)
                {
                case VCType::Frame:
                case VCType::SoloFrame:
                {
                    QString caption = QString::fromStdString(item.value("caption", ""));
                    bool solo = (widgetType == VCType::SoloFrame) || item.value("solo", false);
                    bool hasPage = item.contains("pageIndex");

                    // Check for existing frame with same caption
                    if (!caption.isEmpty())
                    {
                        if (hasPage)
                        {
                            int pageIndex = item.at("pageIndex").get<int>();
                            int existingId = -1;
                            for (const auto &page : vcBridge->pages())
                            {
                                if (page.index == pageIndex)
                                {
                                    for (const auto &w : page.widgets)
                                    {
                                        if ((w.type == "Frame" || w.type == "Solo Frame") && w.caption == caption)
                                        {
                                            existingId = w.id;
                                            break;
                                        }
                                    }
                                    break;
                                }
                            }
                            if (existingId >= 0)
                            {
                                results.push_back({{"widgetID", existingId}, {"status", "existing"}});
                                continue;
                            }
                        }
                        else
                        {
                            int parentID = item.at("parentID").get<int>();
                            int existingId = vcBridge->findWidgetByCaption(parentID, "Frame", caption);
                            if (existingId < 0)
                                existingId = vcBridge->findWidgetByCaption(parentID, "Solo frame", caption);
                            if (existingId >= 0)
                            {
                                results.push_back({{"widgetID", existingId}, {"status", "existing"}});
                                continue;
                            }
                        }
                    }

                    int w = item.value("width", 400);
                    int h = item.value("height", 300);
                    QRect geo;
                    if (item.contains("x") && item.contains("y"))
                        geo = QRect(item.at("x").get<int>(), item.at("y").get<int>(), w, h);
                    else
                        geo = QRect(5, 5, w, h);

                    int id;
                    if (hasPage)
                        id = vcBridge->addFrame(item.at("pageIndex").get<int>(), geo, caption, solo);
                    else
                        id = vcBridge->addFrameInFrame(item.at("parentID").get<int>(), geo, caption, solo);

                    results.push_back({{"widgetID", id}, {"status", id >= 0 ? "created" : "failed"}});
                    if (id >= 0 && (item.contains("bgColor") || item.contains("fgColor")))
                    {
                        QColor bg = item.contains("bgColor") ? QColor(QString::fromStdString(item.at("bgColor").get<std::string>())) : QColor();
                        QColor fg = item.contains("fgColor") ? QColor(QString::fromStdString(item.at("fgColor").get<std::string>())) : QColor();
                        vcBridge->setWidgetColors(id, bg, fg);
                    }
                    if (id >= 0)
                    {
                        VCBridge::FrameConfig frameCfg;
                        bool hasFrameCfg = false;
                        if (item.contains("multipageMode")) { frameCfg.multipageMode = item["multipageMode"].get<bool>(); hasFrameCfg = true; }
                        if (item.contains("totalPages")) { frameCfg.totalPages = item["totalPages"].get<int>(); hasFrameCfg = true; }
                        if (item.contains("pagesLoop")) { frameCfg.pagesLoop = item["pagesLoop"].get<bool>(); hasFrameCfg = true; }
                        if (item.contains("pageLabels"))
                        {
                            QStringList labels;
                            for (auto &lbl : item["pageLabels"])
                                labels.append(QString::fromStdString(lbl.get<std::string>()));
                            frameCfg.pageLabels = labels;
                            hasFrameCfg = true;
                        }
                        if (item.contains("headerVisible")) { frameCfg.headerVisible = item["headerVisible"].get<bool>(); hasFrameCfg = true; }
                        if (item.contains("enableButtonVisible")) { frameCfg.enableButtonVisible = item["enableButtonVisible"].get<bool>(); hasFrameCfg = true; }
                        if (item.contains("soloframeMixing")) { frameCfg.soloframeMixing = item["soloframeMixing"].get<bool>(); hasFrameCfg = true; }
                        if (item.contains("excludeMonitoredFunctions")) { frameCfg.excludeMonitoredFunctions = item["excludeMonitoredFunctions"].get<bool>(); hasFrameCfg = true; }
                        if (hasFrameCfg) vcBridge->configureFrame(id, frameCfg);
                    }
                    break;
                }

                case VCType::Button:
                {
                    int parentID = item.at("parentID").get<int>();
                    QString caption = QString::fromStdString(item.value("caption", ""));
                    if (!caption.isEmpty())
                    {
                        int existingId = vcBridge->findWidgetByCaption(parentID, "Button", caption);
                        if (existingId >= 0)
                        {
                            if (item.contains("childPageIndex") &&
                                !vcBridge->setWidgetPage(existingId, item["childPageIndex"].get<int>()))
                            {
                                results.push_back({{"widgetID", existingId}, {"error", "childPageIndex is outside the parent frame page range"}});
                                continue;
                            }
                            results.push_back({{"widgetID", existingId}, {"status", "existing"}});
                            continue;
                        }
                    }

                    int funcID = -1;
                    if (item.contains("functionName"))
                    {
                        quint32 fid = mcp::resolveFunctionByName(doc, QString::fromStdString(item.at("functionName").get<std::string>()));
                        if (fid != Function::invalidId()) funcID = (int)fid;
                    }
                    if (item.contains("functionID"))
                        funcID = item.at("functionID").get<int>();

                    int w = item.value("width", 100);
                    int h = item.value("height", 60);
                    QRect geo;
                    if (item.contains("x") && item.contains("y"))
                        geo = QRect(item.at("x").get<int>(), item.at("y").get<int>(), w, h);
                    else
                        geo = vcBridge->nextWidgetPosition(parentID, w, h);

                    int id = vcBridge->addButton(
                        parentID, geo,
                        funcID >= 0 ? (quint32)funcID : Function::invalidId(),
                        caption,
                        QString::fromStdString(item.value("action", "toggle")),
                        item.value("stopAllFadeTime", 0));

                    results.push_back({{"widgetID", id}, {"status", "created"}});
                    if (id >= 0 && (item.contains("bgColor") || item.contains("fgColor")))
                    {
                        QColor bg = item.contains("bgColor") ? QColor(QString::fromStdString(item.at("bgColor").get<std::string>())) : QColor();
                        QColor fg = item.contains("fgColor") ? QColor(QString::fromStdString(item.at("fgColor").get<std::string>())) : QColor();
                        vcBridge->setWidgetColors(id, bg, fg);
                    }
                    if (id >= 0)
                    {
                        VCBridge::ButtonConfig btnCfg;
                        bool hasBtnCfg = false;
                        if (item.contains("iconPath")) { btnCfg.iconPath = QString::fromStdString(item["iconPath"].get<std::string>()); hasBtnCfg = true; }
                        if (item.contains("startupIntensityEnabled")) { btnCfg.startupIntensityEnabled = item["startupIntensityEnabled"].get<bool>(); hasBtnCfg = true; }
                        if (item.contains("startupIntensity")) { btnCfg.startupIntensity = item["startupIntensity"].get<double>(); hasBtnCfg = true; }
                        if (item.contains("flashOverride")) { btnCfg.flashOverride = item["flashOverride"].get<bool>(); hasBtnCfg = true; }
                        if (item.contains("flashForceLTP")) { btnCfg.flashForceLTP = item["flashForceLTP"].get<bool>(); hasBtnCfg = true; }
                        if (hasBtnCfg) vcBridge->configureButton(id, btnCfg);
                        if (item.contains("keySequence"))
                            vcBridge->setWidgetKeySequence(id, "default", QKeySequence(QString::fromStdString(item["keySequence"].get<std::string>())));
                    }
                    break;
                }

                case VCType::Slider:
                {
                    int parentID = item.at("parentID").get<int>();
                    QString caption = QString::fromStdString(item.value("caption", ""));
                    auto sliderCfg = parseSliderConfig(item);

                    // Upsert: find existing slider by caption, update if found
                    if (!caption.isEmpty())
                    {
                        int existingId = vcBridge->findWidgetByCaption(parentID, "Slider", caption);
                        if (existingId >= 0)
                        {
                            if (hasSliderConfig(sliderCfg))
                                vcBridge->configureSlider(existingId, sliderCfg);
                            if (item.contains("bgColor") || item.contains("fgColor"))
                            {
                                QColor bg = item.contains("bgColor") ? QColor(QString::fromStdString(item.at("bgColor").get<std::string>())) : QColor();
                                QColor fg = item.contains("fgColor") ? QColor(QString::fromStdString(item.at("fgColor").get<std::string>())) : QColor();
                                vcBridge->setWidgetColors(existingId, bg, fg);
                            }
                            if (item.contains("childPageIndex") &&
                                !vcBridge->setWidgetPage(existingId, item["childPageIndex"].get<int>()))
                            {
                                results.push_back({{"widgetID", existingId}, {"error", "childPageIndex is outside the parent frame page range"}});
                                continue;
                            }
                            results.push_back({{"widgetID", existingId}, {"status", "updated"}});
                            continue;
                        }
                    }

                    int w = item.value("width", 60);
                    int h = item.value("height", 200);
                    QRect geo;
                    if (item.contains("x") && item.contains("y"))
                        geo = QRect(item.at("x").get<int>(), item.at("y").get<int>(), w, h);
                    else
                        geo = vcBridge->nextWidgetPosition(parentID, w, h);

                    QList<QPair<quint32, quint32>> channels;
                    if (item.contains("channels"))
                    {
                        for (auto &ch : item.at("channels"))
                        {
                            auto chErr = validateFields(ch, {"fixtureID", "channel"});
                            if (!chErr.empty()) { results.push_back(nlohmann::json::parse(chErr)); continue; }
                            channels.append({ch.at("fixtureID").get<int>(), ch.at("channel").get<int>()});
                        }
                    }

                    int funcID = item.value("functionID", -1);
                    if (funcID < 0 && item.contains("functionName"))
                    {
                        quint32 fid = mcp::resolveFunctionByName(doc, QString::fromStdString(item.at("functionName").get<std::string>()));
                        if (fid != Function::invalidId()) funcID = (int)fid;
                    }

                    int id = vcBridge->addSlider(
                        parentID, geo,
                        QString::fromStdString(item.value("mode", "level")),
                        caption,
                        funcID,
                        channels);
                    results.push_back({{"widgetID", id}, {"status", "created"}});
                    if (id >= 0)
                    {
                        if (hasSliderConfig(sliderCfg))
                            vcBridge->configureSlider(id, sliderCfg);
                        if (item.contains("widgetStyle"))
                            vcBridge->setSliderWidgetStyle(id, QString::fromStdString(item["widgetStyle"].get<std::string>()));
                        if (item.contains("bgColor") || item.contains("fgColor"))
                        {
                            QColor bg = item.contains("bgColor") ? QColor(QString::fromStdString(item.at("bgColor").get<std::string>())) : QColor();
                            QColor fg = item.contains("fgColor") ? QColor(QString::fromStdString(item.at("fgColor").get<std::string>())) : QColor();
                            vcBridge->setWidgetColors(id, bg, fg);
                        }
                    }
                    break;
                }

                case VCType::XYPad:
                {
                    int parentID = item.at("parentID").get<int>();
                    if (!item.contains("fixtureIDs") && !item.contains("fixtures"))
                    {
                        results.push_back({{"error", "Either fixtureIDs or fixtures is required"}});
                        continue;
                    }

                    int sz = item.value("size", 200);
                    QRect geo;
                    if (item.contains("x") && item.contains("y"))
                        geo = QRect(item.at("x").get<int>(), item.at("y").get<int>(), sz, sz);
                    else
                        geo = vcBridge->nextWidgetPosition(parentID, sz, sz);

                    QString displayMode = "degrees";
                    if (item.contains("displayMode"))
                        displayMode = QString::fromStdString(item.at("displayMode").get<std::string>());
                    bool inverted = item.contains("invertedAppearance") && item.at("invertedAppearance").get<bool>();

                    int id = -1;
                    if (item.contains("fixtures"))
                    {
                        QList<VCBridge::XYPadFixtureConfig> configs;
                        for (auto &fx : item.at("fixtures"))
                        {
                            VCBridge::XYPadFixtureConfig cfg;
                            cfg.fixtureID = fx.at("fixtureID").get<int>();
                            if (fx.contains("head")) cfg.head = fx.at("head").get<int>();
                            if (fx.contains("xMin")) cfg.xMin = fx.at("xMin").get<double>();
                            if (fx.contains("xMax")) cfg.xMax = fx.at("xMax").get<double>();
                            if (fx.contains("xReverse")) cfg.xReverse = fx.at("xReverse").get<bool>();
                            if (fx.contains("yMin")) cfg.yMin = fx.at("yMin").get<double>();
                            if (fx.contains("yMax")) cfg.yMax = fx.at("yMax").get<double>();
                            if (fx.contains("yReverse")) cfg.yReverse = fx.at("yReverse").get<bool>();
                            configs.append(cfg);
                        }
                        id = vcBridge->addXYPadEx(parentID, geo, configs, displayMode, inverted);
                    }
                    else
                    {
                        QList<VCBridge::XYPadFixtureConfig> configs;
                        for (auto &fid : item.at("fixtureIDs"))
                        {
                            VCBridge::XYPadFixtureConfig cfg;
                            cfg.fixtureID = fid.get<int>();
                            configs.append(cfg);
                        }
                        id = vcBridge->addXYPadEx(parentID, geo, configs, displayMode, inverted);
                    }

                    results.push_back({{"widgetID", id}, {"status", "created"}});
                    if (id >= 0 && (item.contains("bgColor") || item.contains("fgColor")))
                    {
                        QColor bg = item.contains("bgColor") ? QColor(QString::fromStdString(item.at("bgColor").get<std::string>())) : QColor();
                        QColor fg = item.contains("fgColor") ? QColor(QString::fromStdString(item.at("fgColor").get<std::string>())) : QColor();
                        vcBridge->setWidgetColors(id, bg, fg);
                    }
                    break;
                }

                case VCType::CueList:
                {
                    int parentID = item.at("parentID").get<int>();
                    QString caption = QString::fromStdString(item.value("caption", ""));
                    if (!caption.isEmpty())
                    {
                        int existingId = vcBridge->findWidgetByCaption(parentID, "CueList", caption);
                        if (existingId >= 0)
                        {
                            if (item.contains("childPageIndex") &&
                                !vcBridge->setWidgetPage(existingId, item["childPageIndex"].get<int>()))
                            {
                                results.push_back({{"widgetID", existingId}, {"error", "childPageIndex is outside the parent frame page range"}});
                                continue;
                            }
                            results.push_back({{"widgetID", existingId}, {"status", "existing"}});
                            continue;
                        }
                    }

                    int w = item.value("width", 300);
                    int h = item.value("height", 200);
                    QRect geo;
                    if (item.contains("x") && item.contains("y"))
                        geo = QRect(item.at("x").get<int>(), item.at("y").get<int>(), w, h);
                    else
                        geo = vcBridge->nextWidgetPosition(parentID, w, h);

                    int chaserIDVal = -1;
                    if (item.contains("chaserID"))
                        chaserIDVal = item.at("chaserID").get<int>();
                    else if (item.contains("chaserName"))
                    {
                        quint32 fid = mcp::resolveFunctionByName(doc, QString::fromStdString(item.at("chaserName").get<std::string>()), Function::ChaserType);
                        if (fid != Function::invalidId()) chaserIDVal = (int)fid;
                    }

                    int id = vcBridge->addCueList(parentID, geo, chaserIDVal, caption);
                    results.push_back({{"widgetID", id}, {"status", "created"}});
                    if (id >= 0 && (item.contains("bgColor") || item.contains("fgColor")))
                    {
                        QColor bg = item.contains("bgColor") ? QColor(QString::fromStdString(item.at("bgColor").get<std::string>())) : QColor();
                        QColor fg = item.contains("fgColor") ? QColor(QString::fromStdString(item.at("fgColor").get<std::string>())) : QColor();
                        vcBridge->setWidgetColors(id, bg, fg);
                    }
                    if (id >= 0)
                    {
                        VCBridge::CueListConfig clCfg;
                        bool hasClCfg = false;
                        if (item.contains("nextPrevBehavior")) { clCfg.nextPrevBehavior = QString::fromStdString(item["nextPrevBehavior"].get<std::string>()); hasClCfg = true; }
                        if (item.contains("playbackLayout")) { clCfg.playbackLayout = QString::fromStdString(item["playbackLayout"].get<std::string>()); hasClCfg = true; }
                        if (item.contains("sideFaderMode")) { clCfg.sideFaderMode = QString::fromStdString(item["sideFaderMode"].get<std::string>()); hasClCfg = true; }
                        if (hasClCfg) vcBridge->configureCueList(id, clCfg);
                    }
                    break;
                }

                case VCType::Label:
                {
                    int parentID = item.at("parentID").get<int>();
                    QString text = QString::fromStdString(item.value("caption", item.value("text", "")));
                    if (!text.isEmpty())
                    {
                        int existingId = vcBridge->findWidgetByCaption(parentID, "Label", text);
                        if (existingId >= 0)
                        {
                            if (item.contains("childPageIndex") &&
                                !vcBridge->setWidgetPage(existingId, item["childPageIndex"].get<int>()))
                            {
                                results.push_back({{"widgetID", existingId}, {"error", "childPageIndex is outside the parent frame page range"}});
                                continue;
                            }
                            results.push_back({{"widgetID", existingId}, {"status", "existing"}});
                            continue;
                        }
                    }

                    int w = item.value("width", 200);
                    int h = item.value("height", 30);
                    QRect geo;
                    if (item.contains("x") && item.contains("y"))
                        geo = QRect(item.at("x").get<int>(), item.at("y").get<int>(), w, h);
                    else
                        geo = vcBridge->nextWidgetPosition(parentID, w, h);
                    int id = vcBridge->addLabel(parentID, geo, text);
                    results.push_back({{"widgetID", id}, {"status", "created"}});
                    if (id >= 0 && (item.contains("bgColor") || item.contains("fgColor")))
                    {
                        QColor bg = item.contains("bgColor") ? QColor(QString::fromStdString(item.at("bgColor").get<std::string>())) : QColor();
                        QColor fg = item.contains("fgColor") ? QColor(QString::fromStdString(item.at("fgColor").get<std::string>())) : QColor();
                        vcBridge->setWidgetColors(id, bg, fg);
                    }
                    break;
                }

                case VCType::SpeedDial:
                {
                    int parentID = item.at("parentID").get<int>();
                    if (!item.contains("functionIDs") && !item.contains("functions"))
                    {
                        results.push_back({{"error", "functionIDs or functions is required for speedDial type"}});
                        continue;
                    }
                    int w = item.value("width", 200);
                    int h = item.value("height", 200);
                    QRect geo;
                    if (item.contains("x") && item.contains("y"))
                        geo = QRect(item.at("x").get<int>(), item.at("y").get<int>(), w, h);
                    else
                        geo = vcBridge->nextWidgetPosition(parentID, w, h);

                    QList<quint32> funcIDs;
                    if (item.contains("functions"))
                    {
                        for (auto &f : item["functions"])
                            funcIDs.append(f.at("functionID").get<int>());
                    }
                    else
                    {
                        for (auto &fid : item.at("functionIDs"))
                            funcIDs.append(fid.get<int>());
                    }

                    int id = vcBridge->addSpeedDial(parentID, geo, funcIDs);
                    results.push_back({{"widgetID", id}, {"status", "created"}});
                    if (id >= 0)
                    {
                        if (item.contains("bgColor") || item.contains("fgColor"))
                        {
                            QColor bg = item.contains("bgColor") ? QColor(QString::fromStdString(item.at("bgColor").get<std::string>())) : QColor();
                            QColor fg = item.contains("fgColor") ? QColor(QString::fromStdString(item.at("fgColor").get<std::string>())) : QColor();
                            vcBridge->setWidgetColors(id, bg, fg);
                        }

                        // Apply SpeedDialConfig if functions with multipliers or other config fields present
                        VCBridge::SpeedDialConfig sdCfg;
                        bool hasSdCfg = false;
                        if (item.contains("functions"))
                        {
                            QList<VCBridge::SpeedDialFunctionInfo> funcs;
                            for (auto &f : item["functions"])
                            {
                                VCBridge::SpeedDialFunctionInfo fi;
                                fi.functionID = f.at("functionID").get<int>();
                                if (f.contains("fadeInMultiplier")) fi.fadeInMultiplier = QString::fromStdString(f["fadeInMultiplier"].get<std::string>());
                                if (f.contains("fadeOutMultiplier")) fi.fadeOutMultiplier = QString::fromStdString(f["fadeOutMultiplier"].get<std::string>());
                                if (f.contains("durationMultiplier")) fi.durationMultiplier = QString::fromStdString(f["durationMultiplier"].get<std::string>());
                                funcs.append(fi);
                            }
                            sdCfg.functions = funcs;
                            hasSdCfg = true;
                        }
                        if (item.contains("absoluteValueMin")) { sdCfg.absoluteValueMin = item["absoluteValueMin"].get<int>(); hasSdCfg = true; }
                        if (item.contains("absoluteValueMax")) { sdCfg.absoluteValueMax = item["absoluteValueMax"].get<int>(); hasSdCfg = true; }
                        if (item.contains("visibilityMask")) { sdCfg.visibilityMask = item["visibilityMask"].get<int>(); hasSdCfg = true; }
                        if (item.contains("resetFactorOnDialChange")) { sdCfg.resetFactorOnDialChange = item["resetFactorOnDialChange"].get<bool>(); hasSdCfg = true; }
                        if (hasSdCfg) vcBridge->configureSpeedDial(id, sdCfg);
                    }
                    break;
                }

                case VCType::AudioTriggers:
                {
                    int parentID = item.at("parentID").get<int>();
                    int w = item.value("width", 300);
                    int h = item.value("height", 150);
                    QRect geo;
                    if (item.contains("x") && item.contains("y"))
                        geo = QRect(item.at("x").get<int>(), item.at("y").get<int>(), w, h);
                    else
                        geo = vcBridge->nextWidgetPosition(parentID, w, h);
                    int id = vcBridge->addAudioTriggers(parentID, geo);
                    results.push_back({{"widgetID", id}, {"status", "created"}});
                    if (id >= 0)
                    {
                        if (item.contains("bgColor") || item.contains("fgColor"))
                        {
                            QColor bg = item.contains("bgColor") ? QColor(QString::fromStdString(item.at("bgColor").get<std::string>())) : QColor();
                            QColor fg = item.contains("fgColor") ? QColor(QString::fromStdString(item.at("fgColor").get<std::string>())) : QColor();
                            vcBridge->setWidgetColors(id, bg, fg);
                        }
                        if (item.contains("barsNumber"))
                            vcBridge->setAudioTriggerBarsNumber(id, item.at("barsNumber").get<int>());
                        if (item.contains("volumeLevel"))
                            vcBridge->setAudioTriggerVolume(id, item.at("volumeLevel").get<int>());
                    }
                    break;
                }

                case VCType::Clock:
                {
                    int parentID = item.at("parentID").get<int>();
                    int w = item.value("width", 200);
                    int h = item.value("height", 60);
                    QRect geo;
                    if (item.contains("x") && item.contains("y"))
                        geo = QRect(item.at("x").get<int>(), item.at("y").get<int>(), w, h);
                    else
                        geo = vcBridge->nextWidgetPosition(parentID, w, h);
                    int id = vcBridge->addClock(
                        parentID, geo,
                        QString::fromStdString(item.value("clockType", "clock")));
                    results.push_back({{"widgetID", id}, {"status", "created"}});
                    if (id >= 0 && (item.contains("bgColor") || item.contains("fgColor")))
                    {
                        QColor bg = item.contains("bgColor") ? QColor(QString::fromStdString(item.at("bgColor").get<std::string>())) : QColor();
                        QColor fg = item.contains("fgColor") ? QColor(QString::fromStdString(item.at("fgColor").get<std::string>())) : QColor();
                        vcBridge->setWidgetColors(id, bg, fg);
                    }
                    if (id >= 0)
                    {
                        VCBridge::ClockConfig clockCfg;
                        bool hasClockCfg = false;
                        if (item.contains("countdownHours")) { clockCfg.countdownH = item["countdownHours"].get<int>(); hasClockCfg = true; }
                        if (item.contains("countdownMinutes")) { clockCfg.countdownM = item["countdownMinutes"].get<int>(); hasClockCfg = true; }
                        if (item.contains("countdownSeconds")) { clockCfg.countdownS = item["countdownSeconds"].get<int>(); hasClockCfg = true; }
                        if (item.contains("schedules"))
                        {
                            QList<VCBridge::ClockScheduleInfo> scheds;
                            for (auto &s : item["schedules"])
                            {
                                VCBridge::ClockScheduleInfo si;
                                if (s.contains("functionID"))
                                    si.functionID = s["functionID"].get<int>();
                                else if (s.contains("functionName"))
                                {
                                    quint32 fid = mcp::resolveFunctionByName(doc, QString::fromStdString(s["functionName"].get<std::string>()));
                                    if (fid != Function::invalidId()) si.functionID = fid;
                                }
                                if (s.contains("hour")) si.hour = s["hour"].get<int>();
                                if (s.contains("minute")) si.minute = s["minute"].get<int>();
                                if (s.contains("second")) si.second = s["second"].get<int>();
                                scheds.append(si);
                            }
                            clockCfg.schedules = scheds;
                            hasClockCfg = true;
                        }
                        if (hasClockCfg) vcBridge->configureClock(id, clockCfg);
                    }
                    break;
                }

                case VCType::Animation:
                {
                    int parentID = item.at("parentID").get<int>();
                    QString caption = QString::fromStdString(item.value("caption", ""));

                    int funcID = -1;
                    if (item.contains("functionName"))
                    {
                        quint32 fid = mcp::resolveFunctionByName(doc, QString::fromStdString(item.at("functionName").get<std::string>()));
                        if (fid != Function::invalidId()) funcID = (int)fid;
                    }
                    if (item.contains("functionID"))
                        funcID = item.at("functionID").get<int>();

                    int w = item.value("width", 200);
                    int h = item.value("height", 200);
                    QRect geo;
                    if (item.contains("x") && item.contains("y"))
                        geo = QRect(item.at("x").get<int>(), item.at("y").get<int>(), w, h);
                    else
                        geo = vcBridge->nextWidgetPosition(parentID, w, h);

                    int id = vcBridge->addMatrix(parentID, geo,
                        funcID >= 0 ? (quint32)funcID : Function::invalidId(), caption);
                    results.push_back({{"widgetID", id}, {"status", id >= 0 ? "created" : "failed"}});
                    if (id >= 0)
                    {
                        if (item.contains("bgColor") || item.contains("fgColor"))
                        {
                            QColor bg = item.contains("bgColor") ? QColor(QString::fromStdString(item.at("bgColor").get<std::string>())) : QColor();
                            QColor fg = item.contains("fgColor") ? QColor(QString::fromStdString(item.at("fgColor").get<std::string>())) : QColor();
                            vcBridge->setWidgetColors(id, bg, fg);
                        }
                        VCBridge::MatrixConfig matCfg;
                        bool hasMatCfg = false;
                        if (item.contains("instantApply")) { matCfg.instantApply = item["instantApply"].get<bool>(); hasMatCfg = true; }
                        if (item.contains("visibilityMask")) { matCfg.visibilityMask = item["visibilityMask"].get<int>(); hasMatCfg = true; }
                        if (item.contains("colors"))
                        {
                            QVector<QColor> cv;
                            for (auto &c : item["colors"])
                                cv.append(QColor(QString::fromStdString(c.get<std::string>())));
                            matCfg.colors = cv;
                            hasMatCfg = true;
                        }
                        else
                        {
                            if (item.contains("color1")) { matCfg.color1 = QColor(QString::fromStdString(item["color1"].get<std::string>())); hasMatCfg = true; }
                            if (item.contains("color2")) { matCfg.color2 = QColor(QString::fromStdString(item["color2"].get<std::string>())); hasMatCfg = true; }
                            if (item.contains("color3")) { matCfg.color3 = QColor(QString::fromStdString(item["color3"].get<std::string>())); hasMatCfg = true; }
                            if (item.contains("color4")) { matCfg.color4 = QColor(QString::fromStdString(item["color4"].get<std::string>())); hasMatCfg = true; }
                            if (item.contains("color5")) { matCfg.color5 = QColor(QString::fromStdString(item["color5"].get<std::string>())); hasMatCfg = true; }
                        }
                        if (item.contains("animation")) { matCfg.animation = QString::fromStdString(item["animation"].get<std::string>()); hasMatCfg = true; }
                        if (hasMatCfg) vcBridge->configureMatrix(id, matCfg);
                    }
                    break;
                }

                case VCType::RecordPanel:
                {
                    int parentID = item.at("parentID").get<int>();
                    QString caption = QString::fromStdString(item.value("caption", ""));
                    if (!caption.isEmpty())
                    {
                        int existingId = vcBridge->findWidgetByCaption(parentID, "Record Panel", caption);
                        if (existingId >= 0)
                        {
                            if (item.contains("childPageIndex") &&
                                !vcBridge->setWidgetPage(existingId, item["childPageIndex"].get<int>()))
                            {
                                results.push_back({{"widgetID", existingId}, {"error", "childPageIndex is outside the parent frame page range"}});
                                continue;
                            }
                            results.push_back({{"widgetID", existingId}, {"status", "existing"}});
                            continue;
                        }
                    }

                    int w = item.value("width", 200);
                    int h = item.value("height", 120);
                    QRect geo;
                    if (item.contains("x") && item.contains("y"))
                        geo = QRect(item.at("x").get<int>(), item.at("y").get<int>(), w, h);
                    else
                        geo = vcBridge->nextWidgetPosition(parentID, w, h);
                    int id = vcBridge->addRecordPanel(parentID, geo);
                    results.push_back({{"widgetID", id}, {"status", id >= 0 ? "created" : "failed"}});
                    if (id >= 0)
                    {
                        if (item.contains("caption"))
                            vcBridge->setWidgetCaption(id, QString::fromStdString(item.at("caption").get<std::string>()));
                        if (item.contains("bgColor") || item.contains("fgColor"))
                        {
                            QColor bg = item.contains("bgColor") ? QColor(QString::fromStdString(item.at("bgColor").get<std::string>())) : QColor();
                            QColor fg = item.contains("fgColor") ? QColor(QString::fromStdString(item.at("fgColor").get<std::string>())) : QColor();
                            vcBridge->setWidgetColors(id, bg, fg);
                        }
                        VCBridge::RecordPanelConfig rpCfg;
                        bool hasRpCfg = false;
                        if (item.contains("targetFolder")) { rpCfg.targetFolder = QString::fromStdString(item["targetFolder"].get<std::string>()); hasRpCfg = true; }
                        if (item.contains("scenePrefix")) { rpCfg.scenePrefix = QString::fromStdString(item["scenePrefix"].get<std::string>()); hasRpCfg = true; }
                        if (item.contains("chaserPrefix")) { rpCfg.chaserPrefix = QString::fromStdString(item["chaserPrefix"].get<std::string>()); hasRpCfg = true; }
                        if (item.contains("defaultFadeIn")) { rpCfg.defaultFadeIn = item["defaultFadeIn"].get<int>(); hasRpCfg = true; }
                        if (item.contains("defaultHold")) { rpCfg.defaultHold = item["defaultHold"].get<int>(); hasRpCfg = true; }
                        if (item.contains("defaultFadeOut")) { rpCfg.defaultFadeOut = item["defaultFadeOut"].get<int>(); hasRpCfg = true; }
                        if (hasRpCfg) vcBridge->configureRecordPanel(id, rpCfg);
                    }
                    break;
                }

                default:
                    results.push_back({{"error", "unsupported widget type: " + typeStr}});
                    break;
                }

                if (item.contains("childPageIndex") && !results.empty() &&
                    results.back().contains("widgetID") && !results.back().contains("error"))
                {
                    int widgetID = results.back()["widgetID"].get<int>();
                    if (!vcBridge->setWidgetPage(widgetID, item["childPageIndex"].get<int>()))
                        results.back() = {{"widgetID", widgetID}, {"error", "childPageIndex is outside the parent frame page range"}};
                }
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("Create Virtual Console widgets. Use 'type' to specify widget kind: "
                     "frame, soloframe, button, slider, xypad, cuelist, label, speedDial, audioTrigger, matrix, clock, recordPanel. "
                     "Upserts: existing widget with same caption is returned. Batch. "
                     "Wrap multiple operations in {\"items\": [...]}. Each item is processed independently."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotIdempotent));
}
