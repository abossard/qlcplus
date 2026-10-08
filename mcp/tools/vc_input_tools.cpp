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
#include "vc_tools_common.h"
#include "doc.h"

#include <QKeySequence>
#include <limits>

#include <fastmcpp/tools/manager.hpp>
#include <fastmcpp/tools/tool.hpp>

void registerVCInputTools(fastmcpp::tools::ToolManager &tm, Doc *doc, VCBridge *vcBridge)
{
    using Json = nlohmann::json;
    using Tool = fastmcpp::tools::Tool;

    if (!vcBridge) return;

    static const char *sourceNameDesc =
        "Input source name. Per widget type: "
        "Button: 'default'. "
        "Slider: 'default', 'overrideReset', 'flashButton'. "
        "CueList: 'next', 'previous', 'playback', 'stop', 'sideFader'. "
        "XYPad: 'pan', 'panFine', 'tilt', 'tiltFine', 'width', 'height', 'preset0'..'presetN'. "
        "SpeedDial: 'absolute', 'tap', 'mult', 'div', 'multDivReset', 'apply', "
        "'1_16x','1_8x','1_4x','1_2x','2x','4x','8x','16x', 'preset0'..'presetN'. "
        "Clock: 'play', 'reset'. "
        "Frame/SoloFrame: 'nextPage', 'previousPage', 'enable', 'collapse', 'shortcut0'..'shortcutN'. "
        "AudioTriggers: 'default', 'volumeControl'. "
        "Matrix: 'default'.";

    // vc_map_inputs (batch)
    tm.register_tool(Tool(
        "vc_map_inputs",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"widgetID", {{"type", "integer"}}},
                {"inputUniverse", {{"type", "integer"}}},
                {"inputChannel", {{"type", "integer"}}},
                {"sourceName", {{"type", "string"}, {"description", sourceNameDesc}}},
                {"idleValue", {{"type", "integer"}, {"description", "LED color when inactive (velocity from color table, 0=off)"}}},
                {"activeValue", {{"type", "integer"}, {"description", "LED color when active (velocity from color table)"}}},
                {"monitorValue", {{"type", "integer"}, {"description", "LED color for monitor/intermediate state (velocity from color table)"}}},
                {"idleChannel", {{"type", "integer"}, {"description", "MIDI channel for idle state (from profile's MIDI channel table)"}}},
                {"activeChannel", {{"type", "integer"}, {"description", "MIDI channel for active state (from profile's MIDI channel table)"}}},
                {"monitorChannel", {{"type", "integer"}, {"description", "MIDI channel for monitor state (from profile's MIDI channel table)"}}}
            }}, {"required", {"widgetID", "inputUniverse", "inputChannel"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc, vcBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto itemsErr = validateItemsArray(args);
            if (itemsErr) return *itemsErr;
            static const char *kFeedbackFields[] = {"idleValue", "activeValue", "monitorValue",
                                                    "idleChannel", "activeChannel", "monitorChannel"};
            Json results = Json::array();
            const Json &items = args.at("items");
            for (size_t i = 0; i < items.size(); ++i)
            {
                const Json &item = items[i];
                auto itemErr = validateFields(item, {"widgetID", "inputUniverse", "inputChannel",
                    "sourceName", "idleValue", "activeValue", "monitorValue",
                    "idleChannel", "activeChannel", "monitorChannel"});
                if (!itemErr.empty()) { results.push_back(mcp::itemErrorFromDump(i, itemErr)); continue; }

                std::string rangeErr;
                auto checked = [&](const char *key, int64_t hi) -> int64_t {
                    auto v = item.contains(key) ? mcp::jsonInteger(item.at(key), 0, hi) : std::nullopt;
                    if (!v && rangeErr.empty()) rangeErr = mcp::integerError(key, 0, hi);
                    return v.value_or(0);
                };
                const int wid = int(checked("widgetID", std::numeric_limits<int>::max()));
                const quint32 uni = quint32(checked("inputUniverse", mcp::kMaxId));
                const quint32 ch = quint32(checked("inputChannel", mcp::kMaxId));

                // Feedback is all-or-none: any one of the six fields requires the other five.
                int supplied = 0;
                for (const char *key : kFeedbackFields)
                    supplied += item.contains(key) ? 1 : 0;
                const bool hasFeedback = supplied > 0;
                int fb[6] = {0, 0, 0, 0, 0, 0};
                if (hasFeedback && rangeErr.empty())
                {
                    if (supplied != 6)
                    {
                        results.push_back(mcp::itemError(i, "all 6 feedback fields required when any feedback is supplied "
                                      "(idleValue, activeValue, monitorValue, idleChannel, activeChannel, monitorChannel)"));
                        continue;
                    }
                    for (int f = 0; f < 6; ++f)
                        fb[f] = int(checked(kFeedbackFields[f], 255));
                }
                if (!rangeErr.empty()) { results.push_back(mcp::itemError(i, rangeErr)); continue; }

                std::string srcName = item.value("sourceName", "default");
                QString qSrcName = QString::fromStdString(srcName);

                // Snapshot existing feedback for this specific source before remap
                VCBridge::FeedbackInfo savedFb = vcBridge->getWidgetFeedbackByName(wid, qSrcName);
                bool hadFeedback = (savedFb.idleValue != 0 || savedFb.activeValue != 0 ||
                                    savedFb.monitorValue != 0);

                // Always map by name — works for all widget types and source counts
                bool ok = vcBridge->mapWidgetInputByName(wid, qSrcName, uni, ch);

                if (!ok)
                {
                    Json rec = mcp::itemError(i, "mapping failed (invalid sourceName or widgetID)");
                    rec["widgetID"] = wid;
                    results.push_back(rec);
                    continue;
                }

                // Apply feedback (source-specific)
                if (hasFeedback)
                {
                    vcBridge->setWidgetFeedbackByName(wid, qSrcName,
                        fb[0], fb[1], fb[2], fb[3], fb[4], fb[5]);
                }
                else if (hadFeedback)
                {
                    vcBridge->setWidgetFeedbackByName(wid, qSrcName,
                        savedFb.idleValue, savedFb.activeValue, savedFb.monitorValue,
                        savedFb.idleMidiCh, savedFb.activeMidiCh, savedFb.monitorMidiCh);
                }

                results.push_back(mcp::itemOk(i, {{"widgetID", wid}}));
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("Map external controller inputs (OSC/MIDI faders) to Virtual Console widgets. "
                     "Optionally set LED feedback in the same call (all 6 feedback fields required together). "
                     "Feedback is preserved across remaps if not explicitly supplied. Batch. "
                     "Wrap multiple operations in {\"items\": [...]}. Each item is processed independently."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotIdempotent));

    // vc_configure_feedback (batch)
    tm.register_tool(Tool(
        "vc_configure_feedback",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"widgetID", {{"type", "integer"}}},
                {"sourceName", {{"type", "string"}, {"description", sourceNameDesc}}},
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
            auto itemsErr = validateItemsArray(args);
            if (itemsErr) return *itemsErr;
            auto midiChFromMode = [](const std::string &mode) -> int {
                if (mode == "flashing") return 1;
                if (mode == "pulsing") return 2;
                return 0;
            };
            static const Json kModeEnums = {
                {"idleMode", {{"enum", {"static", "flashing", "pulsing"}}}},
                {"activeMode", {{"enum", {"static", "flashing", "pulsing"}}}},
                {"monitorMode", {{"enum", {"static", "flashing", "pulsing"}}}}
            };
            Json results = Json::array();
            const Json &items = args.at("items");
            for (size_t i = 0; i < items.size(); ++i)
            {
                const Json &item = items[i];
                auto err = validateFields(item, {"widgetID", "activeValue", "idleValue", "monitorValue",
                    "sourceName", "idleChannel", "activeChannel", "monitorChannel",
                    "idleMode", "activeMode", "monitorMode"});
                if (err.empty()) err = validateEnums(item, kModeEnums);
                if (!err.empty()) { results.push_back(mcp::itemErrorFromDump(i, err)); continue; }

                std::string rangeErr;
                auto checked = [&](const char *key, int64_t hi, int64_t fallback, bool required) -> int64_t {
                    if (!item.contains(key))
                    {
                        if (required && rangeErr.empty()) rangeErr = mcp::integerError(key, 0, hi);
                        return fallback;
                    }
                    auto v = mcp::jsonInteger(item.at(key), 0, hi);
                    if (!v && rangeErr.empty()) rangeErr = mcp::integerError(key, 0, hi);
                    return v.value_or(fallback);
                };
                auto mode = [&](const char *key, const char *fallback) {
                    return midiChFromMode(VCValidate::canonicalEnum(item.value(key, fallback), kModeEnums[key]["enum"]));
                };
                const int wid = int(checked("widgetID", std::numeric_limits<int>::max(), 0, true));
                const int activeVal = int(checked("activeValue", 255, 0, true));
                const int idleVal = int(checked("idleValue", 255, 0, false));
                const int monitorVal = int(checked("monitorValue", 255, 0, false));
                // Integer channel fields take precedence; fall back to string mode names
                const int midiChIdle = int(checked("idleChannel", 255, mode("idleMode", "static"), false));
                const int midiChActive = int(checked("activeChannel", 255, mode("activeMode", "static"), false));
                const int midiChMonitor = int(checked("monitorChannel", 255, mode("monitorMode", ""), false));
                if (!rangeErr.empty()) { results.push_back(mcp::itemError(i, rangeErr)); continue; }

                QString srcName = QString::fromStdString(item.value("sourceName", "default"));
                bool ok = vcBridge->setWidgetFeedbackByName(wid, srcName,
                    idleVal, activeVal, monitorVal, midiChIdle, midiChActive, midiChMonitor);

                // Fall back to legacy per-widget feedback if sourceName is default
                if (!ok && srcName == QStringLiteral("default"))
                    ok = vcBridge->setWidgetFeedback(wid, idleVal, activeVal, monitorVal,
                                                      midiChIdle, midiChActive, midiChMonitor);

                Json rec = ok ? mcp::itemOk(i, {{"outcome", "updated"}})
                              : mcp::itemError(i, "feedback not applied (unknown widgetID or sourceName)");
                rec["widgetID"] = wid;
                results.push_back(rec);
            }
            return results.dump();
            } catch (const std::exception &e) {
                return Json({{"error", e.what()}}).dump();
            }
            });
        },
        std::nullopt,
        std::string("Set LED feedback colors and animation mode per widget input source. "
                     "Use sourceName to target a specific source (default 'default'). "
                     "Use integer idleChannel/activeChannel/monitorChannel (from query_feedback_profile) "
                     "or legacy string idleMode/activeMode/monitorMode. Batch. "
                     "Wrap multiple operations in {\"items\": [...]}. Each item is processed independently."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotIdempotent));

    // vc_set_key_sequences (batch)
    tm.register_tool(Tool(
        "vc_set_key_sequences",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"widgetID", {{"type", "integer"}}},
                {"sourceName", {{"type", "string"}, {"description", sourceNameDesc}}},
                {"keySequence", {{"type", "string"}, {"description", "Key sequence string (e.g. 'Ctrl+A', 'Space', 'F5')"}}}
            }}, {"required", {"widgetID", "keySequence"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc, vcBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto itemsErr = validateItemsArray(args);
            if (itemsErr) return *itemsErr;
            Json results = Json::array();
            const Json &items = args.at("items");
            for (size_t i = 0; i < items.size(); ++i)
            {
                const Json &item = items[i];
                auto err = validateFields(item, {"widgetID", "sourceName", "keySequence"});
                if (!err.empty()) { results.push_back(mcp::itemErrorFromDump(i, err)); continue; }
                auto widgetID = item.contains("widgetID")
                    ? mcp::jsonInteger(item.at("widgetID"), 0, std::numeric_limits<int>::max()) : std::nullopt;
                if (!widgetID)
                {
                    results.push_back(mcp::itemError(i, mcp::integerError("widgetID", 0, std::numeric_limits<int>::max())));
                    continue;
                }
                if (!item.contains("keySequence") || !item.at("keySequence").is_string())
                {
                    results.push_back(mcp::itemError(i, "keySequence must be a string"));
                    continue;
                }
                QString sourceName = QString::fromStdString(item.value("sourceName", "default"));
                QKeySequence ks(QString::fromStdString(item.at("keySequence").get<std::string>()));
                bool ok = vcBridge->setWidgetKeySequence(int(*widgetID), sourceName, ks);
                Json rec = ok ? mcp::itemOk(i, {{"outcome", "updated"}})
                              : mcp::itemError(i, "failed to set key sequence");
                rec["widgetID"] = int(*widgetID);
                results.push_back(rec);
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("Set keyboard shortcuts on Virtual Console widgets. Batch. "
                     "Wrap multiple operations in {\"items\": [...]}. Each item is processed independently."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotIdempotent));
}
