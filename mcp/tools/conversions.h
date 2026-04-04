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
#include "qlccapability.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcphysical.h"
#include "qlcpalette.h"
#include "channelmodifier.h"
#include "scene.h"
#include "chaser.h"
#include "collection.h"
#include "scenevalue.h"
#include "doc.h"

namespace mcp {

using Json = nlohmann::json;

// Pure function: Convert a QLCCapability to JSON
inline Json capabilityToJson(const QLCCapability *cap)
{
    Json entry;
    entry["min"] = (int)cap->min();
    entry["max"] = (int)cap->max();
    entry["name"] = cap->name().toStdString();
    QString presetStr = QLCCapability::presetToString(cap->preset());
    if (!presetStr.isEmpty() && cap->preset() != QLCCapability::Custom)
        entry["preset"] = presetStr.toStdString();

    switch (cap->presetType())
    {
        case QLCCapability::SingleColor:
        {
            QVariant res = cap->resource(0);
            if (res.isValid())
                entry["color1"] = res.value<QColor>().name().toStdString();
            break;
        }
        case QLCCapability::DoubleColor:
        {
            QVariant res0 = cap->resource(0);
            QVariant res1 = cap->resource(1);
            if (res0.isValid())
                entry["color1"] = res0.value<QColor>().name().toStdString();
            if (res1.isValid())
                entry["color2"] = res1.value<QColor>().name().toStdString();
            break;
        }
        case QLCCapability::Picture:
        {
            QVariant res = cap->resource(0);
            if (res.isValid())
                entry["image"] = res.toString().toStdString();
            break;
        }
        case QLCCapability::SingleValue:
        {
            QVariant res = cap->resource(0);
            if (res.isValid())
                entry["value"] = res.toFloat();
            QString units = cap->presetUnits();
            if (!units.isEmpty())
                entry["unit"] = units.toStdString();
            break;
        }
        case QLCCapability::DoubleValue:
        {
            QVariant res0 = cap->resource(0);
            QVariant res1 = cap->resource(1);
            if (res0.isValid())
                entry["valueMin"] = res0.toFloat();
            if (res1.isValid())
                entry["valueMax"] = res1.toFloat();
            QString units = cap->presetUnits();
            if (!units.isEmpty())
                entry["unit"] = units.toStdString();
            break;
        }
        default:
            break;
    }
    return entry;
}

// Pure function: Convert QLCPhysical to JSON (omits default/empty values)
inline Json physicalToJson(const QLCPhysical &phy)
{
    Json entry;
    if (phy.weight() > 0)
        entry["weight"] = phy.weight();
    if (phy.width() > 0)
        entry["width"] = phy.width();
    if (phy.height() > 0)
        entry["height"] = phy.height();
    if (phy.depth() > 0)
        entry["depth"] = phy.depth();
    if (!phy.bulbType().isEmpty())
        entry["bulbType"] = phy.bulbType().toStdString();
    if (phy.bulbLumens() > 0)
        entry["bulbLumens"] = phy.bulbLumens();
    if (phy.bulbColourTemperature() > 0)
        entry["bulbColourTemperature"] = phy.bulbColourTemperature();
    if (!phy.lensName().isEmpty())
        entry["lensName"] = phy.lensName().toStdString();
    if (phy.lensDegreesMin() > 0)
        entry["lensDegreesMin"] = phy.lensDegreesMin();
    if (phy.lensDegreesMax() > 0)
        entry["lensDegreesMax"] = phy.lensDegreesMax();
    if (!phy.focusType().isEmpty())
        entry["focusType"] = phy.focusType().toStdString();
    if (phy.focusPanMax() > 0)
        entry["focusPanMax"] = phy.focusPanMax();
    if (phy.focusTiltMax() > 0)
        entry["focusTiltMax"] = phy.focusTiltMax();
    if (phy.powerConsumption() > 0)
        entry["powerConsumption"] = phy.powerConsumption();
    if (!phy.dmxConnector().isEmpty())
        entry["dmxConnector"] = phy.dmxConnector().toStdString();
    return entry;
}

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
    entry["universe"] = (int)fxi->universe();
    entry["address"] = (int)fxi->address();
    entry["channels"] = fxi->channels();
    entry["heads"] = fxi->heads();
    if (fxi->fixtureDef())
    {
        entry["manufacturer"] = fxi->fixtureDef()->manufacturer().toStdString();
        entry["model"] = fxi->fixtureDef()->model().toStdString();
    }
    if (fxi->fixtureMode())
        entry["mode"] = fxi->fixtureMode()->name().toStdString();
    entry["capabilities"] = fixtureCapabilities(fxi);

    if (fxi->fixtureMode())
    {
        QLCPhysical phy = fxi->fixtureMode()->physical();
        Json phyJson = physicalToJson(phy);
        if (!phyJson.empty())
            entry["physical"] = phyJson;
    }

    return entry;
}

// Pure function: Convert a function to JSON summary
inline Json functionToJson(Function *fn)
{
    Json entry = {
        {"id", (int)fn->id()},
        {"name", fn->name().toStdString()},
        {"type", Function::typeToString(fn->type()).toStdString()},
        {"duration", (int)fn->totalDuration()}
    };

    if (fn->type() == Function::SceneType)
    {
        Scene *scene = qobject_cast<Scene*>(fn);
        if (scene)
        {
            QSet<quint32> fxIds;
            for (const SceneValue &sv : scene->values())
                fxIds.insert(sv.fxi);
            entry["fixtureCount"] = fxIds.size();
            entry["channelCount"] = scene->values().size();

            // Palette references
            if (!scene->palettes().isEmpty())
            {
                Json palRefs = Json::array();
                Doc *doc = qobject_cast<Doc*>(fn->parent());
                for (quint32 palId : scene->palettes())
                {
                    Json ref = {{"id", (int)palId}};
                    if (doc)
                    {
                        QLCPalette *pal = doc->palette(palId);
                        if (pal)
                        {
                            ref["name"] = pal->name().toStdString();
                            ref["type"] = QLCPalette::typeToString(pal->type()).toStdString();
                        }
                    }
                    palRefs.push_back(ref);
                }
                entry["paletteRefs"] = palRefs;
            }
        }
    }
    else if (fn->type() == Function::ChaserType)
    {
        Chaser *chaser = qobject_cast<Chaser*>(fn);
        if (chaser)
        {
            entry["stepCount"] = chaser->stepsCount();
            entry["runOrder"] = Function::runOrderToString(chaser->runOrder()).toStdString();
            entry["direction"] = Function::directionToString(chaser->direction()).toStdString();
            entry["tempoType"] = chaser->tempoType() == Function::Beats ? "beats" : "time";
        }
    }
    else if (fn->type() == Function::CollectionType)
    {
        Collection *col = qobject_cast<Collection*>(fn);
        if (col)
        {
            Json ids = Json::array();
            for (quint32 fid : col->functions())
                ids.push_back((int)fid);
            entry["functionIDs"] = ids;
        }
    }

    return entry;
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
        {"preset", QLCChannel::presetToString(channel->preset()).toStdString()},
        {"canFade", fxi->channelCanFade((int)chIndex)},
        {"precedence", precedence},
        {"modifier", modName},
        {"defaultHTP", channel->group() == QLCChannel::Intensity},
        {"capabilities", [&]() {
            Json caps = Json::array();
            for (const QLCCapability *cap : channel->capabilities())
                caps.push_back(capabilityToJson(cap));
            return caps;
        }()}
    };
}

} // namespace mcp
#endif
