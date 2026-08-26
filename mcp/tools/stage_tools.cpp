/*
  Q Light Controller Plus
  stage_tools.cpp

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
#include "doc.h"
#include "fixture.h"
#include "channelsgroup.h"
#include "monitorproperties.h"
#include "scenevalue.h"

#include <QColor>
#include <QVector3D>

#include <fastmcpp/tools/manager.hpp>
#include <fastmcpp/tools/tool.hpp>

namespace {

using Json = nlohmann::json;

/** Positions and sizes in MonitorProperties are millimetres throughout. */
Json vectorToJson(const QVector3D &v)
{
    return Json{{"x", v.x()}, {"y", v.y()}, {"z", v.z()}};
}

/**
 * Read an {x,y,z} object. Missing components keep their current value, so a
 * caller can nudge one axis without restating the others.
 */
std::optional<std::string> readVector(const Json &obj, const char *field, QVector3D &out)
{
    if (!obj.contains(field))
        return std::nullopt;
    const Json &v = obj.at(field);
    if (!v.is_object())
        return Json({{"error", std::string(field) + " must be an object with x, y and z"}}).dump();

    auto err = validateFields(v, {"x", "y", "z"});
    if (!err.empty()) return err;

    for (const char *axis : {"x", "y", "z"})
    {
        if (!v.contains(axis))
            continue;
        if (!v.at(axis).is_number())
            return Json({{"error", std::string(field) + "." + axis + " must be a number"}}).dump();
        const float value = v.at(axis).get<float>();
        if (axis[0] == 'x') out.setX(value);
        else if (axis[0] == 'y') out.setY(value);
        else out.setZ(value);
    }
    return std::nullopt;
}

/** Heads are addressed by index; validate against the fixture's real head count. */
std::optional<std::string> readHead(const Json &item, const Fixture *fixture, quint16 &head)
{
    if (!item.contains("head"))
    {
        head = 0;
        return std::nullopt;
    }
    if (!item.at("head").is_number_integer() || item.at("head").get<int>() < 0)
        return Json({{"error", "head must be a non-negative integer"}}).dump();

    const int value = item.at("head").get<int>();
    const int heads = fixture->heads();
    if (value >= heads)
        return Json({{"error", "head index out of range"},
                     {"head", value}, {"heads", heads}}).dump();
    head = (quint16)value;
    return std::nullopt;
}

}

void registerStageTools(fastmcpp::tools::ToolManager &tm, Doc *doc)
{
    using Tool = fastmcpp::tools::Tool;

    // set_fixture_placement — where a fixture sits in the 2D/3D stage views
    tm.register_tool(Tool(
        "set_fixture_placement",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"fixtureID", {{"type", "integer"}, {"minimum", 0}}},
                {"head", {{"type", "integer"}, {"minimum", 0},
                          {"description", "Head index for multi-head fixtures. Default 0."}}},
                {"position", {{"type", "object"}, {"properties", {
                    {"x", {{"type", "number"}}}, {"y", {{"type", "number"}}}, {"z", {{"type", "number"}}}
                }}, {"description", "Stage position in MILLIMETRES. Omitted axes keep their value."}}},
                {"rotation", {{"type", "object"}, {"properties", {
                    {"x", {{"type", "number"}}}, {"y", {{"type", "number"}}}, {"z", {{"type", "number"}}}
                }}, {"description", "Rotation in degrees per axis. Omitted axes keep their value."}}},
                {"gelColor", {{"type", "string"},
                              {"description", "Gel colour as #rrggbb or an SVG colour name"}}}
            }}, {"required", {"fixtureID"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto itemsErr = validateItemsArray(args);
            if (itemsErr) return *itemsErr;

            MonitorProperties *props = doc->monitorProperties();
            Json results = Json::array();
            for (auto &item : args.at("items"))
            {
                auto err = validateFields(item, {"fixtureID", "head", "position", "rotation", "gelColor"});
                if (!err.empty()) { results.push_back(Json::parse(err)); continue; }

                if (!item.contains("fixtureID") || !item.at("fixtureID").is_number_integer())
                { results.push_back({{"error", "fixtureID must be an integer"}}); continue; }

                const quint32 fxID = item.at("fixtureID").get<quint32>();
                Fixture *fixture = doc->fixture(fxID);
                if (fixture == NULL)
                { results.push_back({{"fixtureID", (int)fxID}, {"error", "fixture not found"}}); continue; }

                quint16 head = 0;
                auto headErr = readHead(item, fixture, head);
                if (headErr) { results.push_back(Json::parse(*headErr)); continue; }

                QVector3D position = props->fixturePosition(fxID, head, 0);
                auto posErr = readVector(item, "position", position);
                if (posErr) { results.push_back(Json::parse(*posErr)); continue; }

                QVector3D rotation = props->fixtureRotation(fxID, head, 0);
                auto rotErr = readVector(item, "rotation", rotation);
                if (rotErr) { results.push_back(Json::parse(*rotErr)); continue; }

                QColor gel;
                if (item.contains("gelColor"))
                {
                    if (!item.at("gelColor").is_string())
                    { results.push_back({{"fixtureID", (int)fxID}, {"error", "gelColor must be a string"}}); continue; }
                    gel = QColor(QString::fromStdString(item.at("gelColor").get<std::string>()));
                    if (!gel.isValid())
                    {
                        results.push_back({{"fixtureID", (int)fxID},
                                           {"error", "gelColor is not a valid colour"}});
                        continue;
                    }
                }

                props->setFixturePosition(fxID, head, 0, position);
                props->setFixtureRotation(fxID, head, 0, rotation);
                if (gel.isValid())
                    props->setFixtureGelColor(fxID, head, 0, gel);
                doc->setModified();

                // An unset gel is an invalid QColor, whose name() is "#000000" —
                // reporting that would turn "no gel" into a real black gel on the
                // next round trip.
                const QColor storedGel = props->fixtureGelColor(fxID, head, 0);
                Json entry = {
                    {"fixtureID", (int)fxID}, {"head", (int)head},
                    {"position", vectorToJson(position)},
                    {"rotation", vectorToJson(rotation)},
                    {"status", "ok"}
                };
                if (storedGel.isValid())
                    entry["gelColor"] = storedGel.name().toStdString();
                results.push_back(entry);
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("Place fixtures in the 2D/3D stage views. Batch: {\"items\": [...]}. "
                     "Positions are in MILLIMETRES from the stage origin and rotations in degrees; "
                     "omitted axes keep their current value. This is view geometry only — it does "
                     "not change any DMX output."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotIdempotent));

    // query_fixture_placement — read back stage geometry
    tm.register_tool(Tool(
        "query_fixture_placement",
        Json{{"type", "object"}, {"properties", {
            {"fixtureID", {{"type", "integer"}, {"minimum", 0},
                           {"description", "Restrict to one fixture. Omit for all placed fixtures."}}}
        }}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto err = validateFields(args, {"fixtureID"});
            if (!err.empty()) return err;
            if (args.contains("fixtureID") && !args.at("fixtureID").is_number_integer())
                return Json({{"error", "fixtureID must be an integer"}}).dump();

            MonitorProperties *props = doc->monitorProperties();
            Json results = Json::array();
            for (Fixture *fixture : doc->fixtures())
            {
                if (fixture == NULL)
                    continue;
                if (args.contains("fixtureID") && fixture->id() != args.at("fixtureID").get<quint32>())
                    continue;

                for (int head = 0; head < fixture->heads(); head++)
                {
                    if (!props->containsItem(fixture->id(), (quint16)head, 0))
                        continue;
                    Json row = {
                        {"fixtureID", (int)fixture->id()},
                        {"name", fixture->name().toStdString()},
                        {"head", head},
                        {"position", vectorToJson(props->fixturePosition(fixture->id(), (quint16)head, 0))},
                        {"rotation", vectorToJson(props->fixtureRotation(fixture->id(), (quint16)head, 0))}
                    };
                    const QColor gel = props->fixtureGelColor(fixture->id(), (quint16)head, 0);
                    if (gel.isValid())
                        row["gelColor"] = gel.name().toStdString();
                    results.push_back(row);
                }
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("Read stage placement (millimetre position, degree rotation, gel colour) for "
                     "fixtures that have been placed; fixtures never placed are omitted, and "
                     "gelColor is absent when no gel was set. Head 0 is the fixture's own root "
                     "entry and appears once any head of that fixture is placed. Linked fixture "
                     "copies are not reported."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotReadOnly));

    // configure_stage — the environment the 2D/3D views draw fixtures into
    tm.register_tool(Tool(
        "configure_stage",
        Json{{"type", "object"}, {"properties", {
            {"size", {{"type", "object"}, {"properties", {
                {"x", {{"type", "number"}, {"exclusiveMinimum", 0}}},
                {"y", {{"type", "number"}, {"exclusiveMinimum", 0}}},
                {"z", {{"type", "number"}, {"exclusiveMinimum", 0}}}
            }}, {"description", "Stage dimensions in the current grid units, all positive"}}},
            {"units", {{"type", "string"}, {"enum", {"meters", "feet"}}}},
            {"stageType", {{"type", "string"}, {"enum", {"simple", "box", "rock", "theatre"}}}},
            {"showLabels", {{"type", "boolean"}}}
        }}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto err = validateFields(args, {"size", "units", "stageType", "showLabels"});
            if (!err.empty()) return err;

            static const Json kEnums = {
                {"units", {{"enum", {"meters", "feet"}}}},
                {"stageType", {{"enum", {"simple", "box", "rock", "theatre"}}}}
            };
            auto enumErr = validateEnums(args, kEnums);
            if (!enumErr.empty()) return enumErr;

            MonitorProperties *props = doc->monitorProperties();

            QVector3D size = props->gridSize();
            auto sizeErr = readVector(args, "size", size);
            if (sizeErr) return *sizeErr;
            if (args.contains("size"))
            {
                if (size.x() <= 0 || size.y() <= 0 || size.z() <= 0)
                    return Json({{"error", "stage size must be positive on every axis"},
                                 {"size", vectorToJson(size)}}).dump();
                props->setGridSize(size);
            }

            if (args.contains("units"))
            {
                const std::string units = toLowerStd(args.at("units").get<std::string>());
                props->setGridUnits(units == "feet" ? MonitorProperties::Feet
                                                    : MonitorProperties::Meters);
            }
            if (args.contains("stageType"))
            {
                const std::string type = toLowerStd(args.at("stageType").get<std::string>());
                if (type == "box") props->setStageType(MonitorProperties::StageBox);
                else if (type == "rock") props->setStageType(MonitorProperties::StageRock);
                else if (type == "theatre") props->setStageType(MonitorProperties::StageTheatre);
                else props->setStageType(MonitorProperties::StageSimple);
            }
            // Deliberately no pointOfView here: MonitorProperties::setPointOfView
            // coordinate-transforms every placed fixture (and can overwrite the
            // grid size), and re-applies the transform whenever the current view
            // is Undefined — so setting it twice walks the whole rig off-stage.
            // Choosing a view belongs in the UI, not in a configuration tool.
            if (args.contains("showLabels"))
            {
                if (!args.at("showLabels").is_boolean())
                    return Json({{"error", "showLabels must be a boolean"}}).dump();
                props->setLabelsVisible(args.at("showLabels").get<bool>());
            }

            doc->setModified();
            return Json({
                {"status", "ok"},
                {"size", vectorToJson(props->gridSize())},
                {"units", props->gridUnits() == MonitorProperties::Feet ? "feet" : "meters"},
                {"showLabels", props->labelsVisible()}
            }).dump();
            });
        },
        std::nullopt,
        std::string("Configure the stage the 2D/3D views draw: dimensions, units, stage type and "
                     "fixture labels. Point of view is not settable here — changing it rewrites "
                     "every fixture's coordinates, so it belongs in the UI."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotIdempotent));

    // create_channel_groups — named sets of raw channels for Simple Desk
    tm.register_tool(Tool(
        "create_channel_groups",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"name", {{"type", "string"}}},
                {"channels", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                    {"fixtureID", {{"type", "integer"}, {"minimum", 0}}},
                    {"channel", {{"type", "integer"}, {"minimum", 0}}}
                }}, {"required", {"fixtureID", "channel"}}}}}}
            }}, {"required", {"name", "channels"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto itemsErr = validateItemsArray(args);
            if (itemsErr) return *itemsErr;

            Json results = Json::array();
            for (auto &item : args.at("items"))
            {
                auto err = validateFields(item, {"name", "channels"});
                if (!err.empty()) { results.push_back(Json::parse(err)); continue; }

                if (!item.contains("name") || !item.at("name").is_string())
                { results.push_back({{"error", "name is required and must be a string"}}); continue; }
                const QString name = QString::fromStdString(item.at("name").get<std::string>());
                if (name.isEmpty())
                { results.push_back({{"error", "name must not be empty"}}); continue; }
                if (!item.contains("channels") || !item.at("channels").is_array())
                { results.push_back({{"name", name.toStdString()},
                                     {"error", "channels is required and must be an array"}}); continue; }

                // Upsert by name, like every other create_* tool.
                ChannelsGroup *group = NULL;
                bool isNew = true;
                for (ChannelsGroup *existing : doc->channelsGroups())
                {
                    if (existing != NULL && existing->name() == name)
                    { group = existing; isNew = false; break; }
                }
                if (group == NULL)
                {
                    group = new ChannelsGroup(doc);
                    group->setName(name);
                }

                // Validate every channel before mutating, so a bad entry cannot
                // leave a half-rebuilt group behind.
                QList<SceneValue> channels;
                std::optional<Json> failure;
                for (auto &entry : item.at("channels"))
                {
                    auto entryErr = validateFields(entry, {"fixtureID", "channel"});
                    if (!entryErr.empty()) { failure = Json::parse(entryErr); break; }
                    if (!entry.contains("fixtureID") || !entry.contains("channel") ||
                        !entry.at("fixtureID").is_number_integer() ||
                        !entry.at("channel").is_number_integer())
                    { failure = Json({{"error", "fixtureID and channel must be integers"}}); break; }

                    const quint32 fxID = entry.at("fixtureID").get<quint32>();
                    const quint32 channel = entry.at("channel").get<quint32>();
                    Fixture *fixture = doc->fixture(fxID);
                    if (fixture == NULL)
                    { failure = Json({{"error", "fixture not found"}, {"fixtureID", (int)fxID}}); break; }
                    if (channel >= fixture->channels())
                    {
                        failure = Json({{"error", "channel out of range for fixture"},
                                        {"fixtureID", (int)fxID}, {"channel", (int)channel},
                                        {"channels", (int)fixture->channels()}});
                        break;
                    }
                    channels.append(SceneValue(fxID, channel, 0));
                }
                if (failure)
                {
                    if (isNew) delete group;
                    Json entry = *failure;
                    entry["name"] = name.toStdString();
                    results.push_back(entry);
                    continue;
                }

                // addChannel only appends, so clear first to make a second call
                // replace the membership. Updating in place keeps the group's id,
                // its position in the ordered list and any input source mapped to
                // it — recreating the object would silently drop all three.
                group->resetChannels();
                for (const SceneValue &sv : channels)
                    group->addChannel(sv.fxi, sv.channel);
                if (isNew)
                    doc->addChannelsGroup(group);
                else
                    doc->setModified();

                results.push_back({{"id", (int)group->id()}, {"name", name.toStdString()},
                                   {"channels", (int)channels.size()},
                                   {"status", isNew ? "created" : "updated"}});
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("Create or update named channel groups — arbitrary sets of raw fixture channels "
                     "driven together from Simple Desk. Upserts by name; a second call with the same "
                     "name replaces the membership. Batch: {\"items\": [...]}."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotIdempotent));

    // query_channel_groups
    tm.register_tool(Tool(
        "query_channel_groups",
        Json{{"type", "object"}, {"properties", Json::object()}},
        Json{},
        [doc](const Json &) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            for (ChannelsGroup *group : doc->channelsGroups())
            {
                if (group == NULL)
                    continue;
                Json channels = Json::array();
                for (const SceneValue &sv : group->getChannels())
                    channels.push_back({{"fixtureID", (int)sv.fxi}, {"channel", (int)sv.channel}});
                results.push_back({{"id", (int)group->id()},
                                   {"name", group->name().toStdString()},
                                   {"channels", channels}});
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("List channel groups with their member fixture channels."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotReadOnly));

    // delete_channel_groups
    tm.register_tool(Tool(
        "delete_channel_groups",
        Json{{"type", "object"}, {"properties", {
            {"ids", {{"type", "array"}, {"items", {{"type", "integer"}}},
                     {"description", "Channel group IDs to delete"}}}
        }}, {"required", {"ids"}}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto err = validateFields(args, {"ids"});
            if (!err.empty()) return err;
            if (!args.contains("ids") || !args.at("ids").is_array())
                return Json({{"error", "ids must be an array of integers"}}).dump();

            Json results = Json::array();
            for (auto &v : args.at("ids"))
            {
                if (!v.is_number_integer())
                { results.push_back({{"error", "ids must be an array of integers"}}); continue; }
                const quint32 id = v.get<quint32>();
                ChannelsGroup *group = doc->channelsGroup(id);
                if (group == NULL)
                { results.push_back({{"id", (int)id}, {"status", "not found"}}); continue; }

                const std::string name = group->name().toStdString();
                if (doc->deleteChannelsGroup(id))
                    results.push_back({{"id", (int)id}, {"name", name}, {"status", "deleted"}});
                else
                    results.push_back({{"id", (int)id}, {"error", "could not delete channel group"}});
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("Delete channel groups by ID. Batch: {\"ids\": [...]}. The fixtures and their "
                     "channels are untouched; only the grouping is removed."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotDestructive));
}
