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
#include <map>
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
    // Delegate to the engine so there is a single source of truth for the
    // palette-type spellings (incl. Position3D, Shutter, Gobo, Zoom).
    return QLCPalette::stringToType(QString::fromStdString(s));
}

using Json = nlohmann::json;

// Parses one create_palettes item into palette values without touching the Doc.
// Returns an error message, or empty on success.
std::string parsePaletteValues(const Json &item, QLCPalette::PaletteType ptype, QVariantList &values)
{
    auto integer = [&](const char *key, int64_t def, int64_t lo, int64_t hi, std::string &err) -> QVariant {
        if (!item.contains(key)) return QVariant(int(def));
        auto v = mcp::jsonInteger(item.at(key), lo, hi);
        if (!v) { err = mcp::integerError(key, lo, hi); return QVariant(); }
        return QVariant(int(*v));
    };
    auto number = [&](const char *key, std::string &err) -> QVariant {
        if (!item.contains(key)) return QVariant(0.0);
        const Json &v = item.at(key);
        if (!v.is_number() || !std::isfinite(v.get<double>()))
        {
            err = std::string(key) + " must be a finite number";
            return QVariant();
        }
        return QVariant(v.get<double>());
    };
    auto color = [&](const char *key, const char *def, std::string &err) -> QColor {
        if (!item.contains(key)) return QColor(def);
        const Json &v = item.at(key);
        QColor c = v.is_string() ? QColor(QString::fromStdString(v.get<std::string>())) : QColor();
        if (!c.isValid()) err = std::string(key) + " must be a color string like '#rrggbb'";
        return c;
    };

    std::string err;
    switch (ptype)
    {
        case QLCPalette::Dimmer: values << integer("value", 255, 0, 255, err); break;
        case QLCPalette::Color:
        {
            QColor rgb = color("rgb", "#ffffff", err);
            QColor wauv = color("wauv", "#000000", err);
            values << QVariant(QLCPalette::colorToString(rgb, wauv));
        }
        break;
        case QLCPalette::Pan: values << number("panDegrees", err); break;
        case QLCPalette::Tilt: values << number("tiltDegrees", err); break;
        case QLCPalette::PanTilt: values << number("panDegrees", err) << number("tiltDegrees", err); break;
        case QLCPalette::Position3D: values << number("x", err) << number("y", err) << number("z", err); break;
        case QLCPalette::Shutter: values << integer("value", 0, 0, 255, err) << integer("value2", 0, 0, 100, err); break;
        case QLCPalette::Gobo: values << integer("value", 0, 0, 255, err); break;
        case QLCPalette::Zoom:
        {
            // Zoom degrees may be fractional (the GUI authors them that way);
            // whole numbers stay ints so existing readback is unchanged.
            if (item.contains("value") && item.at("value").is_number_float())
            {
                QVariant v = number("value", err);
                if (err.empty() && (v.toDouble() < 0 || v.toDouble() > 360))
                    err = "value must be a number in [0, 360]";
                values << v;
            }
            else
                values << integer("value", 0, 0, 360, err);
        }
        break;
        default: break;
    }
    return err;
}

} // anonymous namespace

void registerPaletteTools(fastmcpp::tools::ToolManager &tm, Doc *doc)
{
    using Json = nlohmann::json;
    using Tool = fastmcpp::tools::Tool;

    // create_palettes (batch)
    static const std::string typeDesc =
        "Palette type: Dimmer, Color, Pan, Tilt, PanTilt, Position3D, Shutter, Gobo, Zoom";

    // Default-initialize absent icons instead of moving a nullopt icon vector
    // through the extended constructor (GCC 13 -Wmaybe-uninitialized).
    tm.register_tool(Tool(
        "create_palettes",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"name", {{"type", "string"}, {"description", "Palette name (required)"}}},
                {"type", {{"type", "string"}, {"enum", {"Dimmer", "Color", "Pan", "Tilt", "PanTilt", "Position3D", "Shutter", "Gobo", "Zoom"}}, {"description", typeDesc}}},
                {"value", {{"type", "number"}, {"description", "Integer for Dimmer intensity 0-255, Gobo, and the Shutter preset; Zoom degrees 0-360 may be fractional"}}},
                {"value2", {{"type", "integer"}, {"description", "Shutter percentage 0-100; used with `value` as the shutter preset"}}},
                {"panDegrees", {{"type", "number"}, {"description", "Pan degrees (Pan or PanTilt type)"}}},
                {"tiltDegrees", {{"type", "number"}, {"description", "Tilt degrees (Tilt or PanTilt type)"}}},
                {"rgb", {{"type", "string"}, {"description", "Color: RGB hex '#rrggbb'"}}},
                {"wauv", {{"type", "string"}, {"description", "Color: White/Amber/UV hex '#wwaauu' (optional, default '#000000')"}}},
                {"x", {{"type", "number"}, {"description", "Position3D: X coordinate"}}},
                {"y", {{"type", "number"}, {"description", "Position3D: Y coordinate"}}},
                {"z", {{"type", "number"}, {"description", "Position3D: Z coordinate"}}}
            }}, {"required", {"name", "type"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto err = validateFields(args, {"items"});
            if (!err.empty()) return err;
            auto itemsErr = validateItemsArray(args);
            if (itemsErr) return *itemsErr;

            Json results = Json::array();
            const Json &items = args.at("items");
            for (size_t i = 0; i < items.size(); ++i)
            {
                const Json &item = items[i];
                if (!item.is_object()) { results.push_back(mcp::itemError(i, "item must be an object")); continue; }
                auto itemErr = validateFields(item, {"name", "type", "value", "value2", "panDegrees", "tiltDegrees", "rgb", "wauv", "x", "y", "z"});
                if (!itemErr.empty()) { results.push_back(mcp::itemErrorFromDump(i, itemErr)); continue; }

                if (!item.contains("name") || !item.contains("type"))
                {
                    results.push_back(mcp::itemError(i, "name and type are required"));
                    continue;
                }
                if (!item.at("name").is_string() || !item.at("type").is_string())
                {
                    results.push_back(mcp::itemError(i, "name and type must be strings"));
                    continue;
                }

                QString name = QString::fromStdString(item.at("name").get<std::string>());
                QLCPalette::PaletteType ptype = stringToPaletteType(item.at("type").get<std::string>());
                if (ptype == QLCPalette::Undefined)
                {
                    Json e = mcp::itemError(i, "invalid type. Must be: Dimmer, Color, Pan, Tilt, PanTilt, Position3D, Shutter, Gobo, Zoom");
                    e["name"] = name.toStdString();
                    results.push_back(e);
                    continue;
                }

                QVariantList values;
                std::string valueErr = parsePaletteValues(item, ptype, values);
                if (!valueErr.empty()) { results.push_back(mcp::itemError(i, valueErr)); continue; }

                // Upsert: find existing by name+type
                QLCPalette *palette = findPaletteByNameAndType(doc, name, ptype);
                bool isNew = (palette == nullptr);
                if (isNew)
                {
                    palette = new QLCPalette(ptype);
                    palette->setName(name);
                }

                palette->resetValues();
                if (values.size() == 1) palette->setValue(values[0]);
                else if (values.size() == 2) palette->setValue(values[0], values[1]);
                else if (values.size() == 3) palette->setValue(values[0], values[1], values[2]);

                if (isNew)
                {
                    palette->setTemporary(false);
                    if (!doc->addPalette(palette))
                    {
                        delete palette;
                        results.push_back(mcp::itemError(i, "Doc refused to add palette"));
                        continue;
                    }
                }
                else
                {
                    doc->setModified();
                }

                results.push_back({
                    {"index", i},
                    {"id", (int)palette->id()},
                    {"name", palette->name().toStdString()},
                    {"type", QLCPalette::typeToString(palette->type()).toStdString()},
                    {"status", "ok"},
                    {"outcome", isNew ? "created" : "updated"}
                });
            }
            return results.dump();
            });
        }
    )
    .set_description("Create or update palettes — reusable value definitions for Dimmer, Color, Pan, Tilt, PanTilt, "
                     "Position3D (x/y/z), Shutter (preset `value` + `value2` percentage 0-100), Gobo, Zoom (single value). "
                     "Upserts by name+type. Palettes are the building blocks for scenes: create palettes first, then "
                     "reference them in create_scenes via paletteNames/paletteIDs. Batch. "
                     "Wrap multiple operations in {\"items\": [...]}. Each item is processed independently.")
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

            for (const char *key : {"ids", "names"})
                if (args.contains(key) && !args.at(key).is_array())
                    return Json({{"error", std::string(key) + " must be an array"}}).dump();
            const Json ids = args.value("ids", Json::array());
            const Json names = args.value("names", Json::array());

            // Globs match against the palettes present when the call started, so a
            // match already deleted by an earlier selector reports duplicateOf.
            QList<QPair<quint32, QString>> snapshot;
            for (QLCPalette *p : doc->palettes())
                snapshot.append({p->id(), p->name()});
            std::map<quint32, size_t> deletedAt;
            auto remove = [&](quint32 id) {
                for (Function *fn : doc->functions())
                    if (Scene *scene = qobject_cast<Scene *>(fn))
                        scene->removePalette(id);
                doc->deletePalette(id);
            };

            // ids first, then names: one record per selector, indexed globally.
            Json results = Json::array();
            for (size_t i = 0; i < ids.size(); ++i)
            {
                const auto idOpt = mcp::jsonInteger(ids[i], 0, mcp::kMaxId);
                if (!idOpt) { results.push_back(mcp::itemError(i, mcp::integerError("ids[]", 0, mcp::kMaxId))); continue; }
                const quint32 id = quint32(*idOpt);
                if (deletedAt.count(id))
                {
                    results.push_back(mcp::itemOk(i, {{"id", (int)id}, {"outcome", "duplicate"},
                                                      {"duplicateOf", deletedAt.at(id)}}));
                    continue;
                }
                QLCPalette *p = doc->palette(id);
                if (!p)
                {
                    Json r = mcp::itemError(i, "palette not found");
                    r["id"] = (int)id;
                    results.push_back(r);
                    continue;
                }
                const std::string name = p->name().toStdString();
                remove(id);
                deletedAt[id] = i;
                results.push_back(mcp::itemOk(i, {{"id", (int)id}, {"name", name}, {"outcome", "deleted"}}));
            }
            for (size_t j = 0; j < names.size(); ++j)
            {
                const size_t i = ids.size() + j;
                if (!names[j].is_string()) { results.push_back(mcp::itemError(i, "names[] must be a string")); continue; }
                QRegularExpression re(
                    QRegularExpression::wildcardToRegularExpression(QString::fromStdString(names[j].get<std::string>())),
                    QRegularExpression::CaseInsensitiveOption);
                Json matches = Json::array();
                bool deletedHere = false;
                for (const auto &[id, name] : snapshot)
                {
                    if (!re.match(name).hasMatch()) continue;
                    Json m = {{"id", (int)id}, {"name", name.toStdString()}};
                    if (deletedAt.count(id))
                        m["duplicateOf"] = deletedAt.at(id);
                    else
                    {
                        remove(id);
                        deletedAt[id] = i;
                        m["outcome"] = "deleted";
                        deletedHere = true;
                    }
                    matches.push_back(m);
                }
                results.push_back(mcp::itemOk(i, {{"name", names[j]}, {"palettes", matches},
                    {"outcome", deletedHere ? "deleted" : matches.empty() ? "noMatch" : "duplicate"}}));
            }
            return results.dump();
            });
        }
    )
    .set_description("Delete palettes by ID or name pattern (glob). Returns one record per selector, ids first then "
                     "names, indexed in that order; a glob nests its matches under palettes, reports outcome "
                     "\"noMatch\" when nothing matches, and marks palettes already deleted by an earlier "
                     "selector with duplicateOf. Automatically removes palette references from any scenes that use them. Batch.")
    .set_annotations(mcp::kAnnotDestructive));
}
