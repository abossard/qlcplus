/*
  Q Light Controller Plus
  vc_tools.cpp

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
#include "doc.h"

#include <fastmcpp/tools/manager.hpp>
#include <fastmcpp/tools/tool.hpp>

void registerVCTools(fastmcpp::tools::ToolManager &tm, Doc *doc, VCBridge *vcBridge)
{
    using Json = nlohmann::json;
    using Tool = fastmcpp::tools::Tool;

    if (!vcBridge) return;

    // create_vc_pages (batch)
    tm.register_tool(Tool(
        "create_vc_pages",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"name", {{"type", "string"}}}
            }}, {"required", {"name"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc, vcBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            for (auto &item : args["items"])
            {
                int idx = vcBridge->addPage(QString::fromStdString(item["name"].get<std::string>()));
                results.push_back({{"pageIndex", idx}});
            }
            return results;
            });
        },
        std::nullopt,
        std::string("Create new Virtual Console pages. Batch: pass multiple in 'items'."),
        std::nullopt
    ));

    // add_vc_frames (batch)
    tm.register_tool(Tool(
        "add_vc_frames",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"pageIndex", {{"type", "integer"}}},
                {"x", {{"type", "integer"}}}, {"y", {{"type", "integer"}}},
                {"width", {{"type", "integer"}}}, {"height", {{"type", "integer"}}},
                {"caption", {{"type", "string"}}},
                {"solo", {{"type", "boolean"}, {"description", "If true, only one child can be active at a time (SoloFrame)"}}}
            }}, {"required", {"pageIndex", "x", "y", "width", "height", "caption"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc, vcBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            for (auto &item : args["items"])
            {
                QRect geo(item["x"].get<int>(), item["y"].get<int>(),
                          item["width"].get<int>(), item["height"].get<int>());
                int id = vcBridge->addFrame(
                    item["pageIndex"].get<int>(), geo,
                    QString::fromStdString(item["caption"].get<std::string>()),
                    item.value("solo", false));
                results.push_back({{"widgetID", id}});
            }
            return results;
            });
        },
        std::nullopt,
        std::string("Add frame containers to Virtual Console. Use solo=true for mutually exclusive moods. Batch."),
        std::nullopt
    ));

    // add_vc_buttons (batch)
    tm.register_tool(Tool(
        "add_vc_buttons",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"parentID", {{"type", "integer"}, {"description", "Frame or page widget ID"}}},
                {"x", {{"type", "integer"}}}, {"y", {{"type", "integer"}}},
                {"width", {{"type", "integer"}}}, {"height", {{"type", "integer"}}},
                {"functionID", {{"type", "integer"}}},
                {"caption", {{"type", "string"}}},
                {"action", {{"type", "string"}, {"enum", {"toggle", "flash"}}, {"description", "Button behavior (default toggle)"}}}
            }}, {"required", {"parentID", "x", "y", "width", "height", "functionID", "caption"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc, vcBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            for (auto &item : args["items"])
            {
                QRect geo(item["x"].get<int>(), item["y"].get<int>(),
                          item["width"].get<int>(), item["height"].get<int>());
                int id = vcBridge->addButton(
                    item["parentID"].get<int>(), geo,
                    item["functionID"].get<int>(),
                    QString::fromStdString(item["caption"].get<std::string>()),
                    QString::fromStdString(item.value("action", "toggle")));
                results.push_back({{"widgetID", id}});
            }
            return results;
            });
        },
        std::nullopt,
        std::string("Add buttons linked to functions. Use action='flash' for strobes. Batch."),
        std::nullopt
    ));

    // add_vc_sliders (batch)
    tm.register_tool(Tool(
        "add_vc_sliders",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"parentID", {{"type", "integer"}}},
                {"x", {{"type", "integer"}}}, {"y", {{"type", "integer"}}},
                {"width", {{"type", "integer"}}}, {"height", {{"type", "integer"}}},
                {"caption", {{"type", "string"}}},
                {"mode", {{"type", "string"}, {"enum", {"level", "playback", "submaster"}}}},
                {"functionID", {{"type", "integer"}, {"description", "Function to control (for playback mode)"}}},
                {"channels", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                    {"fixtureID", {{"type", "integer"}}}, {"channel", {{"type", "integer"}}}
                }}}}, {"description", "Fixture channels to control (for level mode)"}}}
            }}, {"required", {"parentID", "x", "y", "width", "height", "caption", "mode"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc, vcBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            for (auto &item : args["items"])
            {
                QRect geo(item["x"].get<int>(), item["y"].get<int>(),
                          item["width"].get<int>(), item["height"].get<int>());

                QList<QPair<quint32, quint32>> channels;
                if (item.contains("channels"))
                {
                    for (auto &ch : item["channels"])
                        channels.append({ch["fixtureID"].get<int>(), ch["channel"].get<int>()});
                }

                int id = vcBridge->addSlider(
                    item["parentID"].get<int>(), geo,
                    QString::fromStdString(item["mode"].get<std::string>()),
                    QString::fromStdString(item.value("caption", "")),
                    item.value("functionID", -1),
                    channels);
                results.push_back({{"widgetID", id}});
            }
            return results;
            });
        },
        std::nullopt,
        std::string("Add sliders. Modes: 'level' (DMX channels), 'playback' (function intensity), 'submaster' (master dimmer). Batch."),
        std::nullopt
    ));

    // add_vc_xypads (batch)
    tm.register_tool(Tool(
        "add_vc_xypads",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"parentID", {{"type", "integer"}}},
                {"x", {{"type", "integer"}}}, {"y", {{"type", "integer"}}},
                {"size", {{"type", "integer"}, {"description", "Width and height (square)"}}},
                {"fixtureIDs", {{"type", "array"}, {"items", {{"type", "integer"}}}}}
            }}, {"required", {"parentID", "x", "y", "size", "fixtureIDs"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc, vcBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            for (auto &item : args["items"])
            {
                int sz = item["size"].get<int>();
                QRect geo(item["x"].get<int>(), item["y"].get<int>(), sz, sz);
                QList<quint32> fxIDs;
                for (auto &fid : item["fixtureIDs"])
                    fxIDs.append(fid.get<int>());
                int id = vcBridge->addXYPad(item["parentID"].get<int>(), geo, fxIDs);
                results.push_back({{"widgetID", id}});
            }
            return results;
            });
        },
        std::nullopt,
        std::string("Add XY pads for pan/tilt control of moving heads. Batch."),
        std::nullopt
    ));

    // add_vc_cuelists (batch)
    tm.register_tool(Tool(
        "add_vc_cuelists",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"parentID", {{"type", "integer"}}},
                {"x", {{"type", "integer"}}}, {"y", {{"type", "integer"}}},
                {"width", {{"type", "integer"}}}, {"height", {{"type", "integer"}}},
                {"chaserID", {{"type", "integer"}}},
                {"caption", {{"type", "string"}}}
            }}, {"required", {"parentID", "x", "y", "width", "height", "chaserID"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc, vcBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            for (auto &item : args["items"])
            {
                QRect geo(item["x"].get<int>(), item["y"].get<int>(),
                          item["width"].get<int>(), item["height"].get<int>());
                int id = vcBridge->addCueList(
                    item["parentID"].get<int>(), geo,
                    item["chaserID"].get<int>(),
                    QString::fromStdString(item.value("caption", "")));
                results.push_back({{"widgetID", id}});
            }
            return results;
            });
        },
        std::nullopt,
        std::string("Add cue list widgets for chaser playback. Batch."),
        std::nullopt
    ));

    // add_vc_labels (batch)
    tm.register_tool(Tool(
        "add_vc_labels",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"parentID", {{"type", "integer"}}},
                {"x", {{"type", "integer"}}}, {"y", {{"type", "integer"}}},
                {"width", {{"type", "integer"}}}, {"height", {{"type", "integer"}}},
                {"text", {{"type", "string"}}}
            }}, {"required", {"parentID", "x", "y", "width", "height", "text"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc, vcBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            for (auto &item : args["items"])
            {
                QRect geo(item["x"].get<int>(), item["y"].get<int>(),
                          item["width"].get<int>(), item["height"].get<int>());
                int id = vcBridge->addLabel(
                    item["parentID"].get<int>(), geo,
                    QString::fromStdString(item["text"].get<std::string>()));
                results.push_back({{"widgetID", id}});
            }
            return results;
            });
        },
        std::nullopt,
        std::string("Add text labels. Batch."),
        std::nullopt
    ));

    // map_vc_inputs (batch)
    tm.register_tool(Tool(
        "map_vc_inputs",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"widgetID", {{"type", "integer"}}},
                {"inputUniverse", {{"type", "integer"}}},
                {"inputChannel", {{"type", "integer"}}}
            }}, {"required", {"widgetID", "inputUniverse", "inputChannel"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc, vcBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            for (auto &item : args["items"])
            {
                bool ok = vcBridge->mapWidgetInput(
                    item["widgetID"].get<int>(),
                    item["inputUniverse"].get<int>(),
                    item["inputChannel"].get<int>());
                results.push_back({
                    {"widgetID", item["widgetID"].get<int>()},
                    {"status", ok ? "ok" : "failed"}
                });
            }
            return results;
            });
        },
        std::nullopt,
        std::string("Map external controller inputs (OSC/MIDI faders) to Virtual Console widgets. Batch."),
        std::nullopt
    ));
}
