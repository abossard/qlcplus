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
#include "qlcfixturehead.h"
#include "qlcphysical.h"
#include "qlcpalette.h"
#include "channelmodifier.h"
#include "scene.h"
#include "chaser.h"
#include "collection.h"
#include "scenevalue.h"
#include "rgbmatrix.h"
#include "rgbalgorithm.h"
#include "fixturegroup.h"
#include "universe.h"
#include "doc.h"

namespace mcp {

using Json = nlohmann::json;

// Beat string to internal value conversion.
// QLC+ encodes beats as: 1 beat = 1000, with 1/16 grid quantization.
// Accepts: "1/16"=63, "1/8"=125, "1/4"=250, "3/8"=375, "1/2"=500, "1"=1000, "2"=2000, etc.
// Returns 0 on parse failure.
inline uint beatStringToValue(const std::string &str)
{
    if (str.empty()) return 0;

    try
    {
        // Try fraction format "N/D"
        size_t slash = str.find('/');
        if (slash != std::string::npos)
        {
            int num = std::stoi(str.substr(0, slash));
            int den = std::stoi(str.substr(slash + 1));
            if (den == 0 || num <= 0) return 0;

            // If denominator is a supported subdivision, use the engine helper directly.
            if (den == 1 || den == 2 || den == 4 || den == 8 || den == 16)
                return Function::musicalBeatValue(num, den);

            // Generic fallback: convert to beats then quantize via timeToBeats with 1000ms beat.
            double beats = (double)num / (double)den;
            uint raw = static_cast<uint>(beats * 1000.0);
            return Function::timeToBeats(raw, 1000);
        }

        // Plain number (integer or decimal beats)
        double beats = std::stod(str);
        if (beats <= 0) return 0;
        uint raw = static_cast<uint>(beats * 1000.0);
        // Quantize via the engine's 1/16 table (1000ms-per-beat reference frame).
        return Function::timeToBeats(raw, 1000);
    }
    catch (...)
    {
        return 0;
    }
}

// Internal beat value to human-readable string. Uses Function::beatValueToMusical
// to decompose into count x subdivision on the 1/16 grid.
inline std::string valueToBeatString(uint val)
{
    if (val == 0) return "0";

    QPair<int, int> m = Function::beatValueToMusical(val);
    int count = m.first;
    int subdiv = m.second;

    if (count > 0 && subdiv > 0)
    {
        // Whole beats: "1", "2", "4", ...
        if (subdiv == 1)
            return std::to_string(count);

        // Reduce common factors so e.g. 2/4 -> 1/2, 4/8 -> 1/2, 2/16 -> 1/8.
        int n = count;
        int d = subdiv;
        auto gcd = [](int a, int b) { while (b) { int t = b; b = a % b; a = t; } return a; };
        int g = gcd(n, d);
        n /= g;
        d /= g;

        if (d == 1)
            return std::to_string(n);

        return std::to_string(n) + "/" + std::to_string(d);
    }

    // Off-grid: split into whole + fractional remainder.
    uint whole = val / 1000;
    uint frac = val % 1000;
    QPair<int, int> mf = Function::beatValueToMusical(frac);
    std::string fracStr;
    if (mf.first > 0 && mf.second > 0 && mf.second != 1)
    {
        int n = mf.first;
        int d = mf.second;
        auto gcd = [](int a, int b) { while (b) { int t = b; b = a % b; a = t; } return a; };
        int g = gcd(n, d);
        n /= g;
        d /= g;
        fracStr = std::to_string(n) + "/" + std::to_string(d);
    }
    else
    {
        fracStr = std::to_string(frac);
    }
    if (whole == 0)
        return fracStr;
    return std::to_string(whole) + "+" + fracStr;
}

// Parse a duration field that can be either integer (ms) or string (beat fraction).
// Sets isBeat to true if a beat string was parsed.
inline uint parseDurationField(const Json &val, bool &isBeat)
{
    if (val.is_string())
    {
        isBeat = true;
        return beatStringToValue(val.get<std::string>());
    }
    return val.get<uint>();
}

// Convert RGBAlgorithm::Type to string
inline std::string rgbAlgorithmTypeToString(RGBAlgorithm::Type type)
{
    switch (type)
    {
        case RGBAlgorithm::Text:   return "Text";
        case RGBAlgorithm::Script: return "Script";
        case RGBAlgorithm::Image:  return "Image";
        case RGBAlgorithm::Audio:  return "Audio";
        case RGBAlgorithm::Plain:  return "Plain";
        default:                   return "Unknown";
    }
}

// Convert an RGBMatrix to detailed JSON
inline Json rgbMatrixToJson(RGBMatrix *matrix)
{
    Json entry;
    entry["id"] = (int)matrix->id();
    entry["name"] = matrix->name().toStdString();
    if (!matrix->path().isEmpty())
        entry["path"] = matrix->path().toStdString();
    entry["fixtureGroupID"] = (int)matrix->fixtureGroup();

    bool isBeatMode = (matrix->tempoType() == Function::Beats);
    entry["tempoType"] = isBeatMode ? "Beats" : "Time";

    if (isBeatMode)
    {
        entry["duration"] = valueToBeatString(matrix->duration());
        entry["fadeIn"] = valueToBeatString(matrix->fadeInSpeed());
        entry["fadeOut"] = valueToBeatString(matrix->fadeOutSpeed());
    }
    else
    {
        entry["duration"] = (int)matrix->duration();
        entry["fadeIn"] = (int)matrix->fadeInSpeed();
        entry["fadeOut"] = (int)matrix->fadeOutSpeed();
    }

    entry["runOrder"] = Function::runOrderToString(matrix->runOrder()).toStdString();
    entry["direction"] = Function::directionToString(matrix->direction()).toStdString();
    entry["controlMode"] = RGBMatrix::controlModeToString(matrix->controlMode()).toStdString();
    entry["blendMode"] = Universe::blendModeToString(matrix->blendMode()).toStdString();

    // Algorithm
    RGBAlgorithm *algo = matrix->algorithm();
    if (algo)
    {
        Json algoJson;
        algoJson["name"] = algo->name().toStdString();
        algoJson["type"] = mcp::rgbAlgorithmTypeToString(algo->type());
        algoJson["acceptColors"] = algo->acceptColors();
        entry["algorithm"] = algoJson;
    }

    // Colors
    QVector<QColor> colors = matrix->getColors();
    Json colorsJson = Json::array();
    for (const QColor &c : colors)
    {
        if (c.isValid())
            colorsJson.push_back(c.name().toStdString());
    }
    entry["colors"] = colorsJson;

    entry["stepsCount"] = matrix->stepsCount();

    // Rotation & Mirror
    if (matrix->rotation() != 0)
        entry["rotation"] = matrix->rotation() * 90; // 0, 90, 180, 270
    if (matrix->mirror() != 0)
    {
        static const char *mirrorNames[] = {"Off", "Horizontal", "Vertical", "Both"};
        entry["mirror"] = mirrorNames[matrix->mirror() & 3];
    }
    if (matrix->mirrorBlend() != RGBMatrix::MirrorFlip)
        entry["mirrorBlend"] = RGBMatrix::mirrorBlendToString(matrix->mirrorBlend()).toStdString();

    return entry;
}

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

// Pure function: Extract fixture capabilities as JSON array (deduped)
inline Json fixtureCapabilities(const Fixture *fxi)
{
    QSet<QString> seen;
    Json caps = Json::array();
    bool hasPan = false, hasTilt = false;
    bool hasR = false, hasG = false, hasB = false;
    bool hasC = false, hasM = false, hasY = false;
    bool hasW = false, hasA = false, hasUV = false;
    bool hasContinuousPan = false, hasContinuousTilt = false;

    auto addOnce = [&](const QString &cap) {
        if (!seen.contains(cap)) { seen.insert(cap); caps.push_back(cap.toStdString()); }
    };

    for (quint32 ch = 0; ch < fxi->channels(); ch++)
    {
        const QLCChannel *channel = fxi->channel(ch);
        if (!channel) continue;
        switch (channel->group())
        {
            case QLCChannel::Pan:
                hasPan = true;
                if (!hasContinuousPan)
                {
                    for (const QLCCapability *cap : channel->capabilities())
                    {
                        auto p = cap->preset();
                        if (p >= QLCCapability::RotationClockwise && p <= QLCCapability::RotationCounterClockwiseFastToSlow)
                            { hasContinuousPan = true; break; }
                    }
                }
                break;
            case QLCChannel::Tilt:
                hasTilt = true;
                if (!hasContinuousTilt)
                {
                    for (const QLCCapability *cap : channel->capabilities())
                    {
                        auto p = cap->preset();
                        if (p >= QLCCapability::RotationClockwise && p <= QLCCapability::RotationCounterClockwiseFastToSlow)
                            { hasContinuousTilt = true; break; }
                    }
                }
                break;
            case QLCChannel::Colour: addOnce("Colour"); break;
            case QLCChannel::Gobo: addOnce("Gobo"); break;
            case QLCChannel::Shutter: addOnce("Shutter"); break;
            case QLCChannel::Beam: addOnce("Beam"); break;
            case QLCChannel::Prism: addOnce("Prism"); break;
            case QLCChannel::Intensity:
                switch (channel->colour())
                {
                    case QLCChannel::Red: hasR = true; break;
                    case QLCChannel::Green: hasG = true; break;
                    case QLCChannel::Blue: hasB = true; break;
                    case QLCChannel::Cyan: hasC = true; break;
                    case QLCChannel::Magenta: hasM = true; break;
                    case QLCChannel::Yellow: hasY = true; break;
                    case QLCChannel::White: hasW = true; break;
                    case QLCChannel::Amber: hasA = true; break;
                    case QLCChannel::UV: hasUV = true; break;
                    default: break;
                }
                break;
            default: break;
        }
    }
    if (hasPan && hasTilt) caps.push_back("Pan/Tilt");
    if (hasR && hasG && hasB)
    {
        if (hasW) caps.push_back("RGBW");
        else caps.push_back("RGB");
    }
    if (hasC && hasM && hasY) caps.push_back("CMY");
    if (hasA) caps.push_back("Amber");
    if (hasUV) caps.push_back("UV");
    if (hasContinuousPan) caps.push_back("ContinuousPanRotation");
    if (hasContinuousTilt) caps.push_back("ContinuousTiltRotation");
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
        entry["type"] = QLCFixtureDef::typeToString(fxi->fixtureDef()->type()).toStdString();
    }
    if (fxi->fixtureMode())
    {
        entry["mode"] = fxi->fixtureMode()->name().toStdString();

        // Per-head channel mapping
        const auto &modeHeads = fxi->fixtureMode()->heads();
        if (!modeHeads.isEmpty())
        {
            Json headMap = Json::array();
            for (int h = 0; h < modeHeads.size(); h++)
            {
                const QLCFixtureHead &head = modeHeads[h];
                Json hEntry;
                hEntry["index"] = h;
                Json chList = Json::array();
                for (quint32 c : head.channels())
                    chList.push_back((int)c);
                hEntry["channels"] = chList;

                QVector<quint32> rgb = fxi->rgbChannels(h);
                if (rgb.size() == 3)
                {
                    hEntry["rgbChannels"] = Json::array({(int)rgb[0], (int)rgb[1], (int)rgb[2]});
                }
                QVector<quint32> cmy = fxi->cmyChannels(h);
                if (cmy.size() == 3)
                {
                    hEntry["cmyChannels"] = Json::array({(int)cmy[0], (int)cmy[1], (int)cmy[2]});
                }
                headMap.push_back(hEntry);
            }
            entry["headMap"] = headMap;
        }
    }
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

    QString fnPath = fn->path(true);
    if (!fnPath.isEmpty())
        entry["path"] = fnPath.toStdString();

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
    else if (fn->type() == Function::RGBMatrixType)
    {
        RGBMatrix *matrix = qobject_cast<RGBMatrix*>(fn);
        if (matrix)
        {
            entry["fixtureGroupID"] = (int)matrix->fixtureGroup();
            entry["tempoType"] = matrix->tempoType() == Function::Beats ? "Beats" : "Time";
            entry["controlMode"] = RGBMatrix::controlModeToString(matrix->controlMode()).toStdString();
            if (matrix->blendMode() != Universe::NormalBlend)
                entry["blendMode"] = Universe::blendModeToString(matrix->blendMode()).toStdString();
            RGBAlgorithm *algo = matrix->algorithm();
            if (algo)
                entry["algorithm"] = algo->name().toStdString();
            QVector<QColor> colors = matrix->getColors();
            Json colorsJson = Json::array();
            for (const QColor &c : colors)
                if (c.isValid()) colorsJson.push_back(c.name().toStdString());
            if (!colorsJson.empty())
                entry["colors"] = colorsJson;
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

    Json entry = {
        {"index", (int)chIndex},
        {"name", channel->name().toStdString()},
        {"group", QLCChannel::groupToString(channel->group()).toStdString()},
        {"colour", QLCChannel::colourToString(channel->colour()).toStdString()},
        {"preset", QLCChannel::presetToString(channel->preset()).toStdString()},
        {"controlByte", channel->controlByte() == QLCChannel::MSB ? "coarse" : "fine"},
        {"defaultValue", (int)channel->defaultValue()},
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

    // Head index for this channel (-1 means not in any head)
    if (fxi->fixtureMode())
    {
        int headIdx = fxi->fixtureMode()->headForChannel(chIndex);
        if (headIdx >= 0)
            entry["headIndex"] = headIdx;
    }

    return entry;
}

} // namespace mcp
#endif
