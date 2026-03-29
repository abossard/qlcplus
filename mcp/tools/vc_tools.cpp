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
#include "idempotency.h"
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
                {"parentID", {{"type", "integer"}, {"description", "Parent frame widget ID for nesting. Mutually exclusive with pageIndex."}}},
                {"x", {{"type", "integer"}}}, {"y", {{"type", "integer"}}},
                {"width", {{"type", "integer"}}}, {"height", {{"type", "integer"}}},
                {"caption", {{"type", "string"}}},
                {"solo", {{"type", "boolean"}, {"description", "If true, only one child can be active at a time (SoloFrame)"}}},
                {"bgColor", {{"type", "string"}, {"description", "Background color hex (e.g. #1a3300)"}}},
                {"fgColor", {{"type", "string"}, {"description", "Foreground/text color hex (e.g. #ffffff)"}}}
            }}, {"required", {"caption"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc, vcBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            for (auto &item : args.at("items"))
            {
                QString caption = QString::fromStdString(item.at("caption").get<std::string>());
                bool solo = item.value("solo", false);
                bool hasPage = item.contains("pageIndex");
                bool hasParent = item.contains("parentID");

                if (hasPage && hasParent)
                {
                    results.push_back({{"error", "pageIndex and parentID are mutually exclusive"}});
                    continue;
                }
                if (!hasPage && !hasParent)
                {
                    results.push_back({{"error", "either pageIndex or parentID is required"}});
                    continue;
                }

                // Check for existing frame with same caption
                if (hasPage)
                {
                    int pageIndex = item.at("pageIndex").get<int>();
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
                }
                else
                {
                    int parentID = item.at("parentID").get<int>();
                    int existingId = vcBridge->findWidgetByCaption(parentID, "Frame", caption);
                    if (existingId < 0)
                        existingId = vcBridge->findWidgetByCaption(parentID, "Solo frame", caption);
                    if (existingId >= 0)
                    {
                        results.push_back({{"widgetID", existingId}, {"status", "existing"}});
                        continue;
                    }
                }

                int w = item.value("width", 400);
                int h = item.value("height", 300);
                QRect geo;
                if (item.contains("x") && item.contains("y"))
                    geo = QRect(item.at("x").get<int>(), item.at("y").get<int>(), w, h);
                else
                    geo = QRect(5, 5, w, h);

                int id;
                if (hasPage)
                    id = vcBridge->addFrame(item.at("pageIndex").get<int>(), geo, caption, solo);
                else
                    id = vcBridge->addFrameInFrame(item.at("parentID").get<int>(), geo, caption, solo);

                results.push_back({{"widgetID", id}, {"status", id >= 0 ? "created" : "failed"}});
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
        std::string("Add frame containers to Virtual Console. Use pageIndex for top-level frames, "
                     "parentID for nested frames (mutually exclusive). Use solo=true for mutually exclusive moods. Batch."),
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
                {"functionName", {{"type", "string"}, {"description", "Function name. Alternative to functionID."}}},
                {"caption", {{"type", "string"}}},
                {"action", {{"type", "string"}, {"enum", {"toggle", "flash", "blackout", "stopall"}}, {"description", "Button behavior: toggle (start/stop), flash (hold), blackout (system blackout toggle), stopall (stop all functions/panic)"}}},
                {"stopAllFadeTime", {{"type", "integer"}, {"description", "Fade out time in ms before stopping all functions (only for stopall action, default 0 = immediate)"}}},
                {"bgColor", {{"type", "string"}, {"description", "Background color hex (e.g. #1a3300)"}}},
                {"fgColor", {{"type", "string"}, {"description", "Foreground/text color hex (e.g. #ffffff)"}}}
            }}, {"required", {"parentID", "caption"}}}}}}
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

                // Resolve function ID from name or direct ID
                int funcID = -1;
                if (item.contains("functionName"))
                {
                    quint32 fid = mcp::resolveFunctionByName(doc, QString::fromStdString(item.at("functionName").get<std::string>()));
                    if (fid != Function::invalidId()) funcID = (int)fid;
                }
                if (item.contains("functionID"))
                    funcID = item.at("functionID").get<int>();

                // Auto-layout: use provided geometry or compute next position
                int w = item.value("width", 100);
                int h = item.value("height", 60);
                QRect geo;
                if (item.contains("x") && item.contains("y"))
                    geo = QRect(item.at("x").get<int>(), item.at("y").get<int>(), w, h);
                else
                    geo = vcBridge->nextWidgetPosition(parentID, w, h);

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
                {"functionName", {{"type", "string"}, {"description", "Function name. Alternative to functionID (for playback mode)."}}},
                {"channels", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                    {"fixtureID", {{"type", "integer"}}}, {"channel", {{"type", "integer"}}}
                }}}}, {"description", "Fixture channels to control (for level mode)"}}},
                {"bgColor", {{"type", "string"}, {"description", "Background color hex (e.g. #1a3300)"}}},
                {"fgColor", {{"type", "string"}, {"description", "Foreground/text color hex (e.g. #ffffff)"}}}
            }}, {"required", {"parentID", "caption", "mode"}}}}}}
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

                int w = item.value("width", 60);
                int h = item.value("height", 200);
                QRect geo;
                if (item.contains("x") && item.contains("y"))
                    geo = QRect(item.at("x").get<int>(), item.at("y").get<int>(), w, h);
                else
                    geo = vcBridge->nextWidgetPosition(parentID, w, h);

                QList<QPair<quint32, quint32>> channels;
                if (item.contains("channels"))
                {
                    for (auto &ch : item.at("channels"))
                        channels.append({ch.at("fixtureID").get<int>(), ch.at("channel").get<int>()});
                }

                int funcID = item.value("functionID", -1);
                if (funcID < 0 && item.contains("functionName"))
                {
                    quint32 fid = mcp::resolveFunctionByName(doc, QString::fromStdString(item.at("functionName").get<std::string>()));
                    if (fid != Function::invalidId()) funcID = (int)fid;
                }

                int id = vcBridge->addSlider(
                    parentID, geo,
                    QString::fromStdString(item.at("mode").get<std::string>()),
                    caption,
                    funcID,
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
            }}, {"required", {"parentID", "size", "fixtureIDs"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc, vcBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            for (auto &item : args.at("items"))
            {
                int sz = item.at("size").get<int>();
                QRect geo;
                if (item.contains("x") && item.contains("y"))
                    geo = QRect(item.at("x").get<int>(), item.at("y").get<int>(), sz, sz);
                else
                    geo = vcBridge->nextWidgetPosition(item.at("parentID").get<int>(), sz, sz);
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
                {"chaserName", {{"type", "string"}, {"description", "Chaser name. Alternative to chaserID."}}},
                {"caption", {{"type", "string"}}},
                {"bgColor", {{"type", "string"}, {"description", "Background color hex (e.g. #1a3300)"}}},
                {"fgColor", {{"type", "string"}, {"description", "Foreground/text color hex (e.g. #ffffff)"}}}
            }}, {"required", {"parentID"}}}}}}
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

                int w = item.value("width", 300);
                int h = item.value("height", 200);
                QRect geo;
                if (item.contains("x") && item.contains("y"))
                    geo = QRect(item.at("x").get<int>(), item.at("y").get<int>(), w, h);
                else
                    geo = vcBridge->nextWidgetPosition(parentID, w, h);

                int chaserIDVal = -1;
                if (item.contains("chaserID"))
                    chaserIDVal = item.at("chaserID").get<int>();
                else if (item.contains("chaserName"))
                {
                    quint32 fid = mcp::resolveFunctionByName(doc, QString::fromStdString(item.at("chaserName").get<std::string>()), Function::ChaserType);
                    if (fid != Function::invalidId()) chaserIDVal = (int)fid;
                }

                int id = vcBridge->addCueList(
                    parentID, geo,
                    chaserIDVal,
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
            }}, {"required", {"parentID", "text"}}}}}}
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

                int w = item.value("width", 200);
                int h = item.value("height", 30);
                QRect geo;
                if (item.contains("x") && item.contains("y"))
                    geo = QRect(item.at("x").get<int>(), item.at("y").get<int>(), w, h);
                else
                    geo = vcBridge->nextWidgetPosition(parentID, w, h);
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
            }}, {"required", {"widgetID", "inputUniverse", "inputChannel"}}}}}},
            {"mode", {{"type", "string"}, {"enum", {"replace", "add"}},
                {"description", "replace (default): clear existing inputs first. add: append without clearing."}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc, vcBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            std::string mode = args.value("mode", "replace");
            for (auto &item : args.at("items"))
            {
                int wid = item.at("widgetID").get<int>();
                quint32 uni = item.at("inputUniverse").get<int>();
                quint32 ch = item.at("inputChannel").get<int>();
                bool ok;
                if (mode == "add")
                    ok = vcBridge->addWidgetInput(wid, uni, ch);
                else
                    ok = vcBridge->mapWidgetInput(wid, uni, ch);
                results.push_back({{"widgetID", wid}, {"status", ok ? "ok" : "failed"}});
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("Map external controller inputs (OSC/MIDI faders) to Virtual Console widgets. "
                     "mode='replace' (default): clears existing inputs. mode='add': appends without clearing. Batch."),
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
            }}, {"required", {"parentID", "functionIDs"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc, vcBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            for (auto &item : args.at("items"))
            {
                int parentID = item.at("parentID").get<int>();
                int w = item.value("width", 200);
                int h = item.value("height", 200);
                QRect geo;
                if (item.contains("x") && item.contains("y"))
                    geo = QRect(item.at("x").get<int>(), item.at("y").get<int>(), w, h);
                else
                    geo = vcBridge->nextWidgetPosition(parentID, w, h);
                QList<quint32> funcIDs;
                for (auto &fid : item.at("functionIDs"))
                    funcIDs.append(fid.get<int>());
                int id = vcBridge->addSpeedDial(parentID, geo, funcIDs);
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
            }}, {"required", {"parentID"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc, vcBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            for (auto &item : args.at("items"))
            {
                int parentID = item.at("parentID").get<int>();
                int w = item.value("width", 300);
                int h = item.value("height", 150);
                QRect geo;
                if (item.contains("x") && item.contains("y"))
                    geo = QRect(item.at("x").get<int>(), item.at("y").get<int>(), w, h);
                else
                    geo = vcBridge->nextWidgetPosition(parentID, w, h);
                int id = vcBridge->addAudioTriggers(parentID, geo);
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
            }}, {"required", {"parentID"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc, vcBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            for (auto &item : args.at("items"))
            {
                int parentID = item.at("parentID").get<int>();
                int w = item.value("width", 200);
                int h = item.value("height", 60);
                QRect geo;
                if (item.contains("x") && item.contains("y"))
                    geo = QRect(item.at("x").get<int>(), item.at("y").get<int>(), w, h);
                else
                    geo = vcBridge->nextWidgetPosition(parentID, w, h);
                int id = vcBridge->addClock(
                    parentID, geo,
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

    // build_show_page — composite: creates a full VC page with sections in one call
    // Supports per-widget colors, LED feedback, and Launchpad input mapping.
    // Upsert: existing widgets are updated (not skipped).
    {
    Json buttonSchema = Json{{"type", "object"}, {"properties", {
        {"caption", {{"type", "string"}}},
        {"functionName", {{"type", "string"}, {"description", "Function name to resolve"}}},
        {"functionID", {{"type", "integer"}, {"description", "Direct function ID (overrides functionName)"}}},
        {"action", {{"type", "string"}, {"enum", {"toggle", "flash", "blackout", "stopall"}}, {"description", "Button action (default toggle)"}}},
        {"bgColor", {{"type", "string"}, {"description", "Background color hex (e.g. #1a3300)"}}},
        {"fgColor", {{"type", "string"}, {"description", "Foreground/text color hex (e.g. #ffffff)"}}},
        {"inputUniverse", {{"type", "integer"}, {"description", "Controller input universe for this button"}}},
        {"inputChannel", {{"type", "integer"}, {"description", "Controller input channel (e.g. Launchpad pad 81 = channel 209)"}}},
        {"idleValue", {{"type", "integer"}, {"description", "LED color when inactive (velocity from color table, 0=off)"}}},
        {"activeValue", {{"type", "integer"}, {"description", "LED color when active"}}},
        {"monitorValue", {{"type", "integer"}, {"description", "LED color for monitor/intermediate state"}}},
        {"idleMode", {{"type", "string"}, {"enum", {"static", "flashing", "pulsing"}}, {"description", "LED animation mode for idle state (default static)"}}},
        {"activeMode", {{"type", "string"}, {"enum", {"static", "flashing", "pulsing"}}, {"description", "LED animation mode for active state (default static)"}}},
        {"monitorMode", {{"type", "string"}, {"enum", {"static", "flashing", "pulsing"}}, {"description", "LED animation mode for monitor state"}}},
        {"stopAllFadeTime", {{"type", "integer"}, {"description", "Fade out time in ms before stopping all functions (only for stopall action, default 0)"}}}
    }}, {"required", {"caption"}}};
    Json sliderSchema = Json{{"type", "object"}, {"properties", {
        {"caption", {{"type", "string"}}},
        {"mode", {{"type", "string"}, {"enum", {"level", "playback", "submaster"}}}},
        {"functionName", {{"type", "string"}, {"description", "Function name (for playback mode)"}}},
        {"functionID", {{"type", "integer"}, {"description", "Direct function ID (overrides functionName)"}}},
        {"channels", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
            {"fixtureID", {{"type", "integer"}}},
            {"channel", {{"type", "integer"}}}
        }}}}, {"description", "Fixture channels to control (for level mode)"}}},
        {"bgColor", {{"type", "string"}, {"description", "Background color hex"}}},
        {"fgColor", {{"type", "string"}, {"description", "Foreground/text color hex"}}},
        {"inputUniverse", {{"type", "integer"}, {"description", "Controller input universe"}}},
        {"inputChannel", {{"type", "integer"}, {"description", "Controller input channel"}}}
    }}, {"required", {"caption", "mode"}}};
    Json sectionSchema = Json{{"type", "object"}, {"properties", {
        {"caption", {{"type", "string"}}},
        {"solo", {{"type", "boolean"}, {"description", "If true, only one child can be active at a time (SoloFrame). Default false."}}},
        {"columns", {{"type", "integer"}, {"description", "Number of button/slider columns per row. Auto-computed from page width if omitted."}}},
        {"bgColor", {{"type", "string"}, {"description", "Frame background color hex"}}},
        {"fgColor", {{"type", "string"}, {"description", "Frame foreground/text color hex"}}},
        {"buttons", {{"type", "array"}, {"items", buttonSchema}}},
        {"sliders", {{"type", "array"}, {"items", sliderSchema}}}
    }}, {"required", {"caption"}}};
    Json buildPageSchema = Json{{"type", "object"}, {"properties", {
        {"pageName", {{"type", "string"}}},
        {"pageWidth", {{"type", "integer"}, {"description", "Page/frame width in pixels (default 1910)"}}},
        {"buttonWidth", {{"type", "integer"}, {"description", "Button width in pixels (default 100)"}}},
        {"buttonHeight", {{"type", "integer"}, {"description", "Button height in pixels (default 60)"}}},
        {"sliderWidth", {{"type", "integer"}, {"description", "Slider width in pixels (default 60)"}}},
        {"sliderHeight", {{"type", "integer"}, {"description", "Slider height in pixels (default 200)"}}},
        {"sections", {{"type", "array"}, {"items", sectionSchema}}}
    }}, {"required", {"pageName", "sections"}}};
    tm.register_tool(Tool(
        "build_show_page",
        buildPageSchema,
        Json{},
        [doc, vcBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            QString pageName = QString::fromStdString(args.at("pageName").get<std::string>());

            // Find or create the page
            int pageIndex = vcBridge->findPageByName(pageName);
            if (pageIndex < 0)
                pageIndex = vcBridge->addPage(pageName);

            // Helper: parse MIDI channel from mode string
            auto midiChFromMode = [](const std::string &mode) -> int {
                if (mode == "flashing") return 1;
                if (mode == "pulsing") return 2;
                return 0;
            };

            // Helper: apply optional colors, input mapping, and LED feedback to a widget
            auto applyWidgetProps = [&](int widgetID, const Json &props) {
                // Colors
                if (props.contains("bgColor") || props.contains("fgColor"))
                {
                    QColor bg = props.contains("bgColor")
                        ? QColor(QString::fromStdString(props.at("bgColor").get<std::string>()))
                        : QColor();
                    QColor fg = props.contains("fgColor")
                        ? QColor(QString::fromStdString(props.at("fgColor").get<std::string>()))
                        : QColor();
                    vcBridge->setWidgetColors(widgetID, bg, fg);
                }

                // Input mapping
                if (props.contains("inputUniverse") && props.contains("inputChannel"))
                {
                    vcBridge->mapWidgetInput(widgetID,
                        props.at("inputUniverse").get<int>(),
                        props.at("inputChannel").get<int>());
                }

                // LED feedback
                if (props.contains("activeValue"))
                {
                    int activeVal = props.at("activeValue").get<int>();
                    int idleVal = props.value("idleValue", 0);
                    int monitorVal = props.value("monitorValue", 0);
                    int idleMidiCh = midiChFromMode(props.value("idleMode", "static"));
                    int activeMidiCh = midiChFromMode(props.value("activeMode", "static"));
                    int monitorMidiCh = midiChFromMode(props.value("monitorMode", ""));
                    vcBridge->setWidgetFeedback(widgetID, idleVal, activeVal, monitorVal,
                                                idleMidiCh, activeMidiCh, monitorMidiCh);
                }
            };

            Json sectionsResult = Json::array();
            int frameY = 5;
            const int frameWidth = args.value("pageWidth", 1910);
            const int framePad = 10;
            const int pad = 5;
            const int headerHeight = 40;
            const int btnWidth = args.value("buttonWidth", 100);
            const int btnHeight = args.value("buttonHeight", 60);
            const int sliderWidth = args.value("sliderWidth", 60);
            const int sliderHeight = args.value("sliderHeight", 200);

            for (auto &section : args.at("sections"))
            {
                QString caption = QString::fromStdString(section.at("caption").get<std::string>());
                bool solo = section.value("solo", false);
                int columns = section.value("columns", 0);

                // Count children to estimate frame height using flow layout
                int btnCount = section.contains("buttons") ? (int)section.at("buttons").size() : 0;
                int sliderCount = section.contains("sliders") ? (int)section.at("sliders").size() : 0;

                int btnCols = columns > 0 ? columns : qMax(1, (frameWidth - pad) / (btnWidth + pad));
                int btnRows = btnCount > 0 ? (btnCount + btnCols - 1) / btnCols : 0;
                int sliderCols = columns > 0 ? columns : qMax(1, (frameWidth - pad) / (sliderWidth + pad));
                int sliderRows = sliderCount > 0 ? (sliderCount + sliderCols - 1) / sliderCols : 0;
                int frameHeight = headerHeight + btnRows * (btnHeight + pad) + sliderRows * (sliderHeight + pad) + pad;
                if (frameHeight < 80) frameHeight = 80;

                // Find or create the frame
                int frameID = -1;
                for (const auto &page : vcBridge->pages())
                {
                    if (page.index == pageIndex)
                    {
                        for (const auto &w : page.widgets)
                        {
                            if ((w.type == "Frame" || w.type == "Solo Frame") && w.caption == caption)
                            {
                                frameID = w.id;
                                break;
                            }
                        }
                        break;
                    }
                }
                if (frameID < 0)
                {
                    QRect frameGeo(5, frameY, frameWidth, frameHeight);
                    frameID = vcBridge->addFrame(pageIndex, frameGeo, caption, solo);
                }
                else
                {
                    // Resize existing frame to match new layout
                    vcBridge->setWidgetGeometry(frameID, QRect(5, frameY, frameWidth, frameHeight));
                }
                frameY += frameHeight + framePad;

                // Apply colors to frame
                if (section.contains("bgColor") || section.contains("fgColor"))
                {
                    QColor bg = section.contains("bgColor")
                        ? QColor(QString::fromStdString(section.at("bgColor").get<std::string>()))
                        : QColor();
                    QColor fg = section.contains("fgColor")
                        ? QColor(QString::fromStdString(section.at("fgColor").get<std::string>()))
                        : QColor();
                    vcBridge->setWidgetColors(frameID, bg, fg);
                }

                Json sectionResult;
                sectionResult["widgetID"] = frameID;
                sectionResult["caption"] = caption.toStdString();

                // Create or update buttons inside frame
                Json buttonsResult = Json::array();
                if (section.contains("buttons"))
                {
                    int btnIdx = 0;
                    for (auto &btn : section.at("buttons"))
                    {
                        QString btnCaption = QString::fromStdString(btn.at("caption").get<std::string>());
                        int btnID = vcBridge->findWidgetByCaption(frameID, "Button", btnCaption);
                        std::string status;

                        // Compute flow position for this button's index
                        QRect geo = VCBridge::computeFlowPosition(
                            frameWidth, headerHeight, btnIdx, btnWidth, btnHeight, btnCols, pad);

                        if (btnID >= 0)
                        {
                            // Upsert: update existing widget geometry and properties
                            vcBridge->setWidgetGeometry(btnID, geo);
                            applyWidgetProps(btnID, btn);
                            status = "updated";
                        }
                        else
                        {
                            // Create new button
                            int funcID = -1;
                            if (btn.contains("functionName"))
                            {
                                quint32 fid = mcp::resolveFunctionByName(doc,
                                    QString::fromStdString(btn.at("functionName").get<std::string>()));
                                if (fid != Function::invalidId()) funcID = (int)fid;
                            }
                            if (btn.contains("functionID"))
                                funcID = btn.at("functionID").get<int>();

                            int fadeTime = btn.value("stopAllFadeTime", 0);
                            btnID = vcBridge->addButton(
                                frameID, geo,
                                funcID >= 0 ? (quint32)funcID : Function::invalidId(),
                                btnCaption,
                                QString::fromStdString(btn.value("action", "toggle")),
                                fadeTime);

                            // Apply colors, input mapping, and LED feedback
                            if (btnID >= 0)
                                applyWidgetProps(btnID, btn);

                            status = "created";
                        }
                        buttonsResult.push_back({{"widgetID", btnID}, {"status", status}});
                        btnIdx++;
                    }
                }
                sectionResult["buttons"] = buttonsResult;

                // Create or update sliders inside frame
                Json slidersResult = Json::array();
                if (section.contains("sliders"))
                {
                    // Sliders start below buttons
                    int sliderOffsetY = headerHeight + btnRows * (btnHeight + pad);
                    int sliderIdx = 0;
                    for (auto &sl : section.at("sliders"))
                    {
                        QString slCaption = QString::fromStdString(sl.at("caption").get<std::string>());
                        int slID = vcBridge->findWidgetByCaption(frameID, "Slider", slCaption);
                        std::string status;

                        // Compute flow position for this slider's index
                        QRect geo = VCBridge::computeFlowPosition(
                            frameWidth, sliderOffsetY, sliderIdx, sliderWidth, sliderHeight, sliderCols, pad);

                        if (slID >= 0)
                        {
                            // Upsert: update existing widget geometry and properties
                            vcBridge->setWidgetGeometry(slID, geo);
                            applyWidgetProps(slID, sl);
                            status = "updated";
                        }
                        else
                        {
                            // Create new slider
                            int funcID = -1;
                            if (sl.contains("functionName"))
                            {
                                quint32 fid = mcp::resolveFunctionByName(doc,
                                    QString::fromStdString(sl.at("functionName").get<std::string>()));
                                if (fid != Function::invalidId()) funcID = (int)fid;
                            }
                            if (sl.contains("functionID"))
                                funcID = sl.at("functionID").get<int>();

                            QList<QPair<quint32, quint32>> channels;
                            if (sl.contains("channels") && sl.at("channels").is_array())
                            {
                                for (auto &ch : sl.at("channels"))
                                {
                                    if (ch.contains("fixtureID") && ch.contains("channel"))
                                        channels.append(qMakePair(
                                            (quint32)ch.at("fixtureID").get<int>(),
                                            (quint32)ch.at("channel").get<int>()));
                                }
                            }
                            slID = vcBridge->addSlider(
                                frameID, geo,
                                QString::fromStdString(sl.at("mode").get<std::string>()),
                                slCaption,
                                funcID,
                                channels);

                            // Apply colors and input mapping
                            if (slID >= 0)
                                applyWidgetProps(slID, sl);

                            status = "created";
                        }
                        slidersResult.push_back({{"widgetID", slID}, {"status", status}});
                        sliderIdx++;
                    }
                }
                sectionResult["sliders"] = slidersResult;

                sectionsResult.push_back(sectionResult);
            }

            Json result;
            result["pageIndex"] = pageIndex;
            result["sections"] = sectionsResult;
            return result.dump();
            });
        },
        std::nullopt,
        std::string("Build a complete Virtual Console page in one call with upsert semantics. Sections become frames (solo=true for mutually exclusive moods). "
                     "Buttons and sliders auto-layout horizontally in rows using full page width. Use 'columns' per section to control items per row. "
                     "Supports per-widget colors (bgColor/fgColor), Launchpad input mapping (inputUniverse/inputChannel), "
                     "and LED feedback (idleValue/activeValue/monitorValue with static/flashing/pulsing modes). "
                     "Existing widgets are updated (not skipped). Use functionName to reference functions by name."),
        std::nullopt
    ));
    } // end build_show_page schema scope

    // query_widget_details (batch)
    tm.register_tool(Tool(
        "query_widget_details",
        Json{{"type", "object"}, {"properties", {
            {"widgetIDs", {{"type", "array"}, {"items", {{"type", "integer"}}},
                {"description", "Widget IDs to query. Returns full details for each."}}}
        }}, {"required", {"widgetIDs"}}},
        Json{},
        [doc, vcBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            for (auto &wid : args.at("widgetIDs"))
            {
                int id = wid.get<int>();
                auto d = vcBridge->getWidgetDetails(id);
                if (d.id < 0)
                {
                    results.push_back({{"id", id}, {"error", "not found"}});
                    continue;
                }
                Json entry;
                entry["id"] = d.id;
                entry["type"] = d.type.toStdString();
                entry["caption"] = d.caption.toStdString();
                entry["geometry"] = {{"x", d.geometry.x()}, {"y", d.geometry.y()},
                                     {"width", d.geometry.width()}, {"height", d.geometry.height()}};
                entry["parentID"] = d.parentID;

                if (d.functionID != 0 && d.functionID != (quint32)-1)
                    entry["functionID"] = (int)d.functionID;
                if (!d.action.isEmpty())
                    entry["action"] = d.action.toStdString();
                if (!d.sliderMode.isEmpty())
                    entry["sliderMode"] = d.sliderMode.toStdString();

                if (!d.channels.isEmpty())
                {
                    Json chArr = Json::array();
                    for (auto &ch : d.channels)
                        chArr.push_back({{"fixtureID", (int)ch.first}, {"channel", (int)ch.second}});
                    entry["channels"] = chArr;
                }

                Json inputs = Json::array();
                for (auto &m : d.inputMappings)
                    inputs.push_back({{"universe", (int)m.universe}, {"channel", (int)m.channel}});
                entry["inputMappings"] = inputs;

                if (d.bgColor.isValid())
                    entry["bgColor"] = d.bgColor.name().toStdString();
                if (d.fgColor.isValid())
                    entry["fgColor"] = d.fgColor.name().toStdString();

                if (!d.inputMappings.isEmpty())
                {
                    entry["feedback"] = {
                        {"idleValue", d.feedback.idleValue},
                        {"activeValue", d.feedback.activeValue},
                        {"monitorValue", d.feedback.monitorValue},
                        {"idleMidiCh", d.feedback.idleMidiCh},
                        {"activeMidiCh", d.feedback.activeMidiCh},
                        {"monitorMidiCh", d.feedback.monitorMidiCh}
                    };
                }
                results.push_back(entry);
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("Query full details of Virtual Console widgets: type, caption, geometry, function, action, "
                     "slider mode, channels, input mappings, colors, LED feedback. Batch."),
        std::nullopt
    ));

    // update_widgets (batch — sparse update)
    tm.register_tool(Tool(
        "update_widgets",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"widgetID", {{"type", "integer"}}},
                {"caption", {{"type", "string"}, {"description", "New widget caption/label"}}},
                {"x", {{"type", "integer"}, {"description", "New X position"}}},
                {"y", {{"type", "integer"}, {"description", "New Y position"}}},
                {"width", {{"type", "integer"}, {"description", "New width"}}},
                {"height", {{"type", "integer"}, {"description", "New height"}}},
                {"functionID", {{"type", "integer"}, {"description", "New function ID (buttons: controlled function, sliders: playback function)"}}},
                {"action", {{"type", "string"}, {"enum", {"toggle", "flash", "blackout", "stopall"}},
                    {"description", "Button action type"}}},
                {"mode", {{"type", "string"}, {"enum", {"level", "playback", "submaster"}},
                    {"description", "Slider mode"}}},
                {"channels", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                    {"fixtureID", {{"type", "integer"}}},
                    {"channel", {{"type", "integer"}}}
                }}}}, {"description", "Slider level-mode channels (replaces existing)"}}},
                {"bgColor", {{"type", "string"}, {"description", "Background color hex"}}},
                {"fgColor", {{"type", "string"}, {"description", "Foreground color hex"}}}
            }}, {"required", {"widgetID"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc, vcBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            for (auto &item : args.at("items"))
            {
                int wid = item.at("widgetID").get<int>();
                Json changes = Json::array();

                if (item.contains("caption"))
                {
                    bool ok = vcBridge->setWidgetCaption(wid,
                        QString::fromStdString(item.at("caption").get<std::string>()));
                    changes.push_back({{"property", "caption"}, {"status", ok ? "ok" : "failed"}});
                }

                // Geometry: update only specified fields, preserve others
                if (item.contains("x") || item.contains("y") ||
                    item.contains("width") || item.contains("height"))
                {
                    auto d = vcBridge->getWidgetDetails(wid);
                    QRect geo = d.geometry;
                    if (item.contains("x")) geo.setX(item.at("x").get<int>());
                    if (item.contains("y")) geo.setY(item.at("y").get<int>());
                    if (item.contains("width")) geo.setWidth(item.at("width").get<int>());
                    if (item.contains("height")) geo.setHeight(item.at("height").get<int>());
                    vcBridge->setWidgetGeometry(wid, geo);
                    changes.push_back({{"property", "geometry"}, {"status", "ok"}});
                }

                if (item.contains("functionID"))
                {
                    int fid = item.at("functionID").get<int>();
                    bool ok = vcBridge->setButtonFunction(wid, fid);
                    if (!ok) ok = vcBridge->setSliderFunction(wid, fid);
                    changes.push_back({{"property", "functionID"}, {"status", ok ? "ok" : "failed"}});
                }

                if (item.contains("action"))
                {
                    bool ok = vcBridge->setButtonAction(wid,
                        QString::fromStdString(item.at("action").get<std::string>()));
                    changes.push_back({{"property", "action"}, {"status", ok ? "ok" : "failed"}});
                }

                if (item.contains("mode"))
                {
                    bool ok = vcBridge->setSliderMode(wid,
                        QString::fromStdString(item.at("mode").get<std::string>()));
                    changes.push_back({{"property", "mode"}, {"status", ok ? "ok" : "failed"}});
                }

                if (item.contains("channels"))
                {
                    QList<QPair<quint32, quint32>> channels;
                    for (auto &ch : item.at("channels"))
                        channels.append(qMakePair(
                            (quint32)ch.at("fixtureID").get<int>(),
                            (quint32)ch.at("channel").get<int>()));
                    bool ok = vcBridge->setSliderChannels(wid, channels);
                    changes.push_back({{"property", "channels"}, {"status", ok ? "ok" : "failed"}});
                }

                if (item.contains("bgColor") || item.contains("fgColor"))
                {
                    QColor bg = item.contains("bgColor")
                        ? QColor(QString::fromStdString(item.at("bgColor").get<std::string>()))
                        : QColor();
                    QColor fg = item.contains("fgColor")
                        ? QColor(QString::fromStdString(item.at("fgColor").get<std::string>()))
                        : QColor();
                    vcBridge->setWidgetColors(wid, bg, fg);
                    changes.push_back({{"property", "colors"}, {"status", "ok"}});
                }

                results.push_back({{"widgetID", wid}, {"changes", changes}});
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("Update Virtual Console widget properties. Sparse: only provided fields are changed, "
                     "others are preserved. Supports caption, geometry (x/y/width/height), functionID, "
                     "action (buttons), mode (sliders), channels (sliders), colors. Batch."),
        std::nullopt
    ));

    // reparent_widgets (batch)
    tm.register_tool(Tool(
        "reparent_widgets",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"widgetID", {{"type", "integer"}}},
                {"newParentID", {{"type", "integer"}, {"description", "Target frame widget ID"}}},
                {"x", {{"type", "integer"}, {"description", "X position in new parent (default 5)"}}},
                {"y", {{"type", "integer"}, {"description", "Y position in new parent (default 5)"}}},
                {"width", {{"type", "integer"}, {"description", "Width (preserved from current if omitted)"}}},
                {"height", {{"type", "integer"}, {"description", "Height (preserved from current if omitted)"}}}
            }}, {"required", {"widgetID", "newParentID"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc, vcBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            for (auto &item : args.at("items"))
            {
                int wid = item.at("widgetID").get<int>();
                int newParent = item.at("newParentID").get<int>();

                // Get current geometry to preserve size if not specified
                auto d = vcBridge->getWidgetDetails(wid);
                int x = item.value("x", 5);
                int y = item.value("y", 5);
                int w = item.value("width", d.geometry.width());
                int h = item.value("height", d.geometry.height());

                bool ok = vcBridge->reparentWidget(wid, newParent, QRect(x, y, w, h));
                results.push_back({
                    {"widgetID", wid},
                    {"newParentID", newParent},
                    {"status", ok ? "ok" : "failed"}
                });
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("Move Virtual Console widgets between frames. Preserves all properties (input mappings, "
                     "colors, LED feedback, function bindings). Widget size is preserved if not specified. Batch."),
        std::nullopt
    ));

    // delete_widgets (batch)
    tm.register_tool(Tool(
        "delete_widgets",
        Json{{"type", "object"}, {"properties", {
            {"ids", {{"type", "array"}, {"items", {{"type", "integer"}}}, {"description", "Widget IDs to delete"}}}
        }}, {"required", {"ids"}}},
        Json{},
        [doc, vcBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            for (auto &wid : args.at("ids"))
            {
                int id = wid.get<int>();
                bool ok = vcBridge->removeWidget(id);
                results.push_back({{"id", id}, {"status", ok ? "deleted" : "not found"}});
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("Delete Virtual Console widgets by ID. Batch."),
        std::nullopt
    ));
}
