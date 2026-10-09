/*
  Q Light Controller Plus
  io_tools.cpp

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
#include "doc.h"
#include "fixture.h"
#include "inputoutputmap.h"
#include "inputpatch.h"
#include "outputpatch.h"
#include "universe.h"
#include "ioplugincache.h"
#include "qlcioplugin.h"
#include "qlcinputprofile.h"
#include "mastertimer.h"
#include "grandmaster.h"

#include <fastmcpp/tools/manager.hpp>
#include <map>
#include <fastmcpp/tools/tool.hpp>

void registerIOTools(fastmcpp::tools::ToolManager &tm, Doc *doc)
{
    using Json = nlohmann::json;
    using Tool = fastmcpp::tools::Tool;

    // configure_universes (batch)
    tm.register_tool(Tool(
        "configure_universes",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"universeID", {{"type", "integer"}}},
                {"name", {{"type", "string"}}},
                {"inputPlugin", {{"type", "string"}, {"description", "Input plugin name, or \"None\" to remove the universe's input patch (inputLine is then ignored). Unknown names are rejected."}}},
                {"inputLine", {{"type", "integer"}}},
                {"outputPlugin", {{"type", "string"}}},
                {"outputLine", {{"type", "integer"}}},
                {"passthrough", {{"type", "boolean"}}},
                {"feedbackEnabled", {{"type", "boolean"}, {"description", "Enable MIDI feedback on same port as input"}}}
            }}, {"required", {"universeID"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto itemsErr = validateItemsArray(args);
            if (itemsErr) return *itemsErr;
            Json results = Json::array();
            InputOutputMap *ioMap = doc->inputOutputMap();
            const Json &items = args.at("items");
            for (size_t i = 0; i < items.size(); ++i)
            {
                const Json &item = items[i];
                if (!item.is_object()) { results.push_back(mcp::itemError(i, "item must be an object")); continue; }
                auto err = validateFields(item, {"universeID", "name", "inputPlugin", "inputLine", "outputPlugin", "outputLine", "passthrough", "feedbackEnabled"});
                if (!err.empty()) { results.push_back(mcp::itemErrorFromDump(i, err)); continue; }

                // Preflight every field before any model or device change.
                auto uidOpt = item.contains("universeID") ? mcp::jsonInteger(item.at("universeID"), 0, 127) : std::nullopt;
                if (!uidOpt)
                {
                    results.push_back(mcp::itemError(i, "universeID must be an integer from 0 to 127"));
                    continue;
                }
                const int uid = int(*uidOpt);
                auto fail = [&](const std::string &msg) {
                    Json e = mcp::itemError(i, msg);
                    e["universeID"] = uid;
                    results.push_back(e);
                };

                std::string typeErr;
                for (const char *key : {"name", "inputPlugin", "outputPlugin"})
                    if (item.contains(key) && !item.at(key).is_string())
                        typeErr = std::string(key) + " must be a string";
                for (const char *key : {"passthrough", "feedbackEnabled"})
                    if (item.contains(key) && !item.at(key).is_boolean())
                        typeErr = std::string(key) + " must be a boolean";
                std::optional<int64_t> inputLine, outputLine;
                if (item.contains("inputLine") && !(inputLine = mcp::jsonInteger(item.at("inputLine"), -1, INT32_MAX)))
                    typeErr = mcp::integerError("inputLine", -1, INT32_MAX);
                if (item.contains("outputLine") && !(outputLine = mcp::jsonInteger(item.at("outputLine"), -1, INT32_MAX)))
                    typeErr = mcp::integerError("outputLine", -1, INT32_MAX);
                if (!typeErr.empty()) { fail(typeErr); continue; }

                if (item.contains("outputPlugin") != item.contains("outputLine"))
                {
                    fail("outputPlugin and outputLine must be given together");
                    continue;
                }
                if (item.contains("inputLine") && !item.contains("inputPlugin"))
                {
                    fail("inputLine requires inputPlugin");
                    continue;
                }

                QString inputPlugin;
                bool removeInput = false;
                if (item.contains("inputPlugin"))
                {
                    inputPlugin = QString::fromStdString(item.at("inputPlugin").get<std::string>());
                    removeInput = (inputPlugin == "None" || inputPlugin == KInputNone);
                    if (!removeInput && doc->ioPluginCache()->plugin(inputPlugin) == nullptr)
                    {
                        fail("unknown input plugin: " + inputPlugin.toStdString());
                        continue;
                    }
                    if (!removeInput && !inputLine)
                    {
                        fail("inputLine is required with inputPlugin");
                        continue;
                    }
                }
                auto toLine = [](int64_t line) {
                    return line < 0 ? QLCIOPlugin::invalidLine() : quint32(line);
                };

                bool ok = true;

                // Grow the universe list on demand. addUniverse() fills any gap
                // between the current count and the requested id by itself, so a
                // single call is enough to make `uid` addressable.
                int created = 0;
                if (uid >= (int)ioMap->universesCount())
                {
                    int before = (int)ioMap->universesCount();
                    if (ioMap->addUniverse((quint32)uid) == false)
                    {
                        fail("could not create universe");
                        continue;
                    }
                    created = (int)ioMap->universesCount() - before;

                    // addUniverse() creates the Universe but never starts its
                    // writer thread, so a universe added at runtime stays silent
                    // until the project is reloaded. start() on an already
                    // running Universe is a no-op.
                    ioMap->startUniverses();
                }

                if (item.contains("name"))
                {
                    Universe *uni = ioMap->universe(uid);
                    if (uni) uni->setName(QString::fromStdString(item.at("name").get<std::string>()));
                }
                if (removeInput)
                {
                    ok &= ioMap->setInputPatch(uid, KInputNone, QString(), QString(),
                                               QLCIOPlugin::invalidLine());
                }
                else if (item.contains("inputPlugin"))
                {
                    ok &= ioMap->setInputPatch(uid, inputPlugin, QString(), QString(), toLine(*inputLine));
                }
                if (item.contains("outputPlugin"))
                {
                    ok &= ioMap->setOutputPatch(uid,
                        QString::fromStdString(item.at("outputPlugin").get<std::string>()),
                        QString(), QString(), toLine(*outputLine));
                }
                if (item.contains("passthrough"))
                {
                    Universe *uni = ioMap->universe(uid);
                    if (uni) uni->setPassthrough(item.at("passthrough").get<bool>());
                }
                if (item.contains("feedbackEnabled"))
                {
                    if (item.at("feedbackEnabled").get<bool>())
                    {
                        InputPatch *inPatch = ioMap->inputPatch(uid);
                        if (inPatch && inPatch->isPatched())
                            ok &= ioMap->setOutputPatch(uid, inPatch->pluginName(), "", "", inPatch->input(), true);
                        else
                            ok = false;
                    }
                    else if (ioMap->feedbackPatch(uid) != nullptr)
                    {
                        ok &= ioMap->setOutputPatch(uid, KOutputNone, "", "", QLCIOPlugin::invalidLine(), true);
                    }
                }
                doc->setModified();

                // Patch setters talk to plugins/devices; a false return means at
                // least one requested patch was not confirmed. Model edits above
                // (universe creation, name, passthrough) are not rolled back.
                Json entry = ok ? mcp::itemOk(i)
                                : mcp::itemError(i, "one or more patch operations were not confirmed by the plugin");
                entry["universeID"] = uid;
                if (created > 0)
                    entry["universesCreated"] = created;
                results.push_back(entry);
            }
            return results.dump();
            });
        }
    )
    .set_description("Configure universe input/output plugins (OSC, ArtNet, E1.31, etc.). Batch. "
                     "Wrap multiple operations in {\"items\": [...]}. Each item is processed independently. "
                     "A universeID beyond the current universe count creates the missing universes, "
                     "so this also serves as \"add universe\"; the reply reports universesCreated.")
    .set_annotations(mcp::kAnnotOpenWorld));

    // delete_universes — remove trailing universes
    tm.register_tool(Tool(
        "delete_universes",
        Json{{"type", "object"}, {"properties", {
            {"ids", {{"type", "array"}, {"items", {{"type", "integer"}}},
                     {"description", "Universe IDs to delete. Only trailing universes can be removed — "
                                     "the engine refuses to leave a gap in the universe list."}}}
        }}, {"required", {"ids"}}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto err = validateFields(args, {"ids"});
            if (!err.empty()) return err;
            if (!args.contains("ids") || !args.at("ids").is_array())
                return Json({{"error", "ids must be an array of integers"}}).dump();

            InputOutputMap *ioMap = doc->inputOutputMap();

                        // Parse per input index; a malformed id fails only its own item.
            const Json &input = args.at("ids");
            std::vector<std::optional<int>> parsedIds;
            std::map<int, size_t> firstIndex;
            for (size_t i = 0; i < input.size(); ++i)
            {
                auto id = mcp::jsonInteger(input[i], 0, mcp::kMaxId);
                parsedIds.push_back(id ? std::optional<int>(int(std::min<std::int64_t>(
                                              *id, std::numeric_limits<int>::max())))
                                       : std::nullopt);
                if (parsedIds.back())
                    firstIndex.emplace(*parsedIds.back(), i);
            }

            // Highest id first: removing the tail one at a time is the only order
            // the engine accepts, and it lets a batch like [2,3] succeed.
            std::map<int, Json> outcomes;
            for (auto it = firstIndex.rbegin(); it != firstIndex.rend(); ++it)
            {
                const int uid = it->first;
                Json &out = outcomes[uid];
                if (uid >= (int)ioMap->universesCount())
                {
                    out = {{"universeID", uid}, {"error", "universe not found"}};
                    continue;
                }
                if (ioMap->universesCount() == 1)
                {
                    out = {{"universeID", uid},
                           {"error", "cannot delete the last remaining universe"}};
                    continue;
                }

                // A cross-universe fixture occupies channels past the end of its
                // own universe, so compare the whole footprint rather than just
                // Fixture::universe().
                QList<quint32> patched;
                for (Fixture *fxi : doc->fixtures())
                {
                    if (fxi == NULL)
                        continue;
                    const int first = (int)fxi->universe();
                    const int last = fxi->channels() > 0
                        ? (int)((fxi->universeAddress() + fxi->channels() - 1) >> 9)
                        : first;
                    if (uid >= first && uid <= last)
                        patched.append(fxi->id());
                }

                if (!patched.isEmpty())
                {
                    Json ids_ = Json::array();
                    for (quint32 fid : patched) ids_.push_back((int)fid);
                    out = {{"universeID", uid},
                           {"error", "universe has patched fixtures — delete them first"},
                           {"fixtureIDs", ids_}};
                    continue;
                }

                if (uid != (int)ioMap->universesCount() - 1)
                {
                    out = {{"universeID", uid},
                           {"error", "only the last universe can be deleted — "
                                     "removing this one would leave a gap"}};
                    continue;
                }

                if (ioMap->removeUniverse(uid))
                {
                    doc->setModified();
                    out = {{"universeID", uid}, {"outcome", "deleted"}};
                }
                else
                {
                    out = {{"universeID", uid}, {"error", "could not delete universe"}};
                }
            }

            Json results = Json::array();
            for (size_t i = 0; i < parsedIds.size(); ++i)
            {
                if (!parsedIds[i])
                {
                    results.push_back(mcp::itemError(i, mcp::integerError("ids[]", 0, mcp::kMaxId)));
                    continue;
                }
                Json out = outcomes.at(*parsedIds[i]);
                const size_t first = firstIndex.at(*parsedIds[i]);
                if (first != i)
                {
                    // Repeats never claim a second physical deletion.
                    out["duplicateOf"] = first;
                    if (!out.contains("error"))
                        out["outcome"] = "duplicate";
                }
                if (out.contains("error"))
                {
                    out["index"] = i;
                    out["status"] = "error";
                    results.push_back(out);
                }
                else
                    results.push_back(mcp::itemOk(i, out));
            }
            return results.dump();
            });
        }
    )
    .set_description("Delete universes by ID. Batch: {\"ids\": [...]}. Only trailing universes can be "
                     "removed and a universe holding patched fixtures is refused. The last remaining "
                     "universe cannot be deleted. Returns one record per input id, in input order; "
                     "deletion runs highest id first, and a repeated id reports outcome \"duplicate\" "
                     "with duplicateOf instead of a second deletion.")
    .set_annotations(mcp::kAnnotDestructive));

    // query_midi_devices — list connected MIDI input/output ports
    tm.register_tool(Tool(
        "configure_plugin_params",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"universeID", {{"type", "integer"}}},
                {"plugin", {{"type", "string"}}},
                {"params", {{"type", "object"}, {"description", "Key-value parameters (e.g., initmessage, midichannel)"}}}
            }}, {"required", {"universeID", "plugin", "params"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto itemsErr = validateItemsArray(args);
            if (itemsErr) return *itemsErr;
            Json results = Json::array();
            for (auto &item : args.at("items"))
            {
                auto err = validateFields(item, {"universeID", "plugin", "params"});
                if (!err.empty()) { results.push_back(Json::parse(err)); continue; }
                auto uidOpt = item.contains("universeID")
                    ? mcp::jsonInteger(item.at("universeID"), 0, 127) : std::nullopt;
                if (!uidOpt) { results.push_back({{"error", mcp::integerError("universeID", 0, 127)}}); continue; }
                if (!item.contains("plugin") || !item.at("plugin").is_string())
                { results.push_back({{"error", "plugin must be a string"}}); continue; }
                bool stringParams = item.contains("params") && item.at("params").is_object();
                if (stringParams)
                    for (auto &[key, value] : item.at("params").items())
                        stringParams = stringParams && value.is_string();
                if (!stringParams)
                { results.push_back({{"error", "params must be an object of string values"}}); continue; }

                int uid = int(*uidOpt);
                QString pluginName = QString::fromStdString(item.at("plugin").get<std::string>());

                // Find the plugin
                QLCIOPlugin *plugin = nullptr;
                for (QLCIOPlugin *p : doc->ioPluginCache()->plugins())
                {
                    if (p->name() == pluginName)
                    {
                        plugin = p;
                        break;
                    }
                }

                if (!plugin)
                {
                    results.push_back({{"universeID", uid}, {"error", "plugin not found"}});
                    continue;
                }

                // Get the output line from the universe's output or feedback patch
                InputOutputMap *ioMap = doc->inputOutputMap();
                quint32 line = QLCIOPlugin::invalidLine();

                // Try feedback patch first, then output patch, then input patch
                Universe *uni = ioMap->universe(uid);
                if (uni)
                {
                    OutputPatch *fbPatch = uni->feedbackPatch();
                    if (fbPatch && fbPatch->isPatched())
                        line = fbPatch->output();
                    else
                    {
                        OutputPatch *outPatch = uni->outputPatch(0);
                        if (outPatch && outPatch->isPatched())
                            line = outPatch->output();
                        else
                        {
                            InputPatch *inPatch = uni->inputPatch();
                            if (inPatch && inPatch->isPatched())
                                line = inPatch->input();
                        }
                    }
                }

                // Set each parameter
                for (auto &[key, value] : item.at("params").items())
                {
                    QString qKey = QString::fromStdString(key);
                    QString qValue = QString::fromStdString(value.get<std::string>());
                    plugin->setParameter(uid, line, QLCIOPlugin::Output, qKey, qValue);
                }

                // setParameter returns void: the device write is attempted, not confirmed.
                results.push_back({{"universeID", uid}, {"status", "ok"}, {"effect", "attempted"}});
            }
            return mcp::indexedRecords(results).dump();
            });
        }
    )
    .set_description("Set plugin-specific parameters (e.g., MIDI init message, channel). Batch. "
                     "Wrap multiple operations in {\"items\": [...]}. Each item is processed independently.")
    .set_annotations(mcp::kAnnotOpenWorld));

    // query_midi_devices — list connected MIDI input/output ports
    tm.register_tool(Tool(
        "query_midi_devices",
        Json{{"type", "object"}, {"properties", Json::object()}},
        Json{},
        [doc](const Json &) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            QList<QLCIOPlugin *> plugins = doc->ioPluginCache()->plugins();
            for (QLCIOPlugin *plugin : plugins)
            {
                Json pluginEntry;
                pluginEntry["plugin"] = plugin->name().toStdString();
                Json inputLines = Json::array();
                QStringList inNames = plugin->inputs();
                for (int i = 0; i < inNames.count(); i++)
                    inputLines.push_back({{"line", i}, {"name", inNames[i].toStdString()}});
                Json outputLines = Json::array();
                QStringList outNames = plugin->outputs();
                for (int i = 0; i < outNames.count(); i++)
                    outputLines.push_back({{"line", i}, {"name", outNames[i].toStdString()}});
                pluginEntry["inputs"] = inputLines;
                pluginEntry["outputs"] = outputLines;
                results.push_back(pluginEntry);
            }
            return results.dump();
            });
        }
    )
    .set_description("List connected MIDI devices with their input/output ports.")
    .set_annotations(mcp::kAnnotReadOnly));

    // query_input_profiles — list available input profiles
    tm.register_tool(Tool(
        "query_input_profiles",
        Json{{"type", "object"}, {"properties", Json::object()}},
        Json{},
        [doc](const Json &) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            InputOutputMap *ioMap = doc->inputOutputMap();
            for (const QString &name : ioMap->profileNames())
            {
                QLCInputProfile *prof = ioMap->profile(name);
                if (prof)
                {
                    results.push_back({
                        {"name", prof->name().toStdString()},
                        {"manufacturer", prof->manufacturer().toStdString()},
                        {"model", prof->model().toStdString()},
                        {"type", QLCInputProfile::typeToString(prof->type()).toStdString()}
                    });
                }
            }
            return results.dump();
            });
        }
    )
    .set_description("List available input profiles (e.g., Novation Launchpad Mini MK3).")
    .set_annotations(mcp::kAnnotReadOnly));

    // set_input_profile (batch)
    tm.register_tool(Tool(
        "set_input_profile",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"universeID", {{"type", "integer"}}},
                {"profileName", {{"type", "string"}}}
            }}, {"required", {"universeID", "profileName"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto itemsErr = validateItemsArray(args);
            if (itemsErr) return *itemsErr;
            Json results = Json::array();
            for (auto &item : args.at("items"))
            {
                auto err = validateFields(item, {"universeID", "profileName"});
                if (!err.empty()) { results.push_back(Json::parse(err)); continue; }

                auto uidOpt = item.contains("universeID")
                    ? mcp::jsonInteger(item.at("universeID"), 0, 127) : std::nullopt;
                if (!uidOpt) { results.push_back({{"error", mcp::integerError("universeID", 0, 127)}}); continue; }
                if (!item.contains("profileName") || !item.at("profileName").is_string())
                { results.push_back({{"error", "profileName must be a string"}}); continue; }
                int uid = int(*uidOpt);
                QString profName = QString::fromStdString(item.at("profileName").get<std::string>());
                InputOutputMap *ioMap = doc->inputOutputMap();
                if (profName != KInputNone && !ioMap->profile(profName))
                {
                    results.push_back({{"universeID", uid}, {"error", "input profile not found: " + profName.toStdString()}});
                    continue;
                }
                if (!ioMap->setInputProfile(uid, profName))
                {
                    results.push_back({{"universeID", uid}, {"error", "universe not found"}});
                    continue;
                }
                // The engine only stores a profile on an existing input patch.
                results.push_back({{"universeID", uid}, {"status", "ok"},
                                   {"applied", ioMap->inputPatch(uid) != nullptr}});
            }
            return mcp::indexedRecords(results).dump();
            });
        }
    )
    .set_description("Set input profile for a universe. Batch. "
                     "Wrap multiple operations in {\"items\": [...]}. Each item is processed independently.")
    .set_annotations(mcp::kAnnotIdempotent));

    // query_feedback_profile — get color table and MIDI channel table from an input profile
    tm.register_tool(Tool(
        "query_feedback_profile",
        Json{{"type", "object"}, {"properties", {
            {"profileName", {{"type", "string"}, {"description", "Name of the input profile to query. Use query_input_profiles to list available profiles."}}},
            {"universeID", {{"type", "integer"}, {"description", "Universe ID to get the active input profile from. Alternative to profileName."}}}
        }}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto err = validateFields(args, {"profileName", "universeID"});
            if (!err.empty()) return err;

            InputOutputMap *ioMap = doc->inputOutputMap();
            QLCInputProfile *prof = nullptr;

            if (args.contains("profileName"))
            {
                QString profName = QString::fromStdString(args.at("profileName").get<std::string>());
                prof = ioMap->profile(profName);
                if (!prof)
                    return Json({{"error", "profile not found: " + profName.toStdString()}}).dump();
            }
            else if (args.contains("universeID"))
            {
                auto uidOpt = mcp::jsonInteger(args.at("universeID"), 0, 127);
                if (!uidOpt)
                    return Json({{"error", mcp::integerError("universeID", 0, 127)}}).dump();
                int uid = int(*uidOpt);
                InputPatch *inPatch = ioMap->inputPatch(uid);
                if (!inPatch || !inPatch->profile())
                    return Json({{"error", "no input profile set on universe " + std::to_string(uid)}}).dump();
                prof = inPatch->profile();
            }
            else
            {
                return Json({{"error", "either profileName or universeID is required"}}).dump();
            }

            Json result;
            result["profileName"] = prof->name().toStdString();

            // Color table
            result["hasColorTable"] = prof->hasColorTable();
            Json colorTable = Json::array();
            if (prof->hasColorTable())
            {
                QMapIterator<uchar, QPair<QString, QColor>> it(prof->colorTable());
                while (it.hasNext())
                {
                    it.next();
                    colorTable.push_back({
                        {"value", (int)it.key()},
                        {"label", it.value().first.toStdString()},
                        {"color", it.value().second.name().toStdString()}
                    });
                }
            }
            result["colorTable"] = colorTable;

            // MIDI channel table
            result["hasMidiChannelTable"] = prof->hasMidiChannelTable();
            Json midiChannelTable = Json::array();
            if (prof->hasMidiChannelTable())
            {
                QMapIterator<uchar, QString> it(prof->midiChannelTable());
                while (it.hasNext())
                {
                    it.next();
                    midiChannelTable.push_back({
                        {"value", (int)it.key()},
                        {"label", it.value().toStdString()}
                    });
                }
            }
            result["midiChannelTable"] = midiChannelTable;

            return result.dump();
            });
        }
    )
    .set_description("Get the color table and MIDI channel table from an input profile. "
                     "Accepts profileName or universeID (to use the active profile on that universe). "
                     "Returns available LED colors (velocity values) and animation modes "
                     "for use with feedback configuration.")
    .set_annotations(mcp::kAnnotReadOnly));

    // configure_osc — one-call OSC plugin setup per universe
    tm.register_tool(Tool(
        "configure_osc",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"universeID", {{"type", "integer"}}},
                {"inputPort", {{"type", "integer"}, {"description", "OSC listening port (default 7700+universe)"}}},
                {"outputIP", {{"type", "string"}, {"description", "IP to send OSC output to"}}},
                {"outputPort", {{"type", "integer"}, {"description", "Port to send OSC output to (default 9000+universe)"}}},
                {"feedbackIP", {{"type", "string"}, {"description", "IP to send OSC feedback to"}}},
                {"feedbackPort", {{"type", "integer"}, {"description", "Port to send OSC feedback to"}}},
                {"inputEnabled", {{"type", "boolean"}, {"description", "Patch OSC as input (default true)"}}},
                {"outputEnabled", {{"type", "boolean"}, {"description", "Patch OSC as output (default false)"}}},
                {"feedbackEnabled", {{"type", "boolean"}, {"description", "Enable feedback (default false)"}}}
            }}, {"required", {"universeID"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            try {
            auto itemsErr = validateItemsArray(args);
            if (itemsErr) return *itemsErr;
            const Json &items = args.at("items");
            InputOutputMap *ioMap = doc->inputOutputMap();

            // Preflight every item before any patch or plugin effect.
            struct OscItem { int uid; QVariantMap input, output; bool in, out, fb; };
            std::vector<std::optional<OscItem>> parsedItems;
            Json results = Json::array();
            for (size_t i = 0; i < items.size(); ++i)
            {
                const Json &item = items[i];
                parsedItems.push_back(std::nullopt);
                results.push_back(nullptr);
                auto err = validateFields(item, {"universeID", "inputEnabled", "inputPort", "outputEnabled", "outputIP", "outputPort", "feedbackEnabled", "feedbackIP", "feedbackPort"});
                if (!err.empty()) { results[i] = mcp::itemErrorFromDump(i, err); continue; }

                const int universes = int(ioMap->universesCount());
                auto uid = item.contains("universeID")
                    ? mcp::jsonInteger(item.at("universeID"), 0, universes - 1) : std::nullopt;
                if (!uid) { results[i] = mcp::itemError(i, mcp::integerError("universeID", 0, universes - 1)); continue; }

                OscItem parsed{int(*uid), {}, {}, true, false, false};
                std::string error;
                for (auto [key, target] : {std::pair<const char *, bool *>{"inputEnabled", &parsed.in},
                                           {"outputEnabled", &parsed.out}, {"feedbackEnabled", &parsed.fb}})
                {
                    if (!item.contains(key)) continue;
                    if (!item.at(key).is_boolean()) { error = std::string(key) + " must be a boolean"; break; }
                    *target = item.at(key).get<bool>();
                }
                for (const char *key : {"inputPort", "outputPort", "feedbackPort"})
                {
                    if (!error.empty() || !item.contains(key)) continue;
                    auto port = mcp::jsonInteger(item.at(key), 0, 65535);
                    if (!port) { error = mcp::integerError(key, 0, 65535); break; }
                    (std::string(key) == "inputPort" ? parsed.input : parsed.output)[key] = int(*port);
                }
                for (const char *key : {"outputIP", "feedbackIP"})
                {
                    if (!error.empty() || !item.contains(key)) continue;
                    if (!item.at(key).is_string()) { error = std::string(key) + " must be a string"; break; }
                    parsed.output[key] = QString::fromStdString(item.at(key).get<std::string>());
                }
                if (!error.empty()) { results[i] = mcp::itemError(i, error); continue; }
                parsedItems.back() = parsed;
            }

            // Find the OSC plugin
            QLCIOPlugin *oscPlugin = nullptr;
            for (QLCIOPlugin *plugin : doc->ioPluginCache()->plugins())
            {
                if (plugin->name() == "OSC")
                {
                    oscPlugin = plugin;
                    break;
                }
            }

            for (size_t i = 0; i < parsedItems.size(); ++i)
            {
                if (!parsedItems[i]) continue;
                if (!oscPlugin) { results[i] = mcp::itemError(i, "OSC plugin not found"); continue; }
                const OscItem &item = *parsedItems[i];
                const int uid = item.uid;
                Json confirmed = Json::array();
                bool ok = true;
                auto patched = [&](bool applied, const char *what) {
                    ok &= applied;
                    if (applied) confirmed.push_back(what);
                };

                // Patch OSC as input
                if (item.in && !oscPlugin->inputs().isEmpty())
                    patched(ioMap->setInputPatch(uid, "OSC", "", "", 0), "inputPatch");

                // Patch OSC as output
                if (item.out && !oscPlugin->outputs().isEmpty())
                    patched(ioMap->setOutputPatch(uid, "OSC", "", "", 0, false), "outputPatch");

                // Enable feedback
                if (item.fb)
                {
                    InputPatch *inPatch = ioMap->inputPatch(uid);
                    if (inPatch && inPatch->isPatched())
                        patched(ioMap->setOutputPatch(uid, inPatch->pluginName(), "", "", inPatch->input(), true),
                                "feedbackPatch");
                }

                // Set plugin parameters; setParameter returns void, so these are
                // reported as attempted, never as confirmed.
                InputPatch *inPatch = ioMap->inputPatch(uid);
                OutputPatch *outPatch = ioMap->outputPatch(uid);
                quint32 line = inPatch ? inPatch->input() : (outPatch ? outPatch->output() : 0);
                Json attempted = Json::array();
                for (auto it = item.input.cbegin(); it != item.input.cend(); ++it)
                {
                    oscPlugin->setParameter(uid, line, QLCIOPlugin::Input, it.key(), it.value());
                    attempted.push_back(it.key().toStdString());
                }
                for (auto it = item.output.cbegin(); it != item.output.cend(); ++it)
                {
                    oscPlugin->setParameter(uid, line, QLCIOPlugin::Output, it.key(), it.value());
                    attempted.push_back(it.key().toStdString());
                }

                results[i] = {{"index", i}, {"universeID", uid}, {"status", ok ? "ok" : "partial"},
                              {"confirmed", confirmed}, {"attempted", attempted}};
                if (!ok)
                    results[i]["error"] = "one or more OSC patches could not be applied";
            }
            return results.dump();
            } catch (const std::exception &e) {
                return Json({{"error", e.what()}}).dump();
            }
            });
        }
    )
    .set_description("Configure OSC plugin for a universe in one call. Sets input/output/feedback ports and addresses. Batch. "
                     "Wrap multiple operations in {\"items\": [...]}. Each item is processed independently. "
                     "Each record lists confirmed patches and attempted plugin parameters (the plugin does not "
                     "confirm those); status is partial when a patch fails.")
    .set_annotations(mcp::kAnnotOpenWorld));

    // query_osc_status — show current OSC configuration
    tm.register_tool(Tool(
        "query_osc_status",
        Json{{"type", "object"}, {"properties", Json::object()}},
        Json{},
        [doc](const Json &) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            try {
            Json results = Json::array();
            InputOutputMap *ioMap = doc->inputOutputMap();

            // Find the OSC plugin
            QLCIOPlugin *oscPlugin = nullptr;
            for (QLCIOPlugin *plugin : doc->ioPluginCache()->plugins())
            {
                if (plugin->name() == "OSC")
                {
                    oscPlugin = plugin;
                    break;
                }
            }
            if (!oscPlugin)
                return Json({{"error", "OSC plugin not found"}}).dump();

            for (Universe *uni : ioMap->universes())
            {
                InputPatch *inPatch = ioMap->inputPatch(uni->id());
                OutputPatch *outPatch = ioMap->outputPatch(uni->id());

                bool hasOscInput = inPatch && inPatch->isPatched() && inPatch->pluginName() == "OSC";
                bool hasOscOutput = outPatch && outPatch->isPatched() && outPatch->pluginName() == "OSC";

                if (!hasOscInput && !hasOscOutput)
                    continue;

                Json entry;
                entry["universeID"] = (int)uni->id();
                entry["universeName"] = uni->name().toStdString();

                if (hasOscInput)
                {
                    quint32 line = inPatch->input();
                    entry["inputLine"] = (int)line;
                    QMap<QString, QVariant> params = oscPlugin->getParameters(uni->id(), line, QLCIOPlugin::Input);
                    if (params.contains("inputPort"))
                        entry["inputPort"] = params["inputPort"].toInt();
                }
                if (hasOscOutput)
                {
                    quint32 line = outPatch->output();
                    entry["outputLine"] = (int)line;
                    entry["hasFeedback"] = uni->hasFeedback();
                    QMap<QString, QVariant> params = oscPlugin->getParameters(uni->id(), line, QLCIOPlugin::Output);
                    if (params.contains("outputIP"))
                        entry["outputIP"] = params["outputIP"].toString().toStdString();
                    if (params.contains("outputPort"))
                        entry["outputPort"] = params["outputPort"].toInt();
                    if (params.contains("feedbackIP"))
                        entry["feedbackIP"] = params["feedbackIP"].toString().toStdString();
                    if (params.contains("feedbackPort"))
                        entry["feedbackPort"] = params["feedbackPort"].toInt();
                }

                results.push_back(entry);
            }
            return results.dump();
            } catch (const std::exception &e) {
                return Json({{"error", e.what()}}).dump();
            }
            });
        }
    )
    .set_description("Show current OSC configuration for all universes that have OSC patched.")
    .set_annotations(mcp::kAnnotReadOnly));

    // configure_beat_source — set beat generator type and optional BPM
    tm.register_tool(Tool(
        "configure_beat_source",
        Json{{"type", "object"}, {"properties", {
            {"type", {{"type", "string"}, {"enum", {"disabled", "internal", "plugin", "midi", "audio"}}, {"description", "Beat source: disabled, internal, plugin (OS2L/MIDI), audio"}}},
            {"bpm", {{"type", "integer"}, {"description", "BPM value (only for internal, default 120)"}}}
        }}, {"required", {"type"}}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto err = validateFields(args, {"type", "bpm"});
            if (!err.empty()) return err;

            static const Json kEnums = {
                {"type", {{"enum", {"disabled", "internal", "plugin", "midi", "audio"}}}}
            };
            err = validateEnums(args, kEnums);
            if (!err.empty()) return err;

            InputOutputMap *ioMap = doc->inputOutputMap();
            std::string typeStr = args.at("type").get<std::string>();

            InputOutputMap::BeatGeneratorType beatType;
            if (typeStr == "disabled")
                beatType = InputOutputMap::Disabled;
            else if (typeStr == "internal")
                beatType = InputOutputMap::Internal;
            else if (typeStr == "plugin")
                beatType = InputOutputMap::Plugin;
            else if (typeStr == "midi")
                beatType = InputOutputMap::Plugin;
            else if (typeStr == "audio")
                beatType = InputOutputMap::Audio;
            else
                return Json({{"error", "Invalid type. Use: disabled, internal, plugin, audio"}}).dump();

            auto bpm = args.contains("bpm")
                ? mcp::jsonInteger(args.at("bpm"), 1, std::numeric_limits<int>::max())
                : std::optional<std::int64_t>(120);
            if (!bpm)
                return Json({{"error", mcp::integerError("bpm", 1, std::numeric_limits<int>::max())}}).dump();

            ioMap->setBeatGeneratorType(beatType);

            if (beatType == InputOutputMap::Internal)
                ioMap->setBpmNumber(int(*bpm));

            Json result;
            result["beatSource"] = typeStr;
            result["bpm"] = ioMap->bpmNumber();
            return result.dump();
            });
        }
    )
    .set_description("Set beat generator source: disabled, internal (with BPM), plugin (OS2L/MIDI beat input), audio (mic/line-in beat detection).")
    .set_annotations(mcp::kAnnotOpenWorld));

    // configure_launchpad — auto-configure a Novation Launchpad in one call
    tm.register_tool(Tool(
        "configure_launchpad",
        Json{{"type", "object"}, {"properties", {
            {"model", {{"type", "string"}, {"description", "Launchpad model name, e.g. 'Launchpad Mini MK3'"}}}
        }}, {"required", {"model"}}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto err = validateFields(args, {"model"});
            if (!err.empty()) return err;

            QString model = QString::fromStdString(args.at("model").get<std::string>());
            InputOutputMap *ioMap = doc->inputOutputMap();

            // Find the MIDI plugin
            QLCIOPlugin *midiPlugin = nullptr;
            for (QLCIOPlugin *p : doc->ioPluginCache()->plugins())
            {
                if (p->name() == "MIDI")
                {
                    midiPlugin = p;
                    break;
                }
            }
            if (!midiPlugin)
                return Json({{"error", "MIDI plugin not found"}}).dump();

            // Scan for matching input/output lines
            // Select port 2 (line index 1) — the DAW port, NOT port 1 (MIDI port)
            int inputLine = -1;
            int outputLine = -1;
            QStringList inNames = midiPlugin->inputs();
            for (int i = 0; i < inNames.count(); i++)
            {
                if (inNames[i].contains(model, Qt::CaseInsensitive))
                {
                    // Prefer DAW port (second occurrence) over MIDI port (first)
                    if (inputLine < 0)
                        inputLine = i;
                    else
                        inputLine = i; // overwrite with later (DAW) port
                }
            }
            QStringList outNames = midiPlugin->outputs();
            for (int i = 0; i < outNames.count(); i++)
            {
                if (outNames[i].contains(model, Qt::CaseInsensitive))
                {
                    if (outputLine < 0)
                        outputLine = i;
                    else
                        outputLine = i; // overwrite with later (DAW) port
                }
            }

            if (inputLine < 0 && outputLine < 0)
            {
                Json portNames = Json::array();
                for (const QString &n : inNames) portNames.push_back(n.toStdString());
                for (const QString &n : outNames) portNames.push_back(n.toStdString());
                return Json({{"error", "Launchpad model '" + model.toStdString() + "' not found in available ports. Connect device and try again."},
                             {"availablePorts", portNames}}).dump();
            }

            // Find a free universe or use universe 0
            int universeID = -1;
            for (Universe *uni : ioMap->universes())
            {
                InputPatch *inP = ioMap->inputPatch(uni->id());
                OutputPatch *outP = ioMap->outputPatch(uni->id());
                bool inFree = !inP || !inP->isPatched();
                bool outFree = !outP || !outP->isPatched();
                if (inFree && outFree)
                {
                    universeID = (int)uni->id();
                    break;
                }
            }
            if (universeID < 0)
                universeID = 0;

            bool ok = true;

            // Set input patch to MIDI DAW port
            if (inputLine >= 0)
                ok &= ioMap->setInputPatch(universeID, "MIDI", "", "", inputLine);

            // Set output patch to MIDI DAW port
            if (outputLine >= 0)
                ok &= ioMap->setOutputPatch(universeID, "MIDI", "", "", outputLine, false);

            // Enable feedback on same port as input
            InputPatch *inPatch = ioMap->inputPatch(universeID);
            if (inPatch && inPatch->isPatched())
                ok &= ioMap->setOutputPatch(universeID, inPatch->pluginName(), "", "", inPatch->input(), true);

            // Set input profile matching the model
            for (const QString &profName : ioMap->profileNames())
            {
                QLCInputProfile *prof = ioMap->profile(profName);
                if (prof && prof->name().contains(model, Qt::CaseInsensitive))
                {
                    ioMap->setInputProfile(universeID, profName);
                    break;
                }
            }

            // Send Programmer Mode init message
            QString initMsg = "Novation " + model + " Developer Mode";
            quint32 line = inputLine >= 0 ? (quint32)inputLine : (quint32)outputLine;
            midiPlugin->setParameter(universeID, line, QLCIOPlugin::Output,
                "initmessage", initMsg);

            Json result;
            result["status"] = ok ? "ok" : "partial";
            result["universeID"] = universeID;
            result["inputLine"] = inputLine;
            result["outputLine"] = outputLine;
            result["model"] = model.toStdString();
            return result.dump();
            });
        }
    )
    .set_description("Auto-configure a Novation Launchpad. Detects the device, sets DAW port, sends init message, sets input profile, enables LED feedback.")
    .set_annotations(mcp::kAnnotOpenWorld));
}
