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
                {"inputPlugin", {{"type", "string"}}},
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
            Json results = Json::array();
            InputOutputMap *ioMap = doc->inputOutputMap();
            for (auto &item : args.at("items"))
            {
                auto err = validateFields(item, {"universeID", "name", "inputPlugin", "inputLine", "outputPlugin", "outputLine", "passthrough", "feedbackEnabled"});
                if (!err.empty()) { results.push_back(Json::parse(err)); continue; }

                int uid = item.at("universeID").get<int>();
                bool ok = true;

                if (item.contains("name"))
                {
                    Universe *uni = ioMap->universe(uid);
                    if (uni) uni->setName(QString::fromStdString(item.at("name").get<std::string>()));
                }
                if (item.contains("inputPlugin") && item.contains("inputLine"))
                {
                    ok &= ioMap->setInputPatch(uid,
                        QString::fromStdString(item.at("inputPlugin").get<std::string>()),
                        QString(), item.at("inputLine").get<int>());
                }
                if (item.contains("outputPlugin") && item.contains("outputLine"))
                {
                    ok &= ioMap->setOutputPatch(uid,
                        QString::fromStdString(item.at("outputPlugin").get<std::string>()),
                        QString(), item.at("outputLine").get<int>());
                }
                if (item.contains("passthrough"))
                {
                    Universe *uni = ioMap->universe(uid);
                    if (uni) uni->setPassthrough(item.at("passthrough").get<bool>());
                }
                if (item.contains("feedbackEnabled") && item.at("feedbackEnabled").get<bool>())
                {
                    InputPatch *inPatch = ioMap->inputPatch(uid);
                    if (inPatch && inPatch->isPatched())
                    {
                        ok &= ioMap->setOutputPatch(uid, inPatch->pluginName(), "", inPatch->input(), true);
                    }
                }

                results.push_back({{"universeID", uid}, {"status", ok ? "ok" : "failed"}});
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("Configure universe input/output plugins (OSC, ArtNet, E1.31, etc.). Batch."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotOpenWorld));

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
            Json results = Json::array();
            for (auto &item : args.at("items"))
            {
                auto err = validateFields(item, {"universeID", "plugin", "params"});
                if (!err.empty()) { results.push_back(Json::parse(err)); continue; }

                int uid = item.at("universeID").get<int>();
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

                results.push_back({{"universeID", uid}, {"status", "ok"}});
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("Set plugin-specific parameters (e.g., MIDI init message, channel). Batch."),
        std::nullopt
    )
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
        },
        std::nullopt,
        std::string("List connected MIDI devices with their input/output ports."),
        std::nullopt
    )
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
        },
        std::nullopt,
        std::string("List available input profiles (e.g., Novation Launchpad Mini MK3)."),
        std::nullopt
    )
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
            Json results = Json::array();
            for (auto &item : args.at("items"))
            {
                auto err = validateFields(item, {"universeID", "profileName"});
                if (!err.empty()) { results.push_back(Json::parse(err)); continue; }

                int uid = item.at("universeID").get<int>();
                QString profName = QString::fromStdString(item.at("profileName").get<std::string>());
                bool ok = doc->inputOutputMap()->setInputProfile(uid, profName);
                results.push_back({{"universeID", uid}, {"status", ok ? "ok" : "failed"}});
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("Set input profile for a universe. Batch."),
        std::nullopt
    )
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
                int uid = args.at("universeID").get<int>();
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
        },
        std::nullopt,
        std::string("Get the color table and MIDI channel table from an input profile. "
                     "Accepts profileName or universeID (to use the active profile on that universe). "
                     "Returns available LED colors (velocity values) and animation modes "
                     "for use with feedback configuration."),
        std::nullopt
    )
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

            for (auto &item : args.at("items"))
            {
                auto err = validateFields(item, {"universeID", "inputEnabled", "inputPort", "outputEnabled", "outputIP", "outputPort", "feedbackEnabled", "feedbackIP", "feedbackPort"});
                if (!err.empty()) { results.push_back(Json::parse(err)); continue; }

                int uid = item.at("universeID").get<int>();
                Json result;
                result["universeID"] = uid;
                bool ok = true;

                // Patch OSC as input
                bool inputEnabled = item.value("inputEnabled", true);
                if (inputEnabled)
                {
                    quint32 line = 0;
                    QStringList inputs = oscPlugin->inputs();
                    if (!inputs.isEmpty())
                        ok &= ioMap->setInputPatch(uid, "OSC", "", line);
                }

                // Patch OSC as output
                bool outputEnabled = item.value("outputEnabled", false);
                if (outputEnabled)
                {
                    quint32 line = 0;
                    QStringList outputs = oscPlugin->outputs();
                    if (!outputs.isEmpty())
                        ok &= ioMap->setOutputPatch(uid, "OSC", "", line, false);
                }

                // Enable feedback
                if (item.value("feedbackEnabled", false))
                {
                    InputPatch *inPatch = ioMap->inputPatch(uid);
                    if (inPatch && inPatch->isPatched())
                        ok &= ioMap->setOutputPatch(uid, inPatch->pluginName(), "", inPatch->input(), true);
                }

                // Set plugin parameters
                InputPatch *inPatch = ioMap->inputPatch(uid);
                OutputPatch *outPatch = ioMap->outputPatch(uid);
                quint32 line = inPatch ? inPatch->input() : (outPatch ? outPatch->output() : 0);

                if (item.contains("inputPort"))
                    oscPlugin->setParameter(uid, line, QLCIOPlugin::Input,
                        "inputPort", QVariant(item.at("inputPort").get<int>()));
                if (item.contains("outputIP"))
                    oscPlugin->setParameter(uid, line, QLCIOPlugin::Output,
                        "outputIP", QVariant(QString::fromStdString(item.at("outputIP").get<std::string>())));
                if (item.contains("outputPort"))
                    oscPlugin->setParameter(uid, line, QLCIOPlugin::Output,
                        "outputPort", QVariant(item.at("outputPort").get<int>()));
                if (item.contains("feedbackIP"))
                    oscPlugin->setParameter(uid, line, QLCIOPlugin::Output,
                        "feedbackIP", QVariant(QString::fromStdString(item.at("feedbackIP").get<std::string>())));
                if (item.contains("feedbackPort"))
                    oscPlugin->setParameter(uid, line, QLCIOPlugin::Output,
                        "feedbackPort", QVariant(item.at("feedbackPort").get<int>()));

                result["status"] = ok ? "ok" : "failed";
                results.push_back(result);
            }
            return results.dump();
            } catch (const std::exception &e) {
                return Json({{"error", e.what()}}).dump();
            }
            });
        },
        std::nullopt,
        std::string("Configure OSC plugin for a universe in one call. Sets input/output/feedback ports and addresses. Batch."),
        std::nullopt
    )
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
        },
        std::nullopt,
        std::string("Show current OSC configuration for all universes that have OSC patched."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotReadOnly));

    // configure_beat_source — set beat generator type and optional BPM
    tm.register_tool(Tool(
        "configure_beat_source",
        Json{{"type", "object"}, {"properties", {
            {"type", {{"type", "string"}, {"description", "Beat source: disabled, internal, plugin (OS2L/MIDI), audio"}}},
            {"bpm", {{"type", "integer"}, {"description", "BPM value (only for internal, default 120)"}}}
        }}, {"required", {"type"}}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto err = validateFields(args, {"type", "bpm"});
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

            ioMap->setBeatGeneratorType(beatType);

            if (beatType == InputOutputMap::Internal)
            {
                int bpm = args.value("bpm", 120);
                ioMap->setBpmNumber(bpm);
            }

            Json result;
            result["beatSource"] = typeStr;
            result["bpm"] = ioMap->bpmNumber();
            return result.dump();
            });
        },
        std::nullopt,
        std::string("Set beat generator source: disabled, internal (with BPM), plugin (OS2L/MIDI beat input), audio (mic/line-in beat detection)."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotOpenWorld));

    // set_grand_master — control grand master value and mode
    tm.register_tool(Tool(
        "set_grand_master",
        Json{{"type", "object"}, {"properties", {
            {"value", {{"type", "integer"}, {"description", "Grand master value 0-255"}}},
            {"valueMode", {{"type", "string"}, {"description", "'limit' or 'reduce'"}}},
            {"channelMode", {{"type", "string"}, {"description", "'intensity' or 'all'"}}}
        }}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto err = validateFields(args, {"value", "valueMode", "channelMode"});
            if (!err.empty()) return err;

            InputOutputMap *ioMap = doc->inputOutputMap();

            if (args.contains("value"))
            {
                int val = args.at("value").get<int>();
                if (val < 0) val = 0;
                if (val > 255) val = 255;
                ioMap->setGrandMasterValue(static_cast<uchar>(val));
            }

            if (args.contains("valueMode"))
            {
                std::string mode = args.at("valueMode").get<std::string>();
                if (mode == "limit")
                    ioMap->setGrandMasterValueMode(GrandMaster::Limit);
                else if (mode == "reduce")
                    ioMap->setGrandMasterValueMode(GrandMaster::Reduce);
            }

            if (args.contains("channelMode"))
            {
                std::string mode = args.at("channelMode").get<std::string>();
                if (mode == "intensity")
                    ioMap->setGrandMasterChannelMode(GrandMaster::Intensity);
                else if (mode == "all")
                    ioMap->setGrandMasterChannelMode(GrandMaster::AllChannels);
            }

            Json result;
            result["value"] = (int)ioMap->grandMasterValue();
            result["valueMode"] = (ioMap->grandMasterValueMode() == GrandMaster::Limit) ? "limit" : "reduce";
            result["channelMode"] = (ioMap->grandMasterChannelMode() == GrandMaster::Intensity) ? "intensity" : "all";
            return result.dump();
            });
        },
        std::nullopt,
        std::string("Set grand master value, value mode, and channel mode."),
        std::nullopt
    )
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
                if (inNames[i].contains(model, Qt::CaseInsensitive) && inNames[i].contains("MIDI", Qt::CaseInsensitive))
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
                if (outNames[i].contains(model, Qt::CaseInsensitive) && outNames[i].contains("MIDI", Qt::CaseInsensitive))
                {
                    if (outputLine < 0)
                        outputLine = i;
                    else
                        outputLine = i; // overwrite with later (DAW) port
                }
            }

            if (inputLine < 0 && outputLine < 0)
                return Json({{"error", "Launchpad not found. Connect device and try again."}}).dump();

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
                ok &= ioMap->setInputPatch(universeID, "MIDI", "", inputLine);

            // Set output patch to MIDI DAW port
            if (outputLine >= 0)
                ok &= ioMap->setOutputPatch(universeID, "MIDI", "", outputLine, false);

            // Enable feedback on same port as input
            InputPatch *inPatch = ioMap->inputPatch(universeID);
            if (inPatch && inPatch->isPatched())
                ok &= ioMap->setOutputPatch(universeID, inPatch->pluginName(), "", inPatch->input(), true);

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
        },
        std::nullopt,
        std::string("Auto-configure a Novation Launchpad. Detects the device, sets DAW port, sends init message, sets input profile, enables LED feedback."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotOpenWorld));
}
