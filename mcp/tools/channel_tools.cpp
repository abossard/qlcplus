/*
  Q Light Controller Plus
  channel_tools.cpp

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
#include "qlcchannel.h"
#include "channelmodifier.h"
#include "qlcmodifierscache.h"

#include <fastmcpp/tools/manager.hpp>
#include <fastmcpp/tools/tool.hpp>

void registerChannelTools(fastmcpp::tools::ToolManager &tm, Doc *doc)
{
    using Json = nlohmann::json;
    using Tool = fastmcpp::tools::Tool;

    // query_fixture_channels — detailed per-channel info
    tm.register_tool(Tool(
        "query_fixture_channels",
        Json{{"type", "object"}, {"properties", {
            {"fixtureIDs", {{"type", "array"}, {"items", {{"type", "integer"}}}, {"description", "Fixture IDs to query (empty = all)"}}}
        }}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            QList<quint32> ids;
            if (args.contains("fixtureIDs"))
                for (auto &fid : args["fixtureIDs"])
                    ids.append(fid.get<int>());

            for (Fixture *fxi : doc->fixtures())
            {
                if (!ids.isEmpty() && !ids.contains(fxi->id()))
                    continue;

                Json fxEntry;
                fxEntry["fixtureID"] = (int)fxi->id();
                fxEntry["name"] = fxi->name().toStdString();
                Json channels = Json::array();

                QList<int> forcedHTP = fxi->forcedHTPChannels();
                QList<int> forcedLTP = fxi->forcedLTPChannels();

                for (quint32 ch = 0; ch < fxi->channels(); ch++)
                {
                    const QLCChannel *channel = fxi->channel(ch);
                    if (!channel) continue;

                    std::string precedence = "auto";
                    if (forcedHTP.contains((int)ch)) precedence = "htp";
                    else if (forcedLTP.contains((int)ch)) precedence = "ltp";

                    std::string modName = "";
                    ChannelModifier *mod = fxi->channelModifier(ch);
                    if (mod) modName = mod->name().toStdString();

                    channels.push_back({
                        {"index", (int)ch},
                        {"name", channel->name().toStdString()},
                        {"group", QLCChannel::groupToString(channel->group()).toStdString()},
                        {"colour", QLCChannel::colourToString(channel->colour()).toStdString()},
                        {"canFade", fxi->channelCanFade((int)ch)},
                        {"precedence", precedence},
                        {"modifier", modName},
                        {"defaultHTP", channel->group() == QLCChannel::Intensity}
                    });
                }
                fxEntry["channels"] = channels;
                results.push_back(fxEntry);
            }
            return results;
            });
        },
        std::nullopt,
        std::string("Query detailed per-channel info for fixtures: name, group, colour, canFade, precedence (auto/htp/ltp), modifier, defaultHTP."),
        std::nullopt
    ));

    // configure_channels — set precedence and canFade (batch)
    tm.register_tool(Tool(
        "configure_channels",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"fixtureID", {{"type", "integer"}}},
                {"channel", {{"type", "integer"}}},
                {"precedence", {{"type", "string"}, {"enum", {"auto", "htp", "ltp"}},
                    {"description", "auto=use default (Intensity→HTP, others→LTP), htp=force HTP, ltp=force LTP"}}},
                {"canFade", {{"type", "boolean"}, {"description", "Whether the channel participates in fades"}}}
            }}, {"required", {"fixtureID", "channel"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            for (auto &item : args["items"])
            {
                quint32 fxID = item["fixtureID"].get<int>();
                int ch = item["channel"].get<int>();
                Fixture *fxi = doc->fixture(fxID);
                if (!fxi) { results.push_back({{"error", "fixture not found"}}); continue; }

                if (item.contains("precedence"))
                {
                    std::string prec = item["precedence"].get<std::string>();
                    QList<int> htpList = fxi->forcedHTPChannels();
                    QList<int> ltpList = fxi->forcedLTPChannels();
                    htpList.removeAll(ch);
                    ltpList.removeAll(ch);

                    if (prec == "htp") htpList.append(ch);
                    else if (prec == "ltp") ltpList.append(ch);
                    // "auto" = removed from both lists

                    fxi->setForcedHTPChannels(htpList);
                    fxi->setForcedLTPChannels(ltpList);
                }

                if (item.contains("canFade"))
                    fxi->setChannelCanFade(ch, item["canFade"].get<bool>());

                results.push_back({{"fixtureID", (int)fxID}, {"channel", ch}, {"status", "ok"}});
            }
            return results;
            });
        },
        std::nullopt,
        std::string("Set channel precedence (auto/htp/ltp) and fade behavior per channel. auto restores default. Batch."),
        std::nullopt
    ));

    // query_channel_modifiers — list available modifier templates
    tm.register_tool(Tool(
        "query_channel_modifiers",
        Json{{"type", "object"}, {"properties", Json::object()}},
        Json{},
        [doc](const Json &) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            const QLCModifiersCache *cache = doc->modifiersCache();
            if (cache)
            {
                for (const QString &name : cache->templateNames())
                    results.push_back({{"name", name.toStdString()}});
            }
            return results;
            });
        },
        std::nullopt,
        std::string("List available channel modifier templates (Invert, Exponential, Logarithmic, Linear, etc.)"),
        std::nullopt
    ));

    // set_channel_modifiers — apply modifier templates to channels (batch)
    tm.register_tool(Tool(
        "set_channel_modifiers",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"fixtureID", {{"type", "integer"}}},
                {"channel", {{"type", "integer"}}},
                {"modifierName", {{"type", "string"}, {"description", "Modifier template name, or 'none' to remove"}}}
            }}, {"required", {"fixtureID", "channel", "modifierName"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            for (auto &item : args["items"])
            {
                quint32 fxID = item["fixtureID"].get<int>();
                int ch = item["channel"].get<int>();
                QString modName = QString::fromStdString(item["modifierName"].get<std::string>());
                Fixture *fxi = doc->fixture(fxID);
                if (!fxi) { results.push_back({{"error", "fixture not found"}}); continue; }

                if (modName == "none" || modName.isEmpty())
                {
                    fxi->setChannelModifier(ch, nullptr);
                }
                else
                {
                    const QLCModifiersCache *cache = doc->modifiersCache();
                    ChannelModifier *mod = cache ? cache->modifier(modName) : nullptr;
                    if (mod)
                        fxi->setChannelModifier(ch, mod);
                    else
                    {
                        results.push_back({{"fixtureID", (int)fxID}, {"channel", ch},
                                           {"error", "modifier not found: " + modName.toStdString()}});
                        continue;
                    }
                }
                results.push_back({{"fixtureID", (int)fxID}, {"channel", ch}, {"status", "ok"}});
            }
            return results;
            });
        },
        std::nullopt,
        std::string("Apply channel modifier templates (Invert, Exponential, etc.) to fixture channels. Use 'none' to remove. Use 'Invert' on Pan/Tilt to reverse direction. Batch."),
        std::nullopt
    ));
}
