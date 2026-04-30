/*
  Q Light Controller Plus
  flow_tools.cpp

  Copyright (C) Massimo Callegari

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include "tool_registry.h"
#include "flowconsole.h"
#include "vcwidget.h"
#include "doc.h"

#include <fastmcpp/tools/manager.hpp>
#include <fastmcpp/tools/tool.hpp>

void registerFlowTools(fastmcpp::tools::ToolManager &tm, Doc *doc, FlowConsole *fc)
{
    if (!fc) return;

    using Json = nlohmann::json;
    using Tool = fastmcpp::tools::Tool;

    // ─── flow_query_layout ───────────────────────────────────────────
    tm.register_tool(Tool(
        "flow_query_layout",
        Json{{"type", "object"}, {"properties", {
            {"pageIndex", {{"type", "integer"}, {"description",
                "Page index to query (default: all pages)"}}}
        }}},
        Json{},
        [doc, fc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
                auto err = validateFields(args, {"pageIndex"});
                if (!err.empty()) return err;

                Json result = Json::array();
                QVariantList pages = fc->pages();
                int filterPage = args.contains("pageIndex") ? args.at("pageIndex").get<int>() : -1;

                for (const QVariant &pv : pages)
                {
                    QVariantMap pm = pv.toMap();
                    int pageIdx = pm["index"].toInt();
                    if (filterPage >= 0 && pageIdx != filterPage)
                        continue;

                    Json page;
                    page["index"] = pageIdx;
                    page["name"] = pm["name"].toString().toStdString();

                    Json sections = Json::array();
                    QVariantList sects = fc->sectionsForPage(pageIdx);
                    for (const QVariant &sv : sects)
                    {
                        QVariantMap sm = sv.toMap();
                        Json sec;
                        sec["id"] = sm["id"].toInt();
                        sec["caption"] = sm["caption"].toString().toStdString();
                        sec["sizePreset"] = sm["sizePreset"].toString().toStdString();
                        sec["columns"] = sm["columns"].toInt();
                        sec["isSolo"] = sm["isSolo"].toBool();

                        Json widgets = Json::array();
                        QVariantList wl = fc->widgetsForSection(sm["id"].toInt());
                        for (const QVariant &wv : wl)
                        {
                            QVariantMap wm = wv.toMap();
                            Json w;
                            w["id"] = wm["id"].toUInt();
                            w["type"] = VCWidget::typeToString(wm["type"].toInt()).toStdString();
                            w["colSpan"] = wm["colSpan"].toInt();
                            QObject *wObj = fc->widget(wm["id"].toUInt());
                            if (wObj)
                            {
                                VCWidget *vcw = qobject_cast<VCWidget*>(wObj);
                                if (vcw)
                                    w["caption"] = vcw->caption().toStdString();
                            }
                            widgets.push_back(w);
                        }
                        sec["widgets"] = widgets;
                        sections.push_back(sec);
                    }
                    page["sections"] = sections;
                    result.push_back(page);
                }
                return result;
            });
        })
    .set_description("Query the Flow Console layout — pages, sections, widgets with logical positions.")
    .set_annotations(mcp::kAnnotReadOnly));

    // ─── flow_create_section ─────────────────────────────────────────
    tm.register_tool(Tool(
        "flow_create_section",
        Json{{"type", "object"}, {"properties", {
            {"pageIndex", {{"type", "integer"}, {"description", "Page index (default 0)"}}},
            {"caption", {{"type", "string"}, {"description", "Section caption"}}},
            {"sizePreset", {{"type", "string"}, {"description", "Size: full, half, third, quarter (default: full)"}}},
            {"columns", {{"type", "integer"}, {"description", "Grid columns (default 4, max 12)"}}},
            {"solo", {{"type", "boolean"}, {"description", "Solo mode (default false)"}}}
        }}, {"required", {"caption"}}},
        Json{},
        [doc, fc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
                auto err = validateFields(args, {"pageIndex", "caption", "sizePreset", "columns", "solo"});
                if (!err.empty()) return err;

                int pageIndex = args.value("pageIndex", 0);
                QString caption = QString::fromStdString(args.at("caption").get<std::string>());
                QString sizePreset = QString::fromStdString(args.value("sizePreset", "full"));
                int columns = args.value("columns", 4);
                bool solo = args.value("solo", false);

                int id = fc->addSection(pageIndex, caption, sizePreset, columns, solo);
                if (id < 0)
                    return Json({{"error", "failed to create section"}}).dump();

                return Json{{"id", id}, {"status", "created"}};
            });
        })
    .set_description("Create a section in the Flow Console.")
    .set_annotations(mcp::kAnnotIdempotent));

    // ─── flow_create_widget ──────────────────────────────────────────
    tm.register_tool(Tool(
        "flow_create_widget",
        Json{{"type", "object"}, {"properties", {
            {"sectionId", {{"type", "integer"}, {"description", "Section ID"}}},
            {"type", {{"type", "string"}, {"description",
                "Widget type: button, slider, label, clock, cuelist, xypad, speedDial, matrix, audioTrigger"}}},
            {"colSpan", {{"type", "integer"}, {"description", "Grid column span (default 1)"}}},
            {"caption", {{"type", "string"}, {"description", "Widget caption (optional)"}}}
        }}, {"required", {"sectionId", "type"}}},
        Json{},
        [doc, fc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
                auto err = validateFields(args, {"sectionId", "type", "colSpan", "caption"});
                if (!err.empty()) return err;

                int sectionId = args.at("sectionId").get<int>();
                QString type = QString::fromStdString(args.at("type").get<std::string>());
                int colSpan = args.value("colSpan", 1);

                int wid = fc->addWidget(sectionId, type, colSpan);
                if (wid < 0)
                    return Json({{"error", "failed to create widget"}}).dump();

                if (args.contains("caption"))
                    fc->setWidgetCaption(wid, QString::fromStdString(args.at("caption").get<std::string>()));

                return Json{{"id", wid}, {"status", "created"}};
            });
        })
    .set_description("Create a widget in a Flow Console section.")
    .set_annotations(mcp::kAnnotIdempotent));

    // ─── flow_reorder_widget ─────────────────────────────────────────
    tm.register_tool(Tool(
        "flow_reorder_widget",
        Json{{"type", "object"}, {"properties", {
            {"widgetId", {{"type", "integer"}, {"description", "Widget ID"}}},
            {"newIndex", {{"type", "integer"}, {"description", "New position index"}}}
        }}, {"required", {"widgetId", "newIndex"}}},
        Json{},
        [doc, fc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
                auto err = validateFields(args, {"widgetId", "newIndex"});
                if (!err.empty()) return err;
                return Json{{"success", fc->reorderWidget(args.at("widgetId").get<int>(),
                                                          args.at("newIndex").get<int>())}};
            });
        })
    .set_description("Reorder a widget within its section.")
    .set_annotations(mcp::kAnnotIdempotent));

    // ─── flow_delete_section ─────────────────────────────────────────
    tm.register_tool(Tool(
        "flow_delete_section",
        Json{{"type", "object"}, {"properties", {
            {"sectionId", {{"type", "integer"}, {"description", "Section ID to delete"}}}
        }}, {"required", {"sectionId"}}},
        Json{},
        [doc, fc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
                auto err = validateFields(args, {"sectionId"});
                if (!err.empty()) return err;
                return Json{{"success", fc->removeSection(args.at("sectionId").get<int>())}};
            });
        })
    .set_description("Delete a section and all its widgets.")
    .set_annotations(mcp::kAnnotDestructive));

    // ─── flow_delete_widget ──────────────────────────────────────────
    tm.register_tool(Tool(
        "flow_delete_widget",
        Json{{"type", "object"}, {"properties", {
            {"widgetId", {{"type", "integer"}, {"description", "Widget ID to delete"}}}
        }}, {"required", {"widgetId"}}},
        Json{},
        [doc, fc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
                auto err = validateFields(args, {"widgetId"});
                if (!err.empty()) return err;
                return Json{{"success", fc->removeWidget(args.at("widgetId").get<int>())}};
            });
        })
    .set_description("Delete a widget from the Flow Console.")
    .set_annotations(mcp::kAnnotDestructive));
}
