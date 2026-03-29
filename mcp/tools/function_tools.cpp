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
#include "doc.h"
#include "fixture.h"
#include "scene.h"
#include "chaser.h"
#include "chaserstep.h"
#include "collection.h"
#include "efx.h"
#include "efxfixture.h"
#include "rgbmatrix.h"
#include "fixturegroup.h"
#include "qlcchannel.h"
#include "scenevalue.h"

#include <fastmcpp/tools/manager.hpp>
#include <fastmcpp/tools/tool.hpp>

void registerFunctionTools(fastmcpp::tools::ToolManager &tm, Doc *doc)
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
                {"color", {{"type", "object"}, {"properties", {
                    {"r", {{"type", "integer"}}}, {"g", {{"type", "integer"}}}, {"b", {{"type", "integer"}}}
                }}}},
                {"intensity", {{"type", "integer"}, {"description", "Dimmer value 0-255 (default 255)"}}},
                {"fadeIn", {{"type", "integer"}, {"description", "Fade in time in ms (default 0)"}}},
                {"fadeOut", {{"type", "integer"}, {"description", "Fade out time in ms (default 0)"}}},
                {"channelValues", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                    {"fixtureID", {{"type", "integer"}}},
                    {"channel", {{"type", "integer"}}},
                    {"value", {{"type", "integer"}}}
                }}}}, {"description", "Set arbitrary DMX values on specific channels (for gobos, prism, color wheel, etc.)"}}}
            }}, {"required", {"name", "fixtureIDs"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            try {
            Json results = Json::array();
            if (!args.contains("items") || !args["items"].is_array())
                return Json({{"error","items array required"}}).dump();
            for (auto &item : args["items"])
            {
                if (!item.contains("name") || !item.contains("fixtureIDs"))
                {
                    results.push_back({{"error","name and fixtureIDs required"}});
                    continue;
                }
                Scene *scene = new Scene(doc);
                scene->setName(QString::fromStdString(item["name"].get<std::string>()));

                if (item.contains("fadeIn"))
                    scene->setFadeInSpeed(item["fadeIn"].get<int>());
                if (item.contains("fadeOut"))
                    scene->setFadeOutSpeed(item["fadeOut"].get<int>());

                int intensity = item.value("intensity", 255);
                bool hasColor = item.contains("color");
                int r = 0, g = 0, b = 0;
                if (hasColor)
                {
                    r = item["color"].value("r", 0);
                    g = item["color"].value("g", 0);
                    b = item["color"].value("b", 0);
                }

                for (auto &fxId : item["fixtureIDs"])
                {
                    quint32 id = fxId.get<int>();
                    Fixture *fxi = doc->fixture(id);
                    if (!fxi) continue;

                    for (quint32 ch = 0; ch < fxi->channels(); ch++)
                    {
                        const QLCChannel *channel = fxi->channel(ch);
                        if (!channel) continue;

                        if (channel->group() == QLCChannel::Intensity)
                        {
                            uchar val = intensity;
                            if (hasColor)
                            {
                                switch (channel->colour())
                                {
                                    case QLCChannel::Red: val = r; break;
                                    case QLCChannel::Green: val = g; break;
                                    case QLCChannel::Blue: val = b; break;
                                    case QLCChannel::Cyan: val = 255 - r; break;
                                    case QLCChannel::Magenta: val = 255 - g; break;
                                    case QLCChannel::Yellow: val = 255 - b; break;
                                    default: val = intensity; break;
                                }
                            }
                            scene->setValue(SceneValue(id, ch, val));
                        }
                    }
                }

                // Set arbitrary channel values (for gobos, prism, color wheel, etc.)
                if (item.contains("channelValues") && item["channelValues"].is_array())
                {
                    for (auto &cv : item["channelValues"])
                    {
                        if (!cv.contains("fixtureID") || !cv.contains("channel") || !cv.contains("value"))
                            continue;
                        quint32 fxID = cv["fixtureID"].get<int>();
                        quint32 chIdx = cv["channel"].get<int>();
                        uchar value = cv["value"].get<int>();
                        scene->setValue(SceneValue(fxID, chIdx, value));
                    }
                }

                doc->addFunction(scene);
                results.push_back({{"id", (int)scene->id()}, {"name", scene->name().toStdString()}});
            }
            return results.dump();
            } catch (const std::exception &e) {
                return Json({{"error", e.what()}}).dump();
            }
            });
        },
        std::nullopt,
        std::string("Create color/intensity scenes. Batch: pass multiple scenes in 'items'. Auto-detects RGB/CMY/dimmer channels."),
        std::nullopt
    ));

    // create_chasers (batch)
    tm.register_tool(Tool(
        "create_chasers",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"name", {{"type", "string"}}},
                {"functionIDs", {{"type", "array"}, {"items", {{"type", "integer"}}}}},
                {"holdTime", {{"type", "integer"}, {"description", "Hold/duration per step in ms (default 1000)"}}},
                {"fadeIn", {{"type", "integer"}, {"description", "Fade in time per step in ms (default 0)"}}},
                {"fadeOut", {{"type", "integer"}, {"description", "Fade out time per step in ms (default 0)"}}},
                {"runOrder", {{"type", "string"}, {"enum", {"loop", "single", "pingpong", "random"}}, {"description", "Run order (default loop)"}}},
                {"direction", {{"type", "string"}, {"enum", {"forward", "backward"}}, {"description", "Direction (default forward)"}}},
                {"tempoType", {{"type", "string"}, {"enum", {"time", "beats"}}, {"description", "Tempo type: 'time' (ms) or 'beats' (BPM-synced) (default time)"}}},
                {"fadeInMode", {{"type", "string"}, {"enum", {"default", "common", "perStep"}}, {"description", "Fade in speed mode (default common)"}}},
                {"fadeOutMode", {{"type", "string"}, {"enum", {"default", "common", "perStep"}}, {"description", "Fade out speed mode (default common)"}}},
                {"durationMode", {{"type", "string"}, {"enum", {"default", "common", "perStep"}}, {"description", "Duration speed mode (default common)"}}}
            }}, {"required", {"name", "functionIDs"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            try {
            Json results = Json::array();
            if (!args.contains("items") || !args["items"].is_array())
                return Json({{"error","items array required"}}).dump();
            for (auto &item : args["items"])
            {
                if (!item.contains("name") || !item.contains("functionIDs") || !item["functionIDs"].is_array())
                {
                    results.push_back({{"error","name and functionIDs required"}});
                    continue;
                }
                Chaser *chaser = new Chaser(doc);
                chaser->setName(QString::fromStdString(item["name"].get<std::string>()));

                // Speed modes
                auto parseSpeedMode = [](const std::string &mode) -> Chaser::SpeedMode {
                    if (mode == "default") return Chaser::Default;
                    if (mode == "perStep") return Chaser::PerStep;
                    return Chaser::Common;
                };
                chaser->setFadeInMode(parseSpeedMode(item.value("fadeInMode", "common")));
                chaser->setFadeOutMode(parseSpeedMode(item.value("fadeOutMode", "common")));
                chaser->setDurationMode(parseSpeedMode(item.value("durationMode", "common")));

                // Speeds
                chaser->setFadeInSpeed(item.value("fadeIn", 0));
                chaser->setFadeOutSpeed(item.value("fadeOut", 0));
                chaser->setDuration(item.value("holdTime", 1000));

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

                for (auto &fid : item["functionIDs"])
                    chaser->addStep(ChaserStep(fid.get<int>()));

                doc->addFunction(chaser);
                results.push_back({{"id", (int)chaser->id()}, {"name", chaser->name().toStdString()}});
            }
            return results.dump();
            } catch (const std::exception &e) {
                return Json({{"error", e.what()}}).dump();
            }
            });
        },
        std::nullopt,
        std::string("Create chasers with full run properties: runOrder, direction, tempoType, fadeIn/fadeOut/duration with speed modes (default/common/perStep). Batch."),
        std::nullopt
    ));

    // create_efxs (batch)
    tm.register_tool(Tool(
        "create_efxs",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"name", {{"type", "string"}}},
                {"fixtureIDs", {{"type", "array"}, {"items", {{"type", "integer"}}}}},
                {"algorithm", {{"type", "string"}, {"enum", {"Circle", "Eight", "Line", "Diamond", "Square", "Lissajous"}}}},
                {"width", {{"type", "integer"}, {"description", "Width 0-255 (default 127)"}}},
                {"height", {{"type", "integer"}, {"description", "Height 0-255 (default 127)"}}},
                {"speed", {{"type", "integer"}, {"description", "Duration in ms (default 5000)"}}}
            }}, {"required", {"name", "fixtureIDs"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            for (auto &item : args["items"])
            {
                EFX *efx = new EFX(doc);
                efx->setName(QString::fromStdString(item["name"].get<std::string>()));

                QString algo = QString::fromStdString(item.value("algorithm", "Circle"));
                efx->setAlgorithm(EFX::stringToAlgorithm(algo));
                efx->setWidth(item.value("width", 127));
                efx->setHeight(item.value("height", 127));
                efx->setDuration(item.value("speed", 5000));

                for (auto &fid : item["fixtureIDs"])
                {
                    EFXFixture *ef = new EFXFixture(efx);
                    ef->setHead(GroupHead(fid.get<int>(), 0));
                    efx->addFixture(ef);
                }

                doc->addFunction(efx);
                results.push_back({{"id", (int)efx->id()}, {"name", efx->name().toStdString()}});
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("Create EFX position effects for moving heads. Batch: pass multiple in 'items'."),
        std::nullopt
    ));

    // create_collections (batch)
    tm.register_tool(Tool(
        "create_collections",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"name", {{"type", "string"}}},
                {"functionIDs", {{"type", "array"}, {"items", {{"type", "integer"}}}}}
            }}, {"required", {"name", "functionIDs"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            for (auto &item : args["items"])
            {
                Collection *col = new Collection(doc);
                col->setName(QString::fromStdString(item["name"].get<std::string>()));
                for (auto &fid : item["functionIDs"])
                    col->addFunction(fid.get<int>());
                doc->addFunction(col);
                results.push_back({{"id", (int)col->id()}, {"name", col->name().toStdString()}});
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("Create collections (parallel function groups — use for moods/phases). Batch: pass multiple in 'items'."),
        std::nullopt
    ));

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
            for (auto &item : args["items"])
            {
                RGBMatrix *matrix = new RGBMatrix(doc);
                matrix->setName(QString::fromStdString(item["name"].get<std::string>()));
                if (item.contains("fixtureGroupID"))
                    matrix->setFixtureGroup(item["fixtureGroupID"].get<int>());
                if (item.contains("startColor"))
                    matrix->setColor(0, QColor(QString::fromStdString(item["startColor"].get<std::string>())));
                if (item.contains("endColor"))
                    matrix->setColor(1, QColor(QString::fromStdString(item["endColor"].get<std::string>())));
                doc->addFunction(matrix);
                results.push_back({{"id", (int)matrix->id()}, {"name", matrix->name().toStdString()}});
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("Create RGB matrix color animations. Batch: pass multiple in 'items'."),
        std::nullopt
    ));

    // delete_functions (batch)
    tm.register_tool(Tool(
        "delete_functions",
        Json{{"type", "object"}, {"properties", {
            {"ids", {{"type", "array"}, {"items", {{"type", "integer"}}}, {"description", "Function IDs to delete"}}}
        }}, {"required", {"ids"}}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            Json results = Json::array();
            for (auto &fid : args["ids"])
            {
                int id = fid.get<int>();
                bool ok = doc->deleteFunction(id);
                results.push_back({{"id", id}, {"status", ok ? "deleted" : "not found"}});
            }
            return results.dump();
            });
        },
        std::nullopt,
        std::string("Delete functions by ID. Batch."),
        std::nullopt
    ));
}
