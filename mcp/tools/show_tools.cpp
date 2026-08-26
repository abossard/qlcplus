/*
  Q Light Controller Plus
  show_tools.cpp

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
#include "function.h"
#include "show.h"
#include "showfunction.h"
#include "track.h"

#include <fastmcpp/tools/manager.hpp>
#include <fastmcpp/tools/tool.hpp>

namespace {

using Json = nlohmann::json;

Json trackToJson(const Track *track, const Doc *doc)
{
    Json items = Json::array();
    for (ShowFunction *sf : track->showFunctions())
    {
        if (sf == NULL)
            continue;
        Function *fn = doc->function(sf->functionID());
        items.push_back({
            {"id", (int)sf->id()},
            {"functionID", (int)sf->functionID()},
            {"functionName", fn ? fn->name().toStdString() : std::string()},
            {"startTime", (int)sf->startTime()},
            {"duration", (int)sf->duration()},
            {"locked", sf->isLocked()}
        });
    }
    return Json{
        {"id", (int)track->id()},
        {"name", track->name().toStdString()},
        {"mute", track->isMute()},
        {"items", items}
    };
}

/** The exact strings create_shows accepts, so query output can be fed back in. */
std::string timeDivisionToString(Show::TimeDivision division)
{
    switch (division)
    {
        case Show::BPM_4_4: return "4/4";
        case Show::BPM_3_4: return "3/4";
        case Show::BPM_2_4: return "2/4";
        default:            return "time";
    }
}

/** Look up any function by name, regardless of type. */
Function *functionByName(const Doc *doc, const QString &name, bool &ambiguous)
{
    Function *found = NULL;
    ambiguous = false;
    for (Function *fn : doc->functions())
    {
        if (fn == NULL || fn->name() != name)
            continue;
        if (found != NULL) { ambiguous = true; return NULL; }
        found = fn;
    }
    return found;
}

/** End of an item on the timeline, in milliseconds. */
quint32 endOf(const ShowFunction *sf)
{
    return sf->startTime() + sf->duration();
}

}

void registerShowTools(fastmcpp::tools::ToolManager &tm, Doc *doc)
{
    using Tool = fastmcpp::tools::Tool;

    // create_shows — a Show with its tracks
    tm.register_tool(Tool(
        "create_shows",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"name", {{"type", "string"}}},
                {"path", {{"type", "string"}, {"description", "Folder path, e.g. 'Shows/Summer'"}}},
                {"tempoType", {{"type", "string"}, {"enum", {"time", "4/4", "3/4", "2/4"}},
                               {"description", "Timeline division: time, or a beat signature. Default time."}}},
                {"bpm", {{"type", "integer"}, {"minimum", 1},
                         {"description", "Timeline BPM when tempoType is beats"}}},
                {"tracks", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                    {"name", {{"type", "string"}}},
                    {"mute", {{"type", "boolean"}}}
                }}, {"required", {"name"}}}},
                            {"description", "Tracks to create. Existing tracks are matched by name."}}}
            }}, {"required", {"name"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto itemsErr = validateItemsArray(args);
            if (itemsErr) return *itemsErr;

            static const Json kEnums = {{"tempoType", {{"enum", {"time", "4/4", "3/4", "2/4"}}}}};

            Json results = Json::array();
            for (auto &item : args.at("items"))
            {
                auto err = validateFields(item, {"name", "path", "tempoType", "bpm", "tracks"});
                if (!err.empty()) { results.push_back(Json::parse(err)); continue; }
                auto enumErr = validateEnums(item, kEnums);
                if (!enumErr.empty()) { results.push_back(Json::parse(enumErr)); continue; }

                if (!item.contains("name") || !item.at("name").is_string())
                { results.push_back({{"error", "name is required and must be a string"}}); continue; }
                const QString name = QString::fromStdString(item.at("name").get<std::string>());
                if (name.isEmpty())
                { results.push_back({{"error", "name must not be empty"}}); continue; }

                // Validate everything before the first mutation: a rejected item
                // must leave no half-applied show behind.
                if (item.contains("path") && !item.at("path").is_string())
                { results.push_back({{"name", name.toStdString()},
                                     {"error", "path must be a string"}}); continue; }
                if (item.contains("bpm") &&
                    (!item.at("bpm").is_number_integer() || item.at("bpm").get<int>() < 1))
                { results.push_back({{"name", name.toStdString()},
                                     {"error", "bpm must be an integer of at least 1"}}); continue; }
                if (item.contains("tracks") && !item.at("tracks").is_array())
                { results.push_back({{"name", name.toStdString()},
                                     {"error", "tracks must be an array"}}); continue; }

                Function *existing = mcp::findFunction(doc, name, Function::ShowType);
                Show *show = qobject_cast<Show*>(existing);
                const bool isNew = show == NULL;
                if (isNew)
                {
                    show = new Show(doc);
                    show->setName(name);
                }

                if (item.contains("path"))
                    show->setPath(QString::fromStdString(item.at("path").get<std::string>()));

                if (item.contains("tempoType") || item.contains("bpm"))
                {
                    const std::string tempo = toLowerStd(item.value("tempoType", std::string("time")));
                    const int bpm = item.value("bpm", show->timeDivisionBPM() > 0
                                                          ? show->timeDivisionBPM() : 120);
                    Show::TimeDivision division = Show::Time;
                    if (tempo == "4/4") division = Show::BPM_4_4;
                    else if (tempo == "3/4") division = Show::BPM_3_4;
                    else if (tempo == "2/4") division = Show::BPM_2_4;
                    show->setTimeDivision(division, bpm);
                }

                if (isNew && !doc->addFunction(show))
                {
                    delete show;
                    results.push_back({{"name", name.toStdString()},
                                       {"error", "could not add show to the project"}});
                    continue;
                }

                // Tracks upsert by name so calling twice does not duplicate them.
                Json tracks = Json::array();
                if (item.contains("tracks"))
                {
                    for (auto &entry : item.at("tracks"))
                    {
                        auto trackErr = validateFields(entry, {"name", "mute"});
                        if (!trackErr.empty()) { tracks.push_back(Json::parse(trackErr)); continue; }
                        if (!entry.contains("name") || !entry.at("name").is_string())
                        { tracks.push_back({{"error", "track name must be a string"}}); continue; }

                        const QString trackName =
                            QString::fromStdString(entry.at("name").get<std::string>());

                        Track *track = NULL;
                        for (Track *candidate : show->tracks())
                            if (candidate != NULL && candidate->name() == trackName)
                            { track = candidate; break; }

                        const bool trackIsNew = track == NULL;
                        if (trackIsNew)
                        {
                            // Parent to the show: Track::createShowFunction reads
                            // parent() to allocate ShowFunction ids, so an
                            // unparented track hands every item the id 0.
                            track = new Track(Function::invalidId(), show);
                            track->setName(trackName);
                            if (!show->addTrack(track))
                            {
                                delete track;
                                tracks.push_back({{"name", trackName.toStdString()},
                                                  {"error", "could not add track"}});
                                continue;
                            }
                        }
                        if (entry.contains("mute"))
                        {
                            if (!entry.at("mute").is_boolean())
                            { tracks.push_back({{"name", trackName.toStdString()},
                                                {"error", "mute must be a boolean"}}); continue; }
                            track->setMute(entry.at("mute").get<bool>());
                        }

                        tracks.push_back({{"id", (int)track->id()},
                                          {"name", trackName.toStdString()},
                                          {"status", trackIsNew ? "created" : "updated"}});
                    }
                }

                // Show::setPath/setTimeDivision/addTrack and Track::setMute emit
                // nothing Doc listens to, so the dirty flag has to be set here or
                // load_workspace would discard these edits without warning.
                doc->setModified();

                results.push_back({{"id", (int)show->id()}, {"name", name.toStdString()},
                                   {"tracks", tracks},
                                   {"status", isNew ? "created" : "updated"}});
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("Create or update Shows and their tracks. Upserts by name, and tracks upsert by "
                     "name within the show, so repeated calls do not duplicate. Put functions on the "
                     "timeline with add_show_items. Batch: {\"items\": [...]}."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotIdempotent));

    // query_shows — the timeline as data
    tm.register_tool(Tool(
        "query_shows",
        Json{{"type", "object"}, {"properties", {
            {"showID", {{"type", "integer"}, {"minimum", 0},
                        {"description", "Restrict to one show. Omit for all shows."}}}
        }}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto err = validateFields(args, {"showID"});
            if (!err.empty()) return err;
            if (args.contains("showID") && !args.at("showID").is_number_integer())
                return Json({{"error", "showID must be an integer"}}).dump();

            Json results = Json::array();
            for (Function *fn : doc->functions())
            {
                Show *show = qobject_cast<Show*>(fn);
                if (show == NULL)
                    continue;
                if (args.contains("showID") && show->id() != args.at("showID").get<quint32>())
                    continue;

                Json tracks = Json::array();
                for (Track *track : show->tracks())
                    if (track != NULL)
                        tracks.push_back(trackToJson(track, doc));

                results.push_back({
                    {"id", (int)show->id()},
                    {"name", show->name().toStdString()},
                    {"path", show->path().toStdString()},
                    {"tempoType", timeDivisionToString(show->timeDivisionType())},
                    {"beatsDivision", show->beatsDivision()},
                    {"bpm", show->timeDivisionBPM()},
                    {"totalDuration", (int)show->totalDuration()},
                    {"tracks", tracks}
                });
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("List Shows with their tracks and timeline items (start time and duration in "
                     "milliseconds)."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotReadOnly));

    // add_show_items — place functions on a track's timeline
    tm.register_tool(Tool(
        "add_show_items",
        Json{{"type", "object"}, {"properties", {
            {"showID", {{"type", "integer"}, {"minimum", 0}}},
            {"showName", {{"type", "string"}, {"description", "Alternative to showID"}}},
            {"trackName", {{"type", "string"}, {"description", "Track to add to; created if missing"}}},
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"functionID", {{"type", "integer"}, {"minimum", 0}}},
                {"functionName", {{"type", "string"}, {"description", "Alternative to functionID"}}},
                {"startTime", {{"type", "integer"}, {"minimum", 0},
                               {"description", "Start on the timeline, in milliseconds"}}},
                {"duration", {{"type", "integer"}, {"minimum", 1},
                              {"description", "Length in milliseconds. Defaults to the function's own duration."}}}
            }}, {"required", {"startTime"}}}}}}
        }}, {"required", {"trackName", "items"}}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto err = validateFields(args, {"showID", "showName", "trackName", "items"});
            if (!err.empty()) return err;
            auto itemsErr = validateItemsArray(args);
            if (itemsErr) return *itemsErr;

            Show *show = NULL;
            if (args.contains("showID"))
            {
                if (!args.at("showID").is_number_integer())
                    return Json({{"error", "showID must be an integer"}}).dump();
                show = qobject_cast<Show*>(doc->function(args.at("showID").get<quint32>()));
            }
            else if (args.contains("showName"))
            {
                if (!args.at("showName").is_string())
                    return Json({{"error", "showName must be a string"}}).dump();
                show = qobject_cast<Show*>(mcp::findFunction(
                    doc, QString::fromStdString(args.at("showName").get<std::string>()),
                    Function::ShowType));
            }
            else
            {
                return Json({{"error", "provide showID or showName"}}).dump();
            }
            if (show == NULL)
                return Json({{"error", "show not found"}}).dump();

            if (!args.contains("trackName") || !args.at("trackName").is_string())
                return Json({{"error", "trackName is required and must be a string"}}).dump();
            const QString trackName = QString::fromStdString(args.at("trackName").get<std::string>());

            Track *track = NULL;
            for (Track *candidate : show->tracks())
                if (candidate != NULL && candidate->name() == trackName)
                { track = candidate; break; }
            if (track == NULL)
            {
                track = new Track(Function::invalidId(), show);
                track->setName(trackName);
                if (!show->addTrack(track))
                {
                    delete track;
                    return Json({{"error", "could not create track"},
                                 {"trackName", trackName.toStdString()}}).dump();
                }
            }

            Json results = Json::array();
            for (auto &item : args.at("items"))
            {
                auto itemErr = validateFields(item, {"functionID", "functionName", "startTime", "duration"});
                if (!itemErr.empty()) { results.push_back(Json::parse(itemErr)); continue; }

                Function *function = NULL;
                if (item.contains("functionID"))
                {
                    if (!item.at("functionID").is_number_integer())
                    { results.push_back({{"error", "functionID must be an integer"}}); continue; }
                    function = doc->function(item.at("functionID").get<quint32>());
                }
                else if (item.contains("functionName"))
                {
                    if (!item.at("functionName").is_string())
                    { results.push_back({{"error", "functionName must be a string"}}); continue; }
                    // Function names are not unique in QLC+; refuse rather than
                    // silently binding to whichever one happens to come first.
                    bool ambiguous = false;
                    const QString wanted =
                        QString::fromStdString(item.at("functionName").get<std::string>());
                    function = functionByName(doc, wanted, ambiguous);
                    if (ambiguous)
                    {
                        results.push_back({{"functionName", wanted.toStdString()},
                                           {"error", "more than one function has this name — "
                                                     "use functionID"}});
                        continue;
                    }
                }
                if (function == NULL)
                { results.push_back({{"error", "function not found"}}); continue; }

                // A Show cannot contain itself, and nesting a Show inside a Show
                // is not something the timeline supports.
                if (function->id() == show->id() || function->type() == Function::ShowType)
                {
                    results.push_back({{"functionID", (int)function->id()},
                                       {"error", "a show cannot be placed on a show timeline"}});
                    continue;
                }

                if (!item.contains("startTime") || !item.at("startTime").is_number_integer() ||
                    item.at("startTime").get<int>() < 0)
                { results.push_back({{"error", "startTime must be a non-negative integer"}}); continue; }
                const quint32 startTime = item.at("startTime").get<quint32>();

                quint32 duration = function->totalDuration();
                if (item.contains("duration"))
                {
                    if (!item.at("duration").is_number_integer() || item.at("duration").get<int>() < 1)
                    { results.push_back({{"error", "duration must be a positive integer"}}); continue; }
                    duration = item.at("duration").get<quint32>();
                }
                if (duration == 0)
                    duration = 5000;   // same fallback the Show Manager uses

                // Overlaps on one track are not representable: the runner would
                // have two functions owning the same instant on the same track.
                bool overlaps = false;
                Json conflict;
                for (ShowFunction *existing : track->showFunctions())
                {
                    if (existing == NULL)
                        continue;
                    if (startTime < endOf(existing) && existing->startTime() < startTime + duration)
                    {
                        overlaps = true;
                        conflict = {{"id", (int)existing->id()},
                                    {"functionID", (int)existing->functionID()},
                                    {"startTime", (int)existing->startTime()},
                                    {"duration", (int)existing->duration()}};
                        break;
                    }
                }
                if (overlaps)
                {
                    results.push_back({{"functionID", (int)function->id()},
                                       {"startTime", (int)startTime}, {"duration", (int)duration},
                                       {"error", "overlaps an item already on this track"},
                                       {"conflictsWith", conflict}});
                    continue;
                }

                // ShowManager::addFunctions switches the function's tempo to match
                // the timeline, which is what connects it to BPM changes. Without
                // this the same show behaves differently depending on whether the
                // UI or MCP built it.
                function->setTempoType(Show::isTimeBasedDivision(show->timeDivisionType())
                                           ? Function::Time : Function::Beats);

                ShowFunction *sf = track->createShowFunction(function->id());
                sf->setStartTime(startTime);
                sf->setDuration(duration);
                sf->setColor(ShowFunction::defaultColor(function->type()));
                doc->setModified();

                results.push_back({{"id", (int)sf->id()},
                                   {"functionID", (int)function->id()},
                                   {"functionName", function->name().toStdString()},
                                   {"startTime", (int)startTime},
                                   {"duration", (int)duration},
                                   {"status", "added"}});
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("Place functions on a Show track's timeline. Times are milliseconds. The track "
                     "is created if it does not exist. An item overlapping one already on the same "
                     "track is refused, with the conflicting item reported. Batch: {\"items\": [...]}."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotIdempotent));

    // delete_show_items — remove timeline items, or whole tracks
    tm.register_tool(Tool(
        "delete_show_items",
        Json{{"type", "object"}, {"properties", {
            {"showID", {{"type", "integer"}, {"minimum", 0}}},
            {"itemIDs", {{"type", "array"}, {"items", {{"type", "integer"}}},
                         {"description", "ShowFunction IDs, as reported by query_shows"}}},
            {"trackNames", {{"type", "array"}, {"items", {{"type", "string"}}},
                            {"description", "Whole tracks to remove, with everything on them"}}}
        }}, {"required", {"showID"}}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto err = validateFields(args, {"showID", "itemIDs", "trackNames"});
            if (!err.empty()) return err;
            if (!args.contains("showID") || !args.at("showID").is_number_integer())
                return Json({{"error", "showID is required and must be an integer"}}).dump();

            Show *show = qobject_cast<Show*>(doc->function(args.at("showID").get<quint32>()));
            if (show == NULL)
                return Json({{"error", "show not found"}}).dump();

            Json results = Json::array();

            if (args.contains("itemIDs"))
            {
                if (!args.at("itemIDs").is_array())
                    return Json({{"error", "itemIDs must be an array of integers"}}).dump();
                for (auto &v : args.at("itemIDs"))
                {
                    if (!v.is_number_integer())
                    { results.push_back({{"error", "itemIDs must be an array of integers"}}); continue; }
                    const quint32 id = v.get<quint32>();

                    ShowFunction *target = NULL;
                    Track *owner = NULL;
                    for (Track *track : show->tracks())
                    {
                        if (track == NULL)
                            continue;
                        for (ShowFunction *sf : track->showFunctions())
                            if (sf != NULL && sf->id() == id)
                            { target = sf; owner = track; break; }
                        if (target != NULL)
                            break;
                    }
                    if (target == NULL)
                    { results.push_back({{"itemID", (int)id}, {"status", "not found"}}); continue; }

                    owner->removeShowFunction(target);
                    doc->setModified();
                    results.push_back({{"itemID", (int)id}, {"status", "deleted"}});
                }
            }

            if (args.contains("trackNames"))
            {
                if (!args.at("trackNames").is_array())
                    return Json({{"error", "trackNames must be an array of strings"}}).dump();
                for (auto &v : args.at("trackNames"))
                {
                    if (!v.is_string())
                    { results.push_back({{"error", "trackNames must be an array of strings"}}); continue; }
                    const QString name = QString::fromStdString(v.get<std::string>());

                    Track *track = NULL;
                    for (Track *candidate : show->tracks())
                        if (candidate != NULL && candidate->name() == name)
                        { track = candidate; break; }
                    if (track == NULL)
                    { results.push_back({{"trackName", name.toStdString()}, {"status", "not found"}}); continue; }

                    const quint32 id = track->id();
                    if (show->removeTrack(id))
                    {
                        doc->setModified();
                        results.push_back({{"trackName", name.toStdString()}, {"status", "deleted"}});
                    }
                    else
                    {
                        results.push_back({{"trackName", name.toStdString()},
                                           {"error", "could not remove track"}});
                    }
                }
            }

            return results.dump();
            });
        },
        std::nullopt,
        std::string("Remove items from a Show timeline by ShowFunction ID, and/or remove whole "
                     "tracks by name. The referenced Functions themselves are not deleted."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotDestructive));
}
