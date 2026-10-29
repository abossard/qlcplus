#include "tool_registry.h"
#include "doc.h"
#include "vcbridge.h"
#include "workspacebridge.h"
#include <QJsonDocument>
#include <fastmcpp/tools/manager.hpp>
#include <fastmcpp/tools/tool.hpp>

namespace {
using Json = nlohmann::json;

std::string serialized(const QVariantMap &state)
{
    return QJsonDocument::fromVariant(state).toJson(QJsonDocument::Compact).toStdString();
}
}

void registerLiveControlTools(fastmcpp::tools::ToolManager &tm, Doc *doc,
                             VCBridge *vcBridge, WorkspaceBridge *wsBridge)
{
    using Tool = fastmcpp::tools::Tool;
    if (wsBridge)
    {
        tm.register_tool(Tool(
            "quit_app",
            Json{{"type", "object"}, {"properties", {
                {"discard", {{"type", "boolean"}, {"default", false},
                             {"description", "Explicitly discard unsaved changes and armed recording"}}}
            }}}, Json{},
            [doc, wsBridge](const Json &args) -> Json {
                return execOnMainThread(doc, [&]() -> Json {
                    const auto err = validateFields(args, {"discard"});
                    if (!err.empty()) return err;
                    if (!args.is_object())
                        return Json({{"error", "arguments must be an object"}}).dump();
                    if (args.contains("discard") && !args.at("discard").is_boolean())
                        return Json({{"error", "discard must be a boolean"}}).dump();
                    const bool discard = args.value("discard", false);
                    if (!discard && (doc->isModified() || wsBridge->isRecording()))
                        return Json({{"error", "workspace has unsaved changes or recording is armed; "
                                              "save or pass discard: true"}}).dump();
                    if (!wsBridge->quitApp(discard))
                        return Json({{"error", "could not queue application exit"}}).dump();
                    return Json({{"status", "accepted"}}).dump();
                });
            }
        ).set_description("Queue the application's normal exit path. Refuses unsaved changes or "
                          "armed recording unless discard is true. The connection closes on exit.")
         .set_annotations(mcp::kAnnotDestructive));
    }
    if (!vcBridge)
        return;

    tm.register_tool(Tool(
        "set_perform_mode",
        Json{{"type", "object"}, {"properties", {
            {"on", {{"type", "boolean"}}}
        }}, {"required", {"on"}}},
        Json{},
        [doc, vcBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
                const auto err = validateFields(args, {"on"});
                if (!err.empty()) return err;
                if (!args.contains("on") || !args.at("on").is_boolean())
                    return Json({{"error", "on is required and must be a boolean"}}).dump();
                return serialized(vcBridge->setPerformMode(args.at("on").get<bool>()));
            });
        }
    ).set_description("Enable or disable DJ Perform through the DJ Manager. Returns DJ state.")
     .set_annotations(mcp::kAnnotOpenWorld));

    tm.register_tool(Tool(
        "get_dj_state", Json{{"type", "object"}, {"properties", Json::object()}}, Json{},
        [doc, vcBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
                const auto err = validateFields(args, {});
                if (!err.empty()) return err;
                if (!args.is_object())
                    return Json({{"error", "arguments must be an object"}}).dump();
                return serialized(vcBridge->djState());
            });
        }
    ).set_description("Read Perform mode, the active deck's state and Show, each deck's song, elapsedMs and "
                      "mix weight, and drivenShows: every Show Perform plays, with its deck, state, dominant "
                      "(the active deck's), masterIntensity (its deck's weight), transport "
                      "(running|paused|queued|stopped), external clock and positionMs. "
                      "activeShowId is null when no Show is resolved; activeDeck is 0 when none.")
     .set_annotations(mcp::kAnnotReadOnly));

    tm.register_tool(Tool(
        "press_vc_widget",
        Json{{"type", "object"}, {"properties", {
            {"widget", {{"type", "integer"}, {"minimum", 0}, {"maximum", std::numeric_limits<int>::max()},
                        {"description", "VC button widget ID"}}},
            {"action", {{"type", "string"}, {"enum", {"press", "release", "toggle"}}}}
        }}, {"required", {"widget", "action"}}},
        Json{},
        [doc, vcBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
                const auto err = validateFields(args, {"widget", "action"});
                if (!err.empty()) return err;
                const auto widget = args.contains("widget")
                    ? mcp::jsonInteger(args.at("widget"), 0, std::numeric_limits<int>::max()) : std::nullopt;
                if (!widget)
                    return Json({{"error", "widget is required and must be a nonnegative integer widget ID"}}).dump();
                static const Json enums = {{"action", {{"enum", {"press", "release", "toggle"}}}}};
                const auto enumError = validateEnums(args, enums);
                if (!enumError.empty()) return enumError;
                if (!args.contains("action") || !args.at("action").is_string())
                    return Json({{"error", "action is required and must be a string"}}).dump();
                return serialized(vcBridge->pressWidget(int(*widget),
                    QString::fromStdString(toLowerStd(args.at("action").get<std::string>()))));
            });
        }
    ).set_description("Submit live VC button input through the same recorder-aware entry as the mouse. "
                      "press activates, release releases momentary buttons (no-op for latched buttons), "
                      "toggle flips the button's current state. Requires VC play mode and an enabled, "
                      "visible button. Accepted input may wait for pending replay work.")
     .set_annotations(Json{{"readOnlyHint", false}, {"destructiveHint", false},
                           {"idempotentHint", false}, {"openWorldHint", true}}));

    tm.register_tool(Tool(
        "set_recording",
        Json{{"type", "object"}, {"properties", {
            {"on", {{"type", "boolean"}}}
        }}, {"required", {"on"}}},
        Json{},
        [doc, vcBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
                const auto err = validateFields(args, {"on"});
                if (!err.empty()) return err;
                if (!args.contains("on") || !args.at("on").is_boolean())
                    return Json({{"error", "on is required and must be a boolean"}}).dump();
                return serialized(vcBridge->setRecording(args.at("on").get<bool>()));
            });
        }
    ).set_description("Arm or disarm the shared command recorder, exactly like REC. Does not start "
                      "transport. Disarming can fail while keeping REC on; returns recording state.")
     .set_annotations(mcp::kAnnotIdempotent));

    tm.register_tool(Tool(
        "get_recording_state", Json{{"type", "object"}, {"properties", Json::object()}}, Json{},
        [doc, vcBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
                const auto err = validateFields(args, {});
                if (!err.empty()) return err;
                if (!args.is_object())
                    return Json({{"error", "arguments must be an object"}}).dump();
                return serialized(vcBridge->recordingState());
            });
        }
    ).set_description("Read REC, its phase, boundShowId (null when unbound), and lastError.")
     .set_annotations(mcp::kAnnotReadOnly));
}
