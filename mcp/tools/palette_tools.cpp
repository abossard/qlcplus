/*
  Q Light Controller Plus
  palette_tools.cpp

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
#include "doc.h"
#include "qlcpalette.h"
#include "scene.h"

#include <QRegularExpression>
#include <QColor>
#include <fastmcpp/tools/manager.hpp>
#include <fastmcpp/tools/tool.hpp>

namespace {

QLCPalette* findPaletteByNameAndType(Doc *doc, const QString &name, QLCPalette::PaletteType type)
{
    for (QLCPalette *p : doc->palettes())
    {
        if (p->name() == name && p->type() == type)
            return p;
    }
    return nullptr;
}

QLCPalette::PaletteType stringToPaletteType(const std::string &s)
{
    if (s == "Dimmer")  return QLCPalette::Dimmer;
    if (s == "Color")   return QLCPalette::Color;
    if (s == "Pan")     return QLCPalette::Pan;
    if (s == "Tilt")    return QLCPalette::Tilt;
    if (s == "PanTilt") return QLCPalette::PanTilt;
    return QLCPalette::Undefined;
}

void applyFanning(QLCPalette *palette, const nlohmann::json &fan)
{
    if (fan.contains("type"))
    {
        std::string t = fan.at("type").get<std::string>();
        if (t == "Flat")    palette->setFanningType(QLCPalette::Flat);
        else if (t == "Linear") palette->setFanningType(QLCPalette::Linear);
        else if (t == "Sine")   palette->setFanningType(QLCPalette::Sine);
        else if (t == "Square") palette->setFanningType(QLCPalette::Square);
        else if (t == "Saw")    palette->setFanningType(QLCPalette::Saw);
    }
    if (fan.contains("layout"))
    {
        std::string l = fan.at("layout").get<std::string>();
        if (l == "XAscending")  palette->setFanningLayout(QLCPalette::XAscending);
        else if (l == "XDescending") palette->setFanningLayout(QLCPalette::XDescending);
        else if (l == "XCentered")   palette->setFanningLayout(QLCPalette::XCentered);
        else if (l == "YAscending")  palette->setFanningLayout(QLCPalette::YAscending);
        else if (l == "YDescending") palette->setFanningLayout(QLCPalette::YDescending);
        else if (l == "YCentered")   palette->setFanningLayout(QLCPalette::YCentered);
        else if (l == "ZAscending")  palette->setFanningLayout(QLCPalette::ZAscending);
        else if (l == "ZDescending") palette->setFanningLayout(QLCPalette::ZDescending);
        else if (l == "ZCentered")   palette->setFanningLayout(QLCPalette::ZCentered);
    }
    if (fan.contains("amount"))
        palette->setFanningAmount(fan.at("amount").get<int>());
    if (fan.contains("value"))
    {
        auto &v = fan.at("value");
        if (v.is_string())
        {
            // Color fanning value: "#rrggbb"
            palette->setFanningValue(QVariant(QColor(QString::fromStdString(v.get<std::string>()))));
        }
        else if (v.is_number_float())
        {
            palette->setFanningValue(QVariant(v.get<double>()));
        }
        else if (v.is_number_integer())
        {
            palette->setFanningValue(QVariant(v.get<int>()));
        }
    }
}

nlohmann::json fanningToJson(const QLCPalette *palette)
{
    using Json = nlohmann::json;
    Json fan;
    fan["type"] = QLCPalette::fanningTypeToString(palette->fanningType()).toStdString();
    fan["layout"] = QLCPalette::fanningLayoutToString(palette->fanningLayout()).toStdString();
    fan["amount"] = palette->fanningAmount();
    QVariant fv = palette->fanningValue();
    if (fv.canConvert<QColor>())
        fan["value"] = fv.value<QColor>().name().toStdString();
    else if (fv.canConvert<double>())
        fan["value"] = fv.toDouble();
    else if (fv.canConvert<int>())
        fan["value"] = fv.toInt();
    else if (fv.isValid())
        fan["value"] = fv.toString().toStdString();
    return fan;
}

} // anonymous namespace

void registerPaletteTools(fastmcpp::tools::ToolManager &tm, Doc *doc)
{
    using Json = nlohmann::json;
    using Tool = fastmcpp::tools::Tool;

    // create_palettes (batch)
    static const std::string typeDesc =
        "Palette type: Dimmer, Color, Pan, Tilt, PanTilt";
    static const std::string fanningDesc =
        "Optional fanning. type: Flat|Linear|Sine|Square|Saw. "
        "layout: XAscending|XDescending|XCentered|YAscending|YDescending|YCentered|ZAscending|ZDescending|ZCentered. "
        "amount: 0-1000 (percentage). value: fanning end value (int for Dimmer, '#rrggbb' for Color, degrees for Position).";

    tm.register_tool(Tool(
        "create_palettes",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"name", {{"type", "string"}, {"description", "Palette name (required)"}}},
                {"type", {{"type", "string"}, {"description", typeDesc}}},
                {"value", {{"type", "integer"}, {"description", "Dimmer intensity 0-255"}}},
                {"panDegrees", {{"type", "number"}, {"description", "Pan degrees (Pan or PanTilt type)"}}},
                {"tiltDegrees", {{"type", "number"}, {"description", "Tilt degrees (Tilt or PanTilt type)"}}},
                {"rgb", {{"type", "string"}, {"description", "Color: RGB hex '#rrggbb'"}}},
                {"wauv", {{"type", "string"}, {"description", "Color: White/Amber/UV hex '#wwaauu' (optional, default '#000000')"}}},
                {"fanning", {{"type", "object"}, {"description", fanningDesc}, {"properties", {
                    {"type", {{"type", "string"}}},
                    {"layout", {{"type", "string"}}},
                    {"amount", {{"type", "integer"}}},
                    {"value", {}}
                }}}}
            }}, {"required", {"name", "type"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto err = validateFields(args, {"items"});
            if (!err.empty()) return err;
            if (!args.contains("items") || !args.at("items").is_array())
                return Json({{"error", "items array required"}}).dump();

            Json results = Json::array();
            for (auto &item : args.at("items"))
            {
                auto itemErr = validateFields(item, {"name", "type", "value", "panDegrees", "tiltDegrees", "rgb", "wauv", "fanning"});
                if (!itemErr.empty()) { results.push_back(Json::parse(itemErr)); continue; }

                if (!item.contains("name") || !item.contains("type"))
                {
                    results.push_back({{"error", "name and type are required"}});
                    continue;
                }

                QString name = QString::fromStdString(item.at("name").get<std::string>());
                QLCPalette::PaletteType ptype = stringToPaletteType(item.at("type").get<std::string>());
                if (ptype == QLCPalette::Undefined)
                {
                    results.push_back({{"error", "invalid type. Must be: Dimmer, Color, Pan, Tilt, PanTilt"},
                                       {"name", name.toStdString()}});
                    continue;
                }

                // Upsert: find existing by name+type
                QLCPalette *palette = findPaletteByNameAndType(doc, name, ptype);
                bool isNew = (palette == nullptr);
                if (isNew)
                {
                    palette = new QLCPalette(ptype);
                    palette->setName(name);
                }

                palette->resetValues();

                // Set values based on type
                switch (ptype)
                {
                    case QLCPalette::Dimmer:
                    {
                        int val = item.value("value", 255);
                        palette->setValue(QVariant(val));
                    }
                    break;
                    case QLCPalette::Color:
                    {
                        QColor rgb(QString::fromStdString(item.value("rgb", "#ffffff")));
                        QColor wauv(QString::fromStdString(item.value("wauv", "#000000")));
                        palette->setValue(QVariant(QLCPalette::colorToString(rgb, wauv)));
                    }
                    break;
                    case QLCPalette::Pan:
                    {
                        double deg = item.value("panDegrees", 0.0);
                        palette->setValue(QVariant(deg));
                    }
                    break;
                    case QLCPalette::Tilt:
                    {
                        double deg = item.value("tiltDegrees", 0.0);
                        palette->setValue(QVariant(deg));
                    }
                    break;
                    case QLCPalette::PanTilt:
                    {
                        double panDeg = item.value("panDegrees", 0.0);
                        double tiltDeg = item.value("tiltDegrees", 0.0);
                        palette->setValue(QVariant(panDeg), QVariant(tiltDeg));
                    }
                    break;
                    default:
                        break;
                }

                // Apply fanning if provided
                if (item.contains("fanning") && item.at("fanning").is_object())
                    applyFanning(palette, item.at("fanning"));

                if (isNew)
                {
                    palette->setTemporary(false);
                    doc->addPalette(palette);
                }

                results.push_back({
                    {"id", (int)palette->id()},
                    {"name", palette->name().toStdString()},
                    {"type", QLCPalette::typeToString(palette->type()).toStdString()},
                    {"status", isNew ? "created" : "updated"}
                });
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("Create or update palettes — reusable value definitions for Dimmer, Color, Pan, Tilt, PanTilt. "
                     "Upserts by name+type. Palettes are the building blocks for scenes: create palettes first, then "
                     "reference them in create_scenes via paletteNames/paletteIDs. Supports fanning for distributing "
                     "values across fixtures (gradients, waves). Batch."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotIdempotent));

    // delete_palettes (batch)
    tm.register_tool(Tool(
        "delete_palettes",
        Json{{"type", "object"}, {"properties", {
            {"ids", {{"type", "array"}, {"items", {{"type", "integer"}}}, {"description", "Palette IDs to delete"}}},
            {"names", {{"type", "array"}, {"items", {{"type", "string"}}}, {"description", "Palette names to delete (glob patterns: * and ?)"}}}
        }}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto err = validateFields(args, {"ids", "names"});
            if (!err.empty()) return err;

            QList<quint32> idsToDelete;

            // Collect IDs from direct ID list
            if (args.contains("ids") && args.at("ids").is_array())
            {
                for (auto &idVal : args.at("ids"))
                    idsToDelete.append(idVal.get<int>());
            }

            // Collect IDs from name patterns
            if (args.contains("names") && args.at("names").is_array())
            {
                for (auto &nameVal : args.at("names"))
                {
                    QString pattern = QString::fromStdString(nameVal.get<std::string>());
                    QRegularExpression re(
                        QRegularExpression::wildcardToRegularExpression(pattern),
                        QRegularExpression::CaseInsensitiveOption);
                    for (QLCPalette *p : doc->palettes())
                    {
                        if (re.match(p->name()).hasMatch() && !idsToDelete.contains(p->id()))
                            idsToDelete.append(p->id());
                    }
                }
            }

            // Remove palette refs from any scenes that reference them
            for (Function *fn : doc->functions())
            {
                if (fn->type() != Function::SceneType) continue;
                Scene *scene = qobject_cast<Scene*>(fn);
                if (!scene) continue;
                for (quint32 palId : idsToDelete)
                    scene->removePalette(palId);
            }

            // Delete palettes
            Json results = Json::array();
            for (quint32 id : idsToDelete)
            {
                QLCPalette *p = doc->palette(id);
                if (!p)
                {
                    results.push_back({{"id", (int)id}, {"status", "not found"}});
                    continue;
                }
                QString name = p->name();
                doc->deletePalette(id);
                results.push_back({{"id", (int)id}, {"name", name.toStdString()}, {"status", "deleted"}});
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("Delete palettes by ID or name pattern (glob). Automatically removes palette references from any scenes that use them. Batch."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotDestructive));
}
