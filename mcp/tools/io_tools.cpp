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
#include "universe.h"

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
                {"passthrough", {{"type", "boolean"}}}
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

                results.push_back({{"universeID", uid}, {"status", ok ? "ok" : "failed"}});
            }
            return results;
            });
        },
        std::nullopt,
        std::string("Configure universe input/output plugins (OSC, ArtNet, E1.31, etc.). Batch."),
        std::nullopt
    ));
}
