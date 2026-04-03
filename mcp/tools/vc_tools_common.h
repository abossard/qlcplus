/*
  Q Light Controller Plus
  vc_tools_common.h

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

#ifndef VC_TOOLS_COMMON_H
#define VC_TOOLS_COMMON_H

#include <QMap>
#include <QString>
#include <QRegularExpression>
#include <nlohmann/json.hpp>

#include <string>
#include <vector>
#include <set>
#include <initializer_list>

/*
 * Widget type enum values from VCWidget::WidgetType.
 * Duplicated here to avoid pulling in the full UI header in tools code.
 */
namespace VCType
{
    enum WidgetType
    {
        Unknown          = 0,
        Button           = 1,
        Slider           = 2,
        XYPad            = 3,
        Frame            = 4,
        SoloFrame        = 5,
        SpeedDial        = 6,
        CueList          = 7,
        Label            = 8,
        AudioTriggers    = 9,
        Animation        = 10, // VCMatrix
        Clock            = 11
    };

    /** Map JSON type name → enum. Used by vc_create_widgets. */
    inline int fromString(const std::string &name)
    {
        static const QMap<std::string, int> map = {
            {"button",       Button},
            {"slider",       Slider},
            {"xypad",        XYPad},
            {"frame",        Frame},
            {"soloframe",    SoloFrame},
            {"speedDial",    SpeedDial},
            {"cuelist",      CueList},
            {"label",        Label},
            {"audioTrigger", AudioTriggers},
            {"matrix",       Animation},
            {"clock",        Clock}
        };
        auto it = map.find(name);
        return (it != map.end()) ? it.value() : Unknown;
    }

    /** Map VCWidget::typeToString output → enum. Used by vc_update_widgets. */
    inline int fromDisplayString(const QString &name)
    {
        static const QMap<QString, int> map = {
            {"Button",          Button},
            {"Slider",          Slider},
            {"XYPad",           XYPad},
            {"Frame",           Frame},
            {"Solo frame",      SoloFrame},
            {"Speed dial",      SpeedDial},
            {"Cue list",        CueList},
            {"Label",           Label},
            {"Audio Triggers",  AudioTriggers},
            {"Animation",       Animation},
            {"Clock",           Clock}
        };
        auto it = map.find(name);
        return (it != map.end()) ? it.value() : Unknown;
    }

    /** Map enum → JSON type name (for error messages). */
    inline std::string toString(int type)
    {
        static const QMap<int, std::string> map = {
            {Button,        "button"},
            {Slider,        "slider"},
            {XYPad,         "xypad"},
            {Frame,         "frame"},
            {SoloFrame,     "soloframe"},
            {SpeedDial,     "speedDial"},
            {CueList,       "cuelist"},
            {Label,         "label"},
            {AudioTriggers, "audioTrigger"},
            {Animation,     "matrix"},
            {Clock,         "clock"}
        };
        auto it = map.find(type);
        return (it != map.end()) ? it.value() : "unknown";
    }
}

// ─── Per-type allowed-field lists ──────────────────────────────────

namespace VCFields
{
    // Common fields accepted by ALL widget types on create
    inline const std::vector<std::string> &commonCreate()
    {
        static const std::vector<std::string> fields = {
            "type", "parentID", "caption", "upsert",
            "x", "y", "width", "height",
            "bgColor", "fgColor",
            "functionID", "functionName"
        };
        return fields;
    }

    // Common fields accepted by ALL widget types on update
    inline const std::vector<std::string> &commonUpdate()
    {
        static const std::vector<std::string> fields = {
            "widgetID", "caption",
            "x", "y", "width", "height",
            "bgColor", "fgColor",
            "font", "backgroundImage", "disabled"
        };
        return fields;
    }

    // Type-specific extra fields for create
    inline const std::vector<std::string> &createFieldsForType(int type)
    {
        using namespace VCType;
        static const QMap<int, std::vector<std::string>> map = {
            {Button, {
                "action", "iconPath", "keySequence",
                "startupIntensityEnabled", "startupIntensity",
                "flashOverride", "flashForceLTP", "stopAllFadeTime"
            }},
            {Slider, {
                "mode", "widgetStyle", "channels",
                "clickAndGoType", "valueDisplayStyle", "invertedAppearance",
                "rangeLowLimit", "rangeHighLimit", "monitorEnabled",
                "gmValueMode", "gmChannelMode"
            }},
            {Frame, {
                "pageIndex", "solo", "multipageMode", "totalPages", "pagesLoop",
                "pageLabels", "headerVisible", "enableButtonVisible"
            }},
            {SoloFrame, {
                "pageIndex", "multipageMode", "totalPages", "pagesLoop",
                "pageLabels", "headerVisible", "enableButtonVisible",
                "soloframeMixing", "excludeMonitoredFunctions"
            }},
            {XYPad, {
                "size", "fixtureIDs", "fixtures",
                "displayMode", "invertedAppearance"
            }},
            {CueList, {
                "chaserID", "chaserName",
                "nextPrevBehavior", "playbackLayout", "sideFaderMode"
            }},
            {Label, {"text"}},
            {SpeedDial, {
                "functions", "functionIDs",
                "absoluteValueMin", "absoluteValueMax",
                "visibilityMask", "resetFactorOnDialChange"
            }},
            {AudioTriggers, {
                "captureEnabled", "volumeLevel", "barsNumber"
            }},
            {Animation, {
                "instantApply", "visibilityMask"
            }},
            {Clock, {
                "clockType",
                "countdownHours", "countdownMinutes", "countdownSeconds",
                "schedules"
            }}
        };
        static const std::vector<std::string> empty;
        auto it = map.find(type);
        return (it != map.end()) ? it.value() : empty;
    }

    // Type-specific extra fields for update
    inline const std::vector<std::string> &updateFieldsForType(int type)
    {
        using namespace VCType;
        static const QMap<int, std::vector<std::string>> map = {
            {Button, {
                "functionID", "functionName", "action",
                "iconPath", "keySequence",
                "startupIntensityEnabled", "startupIntensity",
                "flashOverride", "flashForceLTP", "stopAllFadeTime"
            }},
            {Slider, {
                "functionID", "functionName",
                "mode", "widgetStyle", "channels",
                "clickAndGoType", "valueDisplayStyle", "invertedAppearance",
                "rangeLowLimit", "rangeHighLimit", "monitorEnabled", "catchValues",
                "gmValueMode", "gmChannelMode"
            }},
            {Frame, {
                "multipageMode", "totalPages", "currentPage", "pagesLoop",
                "pageLabels", "headerVisible", "enableButtonVisible", "collapsed"
            }},
            {SoloFrame, {
                "multipageMode", "totalPages", "currentPage", "pagesLoop",
                "pageLabels", "headerVisible", "enableButtonVisible", "collapsed",
                "soloframeMixing", "excludeMonitoredFunctions"
            }},
            {XYPad, {
                "displayMode", "invertedAppearance",
                "position", "xyPadPosition", "presets"
            }},
            {CueList, {
                "chaserID", "chaserName",
                "nextPrevBehavior", "playbackLayout", "sideFaderMode"
            }},
            {Label, {}},
            {SpeedDial, {
                "functions", "presets",
                "absoluteValueMin", "absoluteValueMax",
                "visibilityMask", "resetFactorOnDialChange"
            }},
            {AudioTriggers, {
                "captureEnabled", "volumeLevel", "barsNumber", "bars"
            }},
            {Animation, {
                "functionID", "functionName",
                "color1", "color2", "color3", "color4", "color5",
                "animation", "instantApply",
                "visibilityMask", "customControls"
            }},
            {Clock, {
                "clockType",
                "countdownHours", "countdownMinutes", "countdownSeconds",
                "schedules"
            }}
        };
        static const std::vector<std::string> empty;
        auto it = map.find(type);
        return (it != map.end()) ? it.value() : empty;
    }
}

// ─── Validation functions ──────────────────────────────────────────

namespace VCValidate
{
    using Json = nlohmann::json;

    /**
     * Build the full allowed-field set for a widget type + operation.
     * Merges common fields with type-specific fields.
     */
    inline std::set<std::string> allowedFields(int widgetType, bool isCreate)
    {
        std::set<std::string> allowed;
        const auto &common = isCreate
            ? VCFields::commonCreate()
            : VCFields::commonUpdate();
        for (const auto &f : common)
            allowed.insert(f);

        const auto &typeSpecific = isCreate
            ? VCFields::createFieldsForType(widgetType)
            : VCFields::updateFieldsForType(widgetType);
        for (const auto &f : typeSpecific)
            allowed.insert(f);

        return allowed;
    }

    /**
     * Validate that all fields in a JSON object are in the allowed set
     * for the given widget type. Returns empty string on success,
     * or a JSON error string with details on failure.
     */
    inline std::string validateFieldsForType(
        const Json &obj, int widgetType, bool isCreate)
    {
        if (!obj.is_object()) return "";

        auto allowed = allowedFields(widgetType, isCreate);
        std::vector<std::string> unknown;

        for (auto it = obj.begin(); it != obj.end(); ++it)
        {
            if (allowed.find(it.key()) == allowed.end())
                unknown.push_back(it.key());
        }

        if (unknown.empty()) return "";

        std::string typeName = VCType::toString(widgetType);
        std::string msg = "field";
        if (unknown.size() > 1) msg += "s";
        msg += " not valid for widget type '" + typeName + "': ";
        for (size_t i = 0; i < unknown.size(); i++)
        {
            if (i > 0) msg += ", ";
            msg += "'" + unknown[i] + "'";
        }

        // Build allowed list for the error message
        msg += ". Allowed: ";
        bool first = true;
        for (const auto &a : allowed)
        {
            if (!first) msg += ", ";
            msg += a;
            first = false;
        }

        Json errJson;
        errJson["error"] = msg;
        errJson["widgetType"] = typeName;
        if (obj.contains("widgetID"))
            errJson["widgetID"] = obj["widgetID"];
        Json unknownArr = Json::array();
        for (const auto &u : unknown)
            unknownArr.push_back(u);
        errJson["invalidFields"] = unknownArr;

        return errJson.dump();
    }

    // ── Value validation helpers ──

    inline bool isValidEnum(const std::string &value,
                             std::initializer_list<std::string> allowed)
    {
        for (const auto &a : allowed)
            if (value == a) return true;
        return false;
    }

    inline bool isValidHexColor(const std::string &value)
    {
        static QRegularExpression re("^#[0-9a-fA-F]{6}$");
        return re.match(QString::fromStdString(value)).hasMatch();
    }

    /**
     * Validate field values (enums, ranges, formats) for a JSON item.
     * Returns empty string on success, or JSON error string on failure.
     * Called after validateFieldsForType (so all field names are known-valid).
     */
    inline std::string validateFieldValues(const Json &item, int widgetType)
    {
        auto mkErr = [&](const std::string &field, const std::string &msg) -> std::string {
            Json err;
            err["error"] = msg;
            err["field"] = field;
            err["widgetType"] = VCType::toString(widgetType);
            if (item.contains("widgetID"))
                err["widgetID"] = item["widgetID"];
            return err.dump();
        };

        // type (create only)
        if (item.contains("type"))
        {
            auto t = item["type"].get<std::string>();
            if (VCType::fromString(t) == VCType::Unknown)
                return mkErr("type", "invalid widget type '" + t +
                    "'. Must be one of: button, slider, frame, soloframe, xypad, "
                    "cuelist, label, speedDial, audioTrigger, matrix, clock");
        }

        // action (button)
        if (item.contains("action"))
        {
            auto v = item["action"].get<std::string>();
            if (!isValidEnum(v, {"toggle", "flash", "blackout", "stopall"}))
                return mkErr("action", "invalid value '" + v +
                    "'. Must be one of: toggle, flash, blackout, stopall");
        }

        // mode (slider)
        if (item.contains("mode"))
        {
            auto v = item["mode"].get<std::string>();
            if (!isValidEnum(v, {"level", "playback", "submaster", "grandmaster"}))
                return mkErr("mode", "invalid value '" + v +
                    "'. Must be one of: level, playback, submaster, grandmaster");
        }

        // widgetStyle (slider)
        if (item.contains("widgetStyle"))
        {
            auto v = item["widgetStyle"].get<std::string>();
            if (!isValidEnum(v, {"slider", "knob"}))
                return mkErr("widgetStyle", "invalid value '" + v +
                    "'. Must be one of: slider, knob");
        }

        // clickAndGoType (slider)
        if (item.contains("clickAndGoType"))
        {
            auto v = item["clickAndGoType"].get<std::string>();
            if (!isValidEnum(v, {"none", "colors", "preset", "rgb", "cmy"}))
                return mkErr("clickAndGoType", "invalid value '" + v +
                    "'. Must be one of: none, colors, preset, rgb, cmy");
        }

        // valueDisplayStyle (slider)
        if (item.contains("valueDisplayStyle"))
        {
            auto v = item["valueDisplayStyle"].get<std::string>();
            if (!isValidEnum(v, {"dmx", "percentage"}))
                return mkErr("valueDisplayStyle", "invalid value '" + v +
                    "'. Must be one of: dmx, percentage");
        }

        // gmValueMode (slider grandmaster)
        if (item.contains("gmValueMode"))
        {
            auto v = item["gmValueMode"].get<std::string>();
            if (!isValidEnum(v, {"limit", "reduce"}))
                return mkErr("gmValueMode", "invalid value '" + v +
                    "'. Must be one of: limit, reduce");
        }

        // gmChannelMode (slider grandmaster)
        if (item.contains("gmChannelMode"))
        {
            auto v = item["gmChannelMode"].get<std::string>();
            if (!isValidEnum(v, {"intensity", "allchannels"}))
                return mkErr("gmChannelMode", "invalid value '" + v +
                    "'. Must be one of: intensity, allchannels");
        }

        // displayMode (xypad)
        if (item.contains("displayMode"))
        {
            auto v = item["displayMode"].get<std::string>();
            if (!isValidEnum(v, {"degrees", "percentage", "dmx"}))
                return mkErr("displayMode", "invalid value '" + v +
                    "'. Must be one of: degrees, percentage, dmx");
        }

        // nextPrevBehavior (cuelist)
        if (item.contains("nextPrevBehavior"))
        {
            auto v = item["nextPrevBehavior"].get<std::string>();
            if (!isValidEnum(v, {"defaultRunFirst", "runNext", "select", "nothing"}))
                return mkErr("nextPrevBehavior", "invalid value '" + v +
                    "'. Must be one of: defaultRunFirst, runNext, select, nothing");
        }

        // playbackLayout (cuelist)
        if (item.contains("playbackLayout"))
        {
            auto v = item["playbackLayout"].get<std::string>();
            if (!isValidEnum(v, {"playPauseStop", "playStopPause"}))
                return mkErr("playbackLayout", "invalid value '" + v +
                    "'. Must be one of: playPauseStop, playStopPause");
        }

        // sideFaderMode (cuelist)
        if (item.contains("sideFaderMode"))
        {
            auto v = item["sideFaderMode"].get<std::string>();
            if (!isValidEnum(v, {"none", "crossfade", "steps"}))
                return mkErr("sideFaderMode", "invalid value '" + v +
                    "'. Must be one of: none, crossfade, steps");
        }

        // clockType (clock)
        if (item.contains("clockType"))
        {
            auto v = item["clockType"].get<std::string>();
            if (!isValidEnum(v, {"clock", "stopwatch", "countdown"}))
                return mkErr("clockType", "invalid value '" + v +
                    "'. Must be one of: clock, stopwatch, countdown");
        }

        // Numeric range checks
        if (item.contains("startupIntensity"))
        {
            double v = item["startupIntensity"].get<double>();
            if (v < 0.0 || v > 1.0)
                return mkErr("startupIntensity", "value " + std::to_string(v) +
                    " out of range. Must be 0.0-1.0");
        }

        if (item.contains("rangeLowLimit"))
        {
            double v = item["rangeLowLimit"].get<double>();
            if (v < 0 || v > 255)
                return mkErr("rangeLowLimit", "value " + std::to_string(v) +
                    " out of range. Must be 0-255");
        }

        if (item.contains("rangeHighLimit"))
        {
            double v = item["rangeHighLimit"].get<double>();
            if (v < 0 || v > 255)
                return mkErr("rangeHighLimit", "value " + std::to_string(v) +
                    " out of range. Must be 0-255");
        }

        if (item.contains("stopAllFadeTime"))
        {
            int v = item["stopAllFadeTime"].get<int>();
            if (v < 0)
                return mkErr("stopAllFadeTime", "value must be >= 0");
        }

        // Color format checks
        for (const auto &colorField : {"bgColor", "fgColor",
                "color1", "color2", "color3", "color4", "color5"})
        {
            if (item.contains(colorField))
            {
                auto v = item[colorField].get<std::string>();
                if (!isValidHexColor(v))
                    return mkErr(colorField, "invalid hex color '" + v +
                        "'. Must be format #rrggbb");
            }
        }

        // Font object validation
        if (item.contains("font"))
        {
            if (!item["font"].is_object())
                return mkErr("font", "must be an object with optional fields: family, size, bold, italic");
            const auto &font = item["font"];
            for (auto it = font.begin(); it != font.end(); ++it)
            {
                if (!isValidEnum(it.key(), {"family", "size", "bold", "italic"}))
                    return mkErr("font", "unknown font field '" + it.key() +
                        "'. Allowed: family, size, bold, italic");
            }
            if (font.contains("size"))
            {
                int s = font["size"].get<int>();
                if (s < 1 || s > 200)
                    return mkErr("font.size", "value " + std::to_string(s) +
                        " out of range. Must be 1-200");
            }
        }

        // Position object validation (xypad)
        if (item.contains("position"))
        {
            if (!item["position"].is_object())
                return mkErr("position", "must be an object with x and y (0.0-1.0)");
            const auto &pos = item["position"];
            if (pos.contains("x"))
            {
                double v = pos["x"].get<double>();
                if (v < 0.0 || v > 1.0)
                    return mkErr("position.x", "value out of range. Must be 0.0-1.0");
            }
            if (pos.contains("y"))
            {
                double v = pos["y"].get<double>();
                if (v < 0.0 || v > 1.0)
                    return mkErr("position.y", "value out of range. Must be 0.0-1.0");
            }
        }

        return ""; // all valid
    }

    /**
     * Validate parentID / pageIndex mutual exclusivity for create operations.
     * Frames and SoloFrames accept exactly one of pageIndex or parentID.
     * All other widget types require parentID (pageIndex is not allowed).
     * Returns empty string on success, or a JSON error string on failure.
     */
    inline std::string validateParentOrPage(const Json &item, int widgetType)
    {
        bool hasPage = item.contains("pageIndex");
        bool hasParent = item.contains("parentID");

        if (widgetType == VCType::Frame || widgetType == VCType::SoloFrame)
        {
            if (hasPage && hasParent)
                return Json({{"error", "pageIndex and parentID are mutually exclusive"}}).dump();
            if (!hasPage && !hasParent)
                return Json({{"error", "either pageIndex or parentID is required"}}).dump();
        }
        else
        {
            if (!hasParent)
                return Json({{"error", "parentID is required"}}).dump();
            if (hasPage)
                return Json({{"error", "pageIndex is not valid for widget type '" +
                    VCType::toString(widgetType) + "'"}}).dump();
        }
        return "";
    }

    /**
     * Full validation pipeline: field names + field values.
     * Returns empty string on success, or JSON error string on failure.
     */
    inline std::string validate(const Json &item, int widgetType, bool isCreate)
    {
        auto err = validateFieldsForType(item, widgetType, isCreate);
        if (!err.empty()) return err;

        err = validateFieldValues(item, widgetType);
        if (!err.empty()) return err;

        if (isCreate)
        {
            err = validateParentOrPage(item, widgetType);
            if (!err.empty()) return err;
        }

        return "";
    }
}

#endif // VC_TOOLS_COMMON_H
