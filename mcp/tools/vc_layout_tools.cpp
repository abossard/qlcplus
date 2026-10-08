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
#include "vc_tools_common.h"
#include "doc.h"
#include "gridlayout.h"

#include <algorithm>
#include <limits>
#include <map>
#include <optional>

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
            constexpr int64_t kMaxInt = std::numeric_limits<int>::max();
            constexpr int64_t kMinInt = std::numeric_limits<int>::min();
            Json results = Json::array();
            const Json &items = args.at("items");
            for (size_t i = 0; i < items.size(); ++i)
            {
                const Json &item = items[i];
                if (!item.is_object()) { results.push_back(mcp::itemError(i, "item must be an object")); continue; }
                auto err = validateFields(item, {"widgetID", "newParentID", "x", "y", "width", "height"});
                if (!err.empty()) { results.push_back(mcp::itemErrorFromDump(i, err)); continue; }

                std::string geoErr;
                std::optional<int64_t> geo[4];
                const char *geoFields[] = {"x", "y", "width", "height"};
                for (int g = 0; g < 4 && geoErr.empty(); ++g)
                {
                    if (!item.contains(geoFields[g])) continue;
                    geo[g] = mcp::jsonInteger(item[geoFields[g]], kMinInt, kMaxInt);
                    if (!geo[g]) geoErr = mcp::integerError(geoFields[g], kMinInt, kMaxInt);
                }
                const auto widgetID = item.contains("widgetID")
                    ? mcp::jsonInteger(item["widgetID"], 0, kMaxInt) : std::nullopt;
                const auto newParentID = item.contains("newParentID")
                    ? mcp::jsonInteger(item["newParentID"], 0, kMaxInt) : std::nullopt;
                if (!item.contains("widgetID")) geoErr = "widgetID is required";
                else if (!item.contains("newParentID")) geoErr = "newParentID is required";
                else if (!widgetID) geoErr = mcp::integerError("widgetID", 0, kMaxInt);
                else if (!newParentID) geoErr = mcp::integerError("newParentID", 0, kMaxInt);
                if (!geoErr.empty()) { results.push_back(mcp::itemError(i, geoErr)); continue; }

                const int wid = int(*widgetID);
                const int newParent = int(*newParentID);
                const auto d = vcBridge->getWidgetDetails(wid);
                const auto target = vcBridge->getWidgetDetails(newParent);
                std::string reject;
                if (d.id < 0)
                    reject = "widget " + std::to_string(wid) + " not found";
                else if (target.id < 0)
                    reject = "newParentID " + std::to_string(newParent) + " not found";
                else if (target.machineType != QStringLiteral("frame") &&
                         target.machineType != QStringLiteral("soloframe"))
                    reject = "newParentID must be a frame or soloframe";
                else if (d.parentID < 0)
                    reject = "widget has no parent frame and cannot be moved";
                else if (d.parentID == newParent)
                    reject = "widget is already in frame " + std::to_string(newParent);
                else
                {
                    for (int p = newParent; p >= 0; p = vcBridge->getWidgetDetails(p).parentID)
                        if (p == wid) { reject = "cannot move a frame into itself or its descendant"; break; }
                }
                if (!reject.empty()) { results.push_back(mcp::itemError(i, reject)); continue; }

                const QRect rect(int(geo[0].value_or(5)), int(geo[1].value_or(5)),
                                 int(geo[2].value_or(d.geometry.width())),
                                 int(geo[3].value_or(d.geometry.height())));
                if (vcBridge->reparentWidget(wid, newParent, rect))
                    results.push_back(mcp::itemOk(i, {{"outcome", "moved"}, {"widgetID", wid},
                                                      {"newParentID", newParent}}));
                else
                    results.push_back(mcp::itemError(i, "could not move widget"));
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
            if (!args.contains("ids") || !args.at("ids").is_array())
                return Json({{"error", "ids must be an array of integers"}}).dump();
            constexpr int64_t kMaxInt = std::numeric_limits<int>::max();
            const Json &ids = args.at("ids");

            // Ancestry is captured before any deletion so a child listed after
            // its deleted ancestor reports that removal truthfully.
            std::vector<std::optional<int>> parsed(ids.size());
            std::vector<std::vector<int>> lineage(ids.size());
            for (size_t i = 0; i < ids.size(); ++i)
            {
                if (auto v = mcp::jsonInteger(ids[i], 0, kMaxInt)) parsed[i] = int(*v);
                else continue;
                for (int p = *parsed[i]; p >= 0; p = vcBridge->getWidgetDetails(p).parentID)
                {
                    if (vcBridge->getWidgetDetails(p).id < 0) break;
                    lineage[i].push_back(p);
                }
            }

            Json results = Json::array();
            std::map<int, size_t> firstAt;
            std::map<int, size_t> deletedAt;
            for (size_t i = 0; i < ids.size(); ++i)
            {
                if (!parsed[i]) { results.push_back(mcp::itemError(i, mcp::integerError("ids[]", 0, kMaxInt))); continue; }
                const int id = *parsed[i];
                auto first = firstAt.find(id);
                if (first != firstAt.end())
                {
                    Json dup = results[first->second];
                    dup["index"] = i;
                    dup["duplicateOf"] = first->second;
                    if (dup.value("status", "") == "ok")
                    {
                        dup["outcome"] = "duplicate";
                        dup.erase("deletedWith");
                    }
                    results.push_back(dup);
                    continue;
                }
                firstAt[id] = i;
                Json rec;
                if (lineage[i].empty())
                    rec = mcp::itemError(i, "widget not found");
                else
                {
                    auto ancestor = std::find_if(lineage[i].begin() + 1, lineage[i].end(),
                                                 [&](int p) { return deletedAt.count(p) > 0; });
                    if (ancestor != lineage[i].end())
                        rec = mcp::itemOk(i, {{"outcome", "alreadyDeleted"}, {"deletedWith", deletedAt[*ancestor]}});
                    else if (vcBridge->removeWidget(id))
                    {
                        deletedAt[id] = i;
                        rec = mcp::itemOk(i, {{"outcome", "deleted"}});
                    }
                    else
                        rec = mcp::itemError(i, "could not delete widget");
                }
                rec["id"] = id;
                results.push_back(rec);
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("Delete Virtual Console widgets by ID. Batch: one indexed outcome per id "
                     "(deleted, duplicate, or alreadyDeleted when an ancestor was deleted earlier)."),
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
            constexpr int64_t kMaxInt = std::numeric_limits<int>::max();
            const Json &pageIndexes = args.at("pageIndexes");
            Json results(pageIndexes.size(), Json());
            std::map<int, size_t> firstAt;
            std::vector<std::pair<int, size_t>> unique;
            for (size_t i = 0; i < pageIndexes.size(); ++i)
            {
                const auto v = mcp::jsonInteger(pageIndexes[i], 0, kMaxInt);
                if (!v)
                    results[i] = mcp::itemError(i, mcp::integerError("pageIndexes[]", 0, kMaxInt));
                else if (!firstAt.count(int(*v)))
                {
                    firstAt[int(*v)] = i;
                    unique.push_back({int(*v), i});
                }
            }

            // Highest index first, so the remaining indexes stay valid as pages
            // shift down after each removal; outcomes keep the caller's order.
            std::sort(unique.begin(), unique.end(), [](auto &l, auto &r) { return l.first > r.first; });
            for (const auto &[idx, i] : unique)
            {
                Json rec;
                if (idx >= vcBridge->pagesCount())
                    rec = mcp::itemError(i, "page not found");
                else if (vcBridge->pagesCount() == 1)
                    rec = mcp::itemError(i, "cannot delete the last remaining page");
                else if (vcBridge->deletePage(idx))
                    rec = mcp::itemOk(i, {{"outcome", "deleted"}});
                else
                    rec = mcp::itemError(i, "could not delete page");
                rec["pageIndex"] = idx;
                results[i] = rec;
            }
            for (size_t i = 0; i < pageIndexes.size(); ++i)
            {
                if (!results[i].is_null()) continue;
                const size_t first = firstAt.at(int(*mcp::jsonInteger(pageIndexes[i], 0, kMaxInt)));
                Json dup = results[first];
                dup["index"] = i;
                dup["duplicateOf"] = first;
                if (dup.value("status", "") == "ok")
                    dup["outcome"] = "duplicate";
                results[i] = dup;
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("Delete Virtual Console pages by zero-based index, together with every widget "
                     "on them. Batch: {\"pageIndexes\": [...]}. Indexes are applied highest-first so "
                     "a batch stays consistent; outcomes keep the caller's order. The last remaining page cannot be deleted."),
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

            std::string algorithm = VCValidate::canonicalEnum(
                args.value("algorithm", std::string("flow")), kEnums["algorithm"]["enum"]);

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
            const Json &items = args.at("items");
            for (size_t i = 0; i < items.size(); ++i)
            {
                const Json &item = items[i];
                auto err = validateFields(item, {"frameID", "layoutMode", "columns", "rowHeight", "compact"});
                if (err.empty()) err = validateEnums(item, kEnums);
                if (!err.empty()) { results.push_back(mcp::itemErrorFromDump(i, err)); continue; }

                constexpr int64_t kIntMax = std::numeric_limits<int>::max();
                auto frameIDValue = item.contains("frameID") ? mcp::jsonInteger(item.at("frameID"), 0, kIntMax) : std::nullopt;
                if (!frameIDValue) { results.push_back(mcp::itemError(i, mcp::integerError("frameID", 0, kIntMax))); continue; }
                const int frameID = int(*frameIDValue);
                auto itemError = [&](const std::string &msg) {
                    Json rec = mcp::itemError(i, msg);
                    rec["frameID"] = frameID;
                    results.push_back(rec);
                };

                // Read current to fill missing fields
                auto current = vcBridge->getFrameGridLayout(frameID);
                if (!current.found) { itemError("frame not found"); continue; }
                QString mode = current.layoutMode;
                int columns = current.columns;
                int rowHeight = current.rowHeight;
                bool compact = current.compact;

                std::optional<int64_t> v;
                if (item.contains("columns") && !(v = mcp::jsonInteger(item.at("columns"), 1, kIntMax)))
                { itemError(mcp::integerError("columns", 1, kIntMax)); continue; }
                if (v) columns = int(*v);
                v.reset();
                if (item.contains("rowHeight") && !(v = mcp::jsonInteger(item.at("rowHeight"), 0, kIntMax)))
                { itemError(mcp::integerError("rowHeight", 0, kIntMax)); continue; }
                if (v) rowHeight = int(*v);
                if (item.contains("compact"))
                {
                    if (!item.at("compact").is_boolean()) { itemError("compact must be a boolean"); continue; }
                    compact = item.at("compact").get<bool>();
                }
                if (item.contains("layoutMode"))
                    mode = QString::fromStdString(VCValidate::canonicalEnum(
                        item.at("layoutMode").get<std::string>(), kEnums["layoutMode"]["enum"]));

                if (!vcBridge->setFrameGridLayout(frameID, mode, columns, rowHeight, compact))
                { itemError("grid layout not applied"); continue; }
                results.push_back(mcp::itemOk(i, {{"frameID", frameID}, {"outcome", "updated"},
                    {"layoutMode", mode.toStdString()}, {"columns", columns},
                    {"rowHeight", rowHeight}, {"compact", compact}}));
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
