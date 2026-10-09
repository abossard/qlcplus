/*
  Q Light Controller Plus
  live_tools.cpp

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
 * Setup and configuration that necessarily touches live output.
 *
 * The Grand Master is saved project configuration, and verifying a patch means
 * actually lighting the lamp — neither can be done without the rig responding.
 * These tools exist to make setup complete, not to run a show: there is
 * deliberately no cue stepping, no timed playback and no fade control here.
 * Timed logic belongs in a Script function.
 */

#include "tool_registry.h"
#include "doc.h"
#include "fixture.h"
#include "function.h"
#include "functionparent.h"
#include "genericdmxsource.h"
#include "grandmaster.h"
#include "inputoutputmap.h"
#include "mastertimer.h"
#include "universe.h"

#include <QPointer>

#include <memory>

#include <fastmcpp/tools/manager.hpp>
#include <fastmcpp/tools/tool.hpp>

namespace {

using Json = nlohmann::json;

}

void registerLiveTools(fastmcpp::tools::ToolManager &tm, Doc *doc)
{
    using Tool = fastmcpp::tools::Tool;

    /*
     * One source for the whole server. A GenericDMXSource holds its values on
     * the MasterTimer for as long as it lives, which is what makes "set a
     * channel and go look at the lamp" work. It is not a QObject, so the
     * lifetime is tied to the registered handlers instead of to Doc: the
     * shared_ptr dies with the ToolManager, and its destructor unregisters the
     * source from the MasterTimer.
     */
    QPointer<Doc> docGuard(doc);
    auto source = std::shared_ptr<GenericDMXSource>(
        new GenericDMXSource(doc),
        [docGuard](GenericDMXSource *s) {
            // ~GenericDMXSource calls doc->masterTimer()->unregisterDMXSource(),
            // but ~Doc deletes the MasterTimer first. If Doc is already gone the
            // only safe move is to leak the source rather than dereference null.
            if (!docGuard.isNull())
                delete s;
        });
    source->setOutputEnabled(true);

    // Held values are keyed by fixture ID, and a freshly loaded project hands
    // those IDs to entirely different fixtures — so a hold left over from the
    // previous project would silently drive the wrong lamp.
    QObject::connect(doc, &Doc::cleared, doc, [source]() { source->unsetAll(); });

    // set_grand_master — saved project configuration for the master limiter
    tm.register_tool(Tool(
        "set_grand_master",
        Json{{"type", "object"}, {"properties", {
            {"value", {{"type", "integer"}, {"minimum", 0}, {"maximum", 255},
                       {"description", "Grand Master level, 0-255"}}},
            {"valueMode", {{"type", "string"}, {"enum", {"limit", "reduce"}},
                           {"description", "limit clamps channel values, reduce scales them"}}},
            {"channelMode", {{"type", "string"}, {"enum", {"intensity", "allChannels"}},
                             {"description", "Which channels the Grand Master applies to"}}}
        }}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto err = validateFields(args, {"value", "valueMode", "channelMode"});
            if (!err.empty()) return err;

            static const Json kEnums = {
                {"valueMode", {{"enum", {"limit", "reduce"}}}},
                {"channelMode", {{"enum", {"intensity", "allChannels"}}}}
            };
            auto enumErr = validateEnums(args, kEnums);
            if (!enumErr.empty()) return enumErr;

            InputOutputMap *ioMap = doc->inputOutputMap();

            if (args.contains("value"))
            {
                if (!args.at("value").is_number_integer())
                    return Json({{"error", "value must be an integer"}}).dump();
                const int value = args.at("value").get<int>();
                if (value < 0 || value > 255)
                    return Json({{"error", "value must be from 0 to 255"}}).dump();
                ioMap->setGrandMasterValue((uchar)value);
            }
            if (args.contains("valueMode"))
            {
                const std::string mode = toLowerStd(args.at("valueMode").get<std::string>());
                ioMap->setGrandMasterValueMode(mode == "limit" ? GrandMaster::Limit
                                                               : GrandMaster::Reduce);
            }
            if (args.contains("channelMode"))
            {
                const std::string mode = toLowerStd(args.at("channelMode").get<std::string>());
                ioMap->setGrandMasterChannelMode(mode == "allchannels" ? GrandMaster::AllChannels
                                                                       : GrandMaster::Intensity);
            }

            // Deliberately no setModified(): the Grand Master is not written to
            // the project file, so marking the document dirty would make
            // load_workspace and new_workspace refuse for a change that cannot
            // be saved in the first place.
            return Json({
                {"status", "ok"},
                {"value", (int)ioMap->grandMasterValue()},
                {"valueMode", ioMap->grandMasterValueMode() == GrandMaster::Limit ? "limit" : "reduce"},
                {"channelMode", ioMap->grandMasterChannelMode() == GrandMaster::AllChannels
                                    ? "allChannels" : "intensity"}
            }).dump();
            });
        }
    )
    .set_description("Configure the Grand Master: level, whether it limits or reduces, and which "
                     "channels it governs. Takes effect on live output immediately. NOTE this is "
                     "session state — QLC+ does not write the Grand Master into the project file, "
                     "so it resets on restart and is reset again by load_workspace and "
                     "new_workspace. For a Grand Master that is part of the saved show, create a "
                     "VC slider with mode \"grandMaster\".")
    .set_annotations(mcp::kAnnotIdempotent));

    // query_grand_master
    tm.register_tool(Tool(
        "query_grand_master",
        Json{{"type", "object"}, {"properties", Json::object()}},
        Json{},
        [doc](const Json &) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            InputOutputMap *ioMap = doc->inputOutputMap();
            return Json({
                {"value", (int)ioMap->grandMasterValue()},
                {"valueMode", ioMap->grandMasterValueMode() == GrandMaster::Limit ? "limit" : "reduce"},
                {"channelMode", ioMap->grandMasterChannelMode() == GrandMaster::AllChannels
                                    ? "allChannels" : "intensity"}
            }).dump();
            });
        }
    )
    .set_description("Read the current Grand Master level and modes.")
    .set_annotations(mcp::kAnnotReadOnly));

    // write_dmx — drive raw channels to verify a patch
    tm.register_tool(Tool(
        "write_dmx",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"fixtureID", {{"type", "integer"}, {"minimum", 0}}},
                {"channel", {{"type", "integer"}, {"minimum", 0},
                             {"description", "Channel index within the fixture"}}},
                {"value", {{"type", "integer"}, {"minimum", 0}, {"maximum", 255}}}
            }}, {"required", {"fixtureID", "channel", "value"}}}}}},
            {"release", {{"type", "boolean"},
                         {"description", "Release every channel this tool is holding, then stop. "
                                         "Use when patch checking is finished."}}}
        }}},
        Json{},
        [doc, source](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto err = validateFields(args, {"items", "release"});
            if (!err.empty()) return err;
            if (args.contains("release") && !args.at("release").is_boolean())
                return Json({{"error", "release must be a boolean"}}).dump();

            if (args.value("release", false))
            {
                if (args.contains("items"))
                    return Json({{"error", "pass either items or release, not both"}}).dump();
                source->unsetAll();
                return Json({{"status", "released"}}).dump();
            }

            auto itemsErr = validateItemsArray(args);
            if (itemsErr) return *itemsErr;

            Json results = Json::array();
            const Json &items = args.at("items");
            for (size_t i = 0; i < items.size(); ++i)
            {
                const Json &item = items[i];
                auto itemErr = validateFields(item, {"fixtureID", "channel", "value"});
                if (!itemErr.empty()) { results.push_back(mcp::itemErrorFromDump(i, itemErr)); continue; }

                if (!item.is_object() || !item.contains("fixtureID") || !item.contains("channel") || !item.contains("value"))
                { results.push_back(mcp::itemError(i, "fixtureID, channel and value are required")); continue; }
                const auto fxOpt = mcp::jsonInteger(item.at("fixtureID"), 0, mcp::kMaxId);
                const auto chOpt = mcp::jsonInteger(item.at("channel"), 0, mcp::kMaxId);
                const auto valOpt = mcp::jsonInteger(item.at("value"), 0, 255);
                if (!fxOpt) { results.push_back(mcp::itemError(i, mcp::integerError("fixtureID", 0, mcp::kMaxId))); continue; }
                if (!chOpt) { results.push_back(mcp::itemError(i, mcp::integerError("channel", 0, mcp::kMaxId))); continue; }
                if (!valOpt) { results.push_back(mcp::itemError(i, "value must be from 0 to 255")); continue; }

                const quint32 fxID = quint32(*fxOpt);
                const quint32 channel = quint32(*chOpt);
                const int value = int(*valOpt);

                Fixture *fixture = doc->fixture(fxID);
                if (fixture == NULL)
                {
                    Json e = mcp::itemError(i, "fixture not found");
                    e["fixtureID"] = fxID;
                    results.push_back(e);
                    continue;
                }
                if (channel >= fixture->channels())
                {
                    Json e = mcp::itemError(i, "channel out of range for fixture");
                    e["fixtureID"] = fxID;
                    e["channel"] = channel;
                    e["channels"] = (int)fixture->channels();
                    results.push_back(e);
                    continue;
                }

                source->set(fxID, channel, (uchar)value);
                results.push_back(mcp::itemOk(i, {{"fixtureID", fxID}, {"channel", channel}, {"value", value}}));
            }
            return results.dump();
            });
        }
    )
    .set_description("Drive fixture channels directly to verify a patch — the setup question "
                     "\"is this fixture really at this address?\". Values are held until changed or "
                     "released with {\"release\": true}, and a running Function or the Grand Master "
                     "can still override them. Not a playback mechanism: use Scripts for timed "
                     "sequences.")
    .set_annotations(mcp::kAnnotOpenWorld));

    // set_blackout
    tm.register_tool(Tool(
        "set_blackout",
        Json{{"type", "object"}, {"properties", {
            {"enabled", {{"type", "boolean"}, {"description", "true to black out all output"}}}
        }}, {"required", {"enabled"}}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto err = validateFields(args, {"enabled"});
            if (!err.empty()) return err;
            if (!args.contains("enabled") || !args.at("enabled").is_boolean())
                return Json({{"error", "enabled is required and must be a boolean"}}).dump();

            InputOutputMap *ioMap = doc->inputOutputMap();
            ioMap->setBlackout(args.at("enabled").get<bool>());
            return Json({{"status", "ok"}, {"blackout", ioMap->blackout()}}).dump();
            });
        }
    )
    .set_description("Turn global blackout on or off. Useful while patching so a rig under test "
                     "stays dark. Blackout is not saved with the project.")
    .set_annotations(mcp::kAnnotOpenWorld));

    // run_functions — start or stop a function to verify what was authored
    tm.register_tool(Tool(
        "run_functions",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"functionID", {{"type", "integer"}, {"minimum", 0}}},
                {"action", {{"type", "string"}, {"enum", {"start", "stop"}}}}
            }}, {"required", {"functionID", "action"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto err = validateFields(args, {"items"});
            if (!err.empty()) return err;
            auto itemsErr = validateItemsArray(args);
            if (itemsErr) return *itemsErr;

            static const Json kEnums = {{"action", {{"enum", {"start", "stop"}}}}};

            Json results = Json::array();
            const Json &items = args.at("items");
            for (size_t i = 0; i < items.size(); ++i)
            {
                const Json &item = items[i];
                auto err = validateFields(item, {"functionID", "action"});
                if (!err.empty()) { results.push_back(mcp::itemErrorFromDump(i, err)); continue; }
                auto enumErr = validateEnums(item, kEnums);
                if (!enumErr.empty()) { results.push_back(mcp::itemErrorFromDump(i, enumErr)); continue; }

                const auto fnOpt = item.is_object() && item.contains("functionID")
                    ? mcp::jsonInteger(item.at("functionID"), 0, mcp::kMaxId) : std::nullopt;
                if (!fnOpt)
                { results.push_back(mcp::itemError(i, "functionID is required and must be an integer in [0, 4294967294]")); continue; }
                if (!item.contains("action") || !item.at("action").is_string())
                { results.push_back(mcp::itemError(i, "action is required and must be a string")); continue; }

                const quint32 fnID = quint32(*fnOpt);
                Function *function = doc->function(fnID);
                if (function == NULL)
                {
                    Json e = mcp::itemError(i, "function not found");
                    e["functionID"] = fnID;
                    results.push_back(e);
                    continue;
                }

                const std::string action = toLowerStd(item.at("action").get<std::string>());
                if (action == "start")
                    function->start(doc->masterTimer(), FunctionParent::master());
                else
                    function->stop(FunctionParent::master());

                results.push_back(mcp::itemOk(i, {{"functionID", fnID},
                                   {"name", function->name().toStdString()},
                                   {"action", action}}));
            }
            return results.dump();
            });
        }
    )
    .set_description("Start or stop functions, to check that something just authored actually does "
                     "what was intended. Starting is immediate and honours the function's own "
                     "timing. Both act as the master owner: starting an already-running function "
                     "adds that ownership (so its original owner, e.g. a VC button, can no longer "
                     "stop it), and stopping force-stops it regardless of who started it. This is "
                     "a verification aid, not a cue engine — there is no stepping, no timed "
                     "playback and no fade override; build those as Scripts.")
    .set_annotations(mcp::kAnnotOpenWorld));

    // query_running_functions
    tm.register_tool(Tool(
        "query_running_functions",
        Json{{"type", "object"}, {"properties", Json::object()}},
        Json{},
        [doc](const Json &) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            MasterTimer *timer = doc->masterTimer();
            if (timer == NULL)
                return results.dump();

            for (quint32 id : timer->runningFunctionIds())
            {
                Function *function = doc->function(id);
                results.push_back({
                    {"functionID", (int)id},
                    {"name", function ? function->name().toStdString() : std::string()},
                    {"type", function ? function->typeString().toStdString() : std::string()}
                });
            }
            return results.dump();
            });
        }
    )
    .set_description("List the functions currently running, with their IDs, names and types.")
    .set_annotations(mcp::kAnnotReadOnly));
}
