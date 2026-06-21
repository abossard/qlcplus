/*
  Q Light Controller Plus
  vc_query_helpers.h

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

#ifndef VC_QUERY_HELPERS_H
#define VC_QUERY_HELPERS_H

#include <nlohmann/json.hpp>
#include <QRegularExpression>
#include <QString>
#include <set>
#include <string>
#include <initializer_list>
#include "vcbridge.h"

namespace VCQueryPages {

using Json = nlohmann::json;

// ── Known widget type strings ────────────────────────────────────────────────

inline const std::set<std::string> kValidWidgetTypes = {
    "button", "slider", "xypad", "frame", "soloframe",
    "speedDial", "cuelist", "label", "audioTrigger", "matrix", "clock"
};

// ── Known property names for field selection ─────────────────────────────────
// Includes both individual properties and compound group names.

inline const std::set<std::string> kValidProperties = {
    // Core
    "type", "caption", "pageIndex", "parentID", "geometry",
    // Function binding
    "functionID", "action",
    // Slider
    "sliderMode", "channels", "widgetStyle",
    "clickAndGoType", "valueDisplayStyle", "invertedAppearance",
    "rangeLowLimit", "rangeHighLimit", "monitorEnabled",
    "gmValueMode", "gmChannelMode", "catchValues",
    // Frame
    "multipageMode", "totalPages", "currentPage", "pagesLoop", "pageLabels",
    "headerVisible", "enableButtonVisible", "collapsed",
    "soloframeMixing", "excludeMonitoredFunctions",
    "grid",
    // Input/feedback
    "inputMappings", "validSources",
    // Appearance
    "bgColor", "fgColor", "font", "backgroundImage", "disabled",
    // Button specific
    "iconPath", "startupIntensityEnabled", "startupIntensity",
    "flashOverride", "flashForceLTP", "stopAllFadeTime", "buttonState",
    // CueList specific
    "nextPrevBehavior", "playbackLayout", "sideFaderMode",
    // Clock specific
    "clockType", "countdownHours", "countdownMinutes", "countdownSeconds", "schedules",
    // SpeedDial specific
    "speedDialFunctions", "speedDialPresets",
    "absoluteValueMin", "absoluteValueMax", "speedDialVisibilityMask",
    "resetFactorOnDialChange",
    // Matrix specific
    "visibilityMask", "instantApply",
    "color1", "color2", "color3", "color4", "color5", "colors", "colorCount", "animation",
    // XY Pad specific
    "displayMode", "fixtures", "position", "presets",
    // Audio Triggers specific
    "captureEnabled", "volumeLevel", "barsNumber", "bars",
    // RecordPanel specific
    "targetFolder", "scenePrefix", "chaserPrefix",
    "defaultFadeIn", "defaultHold", "defaultFadeOut", "isRecordingChaser",
    // Compound groups
    "buttonConfig", "cueListConfig", "clockConfig",
    "speedDialConfig", "matrixConfig", "xyPadConfig",
    "recordPanelConfig",
};

// ── Compound group expansion ─────────────────────────────────────────────────

inline std::set<std::string> expandCompoundGroups(const std::set<std::string> &props)
{
    std::set<std::string> expanded = props;

    if (expanded.count("buttonConfig"))
    {
        expanded.erase("buttonConfig");
        for (auto &p : {"iconPath", "startupIntensityEnabled", "startupIntensity",
                        "flashOverride", "flashForceLTP", "stopAllFadeTime", "buttonState"})
            expanded.insert(p);
    }
    if (expanded.count("cueListConfig"))
    {
        expanded.erase("cueListConfig");
        for (auto &p : {"nextPrevBehavior", "playbackLayout", "sideFaderMode"})
            expanded.insert(p);
    }
    if (expanded.count("clockConfig"))
    {
        expanded.erase("clockConfig");
        for (auto &p : {"clockType", "countdownHours", "countdownMinutes",
                        "countdownSeconds", "schedules"})
            expanded.insert(p);
    }
    if (expanded.count("speedDialConfig"))
    {
        expanded.erase("speedDialConfig");
        for (auto &p : {"speedDialFunctions", "speedDialPresets",
                        "absoluteValueMin", "absoluteValueMax",
                        "speedDialVisibilityMask", "resetFactorOnDialChange"})
            expanded.insert(p);
    }
    if (expanded.count("matrixConfig"))
    {
        expanded.erase("matrixConfig");
        for (auto &p : {"visibilityMask", "instantApply",
                        "color1", "color2", "color3", "color4", "color5",
                        "colors", "colorCount", "animation"})
            expanded.insert(p);
    }
    if (expanded.count("xyPadConfig"))
    {
        expanded.erase("xyPadConfig");
        for (auto &p : {"displayMode", "fixtures", "position", "presets"})
            expanded.insert(p);
    }
    if (expanded.count("recordPanelConfig"))
    {
        expanded.erase("recordPanelConfig");
        for (auto &p : {"targetFolder", "scenePrefix", "chaserPrefix",
                        "defaultFadeIn", "defaultHold", "defaultFadeOut",
                        "isRecordingChaser"})
            expanded.insert(p);
    }

    return expanded;
}

// ── Glob matching ────────────────────────────────────────────────────────────

inline bool globMatch(const QString &pattern, const QString &text)
{
    QString rePattern = QRegularExpression::wildcardToRegularExpression(
        pattern, QRegularExpression::UnanchoredWildcardConversion);
    // wildcardToRegularExpression produces an anchored pattern by default on Qt 6.
    // UnanchoredWildcardConversion (Qt 6.6+) gives us unanchored, but we need
    // a full match. Force anchoring:
    if (!rePattern.startsWith("\\A") && !rePattern.startsWith("^"))
        rePattern = "\\A(?:" + rePattern + ")\\z";
    QRegularExpression re(rePattern, QRegularExpression::CaseInsensitiveOption);
    return re.match(text).hasMatch();
}

// ── Argument validation ──────────────────────────────────────────────────────

inline std::string jsonTypeName(const Json &v)
{
    if (v.is_string())           return "string";
    if (v.is_number_integer())   return "integer";
    if (v.is_number_float())     return "float";
    if (v.is_boolean())          return "boolean";
    if (v.is_array())            return "array";
    if (v.is_object())           return "object";
    if (v.is_null())             return "null";
    return "unknown";
}

inline std::string validateArgs(const Json &args)
{
    if (!args.is_object())
        return Json({{"error", "arguments must be a JSON object"}}).dump();

    // Reject unknown top-level fields
    static const std::initializer_list<std::string> kAllowed = {
        "nameFilter", "typeFilter", "functionID", "fixtureID",
        "channel", "pageIndex", "parentID", "properties"
    };
    for (auto it = args.begin(); it != args.end(); ++it)
    {
        bool found = false;
        for (const auto &a : kAllowed)
            if (it.key() == a) { found = true; break; }
        if (!found)
        {
            std::string msg = "unknown field '" + it.key() + "'. Allowed: ";
            bool first = true;
            for (const auto &a : kAllowed)
            {
                if (!first) msg += ", ";
                msg += a;
                first = false;
            }
            return Json({{"error", msg}}).dump();
        }
    }

    // nameFilter: must be string
    if (args.contains("nameFilter"))
    {
        if (!args["nameFilter"].is_string())
            return Json({{"error", "nameFilter must be a string, got " +
                          jsonTypeName(args["nameFilter"])}}).dump();
    }

    // typeFilter: string or array of strings, each must be a known type
    if (args.contains("typeFilter"))
    {
        const auto &tf = args["typeFilter"];
        if (tf.is_string())
        {
            if (kValidWidgetTypes.count(tf.get<std::string>()) == 0)
            {
                std::string msg = "unknown widget type '" + tf.get<std::string>() +
                                  "' in typeFilter. Valid types: ";
                bool first = true;
                for (const auto &t : kValidWidgetTypes)
                {
                    if (!first) msg += ", ";
                    msg += t;
                    first = false;
                }
                return Json({{"error", msg}}).dump();
            }
        }
        else if (tf.is_array())
        {
            for (size_t i = 0; i < tf.size(); ++i)
            {
                if (!tf[i].is_string())
                    return Json({{"error", "typeFilter must be a string or array of strings, "
                                  "but element " + std::to_string(i) + " is " +
                                  jsonTypeName(tf[i])}}).dump();
                if (kValidWidgetTypes.count(tf[i].get<std::string>()) == 0)
                {
                    std::string msg = "unknown widget type '" + tf[i].get<std::string>() +
                                      "' in typeFilter. Valid types: ";
                    bool first = true;
                    for (const auto &t : kValidWidgetTypes)
                    {
                        if (!first) msg += ", ";
                        msg += t;
                        first = false;
                    }
                    return Json({{"error", msg}}).dump();
                }
            }
        }
        else
        {
            return Json({{"error", "typeFilter must be a string or array of strings, got " +
                          jsonTypeName(tf)}}).dump();
        }
    }

    // Integer parameters: functionID, fixtureID, channel, pageIndex, parentID
    for (const auto &field : {"functionID", "fixtureID", "channel", "pageIndex", "parentID"})
    {
        if (args.contains(field))
        {
            const auto &v = args[field];
            if (!v.is_number_integer() || v.get<int64_t>() < 0)
                return Json({{"error", std::string(field) +
                              " must be a non-negative integer, got " +
                              jsonTypeName(v) +
                              (v.is_number() ? " (" + v.dump() + ")" : "")}}).dump();
        }
    }

    // properties: array of strings, each must be a known property
    if (args.contains("properties"))
    {
        const auto &props = args["properties"];
        if (!props.is_array())
            return Json({{"error", "properties must be an array of strings, got " +
                          jsonTypeName(props)}}).dump();
        for (size_t i = 0; i < props.size(); ++i)
        {
            if (!props[i].is_string())
                return Json({{"error", "properties must be an array of strings, "
                              "but element " + std::to_string(i) + " is " +
                              jsonTypeName(props[i])}}).dump();
            const auto &pname = props[i].get<std::string>();
            if (kValidProperties.count(pname) == 0)
            {
                std::string msg = "unknown property '" + pname +
                                  "' in properties. Valid properties: ";
                bool first = true;
                for (const auto &p : kValidProperties)
                {
                    if (!first) msg += ", ";
                    msg += p;
                    first = false;
                }
                return Json({{"error", msg}}).dump();
            }
        }
    }

    return {};  // valid
}

// ── Widget filtering ─────────────────────────────────────────────────────────

inline bool filterWidget(const VCBridge::WidgetDetails &d, const Json &args)
{
    // nameFilter — case-insensitive glob
    if (args.contains("nameFilter"))
    {
        QString pattern = QString::fromStdString(args["nameFilter"].get<std::string>());
        if (!globMatch(pattern, d.caption))
            return false;
    }

    // typeFilter — single string or array of strings
    if (args.contains("typeFilter"))
    {
        const auto &tf = args["typeFilter"];
        std::string wtype = d.type.toLower().toStdString();
        bool match = false;
        if (tf.is_string())
        {
            match = (wtype == tf.get<std::string>());
        }
        else if (tf.is_array())
        {
            for (const auto &t : tf)
            {
                if (wtype == t.get<std::string>())
                {
                    match = true;
                    break;
                }
            }
        }
        if (!match) return false;
    }

    // functionID
    if (args.contains("functionID"))
    {
        int fid = args["functionID"].get<int>();
        if ((int)d.functionID != fid)
            return false;
    }

    // fixtureID and/or channel — check slider channels list
    if (args.contains("fixtureID") || args.contains("channel"))
    {
        bool hasFixtureFilter = args.contains("fixtureID");
        bool hasChannelFilter = args.contains("channel");
        int filterFixtureID = hasFixtureFilter ? args["fixtureID"].get<int>() : -1;
        int filterChannel = hasChannelFilter ? args["channel"].get<int>() : -1;

        bool found = false;
        for (const auto &ch : d.channels)
        {
            bool fixtureOk = !hasFixtureFilter || (int)ch.first == filterFixtureID;
            bool channelOk = !hasChannelFilter || (int)ch.second == filterChannel;
            if (fixtureOk && channelOk)
            {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }

    // parentID
    if (args.contains("parentID"))
    {
        int pid = args["parentID"].get<int>();
        if (d.parentID != pid)
            return false;
    }

    return true;
}

// ── Widget serialization with field selection ────────────────────────────────

inline Json serializeWidget(const VCBridge::WidgetDetails &d,
                            const std::set<std::string> &properties)
{
    // If properties is empty, include everything (backward compat)
    auto has = [&properties](const char *name) -> bool {
        return properties.empty() || properties.count(name);
    };

    Json w;
    w["id"] = d.id;  // always included

    if (has("type"))      w["type"]     = d.type.toStdString();
    if (has("caption"))   w["caption"]  = d.caption.toStdString();
    if (has("parentID"))  w["parentID"] = d.parentID;
    if (has("geometry"))
    {
        w["x"] = d.geometry.x();
        w["y"] = d.geometry.y();
        w["width"]  = d.geometry.width();
        w["height"] = d.geometry.height();
    }

    // Function binding
    if (has("functionID") && d.functionID != 0 && d.functionID != (quint32)-1)
        w["functionID"] = (int)d.functionID;
    if (has("action") && !d.action.isEmpty())
        w["action"] = d.action.toStdString();

    // Slider
    if (has("sliderMode") && !d.sliderMode.isEmpty())
        w["sliderMode"] = d.sliderMode.toStdString();
    if (has("channels") && !d.channels.isEmpty())
    {
        Json chArr = Json::array();
        for (auto &ch : d.channels)
            chArr.push_back({{"fixtureID", (int)ch.first}, {"channel", (int)ch.second}});
        w["channels"] = chArr;
    }
    if (has("widgetStyle") && !d.widgetStyle.isEmpty())
        w["widgetStyle"] = d.widgetStyle.toStdString();
    if (has("clickAndGoType") && !d.clickAndGoType.isEmpty() && d.clickAndGoType != "none")
        w["clickAndGoType"] = d.clickAndGoType.toStdString();
    if (has("valueDisplayStyle") && !d.valueDisplayStyle.isEmpty() && d.valueDisplayStyle != "dmx")
        w["valueDisplayStyle"] = d.valueDisplayStyle.toStdString();
    if (has("invertedAppearance") && d.sliderInvertedAppearance)
        w["invertedAppearance"] = true;
    if (has("rangeLowLimit") && d.rangeLowLimit > 0)
        w["rangeLowLimit"] = d.rangeLowLimit;
    if (has("rangeHighLimit") && d.rangeHighLimit < 255)
        w["rangeHighLimit"] = d.rangeHighLimit;
    if (has("monitorEnabled") && d.monitorEnabled)
        w["monitorEnabled"] = true;
    if (has("gmValueMode") && !d.gmValueMode.isEmpty())
        w["gmValueMode"] = d.gmValueMode.toStdString();
    if (has("gmChannelMode") && !d.gmChannelMode.isEmpty())
        w["gmChannelMode"] = d.gmChannelMode.toStdString();
    if (has("catchValues") && d.catchValues)
        w["catchValues"] = true;

    // Input mappings
    if (has("inputMappings") && !d.inputMappings.isEmpty())
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
        w["inputMappings"] = inputs;
    }

    // Valid sources
    if (has("validSources") && !d.validSources.isEmpty())
    {
        Json vs = Json::array();
        for (const auto &s : d.validSources)
            vs.push_back({{"name", s.name.toStdString()},
                          {"id", s.id},
                          {"description", s.description.toStdString()}});
        w["validSources"] = vs;
    }

    // Appearance
    if (has("bgColor") && d.bgColor.isValid())
        w["bgColor"] = d.bgColor.name().toStdString();
    if (has("fgColor") && d.fgColor.isValid())
        w["fgColor"] = d.fgColor.name().toStdString();
    if (has("backgroundImage") && !d.backgroundImage.isEmpty())
        w["backgroundImage"] = d.backgroundImage.toStdString();
    if (has("disabled") && d.disabled)
        w["disabled"] = true;
    if (has("font") && (d.fontConfig.family.has_value() || d.fontConfig.pointSize.has_value()))
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
        w["font"] = fontJson;
    }

    // Button extended
    if (has("iconPath") && !d.iconPath.isEmpty())
        w["iconPath"] = d.iconPath.toStdString();
    if (has("startupIntensityEnabled") && d.startupIntensityEnabled)
    {
        w["startupIntensityEnabled"] = true;
        if (has("startupIntensity"))
            w["startupIntensity"] = d.startupIntensity;
    }
    if (has("flashOverride") && d.flashOverride)
        w["flashOverride"] = true;
    if (has("flashForceLTP") && d.flashForceLTP)
        w["flashForceLTP"] = true;
    if (has("stopAllFadeTime") && d.stopAllFadeTime > 0)
        w["stopAllFadeTime"] = d.stopAllFadeTime;
    if (has("buttonState"))
    {
        static const char *stateNames[] = {"inactive", "monitoring", "active"};
        int st = qBound(0, d.buttonState, 2);
        w["buttonState"] = stateNames[st];
    }

    // Frame extended
    if (has("multipageMode") && d.multipageMode)
    {
        w["multipageMode"] = true;
        if (has("totalPages"))   w["totalPages"]  = d.totalPages;
        if (has("currentPage"))  w["currentPage"] = d.currentPage;
        if (has("pagesLoop"))    w["pagesLoop"]   = d.pagesLoop;
        if (has("pageLabels") && !d.pageLabels.isEmpty())
        {
            auto arr = Json::array();
            for (const auto &lbl : d.pageLabels)
                arr.push_back(lbl.toStdString());
            w["pageLabels"] = arr;
        }
    }
    if (has("headerVisible") && !d.headerVisible)
        w["headerVisible"] = false;
    if (has("enableButtonVisible") && d.enableButtonVisible)
        w["enableButtonVisible"] = true;
    if (has("collapsed") && d.collapsed)
        w["collapsed"] = true;
    if (has("soloframeMixing") && d.soloframeMixing)
        w["soloframeMixing"] = true;
    if (has("excludeMonitoredFunctions") && d.excludeMonitoredFunctions)
        w["excludeMonitoredFunctions"] = true;

    // Frame grid layout (only for frame/soloframe)
    if (has("grid") && (d.type == "frame" || d.type == "soloframe"))
    {
        Json g;
        g["layoutMode"] = d.gridLayoutMode.isEmpty()
            ? std::string("free") : d.gridLayoutMode.toStdString();
        g["columns"]    = d.gridColumns;
        g["rowHeight"]  = d.gridRowHeight;
        g["compact"]    = d.gridCompact;
        w["grid"] = g;
    }

    // CueList extended
    if (has("nextPrevBehavior") && !d.nextPrevBehavior.isEmpty())
        w["nextPrevBehavior"] = d.nextPrevBehavior.toStdString();
    if (has("playbackLayout") && !d.playbackLayout.isEmpty())
        w["playbackLayout"] = d.playbackLayout.toStdString();
    if (has("sideFaderMode") && !d.sideFaderMode.isEmpty())
        w["sideFaderMode"] = d.sideFaderMode.toStdString();

    // Clock extended
    if (has("clockType") && !d.clockType.isEmpty())
        w["clockType"] = d.clockType.toStdString();
    if (has("countdownHours") || has("countdownMinutes") || has("countdownSeconds"))
    {
        if (d.countdownH > 0 || d.countdownM > 0 || d.countdownS > 0)
        {
            if (has("countdownHours"))   w["countdownHours"]   = d.countdownH;
            if (has("countdownMinutes")) w["countdownMinutes"] = d.countdownM;
            if (has("countdownSeconds")) w["countdownSeconds"] = d.countdownS;
        }
    }
    if (has("schedules") && !d.clockSchedules.isEmpty())
    {
        Json schedArr = Json::array();
        for (auto &sch : d.clockSchedules)
            schedArr.push_back({{"functionID", (int)sch.functionID},
                                {"hour", sch.hour}, {"minute", sch.minute}, {"second", sch.second}});
        w["schedules"] = schedArr;
    }

    // SpeedDial extended
    if (has("speedDialFunctions") && !d.speedDialFunctions.isEmpty())
    {
        Json funcArr = Json::array();
        for (auto &f : d.speedDialFunctions)
            funcArr.push_back({{"functionID", (int)f.functionID},
                               {"fadeInMultiplier", f.fadeInMultiplier.toStdString()},
                               {"fadeOutMultiplier", f.fadeOutMultiplier.toStdString()},
                               {"durationMultiplier", f.durationMultiplier.toStdString()}});
        w["speedDialFunctions"] = funcArr;
    }
    if (has("speedDialPresets") && !d.speedDialPresets.isEmpty())
    {
        Json presetArr = Json::array();
        for (auto &p : d.speedDialPresets)
            presetArr.push_back({{"name", p.name.toStdString()}, {"value", p.value}});
        w["speedDialPresets"] = presetArr;
    }

    // Matrix extended
    if (has("visibilityMask") && d.matrixVisibilityMask > 0)
        w["visibilityMask"] = (int)d.matrixVisibilityMask;
    if (has("instantApply") && d.matrixInstantApply)
        w["instantApply"] = true;
    if (has("color1") && d.matrixColor1.isValid())
        w["color1"] = d.matrixColor1.name().toStdString();
    if (has("color2") && d.matrixColor2.isValid())
        w["color2"] = d.matrixColor2.name().toStdString();
    if (has("color3") && d.matrixColor3.isValid())
        w["color3"] = d.matrixColor3.name().toStdString();
    if (has("color4") && d.matrixColor4.isValid())
        w["color4"] = d.matrixColor4.name().toStdString();
    if (has("color5") && d.matrixColor5.isValid())
        w["color5"] = d.matrixColor5.name().toStdString();
    if (has("animation") && !d.matrixAnimation.isEmpty())
        w["animation"] = d.matrixAnimation.toStdString();
    if (has("colorCount") && d.matrixColorCount > 0)
        w["colorCount"] = d.matrixColorCount;
    if (has("colors") && !d.matrixColors.isEmpty())
    {
        Json arr = Json::array();
        for (const auto &c : d.matrixColors)
            arr.push_back(c.isValid() ? c.name().toStdString() : "");
        w["colors"] = arr;
    }

    // XY Pad extended
    if (has("displayMode") && !d.displayMode.isEmpty())
    {
        w["displayMode"] = d.displayMode.toStdString();
        if (has("invertedAppearance"))
            w["invertedAppearance"] = d.invertedAppearance;
        if (has("position"))
            w["position"] = {{"x", d.xyPadPosition.x()}, {"y", d.xyPadPosition.y()}};
    }
    if (has("fixtures") && !d.xyPadFixtures.isEmpty())
    {
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
        w["fixtures"] = fxArr;
    }
    if (has("presets") && !d.xyPadPresets.isEmpty())
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
        w["presets"] = presetArr;
    }

    // Audio Triggers extended
    if (has("captureEnabled") && d.barsNumber > 0)
        w["captureEnabled"] = d.captureEnabled;
    if (has("volumeLevel") && d.barsNumber > 0)
        w["volumeLevel"] = d.volumeLevel;
    if (has("barsNumber") && d.barsNumber > 0)
        w["barsNumber"] = d.barsNumber;
    if (has("bars") && !d.audioBars.isEmpty())
    {
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
        w["bars"] = barsArr;
    }

    // RecordPanel extended
    if (has("targetFolder") && !d.rpTargetFolder.isEmpty())
        w["targetFolder"] = d.rpTargetFolder.toStdString();
    if (has("scenePrefix") && !d.rpScenePrefix.isEmpty())
        w["scenePrefix"] = d.rpScenePrefix.toStdString();
    if (has("chaserPrefix") && !d.rpChaserPrefix.isEmpty())
        w["chaserPrefix"] = d.rpChaserPrefix.toStdString();
    if (has("defaultFadeIn"))
        w["defaultFadeIn"] = (int)d.rpDefaultFadeIn;
    if (has("defaultHold"))
        w["defaultHold"] = (int)d.rpDefaultHold;
    if (has("defaultFadeOut"))
        w["defaultFadeOut"] = (int)d.rpDefaultFadeOut;
    if (has("isRecordingChaser"))
        w["isRecordingChaser"] = d.rpIsRecordingChaser;

    return w;
}

// ── Parse properties set from args ───────────────────────────────────────────

inline std::set<std::string> parseProperties(const Json &args)
{
    std::set<std::string> props;
    if (args.contains("properties"))
    {
        for (const auto &p : args["properties"])
            props.insert(p.get<std::string>());
        props = expandCompoundGroups(props);
    }
    return props;  // empty = all properties
}

} // namespace VCQueryPages

#endif // VC_QUERY_HELPERS_H
