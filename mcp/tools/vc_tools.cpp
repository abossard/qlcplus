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
#include "function.h"

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
            for (auto &item : args.at("items"))
            {
                QString name = QString::fromStdString(item.at("name").get<std::string>());
                int existingIdx = vcBridge->findPageByName(name);
                if (existingIdx >= 0)
                {
                    results.push_back({{"pageIndex", existingIdx}, {"status", "existing"}});
                    continue;
                }
                int idx = vcBridge->addPage(name);
                results.push_back({{"pageIndex", idx}, {"status", "created"}});
            }
            return results.dump();
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
                {"solo", {{"type", "boolean"}, {"description", "If true, only one child can be active at a time (SoloFrame)"}}},
                {"bgColor", {{"type", "string"}, {"description", "Background color hex (e.g. #1a3300)"}}},
                {"fgColor", {{"type", "string"}, {"description", "Foreground/text color hex (e.g. #ffffff)"}}}
            }}, {"required", {"pageIndex", "x", "y", "width", "height", "caption"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc, vcBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            for (auto &item : args.at("items"))
            {
                QString caption = QString::fromStdString(item.at("caption").get<std::string>());
                bool solo = item.value("solo", false);
                int pageIndex = item.at("pageIndex").get<int>();

                // For frames, the parent is the page root widget
                // Search all pages for a frame with this caption
                int existingId = -1;
                for (const auto &page : vcBridge->pages())
                {
                    if (page.index == pageIndex)
                    {
                        for (const auto &w : page.widgets)
                        {
                            if ((w.type == "Frame" || w.type == "Solo Frame") && w.caption == caption)
                            {
                                existingId = w.id;
                                break;
                            }
                        }
                        break;
                    }
                }
                if (existingId >= 0)
                {
                    results.push_back({{"widgetID", existingId}, {"status", "existing"}});
                    continue;
                }

                QRect geo(item.at("x").get<int>(), item.at("y").get<int>(),
                          item.at("width").get<int>(), item.at("height").get<int>());
                int id = vcBridge->addFrame(pageIndex, geo, caption, solo);
                results.push_back({{"widgetID", id}, {"status", "created"}});
                if (id >= 0 && (item.contains("bgColor") || item.contains("fgColor")))
                {
                    QColor bg = item.contains("bgColor") ? QColor(QString::fromStdString(item.at("bgColor").get<std::string>())) : QColor();
                    QColor fg = item.contains("fgColor") ? QColor(QString::fromStdString(item.at("fgColor").get<std::string>())) : QColor();
                    vcBridge->setWidgetColors(id, bg, fg);
                }
            }
            return results.dump();
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
                {"action", {{"type", "string"}, {"enum", {"toggle", "flash", "blackout", "stopall"}}, {"description", "Button behavior: toggle (start/stop), flash (hold), blackout (system blackout toggle), stopall (stop all functions/panic)"}}},
                {"stopAllFadeTime", {{"type", "integer"}, {"description", "Fade out time in ms before stopping all functions (only for stopall action, default 0 = immediate)"}}},
                {"bgColor", {{"type", "string"}, {"description", "Background color hex (e.g. #1a3300)"}}},
                {"fgColor", {{"type", "string"}, {"description", "Foreground/text color hex (e.g. #ffffff)"}}}
            }}, {"required", {"parentID", "x", "y", "width", "height", "caption"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc, vcBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            for (auto &item : args.at("items"))
            {
                int parentID = item.at("parentID").get<int>();
                QString caption = QString::fromStdString(item.at("caption").get<std::string>());
                int existingId = vcBridge->findWidgetByCaption(parentID, "Button", caption);
                if (existingId >= 0)
                {
                    results.push_back({{"widgetID", existingId}, {"status", "existing"}});
                    continue;
                }

                QRect geo(item.at("x").get<int>(), item.at("y").get<int>(),
                          item.at("width").get<int>(), item.at("height").get<int>());
                int funcID = item.value("functionID", -1);
                int id = vcBridge->addButton(
                    parentID, geo,
                    funcID >= 0 ? (quint32)funcID : Function::invalidId(),
                    caption,
                    QString::fromStdString(item.value("action", "toggle")),
                    item.value("stopAllFadeTime", 0));

                results.push_back({{"widgetID", id}, {"status", "created"}});
                if (id >= 0 && (item.contains("bgColor") || item.contains("fgColor")))
                {
                    QColor bg = item.contains("bgColor") ? QColor(QString::fromStdString(item.at("bgColor").get<std::string>())) : QColor();
                    QColor fg = item.contains("fgColor") ? QColor(QString::fromStdString(item.at("fgColor").get<std::string>())) : QColor();
                    vcBridge->setWidgetColors(id, bg, fg);
                }
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("Add buttons. Actions: toggle (start/stop function), flash (hold to activate), blackout (toggle system blackout), stopall (panic — stop all functions with optional fade). Batch."),
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
                }}}}, {"description", "Fixture channels to control (for level mode)"}}},
                {"bgColor", {{"type", "string"}, {"description", "Background color hex (e.g. #1a3300)"}}},
                {"fgColor", {{"type", "string"}, {"description", "Foreground/text color hex (e.g. #ffffff)"}}}
            }}, {"required", {"parentID", "x", "y", "width", "height", "caption", "mode"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc, vcBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            for (auto &item : args.at("items"))
            {
                int parentID = item.at("parentID").get<int>();
                QString caption = QString::fromStdString(item.value("caption", ""));
                if (!caption.isEmpty())
                {
                    int existingId = vcBridge->findWidgetByCaption(parentID, "Slider", caption);
                    if (existingId >= 0)
                    {
                        results.push_back({{"widgetID", existingId}, {"status", "existing"}});
                        continue;
                    }
                }

                QRect geo(item.at("x").get<int>(), item.at("y").get<int>(),
                          item.at("width").get<int>(), item.at("height").get<int>());

                QList<QPair<quint32, quint32>> channels;
                if (item.contains("channels"))
                {
                    for (auto &ch : item.at("channels"))
                        channels.append({ch.at("fixtureID").get<int>(), ch.at("channel").get<int>()});
                }

                int id = vcBridge->addSlider(
                    parentID, geo,
                    QString::fromStdString(item.at("mode").get<std::string>()),
                    caption,
                    item.value("functionID", -1),
                    channels);
                results.push_back({{"widgetID", id}, {"status", "created"}});
                if (id >= 0 && (item.contains("bgColor") || item.contains("fgColor")))
                {
                    QColor bg = item.contains("bgColor") ? QColor(QString::fromStdString(item.at("bgColor").get<std::string>())) : QColor();
                    QColor fg = item.contains("fgColor") ? QColor(QString::fromStdString(item.at("fgColor").get<std::string>())) : QColor();
                    vcBridge->setWidgetColors(id, bg, fg);
                }
            }
            return results.dump();
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
                {"fixtureIDs", {{"type", "array"}, {"items", {{"type", "integer"}}}}},
                {"bgColor", {{"type", "string"}, {"description", "Background color hex (e.g. #1a3300)"}}},
                {"fgColor", {{"type", "string"}, {"description", "Foreground/text color hex (e.g. #ffffff)"}}}
            }}, {"required", {"parentID", "x", "y", "size", "fixtureIDs"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc, vcBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            for (auto &item : args.at("items"))
            {
                int sz = item.at("size").get<int>();
                QRect geo(item.at("x").get<int>(), item.at("y").get<int>(), sz, sz);
                QList<quint32> fxIDs;
                for (auto &fid : item.at("fixtureIDs"))
                    fxIDs.append(fid.get<int>());
                int id = vcBridge->addXYPad(item.at("parentID").get<int>(), geo, fxIDs);
                results.push_back({{"widgetID", id}});
                if (id >= 0 && (item.contains("bgColor") || item.contains("fgColor")))
                {
                    QColor bg = item.contains("bgColor") ? QColor(QString::fromStdString(item.at("bgColor").get<std::string>())) : QColor();
                    QColor fg = item.contains("fgColor") ? QColor(QString::fromStdString(item.at("fgColor").get<std::string>())) : QColor();
                    vcBridge->setWidgetColors(id, bg, fg);
                }
            }
            return results.dump();
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
                {"caption", {{"type", "string"}}},
                {"bgColor", {{"type", "string"}, {"description", "Background color hex (e.g. #1a3300)"}}},
                {"fgColor", {{"type", "string"}, {"description", "Foreground/text color hex (e.g. #ffffff)"}}}
            }}, {"required", {"parentID", "x", "y", "width", "height", "chaserID"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc, vcBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            for (auto &item : args.at("items"))
            {
                int parentID = item.at("parentID").get<int>();
                QString caption = QString::fromStdString(item.value("caption", ""));
                if (!caption.isEmpty())
                {
                    int existingId = vcBridge->findWidgetByCaption(parentID, "CueList", caption);
                    if (existingId >= 0)
                    {
                        results.push_back({{"widgetID", existingId}, {"status", "existing"}});
                        continue;
                    }
                }

                QRect geo(item.at("x").get<int>(), item.at("y").get<int>(),
                          item.at("width").get<int>(), item.at("height").get<int>());
                int id = vcBridge->addCueList(
                    parentID, geo,
                    item.at("chaserID").get<int>(),
                    caption);
                results.push_back({{"widgetID", id}, {"status", "created"}});
                if (id >= 0 && (item.contains("bgColor") || item.contains("fgColor")))
                {
                    QColor bg = item.contains("bgColor") ? QColor(QString::fromStdString(item.at("bgColor").get<std::string>())) : QColor();
                    QColor fg = item.contains("fgColor") ? QColor(QString::fromStdString(item.at("fgColor").get<std::string>())) : QColor();
                    vcBridge->setWidgetColors(id, bg, fg);
                }
            }
            return results.dump();
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
                {"text", {{"type", "string"}}},
                {"bgColor", {{"type", "string"}, {"description", "Background color hex (e.g. #1a3300)"}}},
                {"fgColor", {{"type", "string"}, {"description", "Foreground/text color hex (e.g. #ffffff)"}}}
            }}, {"required", {"parentID", "x", "y", "width", "height", "text"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc, vcBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            for (auto &item : args.at("items"))
            {
                int parentID = item.at("parentID").get<int>();
                QString text = QString::fromStdString(item.at("text").get<std::string>());
                int existingId = vcBridge->findWidgetByCaption(parentID, "Label", text);
                if (existingId >= 0)
                {
                    results.push_back({{"widgetID", existingId}, {"status", "existing"}});
                    continue;
                }

                QRect geo(item.at("x").get<int>(), item.at("y").get<int>(),
                          item.at("width").get<int>(), item.at("height").get<int>());
                int id = vcBridge->addLabel(parentID, geo, text);
                results.push_back({{"widgetID", id}, {"status", "created"}});
                if (id >= 0 && (item.contains("bgColor") || item.contains("fgColor")))
                {
                    QColor bg = item.contains("bgColor") ? QColor(QString::fromStdString(item.at("bgColor").get<std::string>())) : QColor();
                    QColor fg = item.contains("fgColor") ? QColor(QString::fromStdString(item.at("fgColor").get<std::string>())) : QColor();
                    vcBridge->setWidgetColors(id, bg, fg);
                }
            }
            return results.dump();
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
            for (auto &item : args.at("items"))
            {
                bool ok = vcBridge->mapWidgetInput(
                    item.at("widgetID").get<int>(),
                    item.at("inputUniverse").get<int>(),
                    item.at("inputChannel").get<int>());
                results.push_back({
                    {"widgetID", item.at("widgetID").get<int>()},
                    {"status", ok ? "ok" : "failed"}
                });
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("Map external controller inputs (OSC/MIDI faders) to Virtual Console widgets. Batch."),
        std::nullopt
    ));

    // configure_vc_feedback (batch)
    tm.register_tool(Tool(
        "configure_vc_feedback",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"widgetID", {{"type", "integer"}}},
                {"idleValue", {{"type", "integer"}, {"description", "LED color when inactive (velocity from color table, 0=off)"}}},
                {"activeValue", {{"type", "integer"}, {"description", "LED color when active"}}},
                {"monitorValue", {{"type", "integer"}, {"description", "LED color for monitor/intermediate state"}}},
                {"idleMode", {{"type", "string"}, {"enum", {"static", "flashing", "pulsing"}}, {"description", "LED animation mode for idle state (default static)"}}},
                {"activeMode", {{"type", "string"}, {"enum", {"static", "flashing", "pulsing"}}, {"description", "LED animation mode for active state (default static)"}}},
                {"monitorMode", {{"type", "string"}, {"enum", {"static", "flashing", "pulsing"}}, {"description", "LED animation mode for monitor state"}}}
            }}, {"required", {"widgetID", "activeValue"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc, vcBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            try {
            Json results = Json::array();
            for (auto &item : args.at("items"))
            {
                int wid = item.at("widgetID").get<int>();
                int activeVal = item.at("activeValue").get<int>();
                int idleVal = item.value("idleValue", 0);
                int monitorVal = item.value("monitorValue", 0);

                int midiChIdle = 0, midiChActive = 0, midiChMonitor = 0;
                std::string idleMode = item.value("idleMode", "static");
                std::string activeMode = item.value("activeMode", "static");
                std::string monitorMode = item.value("monitorMode", "");
                if (idleMode == "flashing") midiChIdle = 1;
                else if (idleMode == "pulsing") midiChIdle = 2;
                if (activeMode == "flashing") midiChActive = 1;
                else if (activeMode == "pulsing") midiChActive = 2;
                if (monitorMode == "flashing") midiChMonitor = 1;
                else if (monitorMode == "pulsing") midiChMonitor = 2;

                bool ok = vcBridge->setWidgetFeedback(wid, idleVal, activeVal, monitorVal,
                                                      midiChIdle, midiChActive, midiChMonitor);

                results.push_back({
                    {"widgetID", wid},
                    {"status", ok ? "ok" : "failed"}
                });
            }
            return results.dump();
            } catch (const std::exception &e) {
                return Json({{"error", e.what()}}).dump();
            }
            });
        },
        std::nullopt,
        std::string("Set LED feedback colors per widget. idleValue=LED when inactive, activeValue=LED when active. ledMode: static/flashing/pulsing. Batch."),
        std::nullopt
    ));

    // set_widget_colors (batch)
    tm.register_tool(Tool(
        "set_widget_colors",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"widgetID", {{"type", "integer"}}},
                {"bgColor", {{"type", "string"}, {"description", "Background color hex (e.g. #1a3300)"}}},
                {"fgColor", {{"type", "string"}, {"description", "Foreground/text color hex (e.g. #ffffff)"}}}
            }}, {"required", {"widgetID"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc, vcBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            for (auto &item : args.at("items"))
            {
                int wid = item.at("widgetID").get<int>();
                QColor bg = item.contains("bgColor") ? QColor(QString::fromStdString(item.at("bgColor").get<std::string>())) : QColor();
                QColor fg = item.contains("fgColor") ? QColor(QString::fromStdString(item.at("fgColor").get<std::string>())) : QColor();
                bool ok = vcBridge->setWidgetColors(wid, bg, fg);
                results.push_back({
                    {"widgetID", wid},
                    {"status", ok ? "ok" : "failed"}
                });
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("Set background and/or foreground colors on existing Virtual Console widgets. Batch."),
        std::nullopt
    ));

    // add_vc_speed_dials (batch)
    tm.register_tool(Tool(
        "add_vc_speed_dials",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"parentID", {{"type", "integer"}, {"description", "Frame or page widget ID"}}},
                {"x", {{"type", "integer"}}}, {"y", {{"type", "integer"}}},
                {"width", {{"type", "integer"}}}, {"height", {{"type", "integer"}}},
                {"functionIDs", {{"type", "array"}, {"items", {{"type", "integer"}}}, {"description", "Chasers/EFX/sequences whose speed this dial controls"}}},
                {"bgColor", {{"type", "string"}, {"description", "Background color hex (e.g. #003366)"}}},
                {"fgColor", {{"type", "string"}, {"description", "Foreground/text color hex (e.g. #ffffff)"}}}
            }}, {"required", {"parentID", "x", "y", "width", "height", "functionIDs"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc, vcBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            for (auto &item : args.at("items"))
            {
                QRect geo(item.at("x").get<int>(), item.at("y").get<int>(),
                          item.at("width").get<int>(), item.at("height").get<int>());
                QList<quint32> funcIDs;
                for (auto &fid : item.at("functionIDs"))
                    funcIDs.append(fid.get<int>());
                int id = vcBridge->addSpeedDial(item.at("parentID").get<int>(), geo, funcIDs);
                results.push_back({{"widgetID", id}});
                if (id >= 0 && (item.contains("bgColor") || item.contains("fgColor")))
                {
                    QColor bg = item.contains("bgColor") ? QColor(QString::fromStdString(item.at("bgColor").get<std::string>())) : QColor();
                    QColor fg = item.contains("fgColor") ? QColor(QString::fromStdString(item.at("fgColor").get<std::string>())) : QColor();
                    vcBridge->setWidgetColors(id, bg, fg);
                }
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("Add speed dial widgets to control chaser/EFX/sequence speed with tap tempo. Batch."),
        std::nullopt
    ));

    // add_vc_audio_triggers (batch)
    tm.register_tool(Tool(
        "add_vc_audio_triggers",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"parentID", {{"type", "integer"}, {"description", "Frame or page widget ID"}}},
                {"x", {{"type", "integer"}}}, {"y", {{"type", "integer"}}},
                {"width", {{"type", "integer"}}}, {"height", {{"type", "integer"}}},
                {"bgColor", {{"type", "string"}, {"description", "Background color hex (e.g. #1a0000)"}}},
                {"fgColor", {{"type", "string"}, {"description", "Foreground/text color hex (e.g. #ff6666)"}}}
            }}, {"required", {"parentID", "x", "y", "width", "height"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc, vcBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            for (auto &item : args.at("items"))
            {
                QRect geo(item.at("x").get<int>(), item.at("y").get<int>(),
                          item.at("width").get<int>(), item.at("height").get<int>());
                int id = vcBridge->addAudioTriggers(item.at("parentID").get<int>(), geo);
                results.push_back({{"widgetID", id}});
                if (id >= 0 && (item.contains("bgColor") || item.contains("fgColor")))
                {
                    QColor bg = item.contains("bgColor") ? QColor(QString::fromStdString(item.at("bgColor").get<std::string>())) : QColor();
                    QColor fg = item.contains("fgColor") ? QColor(QString::fromStdString(item.at("fgColor").get<std::string>())) : QColor();
                    vcBridge->setWidgetColors(id, bg, fg);
                }
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("Add audio trigger widgets for sound-reactive lighting. Configure individual frequency bars separately. Batch."),
        std::nullopt
    ));

    // add_vc_clocks (batch)
    tm.register_tool(Tool(
        "add_vc_clocks",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"parentID", {{"type", "integer"}, {"description", "Frame or page widget ID"}}},
                {"x", {{"type", "integer"}}}, {"y", {{"type", "integer"}}},
                {"width", {{"type", "integer"}}}, {"height", {{"type", "integer"}}},
                {"clockType", {{"type", "string"}, {"enum", {"clock", "stopwatch", "countdown"}}, {"description", "Clock type (default clock)"}}},
                {"bgColor", {{"type", "string"}, {"description", "Background color hex (e.g. #1a1a1a)"}}},
                {"fgColor", {{"type", "string"}, {"description", "Foreground/text color hex (e.g. #ffffff)"}}}
            }}, {"required", {"parentID", "x", "y", "width", "height"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc, vcBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            for (auto &item : args.at("items"))
            {
                QRect geo(item.at("x").get<int>(), item.at("y").get<int>(),
                          item.at("width").get<int>(), item.at("height").get<int>());
                int id = vcBridge->addClock(
                    item.at("parentID").get<int>(), geo,
                    QString::fromStdString(item.value("clockType", "clock")));
                results.push_back({{"widgetID", id}});
                if (id >= 0 && (item.contains("bgColor") || item.contains("fgColor")))
                {
                    QColor bg = item.contains("bgColor") ? QColor(QString::fromStdString(item.at("bgColor").get<std::string>())) : QColor();
                    QColor fg = item.contains("fgColor") ? QColor(QString::fromStdString(item.at("fgColor").get<std::string>())) : QColor();
                    vcBridge->setWidgetColors(id, bg, fg);
                }
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("Add clock widgets (real-time clock, stopwatch, or countdown timer). Batch."),
        std::nullopt
    ));
}
