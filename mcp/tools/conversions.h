/*
  Q Light Controller Plus
  conversions.h

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

#ifndef MCP_CONVERSIONS_H
#define MCP_CONVERSIONS_H

#include <nlohmann/json.hpp>

#include "fixture.h"
#include "function.h"
#include "qlcchannel.h"
#include "qlcfixturedef.h"
#include "channelmodifier.h"

namespace mcp {

using Json = nlohmann::json;

// Pure function: Extract fixture capabilities as JSON array
inline Json fixtureCapabilities(const Fixture *fxi)
{
    Json caps = Json::array();
    bool hasPan = false, hasTilt = false;
    bool hasR = false, hasG = false, hasB = false;
    bool hasC = false, hasM = false, hasY = false;

    for (quint32 ch = 0; ch < fxi->channels(); ch++)
    {
        const QLCChannel *channel = fxi->channel(ch);
        if (!channel) continue;
        switch (channel->group())
        {
            case QLCChannel::Pan: hasPan = true; break;
            case QLCChannel::Tilt: hasTilt = true; break;
            case QLCChannel::Colour: caps.push_back("Colour"); break;
            case QLCChannel::Gobo: caps.push_back("Gobo"); break;
            case QLCChannel::Shutter: caps.push_back("Shutter"); break;
            case QLCChannel::Intensity:
                switch (channel->colour())
                {
                    case QLCChannel::Red: hasR = true; break;
                    case QLCChannel::Green: hasG = true; break;
                    case QLCChannel::Blue: hasB = true; break;
                    case QLCChannel::Cyan: hasC = true; break;
                    case QLCChannel::Magenta: hasM = true; break;
                    case QLCChannel::Yellow: hasY = true; break;
                    default: break;
                }
                break;
            default: break;
        }
    }
    if (hasPan && hasTilt) caps.push_back("Pan/Tilt");
    if (hasR && hasG && hasB) caps.push_back("RGB");
    if (hasC && hasM && hasY) caps.push_back("CMY");
    return caps;
}

// Pure function: Convert a fixture to JSON summary
inline Json fixtureToJson(const Fixture *fxi)
{
    Json entry;
    entry["id"] = fxi->id();
    entry["name"] = fxi->name().toStdString();
    entry["channels"] = fxi->channels();
    entry["heads"] = fxi->heads();
    if (fxi->fixtureDef())
    {
        entry["manufacturer"] = fxi->fixtureDef()->manufacturer().toStdString();
        entry["model"] = fxi->fixtureDef()->model().toStdString();
    }
    entry["capabilities"] = fixtureCapabilities(fxi);
    return entry;
}

// Pure function: Convert a function to JSON summary
inline Json functionToJson(Function *fn)
{
    return {
        {"id", (int)fn->id()},
        {"name", fn->name().toStdString()},
        {"type", Function::typeToString(fn->type()).toStdString()},
        {"duration", (int)fn->totalDuration()}
    };
}

// Pure function: Convert a channel to JSON with precedence info
// Uses non-const Fixture* because forcedHTPChannels/forcedLTPChannels/channelModifier are non-const
inline Json channelToJson(Fixture *fxi, quint32 chIndex)
{
    const QLCChannel *channel = fxi->channel(chIndex);
    if (!channel) return Json();

    QList<int> forcedHTP = fxi->forcedHTPChannels();
    QList<int> forcedLTP = fxi->forcedLTPChannels();

    std::string precedence = "auto";
    if (forcedHTP.contains((int)chIndex)) precedence = "htp";
    else if (forcedLTP.contains((int)chIndex)) precedence = "ltp";

    std::string modName = "";
    ChannelModifier *mod = fxi->channelModifier(chIndex);
    if (mod) modName = mod->name().toStdString();

    return {
        {"index", (int)chIndex},
        {"name", channel->name().toStdString()},
        {"group", QLCChannel::groupToString(channel->group()).toStdString()},
        {"colour", QLCChannel::colourToString(channel->colour()).toStdString()},
        {"canFade", fxi->channelCanFade((int)chIndex)},
        {"precedence", precedence},
        {"modifier", modName},
        {"defaultHTP", channel->group() == QLCChannel::Intensity}
    };
}

} // namespace mcp
#endif
