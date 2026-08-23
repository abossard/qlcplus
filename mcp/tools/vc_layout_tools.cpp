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
#include "gridlayout.h"

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
            auto itemsErr = validateItemsArray(args);
            if (itemsErr) return *itemsErr;
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
        std::string("Move Virtual Console widgets between frames. Preserves all properties. Batch. "
                     "Wrap multiple operations in {\"items\": [...]}. Each item is processed independently."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotIdempotent));

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
    )
    .set_annotations(mcp::kAnnotDestructive));

    // vc_delete_pages — remove whole VC pages and everything on them
    tm.register_tool(Tool(
        "vc_delete_pages",
        Json{{"type", "object"}, {"properties", {
            {"pageIndexes", {{"type", "array"}, {"items", {{"type", "integer"}}},
                             {"description", "Zero-based page indexes to delete. Deleting a page "
                                             "also deletes every widget on it."}}}
        }}, {"required", {"pageIndexes"}}},
        Json{},
        [doc, vcBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto err = validateFields(args, {"pageIndexes"});
            if (!err.empty()) return err;
            if (!args.contains("pageIndexes") || !args.at("pageIndexes").is_array())
                return Json({{"error", "pageIndexes must be an array of integers"}}).dump();

            // Highest index first, so the remaining indexes stay valid as pages
            // shift down after each removal.
            std::vector<int> indexes;
            for (auto &v : args.at("pageIndexes"))
            {
                if (!v.is_number_integer())
                    return Json({{"error", "pageIndexes must be an array of integers"}}).dump();
                indexes.push_back(v.get<int>());
            }
            std::sort(indexes.begin(), indexes.end(), [](int a, int b) { return a > b; });
            indexes.erase(std::unique(indexes.begin(), indexes.end()), indexes.end());

            Json results = Json::array();
            for (int idx : indexes)
            {
                if (idx < 0 || idx >= vcBridge->pagesCount())
                {
                    results.push_back({{"pageIndex", idx}, {"status", "not found"}});
                    continue;
                }
                if (vcBridge->pagesCount() == 1)
                {
                    results.push_back({{"pageIndex", idx},
                                       {"error", "cannot delete the last remaining page"}});
                    continue;
                }
                if (vcBridge->deletePage(idx))
                    results.push_back({{"pageIndex", idx}, {"status", "deleted"}});
                else
                    results.push_back({{"pageIndex", idx}, {"error", "could not delete page"}});
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("Delete Virtual Console pages by zero-based index, together with every widget "
                     "on them. Batch: {\"pageIndexes\": [...]}. Indexes are applied highest-first so "
                     "a batch stays consistent. The last remaining page cannot be deleted."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotDestructive));

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
    )
    .set_annotations(mcp::kAnnotReadOnly));

    // vc_reflow_frame — reflow children within a frame (or entire page) using flow layout
    {
    Json reflowSchema = Json{{"type", "object"}, {"properties", {
        {"frameID", {{"type", "integer"}, {"description",
            "Widget ID of the frame to reflow. All children will be repositioned."}}},
        {"pageIndex", {{"type", "integer"}, {"description",
            "Page index (0-based). Auto-detects column layout from x-positions and reflows within each column. "
            "Defaults to dryRun=true. Used only if frameID is not provided."}}},
        {"algorithm", {{"type", "string"}, {"enum", {"flow", "gridCompact"}}, {"description",
            "Layout algorithm. flow (default) = existing flow layout. "
            "gridCompact = grid-based Grafana-style vertical compaction using the frame's grid settings."}}},
        {"columns", {{"type", "integer"}, {"description",
            "Number of columns for flow grid. 0 or omit for auto-compute from width."}}},
        {"pad", {{"type", "integer"}, {"description", "Padding between widgets in pixels (default 5)"}}},
        {"framePad", {{"type", "integer"}, {"description", "Vertical gap between top-level frames (default 10)"}}},
        {"buttonWidth", {{"type", "integer"}, {"description", "Button width in pixels (default 100)"}}},
        {"buttonHeight", {{"type", "integer"}, {"description", "Button height in pixels (default 60)"}}},
        {"sliderWidth", {{"type", "integer"}, {"description", "Slider width in pixels (default 60)"}}},
        {"sliderHeight", {{"type", "integer"}, {"description", "Slider height in pixels (default 200)"}}},
        {"dryRun", {{"type", "boolean"}, {"description",
            "Preview mode: compute plan without applying. Defaults to true for pageIndex (safe preview), false for frameID."}}}
    }}};
    tm.register_tool(Tool(
        "vc_reflow_frame",
        reflowSchema,
        Json{},
        [doc, vcBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto err = validateFields(args, {"frameID", "pageIndex", "algorithm", "columns", "pad", "framePad",
                "buttonWidth", "buttonHeight", "sliderWidth", "sliderHeight", "dryRun"});
            if (!err.empty()) return err;

            static const Json kEnums = {
                {"algorithm", {{"enum", {"flow", "gridCompact"}}}}
            };
            err = validateEnums(args, kEnums);
            if (!err.empty()) return err;

            std::string algorithm = args.value("algorithm", std::string("flow"));

            VCBridge::ReflowOptions opts;
            opts.columns = args.value("columns", 0);
            opts.pad = args.value("pad", 5);
            opts.framePad = args.value("framePad", 10);
            opts.defaultButtonWidth = args.value("buttonWidth", 100);
            opts.defaultButtonHeight = args.value("buttonHeight", 60);
            opts.defaultSliderWidth = args.value("sliderWidth", 60);
            opts.defaultSliderHeight = args.value("sliderHeight", 200);
            opts.gridSize = vcBridge->snappingSize();

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

            // Page-level reflow defaults to dryRun (destructive at page scale)
            bool dryRun = args.value("dryRun", isPage);

            if (snap.id < 0 && snap.children.isEmpty())
                return std::string("{\"error\": \"Frame/page not found\"}");

            VCBridge::LayoutPlan plan;

            if (algorithm == "gridCompact")
            {
                if (isPage)
                    return std::string("{\"error\": \"gridCompact requires frameID (not pageIndex)\"}");
                if (snap.children.isEmpty())
                {
                    Json result;
                    result["applied"] = false;
                    result["algorithm"] = "gridCompact";
                    result["widgetsMoved"] = 0;
                    result["changes"] = Json::array();
                    result["remainingOverlaps"] = Json::array();
                    return result.dump();
                }

                // Pull grid configuration from the frame (fallback to defaults)
                auto gridInfo = vcBridge->getFrameGridLayout(snap.id);
                int columns = gridInfo.found && gridInfo.columns > 0 ? gridInfo.columns : 12;
                int rowHeight = gridInfo.found ? gridInfo.rowHeight : 0;
                if (rowHeight <= 0)
                    rowHeight = opts.gridSize > 0 ? opts.gridSize : 20;

                int frameWidth = snap.geometry.width();
                int cellW = GridLayout::cellWidth(frameWidth, columns);
                if (cellW <= 0)
                    cellW = opts.gridSize > 0 ? opts.gridSize : 20;

                // Convert child geometries to cells (positions are relative to the frame)
                int hdrH = snap.showHeader ? opts.headerHeight : 0;
                QVector<GridLayout::GridItem> items;
                items.reserve(snap.children.size());
                for (const auto &child : snap.children)
                {
                    QRect rel = child.geometry;
                    rel.translate(0, -hdrH);
                    if (rel.y() < 0) rel.moveTop(0);
                    GridLayout::GridItem gi;
                    gi.id = child.id;
                    gi.cell = GridLayout::pixelsToCells(rel, cellW, rowHeight);
                    items.append(gi);
                }

                QVector<GridLayout::GridItem> compacted = GridLayout::compactVertical(items);

                // Map back to pixel geometries, preserving original widths/heights in px
                QHash<int, QRect> originalGeo;
                for (const auto &child : snap.children)
                    originalGeo.insert(child.id, child.geometry);

                QList<VCBridge::WidgetSnapshot> newChildren;
                for (const auto &it : compacted)
                {
                    QRect cellRect = GridLayout::cellsToPixels(it.cell, cellW, rowHeight);
                    QRect orig = originalGeo.value(it.id);
                    QRect out(cellRect.x(), cellRect.y() + hdrH,
                              orig.width(), orig.height());
                    plan.geometries.insert(it.id, out);

                    VCBridge::WidgetSnapshot copy;
                    copy.id = it.id;
                    copy.geometry = out;
                    newChildren.append(copy);
                }
                plan.overlaps = VCBridge::detectOverlaps(newChildren);
            }
            else if (isPage)
            {
                plan = VCBridge::reflowPage(snap, opts);
            }
            else
            {
                int requiredHeight = VCBridge::reflowChildren(snap, opts);
                snap.geometry.setHeight(requiredHeight);
                VCBridge::collectGeometries(snap, plan);
                plan.geometries.insert(snap.id, snap.geometry);
                plan.overlaps = VCBridge::detectOverlaps(snap.children);
            }

            if (!dryRun)
                vcBridge->applyLayoutPlan(plan, algorithm == "gridCompact");

            // Build response
            Json result;
            result["applied"] = !dryRun;
            result["algorithm"] = algorithm;
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
        std::string("Reflow widgets within a frame or page. "
                     "algorithm=flow (default): arranges buttons/sliders in a grid, recursively reflows nested frames, resizes the frame to fit. "
                     "algorithm=gridCompact (frameID only): Grafana-style vertical compaction using the frame's gridColumns/gridRowHeight — "
                     "snaps widgets to cells and drops them as far up as possible without overlapping. "
                     "With pageIndex: auto-detects column groupings from x-positions, preserves multi-column layouts, "
                     "reflows within each column independently. Page-level reflow defaults to dryRun=true (pass dryRun=false to apply). "
                     "Never reparents or creates widgets — only repositions existing children within their current parent."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotIdempotent));
    } // end vc_reflow_frame schema scope

    // vc_set_grid_layout — set grid layout mode on frames (batch)
    tm.register_tool(Tool(
        "vc_set_grid_layout",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"frameID", {{"type", "integer"}}},
                {"layoutMode", {{"type", "string"}, {"enum", {"free", "grid"}}}},
                {"columns", {{"type", "integer"}, {"description", "Grid columns (default 12)"}}},
                {"rowHeight", {{"type", "integer"}, {"description", "Row height in pixels (0 = auto)"}}},
                {"compact", {{"type", "boolean"}, {"description", "Enable vertical compaction (default true)"}}}
            }}, {"required", {"frameID"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc, vcBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto itemsErr = validateItemsArray(args);
            if (itemsErr) return *itemsErr;
            auto topErr = validateFields(args, {"items"});
            if (!topErr.empty()) return topErr;

            static const Json kEnums = {
                {"layoutMode", {{"enum", {"free", "grid"}}}}
            };
            Json results = Json::array();
            for (auto &item : args.at("items"))
            {
                auto err = validateFields(item, {"frameID", "layoutMode", "columns", "rowHeight", "compact"});
                if (!err.empty()) { results.push_back(nlohmann::json::parse(err)); continue; }
                auto enumErr = validateEnums(item, kEnums);
                if (!enumErr.empty()) { results.push_back(nlohmann::json::parse(enumErr)); continue; }

                int frameID = item.at("frameID").get<int>();

                // Read current to fill missing fields
                auto current = vcBridge->getFrameGridLayout(frameID);
                QString mode = current.found ? current.layoutMode : QString("free");
                int columns = current.found ? current.columns : 12;
                int rowHeight = current.found ? current.rowHeight : 0;
                bool compact = current.found ? current.compact : true;

                if (item.contains("layoutMode"))
                    mode = QString::fromStdString(item.at("layoutMode").get<std::string>());
                if (item.contains("columns"))
                    columns = item.at("columns").get<int>();
                if (item.contains("rowHeight"))
                    rowHeight = item.at("rowHeight").get<int>();
                if (item.contains("compact"))
                    compact = item.at("compact").get<bool>();

                bool ok = vcBridge->setFrameGridLayout(frameID, mode, columns, rowHeight, compact);
                Json r;
                r["frameID"] = frameID;
                r["status"] = ok ? "ok" : "failed";
                if (ok)
                {
                    r["layoutMode"] = mode.toStdString();
                    r["columns"] = columns;
                    r["rowHeight"] = rowHeight;
                    r["compact"] = compact;
                }
                else
                {
                    r["error"] = "frame not found";
                }
                results.push_back(r);
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("Set grid layout mode on frames. Enables Grafana-style vertical compaction and collision push-down. "
                    "Use with vc_reflow_frame algorithm=gridCompact to apply the compaction. Batch. "
                    "Wrap multiple operations in {\"items\": [...]}. Each item is processed independently."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotIdempotent));
}
