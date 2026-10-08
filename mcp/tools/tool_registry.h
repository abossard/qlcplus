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
#include <functional>
#include <cmath>
#include <cstdint>
#include <limits>

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
// Pure diagnostic control (no Doc): runtime toggle/snapshot of TimingDiag.
void registerProfilingTools(fastmcpp::tools::ToolManager &tm);
namespace fastmcpp { namespace prompts { class PromptManager; } }
void registerPrompts(fastmcpp::prompts::PromptManager &pm, Doc *doc, VCBridge *vcBridge);

// MCP tool annotation constants (readOnlyHint, destructiveHint, idempotentHint, openWorldHint)
namespace mcp {
using Json = nlohmann::json;
inline const Json kAnnotReadOnly    = {{"readOnlyHint", true},  {"destructiveHint", false}, {"idempotentHint", true},  {"openWorldHint", false}};
inline const Json kAnnotIdempotent  = {{"readOnlyHint", false}, {"destructiveHint", false}, {"idempotentHint", true},  {"openWorldHint", false}};
inline const Json kAnnotDestructive = {{"readOnlyHint", false}, {"destructiveHint", true},  {"idempotentHint", true},  {"openWorldHint", false}};
inline const Json kAnnotAdditive    = {{"readOnlyHint", false}, {"destructiveHint", false}, {"idempotentHint", false}, {"openWorldHint", false}};
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

namespace mcp {

/**
 * Checked JSON integer read. Accepts integer numbers and whole-valued finite
 * floats (JSON Schema "integer"); rejects bool, string, fractional and values
 * outside [lo, hi] before any narrowing.
 */
inline std::optional<int64_t> jsonInteger(const nlohmann::json &v, int64_t lo, int64_t hi)
{
    if (v.is_number_unsigned())
    {
        const uint64_t u = v.get<uint64_t>();
        if (hi < 0 || u > static_cast<uint64_t>(hi) || static_cast<int64_t>(u) < lo)
            return std::nullopt;
        return static_cast<int64_t>(u);
    }
    if (v.is_number_integer())
    {
        const int64_t i = v.get<int64_t>();
        if (i < lo || i > hi) return std::nullopt;
        return i;
    }
    if (v.is_number_float())
    {
        const double d = v.get<double>();
        if (!std::isfinite(d) || std::trunc(d) != d ||
            d < static_cast<double>(lo) || d > static_cast<double>(hi))
            return std::nullopt;
        return static_cast<int64_t>(d);
    }
    return std::nullopt;
}

inline std::string integerError(const std::string &field, int64_t lo, int64_t hi)
{
    return field + " must be an integer in [" + std::to_string(lo) + ", " + std::to_string(hi) + "]";
}

// Q8 batch outcome: one indexed terminal record per input item.
inline nlohmann::json itemError(size_t index, const std::string &message)
{
    return {{"index", index}, {"status", "error"}, {"error", message}};
}

// Same, from a dumped {"error": ...} produced by validateFields/validateEnums.
inline nlohmann::json itemErrorFromDump(size_t index, const std::string &dumpedError)
{
    return itemError(index, nlohmann::json::parse(dumpedError).value("error", dumpedError));
}

inline nlohmann::json itemOk(size_t index, nlohmann::json fields = nlohmann::json::object())
{
    fields["index"] = index;
    fields["status"] = "ok";
    return fields;
}

// Q8 for loops that push exactly one record per input item: stamp index and
// mark records carrying "error" with status "error".
inline nlohmann::json indexedRecords(nlohmann::json records)
{
    for (size_t i = 0; i < records.size(); ++i)
    {
        records[i]["index"] = i;
        if (records[i].contains("error"))
            records[i]["status"] = "error";
    }
    return records;
}

// A tool payload is a total failure when it is a request-level {"error": ...}
// object or a non-empty batch in which every item record has status "error".
inline bool isToolFailure(const nlohmann::json &payload)
{
    if (payload.is_object())
        return payload.contains("error") ||
               (payload.contains("items") && isToolFailure(payload.at("items")));
    if (!payload.is_array() || payload.empty())
        return false;
    for (const auto &rec : payload)
        if (!rec.is_object() || rec.value("status", "") != "error")
            return false;
    return true;
}

// Adds isError (and structuredContent for object payloads) to a CallToolResult
// whose single text block carries the tool's serialized JSON payload.
inline nlohmann::json encodeToolResult(nlohmann::json result)
{
    if (!result.is_object() || result.contains("isError") || !result.contains("content") ||
        !result["content"].is_array() || result["content"].size() != 1 ||
        !result["content"][0].is_object() || !result["content"][0].value("text", nlohmann::json()).is_string())
        return result;
    const nlohmann::json payload = nlohmann::json::parse(
        result["content"][0]["text"].get<std::string>(), nullptr, false);
    if (payload.is_discarded())
        return result;
    result["isError"] = isToolFailure(payload);
    if (payload.is_object() && !result.contains("structuredContent"))
        result["structuredContent"] = payload;
    return result;
}

// Wraps a JSON-RPC MCP handler so tools/call results carry a truthful isError.
inline std::function<nlohmann::json(const nlohmann::json &)>
withToolResultEncoding(std::function<nlohmann::json(const nlohmann::json &)> inner)
{
    return [inner = std::move(inner)](const nlohmann::json &request) {
        nlohmann::json response = inner(request);
        if (request.is_object() && request.value("method", "") == "tools/call" &&
            response.is_object() && response.contains("result"))
            response["result"] = encodeToolResult(std::move(response["result"]));
        return response;
    };
}

// Object/function/group IDs: 0 .. UINT32_MAX-1 (UINT32_MAX is QLC+'s invalid ID).
constexpr int64_t kMaxId = int64_t(std::numeric_limits<uint32_t>::max()) - 1;

} // namespace mcp

#endif // TOOL_REGISTRY_H
