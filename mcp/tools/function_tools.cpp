/*
  Q Light Controller Plus
  function_tools.cpp

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
#include "functionmanager.h"
#include "doc.h"
#include "fixture.h"
#include "qlcchannel.h"
#include "qlcpalette.h"
#include "scene.h"
#include "chaser.h"
#include "chaserstep.h"
#include "sequence.h"
#include "collection.h"
#include "efx.h"
#include "efxfixture.h"
#include "rgbmatrix.h"
#include "rgbalgorithm.h"
#include "fixturegroup.h"
#include "script.h"
#include "scenevalue.h"
#include "inputoutputmap.h"
#include "universe.h"

#include <QRegularExpression>
#include <fastmcpp/tools/manager.hpp>
#include <fastmcpp/tools/tool.hpp>

void registerFunctionTools(fastmcpp::tools::ToolManager &tm, Doc *doc, FunctionManager *funcMgr)
{
    using Json = nlohmann::json;
    using Tool = fastmcpp::tools::Tool;

    // create_scenes (batch)
    tm.register_tool(Tool(
        "create_scenes",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"name", {{"type", "string"}}},
                {"fixtureIDs", {{"type", "array"}, {"items", {{"type", "integer"}}}}},
                {"fixtureNames", {{"type", "array"}, {"items", {{"type", "string"}}}, {"description", "Fixture name patterns (glob: * ?). Alternative to fixtureIDs."}}},
                {"paletteIDs", {{"type", "array"}, {"items", {{"type", "integer"}}}, {"description", "Palette IDs to reference. Palettes provide reusable values (color, position, dimmer)."}}},
                {"paletteNames", {{"type", "array"}, {"items", {{"type", "string"}}}, {"description", "Palette names to reference (glob patterns). Resolved from existing palettes."}}},
                {"fadeIn", {{"type", "integer"}, {"description", "Fade in time in ms (default 0)"}}},
                {"fadeOut", {{"type", "integer"}, {"description", "Fade out time in ms (default 0)"}}},
                {"channelValues", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                    {"fixtureID", {{"type", "integer"}}},
                    {"channel", {{"type", "integer"}}},
                    {"value", {{"type", "integer"}}}
                }}}}, {"description", "Explicit DMX channel values (override palettes). Each entry sets exactly one channel on one fixture."}}},
                {"positions", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                    {"fixtureID", {{"type", "integer"}}},
                    {"panDegrees", {{"type", "number"}, {"description", "Pan position in degrees (0 to focusPanMax)"}}},
                    {"tiltDegrees", {{"type", "number"}, {"description", "Tilt position in degrees (0 to focusTiltMax)"}}},
                    {"zoomDegrees", {{"type", "number"}, {"description", "Zoom/beam angle in degrees"}}}
                }}, {"required", {"fixtureID"}}}}}}
            }}, {"required", {"name"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            try {
            Json results = Json::array();
            if (!args.contains("items") || !args.at("items").is_array())
                return Json({{"error","items array required"}}).dump();
            for (auto &item : args.at("items"))
            {
                auto err = validateFields(item, {"name", "fixtureIDs", "fixtureNames", "paletteIDs", "paletteNames", "fadeIn", "fadeOut", "channelValues", "positions"});
                if (!err.empty()) { results.push_back(nlohmann::json::parse(err)); continue; }

                if (!item.contains("name"))
                {
                    results.push_back({{"error","name required"}});
                    continue;
                }

                // Resolve fixture IDs from names or direct IDs
                QList<quint32> fixtureIDs;
                if (item.contains("fixtureNames"))
                {
                    for (auto &pattern : item.at("fixtureNames"))
                    {
                        auto ids = mcp::resolveFixturesByName(doc, QString::fromStdString(pattern.get<std::string>()));
                        for (quint32 id : ids)
                            if (!fixtureIDs.contains(id)) fixtureIDs.append(id);
                    }
                }
                if (item.contains("fixtureIDs"))
                {
                    for (auto &fxId : item.at("fixtureIDs"))
                    {
                        quint32 id = fxId.get<int>();
                        if (!fixtureIDs.contains(id)) fixtureIDs.append(id);
                    }
                }

                QString name = QString::fromStdString(item.at("name").get<std::string>());
                Function *existing = mcp::findFunction(doc, name, Function::SceneType);
                Scene *scene;
                bool isNew = false;
                if (existing)
                {
                    scene = qobject_cast<Scene*>(existing);
                    scene->clear(); // Reset values for upsert
                    // Clear palette and fixture refs on upsert
                    for (quint32 palId : scene->palettes())
                        scene->removePalette(palId);
                    for (quint32 fxId : scene->fixtures())
                        scene->removeFixture(fxId);
                }
                else
                {
                    scene = new Scene(doc);
                    scene->setName(name);
                    isNew = true;
                }

                // Register fixtures on the scene (required for palette resolution)
                for (quint32 fxId : fixtureIDs)
                    scene->addFixture(fxId);

                if (item.contains("fadeIn"))
                    scene->setFadeInSpeed(item.at("fadeIn").get<int>());
                if (item.contains("fadeOut"))
                    scene->setFadeOutSpeed(item.at("fadeOut").get<int>());

                // Resolve and add palette references
                if (item.contains("paletteIDs") && item.at("paletteIDs").is_array())
                {
                    for (auto &palId : item.at("paletteIDs"))
                    {
                        quint32 id = palId.get<int>();
                        if (doc->palette(id))
                            scene->addPalette(id);
                    }
                }
                if (item.contains("paletteNames") && item.at("paletteNames").is_array())
                {
                    for (auto &palName : item.at("paletteNames"))
                    {
                        QString pattern = QString::fromStdString(palName.get<std::string>());
                        QRegularExpression re(
                            QRegularExpression::wildcardToRegularExpression(pattern),
                            QRegularExpression::CaseInsensitiveOption);
                        for (QLCPalette *p : doc->palettes())
                        {
                            if (re.match(p->name()).hasMatch())
                                scene->addPalette(p->id());
                        }
                    }
                }

                // Apply degree-based positions (before channelValues so explicit values can override)
                if (item.contains("positions") && item.at("positions").is_array())
                {
                    for (auto &pos : item.at("positions"))
                    {
                        auto posErr = validateFields(pos, {"fixtureID", "panDegrees", "tiltDegrees", "zoomDegrees"});
                        if (!posErr.empty()) { results.push_back(nlohmann::json::parse(posErr)); continue; }
                        if (!pos.contains("fixtureID")) continue;
                        quint32 fxID = pos.at("fixtureID").get<int>();
                        Fixture *fxi = doc->fixture(fxID);
                        if (!fxi) continue;

                        // Auto-register fixture from positions
                        scene->addFixture(fxID);

                        if (pos.contains("panDegrees"))
                        {
                            for (const SceneValue &sv : fxi->positionToValues(QLCChannel::Pan, pos.at("panDegrees").get<float>()))
                                scene->setValue(sv);
                        }
                        if (pos.contains("tiltDegrees"))
                        {
                            for (const SceneValue &sv : fxi->positionToValues(QLCChannel::Tilt, pos.at("tiltDegrees").get<float>()))
                                scene->setValue(sv);
                        }
                        if (pos.contains("zoomDegrees"))
                        {
                            for (const SceneValue &sv : fxi->zoomToValues(pos.at("zoomDegrees").get<float>(), false))
                                scene->setValue(sv);
                        }
                    }
                }

                // Set explicit channel values (override palettes and positions)
                if (item.contains("channelValues") && item.at("channelValues").is_array())
                {
                    for (auto &cv : item.at("channelValues"))
                    {
                        auto cvErr = validateFields(cv, {"fixtureID", "channel", "value"});
                        if (!cvErr.empty()) { results.push_back(nlohmann::json::parse(cvErr)); continue; }
                        if (!cv.contains("fixtureID") || !cv.contains("channel") || !cv.contains("value"))
                            continue;
                        quint32 fxID = cv.at("fixtureID").get<int>();
                        quint32 chIdx = cv.at("channel").get<int>();
                        uchar value = cv.at("value").get<int>();
                        scene->setValue(SceneValue(fxID, chIdx, value));

                        // Auto-register fixture from channelValues
                        scene->addFixture(fxID);
                    }
                }

                if (isNew)
                    doc->addFunction(scene);

                Json result = {{"id", (int)scene->id()}, {"name", scene->name().toStdString()}, {"status", isNew ? "created" : "updated"}};
                if (!scene->palettes().isEmpty())
                    result["paletteCount"] = (int)scene->palettes().count();
                results.push_back(result);
            }
            return results.dump();
            } catch (const std::exception &e) {
                return Json({{"error", e.what()}}).dump();
            }
            });
        },
        std::nullopt,
        std::string("Create scenes with palettes, channel values, and/or degree-based positions. "
                     "Palette-first: reference palettes via paletteNames/paletteIDs for reusable values; "
                     "channelValues override palettes for fine-tuning. "
                     "Upserts: replaces all values and palette refs on existing scenes. Batch."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotIdempotent));

    // update_scene_from_dmx (batch) — capture live DMX into scenes
    tm.register_tool(Tool(
        "update_scene_from_dmx",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"sceneName", {{"type", "string"}, {"description", "Target scene name. Creates new if not exists (upsert)."}}},
                {"fixtureIDs", {{"type", "array"}, {"items", {{"type", "integer"}}}, {"description", "Fixture IDs to capture (empty = all patched)"}}},
                {"fixtureNames", {{"type", "array"}, {"items", {{"type", "string"}}}, {"description", "Fixture name patterns (glob: * ?)"}}},
                {"channelFilter", {{"type", "string"}, {"description", "Filter: all (default), dimmer, color, position, gobo, shutter, beam, effect"}}},
                {"nonZeroOnly", {{"type", "boolean"}, {"description", "Only capture non-zero channels (default true)"}}},
                {"merge", {{"type", "boolean"}, {"description", "true = keep existing values for uncaptured channels; false = replace all (default false)"}}}
            }}, {"required", {"sceneName"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto err = validateFields(args, {"items"});
            if (!err.empty()) return err;
            if (!args.contains("items") || !args.at("items").is_array())
                return Json({{"error", "items array required"}}).dump();

            // Channel group filter helper
            auto groupMatches = [](const std::string &filter, QLCChannel::Group g) -> bool {
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

            InputOutputMap *ioMap = doc->inputOutputMap();
            QList<Universe*> universes = ioMap->claimUniverses();

            Json results = Json::array();
            for (auto &item : args.at("items"))
            {
                auto itemErr = validateFields(item, {"sceneName", "fixtureIDs", "fixtureNames", "channelFilter", "nonZeroOnly", "merge"});
                if (!itemErr.empty()) { results.push_back(Json::parse(itemErr)); continue; }

                if (!item.contains("sceneName"))
                {
                    results.push_back({{"error", "sceneName required"}});
                    continue;
                }

                QString sceneName = QString::fromStdString(item.at("sceneName").get<std::string>());
                bool nonZero = item.value("nonZeroOnly", true);
                bool merge = item.value("merge", false);
                std::string filter = item.value("channelFilter", "all");

                // Resolve fixtures
                QList<quint32> fixtureIDs;
                if (item.contains("fixtureNames") && item.at("fixtureNames").is_array())
                {
                    for (auto &p : item.at("fixtureNames"))
                    {
                        auto ids = mcp::resolveFixturesByName(doc, QString::fromStdString(p.get<std::string>()));
                        for (quint32 id : ids)
                            if (!fixtureIDs.contains(id)) fixtureIDs.append(id);
                    }
                }
                if (item.contains("fixtureIDs") && item.at("fixtureIDs").is_array())
                {
                    for (auto &fid : item.at("fixtureIDs"))
                    {
                        quint32 id = fid.get<int>();
                        if (!fixtureIDs.contains(id)) fixtureIDs.append(id);
                    }
                }
                if (fixtureIDs.isEmpty())
                {
                    for (Fixture *fxi : doc->fixtures())
                        fixtureIDs.append(fxi->id());
                }

                // Find or create scene
                Function *existing = mcp::findFunction(doc, sceneName, Function::SceneType);
                Scene *scene;
                bool isNew = false;
                if (existing)
                {
                    scene = qobject_cast<Scene*>(existing);
                    if (!merge)
                    {
                        scene->clear();
                        for (quint32 palId : scene->palettes())
                            scene->removePalette(palId);
                        for (quint32 fxId : scene->fixtures())
                            scene->removeFixture(fxId);
                    }
                }
                else
                {
                    scene = new Scene(doc);
                    scene->setName(sceneName);
                    isNew = true;
                }

                // Read DMX values and populate scene
                int capturedChannels = 0;
                for (quint32 fxID : fixtureIDs)
                {
                    Fixture *fxi = doc->fixture(fxID);
                    if (!fxi) continue;

                    int uniIdx = fxi->universe();
                    int baseAddr = fxi->address();
                    if (uniIdx < 0 || uniIdx >= universes.count()) continue;

                    Universe *uni = universes.at(uniIdx);
                    const QByteArray preGM = uni->preGMValues();

                    scene->addFixture(fxID);

                    for (quint32 ch = 0; ch < fxi->channels(); ch++)
                    {
                        const QLCChannel *qlcCh = fxi->channel(ch);
                        if (!qlcCh) continue;
                        if (!groupMatches(filter, qlcCh->group())) continue;

                        int absAddr = baseAddr + (int)ch;
                        uchar val = (absAddr < preGM.size()) ? static_cast<uchar>(preGM.at(absAddr)) : 0;

                        if (nonZero && val == 0) continue;

                        scene->setValue(SceneValue(fxID, ch, val));
                        capturedChannels++;
                    }
                }

                if (isNew)
                    doc->addFunction(scene);

                results.push_back({
                    {"sceneID", (int)scene->id()},
                    {"sceneName", scene->name().toStdString()},
                    {"status", isNew ? "created" : "updated"},
                    {"capturedChannels", capturedChannels},
                    {"fixtureCount", (int)fixtureIDs.count()}
                });
            }

            ioMap->releaseUniverses(false);

            return results.dump();
            });
        },
        std::nullopt,
        std::string("Capture current live DMX output into scenes. Reads pre-Grand Master values and writes them as scene channel values. "
                     "Supports channel filtering, non-zero-only mode, and merge (keep existing) vs replace mode. "
                     "Use case: 'save current Pan/Tilt of HERO fixtures to scene ABC'. Batch."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotIdempotent));

    // create_chasers (batch)
    tm.register_tool(Tool(
        "create_chasers",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"name", {{"type", "string"}}},
                {"steps", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                    {"functionID", {{"type", "integer"}}},
                    {"functionName", {{"type", "string"}, {"description", "Function name. Alternative to functionID."}}},
                    {"fadeIn", {{"type", "integer"}, {"description", "Fade in: milliseconds when tempoType='time'; in beat mode: internal encoding (1000=1beat, 500=1/2beat, 250=1/4beat, 125=1/8beat). Default 0"}}},
                    {"hold", {{"type", "integer"}, {"description", "Hold: milliseconds when tempoType='time'; in beat mode: internal encoding (1000=1beat, 500=1/2beat, 250=1/4beat, 125=1/8beat). Default 0"}}},
                    {"fadeOut", {{"type", "integer"}, {"description", "Fade out: milliseconds when tempoType='time'; in beat mode: internal encoding (1000=1beat, 500=1/2beat, 250=1/4beat, 125=1/8beat). Default 0"}}}
                }}, {"required", Json::array()}}}}},
                {"runOrder", {{"type", "string"}, {"enum", {"loop", "single", "pingpong", "random"}}, {"description", "Run order (default loop)"}}},
                {"direction", {{"type", "string"}, {"enum", {"forward", "backward"}}, {"description", "Direction (default forward)"}}},
                {"tempoType", {{"type", "string"}, {"enum", {"time", "beats"}}, {"description", "Tempo type: 'time' (ms) or 'beats' (BPM-synced) (default time)"}}},
                {"fadeInMode", {{"type", "string"}, {"enum", {"default", "common", "perStep"}}, {"description", "Fade in speed mode (default perStep)"}}},
                {"fadeOutMode", {{"type", "string"}, {"enum", {"default", "common", "perStep"}}, {"description", "Fade out speed mode (default perStep)"}}},
                {"durationMode", {{"type", "string"}, {"enum", {"default", "common", "perStep"}}, {"description", "Duration speed mode (default perStep)"}}}
            }}, {"required", {"name", "steps"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            try {
            Json results = Json::array();
            if (!args.contains("items") || !args.at("items").is_array())
                return Json({{"error","items array required"}}).dump();
            for (auto &item : args.at("items"))
            {
                auto err = validateFields(item, {"name", "steps", "tempoType", "runOrder", "direction", "fadeInMode", "fadeOutMode", "durationMode"});
                if (!err.empty()) { results.push_back(nlohmann::json::parse(err)); continue; }

                if (!item.contains("name") || !item.contains("steps") || !item.at("steps").is_array())
                {
                    results.push_back({{"error","name and steps required"}});
                    continue;
                }

                QString name = QString::fromStdString(item.at("name").get<std::string>());
                Function *existing = mcp::findFunction(doc, name, Function::ChaserType);
                Chaser *chaser;
                bool isNew = false;
                if (existing)
                {
                    chaser = qobject_cast<Chaser*>(existing);
                    // Clear existing steps for upsert
                    while (chaser->steps().size() > 0)
                        chaser->removeStep(0);
                }
                else
                {
                    chaser = new Chaser(doc);
                    chaser->setName(name);
                    isNew = true;
                }

                // Speed modes (default to perStep since steps carry their own timing)
                auto parseSpeedMode = [](const std::string &mode) -> Chaser::SpeedMode {
                    if (mode == "default") return Chaser::Default;
                    if (mode == "common") return Chaser::Common;
                    return Chaser::PerStep;
                };
                chaser->setFadeInMode(parseSpeedMode(item.value("fadeInMode", "perStep")));
                chaser->setFadeOutMode(parseSpeedMode(item.value("fadeOutMode", "perStep")));
                chaser->setDurationMode(parseSpeedMode(item.value("durationMode", "perStep")));

                // Run order
                QString order = QString::fromStdString(item.value("runOrder", "loop"));
                if (order == "single") chaser->setRunOrder(Function::SingleShot);
                else if (order == "pingpong") chaser->setRunOrder(Function::PingPong);
                else if (order == "random") chaser->setRunOrder(Function::Random);
                else chaser->setRunOrder(Function::Loop);

                // Direction
                QString dir = QString::fromStdString(item.value("direction", "forward"));
                if (dir == "backward") chaser->setDirection(Function::Backward);
                else chaser->setDirection(Function::Forward);

                // Tempo type
                QString tempo = QString::fromStdString(item.value("tempoType", "time"));
                if (tempo == "beats") chaser->setTempoType(Function::Beats);
                else chaser->setTempoType(Function::Time);
                bool isBeatMode = (tempo == "beats");

                // Add steps with per-step timing
                for (auto &step : item.at("steps"))
                {
                    auto stepErr = validateFields(step, {"functionID", "functionName", "fadeIn", "hold", "fadeOut"});
                    if (!stepErr.empty()) { results.push_back(nlohmann::json::parse(stepErr)); continue; }
                    quint32 fid = Function::invalidId();
                    if (step.contains("functionID"))
                        fid = step.at("functionID").get<int>();
                    else if (step.contains("functionName"))
                        fid = mcp::resolveFunctionByName(doc, QString::fromStdString(step.at("functionName").get<std::string>()));
                    if (fid == Function::invalidId()) continue;
                    // In beat mode, values are already in internal encoding
                    // (1000=1beat, 500=1/2beat, 250=1/4beat, 125=1/8beat)
                    uint fadeIn = step.value("fadeIn", 0);
                    uint hold = step.value("hold", 0);
                    uint fadeOut = step.value("fadeOut", 0);
                    chaser->addStep(ChaserStep(fid, fadeIn, hold, fadeOut));
                }

                if (isNew)
                    doc->addFunction(chaser);
                results.push_back({{"id", (int)chaser->id()}, {"name", chaser->name().toStdString()}, {"status", isNew ? "created" : "updated"}});
            }
            return results.dump();
            } catch (const std::exception &e) {
                return Json({{"error", e.what()}}).dump();
            }
            });
        },
        std::nullopt,
        std::string("Create chasers with per-step timing. Upserts: replaces all steps on existing chasers. Batch."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotIdempotent));

    // create_sequences (batch)
    tm.register_tool(Tool(
        "create_sequences",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"name", {{"type", "string"}}},
                {"boundSceneID", {{"type", "integer"}, {"description", "Scene ID this sequence is bound to"}}},
                {"fadeIn", {{"type", "integer"}, {"description", "Fade in time per step in ms (default 0)"}}},
                {"fadeOut", {{"type", "integer"}, {"description", "Fade out time per step in ms (default 0)"}}},
                {"holdTime", {{"type", "integer"}, {"description", "Hold/duration per step in ms (default 1000)"}}},
                {"runOrder", {{"type", "string"}, {"enum", {"loop", "single", "pingpong", "random"}}, {"description", "Run order (default loop)"}}},
                {"direction", {{"type", "string"}, {"enum", {"forward", "backward"}}, {"description", "Direction (default forward)"}}}
            }}, {"required", {"name", "boundSceneID"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            try {
            Json results = Json::array();
            if (!args.contains("items") || !args.at("items").is_array())
                return Json({{"error","items array required"}}).dump();
            for (auto &item : args.at("items"))
            {
                auto err = validateFields(item, {"name", "boundSceneID", "fadeIn", "fadeOut", "holdTime", "runOrder", "direction"});
                if (!err.empty()) { results.push_back(nlohmann::json::parse(err)); continue; }

                if (!item.contains("name") || !item.contains("boundSceneID"))
                {
                    results.push_back({{"error","name and boundSceneID required"}});
                    continue;
                }

                QString name = QString::fromStdString(item.at("name").get<std::string>());
                Function *existing = mcp::findFunction(doc, name, Function::SequenceType);
                Sequence *seq;
                bool isNew = false;
                if (existing)
                {
                    seq = qobject_cast<Sequence*>(existing);
                }
                else
                {
                    seq = new Sequence(doc);
                    seq->setName(name);
                    isNew = true;
                }
                seq->setBoundSceneID(item.at("boundSceneID").get<int>());

                seq->setFadeInSpeed(item.value("fadeIn", 0));
                seq->setFadeOutSpeed(item.value("fadeOut", 0));
                seq->setDuration(item.value("holdTime", 1000));

                QString order = QString::fromStdString(item.value("runOrder", "loop"));
                if (order == "single") seq->setRunOrder(Function::SingleShot);
                else if (order == "pingpong") seq->setRunOrder(Function::PingPong);
                else if (order == "random") seq->setRunOrder(Function::Random);
                else seq->setRunOrder(Function::Loop);

                QString dir = QString::fromStdString(item.value("direction", "forward"));
                if (dir == "backward") seq->setDirection(Function::Backward);
                else seq->setDirection(Function::Forward);

                if (isNew)
                    doc->addFunction(seq);
                results.push_back({{"id", (int)seq->id()}, {"name", seq->name().toStdString()}, {"status", isNew ? "created" : "updated"}});
            }
            return results.dump();
            } catch (const std::exception &e) {
                return Json({{"error", e.what()}}).dump();
            }
            });
        },
        std::nullopt,
        std::string("Create sequences bound to scenes for per-channel step animation. Upserts: replaces timing and binding on existing sequences. Batch."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotIdempotent));

    // create_efxs (batch)
    tm.register_tool(Tool(
        "create_efxs",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"name", {{"type", "string"}}},
                {"fixtureIDs", {{"type", "array"}, {"items", {{"type", "integer"}}}}},
                {"fixtureNames", {{"type", "array"}, {"items", {{"type", "string"}}}, {"description", "Fixture name patterns (glob: * ?). Alternative to fixtureIDs."}}},
                {"algorithm", {{"type", "string"}, {"enum", {"Circle", "Eight", "Line", "Line2", "Diamond", "Square", "SquareChoppy", "SquareTrue", "Leaf", "Lissajous"}}, {"description", "Pattern algorithm (default Circle)"}}},
                {"width", {{"type", "integer"}, {"description", "Pattern width 0-255 (default 127)"}}},
                {"height", {{"type", "integer"}, {"description", "Pattern height 0-255 (default 127)"}}},
                {"xOffset", {{"type", "integer"}, {"description", "X center offset 0-255 (default 127 = middle)"}}},
                {"yOffset", {{"type", "integer"}, {"description", "Y center offset 0-255 (default 127 = middle)"}}},
                {"rotation", {{"type", "integer"}, {"description", "Pattern rotation 0-359 degrees (default 0)"}}},
                {"startOffset", {{"type", "integer"}, {"description", "Start phase offset 0-359 degrees (default 0)"}}},
                {"xFrequency", {{"type", "integer"}, {"description", "X frequency 0-5 for Lissajous (default 2)"}}},
                {"yFrequency", {{"type", "integer"}, {"description", "Y frequency 0-5 for Lissajous (default 3)"}}},
                {"xPhase", {{"type", "integer"}, {"description", "X phase 0-359 for Lissajous (default 0)"}}},
                {"yPhase", {{"type", "integer"}, {"description", "Y phase 0-359 for Lissajous (default 0)"}}},
                {"isRelative", {{"type", "boolean"}, {"description", "Relative to current position (default false)"}}},
                {"propagationMode", {{"type", "string"}, {"enum", {"Parallel", "Serial", "Asymmetric"}}, {"description", "Multi-fixture propagation (default Parallel)"}}},
                {"speed", {{"type", "integer"}, {"description", "Duration/cycle time in ms (default 5000)"}}},
                {"fadeIn", {{"type", "integer"}, {"description", "Fade in time in ms (default 0)"}}},
                {"fadeOut", {{"type", "integer"}, {"description", "Fade out time in ms (default 0)"}}},
                {"runOrder", {{"type", "string"}, {"enum", {"loop", "single", "pingpong"}}, {"description", "Run order (default loop)"}}},
                {"direction", {{"type", "string"}, {"enum", {"forward", "backward"}}, {"description", "Direction (default forward)"}}},
                {"head", {{"type", "integer"}, {"description", "Head index for multi-head fixtures (default 0)"}}}
            }}, {"required", {"name", "algorithm"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            for (auto &item : args.at("items"))
            {
                auto err = validateFields(item, {"name", "fixtureIDs", "fixtureNames", "algorithm", "width", "height", "xOffset", "yOffset", "rotation", "startOffset", "xFrequency", "yFrequency", "xPhase", "yPhase", "isRelative", "propagationMode", "speed", "fadeIn", "fadeOut", "runOrder", "direction", "head"});
                if (!err.empty()) { results.push_back(nlohmann::json::parse(err)); continue; }

                QString name = QString::fromStdString(item.at("name").get<std::string>());

                // Resolve fixtures
                QList<quint32> fixtureIDs;
                if (item.contains("fixtureNames"))
                    for (auto &p : item.at("fixtureNames"))
                        for (quint32 id : mcp::resolveFixturesByName(doc, QString::fromStdString(p.get<std::string>())))
                            if (!fixtureIDs.contains(id)) fixtureIDs.append(id);
                if (item.contains("fixtureIDs"))
                    for (auto &fid : item.at("fixtureIDs"))
                        { quint32 id = fid.get<int>(); if (!fixtureIDs.contains(id)) fixtureIDs.append(id); }

                Function *existing = mcp::findFunction(doc, name, Function::EFXType);
                EFX *efx;
                bool isNew = false;
                if (existing)
                {
                    efx = qobject_cast<EFX*>(existing);
                    efx->removeAllFixtures();
                }
                else
                {
                    efx = new EFX(doc);
                    efx->setName(name);
                    isNew = true;
                }

                // Algorithm
                QString algo = QString::fromStdString(item.value("algorithm", "Circle"));
                efx->setAlgorithm(EFX::stringToAlgorithm(algo));

                // Pattern geometry
                efx->setWidth(item.value("width", 127));
                efx->setHeight(item.value("height", 127));
                efx->setXOffset(item.value("xOffset", 127));
                efx->setYOffset(item.value("yOffset", 127));
                efx->setRotation(item.value("rotation", 0));
                efx->setStartOffset(item.value("startOffset", 0));
                efx->setIsRelative(item.value("isRelative", false));

                // Lissajous parameters
                efx->setXFrequency(item.value("xFrequency", 2));
                efx->setYFrequency(item.value("yFrequency", 3));
                efx->setXPhase(item.value("xPhase", 0));
                efx->setYPhase(item.value("yPhase", 0));

                // Propagation mode
                QString propMode = QString::fromStdString(item.value("propagationMode", "Parallel"));
                if (propMode == "Serial") efx->setPropagationMode(EFX::Serial);
                else if (propMode == "Asymmetric") efx->setPropagationMode(EFX::Asymmetric);
                else efx->setPropagationMode(EFX::Parallel);

                // Speed
                efx->setDuration(item.value("speed", 5000));
                if (item.contains("fadeIn"))
                    efx->setFadeInSpeed(item.at("fadeIn").get<int>());
                if (item.contains("fadeOut"))
                    efx->setFadeOutSpeed(item.at("fadeOut").get<int>());

                // Run order / direction
                QString order = QString::fromStdString(item.value("runOrder", "loop"));
                if (order == "single") efx->setRunOrder(Function::SingleShot);
                else if (order == "pingpong") efx->setRunOrder(Function::PingPong);
                else efx->setRunOrder(Function::Loop);

                QString dir = QString::fromStdString(item.value("direction", "forward"));
                if (dir == "backward") efx->setDirection(Function::Backward);
                else efx->setDirection(Function::Forward);

                for (quint32 fid : fixtureIDs)
                {
                    EFXFixture *ef = new EFXFixture(efx);
                    int head = 0;
                    if (item.contains("head"))
                        head = item.at("head").get<int>();
                    ef->setHead(GroupHead(fid, head));
                    efx->addFixture(ef);
                }

                if (isNew)
                    doc->addFunction(efx);
                results.push_back({{"id", (int)efx->id()}, {"name", efx->name().toStdString()}, {"status", isNew ? "created" : "updated"}});
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("Create EFX position effects for moving heads (10 algorithm types). Upserts: replaces all settings on existing EFXs. Batch."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotIdempotent));

    // create_collections (batch)
    tm.register_tool(Tool(
        "create_collections",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"name", {{"type", "string"}}},
                {"functionIDs", {{"type", "array"}, {"items", {{"type", "integer"}}}}},
                {"functionNames", {{"type", "array"}, {"items", {{"type", "string"}}}, {"description", "Function names to include. Alternative to functionIDs."}}}
            }}, {"required", {"name"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            for (auto &item : args.at("items"))
            {
                auto err = validateFields(item, {"name", "functionIDs", "functionNames"});
                if (!err.empty()) { results.push_back(nlohmann::json::parse(err)); continue; }

                QString name = QString::fromStdString(item.at("name").get<std::string>());
                Function *existing = mcp::findFunction(doc, name, Function::CollectionType);
                Collection *col;
                bool isNew = false;
                if (existing)
                {
                    col = qobject_cast<Collection*>(existing);
                    // Clear existing members for upsert
                    for (quint32 fid : col->functions())
                        col->removeFunction(fid);
                }
                else
                {
                    col = new Collection(doc);
                    col->setName(name);
                    isNew = true;
                }
                // Resolve function IDs from names or direct IDs
                QList<quint32> funcIDs;
                if (item.contains("functionNames"))
                    for (auto &fn : item.at("functionNames"))
                    {
                        quint32 fid = mcp::resolveFunctionByName(doc, QString::fromStdString(fn.get<std::string>()));
                        if (fid != Function::invalidId() && !funcIDs.contains(fid))
                            funcIDs.append(fid);
                    }
                if (item.contains("functionIDs"))
                    for (auto &fid : item.at("functionIDs"))
                    {
                        quint32 id = fid.get<int>();
                        if (!funcIDs.contains(id)) funcIDs.append(id);
                    }
                for (quint32 fid : funcIDs)
                    col->addFunction(fid);
                if (isNew)
                    doc->addFunction(col);
                results.push_back({{"id", (int)col->id()}, {"name", col->name().toStdString()}, {"status", isNew ? "created" : "updated"}});
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("Create collections (parallel function groups — use for moods/phases). Upserts. Batch."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotIdempotent));

    // create_rgb_matrices (batch)
    tm.register_tool(Tool(
        "create_rgb_matrices",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"name", {{"type", "string"}}},
                {"fixtureGroupID", {{"type", "integer"}}},
                {"algorithm", {{"type", "string"}}},
                {"startColor", {{"type", "string"}, {"description", "Hex color e.g. #FF0000"}}},
                {"endColor", {{"type", "string"}, {"description", "Hex color e.g. #0000FF"}}}
            }}, {"required", {"name"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            for (auto &item : args.at("items"))
            {
                auto err = validateFields(item, {"name", "fixtureGroupID", "algorithm", "startColor", "endColor"});
                if (!err.empty()) { results.push_back(nlohmann::json::parse(err)); continue; }

                QString name = QString::fromStdString(item.at("name").get<std::string>());
                Function *existing = mcp::findFunction(doc, name, Function::RGBMatrixType);
                RGBMatrix *matrix;
                bool isNew = false;
                if (existing)
                {
                    matrix = qobject_cast<RGBMatrix*>(existing);
                }
                else
                {
                    matrix = new RGBMatrix(doc);
                    matrix->setName(name);
                    isNew = true;
                }
                if (item.contains("fixtureGroupID"))
                    matrix->setFixtureGroup(item.at("fixtureGroupID").get<int>());
                if (item.contains("algorithm"))
                {
                    QString algoName = QString::fromStdString(item.at("algorithm").get<std::string>());
                    RGBAlgorithm *algo = RGBAlgorithm::algorithm(doc, algoName);
                    if (algo)
                        matrix->setAlgorithm(algo);
                }
                if (item.contains("startColor"))
                    matrix->setColor(0, QColor(QString::fromStdString(item.at("startColor").get<std::string>())));
                if (item.contains("endColor"))
                    matrix->setColor(1, QColor(QString::fromStdString(item.at("endColor").get<std::string>())));
                if (isNew)
                    doc->addFunction(matrix);
                results.push_back({{"id", (int)matrix->id()}, {"name", matrix->name().toStdString()}, {"status", isNew ? "created" : "updated"}});
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("Create RGB matrix color animations. Upserts. Batch."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotIdempotent));

    // create_fixture_groups (batch)
    tm.register_tool(Tool(
        "create_fixture_groups",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"name", {{"type", "string"}}},
                {"fixtureIDs", {{"type", "array"}, {"items", {{"type", "integer"}}}}},
                {"columns", {{"type", "integer"}, {"description", "Grid width (default: fixture count)"}}},
                {"rows", {{"type", "integer"}, {"description", "Grid height (default: 1)"}}}
            }}, {"required", {"name", "fixtureIDs"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            for (auto &item : args.at("items"))
            {
                auto err = validateFields(item, {"name", "fixtureIDs", "columns", "rows"});
                if (!err.empty()) { results.push_back(nlohmann::json::parse(err)); continue; }

                std::string name = item.at("name").get<std::string>();
                auto fixtureIDs = item.at("fixtureIDs");
                int count = (int)fixtureIDs.size();
                int columns = item.value("columns", count);
                int rows = item.value("rows", 1);

                FixtureGroup *existingGroup = mcp::findFixtureGroup(doc, QString::fromStdString(name));
                FixtureGroup *group;
                bool isNew = false;
                if (existingGroup)
                {
                    group = existingGroup;
                    // Clear existing assignments for upsert
                    for (quint32 fid : group->fixtureList())
                        group->resignFixture(fid);
                }
                else
                {
                    group = new FixtureGroup(doc);
                    group->setName(QString::fromStdString(name));
                    isNew = true;
                }
                group->setSize(QSize(columns, rows));

                int col = 0, row = 0;
                for (auto &fid : fixtureIDs)
                {
                    group->assignFixture(fid.get<int>(), QLCPoint(col, row));
                    col++;
                    if (col >= columns)
                    {
                        col = 0;
                        row++;
                    }
                }

                if (isNew)
                    doc->addFixtureGroup(group);
                results.push_back({{"id", (int)group->id()}, {"name", name}, {"status", isNew ? "created" : "updated"}});
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("Create fixture groups with grid layout for RGB matrices. Upserts. Batch."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotIdempotent));

    // create_scripts (batch)
    tm.register_tool(Tool(
        "create_scripts",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"name", {{"type", "string"}}},
                {"commands", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                    {"type", {{"type", "string"}, {"description", "Command type: startfunction, stopfunction, wait, setfixture, blackout, label, jump"}}},
                    {"functionID", {{"type", "integer"}, {"description", "Function ID (for startfunction/stopfunction)"}}},
                    {"time", {{"type", "integer"}, {"description", "Wait time in ms (for wait)"}}},
                    {"fixtureID", {{"type", "integer"}, {"description", "Fixture ID (for setfixture)"}}},
                    {"channel", {{"type", "integer"}, {"description", "Channel number (for setfixture)"}}},
                    {"value", {{"type", "integer"}, {"description", "DMX value 0-255 (for setfixture)"}}},
                    {"state", {{"type", "string"}, {"description", "'on' or 'off' (for blackout)"}}},
                    {"name", {{"type", "string"}, {"description", "Label name (for label/jump)"}}},
                    {"label", {{"type", "string"}, {"description", "Target label (for jump)"}}}
                }}}}}}
            }}, {"required", {"name", "commands"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            for (auto &item : args.at("items"))
            {
                auto err = validateFields(item, {"name", "commands"});
                if (!err.empty()) { results.push_back(nlohmann::json::parse(err)); continue; }

                std::string name = item.at("name").get<std::string>();

                Function *existing = mcp::findFunction(doc, QString::fromStdString(name), Function::ScriptType);
                Script *script;
                bool isNew = false;
                if (existing)
                {
                    script = qobject_cast<Script*>(existing);
                }
                else
                {
                    script = new Script(doc);
                    script->setName(QString::fromStdString(name));
                    isNew = true;
                }

                // Validate commands before building script
                Json cmdErrors = Json::array();
                int lineNum = 0;
                for (auto &cmd : item.at("commands"))
                {
                    auto cmdErr = validateFields(cmd, {"type", "functionID", "time", "fixtureID", "channel", "value", "state", "name", "label"});
                    if (!cmdErr.empty()) { results.push_back(nlohmann::json::parse(cmdErr)); continue; }
                    lineNum++;
                    if (!cmd.contains("type"))
                    {
                        cmdErrors.push_back({{"line", lineNum}, {"error", "missing 'type' field"}});
                        continue;
                    }
                    std::string type = cmd.at("type").get<std::string>();
                    if (type == "startfunction" || type == "stopfunction")
                    {
                        if (!cmd.contains("functionID"))
                            cmdErrors.push_back({{"line", lineNum}, {"error", type + " requires 'functionID'"}});
                    }
                    else if (type == "wait")
                    {
                        if (!cmd.contains("time"))
                            cmdErrors.push_back({{"line", lineNum}, {"error", "wait requires 'time' (ms)"}});
                    }
                    else if (type == "setfixture")
                    {
                        if (!cmd.contains("fixtureID") || !cmd.contains("channel") || !cmd.contains("value"))
                            cmdErrors.push_back({{"line", lineNum}, {"error", "setfixture requires 'fixtureID', 'channel', 'value'"}});
                    }
                    else if (type == "blackout")
                    {
                        if (!cmd.contains("state"))
                            cmdErrors.push_back({{"line", lineNum}, {"error", "blackout requires 'state' ('on' or 'off')"}});
                    }
                    else if (type == "label")
                    {
                        if (!cmd.contains("name"))
                            cmdErrors.push_back({{"line", lineNum}, {"error", "label requires 'name'"}});
                    }
                    else if (type == "jump")
                    {
                        if (!cmd.contains("label"))
                            cmdErrors.push_back({{"line", lineNum}, {"error", "jump requires 'label'"}});
                    }
                    else
                    {
                        cmdErrors.push_back({{"line", lineNum}, {"error", "unknown command type: " + type}});
                    }
                }

                if (!cmdErrors.empty())
                {
                    results.push_back({{"error", "script validation failed"}, {"name", name}, {"validationErrors", cmdErrors}});
                    continue;
                }

                // Build script data string — commands are emitted exactly as provided
                QString scriptData;
                auto &commands = item.at("commands");
                for (size_t i = 0; i < commands.size(); i++)
                {
                    auto &cmd = commands[i];
                    std::string type = cmd.at("type").get<std::string>();

                    if (type == "startfunction")
                        scriptData += QString("startfunction:%1\n").arg(cmd.at("functionID").get<int>());
                    else if (type == "stopfunction")
                        scriptData += QString("stopfunction:%1\n").arg(cmd.at("functionID").get<int>());
                    else if (type == "wait")
                        scriptData += QString("wait:%1\n").arg(cmd.at("time").get<int>());
                    else if (type == "setfixture")
                        scriptData += QString("setfixture:%1 ch:%2 val:%3\n")
                            .arg(cmd.at("fixtureID").get<int>())
                            .arg(cmd.at("channel").get<int>())
                            .arg(cmd.at("value").get<int>());
                    else if (type == "blackout")
                        scriptData += QString("blackout:%1\n")
                            .arg(QString::fromStdString(cmd.at("state").get<std::string>()));
                    else if (type == "label")
                        scriptData += QString("label:%1\n")
                            .arg(QString::fromStdString(cmd.at("name").get<std::string>()));
                    else if (type == "jump")
                        scriptData += QString("jump:%1\n")
                            .arg(QString::fromStdString(cmd.at("label").get<std::string>()));
                }

                script->setData(scriptData);
                if (isNew)
                    doc->addFunction(script);
                results.push_back({{"id", (int)script->id()}, {"name", name}, {"status", isNew ? "created" : "updated"}});
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("Create scripted sequences from commands (startfunction, stopfunction, wait, setfixture, blackout, label, jump). Upserts. Batch."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotIdempotent));

    // delete_functions (batch)
    tm.register_tool(Tool(
        "delete_functions",
        Json{{"type", "object"}, {"properties", {
            {"ids", {{"type", "array"}, {"items", {{"type", "integer"}}}, {"description", "Function IDs to delete"}}}
        }}, {"required", {"ids"}}},
        Json{},
        [doc, funcMgr](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto err = validateFields(args, {"ids"});
            if (!err.empty()) return err;
            Json results = Json::array();
            for (auto &fid : args.at("ids"))
            {
                int id = fid.get<int>();
                Function *f = doc->function(id);
                if (f == nullptr)
                {
                    results.push_back({{"id", id}, {"status", "not found"}});
                    continue;
                }
                if (funcMgr)
                    funcMgr->deleteFunction(id);
                else
                    doc->deleteFunction(id);
                results.push_back({{"id", id}, {"status", "deleted"}});
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("Delete functions by ID. Batch."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotDestructive));
}
