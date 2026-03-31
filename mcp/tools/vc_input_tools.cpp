/*
  Q Light Controller Plus
  vc_input_tools.cpp

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

#include <QKeySequence>

#include <fastmcpp/tools/manager.hpp>
#include <fastmcpp/tools/tool.hpp>

void registerVCInputTools(fastmcpp::tools::ToolManager &tm, Doc *doc, VCBridge *vcBridge)
{
    using Json = nlohmann::json;
    using Tool = fastmcpp::tools::Tool;

    if (!vcBridge) return;

    // vc_map_inputs (batch)
    tm.register_tool(Tool(
        "vc_map_inputs",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"widgetID", {{"type", "integer"}}},
                {"inputUniverse", {{"type", "integer"}}},
                {"inputChannel", {{"type", "integer"}}},
                {"sourceName", {{"type", "string"}, {"description", "Input source name: 'default', or widget-specific names like 'next'/'play'/'tap' etc."}}},
                {"idleValue", {{"type", "integer"}, {"description", "LED color when inactive (velocity from color table, 0=off)"}}},
                {"activeValue", {{"type", "integer"}, {"description", "LED color when active (velocity from color table)"}}},
                {"monitorValue", {{"type", "integer"}, {"description", "LED color for monitor/intermediate state (velocity from color table)"}}},
                {"idleChannel", {{"type", "integer"}, {"description", "MIDI channel for idle state (from profile's MIDI channel table)"}}},
                {"activeChannel", {{"type", "integer"}, {"description", "MIDI channel for active state (from profile's MIDI channel table)"}}},
                {"monitorChannel", {{"type", "integer"}, {"description", "MIDI channel for monitor state (from profile's MIDI channel table)"}}}
            }}, {"required", {"widgetID", "inputUniverse", "inputChannel"}}}}}},
            {"mode", {{"type", "string"}, {"enum", {"replace", "add"}},
                {"description", "replace (default): clear existing inputs first. add: append without clearing."}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc, vcBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto err = validateFields(args, {"items", "mode"});
            if (!err.empty()) return err;
            Json results = Json::array();
            std::string mode = args.value("mode", "replace");
            for (auto &item : args.at("items"))
            {
                auto itemErr = validateFields(item, {"widgetID", "inputUniverse", "inputChannel",
                    "sourceName", "idleValue", "activeValue", "monitorValue",
                    "idleChannel", "activeChannel", "monitorChannel"});
                if (!itemErr.empty()) { results.push_back(nlohmann::json::parse(itemErr)); continue; }
                int wid = item.at("widgetID").get<int>();
                quint32 uni = item.at("inputUniverse").get<int>();
                quint32 ch = item.at("inputChannel").get<int>();

                // Check for multiple sources (not supported)
                int srcCount = vcBridge->widgetInputSourceCount(wid);
                if (srcCount > 1)
                {
                    results.push_back({{"widgetID", wid}, {"status", "error"},
                        {"error", "widget has multiple input sources; not supported"}});
                    continue;
                }

                // Determine feedback: use supplied values or preserve existing
                bool hasFeedback = item.contains("activeValue");
                if (hasFeedback)
                {
                    // All 6 fields required when feedback is supplied
                    if (!item.contains("idleValue") || !item.contains("monitorValue") ||
                        !item.contains("idleChannel") || !item.contains("activeChannel") ||
                        !item.contains("monitorChannel"))
                    {
                        results.push_back({{"widgetID", wid}, {"status", "error"},
                            {"error", "all 6 feedback fields required when any feedback is supplied "
                                      "(idleValue, activeValue, monitorValue, idleChannel, activeChannel, monitorChannel)"}});
                        continue;
                    }
                }

                // Snapshot existing feedback before replacing
                VCBridge::FeedbackInfo savedFb;
                bool hadExistingSource = (srcCount == 1);
                if (hadExistingSource)
                    savedFb = vcBridge->getWidgetFeedback(wid);

                // Map the input (replace or add), with optional sourceName
                std::string srcName = item.value("sourceName", "default");
                bool ok;
                if (srcName != "default")
                {
                    ok = vcBridge->mapWidgetInputByName(wid, QString::fromStdString(srcName), uni, ch);
                }
                else if (mode == "add" && srcCount == 0)
                    ok = vcBridge->addWidgetInput(wid, uni, ch);
                else
                    ok = vcBridge->mapWidgetInput(wid, uni, ch);

                if (!ok)
                {
                    results.push_back({{"widgetID", wid}, {"status", "failed"}});
                    continue;
                }

                // Apply feedback
                if (hasFeedback)
                {
                    vcBridge->setWidgetFeedback(wid,
                        item.at("idleValue").get<int>(),
                        item.at("activeValue").get<int>(),
                        item.at("monitorValue").get<int>(),
                        item.at("idleChannel").get<int>(),
                        item.at("activeChannel").get<int>(),
                        item.at("monitorChannel").get<int>());
                }
                else if (hadExistingSource)
                {
                    // Preserve previous feedback
                    vcBridge->setWidgetFeedback(wid,
                        savedFb.idleValue, savedFb.activeValue, savedFb.monitorValue,
                        savedFb.idleMidiCh, savedFb.activeMidiCh, savedFb.monitorMidiCh);
                }

                results.push_back({{"widgetID", wid}, {"status", ok ? "ok" : "failed"}});
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("Map external controller inputs (OSC/MIDI faders) to Virtual Console widgets. "
                     "Optionally set LED feedback in the same call (all 6 feedback fields required together). "
                     "Feedback is preserved across remaps if not explicitly supplied. Batch."),
        std::nullopt
    ));

    // vc_configure_feedback (batch)
    tm.register_tool(Tool(
        "vc_configure_feedback",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"widgetID", {{"type", "integer"}}},
                {"idleValue", {{"type", "integer"}, {"description", "LED color when inactive (velocity from color table, 0=off)"}}},
                {"activeValue", {{"type", "integer"}, {"description", "LED color when active"}}},
                {"monitorValue", {{"type", "integer"}, {"description", "LED color for monitor/intermediate state"}}},
                {"idleChannel", {{"type", "integer"}, {"description", "MIDI channel for idle state (from profile's MIDI channel table, default 0)"}}},
                {"activeChannel", {{"type", "integer"}, {"description", "MIDI channel for active state (from profile's MIDI channel table, default 0)"}}},
                {"monitorChannel", {{"type", "integer"}, {"description", "MIDI channel for monitor state (from profile's MIDI channel table, default 0)"}}},
                {"idleMode", {{"type", "string"}, {"enum", {"static", "flashing", "pulsing"}}, {"description", "Deprecated: use idleChannel instead. LED animation mode for idle state (default static)"}}},
                {"activeMode", {{"type", "string"}, {"enum", {"static", "flashing", "pulsing"}}, {"description", "Deprecated: use activeChannel instead. LED animation mode for active state (default static)"}}},
                {"monitorMode", {{"type", "string"}, {"enum", {"static", "flashing", "pulsing"}}, {"description", "Deprecated: use monitorChannel instead. LED animation mode for monitor state"}}}
            }}, {"required", {"widgetID", "activeValue"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc, vcBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            try {
            auto midiChFromMode = [](const std::string &mode) -> int {
                if (mode == "flashing") return 1;
                if (mode == "pulsing") return 2;
                return 0;
            };
            Json results = Json::array();
            for (auto &item : args.at("items"))
            {
                auto err = validateFields(item, {"widgetID", "activeValue", "idleValue", "monitorValue",
                    "idleChannel", "activeChannel", "monitorChannel",
                    "idleMode", "activeMode", "monitorMode"});
                if (!err.empty()) { results.push_back(nlohmann::json::parse(err)); continue; }
                int wid = item.at("widgetID").get<int>();
                int activeVal = item.at("activeValue").get<int>();
                int idleVal = item.value("idleValue", 0);
                int monitorVal = item.value("monitorValue", 0);

                // Integer channel fields take precedence; fall back to string mode names
                int midiChIdle = item.contains("idleChannel") ? item.at("idleChannel").get<int>()
                    : midiChFromMode(item.value("idleMode", "static"));
                int midiChActive = item.contains("activeChannel") ? item.at("activeChannel").get<int>()
                    : midiChFromMode(item.value("activeMode", "static"));
                int midiChMonitor = item.contains("monitorChannel") ? item.at("monitorChannel").get<int>()
                    : midiChFromMode(item.value("monitorMode", ""));

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
        std::string("Set LED feedback colors and animation mode per widget. "
                     "Use integer idleChannel/activeChannel/monitorChannel (from query_feedback_profile) "
                     "or legacy string idleMode/activeMode/monitorMode. Batch."),
        std::nullopt
    ));

    // vc_set_key_sequences (batch)
    tm.register_tool(Tool(
        "vc_set_key_sequences",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"widgetID", {{"type", "integer"}}},
                {"sourceName", {{"type", "string"}, {"description", "Input source name: 'default', or widget-specific names like 'next'/'play'/'tap' etc."}}},
                {"keySequence", {{"type", "string"}, {"description", "Key sequence string (e.g. 'Ctrl+A', 'Space', 'F5')"}}}
            }}, {"required", {"widgetID", "keySequence"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc, vcBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            for (auto &item : args.at("items"))
            {
                int widgetID = item.at("widgetID").get<int>();
                QString sourceName = QString::fromStdString(item.value("sourceName", "default"));
                QString keySeqStr = QString::fromStdString(item.at("keySequence").get<std::string>());
                QKeySequence ks(keySeqStr);
                bool ok = vcBridge->setWidgetKeySequence(widgetID, sourceName, ks);
                if (ok)
                    results.push_back({{"widgetID", widgetID}, {"status", "set"}});
                else
                    results.push_back({{"widgetID", widgetID}, {"error", "failed to set key sequence"}});
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("Set keyboard shortcuts on Virtual Console widgets. Batch."),
        std::nullopt
    ));
}
