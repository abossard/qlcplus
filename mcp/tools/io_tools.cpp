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
            for (auto &item : args["items"])
            {
                int uid = item["universeID"].get<int>();
                bool ok = true;

                if (item.contains("name"))
                {
                    Universe *uni = ioMap->universe(uid);
                    if (uni) uni->setName(QString::fromStdString(item["name"].get<std::string>()));
                }
                if (item.contains("inputPlugin") && item.contains("inputLine"))
                {
                    ok &= ioMap->setInputPatch(uid,
                        QString::fromStdString(item["inputPlugin"].get<std::string>()),
                        QString(), item["inputLine"].get<int>());
                }
                if (item.contains("outputPlugin") && item.contains("outputLine"))
                {
                    ok &= ioMap->setOutputPatch(uid,
                        QString::fromStdString(item["outputPlugin"].get<std::string>()),
                        QString(), item["outputLine"].get<int>());
                }
                if (item.contains("passthrough"))
                {
                    Universe *uni = ioMap->universe(uid);
                    if (uni) uni->setPassthrough(item["passthrough"].get<bool>());
                }
                if (item.contains("feedbackEnabled") && item["feedbackEnabled"].get<bool>())
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
    ));

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
            for (auto &item : args["items"])
            {
                int uid = item["universeID"].get<int>();
                QString pluginName = QString::fromStdString(item["plugin"].get<std::string>());

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
                for (auto &[key, value] : item["params"].items())
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
    ));

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
    ));

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
    ));

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
            for (auto &item : args["items"])
            {
                int uid = item["universeID"].get<int>();
                QString profName = QString::fromStdString(item["profileName"].get<std::string>());
                bool ok = doc->inputOutputMap()->setInputProfile(uid, profName);
                results.push_back({{"universeID", uid}, {"status", ok ? "ok" : "failed"}});
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("Set input profile for a universe. Batch: pass multiple in 'items'."),
        std::nullopt
    ));
}
