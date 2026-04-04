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
#include "conversions.h"
#include "idempotency.h"
#include "doc.h"
#include "fixture.h"
#include "qlcchannel.h"
#include "qlcphysical.h"
#include "qlcfixturemode.h"
#include "qlcmodifierscache.h"
#include "scenevalue.h"
#include "inputoutputmap.h"
#include "universe.h"

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
            auto err = validateFields(args, {"fixtureIDs"});
            if (!err.empty()) return err;
            Json results = Json::array();
            QList<quint32> ids;
            if (args.contains("fixtureIDs"))
                for (auto &fid : args.at("fixtureIDs"))
                    ids.append(fid.get<int>());

            for (Fixture *fxi : doc->fixtures())
            {
                if (!ids.isEmpty() && !ids.contains(fxi->id()))
                    continue;

                Json fxEntry;
                fxEntry["fixtureID"] = (int)fxi->id();
                fxEntry["name"] = fxi->name().toStdString();
                Json channels = Json::array();

                for (quint32 ch = 0; ch < fxi->channels(); ch++)
                {
                    Json chJson = mcp::channelToJson(fxi, ch);
                    if (!chJson.is_null())
                        channels.push_back(chJson);
                }
                fxEntry["channels"] = channels;
                results.push_back(fxEntry);
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("Query detailed per-channel info for fixtures including capabilities with DMX value ranges, colors, gobo images, and preset types."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotReadOnly));

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
            for (auto &item : args.at("items"))
            {
                auto err = validateFields(item, {"fixtureID", "channel", "precedence", "canFade"});
                if (!err.empty()) { results.push_back(nlohmann::json::parse(err)); continue; }
                quint32 fxID = item.at("fixtureID").get<int>();
                int ch = item.at("channel").get<int>();
                Fixture *fxi = doc->fixture(fxID);
                if (!fxi) { results.push_back({{"error", "fixture not found"}}); continue; }

                if (item.contains("precedence"))
                {
                    std::string prec = item.at("precedence").get<std::string>();
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
                    fxi->setChannelCanFade(ch, item.at("canFade").get<bool>());

                results.push_back({{"fixtureID", (int)fxID}, {"channel", ch}, {"status", "ok"}});
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("Set channel precedence (auto/htp/ltp) and fade behavior per channel. auto restores default. Batch."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotIdempotent));

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
            return results.dump();
            });
        },
        std::nullopt,
        std::string("List available channel modifier templates (Invert, Exponential, Logarithmic, Linear, etc.)"),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotReadOnly));

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
            for (auto &item : args.at("items"))
            {
                auto err = validateFields(item, {"fixtureID", "channel", "modifierName"});
                if (!err.empty()) { results.push_back(nlohmann::json::parse(err)); continue; }
                quint32 fxID = item.at("fixtureID").get<int>();
                int ch = item.at("channel").get<int>();
                QString modName = QString::fromStdString(item.at("modifierName").get<std::string>());
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
            return results.dump();
            });
        },
        std::nullopt,
        std::string("Apply channel modifier templates (Invert, Exponential, etc.) to fixture channels. Use 'none' to remove. Use 'Invert' on Pan/Tilt to reverse direction. Batch."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotIdempotent));

    // convert_degrees_to_dmx — convert pan/tilt/zoom degrees to DMX channel values
    tm.register_tool(Tool(
        "convert_degrees_to_dmx",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"fixtureID", {{"type", "integer"}}},
                {"panDegrees", {{"type", "number"}, {"description", "Pan position in degrees (0 to focusPanMax)"}}},
                {"tiltDegrees", {{"type", "number"}, {"description", "Tilt position in degrees (0 to focusTiltMax)"}}},
                {"zoomDegrees", {{"type", "number"}, {"description", "Zoom/beam angle in degrees (lensDegreesMin to lensDegreesMax)"}}}
            }}, {"required", {"fixtureID"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            for (auto &item : args.at("items"))
            {
                auto err = validateFields(item, {"fixtureID", "panDegrees", "tiltDegrees", "zoomDegrees"});
                if (!err.empty()) { results.push_back(nlohmann::json::parse(err)); continue; }

                quint32 fxID = item.at("fixtureID").get<int>();
                Fixture *fxi = doc->fixture(fxID);
                if (!fxi) { results.push_back({{"error", "fixture not found"}, {"fixtureID", (int)fxID}}); continue; }

                Json channelValues = Json::array();

                if (item.contains("panDegrees"))
                {
                    float degrees = item.at("panDegrees").get<float>();
                    QList<SceneValue> vals = fxi->positionToValues(QLCChannel::Pan, degrees);
                    for (const SceneValue &sv : vals)
                        channelValues.push_back({{"channel", (int)sv.channel}, {"value", (int)sv.value}});
                }

                if (item.contains("tiltDegrees"))
                {
                    float degrees = item.at("tiltDegrees").get<float>();
                    QList<SceneValue> vals = fxi->positionToValues(QLCChannel::Tilt, degrees);
                    for (const SceneValue &sv : vals)
                        channelValues.push_back({{"channel", (int)sv.channel}, {"value", (int)sv.value}});
                }

                if (item.contains("zoomDegrees"))
                {
                    float degrees = item.at("zoomDegrees").get<float>();
                    QList<SceneValue> vals = fxi->zoomToValues(degrees, false);
                    for (const SceneValue &sv : vals)
                        channelValues.push_back({{"channel", (int)sv.channel}, {"value", (int)sv.value}});
                }

                Json entry;
                entry["fixtureID"] = (int)fxID;
                entry["channelValues"] = channelValues;

                if (fxi->fixtureMode())
                {
                    QLCPhysical phy = fxi->fixtureMode()->physical();
                    entry["panMax"] = phy.focusPanMax();
                    entry["tiltMax"] = phy.focusTiltMax();
                    entry["zoomMin"] = phy.lensDegreesMin();
                    entry["zoomMax"] = phy.lensDegreesMax();
                }

                results.push_back(entry);
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("Convert pan/tilt/zoom degrees to DMX channel values using the fixture's physical range. Returns values ready for create_scenes. Batch."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotReadOnly));

    // read_dmx_values — read current live DMX output
    tm.register_tool(Tool(
        "read_dmx_values",
        Json{{"type", "object"}, {"properties", {
            {"fixtureIDs", {{"type", "array"}, {"items", {{"type", "integer"}}},
                {"description", "Fixture IDs to read (empty = all patched)"}}},
            {"fixtureNames", {{"type", "array"}, {"items", {{"type", "string"}}},
                {"description", "Fixture name patterns (glob: * ?)"}}},
            {"channelFilter", {{"type", "string"},
                {"description", "Filter by channel group: all (default), dimmer, color, position, gobo, shutter, beam, effect"}}},
            {"nonZeroOnly", {{"type", "boolean"},
                {"description", "Only return channels with non-zero values (default false)"}}}
        }}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto err = validateFields(args, {"fixtureIDs", "fixtureNames", "channelFilter", "nonZeroOnly"});
            if (!err.empty()) return err;

            bool nonZero = args.value("nonZeroOnly", false);
            std::string filter = args.value("channelFilter", "all");

            // Map filter string to QLCChannel::Group set
            auto groupMatches = [&](QLCChannel::Group g) -> bool {
                if (filter == "all") return true;
                if (filter == "dimmer")   return g == QLCChannel::Intensity;
                if (filter == "color")    return g == QLCChannel::Colour;
                if (filter == "position") return g == QLCChannel::Pan || g == QLCChannel::Tilt;
                if (filter == "gobo")     return g == QLCChannel::Gobo;
                if (filter == "shutter")  return g == QLCChannel::Shutter;
                if (filter == "beam")     return g == QLCChannel::Beam;
                if (filter == "effect")   return g == QLCChannel::Effect;
                return true;
            };

            // Resolve fixture IDs
            QList<quint32> fixtureIDs;
            if (args.contains("fixtureNames") && args.at("fixtureNames").is_array())
            {
                for (auto &p : args.at("fixtureNames"))
                {
                    auto ids = mcp::resolveFixturesByName(doc, QString::fromStdString(p.get<std::string>()));
                    for (quint32 id : ids)
                        if (!fixtureIDs.contains(id)) fixtureIDs.append(id);
                }
            }
            if (args.contains("fixtureIDs") && args.at("fixtureIDs").is_array())
            {
                for (auto &fid : args.at("fixtureIDs"))
                {
                    quint32 id = fid.get<int>();
                    if (!fixtureIDs.contains(id)) fixtureIDs.append(id);
                }
            }
            // Default: all patched fixtures
            if (fixtureIDs.isEmpty())
            {
                for (Fixture *fxi : doc->fixtures())
                    fixtureIDs.append(fxi->id());
            }

            // Read preGM values from universes
            InputOutputMap *ioMap = doc->inputOutputMap();
            QList<Universe*> universes = ioMap->claimUniverses();

            Json fixtures = Json::array();
            for (quint32 fxID : fixtureIDs)
            {
                Fixture *fxi = doc->fixture(fxID);
                if (!fxi) continue;

                int uniIdx = fxi->universe();
                int baseAddr = fxi->address();
                if (uniIdx < 0 || uniIdx >= universes.count()) continue;

                Universe *uni = universes.at(uniIdx);
                const QByteArray preGM = uni->preGMValues();

                Json channels = Json::array();
                for (quint32 ch = 0; ch < fxi->channels(); ch++)
                {
                    const QLCChannel *qlcCh = fxi->channel(ch);
                    if (!qlcCh) continue;
                    if (!groupMatches(qlcCh->group())) continue;

                    int absAddr = baseAddr + (int)ch;
                    uchar val = (absAddr < preGM.size()) ? static_cast<uchar>(preGM.at(absAddr)) : 0;

                    if (nonZero && val == 0) continue;

                    Json chEntry;
                    chEntry["channel"] = (int)ch;
                    chEntry["name"] = qlcCh->name().toStdString();
                    chEntry["group"] = QLCChannel::groupToString(qlcCh->group()).toStdString();
                    chEntry["value"] = (int)val;

                    // Add degree conversion for position/zoom channels
                    if (qlcCh->group() == QLCChannel::Pan || qlcCh->group() == QLCChannel::Tilt)
                    {
                        QLCFixtureMode *mode = fxi->fixtureMode();
                        if (mode)
                        {
                            QLCPhysical phy = mode->physical();
                            double maxDeg = (qlcCh->group() == QLCChannel::Pan) ? phy.focusPanMax() : phy.focusTiltMax();
                            if (maxDeg > 0)
                                chEntry["degrees"] = (val / 255.0) * maxDeg;
                        }
                    }

                    channels.push_back(chEntry);
                }

                if (!channels.empty())
                {
                    fixtures.push_back({
                        {"fixtureID", (int)fxID},
                        {"fixtureName", fxi->name().toStdString()},
                        {"channels", channels}
                    });
                }
            }

            ioMap->releaseUniverses(false);

            return Json({{"fixtures", fixtures}}).dump();
            });
        },
        std::nullopt,
        std::string("Read current live DMX output values (pre-Grand Master) for fixtures. "
                     "Returns the merged result of all running functions. "
                     "Filter by channel group and optionally exclude zero values. "
                     "Position/zoom channels include degree conversion. "
                     "Use with update_scene_from_dmx to capture live state into scenes."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotReadOnly));
}
