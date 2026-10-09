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
#include "fixture.h"
#include "chaser.h"
#include "rgbmatrix.h"
#include "scene.h"
#include "qlcchannel.h"

#include <limits>

#include <fastmcpp/tools/manager.hpp>
#include <fastmcpp/tools/tool.hpp>

namespace {

using Json = nlohmann::json;

enum class Kind { String, Bool, Int, Number, StringArray, Object, ObjectArray };
using KindTable = std::vector<std::pair<const char *, Kind>>;

bool hasKind(const Json &v, Kind kind)
{
    switch (kind)
    {
        case Kind::String: return v.is_string();
        case Kind::Bool: return v.is_boolean();
        case Kind::Int:
            return mcp::jsonInteger(v, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()).has_value();
        case Kind::Number: return v.is_number();
        case Kind::Object: return v.is_object();
        case Kind::ObjectArray:
            if (!v.is_array()) return false;
            for (const auto &e : v) if (!e.is_object()) return false;
            return true;
        case Kind::StringArray:
            if (!v.is_array()) return false;
            for (const auto &e : v) if (!e.is_string()) return false;
            return true;
    }
    return false;
}

const char *kindName(Kind kind)
{
    switch (kind)
    {
        case Kind::String: return "a string";
        case Kind::Bool: return "a boolean";
        case Kind::Int: return "a 32-bit integer";
        case Kind::Number: return "a number";
        case Kind::StringArray: return "an array of strings";
        case Kind::Object: return "an object";
        case Kind::ObjectArray: return "an array of objects";
    }
    return "valid";
}

std::string checkKinds(const Json &obj, const KindTable &table, const std::string &prefix,
                       std::initializer_list<const char *> required = {})
{
    for (const char *key : required)
        if (!obj.contains(key)) return prefix + key + " is required";
    for (const auto &[key, kind] : table)
        if (obj.contains(key) && !hasKind(obj.at(key), kind))
            return prefix + key + " must be " + kindName(kind);
    return "";
}

std::string checkEntries(const Json &item, const char *field, const KindTable &table,
                         std::initializer_list<const char *> required = {})
{
    if (!item.contains(field)) return "";
    for (const auto &entry : item.at(field))
    {
        for (auto it = entry.begin(); it != entry.end(); ++it)
        {
            bool known = false;
            for (const auto &allowed : table) known = known || it.key() == allowed.first;
            if (!known) return std::string(field) + "[]." + it.key() + " is not a known field";
        }
        auto err = checkKinds(entry, table, std::string(field) + "[].", required);
        if (!err.empty()) return err;
    }
    return "";
}

// Rejects every input the apply phase could choke on, so a valid item never mutates halfway.
std::string preflightUpdate(const Json &item, int widgetType, const VCBridge::WidgetDetails &details, Doc *doc,
                            const VCBridge &bridge)
{
    static const KindTable top = {
        {"caption", Kind::String}, {"x", Kind::Int}, {"y", Kind::Int}, {"width", Kind::Int}, {"height", Kind::Int},
        {"functionID", Kind::Int}, {"functionName", Kind::String}, {"action", Kind::String},
        {"iconPath", Kind::String}, {"keySequence", Kind::String}, {"startupIntensityEnabled", Kind::Bool},
        {"startupIntensity", Kind::Number}, {"flashOverride", Kind::Bool}, {"flashForceLTP", Kind::Bool},
        {"stopAllFadeTime", Kind::Int}, {"mode", Kind::String}, {"widgetStyle", Kind::String},
        {"catchValues", Kind::Bool}, {"channels", Kind::ObjectArray}, {"clickAndGoType", Kind::String},
        {"valueDisplayStyle", Kind::String}, {"invertedAppearance", Kind::Bool}, {"rangeLowLimit", Kind::Number},
        {"rangeHighLimit", Kind::Number}, {"monitorEnabled", Kind::Bool}, {"gmValueMode", Kind::String},
        {"gmChannelMode", Kind::String}, {"bgColor", Kind::String}, {"fgColor", Kind::String},
        {"font", Kind::Object}, {"backgroundImage", Kind::String}, {"disabled", Kind::Bool},
        {"displayMode", Kind::String}, {"presets", Kind::ObjectArray}, {"multipageMode", Kind::Bool},
        {"totalPages", Kind::Int}, {"pagesLoop", Kind::Bool}, {"pageLabels", Kind::StringArray},
        {"headerVisible", Kind::Bool}, {"enableButtonVisible", Kind::Bool}, {"collapsed", Kind::Bool},
        {"soloframeMixing", Kind::Bool}, {"excludeMonitoredFunctions", Kind::Bool}, {"chaserID", Kind::Int},
        {"chaserName", Kind::String}, {"nextPrevBehavior", Kind::String}, {"playbackLayout", Kind::String},
        {"sideFaderMode", Kind::String}, {"color1", Kind::String}, {"color2", Kind::String},
        {"color3", Kind::String}, {"color4", Kind::String}, {"color5", Kind::String},
        {"colors", Kind::StringArray}, {"animation", Kind::String}, {"instantApply", Kind::Bool},
        {"visibilityMask", Kind::Int}, {"clockType", Kind::String}, {"countdownHours", Kind::Int},
        {"countdownMinutes", Kind::Int}, {"countdownSeconds", Kind::Int}, {"schedules", Kind::ObjectArray},
        {"functions", Kind::ObjectArray}, {"absoluteValueMin", Kind::Int}, {"absoluteValueMax", Kind::Int},
        {"resetFactorOnDialChange", Kind::Bool}, {"barsNumber", Kind::Int}, {"bars", Kind::ObjectArray},
        {"targetFolder", Kind::String}, {"scenePrefix", Kind::String}, {"chaserPrefix", Kind::String},
        {"defaultFadeIn", Kind::Int}, {"defaultHold", Kind::Int}, {"defaultFadeOut", Kind::Int}};
    auto err = checkKinds(item, top, "");
    if (!err.empty()) return err;

    if (item.contains("font"))
    {
        err = checkKinds(item.at("font"), {{"family", Kind::String}, {"size", Kind::Int},
                                           {"bold", Kind::Bool}, {"italic", Kind::Bool}}, "font.");
        if (!err.empty()) return err;
    }

    if (widgetType == VCType::SpeedDial)
        err = checkEntries(item, "presets", {{"name", Kind::String}, {"value", Kind::Int}});
    else
        err = checkEntries(item, "presets", {{"name", Kind::String}, {"type", Kind::String}, {"x", Kind::Number},
                                             {"y", Kind::Number}, {"functionID", Kind::Int}});
    if (!err.empty()) return err;
    if (widgetType == VCType::XYPad && item.contains("presets"))
    {
        static const std::set<std::string> presetTypes = {"position", "efx", "scene", "fixtureGroup"};
        for (const auto &p : item.at("presets"))
            if (p.contains("type") && !presetTypes.count(p.at("type").get<std::string>()))
                return "presets[].type must be one of position, efx, scene, fixtureGroup";
    }

    err = checkEntries(item, "functions", {{"functionID", Kind::Int}, {"fadeInMultiplier", Kind::String},
                                           {"fadeOutMultiplier", Kind::String}, {"durationMultiplier", Kind::String}},
                       {"functionID"});
    if (!err.empty()) return err;

    err = checkEntries(item, "schedules", {{"functionID", Kind::Int}, {"functionName", Kind::String},
                                           {"hour", Kind::Int}, {"minute", Kind::Int}, {"second", Kind::Int}});
    if (!err.empty()) return err;

    err = checkEntries(item, "bars", {{"barIndex", Kind::Int}, {"type", Kind::String}, {"minThreshold", Kind::Int},
                                      {"maxThreshold", Kind::Int}, {"divisor", Kind::Int}, {"functionID", Kind::Int},
                                      {"targetWidgetID", Kind::Int}, {"dmxChannels", Kind::ObjectArray}},
                       {"barIndex", "type"});
    if (!err.empty()) return err;
    if (item.contains("bars"))
    {
        static const std::set<std::string> barTypes = {"none", "dmx", "function", "widget"};
        for (const auto &bar : item.at("bars"))
        {
            const int index = bar.at("barIndex").get<int>();
            if (index < 0 || index >= details.barsNumber)
                return "bars[].barIndex must be 0-" + std::to_string(details.barsNumber - 1);
            if (!barTypes.count(bar.at("type").get<std::string>()))
                return "bars[].type must be one of none, dmx, function, widget";
            err = checkEntries(bar, "dmxChannels", {{"fixtureID", Kind::Int}, {"channel", Kind::Int}},
                               {"fixtureID", "channel"});
            if (!err.empty()) return "bars[]." + err;
        }
    }

    return VCRefs::check(item, widgetType, doc, bridge);
}

} // namespace

std::string VCRefs::check(const Json &item, int widgetType, Doc *doc, const VCBridge &bridge)
{
    auto id = [](const Json &v) { return mcp::jsonInteger(v, -1, mcp::kMaxId); };
    auto anyFunction = [](Function *) { return true; };
    auto function = [&](const std::string &field, const Json &v, bool canUnbind,
                        const std::function<bool(Function *)> &fits, const char *what) -> std::string {
        const auto fid = id(v);
        if (canUnbind && fid == -1) return "";
        Function *f = fid && *fid >= 0 ? doc->function(quint32(*fid)) : nullptr;
        return f && fits(f) ? "" : field + " " + v.dump() + " is not an existing " + what;
    };
    auto named = [&](const std::string &field, const Json &name, Function::Type type,
                     const std::function<bool(Function *)> &fits, const char *what) -> std::string {
        const QString text = QString::fromStdString(name.get<std::string>());
        const quint32 fid = type == Function::Undefined ? mcp::resolveFunctionByName(doc, text)
                                                        : mcp::resolveFunctionByName(doc, text, type);
        if (fid == Function::invalidId()) return field + " '" + name.get<std::string>() + "' not found";
        return fits(doc->function(fid)) ? "" : field + " '" + name.get<std::string>() + "' is not " + what;
    };
    auto fixture = [&](const Json &v) -> Fixture * {
        const auto fx = id(v);
        return fx && *fx >= 0 ? doc->fixture(quint32(*fx)) : nullptr;
    };
    auto channel = [&](const std::string &field, const Json &ch) -> std::string {
        Fixture *fxi = fixture(ch.value("fixtureID", Json()));
        if (!fxi) return field + ".fixtureID " + ch.value("fixtureID", Json()).dump() + " is not an existing fixture";
        const auto c = id(ch.value("channel", Json()));
        if (!c || *c < 0 || quint32(*c) >= fxi->channels())
            return field + ".channel " + ch.value("channel", Json()).dump() + " is not a channel of fixture "
                   + std::to_string(fxi->id());
        return "";
    };
    auto isMatrix = [](Function *f) { return qobject_cast<RGBMatrix *>(f) != nullptr; };
    auto isChaser = [](Function *f) { return qobject_cast<Chaser *>(f) != nullptr; };
    const bool matrix = widgetType == VCType::Animation;
    std::string err;
    auto failed = [&](std::string e) { err = std::move(e); return !err.empty(); };

    if (item.contains("functionName")
        && failed(named("functionName", item.at("functionName"), Function::Undefined,
                        matrix ? std::function<bool(Function *)>(isMatrix) : anyFunction, "an RGB matrix")))
        return err;
    if (item.contains("functionID")
        && (widgetType == VCType::Button || widgetType == VCType::Slider || matrix)
        && failed(function("functionID", item.at("functionID"), true,
                           matrix ? std::function<bool(Function *)>(isMatrix) : anyFunction,
                           matrix ? "RGB matrix" : "function")))
        return err;
    if (widgetType == VCType::CueList)
    {
        if (item.contains("chaserName")
            && failed(named("chaserName", item.at("chaserName"), Function::ChaserType, isChaser, "a chaser")))
            return err;
        if (item.contains("chaserID") && failed(function("chaserID", item.at("chaserID"), true, isChaser, "chaser")))
            return err;
    }
    if (widgetType == VCType::Slider && item.contains("channels"))
        for (const auto &ch : item.at("channels"))
            if (failed(channel("channels[]", ch))) return err;
    if (widgetType == VCType::XYPad)
    {
        for (const auto &fx : item.value("fixtureIDs", Json::array()))
            if (!fixture(fx)) return "fixtureIDs[] " + fx.dump() + " is not an existing fixture";
        for (const auto &fx : item.value("fixtures", Json::array()))
        {
            Fixture *fxi = fixture(fx.value("fixtureID", Json()));
            if (!fxi) return "fixtures[].fixtureID " + fx.value("fixtureID", Json()).dump() + " is not an existing fixture";
            const auto head = id(fx.value("head", Json(0)));
            if (!head || *head < 0 || *head >= fxi->heads())
                return "fixtures[].head " + fx.value("head", Json(0)).dump() + " is not a head of fixture "
                       + std::to_string(fxi->id());
        }
        for (const auto &p : item.value("presets", Json::array()))
        {
            const std::string type = p.value("type", "");
            if (type != "efx" && type != "scene") continue;
            // Mirrors VCXYPad::sceneHasPanTilt: addFunctionPreset silently drops other scenes.
            auto panTiltScene = [doc](Function *f) {
                Scene *scene = qobject_cast<Scene *>(f);
                if (!scene) return false;
                for (const SceneValue &scv : scene->values())
                {
                    Fixture *fxi = doc->fixture(scv.fxi);
                    const QLCChannel *ch = fxi ? fxi->channel(scv.channel) : nullptr;
                    if (ch && (ch->group() == QLCChannel::Pan || ch->group() == QLCChannel::Tilt)) return true;
                }
                return false;
            };
            if (failed(type == "efx"
                           ? function("presets[].functionID", p.value("functionID", Json()), false,
                                      [](Function *f) { return f->type() == Function::EFXType; }, "EFX")
                           : function("presets[].functionID", p.value("functionID", Json()), false,
                                      panTiltScene, "scene with pan/tilt values")))
                return err;
        }
    }
    if (widgetType == VCType::SpeedDial)
    {
        for (const auto &fid : item.value("functionIDs", Json::array()))
            if (failed(function("functionIDs[]", fid, false, anyFunction, "function"))) return err;
        for (const auto &f : item.value("functions", Json::array()))
            if (failed(function("functions[].functionID", f.value("functionID", Json()), false, anyFunction, "function")))
                return err;
    }
    if (widgetType == VCType::Clock)
        for (const auto &s : item.value("schedules", Json::array()))
        {
            if (s.contains("functionID"))
            {
                if (failed(function("schedules[].functionID", s.at("functionID"), false, anyFunction, "function")))
                    return err;
            }
            else if (s.contains("functionName")
                     && failed(named("schedules[].functionName", s.at("functionName"), Function::Undefined,
                                     anyFunction, "")))
                return err;
        }
    if (widgetType == VCType::AudioTriggers)
        for (const auto &bar : item.value("bars", Json::array()))
        {
            const std::string type = bar.value("type", "");
            if (type == "function" && bar.contains("functionID")
                && failed(function("bars[].functionID", bar.at("functionID"), true, anyFunction, "function")))
                return err;
            if (type == "widget" && bar.contains("targetWidgetID"))
            {
                const auto target = id(bar.at("targetWidgetID"));
                if (target == -1) continue;
                const QString kind = target ? bridge.getWidgetDetails(int(*target)).machineType : QString();
                if (kind != "button" && kind != "slider" && kind != "speedDial")
                    return "bars[].targetWidgetID " + bar.at("targetWidgetID").dump()
                           + " is not an existing button, slider or speedDial";
            }
            if (type == "dmx")
                for (const auto &ch : bar.value("dmxChannels", Json::array()))
                    if (failed(channel("bars[].dmxChannels[]", ch))) return err;
        }
    return "";
}

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
                {"functionID", {{"type", "integer"}, {"description", "New function ID (buttons: controlled function, sliders: playback function, matrix: RGB matrix). -1 unbinds."}}},
                {"functionName", {{"type", "string"}, {"description", "Function name. Alternative to functionID."}}},
                {"action", {{"type", "string"}, {"enum", {"toggle", "flash", "blackout", "stopall", "freeze", "freezehold"}},
                    {"description", "Button action type. freeze toggles the workspace-global freeze latch; freezehold holds freeze while pressed."}}},
                {"iconPath", {{"type", "string"}, {"description", "Button: icon file path"}}},
                {"keySequence", {{"type", "string"}, {"description", "Button: keyboard shortcut (e.g. 'Ctrl+A')"}}},
                {"startupIntensityEnabled", {{"type", "boolean"}, {"description", "Button: enable startup intensity"}}},
                {"startupIntensity", {{"type", "number"}, {"description", "Button: startup intensity 0.0-1.0"}}},
                {"flashOverride", {{"type", "boolean"}, {"description", "Button: flash overrides other values"}}},
                {"flashForceLTP", {{"type", "boolean"}, {"description", "Button: flash forces LTP"}}},
                {"stopAllFadeTime", {{"type", "integer"}, {"description", "Button: fade time in ms for stopall action"}}},
                {"mode", {{"type", "string"}, {"enum", {"level", "playback", "submaster", "grandmaster"}},
                    {"description", "Slider mode"}}},
                {"widgetStyle", {{"type", "string"}, {"enum", {"slider", "knob"}},
                    {"description", "Slider: visual style"}}},
                {"catchValues", {{"type", "boolean"}, {"description", "Slider: enable value catching"}}},
                {"channels", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                    {"fixtureID", {{"type", "integer"}}},
                    {"channel", {{"type", "integer"}}}
                }}}}, {"description", "Slider level-mode channels (replaces existing)"}}},
                {"clickAndGoType", {{"type", "string"}, {"enum", {"none", "colors", "preset", "rgb", "cmy"}}}},
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
                {"presets", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                    {"name", {{"type", "string"}}},
                    {"type", {{"type", "string"}, {"enum", {"position", "efx", "scene", "fixtureGroup"}}}},
                    {"x", {{"type", "number"}}}, {"y", {{"type", "number"}}},
                    {"functionID", {{"type", "integer"}}},
                    {"value", {{"type", "integer"}, {"description", "SpeedDial: preset time value (ms)"}}}
                }}}}, {"description", "XY Pad: position/EFX/scene presets (name, type, x, y, functionID). SpeedDial: presets (name, value)"}}},
                {"multipageMode", {{"type", "boolean"}, {"description", "Frame: enable multipage mode"}}},
                {"totalPages", {{"type", "integer"}, {"description", "Frame: total number of pages"}}},
                {"pagesLoop", {{"type", "boolean"}, {"description", "Frame: loop pages"}}},
                {"pageLabels", {{"type", "array"}, {"items", {{"type", "string"}}}, {"description", "Frame: page names (one per page, ordered by index)"}}},
                {"headerVisible", {{"type", "boolean"}, {"description", "Frame: show header"}}},
                {"enableButtonVisible", {{"type", "boolean"}, {"description", "Frame: show enable button"}}},
                {"collapsed", {{"type", "boolean"}, {"description", "Frame: collapsed state"}}},
                {"soloframeMixing", {{"type", "boolean"}, {"description", "SoloFrame: allow mixing"}}},
                {"excludeMonitoredFunctions", {{"type", "boolean"}, {"description", "SoloFrame: exclude monitored functions"}}},
                {"chaserID", {{"type", "integer"}, {"description", "CueList: chaser function ID. -1 unbinds."}}},
                {"chaserName", {{"type", "string"}, {"description", "CueList: chaser name"}}},
                {"nextPrevBehavior", {{"type", "string"}, {"enum", {"defaultRunFirst", "runNext", "select", "nothing"}}}},
                {"playbackLayout", {{"type", "string"}, {"enum", {"playPauseStop", "playStopPause"}}}},
                {"sideFaderMode", {{"type", "string"}, {"enum", {"none", "crossfade", "steps"}}}},
                {"color1", {{"type", "string"}, {"description", "Matrix: color 1 hex"}}},
                {"color2", {{"type", "string"}, {"description", "Matrix: color 2 hex"}}},
                {"color3", {{"type", "string"}, {"description", "Matrix: color 3 hex"}}},
                {"color4", {{"type", "string"}, {"description", "Matrix: color 4 hex"}}},
                {"color5", {{"type", "string"}, {"description", "Matrix: color 5 hex"}}},
                {"colors", {{"type", "array"}, {"items", {{"type", "string"}}}, {"maxItems", 5}, {"description", "Matrix: colors array (overrides individual colorN fields)"}}},
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
                {"barsNumber", {{"type", "integer"},
                    {"description", "Audio Triggers: legacy compatibility field; QLC+ 5 uses fixed source mappings. Query widget bars for source identities."}}},
                {"bars", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                    {"barIndex", {{"type", "integer"}, {"description", "Stable source index: 0=Low bank trigger, 1=Mid bank trigger, 2=High bank trigger, 3=Volume, 4=Beat pulse, 5=Kick hit, 6=Kick power, 7=Bass, 8=Lows, 9=Mids, 10=High power. Independent mappings to one slider retain last-update-wins behavior."}}},
                    {"type", {{"type", "string"}, {"description", "Bar type: none, dmx, function, widget"}}},
                    {"minThreshold", {{"type", "integer"}, {"description", "Min trigger threshold 0-100"}}},
                    {"maxThreshold", {{"type", "integer"}, {"description", "Max trigger threshold 0-100"}}},
                    {"divisor", {{"type", "integer"}, {"description", "Beat divisor for trigger skipping"}}},
                    {"functionID", {{"type", "integer"}, {"description", "Function ID (for type=function). -1 unbinds."}}},
                    {"targetWidgetID", {{"type", "integer"}, {"description", "Widget ID to control (for type=widget): button, slider or speedDial. -1 unbinds."}}},
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
            auto itemsErr = validateItemsArray(args);
            if (itemsErr) return *itemsErr;
            Json results = Json::array();
            const Json &items = args.at("items");
            for (size_t i = 0; i < items.size(); ++i)
            {
                const Json &item = items[i];
                auto widgetIDValue = item.contains("widgetID")
                    ? mcp::jsonInteger(item.at("widgetID"), 0, std::numeric_limits<int>::max()) : std::nullopt;
                if (!widgetIDValue)
                {
                    results.push_back(mcp::itemError(i, mcp::integerError("widgetID", 0, std::numeric_limits<int>::max())));
                    continue;
                }
                const int wid = int(*widgetIDValue);
                auto itemError = [&](const std::string &msg) {
                    Json rec = mcp::itemError(i, msg);
                    rec["widgetID"] = wid;
                    results.push_back(rec);
                };

                // 1. Look up widget type
                auto details = vcBridge->getWidgetDetails(wid);
                if (details.id < 0) { itemError("widget not found"); continue; }

                int widgetType = VCType::fromString(details.machineType.toStdString());

                // 2. Validate the whole item before any mutation
                auto preflightErr = preflightUpdate(item, widgetType, details, doc, *vcBridge);
                if (!preflightErr.empty()) { itemError(preflightErr); continue; }
                auto validationErr = VCValidate::validate(item, widgetType, false);
                if (!validationErr.empty())
                {
                    itemError(nlohmann::json::parse(validationErr).value("error", validationErr));
                    continue;
                }
                QList<QPair<quint32, quint32>> channels;
                std::string channelsErr;
                if (item.contains("channels"))
                {
                    for (const auto &ch : item.at("channels"))
                    {
                        auto fieldErr = validateFields(ch, {"fixtureID", "channel"});
                        auto fixtureID = ch.contains("fixtureID") ? mcp::jsonInteger(ch.at("fixtureID"), 0, mcp::kMaxId) : std::nullopt;
                        auto channel = ch.contains("channel") ? mcp::jsonInteger(ch.at("channel"), 0, mcp::kMaxId) : std::nullopt;
                        if (!fieldErr.empty())
                            channelsErr = nlohmann::json::parse(fieldErr).value("error", fieldErr);
                        else if (!fixtureID || !channel)
                            channelsErr = mcp::integerError(!fixtureID ? "channels[].fixtureID" : "channels[].channel", 0, mcp::kMaxId);
                        if (!channelsErr.empty()) break;
                        channels.append(qMakePair(quint32(*fixtureID), quint32(*channel)));
                    }
                }
                if (!channelsErr.empty()) { itemError(channelsErr); continue; }

                // 3. Apply changes only after validation passes
                Json changes = Json::array();
                try {

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
                    if (item.contains("x")) geo.moveLeft(item.at("x").get<int>());
                    if (item.contains("y")) geo.moveTop(item.at("y").get<int>());
                    if (item.contains("width")) geo.setWidth(item.at("width").get<int>());
                    if (item.contains("height")) geo.setHeight(item.at("height").get<int>());
                    vcBridge->setWidgetGeometry(wid, geo);
                    changes.push_back({{"property", "geometry"}, {"status", "ok"}});
                }

                if (item.contains("functionID") && (widgetType == VCType::Button || widgetType == VCType::Slider))
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

                if (widgetType == VCType::XYPad && item.contains("invertedAppearance"))
                {
                    bool ok = vcBridge->setXYPadInvertedAppearance(wid,
                        item.at("invertedAppearance").get<bool>());
                    changes.push_back({{"property", "invertedAppearance"}, {"status", ok ? "ok" : "failed"}});
                }

                // Audio Triggers updates
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
                    if (item.contains("functionName"))
                    {
                        quint32 fid = mcp::resolveFunctionByName(doc, QString::fromStdString(item["functionName"].get<std::string>()));
                        bool ok = fid != Function::invalidId() && vcBridge->setSliderFunction(wid, fid);
                        changes.push_back({{"property", "functionName"}, {"status", ok ? "ok" : "failed"}});
                    }
                    VCBridge::SliderConfig sliderCfg;
                    bool hasSliderCfg = false;
                    auto str = [&](const char *key, std::optional<QString> &out) {
                        if (item.contains(key)) { out = QString::fromStdString(item[key].get<std::string>()); hasSliderCfg = true; }
                    };
                    str("clickAndGoType", sliderCfg.clickAndGoType);
                    str("valueDisplayStyle", sliderCfg.valueDisplayStyle);
                    str("gmValueMode", sliderCfg.gmValueMode);
                    str("gmChannelMode", sliderCfg.gmChannelMode);
                    if (item.contains("invertedAppearance")) { sliderCfg.invertedAppearance = item["invertedAppearance"].get<bool>(); hasSliderCfg = true; }
                    if (item.contains("monitorEnabled")) { sliderCfg.monitorEnabled = item["monitorEnabled"].get<bool>(); hasSliderCfg = true; }
                    if (item.contains("rangeLowLimit")) { sliderCfg.rangeLowLimit = item["rangeLowLimit"].get<double>(); hasSliderCfg = true; }
                    if (item.contains("rangeHighLimit")) { sliderCfg.rangeHighLimit = item["rangeHighLimit"].get<double>(); hasSliderCfg = true; }
                    if (hasSliderCfg)
                    {
                        bool ok = vcBridge->configureSlider(wid, sliderCfg);
                        changes.push_back({{"property", "sliderConfig"}, {"status", ok ? "ok" : "failed"}});
                    }
                }

                if (widgetType == VCType::Frame || widgetType == VCType::SoloFrame)
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

                if (widgetType == VCType::RecordPanel)
                {
                    VCBridge::RecordPanelConfig rpCfg;
                    bool hasRpCfg = false;
                    if (item.contains("targetFolder")) { rpCfg.targetFolder = QString::fromStdString(item["targetFolder"].get<std::string>()); hasRpCfg = true; }
                    if (item.contains("scenePrefix")) { rpCfg.scenePrefix = QString::fromStdString(item["scenePrefix"].get<std::string>()); hasRpCfg = true; }
                    if (item.contains("chaserPrefix")) { rpCfg.chaserPrefix = QString::fromStdString(item["chaserPrefix"].get<std::string>()); hasRpCfg = true; }
                    if (item.contains("defaultFadeIn")) { rpCfg.defaultFadeIn = item["defaultFadeIn"].get<int>(); hasRpCfg = true; }
                    if (item.contains("defaultHold")) { rpCfg.defaultHold = item["defaultHold"].get<int>(); hasRpCfg = true; }
                    if (item.contains("defaultFadeOut")) { rpCfg.defaultFadeOut = item["defaultFadeOut"].get<int>(); hasRpCfg = true; }
                    if (hasRpCfg)
                    {
                        bool ok = vcBridge->configureRecordPanel(wid, rpCfg);
                        changes.push_back({{"property", "recordPanelConfig"}, {"status", ok ? "ok" : "failed"}});
                    }
                }

                } catch (const std::exception &e) {
                    changes.push_back({{"property", "item"}, {"status", "failed"}, {"error", e.what()}});
                }

                bool failed = false;
                for (const auto &change : changes)
                    failed = failed || change.value("status", "") != "ok";
                Json rec = mcp::itemOk(i, {{"widgetID", wid}, {"outcome", "updated"}, {"changes", changes}});
                if (failed)
                {
                    // Unreachable after preflight; surfaced as an error, never as a partial edit.
                    rec["status"] = "error";
                    rec["error"] = "bridge rejected a change that passed preflight";
                    rec.erase("outcome");
                }
                results.push_back(rec);
            }
            return results.dump();
            });
        }
    )
    .set_description("Update Virtual Console widget properties. Sparse: only provided fields are changed. "
                     "Validates fields against widget type. Supports type-specific configuration for buttons, "
                     "sliders, frames, cue lists, matrices, clocks, speed dials, XY pads, audio triggers, and record panels. Batch. "
                     "Wrap multiple operations in {\"items\": [...]}. Each item is processed independently.")
    .set_annotations(mcp::kAnnotIdempotent));
}
