/*
  Q Light Controller Plus
  profiling_tools.cpp

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

/*
 * Runtime control for the opt-in MasterTimer timing diagnostics (TimingDiag),
 * so an operator can profile the active effects on demand without restarting
 * the application.
 *
 * This tool calls the TimingDiag production API directly from the HTTP worker
 * thread. That is deliberate and safe: TimingDiag is fully self-synchronised
 * (an internal mutex plus atomics) and touches no Doc or other Qt object, so it
 * needs no execOnMainThread hop - that convention exists to protect Doc access,
 * which this tool does not perform. All parameters are validated before any
 * state changes, and every response reports the current capture identity and
 * state so a caller can see the effect of the action.
 */

#include "tool_registry.h"
#include "timingdiagnostics.h"

#include <QString>

#include <fastmcpp/tools/manager.hpp>
#include <fastmcpp/tools/tool.hpp>

namespace {

using Json = nlohmann::json;

// Fixed unknown sentinel for a Function id (Function::invalidId()).
constexpr quint32 kInvalidId = 0xFFFFFFFFu;

// Render one capture-cumulative bucket as raw numeric fields. Every duration is
// in nanoseconds; -1 means "unknown" (never a fabricated 0), matching the engine
// convention. The metric fields are named per kind rather than exposed as opaque
// m0..m4.
Json bucketToJson(const TimingDiag::BucketStat &b)
{
    Json j = {
        {"kind", b.kind.toStdString()},
        {"key", b.key.toStdString()},
        {"count", b.count},
        {"windowMs", b.windowMs},
        {"worstPrimaryNs", b.worstPrimaryNs}
    };

    if (b.kind == QStringLiteral("scheduler"))
    {
        j["maxDispatchLateNs"] = b.m0;
        j["periodNs"] = b.m1;
        j["priorCallbackNs"] = b.m2;
        j["priorFinishedLateNs"] = b.m3;
        j["slept"] = b.slept;
        j["unknownTicks"] = b.unknownTicks;
        j["clockReadFailures"] = b.clockReadFailures;
        j["measuredTicks"] = b.count - b.unknownTicks;
    }
    else if (b.kind == QStringLiteral("period"))
    {
        j["maxCallbackNs"] = b.m0;
        j["periodNs"] = b.m1;
        j["maxWriteNs"] = b.m2;
        j["functionsNs"] = b.m3;
        j["dispatchLateNs"] = b.m4;
        j["maxWriteId"] = (b.id == kInvalidId) ? Json(nullptr) : Json(b.id);
        j["maxWriteName"] = b.name.toStdString();
        j["maxWriteType"] = b.type.toStdString();
    }
    else if (b.kind == QStringLiteral("hue"))
    {
        j["maxWaitNs"] = b.m0;
        j["queueNs"] = b.m1;
        j["workerNs"] = b.m2;
        j["workerCpuNs"] = b.m3;
        // Worker wall minus on-CPU thread time: render time NOT running on that
        // core. Unknown if either component is unknown.
        j["workerOffCpuNs"] = (b.m2 >= 0 && b.m3 >= 0)
                              ? (b.m2 - b.m3 < 0 ? 0 : b.m2 - b.m3) : -1;
        j["cpuUnknownRenders"] = b.cpuUnknownCount;
        j["id"] = (b.id == kInvalidId) ? Json(nullptr) : Json(b.id);
        j["name"] = b.name.toStdString();
    }
    else // rgb
    {
        j["id"] = (b.id == kInvalidId) ? Json(nullptr) : Json(b.id);
        j["name"] = b.name.toStdString();
    }
    return j;
}

Json snapshotToJson(const TimingDiag::CaptureStat &s)
{
    Json buckets = Json::array();
    for (const TimingDiag::BucketStat &b : s.buckets)
        buckets.push_back(bucketToJson(b));

    return Json{
        {"enabled", s.enabled},
        {"captureId", s.captureId},
        {"intervalMs", s.intervalMs},
        {"elapsedMs", s.elapsedMs},
        {"discardedSamples", s.discardedSamples},
        {"nsUnknownSentinel", -1},
        {"buckets", buckets}
    };
}

} // namespace

void registerProfilingTools(fastmcpp::tools::ToolManager &tm)
{
    using Tool = fastmcpp::tools::Tool;

    tm.register_tool(Tool(
        "timing_diagnostics",
        Json{{"type", "object"}, {"properties", {
            {"action", {{"type", "string"},
                        {"enum", {"enable", "disable", "set_interval", "reset", "snapshot"}},
                        {"description",
                         "enable: start a fresh capture and begin sampling. "
                         "disable: stop sampling but retain the capture for a later snapshot. "
                         "set_interval: change the [timing] window/report interval (needs intervalMs). "
                         "reset: begin a new capture, clearing retained aggregates. "
                         "snapshot: return the current capture's bounded aggregates without consuming them."}}},
            {"intervalMs", {{"type", "integer"}, {"minimum", 1},
                            {"description", "Window/report interval in milliseconds; only valid with set_interval"}}}
        }}, {"required", {"action"}}},
        Json{},
        [](const Json &args) -> Json {
            // Reject unknown or inapplicable fields and invalid actions BEFORE
            // any state change.
            auto err = validateFields(args, {"action", "intervalMs"});
            if (!err.empty()) return err;

            if (!args.contains("action") || !args.at("action").is_string())
                return Json({{"error", "action is required and must be a string"}}).dump();

            static const Json kEnums = {
                {"action", {{"enum", {"enable", "disable", "set_interval", "reset", "snapshot"}}}}
            };
            auto enumErr = validateEnums(args, kEnums);
            if (!enumErr.empty()) return enumErr;

            const std::string action = toLowerStd(args.at("action").get<std::string>());
            const bool hasInterval = args.contains("intervalMs");

            if (action == "set_interval")
            {
                if (!hasInterval)
                    return Json({{"error", "set_interval requires intervalMs"}}).dump();
                if (!args.at("intervalMs").is_number_integer())
                    return Json({{"error", "intervalMs must be an integer"}}).dump();
                const long long ms = args.at("intervalMs").get<long long>();
                if (ms <= 0)
                    return Json({{"error", "intervalMs must be a positive integer"}}).dump();

                TimingDiag::setIntervalMs(ms);
            }
            else
            {
                if (hasInterval)
                    return Json({{"error", "intervalMs is only valid with action=set_interval"}}).dump();

                if (action == "enable")
                    TimingDiag::setEnabled(true);
                else if (action == "disable")
                    TimingDiag::setEnabled(false);
                else if (action == "reset")
                    TimingDiag::reset();
                // "snapshot" is a pure read; it falls through to the report below.
            }

            Json result = {
                {"status", "ok"},
                {"action", action},
                {"state", snapshotToJson(TimingDiag::snapshot())}
            };
            return result.dump();
        },
        std::nullopt,
        std::optional<std::string>(std::string(
            "Runtime control for the opt-in MasterTimer timing diagnostics. Actions: "
            "enable (start a fresh capture and sample the active effects), disable (stop "
            "sampling but keep the capture), set_interval (change the report window, needs "
            "intervalMs), reset (begin a new capture), snapshot (read bounded, non-consuming "
            "capture aggregates). Durations are nanoseconds; -1 means unknown, never a "
            "fabricated 0. Diagnostic-only: it never changes DMX output, functions or show "
            "state, and does not start or stop the timer.")),
        std::nullopt)
    // readOnly=false (enable/disable/reset/set_interval mutate diagnostic state),
    // destructive=false (no show/output/fixture impact), idempotent=false (reset
    // and enable advance the capture identity), openWorld=false.
    .set_annotations(Json{{"readOnlyHint", false}, {"destructiveHint", false},
                          {"idempotentHint", false}, {"openWorldHint", false}}));
}
