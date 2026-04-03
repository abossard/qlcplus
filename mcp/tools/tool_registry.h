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
#include <string>
#include <vector>

namespace fastmcpp { namespace tools { class ToolManager; } }
class Doc;
class VCBridge;
class FunctionManager;

// Each tool file exports one registration function.
void registerQueryTools(fastmcpp::tools::ToolManager &tm, Doc *doc, VCBridge *vcBridge);
void registerFunctionTools(fastmcpp::tools::ToolManager &tm, Doc *doc, FunctionManager *funcMgr = nullptr);
void registerVCCreateTools(fastmcpp::tools::ToolManager &tm, Doc *doc, VCBridge *vcBridge);
void registerVCUpdateTools(fastmcpp::tools::ToolManager &tm, Doc *doc, VCBridge *vcBridge);
void registerVCInputTools(fastmcpp::tools::ToolManager &tm, Doc *doc, VCBridge *vcBridge);
void registerVCLayoutTools(fastmcpp::tools::ToolManager &tm, Doc *doc, VCBridge *vcBridge);
void registerIOTools(fastmcpp::tools::ToolManager &tm, Doc *doc);
void registerChannelTools(fastmcpp::tools::ToolManager &tm, Doc *doc);
namespace fastmcpp { namespace prompts { class PromptManager; } }
void registerPrompts(fastmcpp::prompts::PromptManager &pm, Doc *doc);

// MCP tool annotation constants (readOnlyHint, destructiveHint, idempotentHint, openWorldHint)
namespace mcp {
using Json = nlohmann::json;
inline const Json kAnnotReadOnly    = {{"readOnlyHint", true},  {"destructiveHint", false}, {"idempotentHint", true},  {"openWorldHint", false}};
inline const Json kAnnotIdempotent  = {{"readOnlyHint", false}, {"destructiveHint", false}, {"idempotentHint", true},  {"openWorldHint", false}};
inline const Json kAnnotDestructive = {{"readOnlyHint", false}, {"destructiveHint", true},  {"idempotentHint", true},  {"openWorldHint", false}};
inline const Json kAnnotOpenWorld   = {{"readOnlyHint", false}, {"destructiveHint", false}, {"idempotentHint", true},  {"openWorldHint", true}};
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

#endif // TOOL_REGISTRY_H
