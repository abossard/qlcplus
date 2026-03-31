/*
  Q Light Controller Plus
  vc_layout_tools.cpp

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

void registerVCLayoutTools(fastmcpp::tools::ToolManager &tm, Doc *doc, VCBridge *vcBridge)
{
    using Json = nlohmann::json;
    using Tool = fastmcpp::tools::Tool;

    if (!vcBridge) return;

    // vc_reparent_widgets (batch)
    tm.register_tool(Tool(
        "vc_reparent_widgets",
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
                auto err = validateFields(item, {"widgetID", "newParentID", "x", "y", "width", "height"});
                if (!err.empty()) { results.push_back(nlohmann::json::parse(err)); continue; }
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
        std::string("Move Virtual Console widgets between frames. Preserves all properties. Batch."),
        std::nullopt
    ));

    // vc_delete_widgets (batch)
    tm.register_tool(Tool(
        "vc_delete_widgets",
        Json{{"type", "object"}, {"properties", {
            {"ids", {{"type", "array"}, {"items", {{"type", "integer"}}}, {"description", "Widget IDs to delete"}}}
        }}, {"required", {"ids"}}},
        Json{},
        [doc, vcBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto err = validateFields(args, {"ids"});
            if (!err.empty()) return err;
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

    // vc_detect_overlaps — find overlapping widgets within a frame or page
    tm.register_tool(Tool(
        "vc_detect_overlaps",
        Json{{"type", "object"}, {"properties", {
            {"parentID", {{"type", "integer"}, {"description",
                "Widget ID of a frame or page to check for overlapping children. "
                "Use a page's root frame ID or any frame ID."}}},
            {"pageIndex", {{"type", "integer"}, {"description",
                "Page index (0-based) to check. Used only if parentID is not provided."}}}
        }}},
        Json{},
        [doc, vcBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto err = validateFields(args, {"parentID", "pageIndex"});
            if (!err.empty()) return err;

            VCBridge::WidgetSnapshot snap;
            if (args.contains("parentID"))
                snap = vcBridge->snapshotFrame(args.at("parentID").get<int>());
            else if (args.contains("pageIndex"))
                snap = vcBridge->snapshotPage(args.at("pageIndex").get<int>());
            else
                return std::string("{\"error\": \"Provide parentID or pageIndex\"}");

            if (snap.id < 0 && snap.children.isEmpty())
                return std::string("{\"error\": \"Frame/page not found\"}");

            auto overlaps = VCBridge::detectOverlaps(snap.children);
            Json result;
            result["parentID"] = snap.id;
            result["childCount"] = (int)snap.children.size();
            Json overlapArr = Json::array();
            for (const auto &ov : overlaps)
            {
                overlapArr.push_back({
                    {"widgetA", ov.widgetA},
                    {"widgetB", ov.widgetB},
                    {"intersection", {
                        {"x", ov.intersection.x()}, {"y", ov.intersection.y()},
                        {"width", ov.intersection.width()}, {"height", ov.intersection.height()}
                    }}
                });
            }
            result["overlaps"] = overlapArr;
            result["overlapCount"] = (int)overlaps.size();
            return result.dump();
            });
        },
        std::nullopt,
        std::string("Detect overlapping widgets within a frame or page. Returns pairs of overlapping widget IDs with their intersection rectangles."),
        std::nullopt
    ));

    // vc_reflow_frame — reflow children within a frame (or entire page) using flow layout
    {
    Json reflowSchema = Json{{"type", "object"}, {"properties", {
        {"frameID", {{"type", "integer"}, {"description",
            "Widget ID of the frame to reflow. All children will be repositioned."}}},
        {"pageIndex", {{"type", "integer"}, {"description",
            "Page index (0-based) to reflow. Stacks all top-level frames vertically. "
            "Used only if frameID is not provided."}}},
        {"columns", {{"type", "integer"}, {"description",
            "Number of columns for flow grid. 0 or omit for auto-compute from width."}}},
        {"pad", {{"type", "integer"}, {"description", "Padding between widgets in pixels (default 5)"}}},
        {"framePad", {{"type", "integer"}, {"description", "Vertical gap between top-level frames (default 10)"}}},
        {"buttonWidth", {{"type", "integer"}, {"description", "Button width in pixels (default 100)"}}},
        {"buttonHeight", {{"type", "integer"}, {"description", "Button height in pixels (default 60)"}}},
        {"sliderWidth", {{"type", "integer"}, {"description", "Slider width in pixels (default 60)"}}},
        {"sliderHeight", {{"type", "integer"}, {"description", "Slider height in pixels (default 200)"}}},
        {"dryRun", {{"type", "boolean"}, {"description",
            "If true, compute the plan but do not apply it. Returns proposed changes without modifying widgets."}}}
    }}};
    tm.register_tool(Tool(
        "vc_reflow_frame",
        reflowSchema,
        Json{},
        [doc, vcBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto err = validateFields(args, {"frameID", "pageIndex", "columns", "pad", "framePad",
                "buttonWidth", "buttonHeight", "sliderWidth", "sliderHeight", "dryRun"});
            if (!err.empty()) return err;

            VCBridge::ReflowOptions opts;
            opts.columns = args.value("columns", 0);
            opts.pad = args.value("pad", 5);
            opts.framePad = args.value("framePad", 10);
            opts.defaultButtonWidth = args.value("buttonWidth", 100);
            opts.defaultButtonHeight = args.value("buttonHeight", 60);
            opts.defaultSliderWidth = args.value("sliderWidth", 60);
            opts.defaultSliderHeight = args.value("sliderHeight", 200);
            bool dryRun = args.value("dryRun", false);

            VCBridge::WidgetSnapshot snap;
            bool isPage = false;
            if (args.contains("frameID"))
            {
                snap = vcBridge->snapshotFrame(args.at("frameID").get<int>());
            }
            else if (args.contains("pageIndex"))
            {
                snap = vcBridge->snapshotPage(args.at("pageIndex").get<int>());
                isPage = true;
            }
            else
                return std::string("{\"error\": \"Provide frameID or pageIndex\"}");

            if (snap.id < 0 && snap.children.isEmpty())
                return std::string("{\"error\": \"Frame/page not found\"}");

            VCBridge::LayoutPlan plan;
            if (isPage)
                plan = VCBridge::reflowPage(snap, opts);
            else
            {
                int requiredHeight = VCBridge::reflowChildren(snap, opts);
                snap.geometry.setHeight(requiredHeight);
                VCBridge::collectGeometries(snap, plan);
                plan.geometries.insert(snap.id, snap.geometry);
                plan.overlaps = VCBridge::detectOverlaps(snap.children);
            }

            if (!dryRun)
                vcBridge->applyLayoutPlan(plan);

            // Build response
            Json result;
            result["applied"] = !dryRun;
            result["widgetsMoved"] = (int)plan.geometries.size();

            Json changes = Json::array();
            for (auto it = plan.geometries.constBegin(); it != plan.geometries.constEnd(); ++it)
            {
                changes.push_back({
                    {"widgetID", it.key()},
                    {"geometry", {{"x", it.value().x()}, {"y", it.value().y()},
                                  {"width", it.value().width()}, {"height", it.value().height()}}}
                });
            }
            result["changes"] = changes;

            Json overlapArr = Json::array();
            for (const auto &ov : plan.overlaps)
            {
                overlapArr.push_back({
                    {"widgetA", ov.widgetA}, {"widgetB", ov.widgetB},
                    {"intersection", {
                        {"x", ov.intersection.x()}, {"y", ov.intersection.y()},
                        {"width", ov.intersection.width()}, {"height", ov.intersection.height()}
                    }}
                });
            }
            result["remainingOverlaps"] = overlapArr;
            return result.dump();
            });
        },
        std::nullopt,
        std::string("Reflow widgets within a frame or page using flow layout. "
                     "Buttons and sliders are arranged in a grid, nested frames are recursively reflowed, "
                     "and the container is resized to fit. Supports dryRun mode to preview changes."),
        std::nullopt
    ));
    } // end vc_reflow_frame schema scope
}
