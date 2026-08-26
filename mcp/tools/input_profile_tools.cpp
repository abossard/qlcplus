/*
  Q Light Controller Plus
  input_profile_tools.cpp

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
#include "inputoutputmap.h"
#include "qlcinputprofile.h"
#include "qlcinputchannel.h"
#include "inputpatch.h"

#include <QCryptographicHash>
#include <QDir>
#include <QRegularExpression>
#include <QFile>
#include <QFileInfo>

#include <fastmcpp/tools/manager.hpp>
#include <fastmcpp/tools/tool.hpp>

namespace {

using Json = nlohmann::json;

QLCInputChannel::Type channelTypeFromString(const std::string &value, bool &ok)
{
    static const QMap<std::string, QLCInputChannel::Type> map = {
        {"slider",   QLCInputChannel::Slider},
        {"knob",     QLCInputChannel::Knob},
        {"encoder",  QLCInputChannel::Encoder},
        {"button",   QLCInputChannel::Button},
        {"nextpage", QLCInputChannel::NextPage},
        {"prevpage", QLCInputChannel::PrevPage},
        {"pageset",  QLCInputChannel::PageSet}
    };
    auto it = map.find(toLowerStd(value));
    ok = it != map.end();
    return ok ? it.value() : QLCInputChannel::NoType;
}

QLCInputProfile::Type profileTypeFromString(const std::string &value, bool &ok)
{
    static const QMap<std::string, QLCInputProfile::Type> map = {
        {"midi",   QLCInputProfile::MIDI},
        {"os2l",   QLCInputProfile::OS2L},
        {"osc",    QLCInputProfile::OSC},
        {"hid",    QLCInputProfile::HID},
        {"dmx",    QLCInputProfile::DMX},
        {"enttec", QLCInputProfile::Enttec}
    };
    auto it = map.find(toLowerStd(value));
    ok = it != map.end();
    return ok ? it.value() : QLCInputProfile::MIDI;
}

/** Profiles live in one .qxi per profile, named after manufacturer and model. */
QString profileFilePath(const QDir &dir, const QLCInputProfile *profile)
{
    // "Acme-X"/"Y" and "Acme"/"X-Y" would otherwise both sanitise to
    // Acme-X-Y.qxi and silently overwrite each other.
    const QString key = profile->manufacturer() + "\x1f" + profile->model();
    QString base = profile->manufacturer() + "-" + profile->model();
    base.replace(QRegularExpression("[^a-zA-Z0-9._-]"), "_");
    const QByteArray digest =
        QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha1).toHex().left(8);
    return dir.absoluteFilePath(base + "-" + QString::fromLatin1(digest) + ".qxi");
}

}

void registerInputProfileTools(fastmcpp::tools::ToolManager &tm, Doc *doc)
{
    using Tool = fastmcpp::tools::Tool;

    // create_input_profiles — author a controller mapping from scratch
    tm.register_tool(Tool(
        "create_input_profiles",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"manufacturer", {{"type", "string"}}},
                {"model", {{"type", "string"}}},
                {"type", {{"type", "string"}, {"enum", {"MIDI", "OS2L", "OSC", "HID", "DMX", "Enttec"}},
                          {"description", "Transport this profile describes. Default MIDI."}}},
                {"channels", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                    {"number", {{"type", "integer"}, {"minimum", 1},
                                {"description", "1-based channel number as shown in the UI"}}},
                    {"name", {{"type", "string"}}},
                    {"type", {{"type", "string"},
                              {"enum", {"Slider", "Knob", "Encoder", "Button", "NextPage", "PrevPage", "PageSet"}}}},
                    {"movement", {{"type", "string"}, {"enum", {"absolute", "relative"}},
                                  {"description", "Slider/Knob movement mode. Default absolute."}}},
                    {"sensitivity", {{"type", "integer"}, {"minimum", 1},
                                     {"description", "Relative movement sensitivity"}}}
                }}, {"required", {"number", "name", "type"}}}}}}
            }}, {"required", {"manufacturer", "model", "channels"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto argsErr = validateFields(args, {"items"});
            if (!argsErr.empty()) return argsErr;
            auto itemsErr = validateItemsArray(args);
            if (itemsErr) return *itemsErr;

            InputOutputMap *ioMap = doc->inputOutputMap();
            QDir userDir = InputOutputMap::userProfileDirectory();
            if (!userDir.exists() && !userDir.mkpath("."))
                return Json({{"error", "could not create the user input-profile directory"},
                             {"path", userDir.absolutePath().toStdString()}}).dump();

            Json results = Json::array();
            for (auto &item : args.at("items"))
            {
                auto err = validateFields(item, {"manufacturer", "model", "type", "channels"});
                if (!err.empty()) { results.push_back(Json::parse(err)); continue; }

                if (!item.contains("manufacturer") || !item.contains("model") ||
                    !item.at("manufacturer").is_string() || !item.at("model").is_string())
                { results.push_back({{"error", "manufacturer and model must be strings"}}); continue; }

                const QString manufacturer =
                    QString::fromStdString(item.at("manufacturer").get<std::string>()).trimmed();
                const QString model =
                    QString::fromStdString(item.at("model").get<std::string>()).trimmed();
                if (manufacturer.isEmpty() || model.isEmpty())
                { results.push_back({{"error", "manufacturer and model must not be empty"}}); continue; }

                QLCInputProfile::Type profileType = QLCInputProfile::MIDI;
                if (item.contains("type"))
                {
                    if (!item.at("type").is_string())
                    { results.push_back({{"error", "type must be a string"}}); continue; }
                    bool ok = false;
                    profileType = profileTypeFromString(item.at("type").get<std::string>(), ok);
                    if (!ok)
                    { results.push_back({{"error", "unknown profile type"},
                                         {"type", item.at("type")}}); continue; }
                }

                if (!item.contains("channels") || !item.at("channels").is_array() ||
                    item.at("channels").empty())
                { results.push_back({{"error", "channels must be a non-empty array"}}); continue; }

                // Build the whole profile before touching the map or the disk, so
                // a bad channel cannot leave a partial profile registered.
                QLCInputProfile *profile = new QLCInputProfile();
                profile->setManufacturer(manufacturer);
                profile->setModel(model);
                profile->setType(profileType);

                std::optional<Json> failure;
                for (auto &channel : item.at("channels"))
                {
                    auto channelErr = validateFields(channel, {"number", "name", "type",
                                                               "movement", "sensitivity"});
                    if (!channelErr.empty()) { failure = Json::parse(channelErr); break; }

                    if (!channel.contains("number") || !channel.at("number").is_number_integer() ||
                        channel.at("number").get<int>() < 1)
                    { failure = Json({{"error", "channel number must be an integer of at least 1"}}); break; }
                    if (!channel.contains("name") || !channel.contains("type") ||
                        !channel.at("name").is_string() || !channel.at("type").is_string())
                    { failure = Json({{"error", "channel name and type must be strings"}}); break; }

                    bool ok = false;
                    const QLCInputChannel::Type type =
                        channelTypeFromString(channel.at("type").get<std::string>(), ok);
                    if (!ok)
                    { failure = Json({{"error", "unknown channel type"}, {"type", channel.at("type")}}); break; }

                    // The UI numbers channels from 1; the profile stores them from 0.
                    const quint32 number = (quint32)channel.at("number").get<int>() - 1;
                    if (profile->channel(number) != NULL)
                    { failure = Json({{"error", "duplicate channel number"},
                                      {"number", channel.at("number")}}); break; }

                    QLCInputChannel *ich = new QLCInputChannel();
                    ich->setName(QString::fromStdString(channel.at("name").get<std::string>()));
                    ich->setType(type);

                    if (channel.contains("movement"))
                    {
                        if (!channel.at("movement").is_string())
                        { delete ich; failure = Json({{"error", "movement must be a string"}}); break; }
                        const std::string movement =
                            toLowerStd(channel.at("movement").get<std::string>());
                        if (movement != "absolute" && movement != "relative")
                        {
                            delete ich;
                            failure = Json({{"error", "movement must be \"absolute\" or \"relative\""},
                                            {"movement", channel.at("movement")}});
                            break;
                        }
                        ich->setMovementType(movement == "relative" ? QLCInputChannel::Relative
                                                                    : QLCInputChannel::Absolute);
                    }
                    if (channel.contains("sensitivity"))
                    {
                        if (!channel.at("sensitivity").is_number_integer() ||
                            channel.at("sensitivity").get<int>() < 1)
                        { delete ich; failure = Json({{"error", "sensitivity must be an integer of at least 1"}}); break; }
                        ich->setMovementSensitivity(channel.at("sensitivity").get<int>());
                    }

                    profile->insertChannel(number, ich);
                }

                if (failure)
                {
                    delete profile;
                    Json entry = *failure;
                    entry["manufacturer"] = manufacturer.toStdString();
                    entry["model"] = model.toStdString();
                    results.push_back(entry);
                    continue;
                }

                const QString path = profileFilePath(userDir, profile);
                if (!profile->saveXML(path))
                {
                    delete profile;
                    results.push_back({{"manufacturer", manufacturer.toStdString()},
                                       {"model", model.toStdString()},
                                       {"error", "could not write the profile file"},
                                       {"path", path.toStdString()}});
                    continue;
                }

                // Replace any same-named profile in the map too. Writing only the
                // file would leave set_input_profile and query_input_profile_channels
                // handing out the stale channel map until the next restart.
                const QString profileName = profile->name();
                const bool existed = ioMap->profile(profileName) != NULL;
                if (existed)
                {
                    // removeProfile deletes the old object, so refuse while a
                    // universe still has it patched — that patch holds a pointer.
                    int patchedUniverse = -1;
                    for (quint32 uni = 0; uni < ioMap->universesCount(); uni++)
                    {
                        InputPatch *patch = ioMap->inputPatch(uni);
                        if (patch != NULL && patch->profile() != NULL &&
                            patch->profile()->name() == profileName)
                        { patchedUniverse = (int)uni; break; }
                    }
                    if (patchedUniverse >= 0)
                    {
                        delete profile;
                        results.push_back({{"manufacturer", manufacturer.toStdString()},
                                           {"model", model.toStdString()},
                                           {"path", path.toStdString()},
                                           {"error", "profile file was written, but it is currently "
                                                     "patched to a universe and cannot be swapped in "
                                                     "memory — unpatch it first, or restart QLC+"},
                                           {"universeID", patchedUniverse}});
                        continue;
                    }
                    ioMap->removeProfile(profileName);
                }

                if (!ioMap->addProfile(profile))
                {
                    delete profile;
                    results.push_back({{"manufacturer", manufacturer.toStdString()},
                                       {"model", model.toStdString()},
                                       {"error", "could not register the profile"}});
                    continue;
                }

                results.push_back({{"name", (manufacturer + " " + model).toStdString()},
                                   {"manufacturer", manufacturer.toStdString()},
                                   {"model", model.toStdString()},
                                   {"path", path.toStdString()},
                                   {"channels", (int)item.at("channels").size()},
                                   {"status", existed ? "updated" : "created"}});
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("Create or update input profiles (.qxi) describing a control surface's "
                     "channels, written into the user profile directory and registered for "
                     "set_input_profile. Upserts by manufacturer+model. Channel numbers are "
                     "1-based, matching the UI. Batch: {\"items\": [...]}."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotIdempotent));

    // query_input_profile_channels — read one profile's channel map
    tm.register_tool(Tool(
        "query_input_profile_channels",
        Json{{"type", "object"}, {"properties", {
            {"profileName", {{"type", "string"},
                             {"description", "Profile name as reported by query_input_profiles"}}}
        }}, {"required", {"profileName"}}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto err = validateFields(args, {"profileName"});
            if (!err.empty()) return err;
            if (!args.contains("profileName") || !args.at("profileName").is_string())
                return Json({{"error", "profileName is required and must be a string"}}).dump();

            const QString name = QString::fromStdString(args.at("profileName").get<std::string>());
            QLCInputProfile *profile = doc->inputOutputMap()->profile(name);
            if (profile == NULL)
                return Json({{"error", "profile not found"}, {"profileName", name.toStdString()}}).dump();

            Json channels = Json::array();
            QMapIterator<quint32, QLCInputChannel*> it(profile->channels());
            while (it.hasNext())
            {
                it.next();
                QLCInputChannel *ich = it.value();
                if (ich == NULL)
                    continue;
                channels.push_back({
                    {"number", (int)it.key() + 1},
                    {"name", ich->name().toStdString()},
                    {"type", QLCInputChannel::typeToString(ich->type()).toStdString()}
                });
            }

            return Json({
                {"name", profile->name().toStdString()},
                {"manufacturer", profile->manufacturer().toStdString()},
                {"model", profile->model().toStdString()},
                {"type", QLCInputProfile::typeToString(profile->type()).toStdString()},
                {"channels", channels}
            }).dump();
            });
        },
        std::nullopt,
        std::string("Read one input profile's channel map — 1-based channel numbers, names and "
                     "types. Use query_input_profiles for the list of profile names."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotReadOnly));
}
