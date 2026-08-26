/*
  Q Light Controller Plus
  tool_registry.h

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

#ifndef TOOL_REGISTRY_H
#define TOOL_REGISTRY_H

#include <QObject>
#include <QThread>
#include <QMetaObject>
#include <nlohmann/json.hpp>

#include <initializer_list>
#include <optional>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>

namespace fastmcpp { namespace tools { class ToolManager; } }
class Doc;
class VCBridge;
class WorkspaceBridge;
class FunctionManager;
class FlowConsole;

// Each tool file exports one registration function.
void registerQueryTools(fastmcpp::tools::ToolManager &tm, Doc *doc, VCBridge *vcBridge);
void registerFunctionTools(fastmcpp::tools::ToolManager &tm, Doc *doc, FunctionManager *funcMgr = nullptr);
void registerVCCreateTools(fastmcpp::tools::ToolManager &tm, Doc *doc, VCBridge *vcBridge);
void registerVCUpdateTools(fastmcpp::tools::ToolManager &tm, Doc *doc, VCBridge *vcBridge);
void registerVCInputTools(fastmcpp::tools::ToolManager &tm, Doc *doc, VCBridge *vcBridge);
void registerVCLayoutTools(fastmcpp::tools::ToolManager &tm, Doc *doc, VCBridge *vcBridge);
void registerIOTools(fastmcpp::tools::ToolManager &tm, Doc *doc);
void registerChannelTools(fastmcpp::tools::ToolManager &tm, Doc *doc);
void registerPaletteTools(fastmcpp::tools::ToolManager &tm, Doc *doc);
void registerFlowTools(fastmcpp::tools::ToolManager &tm, Doc *doc, FlowConsole *fc);
void registerWorkspaceTools(fastmcpp::tools::ToolManager &tm, Doc *doc, WorkspaceBridge *wsBridge);
void registerStageTools(fastmcpp::tools::ToolManager &tm, Doc *doc);
void registerInputProfileTools(fastmcpp::tools::ToolManager &tm, Doc *doc);
void registerLiveTools(fastmcpp::tools::ToolManager &tm, Doc *doc);
void registerShowTools(fastmcpp::tools::ToolManager &tm, Doc *doc);
namespace fastmcpp { namespace prompts { class PromptManager; } }
void registerPrompts(fastmcpp::prompts::PromptManager &pm, Doc *doc, VCBridge *vcBridge);

// MCP tool annotation constants (readOnlyHint, destructiveHint, idempotentHint, openWorldHint)
namespace mcp {
using Json = nlohmann::json;
inline const Json kAnnotReadOnly    = {{"readOnlyHint", true},  {"destructiveHint", false}, {"idempotentHint", true},  {"openWorldHint", false}};
inline const Json kAnnotIdempotent  = {{"readOnlyHint", false}, {"destructiveHint", false}, {"idempotentHint", true},  {"openWorldHint", false}};
inline const Json kAnnotDestructive = {{"readOnlyHint", false}, {"destructiveHint", true},  {"idempotentHint", true},  {"openWorldHint", false}};
inline const Json kAnnotOpenWorld   = {{"readOnlyHint", false}, {"destructiveHint", false}, {"idempotentHint", true},  {"openWorldHint", true}};
}

// ASCII case-insensitive lowercase. Used for enum/string normalisation in
// MCP handlers so agents can send e.g. "Loop", "loop", or "LOOP".
inline std::string toLowerStd(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

// Thread-safe execution helper — runs lambda on Doc's thread
// Wraps in try/catch to prevent crashes from malformed JSON
template<typename Func>
auto execOnMainThread(QObject *context, Func &&func) -> decltype(func())
{
    using ReturnType = decltype(func());
    auto safeFunc = [&func]() -> ReturnType {
        try {
            return func();
        } catch (const std::exception &e) {
            using Json = nlohmann::json;
            return Json({{"error", e.what()}}).dump();
        } catch (...) {
            using Json = nlohmann::json;
            return Json({{"error", "unknown error"}}).dump();
        }
    };
    if (QThread::currentThread() == context->thread())
        return safeFunc();
    ReturnType result;
    QMetaObject::invokeMethod(context, [&result, &safeFunc]() {
        result = safeFunc();
    }, Qt::BlockingQueuedConnection);
    return result;
}

/**
 * Validate a JSON object against a whitelist of allowed field names.
 * Returns an error JSON string if unknown fields are found, or empty string if valid.
 * Usage: auto err = validateFields(item, {"name", "fixtureIDs", "channelValues"});
 *        if (!err.empty()) return err;
 */
inline std::string validateFields(const nlohmann::json &obj,
                                   std::initializer_list<std::string> allowed)
{
    if (!obj.is_object()) return "";
    std::vector<std::string> unknown;
    for (auto it = obj.begin(); it != obj.end(); ++it)
    {
        bool found = false;
        for (const auto &a : allowed)
            if (it.key() == a) { found = true; break; }
        if (!found)
            unknown.push_back(it.key());
    }
    if (unknown.empty()) return "";
    std::string msg = "unknown fields: ";
    for (size_t i = 0; i < unknown.size(); i++)
    {
        if (i > 0) msg += ", ";
        msg += unknown[i];
    }
    msg += ". Allowed: ";
    bool first = true;
    for (const auto &a : allowed)
    {
        if (!first) msg += ", ";
        msg += a;
        first = false;
    }
    return nlohmann::json({{"error", msg}}).dump();
}

/**
 * Validate that args contains a non-empty array under `key`.
 * Returns an error JSON string (already dumped) on failure, or std::nullopt if valid.
 * Usage: auto itemsErr = validateItemsArray(args);
 *        if (itemsErr) return *itemsErr;
 */
inline std::optional<std::string> validateItemsArray(const nlohmann::json &args,
                                                      const char *key = "items")
{
    if (!args.contains(key) || !args[key].is_array())
        return nlohmann::json({{"error", std::string("'") + key + "' array is required"}}).dump();
    if (args[key].empty())
        return nlohmann::json({{"error", std::string("'") + key + "' array must not be empty"}}).dump();
    return std::nullopt;
}

/**
 * Validate enum constraints from a tool schema against actual input values.
 * Checks every field in the input that has an "enum" array in the schema properties.
 * Returns an error JSON string on mismatch, or empty string if all valid.
 * Usage: auto err = validateEnums(args, schema["properties"]);
 *        if (!err.empty()) return err;
 */
inline std::string validateEnums(const nlohmann::json &obj,
                                  const nlohmann::json &schemaProperties)
{
    if (!obj.is_object() || !schemaProperties.is_object()) return "";
    for (auto &[field, subschema] : schemaProperties.items())
    {
        if (!obj.contains(field)) continue;
        if (!subschema.contains("enum") || !subschema["enum"].is_array()) continue;

        const auto &val = obj[field];
        const auto &allowed = subschema["enum"];
        bool found = false;
        std::string valLower = val.is_string() ? toLowerStd(val.get<std::string>()) : std::string();
        for (const auto &v : allowed)
        {
            if (v == val) { found = true; break; }
            if (v.is_string() && val.is_string() &&
                toLowerStd(v.get<std::string>()) == valLower)
            { found = true; break; }
        }
        if (!found)
        {
            std::string msg = "invalid value for '" + field + "': " +
                              val.dump() + ". Allowed: ";
            bool first = true;
            for (const auto &v : allowed)
            {
                if (!first) msg += ", ";
                msg += v.dump();
                first = false;
            }
            return nlohmann::json({{"error", msg}}).dump();
        }
    }
    return "";
}

#endif // TOOL_REGISTRY_H
