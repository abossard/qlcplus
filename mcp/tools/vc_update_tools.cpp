/*
  Q Light Controller Plus
  vc_update_tools.cpp

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

void registerVCUpdateTools(fastmcpp::tools::ToolManager &tm, Doc *doc, VCBridge *vcBridge)
{
    using Json = nlohmann::json;
    using Tool = fastmcpp::tools::Tool;

    if (!vcBridge) return;

    // vc_update_widgets (batch — sparse update)
    // Absorbs former: update_widgets, set_widget_colors, configure_audio_triggers
    tm.register_tool(Tool(
        "vc_update_widgets",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"widgetID", {{"type", "integer"}}},
                {"caption", {{"type", "string"}, {"description", "New widget caption/label"}}},
                {"x", {{"type", "integer"}, {"description", "New X position"}}},
                {"y", {{"type", "integer"}, {"description", "New Y position"}}},
                {"width", {{"type", "integer"}, {"description", "New width"}}},
                {"height", {{"type", "integer"}, {"description", "New height"}}},
                {"functionID", {{"type", "integer"}, {"description", "New function ID (buttons: controlled function, sliders: playback function)"}}},
                {"functionName", {{"type", "string"}, {"description", "Function name. Alternative to functionID."}}},
                {"action", {{"type", "string"}, {"enum", {"toggle", "flash", "blackout", "stopall"}},
                    {"description", "Button action type"}}},
                {"iconPath", {{"type", "string"}, {"description", "Button: icon file path"}}},
                {"keySequence", {{"type", "string"}, {"description", "Button: keyboard shortcut (e.g. 'Ctrl+A')"}}},
                {"startupIntensityEnabled", {{"type", "boolean"}, {"description", "Button: enable startup intensity"}}},
                {"startupIntensity", {{"type", "number"}, {"description", "Button: startup intensity 0.0-1.0"}}},
                {"flashOverride", {{"type", "boolean"}, {"description", "Button: flash overrides other values"}}},
                {"flashForceLTP", {{"type", "boolean"}, {"description", "Button: flash forces LTP"}}},
                {"stopAllFadeTime", {{"type", "integer"}, {"description", "Button: fade time in ms for stopall action"}}},
                {"mode", {{"type", "string"}, {"enum", {"level", "playback", "submaster"}},
                    {"description", "Slider mode"}}},
                {"widgetStyle", {{"type", "string"}, {"enum", {"slider", "knob"}},
                    {"description", "Slider: visual style"}}},
                {"catchValues", {{"type", "boolean"}, {"description", "Slider: enable value catching"}}},
                {"channels", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                    {"fixtureID", {{"type", "integer"}}},
                    {"channel", {{"type", "integer"}}}
                }}}}, {"description", "Slider level-mode channels (replaces existing)"}}},
                {"clickAndGoType", {{"type", "string"}, {"enum", {"none", "colors", "preset"}}}},
                {"valueDisplayStyle", {{"type", "string"}, {"enum", {"dmx", "percentage"}}}},
                {"invertedAppearance", {{"type", "boolean"}}},
                {"rangeLowLimit", {{"type", "number"}}},
                {"rangeHighLimit", {{"type", "number"}}},
                {"monitorEnabled", {{"type", "boolean"}}},
                {"gmValueMode", {{"type", "string"}, {"enum", {"limit", "reduce"}}}},
                {"gmChannelMode", {{"type", "string"}, {"enum", {"intensity", "allchannels"}}}},
                {"bgColor", {{"type", "string"}, {"description", "Background color hex"}}},
                {"fgColor", {{"type", "string"}, {"description", "Foreground color hex"}}},
                {"font", {{"type", "object"}, {"properties", {
                    {"family", {{"type", "string"}}},
                    {"size", {{"type", "integer"}}},
                    {"bold", {{"type", "boolean"}}},
                    {"italic", {{"type", "boolean"}}}
                }}, {"description", "Widget font settings"}}},
                {"backgroundImage", {{"type", "string"}, {"description", "Background image file path"}}},
                {"disabled", {{"type", "boolean"}, {"description", "Disable/enable widget"}}},
                {"displayMode", {{"type", "string"}, {"enum", {"degrees", "percentage", "dmx"}},
                    {"description", "XY Pad display mode"}}},
                {"xyPadPosition", {{"type", "object"}, {"properties", {
                    {"x", {{"type", "number"}, {"description", "X position (0.0-1.0)"}}},
                    {"y", {{"type", "number"}, {"description", "Y position (0.0-1.0)"}}}
                }}, {"description", "Set XY Pad cursor position"}}},
                {"presets", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                    {"name", {{"type", "string"}}},
                    {"type", {{"type", "string"}, {"enum", {"position", "efx", "scene", "fixtureGroup"}}}},
                    {"x", {{"type", "number"}}}, {"y", {{"type", "number"}}},
                    {"functionID", {{"type", "integer"}}}
                }}}}, {"description", "XY Pad: position/EFX/scene presets"}}},
                {"multipageMode", {{"type", "boolean"}, {"description", "Frame: enable multipage mode"}}},
                {"totalPages", {{"type", "integer"}, {"description", "Frame: total number of pages"}}},
                {"currentPage", {{"type", "integer"}, {"description", "Frame: current page index"}}},
                {"pagesLoop", {{"type", "boolean"}, {"description", "Frame: loop pages"}}},
                {"headerVisible", {{"type", "boolean"}, {"description", "Frame: show header"}}},
                {"enableButtonVisible", {{"type", "boolean"}, {"description", "Frame: show enable button"}}},
                {"collapsed", {{"type", "boolean"}, {"description", "Frame: collapsed state"}}},
                {"soloframeMixing", {{"type", "boolean"}, {"description", "SoloFrame: allow mixing"}}},
                {"excludeMonitoredFunctions", {{"type", "boolean"}, {"description", "SoloFrame: exclude monitored functions"}}},
                {"chaserID", {{"type", "integer"}, {"description", "CueList: chaser function ID"}}},
                {"chaserName", {{"type", "string"}, {"description", "CueList: chaser name"}}},
                {"nextPrevBehavior", {{"type", "string"}, {"enum", {"defaultRunFirst", "runNext", "select", "nothing"}}}},
                {"playbackLayout", {{"type", "string"}, {"enum", {"playPauseStop", "playStopPause"}}}},
                {"sideFaderMode", {{"type", "string"}, {"enum", {"none", "crossfade", "steps"}}}},
                {"color1", {{"type", "string"}, {"description", "Matrix: color 1 hex"}}},
                {"color2", {{"type", "string"}, {"description", "Matrix: color 2 hex"}}},
                {"color3", {{"type", "string"}, {"description", "Matrix: color 3 hex"}}},
                {"color4", {{"type", "string"}, {"description", "Matrix: color 4 hex"}}},
                {"color5", {{"type", "string"}, {"description", "Matrix: color 5 hex"}}},
                {"animation", {{"type", "string"}, {"description", "Matrix: animation algorithm name"}}},
                {"instantApply", {{"type", "boolean"}, {"description", "Matrix: instant apply changes"}}},
                {"visibilityMask", {{"type", "integer"}, {"description", "Matrix/SpeedDial: visibility bitmask"}}},
                {"clockType", {{"type", "string"}, {"enum", {"clock", "stopwatch", "countdown"}}}},
                {"countdownHours", {{"type", "integer"}}},
                {"countdownMinutes", {{"type", "integer"}}},
                {"countdownSeconds", {{"type", "integer"}}},
                {"schedules", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                    {"functionID", {{"type", "integer"}}},
                    {"functionName", {{"type", "string"}}},
                    {"hour", {{"type", "integer"}}},
                    {"minute", {{"type", "integer"}}},
                    {"second", {{"type", "integer"}}}
                }}}}, {"description", "Clock: scheduled function triggers"}}},
                {"functions", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                    {"functionID", {{"type", "integer"}}},
                    {"fadeInMultiplier", {{"type", "string"}}},
                    {"fadeOutMultiplier", {{"type", "string"}}},
                    {"durationMultiplier", {{"type", "string"}}}
                }}, {"required", {"functionID"}}}}, {"description", "SpeedDial: functions with multipliers"}}},
                {"absoluteValueMin", {{"type", "integer"}, {"description", "SpeedDial: absolute value range min (ms)"}}},
                {"absoluteValueMax", {{"type", "integer"}, {"description", "SpeedDial: absolute value range max (ms)"}}},
                {"resetFactorOnDialChange", {{"type", "boolean"}, {"description", "SpeedDial: reset factor on dial change"}}},
                {"captureEnabled", {{"type", "boolean"},
                    {"description", "Audio Triggers: enable/disable audio capture"}}},
                {"volumeLevel", {{"type", "integer"},
                    {"description", "Audio Triggers: input volume 0-255"}}},
                {"barsNumber", {{"type", "integer"},
                    {"description", "Audio Triggers: total number of bars (min 1)"}}},
                {"bars", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                    {"barIndex", {{"type", "integer"}, {"description", "Bar index (0=volume bar, 1+=spectrum bars)"}}},
                    {"type", {{"type", "string"}, {"description", "Bar type: none, dmx, function, widget"}}},
                    {"minThreshold", {{"type", "integer"}, {"description", "Min trigger threshold 0-100"}}},
                    {"maxThreshold", {{"type", "integer"}, {"description", "Max trigger threshold 0-100"}}},
                    {"divisor", {{"type", "integer"}, {"description", "Beat divisor for trigger skipping"}}},
                    {"functionID", {{"type", "integer"}, {"description", "Function ID (for type=function)"}}},
                    {"targetWidgetID", {{"type", "integer"}, {"description", "Widget ID to control (for type=widget)"}}},
                    {"dmxChannels", {{"type", "array"}, {"description", "DMX channels (for type=dmx)"},
                        {"items", {{"type", "object"}, {"properties", {
                            {"fixtureID", {{"type", "integer"}}},
                            {"channel", {{"type", "integer"}}}
                        }}}}}}
                }}, {"required", {"barIndex", "type"}}}},
                    {"description", "Audio Triggers: per-bar configurations"}}}
            }}, {"required", {"widgetID"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc, vcBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            for (auto &item : args.at("items"))
            {
                int wid = item.at("widgetID").get<int>();

                // 1. Look up widget type
                auto details = vcBridge->getWidgetDetails(wid);
                if (details.id < 0)
                {
                    results.push_back({{"widgetID", wid}, {"error", "widget not found"}});
                    continue;
                }

                int widgetType = VCType::fromDisplayString(details.type);

                // 2. Validate fields for this widget type
                auto validationErr = VCValidate::validate(item, widgetType, false);
                if (!validationErr.empty()) { results.push_back(nlohmann::json::parse(validationErr)); continue; }

                // 3. Apply changes only after validation passes
                Json changes = Json::array();

                if (item.contains("caption"))
                {
                    bool ok = vcBridge->setWidgetCaption(wid,
                        QString::fromStdString(item.at("caption").get<std::string>()));
                    changes.push_back({{"property", "caption"}, {"status", ok ? "ok" : "failed"}});
                }

                // Geometry: update only specified fields, preserve others
                if (item.contains("x") || item.contains("y") ||
                    item.contains("width") || item.contains("height"))
                {
                    QRect geo = details.geometry;
                    if (item.contains("x")) geo.setX(item.at("x").get<int>());
                    if (item.contains("y")) geo.setY(item.at("y").get<int>());
                    if (item.contains("width")) geo.setWidth(item.at("width").get<int>());
                    if (item.contains("height")) geo.setHeight(item.at("height").get<int>());
                    vcBridge->setWidgetGeometry(wid, geo);
                    changes.push_back({{"property", "geometry"}, {"status", "ok"}});
                }

                if (item.contains("functionID"))
                {
                    int fid = item.at("functionID").get<int>();
                    bool ok = vcBridge->setButtonFunction(wid, fid);
                    if (!ok) ok = vcBridge->setSliderFunction(wid, fid);
                    changes.push_back({{"property", "functionID"}, {"status", ok ? "ok" : "failed"}});
                }

                if (item.contains("action"))
                {
                    bool ok = vcBridge->setButtonAction(wid,
                        QString::fromStdString(item.at("action").get<std::string>()));
                    changes.push_back({{"property", "action"}, {"status", ok ? "ok" : "failed"}});
                }

                if (item.contains("mode"))
                {
                    bool ok = vcBridge->setSliderMode(wid,
                        QString::fromStdString(item.at("mode").get<std::string>()));
                    changes.push_back({{"property", "mode"}, {"status", ok ? "ok" : "failed"}});
                }

                if (item.contains("channels"))
                {
                    QList<QPair<quint32, quint32>> channels;
                    for (auto &ch : item.at("channels"))
                    {
                        auto chErr = validateFields(ch, {"fixtureID", "channel"});
                        if (!chErr.empty()) { results.push_back(nlohmann::json::parse(chErr)); continue; }
                        channels.append(qMakePair(
                            (quint32)ch.at("fixtureID").get<int>(),
                            (quint32)ch.at("channel").get<int>()));
                    }
                    bool ok = vcBridge->setSliderChannels(wid, channels);
                    changes.push_back({{"property", "channels"}, {"status", ok ? "ok" : "failed"}});
                }

                if (item.contains("bgColor") || item.contains("fgColor"))
                {
                    QColor bg = item.contains("bgColor")
                        ? QColor(QString::fromStdString(item.at("bgColor").get<std::string>()))
                        : QColor();
                    QColor fg = item.contains("fgColor")
                        ? QColor(QString::fromStdString(item.at("fgColor").get<std::string>()))
                        : QColor();
                    vcBridge->setWidgetColors(wid, bg, fg);
                    changes.push_back({{"property", "colors"}, {"status", "ok"}});
                }

                // XY Pad specific updates
                if (item.contains("displayMode"))
                {
                    bool ok = vcBridge->setXYPadDisplayMode(wid,
                        QString::fromStdString(item.at("displayMode").get<std::string>()));
                    changes.push_back({{"property", "displayMode"}, {"status", ok ? "ok" : "failed"}});
                }

                if (item.contains("invertedAppearance"))
                {
                    bool ok = vcBridge->setXYPadInvertedAppearance(wid,
                        item.at("invertedAppearance").get<bool>());
                    changes.push_back({{"property", "invertedAppearance"}, {"status", ok ? "ok" : "failed"}});
                }

                if (item.contains("xyPadPosition"))
                {
                    auto pos = item.at("xyPadPosition");
                    qreal x = pos.contains("x") ? pos.at("x").get<double>() : 0.0;
                    qreal y = pos.contains("y") ? pos.at("y").get<double>() : 0.0;
                    bool ok = vcBridge->setXYPadPosition(wid, x, y);
                    changes.push_back({{"property", "xyPadPosition"}, {"status", ok ? "ok" : "failed"}});
                }

                // Audio Triggers updates
                if (item.contains("captureEnabled"))
                {
                    bool ok = vcBridge->setAudioTriggerCapture(wid,
                        item.at("captureEnabled").get<bool>());
                    changes.push_back({{"property", "captureEnabled"}, {"status", ok ? "ok" : "failed"}});
                }

                if (item.contains("volumeLevel"))
                {
                    bool ok = vcBridge->setAudioTriggerVolume(wid,
                        item.at("volumeLevel").get<int>());
                    changes.push_back({{"property", "volumeLevel"}, {"status", ok ? "ok" : "failed"}});
                }

                if (item.contains("barsNumber"))
                {
                    bool ok = vcBridge->setAudioTriggerBarsNumber(wid,
                        item.at("barsNumber").get<int>());
                    changes.push_back({{"property", "barsNumber"}, {"status", ok ? "ok" : "failed"}});
                }

                // Audio Triggers per-bar configuration (absorbed from configure_audio_triggers)
                if (item.contains("bars"))
                {
                    Json barResults = Json::array();
                    for (auto &bar : item.at("bars"))
                    {
                        VCBridge::AudioBarConfig config;
                        config.barIndex = bar.at("barIndex").get<int>();
                        config.type = QString::fromStdString(bar.at("type").get<std::string>());
                        if (bar.contains("minThreshold"))
                            config.minThreshold = bar.at("minThreshold").get<int>();
                        if (bar.contains("maxThreshold"))
                            config.maxThreshold = bar.at("maxThreshold").get<int>();
                        if (bar.contains("divisor"))
                            config.divisor = bar.at("divisor").get<int>();
                        if (bar.contains("functionID"))
                            config.functionID = bar.at("functionID").get<int>();
                        if (bar.contains("targetWidgetID"))
                            config.widgetID = bar.at("targetWidgetID").get<int>();
                        if (bar.contains("dmxChannels"))
                        {
                            for (auto &ch : bar.at("dmxChannels"))
                                config.dmxChannels.append(qMakePair(
                                    (quint32)ch.at("fixtureID").get<int>(),
                                    (quint32)ch.at("channel").get<int>()));
                        }

                        bool ok = vcBridge->configureAudioTriggerBar(wid, config);
                        barResults.push_back({{"barIndex", config.barIndex}, {"status", ok ? "ok" : "failed"}});
                    }
                    changes.push_back({{"property", "bars"}, {"status", "ok"}, {"bars", barResults}});
                }

                // Common properties for all widget types: font, backgroundImage, disabled
                if (item.contains("font"))
                {
                    VCBridge::FontConfig fc;
                    auto &f = item["font"];
                    if (f.contains("family")) fc.family = QString::fromStdString(f["family"].get<std::string>());
                    if (f.contains("size")) fc.pointSize = f["size"].get<int>();
                    if (f.contains("bold")) fc.bold = f["bold"].get<bool>();
                    if (f.contains("italic")) fc.italic = f["italic"].get<bool>();
                    bool ok = vcBridge->setWidgetFont(wid, fc);
                    changes.push_back({{"property", "font"}, {"status", ok ? "ok" : "failed"}});
                }
                if (item.contains("backgroundImage"))
                {
                    bool ok = vcBridge->setWidgetBackgroundImage(wid, QString::fromStdString(item["backgroundImage"].get<std::string>()));
                    changes.push_back({{"property", "backgroundImage"}, {"status", ok ? "ok" : "failed"}});
                }
                if (item.contains("disabled"))
                {
                    bool ok = vcBridge->setWidgetDisableState(wid, item["disabled"].get<bool>());
                    changes.push_back({{"property", "disabled"}, {"status", ok ? "ok" : "failed"}});
                }

                // Type-specific update dispatching
                if (widgetType == VCType::Button)
                {
                    VCBridge::ButtonConfig btnCfg;
                    bool hasBtnCfg = false;
                    if (item.contains("functionID")) { btnCfg.functionID = item["functionID"].get<int>(); hasBtnCfg = true; }
                    if (item.contains("functionName"))
                    {
                        quint32 fid = mcp::resolveFunctionByName(doc, QString::fromStdString(item["functionName"].get<std::string>()));
                        if (fid != Function::invalidId()) { btnCfg.functionID = fid; hasBtnCfg = true; }
                    }
                    if (item.contains("action")) { btnCfg.action = QString::fromStdString(item["action"].get<std::string>()); hasBtnCfg = true; }
                    if (item.contains("iconPath")) { btnCfg.iconPath = QString::fromStdString(item["iconPath"].get<std::string>()); hasBtnCfg = true; }
                    if (item.contains("startupIntensityEnabled")) { btnCfg.startupIntensityEnabled = item["startupIntensityEnabled"].get<bool>(); hasBtnCfg = true; }
                    if (item.contains("startupIntensity")) { btnCfg.startupIntensity = item["startupIntensity"].get<double>(); hasBtnCfg = true; }
                    if (item.contains("flashOverride")) { btnCfg.flashOverride = item["flashOverride"].get<bool>(); hasBtnCfg = true; }
                    if (item.contains("flashForceLTP")) { btnCfg.flashForceLTP = item["flashForceLTP"].get<bool>(); hasBtnCfg = true; }
                    if (item.contains("stopAllFadeTime")) { btnCfg.stopAllFadeTime = item["stopAllFadeTime"].get<int>(); hasBtnCfg = true; }
                    if (hasBtnCfg)
                    {
                        bool ok = vcBridge->configureButton(wid, btnCfg);
                        changes.push_back({{"property", "buttonConfig"}, {"status", ok ? "ok" : "failed"}});
                    }
                    if (item.contains("keySequence"))
                    {
                        bool ok = vcBridge->setWidgetKeySequence(wid, "default",
                            QKeySequence(QString::fromStdString(item["keySequence"].get<std::string>())));
                        changes.push_back({{"property", "keySequence"}, {"status", ok ? "ok" : "failed"}});
                    }
                }

                if (widgetType == VCType::Slider)
                {
                    if (item.contains("widgetStyle"))
                    {
                        bool ok = vcBridge->setSliderWidgetStyle(wid, QString::fromStdString(item["widgetStyle"].get<std::string>()));
                        changes.push_back({{"property", "widgetStyle"}, {"status", ok ? "ok" : "failed"}});
                    }
                    if (item.contains("catchValues"))
                    {
                        bool ok = vcBridge->setSliderCatchValues(wid, item["catchValues"].get<bool>());
                        changes.push_back({{"property", "catchValues"}, {"status", ok ? "ok" : "failed"}});
                    }
                }

                if (widgetType == VCType::Frame || widgetType == VCType::SoloFrame)
                {
                    VCBridge::FrameConfig frameCfg;
                    bool hasFrameCfg = false;
                    if (item.contains("multipageMode")) { frameCfg.multipageMode = item["multipageMode"].get<bool>(); hasFrameCfg = true; }
                    if (item.contains("totalPages")) { frameCfg.totalPages = item["totalPages"].get<int>(); hasFrameCfg = true; }
                    if (item.contains("currentPage")) { frameCfg.currentPage = item["currentPage"].get<int>(); hasFrameCfg = true; }
                    if (item.contains("pagesLoop")) { frameCfg.pagesLoop = item["pagesLoop"].get<bool>(); hasFrameCfg = true; }
                    if (item.contains("headerVisible")) { frameCfg.headerVisible = item["headerVisible"].get<bool>(); hasFrameCfg = true; }
                    if (item.contains("enableButtonVisible")) { frameCfg.enableButtonVisible = item["enableButtonVisible"].get<bool>(); hasFrameCfg = true; }
                    if (item.contains("collapsed")) { frameCfg.collapsed = item["collapsed"].get<bool>(); hasFrameCfg = true; }
                    if (item.contains("soloframeMixing")) { frameCfg.soloframeMixing = item["soloframeMixing"].get<bool>(); hasFrameCfg = true; }
                    if (item.contains("excludeMonitoredFunctions")) { frameCfg.excludeMonitoredFunctions = item["excludeMonitoredFunctions"].get<bool>(); hasFrameCfg = true; }
                    if (hasFrameCfg)
                    {
                        bool ok = vcBridge->configureFrame(wid, frameCfg);
                        changes.push_back({{"property", "frameConfig"}, {"status", ok ? "ok" : "failed"}});
                    }
                }

                if (widgetType == VCType::CueList)
                {
                    VCBridge::CueListConfig clCfg;
                    bool hasClCfg = false;
                    if (item.contains("chaserID")) { clCfg.chaserID = item["chaserID"].get<int>(); hasClCfg = true; }
                    if (item.contains("chaserName"))
                    {
                        quint32 fid = mcp::resolveFunctionByName(doc, QString::fromStdString(item["chaserName"].get<std::string>()), Function::ChaserType);
                        if (fid != Function::invalidId()) { clCfg.chaserID = fid; hasClCfg = true; }
                    }
                    if (item.contains("nextPrevBehavior")) { clCfg.nextPrevBehavior = QString::fromStdString(item["nextPrevBehavior"].get<std::string>()); hasClCfg = true; }
                    if (item.contains("playbackLayout")) { clCfg.playbackLayout = QString::fromStdString(item["playbackLayout"].get<std::string>()); hasClCfg = true; }
                    if (item.contains("sideFaderMode")) { clCfg.sideFaderMode = QString::fromStdString(item["sideFaderMode"].get<std::string>()); hasClCfg = true; }
                    if (hasClCfg)
                    {
                        bool ok = vcBridge->configureCueList(wid, clCfg);
                        changes.push_back({{"property", "cueListConfig"}, {"status", ok ? "ok" : "failed"}});
                    }
                }

                if (widgetType == VCType::Animation)
                {
                    VCBridge::MatrixConfig matCfg;
                    bool hasMatCfg = false;
                    if (item.contains("functionID")) { matCfg.functionID = item["functionID"].get<int>(); hasMatCfg = true; }
                    if (item.contains("functionName"))
                    {
                        quint32 fid = mcp::resolveFunctionByName(doc, QString::fromStdString(item["functionName"].get<std::string>()));
                        if (fid != Function::invalidId()) { matCfg.functionID = fid; hasMatCfg = true; }
                    }
                    if (item.contains("color1")) { matCfg.color1 = QColor(QString::fromStdString(item["color1"].get<std::string>())); hasMatCfg = true; }
                    if (item.contains("color2")) { matCfg.color2 = QColor(QString::fromStdString(item["color2"].get<std::string>())); hasMatCfg = true; }
                    if (item.contains("color3")) { matCfg.color3 = QColor(QString::fromStdString(item["color3"].get<std::string>())); hasMatCfg = true; }
                    if (item.contains("color4")) { matCfg.color4 = QColor(QString::fromStdString(item["color4"].get<std::string>())); hasMatCfg = true; }
                    if (item.contains("color5")) { matCfg.color5 = QColor(QString::fromStdString(item["color5"].get<std::string>())); hasMatCfg = true; }
                    if (item.contains("animation")) { matCfg.animation = QString::fromStdString(item["animation"].get<std::string>()); hasMatCfg = true; }
                    if (item.contains("instantApply")) { matCfg.instantApply = item["instantApply"].get<bool>(); hasMatCfg = true; }
                    if (item.contains("visibilityMask")) { matCfg.visibilityMask = item["visibilityMask"].get<int>(); hasMatCfg = true; }
                    if (hasMatCfg)
                    {
                        bool ok = vcBridge->configureMatrix(wid, matCfg);
                        changes.push_back({{"property", "matrixConfig"}, {"status", ok ? "ok" : "failed"}});
                    }
                }

                if (widgetType == VCType::Clock)
                {
                    VCBridge::ClockConfig clockCfg;
                    bool hasClockCfg = false;
                    if (item.contains("clockType")) { clockCfg.clockType = QString::fromStdString(item["clockType"].get<std::string>()); hasClockCfg = true; }
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
                    if (hasClockCfg)
                    {
                        bool ok = vcBridge->configureClock(wid, clockCfg);
                        changes.push_back({{"property", "clockConfig"}, {"status", ok ? "ok" : "failed"}});
                    }
                }

                if (widgetType == VCType::SpeedDial)
                {
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
                    if (item.contains("presets"))
                    {
                        QList<VCBridge::SpeedDialPresetInfo> presets;
                        for (auto &p : item["presets"])
                        {
                            VCBridge::SpeedDialPresetInfo pi;
                            if (p.contains("name")) pi.name = QString::fromStdString(p["name"].get<std::string>());
                            if (p.contains("value")) pi.value = p["value"].get<int>();
                            presets.append(pi);
                        }
                        sdCfg.presets = presets;
                        hasSdCfg = true;
                    }
                    if (item.contains("absoluteValueMin")) { sdCfg.absoluteValueMin = item["absoluteValueMin"].get<int>(); hasSdCfg = true; }
                    if (item.contains("absoluteValueMax")) { sdCfg.absoluteValueMax = item["absoluteValueMax"].get<int>(); hasSdCfg = true; }
                    if (item.contains("visibilityMask")) { sdCfg.visibilityMask = item["visibilityMask"].get<int>(); hasSdCfg = true; }
                    if (item.contains("resetFactorOnDialChange")) { sdCfg.resetFactorOnDialChange = item["resetFactorOnDialChange"].get<bool>(); hasSdCfg = true; }
                    if (hasSdCfg)
                    {
                        bool ok = vcBridge->configureSpeedDial(wid, sdCfg);
                        changes.push_back({{"property", "speedDialConfig"}, {"status", ok ? "ok" : "failed"}});
                    }
                }

                if (widgetType == VCType::XYPad)
                {
                    if (item.contains("presets"))
                    {
                        QList<VCBridge::XYPadPresetInfo> presets;
                        for (auto &p : item["presets"])
                        {
                            VCBridge::XYPadPresetInfo pi;
                            if (p.contains("name")) pi.name = QString::fromStdString(p["name"].get<std::string>());
                            if (p.contains("type")) pi.type = QString::fromStdString(p["type"].get<std::string>());
                            if (p.contains("x") && p.contains("y")) pi.position = QPointF(p["x"].get<double>(), p["y"].get<double>());
                            if (p.contains("functionID")) pi.functionID = p["functionID"].get<int>();
                            presets.append(pi);
                        }
                        bool ok = vcBridge->setXYPadPresets(wid, presets);
                        changes.push_back({{"property", "presets"}, {"status", ok ? "ok" : "failed"}});
                    }
                }

                results.push_back({{"widgetID", wid}, {"changes", changes}});
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("Update Virtual Console widget properties. Sparse: only provided fields are changed. "
                     "Validates fields against widget type. Supports type-specific configuration for buttons, "
                     "sliders, frames, cue lists, matrices, clocks, speed dials, XY pads, and audio triggers. Batch."),
        std::nullopt
    ));
}
