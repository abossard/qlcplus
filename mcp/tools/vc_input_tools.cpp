/*
  Q Light Controller Plus
  vc_input_tools.cpp

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
#include "vc_tools_common.h"
#include "doc.h"

#include <QKeySequence>
#include <limits>
#include <optional>
#include <set>

#include <fastmcpp/tools/manager.hpp>
#include <fastmcpp/tools/tool.hpp>

namespace {

using Json = nlohmann::json;

constexpr int64_t kControlSlotMax = 255;
constexpr int64_t kPackedInputChannelMax = 65535;
constexpr int64_t kMaxCursorBytes = 1024;
constexpr int64_t kChildSubpageMax = 65535;

Json requestError(const std::string &message, const std::string &code = "invalidArgument")
{
    return Json{
        {"version", 1},
        {"code", code},
        {"error", message},
        {"limitations", Json::array()}
    };
}

Json requestErrorFromDump(const std::string &dumpedError, const std::string &code = "invalidArgument")
{
    const Json parsed = Json::parse(dumpedError, nullptr, false);
    if (parsed.is_object() && parsed.contains("error") && parsed.at("error").is_string())
        return requestError(parsed.at("error").get<std::string>(), code);
    return requestError(dumpedError, code);
}

std::optional<std::string> parseFieldGroups(const Json &args,
                                            const std::set<std::string> &allowed,
                                            QStringList &groups)
{
    if (!args.contains("fields"))
        return std::nullopt;
    if (!args.at("fields").is_array())
        return std::string("fields must be an array of strings");

    std::set<std::string> seen;
    for (const Json &entry : args.at("fields"))
    {
        if (!entry.is_string())
            return std::string("fields must be an array of strings");
        const std::string field = entry.get<std::string>();
        if (allowed.find(field) == allowed.end())
            return std::string("fields contains unknown group '") + field + "'";
        if (seen.insert(field).second)
            groups.append(QString::fromStdString(field));
    }
    return std::nullopt;
}

std::optional<std::string> parseWidgetIds(const Json &value, QList<quint32> &out)
{
    if (!value.is_array())
        return std::string("widgetIDs must be an array of integers");
    for (const Json &entry : value)
    {
        auto id = mcp::jsonInteger(entry, 0, mcp::kMaxId);
        if (!id)
            return mcp::integerError("widgetIDs[]", 0, mcp::kMaxId);
        out.append(quint32(*id));
    }
    return std::nullopt;
}

std::optional<std::string> parseSourceKey(const Json &value, VCBridge::AuthoringSourceKey &out)
{
    if (!value.is_object())
        return std::string("sourceKey must be an object");
    auto fieldErr = validateFields(value, {"controlSlotID", "inputUniverseID", "inputChannel", "childSubpage"});
    if (!fieldErr.empty())
        return Json::parse(fieldErr).value("error", fieldErr);

    if (!value.contains("controlSlotID") || !value.contains("inputUniverseID") ||
        !value.contains("inputChannel") || !value.contains("childSubpage"))
        return std::string("Source key requires controlSlotID, inputUniverseID, inputChannel and childSubpage");

    const auto slot = mcp::jsonInteger(value.at("controlSlotID"), 0, kControlSlotMax);
    if (!slot) return mcp::integerError("sourceKey.controlSlotID", 0, kControlSlotMax);
    const auto universe = mcp::jsonInteger(value.at("inputUniverseID"), 0, mcp::kMaxId);
    if (!universe) return mcp::integerError("sourceKey.inputUniverseID", 0, mcp::kMaxId);
    const auto channel = mcp::jsonInteger(value.at("inputChannel"), 0, kPackedInputChannelMax);
    if (!channel) return mcp::integerError("sourceKey.inputChannel", 0, kPackedInputChannelMax);
    const auto childSubpage = mcp::jsonInteger(value.at("childSubpage"), 0, kChildSubpageMax);
    if (!childSubpage) return mcp::integerError("sourceKey.childSubpage", 0, kChildSubpageMax);

    out.controlSlotID = quint8(*slot);
    out.inputUniverseID = quint32(*universe);
    out.inputChannel = quint32(*channel);
    out.childSubpage = int(*childSubpage);
    return std::nullopt;
}

std::optional<std::string> parseDestination(const Json &value, VCBridge::AuthoringDestination &out)
{
    if (!value.is_object())
        return std::string("destination must be an object");
    auto fieldErr = validateFields(value, {"inputUniverseID", "inputChannel", "profileName"});
    if (!fieldErr.empty())
        return Json::parse(fieldErr).value("error", fieldErr);
    if (!value.contains("inputUniverseID"))
        return std::string("destination.inputUniverseID is required");
    if (!value.contains("inputChannel"))
        return std::string("destination.inputChannel is required");
    if (!value.contains("profileName"))
        return std::string("destination.profileName is required");
    if (!value.at("profileName").is_string() || value.at("profileName").get<std::string>().empty())
        return std::string("destination.profileName must be a non-empty string");

    const auto universe = mcp::jsonInteger(value.at("inputUniverseID"), 0, mcp::kMaxId);
    if (!universe) return mcp::integerError("destination.inputUniverseID", 0, mcp::kMaxId);
    const auto channel = mcp::jsonInteger(value.at("inputChannel"), 0, kPackedInputChannelMax);
    if (!channel) return mcp::integerError("destination.inputChannel", 0, kPackedInputChannelMax);

    out.inputUniverseID = quint32(*universe);
    out.inputChannel = quint32(*channel);
    out.profileName = QString::fromStdString(value.at("profileName").get<std::string>());
    return std::nullopt;
}

}

void registerVCInputTools(fastmcpp::tools::ToolManager &tm, Doc *doc, VCBridge *vcBridge)
{
    using Json = nlohmann::json;
    using Tool = fastmcpp::tools::Tool;

    if (!vcBridge) return;

    static const char *sourceNameDesc =
        "Input source name. Per widget type: "
        "Button: 'default'. "
        "Slider: 'default', 'overrideReset', 'flashButton'. "
        "CueList: 'next', 'previous', 'playback', 'stop', 'sideFader'. "
        "XYPad: 'pan', 'panFine', 'tilt', 'tiltFine', 'width', 'height', 'preset0'..'presetN'. "
        "SpeedDial: 'absolute', 'tap', 'mult', 'div', 'multDivReset', 'apply', "
        "'1_16x','1_8x','1_4x','1_2x','2x','4x','8x','16x', 'preset0'..'presetN'. "
        "Clock: 'play', 'reset'. "
        "Frame/SoloFrame: 'nextPage', 'previousPage', 'enable', 'collapse', 'shortcut0'..'shortcutN'. "
        "AudioTriggers: 'default', 'volumeControl'. "
        "Matrix: 'default'.";

    // vc_query_controls
    tm.register_tool(Tool(
        "vc_query_controls",
        Json{{"type", "object"}, {"properties", {
            {"kind", {{"type", "string"}, {"enum", {"targets", "sources", "profiles"}},
                      {"description", "Query variant. Defaults to targets."}}},
            {"widgetIDs", {{"type", "array"}, {"items", {{"type", "integer"}, {"minimum", 0}, {"maximum", mcp::kMaxId}}},
                           {"description", "Required for kind=targets. Forbidden otherwise."}}},
            {"pluginName", {{"type", "string"},
                            {"description", "Allowed for kind=sources only."}}},
            {"profileName", {{"type", "string"},
                             {"description", "Allowed for kind=profiles only."}}},
            {"inputUniverseID", {{"type", "integer"}, {"minimum", 0}, {"maximum", mcp::kMaxId},
                                 {"description", "Allowed for kind=sources only."}}},
            {"fields", {{"type", "array"}, {"items", {{"type", "string"}}},
                        {"description", "Optional field groups: identity, domains, bindings, feedback, channelMap, feedbackTables, patches, status."}}},
            {"limit", {{"type", "integer"}, {"minimum", 1}, {"maximum", 250},
                       {"description", "Row limit in [1, 250]. Defaults to 100."}}},
            {"cursor", {{"type", "string"},
                        {"description", "Pagination cursor (UTF-8, max 1024 bytes)."}}}
        }}},
        Json{},
        [doc, vcBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto err = validateFields(args, {"kind", "widgetIDs", "pluginName", "profileName",
                "inputUniverseID", "fields", "limit", "cursor"});
            if (!err.empty()) return requestErrorFromDump(err);

            std::string kindValue = "targets";
            if (args.contains("kind"))
            {
                if (!args.at("kind").is_string())
                    return requestError("kind must be one of: targets, sources, profiles");
                kindValue = args.at("kind").get<std::string>();
                if (kindValue != "targets" && kindValue != "sources" && kindValue != "profiles")
                    return requestError("invalid value for 'kind': " + args.at("kind").dump() +
                        ". Allowed: \"targets\", \"sources\", \"profiles\"");
            }

            const auto limit = args.contains("limit")
                ? mcp::jsonInteger(args.at("limit"), 1, 250)
                : std::optional<int64_t>(100);
            if (!limit) return requestError(mcp::integerError("limit", 1, 250));

            VCBridge::AuthoringTargetControlQuery query;
            query.limit = int(*limit);
            if (args.contains("cursor"))
            {
                if (!args.at("cursor").is_string())
                    return requestError("cursor must be a string");
                query.cursor = QString::fromStdString(args.at("cursor").get<std::string>());
                if (query.cursor.toUtf8().size() > kMaxCursorBytes)
                    return requestError("cursor exceeds 1024 bytes");
            }

            static const std::set<std::string> kTargetGroups = {
                "identity", "domains", "bindings", "feedback"
            };
            static const std::set<std::string> kSourceGroups = {
                "identity", "patches", "status"
            };
            static const std::set<std::string> kProfileGroups = {
                "identity", "channelMap", "feedbackTables"
            };

            if (kindValue == "targets")
            {
                query.kind = VCBridge::AuthoringTargetControlQuery::Kind::Targets;
                if (!args.contains("widgetIDs"))
                    return requestError("widgetIDs is required for kind=targets");
                if (args.contains("pluginName"))
                    return requestError("pluginName is not allowed for kind=targets");
                if (args.contains("profileName"))
                    return requestError("profileName is not allowed for kind=targets");
                if (args.contains("inputUniverseID"))
                    return requestError("inputUniverseID is not allowed for kind=targets");
                if (auto fieldsErr = parseFieldGroups(args, kTargetGroups, query.fields))
                    return requestError(*fieldsErr);
                if (auto idsErr = parseWidgetIds(args.at("widgetIDs"), query.widgetIDs))
                    return requestError(*idsErr);
            }
            else if (kindValue == "sources")
            {
                query.kind = VCBridge::AuthoringTargetControlQuery::Kind::Sources;
                if (args.contains("widgetIDs"))
                    return requestError("widgetIDs is not allowed for kind=sources");
                if (args.contains("profileName"))
                    return requestError("profileName is not allowed for kind=sources");
                if (auto fieldsErr = parseFieldGroups(args, kSourceGroups, query.fields))
                    return requestError(*fieldsErr);
                if (args.contains("pluginName") && !args.at("pluginName").is_string())
                    return requestError("pluginName must be a string");
                if (args.contains("pluginName"))
                    query.pluginName = QString::fromStdString(args.at("pluginName").get<std::string>());
                if (args.contains("inputUniverseID"))
                {
                    const auto inputUniverse = mcp::jsonInteger(args.at("inputUniverseID"), 0, mcp::kMaxId);
                    if (!inputUniverse)
                        return requestError(mcp::integerError("inputUniverseID", 0, mcp::kMaxId));
                    query.inputUniverseID = quint32(*inputUniverse);
                }
            }
            else
            {
                query.kind = VCBridge::AuthoringTargetControlQuery::Kind::Profiles;
                if (args.contains("widgetIDs"))
                    return requestError("widgetIDs is not allowed for kind=profiles");
                if (args.contains("pluginName"))
                    return requestError("pluginName is not allowed for kind=profiles");
                if (args.contains("inputUniverseID"))
                    return requestError("inputUniverseID is not allowed for kind=profiles");
                if (auto fieldsErr = parseFieldGroups(args, kProfileGroups, query.fields))
                    return requestError(*fieldsErr);
                if (args.contains("profileName") && !args.at("profileName").is_string())
                    return requestError("profileName must be a string");
                if (args.contains("profileName"))
                    query.profileName = QString::fromStdString(args.at("profileName").get<std::string>());
            }

            return VCAuthoring::toJson(vcBridge->queryAuthoring(VCBridge::AuthoringQuery(query)));
            });
        }
    )
    .set_description("Query authoring control targets, sources or profiles with explicit field groups and pagination.")
    .set_annotations(mcp::kAnnotReadOnly));

    // vc_prepare_bindings
    tm.register_tool(Tool(
        "vc_prepare_bindings",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"widgetID", {{"type", "integer"}, {"minimum", 0}, {"maximum", mcp::kMaxId}}},
                {"kind", {{"type", "string"}, {"enum", {"function", "controller", "key", "feedback"}}}},
                {"action", {{"type", "string"}, {"enum", {"set", "clear", "append", "move", "remove"}}}},
                {"controlSlotID", {{"type", "integer"}, {"minimum", 0}, {"maximum", kControlSlotMax}}},
                {"functionID", {{"type", "integer"}, {"minimum", 0}, {"maximum", mcp::kMaxId}}},
                {"sourceKey", {{"type", "object"}, {"properties", {
                    {"controlSlotID", {{"type", "integer"}, {"minimum", 0}, {"maximum", kControlSlotMax}}},
                    {"inputUniverseID", {{"type", "integer"}, {"minimum", 0}, {"maximum", mcp::kMaxId}}},
                    {"inputChannel", {{"type", "integer"}, {"minimum", 0}, {"maximum", kPackedInputChannelMax}}},
                    {"childSubpage", {{"type", "integer"}, {"minimum", 0}, {"maximum", kChildSubpageMax}}}
                }}}},
                {"destination", {{"type", "object"}, {"properties", {
                    {"inputUniverseID", {{"type", "integer"}, {"minimum", 0}, {"maximum", mcp::kMaxId}}},
                    {"inputChannel", {{"type", "integer"}, {"minimum", 0}, {"maximum", kPackedInputChannelMax}}},
                    {"profileName", {{"type", "string"}}}
                }}}},
                {"keySequence", {{"type", "string"}}},
                {"idleValue", {{"type", "integer"}, {"minimum", 0}, {"maximum", 255}}},
                {"activeValue", {{"type", "integer"}, {"minimum", 0}, {"maximum", 255}}},
                {"monitorValue", {{"type", "integer"}, {"minimum", 0}, {"maximum", 255}}},
                {"idleChannel", {{"type", "integer"}, {"minimum", -1}, {"maximum", 15}}},
                {"activeChannel", {{"type", "integer"}, {"minimum", -1}, {"maximum", 15}}},
                {"monitorChannel", {{"type", "integer"}, {"minimum", -1}, {"maximum", 15}}}
            }}, {"required", {"widgetID", "kind"}}}}, {"minItems", 1}, {"maxItems", VCBridge::kAuthoringMaxItemsPerRequest}}},
            {"expectedBasis", {{"type", "string"}}},
            {"allowFanout", {{"type", "boolean"}}},
            {"requireAtomic", {{"type", "boolean"}}},
            {"diffDetail", {{"type", "string"}, {"enum", {"summary", "full"}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc, vcBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto err = validateFields(args, {"items", "expectedBasis", "allowFanout", "requireAtomic", "diffDetail"});
            if (!err.empty()) return requestErrorFromDump(err);
            if (!args.contains("items") || !args.at("items").is_array())
                return requestError("items must be an array");
            const Json &items = args.at("items");
            if (items.empty() || items.size() > size_t(VCBridge::kAuthoringMaxItemsPerRequest))
                return requestError("items must contain 1..128 entries");

            VCBridge::AuthoringBindingsRequest request;
            request.diffDetail = QStringLiteral("summary");
            if (args.contains("diffDetail"))
            {
                if (!args.at("diffDetail").is_string())
                    return requestError("diffDetail must be one of: summary, full");
                const std::string diffDetail = args.at("diffDetail").get<std::string>();
                if (diffDetail != "summary" && diffDetail != "full")
                    return requestError("invalid value for 'diffDetail': " + args.at("diffDetail").dump() +
                        ". Allowed: \"summary\", \"full\"");
                request.diffDetail = QString::fromStdString(diffDetail);
            }
            if (args.contains("expectedBasis"))
            {
                if (!args.at("expectedBasis").is_string())
                    return requestError("expectedBasis must be a string");
                request.expectedBasis = QString::fromStdString(args.at("expectedBasis").get<std::string>());
            }
            if (args.contains("allowFanout"))
            {
                if (!args.at("allowFanout").is_boolean())
                    return requestError("allowFanout must be a boolean");
                request.allowFanout = args.at("allowFanout").get<bool>();
            }
            if (args.contains("requireAtomic"))
            {
                if (!args.at("requireAtomic").is_boolean())
                    return requestError("requireAtomic must be a boolean");
                request.requireAtomic = args.at("requireAtomic").get<bool>();
            }

            std::set<quint32> widgetSeen;
            for (size_t i = 0; i < items.size(); ++i)
            {
                const Json &item = items[i];
                if (!item.is_object())
                    return requestError("items[] must contain objects");
                err = validateFields(item, {"widgetID", "kind", "action", "controlSlotID", "functionID",
                    "sourceKey", "destination", "keySequence", "idleValue", "activeValue",
                    "monitorValue", "idleChannel", "activeChannel", "monitorChannel"});
                if (!err.empty()) return requestErrorFromDump(err);

                const auto widgetID = item.contains("widgetID")
                    ? mcp::jsonInteger(item.at("widgetID"), 0, mcp::kMaxId)
                    : std::nullopt;
                if (!widgetID) return requestError(mcp::integerError("widgetID", 0, mcp::kMaxId));
                if (!widgetSeen.insert(quint32(*widgetID)).second)
                    return requestError("duplicate widgetID targets are not allowed");

                if (!item.contains("kind") || !item.at("kind").is_string())
                    return requestError("kind must be one of: function, controller, key, feedback");

                VCBridge::AuthoringBindingItem binding;
                binding.widgetID = quint32(*widgetID);
                const std::string kind = item.at("kind").get<std::string>();
                if (kind != "function" && kind != "controller" && kind != "key" && kind != "feedback")
                    return requestError("invalid value for 'kind': " + item.at("kind").dump() +
                        ". Allowed: \"function\", \"controller\", \"key\", \"feedback\"");

                std::optional<std::string> action;
                if (item.contains("action"))
                {
                    if (!item.at("action").is_string())
                        return requestError("action must be one of: set, clear, append, move, remove");
                    const std::string actionValue = item.at("action").get<std::string>();
                    if (actionValue != "set" && actionValue != "clear" && actionValue != "append" &&
                        actionValue != "move" && actionValue != "remove")
                        return requestError("invalid value for 'action': " + item.at("action").dump() +
                            ". Allowed: \"set\", \"clear\", \"append\", \"move\", \"remove\"");
                    action = actionValue;
                }

                if (kind == "function")
                {
                    binding.kind = VCBridge::AuthoringBindingItem::Kind::Function;
                    const std::string resolvedAction = action.value_or("set");
                    if (resolvedAction == "set")
                    {
                        binding.action = VCBridge::AuthoringBindingItem::Action::Set;
                        if (!item.contains("functionID"))
                            return requestError("functionID is required for function set");
                        const auto functionID = mcp::jsonInteger(item.at("functionID"), 0, mcp::kMaxId);
                        if (!functionID) return requestError(mcp::integerError("functionID", 0, mcp::kMaxId));
                        binding.functionID = quint32(*functionID);
                    }
                    else if (resolvedAction == "clear")
                    {
                        binding.action = VCBridge::AuthoringBindingItem::Action::Clear;
                        if (item.contains("functionID"))
                            return requestError("functionID is not allowed for function clear");
                    }
                    else
                        return requestError("Function action is not supported");
                    for (const char *field : {"controlSlotID", "sourceKey", "destination", "keySequence",
                                              "idleValue", "activeValue", "monitorValue",
                                              "idleChannel", "activeChannel", "monitorChannel"})
                        if (item.contains(field))
                            return requestError(std::string(field) + " is not allowed for function bindings");
                }
                else if (kind == "controller")
                {
                    binding.kind = VCBridge::AuthoringBindingItem::Kind::Controller;
                    if (!action.has_value())
                        return requestError("action is required for controller bindings");
                    if (!item.contains("controlSlotID"))
                        return requestError("controlSlotID is required for controller bindings");
                    const auto slot = mcp::jsonInteger(item.at("controlSlotID"), 0, kControlSlotMax);
                    if (!slot) return requestError(mcp::integerError("controlSlotID", 0, kControlSlotMax));
                    binding.controlSlotID = quint8(*slot);

                    if (*action == "append")
                        binding.action = VCBridge::AuthoringBindingItem::Action::Append;
                    else if (*action == "move")
                        binding.action = VCBridge::AuthoringBindingItem::Action::Move;
                    else if (*action == "remove")
                        binding.action = VCBridge::AuthoringBindingItem::Action::Remove;
                    else
                        return requestError("Controller action is not supported");

                    if (item.contains("functionID") || item.contains("keySequence") ||
                        item.contains("idleValue") || item.contains("activeValue") || item.contains("monitorValue") ||
                        item.contains("idleChannel") || item.contains("activeChannel") || item.contains("monitorChannel"))
                        return requestError("controller binding contains inapplicable fields");

                    if (binding.action == VCBridge::AuthoringBindingItem::Action::Append)
                    {
                        if (item.contains("sourceKey"))
                            return requestError("sourceKey is not allowed for controller append");
                    }
                    else
                    {
                        if (!item.contains("sourceKey"))
                            return requestError("sourceKey is required for controller move/remove");
                        VCBridge::AuthoringSourceKey sourceKey;
                        if (auto sourceErr = parseSourceKey(item.at("sourceKey"), sourceKey))
                            return requestError(*sourceErr);
                        if (sourceKey.controlSlotID != binding.controlSlotID)
                            return requestError("controlSlotID must match sourceKey.controlSlotID for move/remove");
                        binding.sourceKey = sourceKey;
                    }

                    if (binding.action == VCBridge::AuthoringBindingItem::Action::Remove)
                    {
                        if (item.contains("destination"))
                            return requestError("destination is not allowed for controller remove");
                    }
                    else
                    {
                        if (!item.contains("destination"))
                            return requestError("destination is required for controller append/move");
                        VCBridge::AuthoringDestination destination;
                        if (auto dstErr = parseDestination(item.at("destination"), destination))
                            return requestError(*dstErr);
                        binding.destination = destination;
                    }
                }
                else if (kind == "key")
                {
                    binding.kind = VCBridge::AuthoringBindingItem::Kind::Key;
                    if (!action.has_value())
                        return requestError("action is required for key bindings");
                    if (*action == "set")
                        binding.action = VCBridge::AuthoringBindingItem::Action::Set;
                    else if (*action == "remove")
                        binding.action = VCBridge::AuthoringBindingItem::Action::Remove;
                    else
                        return requestError("Key action is not supported");
                    if (!item.contains("controlSlotID"))
                        return requestError("controlSlotID is required for key bindings");
                    const auto slot = mcp::jsonInteger(item.at("controlSlotID"), 0, kControlSlotMax);
                    if (!slot) return requestError(mcp::integerError("controlSlotID", 0, kControlSlotMax));
                    binding.controlSlotID = quint8(*slot);
                    if (!item.contains("keySequence") || !item.at("keySequence").is_string())
                        return requestError("keySequence is required for key set/remove");
                    binding.keySequence = QString::fromStdString(item.at("keySequence").get<std::string>());
                    for (const char *field : {"functionID", "sourceKey", "destination",
                                              "idleValue", "activeValue", "monitorValue",
                                              "idleChannel", "activeChannel", "monitorChannel"})
                        if (item.contains(field))
                            return requestError(std::string(field) + " is not allowed for key bindings");
                }
                else
                {
                    binding.kind = VCBridge::AuthoringBindingItem::Kind::Feedback;
                    if (item.contains("action"))
                        return requestError("action is not allowed for feedback bindings");
                    if (item.contains("controlSlotID"))
                        return requestError("controlSlotID is not allowed for feedback bindings");
                    binding.action = VCBridge::AuthoringBindingItem::Action::Set;

                    if (!item.contains("sourceKey"))
                        return requestError("sourceKey is required for feedback set");
                    VCBridge::AuthoringSourceKey sourceKey;
                    if (auto sourceErr = parseSourceKey(item.at("sourceKey"), sourceKey))
                        return requestError(*sourceErr);
                    binding.sourceKey = sourceKey;

                    const bool hasAnyValues = item.contains("idleValue") || item.contains("activeValue") || item.contains("monitorValue");
                    const bool hasAllValues = item.contains("idleValue") && item.contains("activeValue") && item.contains("monitorValue");
                    if (hasAnyValues && !hasAllValues)
                        return requestError("Feedback values group must include idleValue, activeValue and monitorValue");
                    if (hasAllValues)
                    {
                        const auto idleValue = mcp::jsonInteger(item.at("idleValue"), 0, 255);
                        const auto activeValue = mcp::jsonInteger(item.at("activeValue"), 0, 255);
                        const auto monitorValue = mcp::jsonInteger(item.at("monitorValue"), 0, 255);
                        if (!idleValue || !activeValue || !monitorValue)
                            return requestError("Feedback values must be in range 0..255");
                        binding.idleValue = int(*idleValue);
                        binding.activeValue = int(*activeValue);
                        binding.monitorValue = int(*monitorValue);
                    }

                    const bool hasAnyChannels = item.contains("idleChannel") || item.contains("activeChannel") || item.contains("monitorChannel");
                    const bool hasAllChannels = item.contains("idleChannel") && item.contains("activeChannel") && item.contains("monitorChannel");
                    if (hasAnyChannels && !hasAllChannels)
                        return requestError("Feedback channels group must include idleChannel, activeChannel and monitorChannel");
                    if (hasAllChannels)
                    {
                        const auto idleChannel = mcp::jsonInteger(item.at("idleChannel"), -1, 15);
                        const auto activeChannel = mcp::jsonInteger(item.at("activeChannel"), -1, 15);
                        const auto monitorChannel = mcp::jsonInteger(item.at("monitorChannel"), -1, 15);
                        if (!idleChannel || !activeChannel || !monitorChannel)
                            return requestError("Feedback channels must be in range -1..15");
                        binding.idleChannel = int(*idleChannel);
                        binding.activeChannel = int(*activeChannel);
                        binding.monitorChannel = int(*monitorChannel);
                    }

                    if (!hasAllValues && !hasAllChannels)
                        return requestError("Feedback requires at least one complete values or channels group");
                    for (const char *field : {"functionID", "destination", "keySequence"})
                        if (item.contains(field))
                            return requestError(std::string(field) + " is not allowed for feedback bindings");
                }

                request.items.append(binding);
            }

            return VCAuthoring::toJson(vcBridge->prepareAuthoring(VCBridge::AuthoringRequest(request)));
            });
        }
    )
    .set_description("Prepare function/controller/key/feedback bindings with strict per-kind action contracts.")
    .set_annotations(mcp::kAnnotPreparation));

    // vc_map_inputs (batch)
    tm.register_tool(Tool(
        "vc_map_inputs",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"widgetID", {{"type", "integer"}}},
                {"inputUniverse", {{"type", "integer"}}},
                {"inputChannel", {{"type", "integer"}}},
                {"sourceName", {{"type", "string"}, {"description", sourceNameDesc}}},
                {"idleValue", {{"type", "integer"}, {"description", "LED color when inactive (velocity from color table, 0=off)"}}},
                {"activeValue", {{"type", "integer"}, {"description", "LED color when active (velocity from color table)"}}},
                {"monitorValue", {{"type", "integer"}, {"description", "LED color for monitor/intermediate state (velocity from color table)"}}},
                {"idleChannel", {{"type", "integer"}, {"description", "MIDI channel for idle state (from profile's MIDI channel table)"}}},
                {"activeChannel", {{"type", "integer"}, {"description", "MIDI channel for active state (from profile's MIDI channel table)"}}},
                {"monitorChannel", {{"type", "integer"}, {"description", "MIDI channel for monitor state (from profile's MIDI channel table)"}}}
            }}, {"required", {"widgetID", "inputUniverse", "inputChannel"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc, vcBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto itemsErr = validateItemsArray(args);
            if (itemsErr) return *itemsErr;
            static const char *kFeedbackFields[] = {"idleValue", "activeValue", "monitorValue",
                                                    "idleChannel", "activeChannel", "monitorChannel"};
            Json results = Json::array();
            const Json &items = args.at("items");
            for (size_t i = 0; i < items.size(); ++i)
            {
                const Json &item = items[i];
                auto itemErr = validateFields(item, {"widgetID", "inputUniverse", "inputChannel",
                    "sourceName", "idleValue", "activeValue", "monitorValue",
                    "idleChannel", "activeChannel", "monitorChannel"});
                if (!itemErr.empty()) { results.push_back(mcp::itemErrorFromDump(i, itemErr)); continue; }

                std::string rangeErr;
                auto checked = [&](const char *key, int64_t hi) -> int64_t {
                    auto v = item.contains(key) ? mcp::jsonInteger(item.at(key), 0, hi) : std::nullopt;
                    if (!v && rangeErr.empty()) rangeErr = mcp::integerError(key, 0, hi);
                    return v.value_or(0);
                };
                const int wid = int(checked("widgetID", std::numeric_limits<int>::max()));
                const quint32 uni = quint32(checked("inputUniverse", mcp::kMaxId));
                const quint32 ch = quint32(checked("inputChannel", mcp::kMaxId));

                // Feedback is all-or-none: any one of the six fields requires the other five.
                int supplied = 0;
                for (const char *key : kFeedbackFields)
                    supplied += item.contains(key) ? 1 : 0;
                const bool hasFeedback = supplied > 0;
                int fb[6] = {0, 0, 0, 0, 0, 0};
                if (hasFeedback && rangeErr.empty())
                {
                    if (supplied != 6)
                    {
                        results.push_back(mcp::itemError(i, "all 6 feedback fields required when any feedback is supplied "
                                      "(idleValue, activeValue, monitorValue, idleChannel, activeChannel, monitorChannel)"));
                        continue;
                    }
                    for (int f = 0; f < 6; ++f)
                        fb[f] = int(checked(kFeedbackFields[f], 255));
                }
                if (!rangeErr.empty()) { results.push_back(mcp::itemError(i, rangeErr)); continue; }

                std::string srcName = item.value("sourceName", "default");
                QString qSrcName = QString::fromStdString(srcName);

                // Snapshot existing feedback for this specific source before remap
                VCBridge::FeedbackInfo savedFb = vcBridge->getWidgetFeedbackByName(wid, qSrcName);
                bool hadFeedback = (savedFb.idleValue != 0 || savedFb.activeValue != 0 ||
                                    savedFb.monitorValue != 0);

                // Always map by name — works for all widget types and source counts
                bool ok = vcBridge->mapWidgetInputByName(wid, qSrcName, uni, ch);

                if (!ok)
                {
                    Json rec = mcp::itemError(i, "mapping failed (invalid sourceName or widgetID)");
                    rec["widgetID"] = wid;
                    results.push_back(rec);
                    continue;
                }

                // Apply feedback (source-specific)
                if (hasFeedback)
                {
                    vcBridge->setWidgetFeedbackByName(wid, qSrcName,
                        fb[0], fb[1], fb[2], fb[3], fb[4], fb[5]);
                }
                else if (hadFeedback)
                {
                    vcBridge->setWidgetFeedbackByName(wid, qSrcName,
                        savedFb.idleValue, savedFb.activeValue, savedFb.monitorValue,
                        savedFb.idleMidiCh, savedFb.activeMidiCh, savedFb.monitorMidiCh);
                }

                results.push_back(mcp::itemOk(i, {{"widgetID", wid}}));
            }
            return results.dump();
            });
        }
    )
    .set_description("Map external controller inputs (OSC/MIDI faders) to Virtual Console widgets. "
                     "Optionally set LED feedback in the same call (all 6 feedback fields required together). "
                     "Feedback is preserved across remaps if not explicitly supplied. Batch. "
                     "Wrap multiple operations in {\"items\": [...]}. Each item is processed independently.")
    .set_annotations(mcp::kAnnotIdempotent));

    // vc_configure_feedback (batch)
    tm.register_tool(Tool(
        "vc_configure_feedback",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"widgetID", {{"type", "integer"}}},
                {"sourceName", {{"type", "string"}, {"description", sourceNameDesc}}},
                {"idleValue", {{"type", "integer"}, {"description", "LED color when inactive (velocity from color table, 0=off)"}}},
                {"activeValue", {{"type", "integer"}, {"description", "LED color when active"}}},
                {"monitorValue", {{"type", "integer"}, {"description", "LED color for monitor/intermediate state"}}},
                {"idleChannel", {{"type", "integer"}, {"description", "MIDI channel for idle state (from profile's MIDI channel table, default 0)"}}},
                {"activeChannel", {{"type", "integer"}, {"description", "MIDI channel for active state (from profile's MIDI channel table, default 0)"}}},
                {"monitorChannel", {{"type", "integer"}, {"description", "MIDI channel for monitor state (from profile's MIDI channel table, default 0)"}}},
                {"idleMode", {{"type", "string"}, {"enum", {"static", "flashing", "pulsing"}}, {"description", "Deprecated: use idleChannel instead. LED animation mode for idle state (default static)"}}},
                {"activeMode", {{"type", "string"}, {"enum", {"static", "flashing", "pulsing"}}, {"description", "Deprecated: use activeChannel instead. LED animation mode for active state (default static)"}}},
                {"monitorMode", {{"type", "string"}, {"enum", {"static", "flashing", "pulsing"}}, {"description", "Deprecated: use monitorChannel instead. LED animation mode for monitor state"}}}
            }}, {"required", {"widgetID", "activeValue"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc, vcBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            try {
            auto itemsErr = validateItemsArray(args);
            if (itemsErr) return *itemsErr;
            auto midiChFromMode = [](const std::string &mode) -> int {
                if (mode == "flashing") return 1;
                if (mode == "pulsing") return 2;
                return 0;
            };
            static const Json kModeEnums = {
                {"idleMode", {{"enum", {"static", "flashing", "pulsing"}}}},
                {"activeMode", {{"enum", {"static", "flashing", "pulsing"}}}},
                {"monitorMode", {{"enum", {"static", "flashing", "pulsing"}}}}
            };
            Json results = Json::array();
            const Json &items = args.at("items");
            for (size_t i = 0; i < items.size(); ++i)
            {
                const Json &item = items[i];
                auto err = validateFields(item, {"widgetID", "activeValue", "idleValue", "monitorValue",
                    "sourceName", "idleChannel", "activeChannel", "monitorChannel",
                    "idleMode", "activeMode", "monitorMode"});
                if (err.empty()) err = validateEnums(item, kModeEnums);
                if (!err.empty()) { results.push_back(mcp::itemErrorFromDump(i, err)); continue; }

                std::string rangeErr;
                auto checked = [&](const char *key, int64_t hi, int64_t fallback, bool required) -> int64_t {
                    if (!item.contains(key))
                    {
                        if (required && rangeErr.empty()) rangeErr = mcp::integerError(key, 0, hi);
                        return fallback;
                    }
                    auto v = mcp::jsonInteger(item.at(key), 0, hi);
                    if (!v && rangeErr.empty()) rangeErr = mcp::integerError(key, 0, hi);
                    return v.value_or(fallback);
                };
                auto mode = [&](const char *key, const char *fallback) {
                    return midiChFromMode(VCValidate::canonicalEnum(item.value(key, fallback), kModeEnums[key]["enum"]));
                };
                const int wid = int(checked("widgetID", std::numeric_limits<int>::max(), 0, true));
                const int activeVal = int(checked("activeValue", 255, 0, true));
                const int idleVal = int(checked("idleValue", 255, 0, false));
                const int monitorVal = int(checked("monitorValue", 255, 0, false));
                // Integer channel fields take precedence; fall back to string mode names
                const int midiChIdle = int(checked("idleChannel", 255, mode("idleMode", "static"), false));
                const int midiChActive = int(checked("activeChannel", 255, mode("activeMode", "static"), false));
                const int midiChMonitor = int(checked("monitorChannel", 255, mode("monitorMode", ""), false));
                if (!rangeErr.empty()) { results.push_back(mcp::itemError(i, rangeErr)); continue; }

                QString srcName = QString::fromStdString(item.value("sourceName", "default"));
                bool ok = vcBridge->setWidgetFeedbackByName(wid, srcName,
                    idleVal, activeVal, monitorVal, midiChIdle, midiChActive, midiChMonitor);

                // Fall back to legacy per-widget feedback if sourceName is default
                if (!ok && srcName == QStringLiteral("default"))
                    ok = vcBridge->setWidgetFeedback(wid, idleVal, activeVal, monitorVal,
                                                      midiChIdle, midiChActive, midiChMonitor);

                Json rec = ok ? mcp::itemOk(i, {{"outcome", "updated"}})
                              : mcp::itemError(i, "feedback not applied (unknown widgetID or sourceName)");
                rec["widgetID"] = wid;
                results.push_back(rec);
            }
            return results.dump();
            } catch (const std::exception &e) {
                return Json({{"error", e.what()}}).dump();
            }
            });
        }
    )
    .set_description("Set LED feedback colors and animation mode per widget input source. "
                     "Use sourceName to target a specific source (default 'default'). "
                     "Use integer idleChannel/activeChannel/monitorChannel (from query_feedback_profile) "
                     "or legacy string idleMode/activeMode/monitorMode. Batch. "
                     "Wrap multiple operations in {\"items\": [...]}. Each item is processed independently.")
    .set_annotations(mcp::kAnnotIdempotent));

    // vc_set_key_sequences (batch)
    tm.register_tool(Tool(
        "vc_set_key_sequences",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"widgetID", {{"type", "integer"}}},
                {"sourceName", {{"type", "string"}, {"description", sourceNameDesc}}},
                {"keySequence", {{"type", "string"}, {"description", "Key sequence string (e.g. 'Ctrl+A', 'Space', 'F5')"}}}
            }}, {"required", {"widgetID", "keySequence"}}}}}}
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
                auto err = validateFields(item, {"widgetID", "sourceName", "keySequence"});
                if (!err.empty()) { results.push_back(mcp::itemErrorFromDump(i, err)); continue; }
                auto widgetID = item.contains("widgetID")
                    ? mcp::jsonInteger(item.at("widgetID"), 0, std::numeric_limits<int>::max()) : std::nullopt;
                if (!widgetID)
                {
                    results.push_back(mcp::itemError(i, mcp::integerError("widgetID", 0, std::numeric_limits<int>::max())));
                    continue;
                }
                if (!item.contains("keySequence") || !item.at("keySequence").is_string())
                {
                    results.push_back(mcp::itemError(i, "keySequence must be a string"));
                    continue;
                }
                QString sourceName = QString::fromStdString(item.value("sourceName", "default"));
                QKeySequence ks(QString::fromStdString(item.at("keySequence").get<std::string>()));
                bool ok = vcBridge->setWidgetKeySequence(int(*widgetID), sourceName, ks);
                Json rec = ok ? mcp::itemOk(i, {{"outcome", "updated"}})
                              : mcp::itemError(i, "failed to set key sequence");
                rec["widgetID"] = int(*widgetID);
                results.push_back(rec);
            }
            return results.dump();
            });
        }
    )
    .set_description("Set keyboard shortcuts on Virtual Console widgets. Batch. "
                     "Wrap multiple operations in {\"items\": [...]}. Each item is processed independently.")
    .set_annotations(mcp::kAnnotIdempotent));
}
