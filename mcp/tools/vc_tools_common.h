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
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <nlohmann/json.hpp>

#include "tool_registry.h"
#include "vcbridge.h"

#include <limits>
#include <string>
#include <vector>
#include <set>
#include <initializer_list>
#include <type_traits>

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
        Clock            = 11,
        RecordPanel      = 12
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
            {"clock",        Clock},
            {"recordPanel",  RecordPanel}
        };
        auto it = map.find(name);
        return (it != map.end()) ? it.value() : Unknown;
    }

    /** Map enum → stable JSON machine tag (bridge type field and error messages). */
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
            {Clock,         "clock"},
            {RecordPanel,   "recordPanel"}
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
                "volumeLevel", "barsNumber"
            }},
            {Animation, {
                "color1", "color2", "color3", "color4", "color5",
                "colors", "animation", "instantApply", "visibilityMask"
            }},
            {Clock, {
                "clockType",
                "countdownHours", "countdownMinutes", "countdownSeconds",
                "schedules"
            }},
            {RecordPanel, {
                "targetFolder", "scenePrefix", "chaserPrefix",
                "defaultFadeIn", "defaultHold", "defaultFadeOut"
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
                "multipageMode", "totalPages", "pagesLoop",
                "pageLabels", "headerVisible", "enableButtonVisible", "collapsed"
            }},
            {SoloFrame, {
                "multipageMode", "totalPages", "pagesLoop",
                "pageLabels", "headerVisible", "enableButtonVisible", "collapsed",
                "soloframeMixing", "excludeMonitoredFunctions"
            }},
            {XYPad, {
                "displayMode", "invertedAppearance", "presets"
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
                "barsNumber", "bars"
            }},
            {Animation, {
                "functionID", "functionName",
                "color1", "color2", "color3", "color4", "color5",
                "colors", "animation", "instantApply",
                "visibilityMask"
            }},
            {Clock, {
                "clockType",
                "countdownHours", "countdownMinutes", "countdownSeconds",
                "schedules"
            }},
            {RecordPanel, {
                "targetFolder", "scenePrefix", "chaserPrefix",
                "defaultFadeIn", "defaultHold", "defaultFadeOut"
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

    /** validateEnums accepts any letter case; return the schema spelling the
     *  handler compares against, or the value unchanged if nothing matches. */
    inline std::string canonicalEnum(const std::string &value, const Json &allowed)
    {
        const QString needle = QString::fromStdString(value);
        for (const auto &v : allowed)
            if (v.is_string() && needle.compare(QString::fromStdString(v.get<std::string>()),
                                                Qt::CaseInsensitive) == 0)
                return v.get<std::string>();
        return value;
    }

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
        if (isCreate && widgetType != VCType::Frame && widgetType != VCType::SoloFrame)
            allowed.insert("childPageIndex");

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
                    "cuelist, label, speedDial, audioTrigger, matrix, clock, recordPanel");
        }

        // action (button)
        if (item.contains("action"))
        {
            auto v = item["action"].get<std::string>();
            if (!isValidEnum(v, {"toggle", "flash", "blackout", "stopall", "freeze", "freezehold"}))
                return mkErr("action", "invalid value '" + v +
                    "'. Must be one of: toggle, flash, blackout, stopall, freeze, freezehold");
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
        if (item.contains("childPageIndex"))
        {
            if (!mcp::jsonInteger(item["childPageIndex"], 0, std::numeric_limits<int>::max()))
                return mkErr("childPageIndex",
                             mcp::integerError("childPageIndex", 0, std::numeric_limits<int>::max()));
        }

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

        // Dynamic colors array validation
        if (item.contains("colors"))
        {
            if (!item["colors"].is_array())
                return mkErr("colors", "must be an array of hex color strings");
            if (item["colors"].size() > 5)
                return mkErr("colors", "maximum 5 colors allowed");
            for (size_t ci = 0; ci < item["colors"].size(); ci++)
            {
                if (!item["colors"][ci].is_string())
                    return mkErr("colors[" + std::to_string(ci) + "]", "must be a string");
                auto v = item["colors"][ci].get<std::string>();
                if (!isValidHexColor(v))
                    return mkErr("colors[" + std::to_string(ci) + "]",
                        "invalid hex color '" + v + "'. Must be format #rrggbb");
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

        for (const char *field : {"parentID", "pageIndex"})
        {
            if (item.contains(field) &&
                !mcp::jsonInteger(item[field], 0, std::numeric_limits<int>::max()))
                return Json({{"error", mcp::integerError(field, 0, std::numeric_limits<int>::max())}}).dump();
        }

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

class Doc;
class VCBridge;

namespace VCAuthoring
{
    using Json = nlohmann::json;
    constexpr int64_t kMaxToolPayloadBytes = 262144;

    inline Json requestError(const std::string &message,
                             const std::string &code = "invalidArgument")
    {
        return Json{
            {"version", 1},
            {"code", code},
            {"error", message},
            {"limitations", Json::array()}
        };
    }

    inline Json toJson(const VCBridge::AuthoringSourceKey &key)
    {
        Json out = Json::object();
        if (key.controlSlotID.has_value()) out["controlSlotID"] = int(*key.controlSlotID);
        if (key.inputUniverseID.has_value()) out["inputUniverseID"] = *key.inputUniverseID;
        if (key.inputChannel.has_value()) out["inputChannel"] = *key.inputChannel;
        if (key.childSubpage.has_value()) out["childSubpage"] = *key.childSubpage;
        return out;
    }

    inline Json toJson(const VCBridge::AuthoringLimitation &lim)
    {
        Json out = Json::object();
        if (!lim.code.isEmpty()) out["code"] = lim.code.toStdString();
        if (!lim.scope.isEmpty()) out["scope"] = lim.scope.toStdString();
        if (!lim.message.isEmpty()) out["message"] = lim.message.toStdString();
        return out;
    }

    inline Json toJson(const QList<VCBridge::AuthoringLimitation> &limits)
    {
        Json out = Json::array();
        for (const VCBridge::AuthoringLimitation &lim : limits)
            out.push_back(toJson(lim));
        return out;
    }

    inline Json qjsonToJson(const QJsonValue &value)
    {
        switch (value.type())
        {
        case QJsonValue::Null:
        case QJsonValue::Undefined:
            return nullptr;
        case QJsonValue::Bool:
            return value.toBool();
        case QJsonValue::Double:
            return value.toDouble();
        case QJsonValue::String:
            return value.toString().toStdString();
        case QJsonValue::Array:
        {
            Json arr = Json::array();
            const QJsonArray in = value.toArray();
            for (const QJsonValue &entry : in)
                arr.push_back(qjsonToJson(entry));
            return arr;
        }
        case QJsonValue::Object:
        {
            Json obj = Json::object();
            const QJsonObject in = value.toObject();
            for (auto it = in.constBegin(); it != in.constEnd(); ++it)
                obj[it.key().toStdString()] = qjsonToJson(it.value());
            return obj;
        }
        }
        return nullptr;
    }

    inline Json variantToJson(const QVariant &value)
    {
        if (!value.isValid())
            return nullptr;

        if (value.canConvert<QRectF>())
        {
            const QRectF rect = value.toRectF();
            return Json{{"x", rect.x()}, {"y", rect.y()},
                        {"width", rect.width()}, {"height", rect.height()}};
        }

        const QJsonValue jv = QJsonValue::fromVariant(value);
        return qjsonToJson(jv);
    }

    inline Json toJson(const VCBridge::AuthoringFieldChange &change)
    {
        Json out = {
            {"field", change.field.toStdString()},
            {"changed", change.changed},
            {"before", variantToJson(change.before)},
            {"after", variantToJson(change.after)}
        };
        if (!change.note.isEmpty())
            out["note"] = change.note.toStdString();
        return out;
    }

    inline Json toJson(const QList<VCBridge::AuthoringFieldChange> &changes)
    {
        Json out = Json::array();
        for (const VCBridge::AuthoringFieldChange &change : changes)
            out.push_back(toJson(change));
        return out;
    }

    inline Json toJson(const VCBridge::AuthoringWorkSummary &work)
    {
        return Json{
            {"scannedSummaryRows", work.scannedSummaryRows},
            {"hydratedRows", work.hydratedRows},
            {"serializedRows", work.serializedRows},
            {"payloadBytes", work.payloadBytes}
        };
    }

    inline const char *statusToString(VCBridge::AuthoringPreparationItem::Status status)
    {
        return status == VCBridge::AuthoringPreparationItem::Status::Ok ? "ok" : "error";
    }

    inline const char *outcomeToString(VCBridge::AuthoringPreparationItem::Outcome outcome)
    {
        switch (outcome)
        {
        case VCBridge::AuthoringPreparationItem::Outcome::Prepared: return "prepared";
        case VCBridge::AuthoringPreparationItem::Outcome::Unchanged: return "unchanged";
        case VCBridge::AuthoringPreparationItem::Outcome::Refused: return "refused";
        case VCBridge::AuthoringPreparationItem::Outcome::Unsupported: return "unsupported";
        }
        return "unsupported";
    }

    inline const char *outcomeToString(VCBridge::AuthoringApplyItem::Outcome outcome)
    {
        switch (outcome)
        {
        case VCBridge::AuthoringApplyItem::Outcome::Applied: return "applied";
        case VCBridge::AuthoringApplyItem::Outcome::Unchanged: return "unchanged";
        case VCBridge::AuthoringApplyItem::Outcome::Refused: return "refused";
        case VCBridge::AuthoringApplyItem::Outcome::Partial: return "partial";
        case VCBridge::AuthoringApplyItem::Outcome::NotApplied: return "notApplied";
        }
        return "notApplied";
    }

    inline Json toJson(const VCBridge::AuthoringReadRow &typedRow)
    {
        Json row = Json::object();
        std::visit([&](const auto &value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, VCBridge::AuthoringReadRow::SetupPageRow>)
            {
                row["kind"] = "setupPage";
                row["pageWidgetID"] = value.pageWidgetID;
                row["pageIndex"] = value.pageIndex;
                row["caption"] = value.caption.toStdString();
                row["externalInputMode"] = value.externalInputMode.toStdString();
                if (value.configurationDigest.has_value())
                    row["configurationDigest"] = value.configurationDigest->toStdString();
            }
            else if constexpr (std::is_same_v<T, VCBridge::AuthoringReadRow::SetupSubpageRow>)
            {
                row["kind"] = "setupSubpage";
                row["frameWidgetID"] = value.frameWidgetID;
                row["childSubpage"] = value.childSubpage;
                row["label"] = value.label.toStdString();
                if (value.configurationDigest.has_value())
                    row["configurationDigest"] = value.configurationDigest->toStdString();
            }
            else if constexpr (std::is_same_v<T, VCBridge::AuthoringReadRow::SetupWidgetRow>)
            {
                row["kind"] = "setupWidget";
                row["widgetID"] = value.widgetID;
                row["pageWidgetID"] = value.pageWidgetID;
                row["parentWidgetID"] = value.parentWidgetID;
                row["childSubpage"] = value.childSubpage;
                row["siblingIndex"] = value.siblingIndex;
                row["machineType"] = value.machineType.toStdString();
                row["caption"] = value.caption.toStdString();
                if (value.logicalGeometry.has_value())
                {
                    row["logicalGeometry"] = {
                        {"x", value.logicalGeometry->x()},
                        {"y", value.logicalGeometry->y()},
                        {"width", value.logicalGeometry->width()},
                        {"height", value.logicalGeometry->height()}
                    };
                }

                if (value.functionBinding.has_value())
                {
                    Json group = Json::object();
                    if (value.functionBinding->functionID.has_value())
                        group["functionID"] = *value.functionBinding->functionID;
                    if (value.functionBinding->action.has_value())
                        group["action"] = value.functionBinding->action->toStdString();
                    row["functionBinding"] = group;
                }
                if (value.inputBindings.has_value())
                {
                    Json entries = Json::array();
                    for (const auto &entry : value.inputBindings->entries)
                    {
                        Json out = {
                            {"sourceKey", toJson(entry.sourceKey)},
                            {"duplicate", entry.duplicate},
                            {"ambiguous", entry.ambiguous}
                        };
                        if (entry.sourceOrdinal.has_value()) out["sourceOrdinal"] = *entry.sourceOrdinal;
                        if (entry.sourceName.has_value()) out["sourceName"] = entry.sourceName->toStdString();
                        if (entry.configurationDigest.has_value()) out["configurationDigest"] = entry.configurationDigest->toStdString();
                        entries.push_back(out);
                    }
                    row["inputBindings"] = entries;
                }
                if (value.feedback.has_value())
                {
                    Json entries = Json::array();
                    for (const auto &entry : value.feedback->entries)
                    {
                        Json out = {{"sourceKey", toJson(entry.sourceKey)}};
                        if (entry.sourceOrdinal.has_value()) out["sourceOrdinal"] = *entry.sourceOrdinal;
                        if (entry.idleValue.has_value()) out["idleValue"] = *entry.idleValue;
                        if (entry.activeValue.has_value()) out["activeValue"] = *entry.activeValue;
                        if (entry.monitorValue.has_value()) out["monitorValue"] = *entry.monitorValue;
                        if (entry.idleChannel.has_value()) out["idleChannel"] = *entry.idleChannel;
                        if (entry.activeChannel.has_value()) out["activeChannel"] = *entry.activeChannel;
                        if (entry.monitorChannel.has_value()) out["monitorChannel"] = *entry.monitorChannel;
                        if (entry.configurationDigest.has_value()) out["configurationDigest"] = entry.configurationDigest->toStdString();
                        entries.push_back(out);
                    }
                    row["feedback"] = entries;
                }
                if (value.recordPanel.has_value())
                {
                    Json group = Json::object();
                    if (value.recordPanel->targetFolder.has_value()) group["targetFolder"] = value.recordPanel->targetFolder->toStdString();
                    if (value.recordPanel->scenePrefix.has_value()) group["scenePrefix"] = value.recordPanel->scenePrefix->toStdString();
                    if (value.recordPanel->chaserPrefix.has_value()) group["chaserPrefix"] = value.recordPanel->chaserPrefix->toStdString();
                    if (value.recordPanel->defaultFadeIn.has_value()) group["defaultFadeIn"] = *value.recordPanel->defaultFadeIn;
                    if (value.recordPanel->defaultHold.has_value()) group["defaultHold"] = *value.recordPanel->defaultHold;
                    if (value.recordPanel->defaultFadeOut.has_value()) group["defaultFadeOut"] = *value.recordPanel->defaultFadeOut;
                    row["recordPanel"] = group;
                }
                if (value.appearance.has_value())
                {
                    Json group = Json::object();
                    if (value.appearance->bgColor.has_value()) group["bgColor"] = value.appearance->bgColor->name().toStdString();
                    if (value.appearance->fgColor.has_value()) group["fgColor"] = value.appearance->fgColor->name().toStdString();
                    if (value.appearance->disabled.has_value()) group["disabled"] = *value.appearance->disabled;
                    row["appearance"] = group;
                }
                if (value.frameConfiguration.has_value())
                {
                    Json group = Json::object();
                    if (value.frameConfiguration->totalPages.has_value()) group["totalPages"] = *value.frameConfiguration->totalPages;
                    if (value.frameConfiguration->currentPage.has_value()) group["currentPage"] = *value.frameConfiguration->currentPage;
                    if (value.frameConfiguration->pagesLoop.has_value()) group["pagesLoop"] = *value.frameConfiguration->pagesLoop;
                    if (value.frameConfiguration->pageLabels.has_value())
                    {
                        Json labels = Json::array();
                        for (const QString &label : value.frameConfiguration->pageLabels.value())
                            labels.push_back(label.toStdString());
                        group["pageLabels"] = labels;
                    }
                    row["frameConfiguration"] = group;
                }
                if (value.configurationDigest.has_value())
                    row["configurationDigest"] = value.configurationDigest->toStdString();
            }
            else if constexpr (std::is_same_v<T, VCBridge::AuthoringReadRow::TargetRow>)
            {
                row["kind"] = "target";
                row["widgetID"] = value.widgetID;
                row["controlSlotID"] = value.controlSlotID;
                row["label"] = value.label.toStdString();
                row["sourceName"] = value.sourceName.toStdString();
                if (value.validationLevel.has_value()) row["validationLevel"] = value.validationLevel->toStdString();
                if (value.assignmentSupported.has_value()) row["assignmentSupported"] = *value.assignmentSupported;
                if (value.valueDomain.has_value()) row["valueDomain"] = value.valueDomain->toStdString();
                if (value.keyboardAllowed.has_value()) row["keyboardAllowed"] = *value.keyboardAllowed;
                if (value.inputBindings.has_value())
                {
                    Json entries = Json::array();
                    for (const auto &entry : value.inputBindings->entries)
                    {
                        Json out = {
                            {"sourceKey", toJson(entry.sourceKey)},
                            {"duplicate", entry.duplicate},
                            {"ambiguous", entry.ambiguous}
                        };
                        if (entry.sourceOrdinal.has_value()) out["sourceOrdinal"] = *entry.sourceOrdinal;
                        if (entry.sourceName.has_value()) out["sourceName"] = entry.sourceName->toStdString();
                        if (entry.configurationDigest.has_value()) out["configurationDigest"] = entry.configurationDigest->toStdString();
                        entries.push_back(out);
                    }
                    row["inputBindings"] = entries;
                }
                if (value.feedback.has_value())
                {
                    Json entries = Json::array();
                    for (const auto &entry : value.feedback->entries)
                    {
                        Json out = {{"sourceKey", toJson(entry.sourceKey)}};
                        if (entry.sourceOrdinal.has_value()) out["sourceOrdinal"] = *entry.sourceOrdinal;
                        if (entry.idleValue.has_value()) out["idleValue"] = *entry.idleValue;
                        if (entry.activeValue.has_value()) out["activeValue"] = *entry.activeValue;
                        if (entry.monitorValue.has_value()) out["monitorValue"] = *entry.monitorValue;
                        if (entry.idleChannel.has_value()) out["idleChannel"] = *entry.idleChannel;
                        if (entry.activeChannel.has_value()) out["activeChannel"] = *entry.activeChannel;
                        if (entry.monitorChannel.has_value()) out["monitorChannel"] = *entry.monitorChannel;
                        if (entry.configurationDigest.has_value())
                            out["configurationDigest"] = entry.configurationDigest->toStdString();
                        entries.push_back(out);
                    }
                    row["feedback"] = entries;
                }
                if (value.configurationDigest.has_value()) row["configurationDigest"] = value.configurationDigest->toStdString();
            }
            else if constexpr (std::is_same_v<T, VCBridge::AuthoringReadRow::SourceRow>)
            {
                row["kind"] = "source";
                row["pluginName"] = value.pluginName.toStdString();
                row["direction"] = value.direction.toStdString();
                if (value.inputLine.has_value()) row["inputLine"] = *value.inputLine;
                if (value.outputLine.has_value()) row["outputLine"] = *value.outputLine;
                if (value.inputUID.has_value()) row["inputUID"] = value.inputUID->toStdString();
                if (value.outputUID.has_value()) row["outputUID"] = value.outputUID->toStdString();
                if (value.inputUniverseID.has_value()) row["inputUniverseID"] = *value.inputUniverseID;
                if (value.status.has_value()) row["status"] = value.status->toStdString();
                if (value.reason.has_value()) row["reason"] = value.reason->toStdString();
                if (value.sourceOrdinal.has_value()) row["sourceOrdinal"] = *value.sourceOrdinal;
                if (value.configurationDigest.has_value()) row["configurationDigest"] = value.configurationDigest->toStdString();
            }
            else if constexpr (std::is_same_v<T, VCBridge::AuthoringReadRow::ProfileRow>)
            {
                row["kind"] = "profile";
                row["profileName"] = value.profileName.toStdString();
                row["manufacturer"] = value.manufacturer.toStdString();
                row["model"] = value.model.toStdString();
                row["type"] = value.type.toStdString();
                if (value.configurationDigest.has_value()) row["configurationDigest"] = value.configurationDigest->toStdString();
            }
            else if constexpr (std::is_same_v<T, VCBridge::AuthoringReadRow::ProfileChannelRow>)
            {
                row["kind"] = "profileChannel";
                row["profileName"] = value.profileName.toStdString();
                row["profileChannelNumber"] = value.profileChannelNumber;
                row["inputChannel"] = value.inputChannel;
                row["name"] = value.name.toStdString();
                row["type"] = value.type.toStdString();
                row["movement"] = value.movement == VCBridge::AuthoringReadRow::ProfileChannelRow::Movement::Relative
                    ? "relative" : "absolute";
                row["sensitivity"] = value.sensitivity;
                if (value.configurationDigest.has_value()) row["configurationDigest"] = value.configurationDigest->toStdString();
            }
            else if constexpr (std::is_same_v<T, VCBridge::AuthoringReadRow::ProfileFeedbackTableRow>)
            {
                row["kind"] = "profileFeedbackTable";
                row["profileName"] = value.profileName.toStdString();
                row["table"] = value.table.toStdString();
                row["key"] = value.key;
                if (value.label.has_value()) row["label"] = value.label->toStdString();
                if (value.color.has_value()) row["color"] = value.color->toStdString();
                if (value.configurationDigest.has_value()) row["configurationDigest"] = value.configurationDigest->toStdString();
            }
            else if constexpr (std::is_same_v<T, VCBridge::AuthoringReadRow::RecordingReferenceRow>)
            {
                row["kind"] = "recordingReference";
                row["showFunctionID"] = value.showFunctionID;
                row["controlRecordingUUID"] = value.controlRecordingUUID.toStdString();
                row["role"] = value.role.toStdString();
                row["attribute"] = value.attribute.toStdString();
                if (value.status.has_value()) row["status"] = value.status->toStdString();
                if (value.reason.has_value()) row["reason"] = value.reason->toStdString();
                row["count"] = value.count;
            }
            else if constexpr (std::is_same_v<T, VCBridge::AuthoringReadRow::RecordingCommandRow>)
            {
                row["kind"] = "recordingCommand";
                row["showFunctionID"] = value.showFunctionID;
                row["eventID"] = value.eventID;
                if (value.timeMs.has_value()) row["timeMs"] = *value.timeMs;
                if (value.order.has_value()) row["order"] = *value.order;
                if (value.pairID.has_value()) row["pairID"] = *value.pairID;
                if (value.action.has_value()) row["action"] = value.action->toStdString();
                row["controlRecordingUUID"] = value.controlRecordingUUID.toStdString();
                if (value.role.has_value()) row["role"] = value.role->toStdString();
                if (value.attribute.has_value()) row["attribute"] = value.attribute->toStdString();
                if (value.payload.has_value()) row["payload"] = variantToJson(*value.payload);
            }
        }, typedRow.row);

        if (!typedRow.limitations.isEmpty())
            row["limitations"] = toJson(typedRow.limitations);
        return row;
    }

    inline Json toJson(const VCBridge::AuthoringReadResult &result)
    {
        Json out = {
            {"version", result.version},
            {"basis", result.basis.toStdString()},
            {"basisScope", result.basisScope.toStdString()},
            {"complete", result.complete},
            {"nextCursor", result.nextCursor.toStdString()},
            {"limitations", toJson(result.limitations)},
            {"work", toJson(result.work)}
        };
        if (!result.code.isEmpty()) out["code"] = result.code.toStdString();
        if (!result.error.isEmpty()) out["error"] = result.error.toStdString();

        Json rows = Json::array();
        for (const VCBridge::AuthoringReadRow &row : result.rows)
            rows.push_back(toJson(row));
        out["rows"] = rows;
        return out;
    }

    inline const char *atomicScopeToString(VCBridge::AuthoringPreparationResult::AtomicScope scope)
    {
        return scope == VCBridge::AuthoringPreparationResult::AtomicScope::IndependentItems
            ? "independentItems" : "nativeGroup";
    }

    inline const char *diffDetailToString(VCBridge::AuthoringPreparationResult::DiffDetail detail)
    {
        return detail == VCBridge::AuthoringPreparationResult::DiffDetail::Full ? "full" : "summary";
    }

    inline Json toJson(const VCBridge::AuthoringPreparationResult &result)
    {
        Json out = {
            {"version", result.version},
            {"basis", result.basis.toStdString()},
            {"preparationToken", result.preparationToken.toStdString()},
            {"expiresInMs", result.expiresInMs},
            {"atomicScope", atomicScopeToString(result.atomicScope)},
            {"diffDetail", diffDetailToString(result.diffDetail)},
            {"candidateDigest", result.candidateDigest.toStdString()},
            {"diffComplete", result.diffComplete},
            {"limitations", toJson(result.limitations)},
            {"work", toJson(result.work)}
        };
        if (!result.code.isEmpty()) out["code"] = result.code.toStdString();
        if (!result.error.isEmpty()) out["error"] = result.error.toStdString();

        Json preconditions = Json::array();
        for (const VCBridge::AuthoringPreconditionObservation &obs : result.preconditions)
        {
            Json entry = {{"name", obs.name.toStdString()}, {"met", obs.met},
                          {"scope", obs.scope.toStdString()}, {"reason", obs.reason.toStdString()}};
            preconditions.push_back(entry);
        }
        out["preconditions"] = preconditions;

        Json items = Json::array();
        for (const VCBridge::AuthoringPreparationItem &item : result.items)
        {
            Json entry = {
                {"index", item.index},
                {"status", statusToString(item.status)},
                {"outcome", outcomeToString(item.outcome)},
                {"targetWidgetID", item.targetWidgetID},
                {"changedCount", item.changedCount},
                {"beforeLogicalRect", {{"x", item.beforeLogicalRect.x()}, {"y", item.beforeLogicalRect.y()},
                                       {"width", item.beforeLogicalRect.width()}, {"height", item.beforeLogicalRect.height()}}},
                {"afterLogicalRect", {{"x", item.afterLogicalRect.x()}, {"y", item.afterLogicalRect.y()},
                                      {"width", item.afterLogicalRect.width()}, {"height", item.afterLogicalRect.height()}}},
                {"changes", toJson(item.changes)}
            };
            if (!item.code.isEmpty()) entry["code"] = item.code.toStdString();
            if (!item.error.isEmpty()) entry["error"] = item.error.toStdString();
            if (item.selectedPageWidgetID.has_value()) entry["selectedPageWidgetID"] = *item.selectedPageWidgetID;
            if (item.selectedParentWidgetID.has_value()) entry["selectedParentWidgetID"] = *item.selectedParentWidgetID;
            if (item.selectedChildSubpage.has_value()) entry["selectedChildSubpage"] = *item.selectedChildSubpage;
            if (item.selectedControlSlotID.has_value()) entry["selectedControlSlotID"] = *item.selectedControlSlotID;
            if (item.selectedFunctionID.has_value()) entry["selectedFunctionID"] = *item.selectedFunctionID;
            if (item.selectedFunctionBeforeID.has_value()) entry["selectedFunctionBeforeID"] = *item.selectedFunctionBeforeID;
            if (item.selectedFunctionAfterID.has_value()) entry["selectedFunctionAfterID"] = *item.selectedFunctionAfterID;
            if (item.selectedSourceKey.has_value()) entry["selectedSourceKey"] = toJson(*item.selectedSourceKey);
            if (item.sourceOrdinal.has_value()) entry["sourceOrdinal"] = *item.sourceOrdinal;
            if (item.configurationDigest.has_value()) entry["configurationDigest"] = item.configurationDigest->toStdString();
            if (!item.limitations.isEmpty()) entry["limitations"] = toJson(item.limitations);
            items.push_back(entry);
        }
        out["items"] = items;
        return out;
    }

    inline Json toJson(const VCBridge::AuthoringApplyResult &result)
    {
        Json out = {
            {"version", result.version},
            {"applied", result.applied},
            {"tokenConsumed", result.tokenConsumed},
            {"atomicScope", atomicScopeToString(result.atomicScope)},
            {"limitations", toJson(result.limitations)},
            {"work", toJson(result.work)}
        };
        if (!result.code.isEmpty()) out["code"] = result.code.toStdString();
        if (!result.error.isEmpty()) out["error"] = result.error.toStdString();

        Json items = Json::array();
        for (const VCBridge::AuthoringApplyItem &item : result.items)
        {
            Json entry = {
                {"index", item.index},
                {"status", item.status == VCBridge::AuthoringApplyItem::Status::Ok ? "ok" : "error"},
                {"outcome", outcomeToString(item.outcome)},
                {"targetWidgetID", item.targetWidgetID},
                {"changedCount", item.changedCount},
                {"beforeLogicalRect", {{"x", item.beforeLogicalRect.x()}, {"y", item.beforeLogicalRect.y()},
                                       {"width", item.beforeLogicalRect.width()}, {"height", item.beforeLogicalRect.height()}}},
                {"afterLogicalRect", {{"x", item.afterLogicalRect.x()}, {"y", item.afterLogicalRect.y()},
                                      {"width", item.afterLogicalRect.width()}, {"height", item.afterLogicalRect.height()}}},
                {"changes", toJson(item.changes)}
            };
            if (!item.code.isEmpty()) entry["code"] = item.code.toStdString();
            if (!item.error.isEmpty()) entry["error"] = item.error.toStdString();
            if (item.selectedPageWidgetID.has_value()) entry["selectedPageWidgetID"] = *item.selectedPageWidgetID;
            if (item.selectedParentWidgetID.has_value()) entry["selectedParentWidgetID"] = *item.selectedParentWidgetID;
            if (item.selectedChildSubpage.has_value()) entry["selectedChildSubpage"] = *item.selectedChildSubpage;
            if (item.selectedControlSlotID.has_value()) entry["selectedControlSlotID"] = *item.selectedControlSlotID;
            if (item.selectedFunctionID.has_value()) entry["selectedFunctionID"] = *item.selectedFunctionID;
            if (item.selectedFunctionBeforeID.has_value()) entry["selectedFunctionBeforeID"] = *item.selectedFunctionBeforeID;
            if (item.selectedFunctionAfterID.has_value()) entry["selectedFunctionAfterID"] = *item.selectedFunctionAfterID;
            if (item.selectedSourceKey.has_value()) entry["selectedSourceKey"] = toJson(*item.selectedSourceKey);
            if (item.sourceOrdinal.has_value()) entry["sourceOrdinal"] = *item.sourceOrdinal;
            if (item.configurationDigest.has_value()) entry["configurationDigest"] = item.configurationDigest->toStdString();
            if (!item.limitations.isEmpty()) entry["limitations"] = toJson(item.limitations);
            items.push_back(entry);
        }
        out["items"] = items;
        return out;
    }
}

namespace VCRefs
{
    /** Newly supplied function, fixture and widget references must exist and suit the
     *  widget; -1 unbinds a single binding. Defined in vc_update_tools.cpp. */
    std::string check(const nlohmann::json &item, int widgetType, Doc *doc, const VCBridge &bridge);
}

#endif // VC_TOOLS_COMMON_H
