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
#include "conversions.h"
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
#include "huematrix.h"
#include "rgbalgorithm.h"
#include "rgbscriptv4.h"
#include "rgbtext.h"
#include "rgbimage.h"
#include "fixturegroup.h"
#include "scriptv4.h"
#include "scenevalue.h"
#include "inputoutputmap.h"
#include "mastertimer.h"
#include "universe.h"

#include <QRegularExpression>
#include <cmath>
#include <functional>
#include <map>
#include <memory>
#include <fastmcpp/tools/manager.hpp>
#include <fastmcpp/tools/tool.hpp>

namespace {

const char *const kRegistryBusy =
    "Functions cannot be created or deleted while any function is running, starting or queued";

// Q8 error record with a machine-readable refusal code.
nlohmann::json refusal(size_t index, const char *code, const std::string &message)
{
    nlohmann::json rec = mcp::itemError(index, message);
    rec["code"] = code;
    return rec;
}

nlohmann::json invalid(size_t index, const std::string &message)
{
    return refusal(index, "invalid", message);
}

// Sequences whose steps are bound to this Scene, in id order
QList<quint32> sequenceReferrers(Doc *doc, quint32 sceneId)
{
    QList<quint32> ids;
    for (Function *f : doc->functions())
    {
        Sequence *seq = qobject_cast<Sequence*>(f);
        if (seq && seq->boundSceneID() == sceneId)
            ids.append(seq->id());
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

bool isSequenceBound(Doc *doc, quint32 sceneId)
{
    return !sequenceReferrers(doc, sceneId).isEmpty();
}

// Sorted (fixture, channel) set; Sequence step values are aligned to it
QList<QPair<quint32, quint32>> channelSet(const Scene *scene)
{
    QList<QPair<quint32, quint32>> set;
    for (const SceneValue &sv : scene->values())
        set.append({ sv.fxi, sv.channel });
    std::sort(set.begin(), set.end());
    return set;
}

// Commits one fully validated, detached staging function (owned here). A new
// function is added under registry admission (C5); an upsert copies the staged
// content onto the existing function under target+referrer admission, so a
// refused or invalid item never touches the Doc.
nlohmann::json commitStaged(Doc *doc, size_t index, Function *existing, Function *staged)
{
    std::unique_ptr<Function> owned(staged);
    MasterTimer *timer = doc->masterTimer();
    if (existing == nullptr)
    {
        MasterTimer::EditAdmission admission(timer, timer->beginRegistryEdit());
        if (!admission)
            return refusal(index, "functions_running", kRegistryBusy);
        const bool added = doc->addFunction(staged);
        admission.end();
        if (!added)
            return refusal(index, "invalid", "Function could not be added");
        owned.release();
        return mcp::itemOk(index, {{"id", staged->id()}, {"name", staged->name().toStdString()}, {"outcome", "created"}});
    }
    MasterTimer::EditAdmission admission(timer, timer->beginFunctionEdit(existing));
    if (!admission)
        return refusal(index, "running", "Function " + std::to_string(existing->id()) +
                       " or a function using it is running, starting or flashing");
    existing->copyFrom(staged);
    admission.end();
    return mcp::itemOk(index, {{"id", existing->id()}, {"name", existing->name().toStdString()}, {"outcome", "updated"}});
}


// Resolves one ordered detail/update target: {id} or {name[, type]}. A name
// resolves only when it is unique (after the optional type filter).
nlohmann::json resolveTarget(Doc *doc, size_t index, const nlohmann::json &target, Function *&out)
{
    out = nullptr;
    if (!target.is_object())
        return invalid(index, "target must be an object");
    std::string err = validateFields(target, {"id", "name", "type"});
    if (!err.empty())
        return invalid(index, nlohmann::json::parse(err).value("error", err));
    if (target.contains("id") == target.contains("name"))
        return invalid(index, "target needs exactly one of id or name");
    if (target.contains("id"))
    {
        if (target.contains("type"))
            return invalid(index, "type only narrows a name target");
        auto id = mcp::jsonInteger(target.at("id"), 0, mcp::kMaxId);
        if (!id)
            return invalid(index, mcp::integerError("id", 0, mcp::kMaxId));
        out = doc->function(quint32(*id));
        return out ? nlohmann::json() : refusal(index, "not_found", "No function with id " + std::to_string(*id));
    }
    if (!target.at("name").is_string())
        return invalid(index, "name must be a string");
    Function::Type type = Function::Undefined;
    if (target.contains("type"))
    {
        if (!target.at("type").is_string())
            return invalid(index, "type must be a string");
        type = Function::stringToType(QString::fromStdString(target.at("type").get<std::string>()));
        if (type == Function::Undefined)
            return invalid(index, "unknown function type");
    }
    const QString name = QString::fromStdString(target.at("name").get<std::string>());
    QList<Function*> matches;
    for (Function *f : doc->functions())
        if (f->name() == name && (type == Function::Undefined || f->type() == type))
            matches.append(f);
    if (matches.isEmpty())
        return refusal(index, "not_found", "No function named " + name.toStdString());
    if (matches.size() > 1)
    {
        nlohmann::json rec = refusal(index, "ambiguous", "More than one function has this name; use id or type");
        rec["candidates"] = nlohmann::json::array();
        for (Function *f : matches)
            rec["candidates"].push_back({{"id", f->id()}, {"type", Function::typeToString(f->type()).toStdString()}});
        return rec;
    }
    out = matches.first();
    return nlohmann::json();
}

// Typed speed: {"unit":"ms"|"beats","value"} or the {"unit":"default"|"infinite"} sentinels
nlohmann::json speedJson(uint speed, bool beats)
{
    if (speed == Function::defaultSpeed())
        return {{"unit", "default"}};
    if (speed == Function::infiniteSpeed())
        return {{"unit", "infinite"}};
    if (beats)
        return {{"unit", "beats"}, {"value", speed / 1000.0}};
    return {{"unit", "ms"}, {"value", speed}};
}

nlohmann::json valuesJson(const QList<SceneValue> &values)
{
    nlohmann::json out = nlohmann::json::array();
    for (const SceneValue &sv : values)
        out.push_back({{"fixtureID", sv.fxi}, {"channel", sv.channel}, {"value", sv.value}});
    return out;
}

const char *runOrderName(Function::RunOrder order)
{
    switch (order)
    {
        case Function::SingleShot: return "single";
        case Function::PingPong: return "pingpong";
        case Function::Random: return "random";
        default: return "loop";
    }
}

const char *directionName(Function::Direction dir)
{
    return dir == Function::Backward ? "backward" : "forward";
}

const char *speedModeName(Chaser::SpeedMode mode)
{
    switch (mode)
    {
        case Chaser::Default: return "default";
        case Chaser::Common: return "common";
        default: return "perStep";
    }
}

// Spellings match what the create/update tools accept, so a detail record can be edited and sent back
nlohmann::json functionDetails(Doc *doc, Function *fn)
{
    const bool beats = fn->tempoType() == Function::Beats;
    nlohmann::json d = {{"id", fn->id()}, {"name", fn->name().toStdString()},
                        {"type", Function::typeToString(fn->type()).toStdString()},
                        {"path", fn->path(true).toStdString()},
                        {"tempoType", beats ? "beats" : "time"},
                        {"fadeIn", speedJson(fn->fadeInSpeed(), beats)},
                        {"fadeOut", speedJson(fn->fadeOutSpeed(), beats)},
                        {"duration", speedJson(fn->duration(), beats)},
                        {"visible", fn->isVisible()},
                        {"blendMode", Universe::blendModeToString(fn->blendMode()).toStdString()}};
    switch (fn->type())
    {
        case Function::SceneType:
        {
            Scene *scene = qobject_cast<Scene*>(fn);
            d["values"] = valuesJson(scene->values());
            d["fixtureIDs"] = nlohmann::json::array();
            for (quint32 id : scene->fixtures())
                d["fixtureIDs"].push_back(id);
            d["paletteIDs"] = nlohmann::json::array();
            for (quint32 id : scene->palettes())
                d["paletteIDs"].push_back(id);
            d["fixtureGroupIDs"] = nlohmann::json::array();
            for (quint32 id : scene->fixtureGroups())
                d["fixtureGroupIDs"].push_back(id);
            d["channelGroups"] = nlohmann::json::array();
            const QList<quint32> groups = scene->channelGroups();
            const QList<uchar> levels = scene->channelGroupsLevels();
            for (int i = 0; i < groups.size() && i < levels.size(); ++i)
                d["channelGroups"].push_back({{"id", groups.at(i)}, {"level", levels.at(i)}});
            // Sequence playback and editing write step values into the bound
            // Scene; the originally authored values are not kept anywhere.
            d["valuesAuthoritative"] = !isSequenceBound(doc, scene->id());
            break;
        }
        case Function::ChaserType:
        case Function::SequenceType:
        {
            Chaser *chaser = qobject_cast<Chaser*>(fn);
            const bool isSequence = fn->type() == Function::SequenceType;
            d["runOrder"] = runOrderName(chaser->runOrder());
            d["direction"] = directionName(chaser->direction());
            d["fadeInMode"] = speedModeName(chaser->fadeInMode());
            d["fadeOutMode"] = speedModeName(chaser->fadeOutMode());
            d["durationMode"] = speedModeName(chaser->durationMode());
            if (isSequence)
                d["boundSceneID"] = qobject_cast<Sequence*>(fn)->boundSceneID();
            d["steps"] = nlohmann::json::array();
            for (const ChaserStep &step : chaser->steps())
            {
                nlohmann::json js = {{"fadeIn", speedJson(step.fadeIn, beats)},
                                     {"hold", speedJson(step.hold, beats)},
                                     {"fadeOut", speedJson(step.fadeOut, beats)},
                                     {"note", step.note.toStdString()}};
                if (isSequence)
                    js["values"] = valuesJson(step.values);
                else
                    js["functionID"] = step.fid;
                d["steps"].push_back(js);
            }
            break;
        }
        case Function::EFXType:
        {
            EFX *efx = qobject_cast<EFX*>(fn);
            d["algorithm"] = EFX::algorithmToString(efx->algorithm()).toStdString();
            d["width"] = efx->width();
            d["height"] = efx->height();
            d["rotation"] = efx->rotation();
            d["xOffset"] = efx->xOffset();
            d["yOffset"] = efx->yOffset();
            d["xFrequency"] = efx->xFrequency();
            d["yFrequency"] = efx->yFrequency();
            d["xPhase"] = efx->xPhase();
            d["yPhase"] = efx->yPhase();
            d["startOffset"] = efx->startOffset();
            d["isRelative"] = efx->isRelative();
            d["dimmerControl"] = efx->dimmerControlEnabled();
            d["propagationMode"] = EFX::propagationModeToString(efx->propagationMode()).toStdString();
            d["runOrder"] = runOrderName(efx->runOrder());
            d["direction"] = directionName(efx->direction());
            d["fixtures"] = nlohmann::json::array();
            for (const EFXFixture *ef : efx->fixtures())
                d["fixtures"].push_back({{"fixtureID", ef->head().fxi}, {"head", ef->head().head},
                                         {"direction", directionName(ef->direction())},
                                         {"startOffset", ef->startOffset()},
                                         {"mode", EFXFixture::modeToString(ef->mode()).toStdString()}});
            break;
        }
        case Function::CollectionType:
            d["functionIDs"] = nlohmann::json::array();
            for (quint32 id : qobject_cast<Collection*>(fn)->functions())
                d["functionIDs"].push_back(id);
            break;
        case Function::ScriptType:
            d["content"] = qobject_cast<Script*>(fn)->data().toStdString();
            break;
        case Function::RGBMatrixType:
        case Function::HUEMatrixType: d["detailTool"] = "query_rgb_matrices"; break;
        case Function::ShowType: d["detailTool"] = "query_shows"; break;
        default: break;
    }
    return d;
}
// Parses a typed speed for the new APIs. The unit must match the function's
// final tempo; "default" and "infinite" are sentinels without a value.
std::string parseSpeed(const nlohmann::json &v, const std::string &field, bool beats, uint &out)
{
    if (!v.is_object() || !v.contains("unit") || !v.at("unit").is_string())
        return field + " must be an object with a string unit";
    for (auto it = v.begin(); it != v.end(); ++it)
        if (it.key() != "unit" && it.key() != "value")
            return field + " has unknown field " + it.key();
    const std::string unit = v.at("unit").get<std::string>();
    if (unit == "default" || unit == "infinite")
    {
        if (v.contains("value"))
            return field + " " + unit + " takes no value";
        out = unit == "default" ? Function::defaultSpeed() : Function::infiniteSpeed();
        return std::string();
    }
    if (unit != "ms" && unit != "beats")
        return field + ".unit must be ms, beats, default or infinite";
    if (!v.contains("value"))
        return field + " needs a value";
    if ((unit == "beats") != beats)
        return field + " unit " + unit + " does not match the function tempo (" + (beats ? "beats" : "time") + ")";
    const int64_t maxMs = int64_t(Function::infiniteSpeed()) - 1;
    if (unit == "ms")
    {
        auto ms = mcp::jsonInteger(v.at("value"), 0, maxMs);
        if (!ms)
            return mcp::integerError(field + ".value", 0, maxMs);
        out = uint(*ms);
        return std::string();
    }
    if (!v.at("value").is_number() || v.at("value").get<double>() < 0)
        return field + ".value must be a non-negative number of beats";
    const double milli = v.at("value").get<double>() * 1000.0;
    if (milli != std::floor(milli) || milli > double(maxMs))
        return field + ".value must be a multiple of 0.001 beats in range";
    out = uint(milli);
    return std::string();
}

using StagedOp = std::function<void(Function*)>;

std::string parseIntField(const nlohmann::json &item, const char *field, int64_t lo, int64_t hi, int &out)
{
    auto v = mcp::jsonInteger(item.at(field), lo, hi);
    if (!v)
        return mcp::integerError(field, lo, hi);
    out = int(*v);
    return std::string();
}

// Universe::blendModeToString spellings, in enum order
const QStringList kBlendModes = { "Normal", "Mask", "Additive", "Subtractive" };

// Fields update_functions accepts beyond the common ones, per type
QStringList typeFields(Function::Type type)
{
    switch (type)
    {
        case Function::SceneType: return { "values", "fixtureIDs", "paletteIDs", "fixtureGroupIDs", "channelGroups" };
        case Function::ChaserType: return { "runOrder", "direction", "fadeInMode", "fadeOutMode", "durationMode", "steps" };
        case Function::SequenceType: return { "runOrder", "direction", "fadeInMode", "fadeOutMode", "durationMode", "steps" };
        case Function::EFXType: return { "algorithm", "width", "height", "rotation", "xOffset", "yOffset",
                                         "xFrequency", "yFrequency", "xPhase", "yPhase", "startOffset",
                                         "isRelative", "propagationMode", "runOrder", "direction", "fixtures",
                                         "dimmerControl" };
        case Function::CollectionType: return { "functionIDs" };
        case Function::ScriptType: return { "content" };
        default: return {};
    }
}

std::string parseFunctionIDs(Doc *doc, const nlohmann::json &list, const std::string &field, quint32 self,
                             QList<quint32> &out)
{
    if (!list.is_array())
        return field + " must be an array";
    for (const auto &v : list)
    {
        auto id = mcp::jsonInteger(v, 0, mcp::kMaxId);
        if (!id)
            return mcp::integerError(field + " entry", 0, mcp::kMaxId);
        if (doc->function(quint32(*id)) == nullptr)
            return field + ": function " + std::to_string(*id) + " does not exist";
        if (quint32(*id) == self)
            return field + ": a function cannot contain itself";
        out.append(quint32(*id));
    }
    const std::string cycle = mcp::functionCycleError(doc, self, out);
    return cycle.empty() ? cycle : field + ": " + cycle;
}

// Validates every type-specific field of an update item and returns the
// operations to apply to a staged copy; nothing is touched here.
std::string parseTypeFields(Doc *doc, Function *fn, const nlohmann::json &item, bool beats, std::vector<StagedOp> &ops)
{
    if (item.contains("runOrder"))
    {
        static const std::map<std::string, Function::RunOrder> orders = {
            {"loop", Function::Loop}, {"single", Function::SingleShot},
            {"pingpong", Function::PingPong}, {"random", Function::Random}};
        const auto &v = item.at("runOrder");
        if (!v.is_string() || !orders.count(v.get<std::string>()) ||
            (fn->type() == Function::EFXType && v == "random"))
            return fn->type() == Function::EFXType ? "runOrder must be loop, single or pingpong"
                                                   : "runOrder must be loop, single, pingpong or random";
        const Function::RunOrder order = orders.at(v.get<std::string>());
        ops.push_back([order](Function *f) { f->setRunOrder(order); });
    }
    if (item.contains("direction"))
    {
        const auto &v = item.at("direction");
        if (v != "forward" && v != "backward")
            return "direction must be forward or backward";
        const Function::Direction dir = v == "backward" ? Function::Backward : Function::Forward;
        ops.push_back([dir](Function *f) { f->setDirection(dir); });
    }
    for (const char *field : { "fadeInMode", "fadeOutMode", "durationMode" })
    {
        if (!item.contains(field))
            continue;
        const auto &v = item.at(field);
        if (v != "default" && v != "common" && v != "perStep")
            return std::string(field) + " must be default, common or perStep";
        const Chaser::SpeedMode mode = v == "default" ? Chaser::Default : v == "common" ? Chaser::Common : Chaser::PerStep;
        const std::string name(field);
        ops.push_back([mode, name](Function *f) {
            Chaser *c = qobject_cast<Chaser*>(f);
            if (name == "fadeInMode") c->setFadeInMode(mode);
            else if (name == "fadeOutMode") c->setFadeOutMode(mode);
            else c->setDurationMode(mode);
        });
    }

    switch (fn->type())
    {
        case Function::SceneType:
        {
            if (item.contains("values"))
            {
                const auto &list = item.at("values");
                if (!list.is_array())
                    return "values must be an array";
                QList<SceneValue> values;
                for (const auto &v : list)
                {
                    if (!v.is_object() || !v.contains("fixtureID") || !v.contains("channel") || !v.contains("value"))
                        return "each value needs fixtureID, channel and value";
                    auto fxi = mcp::jsonInteger(v.at("fixtureID"), 0, mcp::kMaxId);
                    if (!fxi)
                        return mcp::integerError("fixtureID", 0, mcp::kMaxId);
                    Fixture *fixture = doc->fixture(quint32(*fxi));
                    if (fixture == nullptr)
                        return "fixture " + std::to_string(*fxi) + " does not exist";
                    const int64_t last = int64_t(fixture->channels()) - 1;
                    auto ch = mcp::jsonInteger(v.at("channel"), 0, last);
                    if (!ch)
                        return mcp::integerError("channel", 0, last);
                    auto dmx = mcp::jsonInteger(v.at("value"), 0, 255);
                    if (!dmx)
                        return mcp::integerError("value", 0, 255);
                    const SceneValue sv{quint32(*fxi), quint32(*ch), uchar(*dmx)};
                    if (values.contains(sv))
                        return "values repeat fixture " + std::to_string(*fxi) + " channel " + std::to_string(*ch);
                    values.append(sv);
                }
                ops.push_back([values](Function *f) {
                    Scene *scene = qobject_cast<Scene*>(f);
                    for (const SceneValue &old : scene->values())
                        scene->unsetValue(old.fxi, old.channel);
                    for (const SceneValue &sv : values)
                        scene->setValue(sv);
                });
            }
            // Membership is separate from values: palette-only fixtures carry no values
            if (item.contains("fixtureIDs"))
            {
                const auto &list = item.at("fixtureIDs");
                if (!list.is_array())
                    return "fixtureIDs must be an array";
                QList<quint32> ids;
                for (const auto &v : list)
                {
                    auto id = mcp::jsonInteger(v, 0, mcp::kMaxId);
                    if (!id)
                        return mcp::integerError("fixtureIDs entry", 0, mcp::kMaxId);
                    if (doc->fixture(quint32(*id)) == nullptr)
                        return "fixture " + std::to_string(*id) + " does not exist";
                    if (ids.contains(quint32(*id)))
                        return "fixtureIDs repeat " + std::to_string(*id);
                    ids.append(quint32(*id));
                }
                QList<quint32> valued;
                if (item.contains("values"))
                    for (const auto &v : item.at("values"))
                        valued.append(quint32(v.at("fixtureID").get<int64_t>()));
                else
                    for (const SceneValue &sv : qobject_cast<Scene*>(fn)->values())
                        valued.append(sv.fxi);
                for (quint32 fxi : valued)
                    if (!ids.contains(fxi))
                        return "fixtureIDs must include fixture " + std::to_string(fxi) + ", which has values";
                ops.push_back([ids](Function *f) {
                    Scene *scene = qobject_cast<Scene*>(f);
                    for (quint32 fxi : scene->fixtures())
                        scene->removeFixture(fxi);
                    for (quint32 fxi : ids)
                        scene->addFixture(fxi);
                });
            }
            if (item.contains("paletteIDs"))
            {
                const auto &list = item.at("paletteIDs");
                if (!list.is_array())
                    return "paletteIDs must be an array";
                QList<quint32> ids;
                for (const auto &v : list)
                {
                    auto id = mcp::jsonInteger(v, 0, mcp::kMaxId);
                    if (!id)
                        return mcp::integerError("paletteIDs entry", 0, mcp::kMaxId);
                    if (doc->palette(quint32(*id)) == nullptr)
                        return "palette " + std::to_string(*id) + " does not exist";
                    if (ids.contains(quint32(*id)))
                        return "paletteIDs repeat " + std::to_string(*id);
                    ids.append(quint32(*id));
                }
                ops.push_back([ids](Function *f) {
                    Scene *scene = qobject_cast<Scene*>(f);
                    for (quint32 id : scene->palettes())
                        scene->removePalette(id);
                    for (quint32 id : ids)
                        scene->addPalette(id);
                });
            }
            if (item.contains("fixtureGroupIDs"))
            {
                const auto &list = item.at("fixtureGroupIDs");
                if (!list.is_array())
                    return "fixtureGroupIDs must be an array";
                QList<quint32> ids;
                for (const auto &v : list)
                {
                    auto id = mcp::jsonInteger(v, 0, mcp::kMaxId);
                    if (!id)
                        return mcp::integerError("fixtureGroupIDs entry", 0, mcp::kMaxId);
                    if (doc->fixtureGroup(quint32(*id)) == nullptr)
                        return "fixture group " + std::to_string(*id) + " does not exist";
                    if (ids.contains(quint32(*id)))
                        return "fixtureGroupIDs repeat " + std::to_string(*id);
                    ids.append(quint32(*id));
                }
                ops.push_back([ids](Function *f) {
                    Scene *scene = qobject_cast<Scene*>(f);
                    for (quint32 id : scene->fixtureGroups())
                        scene->removeFixtureGroup(id);
                    for (quint32 id : ids)
                        scene->addFixtureGroup(id);
                });
            }
            if (item.contains("channelGroups"))
            {
                const auto &list = item.at("channelGroups");
                if (!list.is_array())
                    return "channelGroups must be an array";
                QList<QPair<quint32, uchar>> groups;
                QList<quint32> seen;
                for (const auto &v : list)
                {
                    if (!v.is_object() || !v.contains("id") || !v.contains("level") || v.size() != 2)
                        return "each channel group needs exactly id and level";
                    auto id = mcp::jsonInteger(v.at("id"), 0, mcp::kMaxId);
                    if (!id)
                        return mcp::integerError("channel group id", 0, mcp::kMaxId);
                    if (doc->channelsGroup(quint32(*id)) == nullptr)
                        return "channel group " + std::to_string(*id) + " does not exist";
                    auto level = mcp::jsonInteger(v.at("level"), 0, 255);
                    if (!level)
                        return mcp::integerError("channel group level", 0, 255);
                    if (seen.contains(quint32(*id)))
                        return "channelGroups repeat " + std::to_string(*id);
                    seen.append(quint32(*id));
                    groups.append({quint32(*id), uchar(*level)});
                }
                ops.push_back([groups](Function *f) {
                    Scene *scene = qobject_cast<Scene*>(f);
                    for (quint32 id : scene->channelGroups())
                        scene->removeChannelGroup(id);
                    for (const auto &g : groups)
                    {
                        scene->addChannelGroup(g.first);
                        scene->setChannelGroupLevel(g.first, g.second);
                    }
                });
            }
            break;
        }
        case Function::ChaserType:
        case Function::SequenceType:
        {
            if (!item.contains("steps"))
                break;
            const auto &list = item.at("steps");
            if (!list.is_array())
                return "steps must be an array";
            const bool isSequence = fn->type() == Function::SequenceType;
            QList<SceneValue> channels;
            quint32 boundId = Function::invalidId();
            if (isSequence)
            {
                boundId = qobject_cast<Sequence*>(fn)->boundSceneID();
                Scene *bound = qobject_cast<Scene*>(doc->function(boundId));
                if (bound == nullptr)
                    return "bound function " + std::to_string(boundId) + " is not a Scene";
                channels = bound->values();
                std::sort(channels.begin(), channels.end());
            }
            const char *idField = isSequence ? "values" : "functionID";
            QList<ChaserStep> steps;
            for (const auto &js : list)
            {
                if (!js.is_object())
                    return "each step must be an object";
                for (auto it = js.begin(); it != js.end(); ++it)
                    if (it.key() != idField && it.key() != "fadeIn" && it.key() != "hold" &&
                        it.key() != "fadeOut" && it.key() != "note")
                        return "step has unknown field " + it.key();
                if (!js.contains(idField))
                    return std::string("each step needs ") + idField;
                std::string err;
                ChaserStep step(boundId, 0, 0, 0);
                if (isSequence)
                {
                    // A Sequence step carries exactly the bound Scene's channels, sorted
                    const auto &vals = js.at("values");
                    if (!vals.is_array() || vals.size() != size_t(channels.size()))
                        return "step values must list each of the " + std::to_string(channels.size()) +
                               " bound Scene channels exactly once";
                    step.values = channels;
                    QList<bool> seen(channels.size(), false);
                    for (const auto &v : vals)
                    {
                        if (!v.is_object() || !v.contains("fixtureID") || !v.contains("channel") || !v.contains("value"))
                            return "each step value needs fixtureID, channel and value";
                        auto fxi = mcp::jsonInteger(v.at("fixtureID"), 0, mcp::kMaxId);
                        auto ch = mcp::jsonInteger(v.at("channel"), 0, mcp::kMaxId);
                        auto dmx = mcp::jsonInteger(v.at("value"), 0, 255);
                        if (!fxi || !ch)
                            return "step value fixtureID and channel must be non-negative integers";
                        if (!dmx)
                            return mcp::integerError("step value", 0, 255);
                        const int at = channels.indexOf(SceneValue(quint32(*fxi), quint32(*ch)));
                        if (at < 0)
                            return "fixture " + std::to_string(*fxi) + " channel " + std::to_string(*ch) +
                                   " is not in the bound Scene";
                        if (seen[at])
                            return "step values repeat fixture " + std::to_string(*fxi) + " channel " + std::to_string(*ch);
                        seen[at] = true;
                        step.values[at].value = uchar(*dmx);
                    }
                }
                else
                {
                    QList<quint32> fid;
                    err = parseFunctionIDs(doc, nlohmann::json::array({js.at("functionID")}), "step functionID", fn->id(), fid);
                    if (!err.empty())
                        return err;
                    step.fid = fid.first();
                }
                if (js.contains("fadeIn") && !(err = parseSpeed(js.at("fadeIn"), "step fadeIn", beats, step.fadeIn)).empty())
                    return err;
                if (js.contains("hold") && !(err = parseSpeed(js.at("hold"), "step hold", beats, step.hold)).empty())
                    return err;
                if (js.contains("fadeOut") && !(err = parseSpeed(js.at("fadeOut"), "step fadeOut", beats, step.fadeOut)).empty())
                    return err;
                if (js.contains("note"))
                {
                    if (!js.at("note").is_string())
                        return "step note must be a string";
                    step.note = QString::fromStdString(js.at("note").get<std::string>());
                }
                steps.append(step);
            }
            ops.push_back([steps](Function *f) {
                Chaser *chaser = qobject_cast<Chaser*>(f);
                while (chaser->stepsCount() > 0)
                    chaser->removeStep(chaser->stepsCount() - 1);
                for (const ChaserStep &step : steps)
                    chaser->addStep(step);
            });
            break;
        }
        case Function::EFXType:
        {
            if (item.contains("algorithm"))
            {
                static const QStringList algos = EFX::algorithmList();
                const auto &v = item.at("algorithm");
                if (!v.is_string() || !algos.contains(QString::fromStdString(v.get<std::string>())))
                    return "algorithm must be one of " + algos.join(", ").toStdString();
                const EFX::Algorithm algo = EFX::stringToAlgorithm(QString::fromStdString(v.get<std::string>()));
                ops.push_back([algo](Function *f) { qobject_cast<EFX*>(f)->setAlgorithm(algo); });
            }
            if (item.contains("propagationMode"))
            {
                const auto &v = item.at("propagationMode");
                if (v != "Parallel" && v != "Serial" && v != "Asymmetric")
                    return "propagationMode must be Parallel, Serial or Asymmetric";
                const EFX::PropagationMode mode = EFX::stringToPropagationMode(QString::fromStdString(v.get<std::string>()));
                ops.push_back([mode](Function *f) { qobject_cast<EFX*>(f)->setPropagationMode(mode); });
            }
            if (item.contains("isRelative"))
            {
                if (!item.at("isRelative").is_boolean())
                    return "isRelative must be a boolean";
                const bool rel = item.at("isRelative").get<bool>();
                ops.push_back([rel](Function *f) { qobject_cast<EFX*>(f)->setIsRelative(rel); });
            }
            if (item.contains("dimmerControl"))
            {
                if (!item.at("dimmerControl").is_boolean())
                    return "dimmerControl must be a boolean";
                const bool dimmer = item.at("dimmerControl").get<bool>();
                ops.push_back([dimmer](Function *f) { qobject_cast<EFX*>(f)->setDimmerControlEnabled(dimmer); });
            }
            using Setter = void (EFX::*)(int);
            struct Range { const char *field; int64_t lo, hi; Setter set; };
            static const Range ranges[] = {
                {"width", 0, 127, &EFX::setWidth}, {"height", 0, 127, &EFX::setHeight},
                {"rotation", 0, 359, &EFX::setRotation}, {"xOffset", 0, 255, &EFX::setXOffset},
                {"yOffset", 0, 255, &EFX::setYOffset}, {"xFrequency", 0, 32, &EFX::setXFrequency},
                {"yFrequency", 0, 32, &EFX::setYFrequency}, {"xPhase", 0, 359, &EFX::setXPhase},
                {"yPhase", 0, 359, &EFX::setYPhase}, {"startOffset", 0, 359, &EFX::setStartOffset}};
            for (const Range &r : ranges)
            {
                if (!item.contains(r.field))
                    continue;
                int value = 0;
                std::string err = parseIntField(item, r.field, r.lo, r.hi, value);
                if (!err.empty())
                    return err;
                const Setter set = r.set;
                ops.push_back([set, value](Function *f) { (qobject_cast<EFX*>(f)->*set)(value); });
            }
            if (item.contains("fixtures"))
            {
                const auto &list = item.at("fixtures");
                if (!list.is_array())
                    return "fixtures must be an array";
                struct Entry { GroupHead head; Function::Direction dir; int offset; EFXFixture::Mode mode; };
                std::vector<Entry> entries;
                QSet<QString> seen;
                for (size_t i = 0; i < list.size(); ++i)
                {
                    const auto &e = list[i];
                    const std::string at = "fixtures[" + std::to_string(i) + "]";
                    if (!e.is_object())
                        return at + " must be an object";
                    auto err = validateFields(e, {"fixtureID", "head", "direction", "startOffset", "mode"});
                    if (!err.empty())
                        return at + ": " + err;
                    for (const char *key : { "fixtureID", "head", "direction", "startOffset", "mode" })
                        if (!e.contains(key))
                            return at + ": " + key + " is required";
                    const auto fxiValue = mcp::jsonInteger(e.at("fixtureID"), 0, mcp::kMaxId);
                    if (!fxiValue)
                        return at + ": " + mcp::integerError("fixtureID", 0, mcp::kMaxId);
                    const quint32 fxi = quint32(*fxiValue);
                    int head = 0, offset = 0;
                    if (!(err = parseIntField(e, "head", 0, INT_MAX, head)).empty() ||
                        !(err = parseIntField(e, "startOffset", 0, 359, offset)).empty())
                        return at + ": " + err;
                    Fixture *fixture = doc->fixture(fxi);
                    if (fixture == nullptr)
                        return at + ": fixture " + std::to_string(fxi) + " not found";
                    if (head >= fixture->heads())
                        return at + ": head must be below " + std::to_string(fixture->heads());
                    const auto &dir = e.value("direction", nlohmann::json());
                    if (dir != "forward" && dir != "backward")
                        return at + ": direction must be forward or backward";
                    EFXFixture probe(qobject_cast<EFX*>(fn));
                    probe.setHead(GroupHead(fxi, head));
                    const QStringList modes = probe.modeList();
                    const auto &mode = e.value("mode", nlohmann::json());
                    if (!mode.is_string() || !modes.contains(QString::fromStdString(mode.get<std::string>())))
                        return at + ": mode must be one of " + modes.join(", ").toStdString() + " for this head";
                    if (seen.contains(QString("%1/%2/%3").arg(fxi).arg(head).arg(QString::fromStdString(mode.get<std::string>()))))
                        return at + ": duplicate fixture, head and mode";
                    seen.insert(QString("%1/%2/%3").arg(fxi).arg(head).arg(QString::fromStdString(mode.get<std::string>())));
                    entries.push_back({GroupHead(fxi, head),
                                       dir == "backward" ? Function::Backward : Function::Forward, offset,
                                       EFXFixture::stringToMode(QString::fromStdString(mode.get<std::string>()))});
                }
                ops.push_back([entries](Function *f) {
                    EFX *efx = qobject_cast<EFX*>(f);
                    efx->removeAllFixtures();
                    for (const Entry &en : entries)
                    {
                        EFXFixture *ef = new EFXFixture(efx);
                        ef->setHead(en.head);
                        ef->setDirection(en.dir);
                        ef->setStartOffset(en.offset);
                        ef->setMode(en.mode);
                        efx->addFixture(ef);
                    }
                });
            }
            break;
        }
        case Function::CollectionType:
        {
            if (!item.contains("functionIDs"))
                break;
            QList<quint32> ids;
            std::string err = parseFunctionIDs(doc, item.at("functionIDs"), "functionIDs", fn->id(), ids);
            if (!err.empty())
                return err;
            for (int i = 0; i < ids.size(); ++i)
                if (ids.indexOf(ids[i]) != i)
                    return "functionIDs repeat " + std::to_string(ids[i]);
            ops.push_back([ids](Function *f) {
                Collection *col = qobject_cast<Collection*>(f);
                for (quint32 id : col->functions())
                    col->removeFunction(id);
                for (quint32 id : ids)
                    col->addFunction(id);
            });
            break;
        }
        case Function::ScriptType:
        {
            if (!item.contains("content"))
                break;
            if (!item.at("content").is_string())
                return "content must be a string";
            const QString content = QString::fromStdString(item.at("content").get<std::string>());
            const QStringList errors = Script::syntaxErrors(content);
            if (!errors.isEmpty())
                return "content has syntax errors: " + errors.join("; ").toStdString();
            ops.push_back([content](Function *f) { qobject_cast<Script*>(f)->setData(content); });
            break;
        }
        default:
            break;
    }
    return std::string();
}

} // namespace

void registerFunctionTools(fastmcpp::tools::ToolManager &tm, Doc *doc, FunctionManager *funcMgr)
{
    using Json = nlohmann::json;
    using Tool = fastmcpp::tools::Tool;

    // create_scenes (batch)
    tm.register_tool(Tool(
        "create_scenes",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"type", {{"type", "string"}, {"enum", {"RGBMatrix", "HUEMatrix"}}, {"description", "HUEMatrix (default) supports HSV/audio scripts plus rotation, mirror and beat transforms. RGBMatrix is the plain upstream matrix."}}},
                {"name", {{"type", "string"}}},
                {"path", {{"type", "string"}, {"description", "Folder path (e.g. 'Phase1/Moods'). Creates folders implicitly."}}},
                {"fixtureIDs", {{"type", "array"}, {"items", {{"type", "integer"}}}}},
                {"fixtureNames", {{"type", "array"}, {"items", {{"type", "string"}}}, {"description", "Fixture name patterns (glob: * ?). Alternative to fixtureIDs."}}},
                {"paletteIDs", {{"type", "array"}, {"items", {{"type", "integer"}}}, {"description", "Palette IDs to reference. Palettes provide reusable values (color, position, dimmer)."}}},
                {"paletteNames", {{"type", "array"}, {"items", {{"type", "string"}}}, {"description", "Palette names to reference (glob patterns). Resolved from existing palettes."}}},
                {"fadeIn", {{"description", "Fade in: integer ms OR beat string ('1/8','1/4','1/2','1','2','4'). Beat strings auto-set tempoType to Beats."}}},
                {"fadeOut", {{"description", "Fade out: integer ms OR beat string. Beat strings auto-set tempoType to Beats."}}},
                {"tempoType", {{"type", "string"}, {"enum", {"time", "beats"}}, {"description", "Time or Beats. Auto-set to Beats when beat strings used."}}},
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
            auto itemsErr = validateItemsArray(args);
            if (itemsErr) return *itemsErr;
            Json results = Json::array();
            const Json &items = args.at("items");
            for (size_t index = 0; index < items.size(); ++index)
            {
                const Json &item = items[index];
                try {
                auto err = validateFields(item, {"name", "path", "fixtureIDs", "fixtureNames", "paletteIDs", "paletteNames", "fadeIn", "fadeOut", "tempoType", "channelValues", "positions"});
                if (err.empty())
                {
                    static const Json kEnums = {
                        {"tempoType", {{"enum", {"time", "beats"}}}}
                    };
                    err = validateEnums(item, kEnums);
                }
                if (!err.empty()) { results.push_back(invalid(index, Json::parse(err).value("error", err))); continue; }
                if (!item.contains("name") || !item.at("name").is_string())
                {
                    results.push_back(invalid(index, "name must be a string"));
                    continue;
                }

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
                std::string itemErr;
                if (item.contains("fixtureIDs"))
                {
                    for (auto &fxId : item.at("fixtureIDs"))
                    {
                        auto id = mcp::jsonInteger(fxId, 0, mcp::kMaxId);
                        if (!id || !doc->fixture(quint32(*id))) { itemErr = "fixtureIDs entries must be IDs of patched fixtures"; break; }
                        if (!fixtureIDs.contains(quint32(*id))) fixtureIDs.append(quint32(*id));
                    }
                }
                if (!itemErr.empty()) { results.push_back(invalid(index, itemErr)); continue; }

                auto timing = mcp::parseTimingFields(item, {"fadeIn", "fadeOut"});
                if (!timing.error.empty()) { results.push_back(invalid(index, timing.error)); continue; }

                QString name = QString::fromStdString(item.at("name").get<std::string>());
                Function *existing = mcp::findFunction(doc, name, Function::SceneType);
                std::unique_ptr<Scene> scene(new Scene(doc));
                if (existing)
                {
                    scene->copyFrom(existing);
                    scene->clear(); // Upsert replaces values, fixtures and palette refs
                }
                else
                    scene->setName(name);

                for (quint32 fxId : fixtureIDs)
                    scene->addFixture(fxId);

                if (item.contains("path"))
                    scene->setPath(QString::fromStdString(item.at("path").get<std::string>()));

                if (timing.useBeatMode)
                    scene->setTempoType(Function::Beats);
                else if (item.contains("tempoType"))
                    scene->setTempoType(Function::Time);
                if (timing.present.count("fadeIn")) scene->setFadeInSpeed(timing.values["fadeIn"]);
                if (timing.present.count("fadeOut")) scene->setFadeOutSpeed(timing.values["fadeOut"]);

                if (item.contains("paletteIDs"))
                {
                    for (auto &palId : item.at("paletteIDs"))
                    {
                        auto id = mcp::jsonInteger(palId, 0, mcp::kMaxId);
                        if (!id || !doc->palette(quint32(*id))) { itemErr = "paletteIDs entries must be existing palette IDs"; break; }
                        scene->addPalette(quint32(*id));
                    }
                }
                if (item.contains("paletteNames"))
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

                // Degree-based positions first so explicit channelValues can override
                if (itemErr.empty() && item.contains("positions"))
                {
                    for (auto &pos : item.at("positions"))
                    {
                        auto posErr = validateFields(pos, {"fixtureID", "panDegrees", "tiltDegrees", "zoomDegrees"});
                        if (!posErr.empty()) { itemErr = Json::parse(posErr).value("error", posErr); break; }
                        auto fxID = pos.contains("fixtureID") ? mcp::jsonInteger(pos.at("fixtureID"), 0, mcp::kMaxId) : std::nullopt;
                        Fixture *fxi = fxID ? doc->fixture(quint32(*fxID)) : nullptr;
                        if (!fxi) { itemErr = "positions[].fixtureID must be the ID of a patched fixture"; break; }

                        scene->addFixture(fxi->id());
                        if (pos.contains("panDegrees"))
                            for (const SceneValue &sv : fxi->positionToValues(QLCChannel::Pan, pos.at("panDegrees").get<float>()))
                                scene->setValue(sv);
                        if (pos.contains("tiltDegrees"))
                            for (const SceneValue &sv : fxi->positionToValues(QLCChannel::Tilt, pos.at("tiltDegrees").get<float>()))
                                scene->setValue(sv);
                        if (pos.contains("zoomDegrees"))
                            for (const SceneValue &sv : fxi->zoomToValues(pos.at("zoomDegrees").get<float>(), false))
                                scene->setValue(sv);
                    }
                }

                if (itemErr.empty() && item.contains("channelValues"))
                {
                    for (auto &cv : item.at("channelValues"))
                    {
                        auto cvErr = validateFields(cv, {"fixtureID", "channel", "value"});
                        if (!cvErr.empty()) { itemErr = Json::parse(cvErr).value("error", cvErr); break; }
                        auto fxID = cv.contains("fixtureID") ? mcp::jsonInteger(cv.at("fixtureID"), 0, mcp::kMaxId) : std::nullopt;
                        Fixture *fxi = fxID ? doc->fixture(quint32(*fxID)) : nullptr;
                        if (!fxi) { itemErr = "channelValues[].fixtureID must be the ID of a patched fixture"; break; }
                        auto ch = cv.contains("channel") ? mcp::jsonInteger(cv.at("channel"), 0, int64_t(fxi->channels()) - 1) : std::nullopt;
                        if (!ch) { itemErr = mcp::integerError("channelValues[].channel", 0, int64_t(fxi->channels()) - 1); break; }
                        auto value = cv.contains("value") ? mcp::jsonInteger(cv.at("value"), 0, 255) : std::nullopt;
                        if (!value) { itemErr = mcp::integerError("channelValues[].value", 0, 255); break; }
                        scene->setValue(SceneValue(fxi->id(), quint32(*ch), uchar(*value)));
                        scene->addFixture(fxi->id());
                    }
                }
                if (!itemErr.empty()) { results.push_back(invalid(index, itemErr)); continue; }

                if (existing && isSequenceBound(doc, existing->id()) &&
                    channelSet(qobject_cast<Scene*>(existing)) != channelSet(scene.get()))
                {
                    results.push_back(refusal(index, "bound_scene",
                        "Scene " + std::to_string(existing->id()) +
                        " is bound by a sequence; its channel set cannot change"));
                    continue;
                }

                const int paletteCount = scene->palettes().count();
                Json rec = commitStaged(doc, index, existing, scene.release());
                if (rec.value("status", "") == "ok" && paletteCount > 0)
                    rec["paletteCount"] = paletteCount;
                results.push_back(rec);
                } catch (const std::exception &e) {
                    results.push_back(invalid(index, e.what()));
                }
            }
            return results.dump();
            });
        }
    )
    .set_description("Create scenes with palettes, channel values, and/or degree-based positions. "
                     "Palette-first: reference palettes via paletteNames/paletteIDs for reusable values; "
                     "channelValues override palettes for fine-tuning. "
                     "Upserts: replaces all values and palette refs on existing scenes. Batch: wrap entries in {\"items\": [...]}.")
    .set_annotations(mcp::kAnnotIdempotent));

    // create_chasers (batch)
    tm.register_tool(Tool(
        "create_chasers",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"name", {{"type", "string"}}},
                {"path", {{"type", "string"}, {"description", "Folder path (e.g. 'Phase1/Chasers'). Creates folders implicitly."}}},
                {"steps", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                    {"functionID", {{"type", "integer"}}},
                    {"functionName", {{"type", "string"}, {"description", "Function name. Alternative to functionID."}}},
                    {"fadeIn", {{"type", "number"}, {"description", "Fade in: beats when tempoType='beats' (e.g. 2=2beats, 0.5=half beat, 0.25=quarter beat); milliseconds when tempoType='time'. Default 0"}}},
                    {"hold", {{"type", "number"}, {"description", "Hold: beats when tempoType='beats' (e.g. 4=4beats, 0.5=half beat); milliseconds when tempoType='time'. Default 0"}}},
                    {"fadeOut", {{"type", "number"}, {"description", "Fade out: beats when tempoType='beats'; milliseconds when tempoType='time'. Default 0"}}}
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
            auto itemsErr = validateItemsArray(args);
            if (itemsErr) return *itemsErr;
            Json results = Json::array();
            const Json &items = args.at("items");
            for (size_t index = 0; index < items.size(); ++index)
            {
                const Json &item = items[index];
                try {
                auto err = validateFields(item, {"name", "path", "steps", "tempoType", "runOrder", "direction", "fadeInMode", "fadeOutMode", "durationMode"});
                if (err.empty())
                {
                    static const Json kEnums = {
                        {"tempoType", {{"enum", {"time", "beats"}}}},
                        {"runOrder", {{"enum", {"loop", "single", "pingpong", "random"}}}},
                        {"direction", {{"enum", {"forward", "backward"}}}},
                        {"fadeInMode", {{"enum", {"default", "common", "perStep"}}}},
                        {"fadeOutMode", {{"enum", {"default", "common", "perStep"}}}},
                        {"durationMode", {{"enum", {"default", "common", "perStep"}}}}
                    };
                    err = validateEnums(item, kEnums);
                }
                if (!err.empty()) { results.push_back(invalid(index, Json::parse(err).value("error", err))); continue; }

                if (!item.contains("name") || !item.at("name").is_string() || !item.contains("steps") || !item.at("steps").is_array())
                {
                    results.push_back(invalid(index, "name and steps required"));
                    continue;
                }

                QString name = QString::fromStdString(item.at("name").get<std::string>());
                Function *existing = mcp::findFunction(doc, name, Function::ChaserType);
                std::unique_ptr<Chaser> chaser(new Chaser(doc));
                if (existing)
                    chaser->copyFrom(existing); // steps are replaced below
                else
                    chaser->setName(name);

                if (item.contains("path"))
                    chaser->setPath(QString::fromStdString(item.at("path").get<std::string>()));

                // Speed modes (default to perStep since steps carry their own timing)
                auto parseSpeedMode = [](const std::string &mode) -> Chaser::SpeedMode {
                    if (mode == "default") return Chaser::Default;
                    if (mode == "common") return Chaser::Common;
                    return Chaser::PerStep;
                };
                chaser->setFadeInMode(parseSpeedMode(item.value("fadeInMode", "perStep")));
                chaser->setFadeOutMode(parseSpeedMode(item.value("fadeOutMode", "perStep")));
                chaser->setDurationMode(parseSpeedMode(item.value("durationMode", "perStep")));

                QString order = QString::fromStdString(item.value("runOrder", "loop"));
                if (order == "single") chaser->setRunOrder(Function::SingleShot);
                else if (order == "pingpong") chaser->setRunOrder(Function::PingPong);
                else if (order == "random") chaser->setRunOrder(Function::Random);
                else chaser->setRunOrder(Function::Loop);

                QString dir = QString::fromStdString(item.value("direction", "forward"));
                if (dir == "backward") chaser->setDirection(Function::Backward);
                else chaser->setDirection(Function::Forward);

                QString tempo = QString::fromStdString(item.value("tempoType", "time"));
                if (tempo == "beats") chaser->setTempoType(Function::Beats);
                else chaser->setTempoType(Function::Time);
                const bool isBeatMode = (tempo == "beats");

                // Beat mode takes human-readable beats (2 = 2 beats, 0.5 = half beat);
                // the engine stores beats x 1000.
                const double scale = isBeatMode ? 1000.0 : 1.0;
                auto stepTime = [&](const Json &step, const char *field) -> std::optional<uint> {
                    if (!step.contains(field))
                        return 0u;
                    const Json &v = step.at(field);
                    if (!v.is_number() || !std::isfinite(v.get<double>()) || v.get<double>() < 0 ||
                        v.get<double>() * scale >= double(Function::infiniteSpeed()))
                        return std::nullopt;
                    return static_cast<uint>(v.get<double>() * scale);
                };

                std::string itemErr;
                QList<ChaserStep> steps;
                for (auto &step : item.at("steps"))
                {
                    auto stepErr = validateFields(step, {"functionID", "functionName", "fadeIn", "hold", "fadeOut"});
                    if (!stepErr.empty()) { itemErr = Json::parse(stepErr).value("error", stepErr); break; }
                    quint32 fid = Function::invalidId();
                    if (step.contains("functionID"))
                    {
                        auto id = mcp::jsonInteger(step.at("functionID"), 0, mcp::kMaxId);
                        if (id) fid = quint32(*id);
                    }
                    else if (step.contains("functionName") && step.at("functionName").is_string())
                        fid = mcp::resolveFunctionByName(doc, QString::fromStdString(step.at("functionName").get<std::string>()));
                    if (fid == Function::invalidId() || doc->function(fid) == nullptr)
                    { itemErr = "steps[] must reference an existing function by functionID or functionName"; break; }
                    if (existing && fid == existing->id())
                    { itemErr = "steps[] cannot reference the chaser itself"; break; }

                    auto fadeIn = stepTime(step, "fadeIn");
                    auto hold = stepTime(step, "hold");
                    auto fadeOut = stepTime(step, "fadeOut");
                    if (!fadeIn || !hold || !fadeOut)
                    { itemErr = "steps[] fadeIn/hold/fadeOut must be non-negative finite numbers"; break; }
                    steps.append(ChaserStep(fid, *fadeIn, *hold, *fadeOut));
                }
                if (itemErr.empty())
                {
                    QList<quint32> refs;
                    for (const ChaserStep &s : steps)
                        refs.append(s.fid);
                    itemErr = mcp::functionCycleError(doc, existing ? existing->id() : Function::invalidId(), refs);
                }
                if (!itemErr.empty()) { results.push_back(invalid(index, itemErr)); continue; }

                while (chaser->stepsCount() > 0)
                    chaser->removeStep(0);
                for (const ChaserStep &s : steps)
                    chaser->addStep(s);

                results.push_back(commitStaged(doc, index, existing, chaser.release()));
                } catch (const std::exception &e) {
                    results.push_back(invalid(index, e.what()));
                }
            }
            return results.dump();
            });
        }
    )
    .set_description("Create chasers with per-step timing. Upserts: replaces all steps on existing chasers. Batch: wrap entries in {\"items\": [...]}.")
    .set_annotations(mcp::kAnnotIdempotent));

    // create_sequences (batch)
    tm.register_tool(Tool(
        "create_sequences",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"name", {{"type", "string"}}},
                {"path", {{"type", "string"}, {"description", "Folder path (e.g. 'Phase1/Sequences'). Creates folders implicitly."}}},
                {"boundSceneID", {{"type", "integer"}, {"description", "Scene ID this sequence is bound to"}}},
                {"fadeIn", {{"description", "Fade in per step: integer ms OR beat string ('1/8','1/4','1/2','1','2','4'). Beat strings auto-set tempoType to Beats."}}},
                {"fadeOut", {{"description", "Fade out per step: integer ms OR beat string. Beat strings auto-set tempoType to Beats."}}},
                {"holdTime", {{"description", "Hold/duration per step: integer ms OR beat string. Beat strings auto-set tempoType to Beats."}}},
                {"tempoType", {{"type", "string"}, {"enum", {"time", "beats"}}, {"description", "Time or Beats. Auto-set to Beats when beat strings used."}}},
                {"runOrder", {{"type", "string"}, {"enum", {"loop", "single", "pingpong", "random"}}, {"description", "Run order (default loop)"}}},
                {"direction", {{"type", "string"}, {"enum", {"forward", "backward"}}, {"description", "Direction (default forward)"}}}
            }}, {"required", {"name", "boundSceneID"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto itemsErr = validateItemsArray(args);
            if (itemsErr) return *itemsErr;
            Json results = Json::array();
            const Json &items = args.at("items");
            for (size_t index = 0; index < items.size(); ++index)
            {
                const Json &item = items[index];
                try {
                auto err = validateFields(item, {"name", "path", "boundSceneID", "fadeIn", "fadeOut", "holdTime", "tempoType", "runOrder", "direction"});
                if (err.empty())
                {
                    static const Json kEnums = {
                        {"tempoType", {{"enum", {"time", "beats"}}}},
                        {"runOrder", {{"enum", {"loop", "single", "pingpong", "random"}}}},
                        {"direction", {{"enum", {"forward", "backward"}}}}
                    };
                    err = validateEnums(item, kEnums);
                }
                if (!err.empty()) { results.push_back(invalid(index, Json::parse(err).value("error", err))); continue; }

                if (!item.contains("name") || !item.at("name").is_string() || !item.contains("boundSceneID"))
                {
                    results.push_back(invalid(index, "name and boundSceneID required"));
                    continue;
                }
                auto sceneId = mcp::jsonInteger(item.at("boundSceneID"), 0, mcp::kMaxId);
                Function *boundScene = sceneId ? doc->function(quint32(*sceneId)) : nullptr;
                if (boundScene == nullptr || boundScene->type() != Function::SceneType)
                {
                    results.push_back(invalid(index, "boundSceneID must be the ID of an existing Scene"));
                    continue;
                }

                auto timing = mcp::parseTimingFields(item, {"fadeIn", "fadeOut", "holdTime"});
                if (!timing.error.empty()) { results.push_back(invalid(index, timing.error)); continue; }

                QString name = QString::fromStdString(item.at("name").get<std::string>());
                Function *existing = mcp::findFunction(doc, name, Function::SequenceType);
                std::unique_ptr<Sequence> seq(new Sequence(doc));
                if (existing)
                {
                    seq->copyFrom(existing);
                    // Step values are aligned to the bound Scene's channels
                    if (seq->boundSceneID() != boundScene->id() && seq->stepsCount() > 0)
                    {
                        results.push_back(invalid(index, "boundSceneID cannot change on a sequence that has steps"));
                        continue;
                    }
                }
                else
                    seq->setName(name);

                if (item.contains("path"))
                    seq->setPath(QString::fromStdString(item.at("path").get<std::string>()));

                seq->setBoundSceneID(boundScene->id());

                if (timing.useBeatMode)
                    seq->setTempoType(Function::Beats);
                else if (item.contains("tempoType"))
                    seq->setTempoType(Function::Time);

                if (timing.present.count("fadeIn")) seq->setFadeInSpeed(timing.values["fadeIn"]);
                else seq->setFadeInSpeed(0);
                if (timing.present.count("fadeOut")) seq->setFadeOutSpeed(timing.values["fadeOut"]);
                else seq->setFadeOutSpeed(0);
                const uint holdTime = timing.present.count("holdTime") ? timing.values["holdTime"] : 1000;
                seq->setDuration(Function::speedAdd(seq->fadeInSpeed(), holdTime));

                QString order = QString::fromStdString(item.value("runOrder", "loop"));
                if (order == "single") seq->setRunOrder(Function::SingleShot);
                else if (order == "pingpong") seq->setRunOrder(Function::PingPong);
                else if (order == "random") seq->setRunOrder(Function::Random);
                else seq->setRunOrder(Function::Loop);

                QString dir = QString::fromStdString(item.value("direction", "forward"));
                if (dir == "backward") seq->setDirection(Function::Backward);
                else seq->setDirection(Function::Forward);

                results.push_back(commitStaged(doc, index, existing, seq.release()));
                } catch (const std::exception &e) {
                    results.push_back(invalid(index, e.what()));
                }
            }
            return results.dump();
            });
        }
    )
    .set_description("Create sequences bound to scenes for per-channel step animation. Upserts: replaces timing and binding on existing sequences. Batch: wrap entries in {\"items\": [...]}.")
    .set_annotations(mcp::kAnnotIdempotent));

    // create_efxs (batch)
    tm.register_tool(Tool(
        "create_efxs",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"name", {{"type", "string"}}},
                {"path", {{"type", "string"}, {"description", "Folder path (e.g. 'Effects/EFX'). Creates folders implicitly."}}},
                {"fixtureIDs", {{"type", "array"}, {"items", {{"type", "integer"}}}}},
                {"fixtureNames", {{"type", "array"}, {"items", {{"type", "string"}}}, {"description", "Fixture name patterns (glob: * ?). Alternative to fixtureIDs."}}},
                {"algorithm", {{"type", "string"}, {"enum", {"Circle", "Eight", "Line", "Line2", "Diamond", "Square", "SquareChoppy", "SquareTrue", "Leaf", "Lissajous"}}, {"description", "Pattern algorithm (default Circle)"}}},
                {"width", {{"type", "integer"}, {"description", "Pattern width 0-127 (default 127)"}}},
                {"height", {{"type", "integer"}, {"description", "Pattern height 0-127 (default 127)"}}},
                {"xOffset", {{"type", "integer"}, {"description", "X center offset 0-255 (default 127 = middle)"}}},
                {"yOffset", {{"type", "integer"}, {"description", "Y center offset 0-255 (default 127 = middle)"}}},
                {"rotation", {{"type", "integer"}, {"description", "Pattern rotation 0-359 degrees (default 0)"}}},
                {"startOffset", {{"type", "integer"}, {"description", "Start phase offset 0-359 degrees (default 0)"}}},
                {"xFrequency", {{"type", "integer"}, {"description", "X frequency 0-32 for Lissajous (default 2)"}}},
                {"yFrequency", {{"type", "integer"}, {"description", "Y frequency 0-32 for Lissajous (default 3)"}}},
                {"xPhase", {{"type", "integer"}, {"description", "X phase 0-359 for Lissajous (default 0)"}}},
                {"yPhase", {{"type", "integer"}, {"description", "Y phase 0-359 for Lissajous (default 0)"}}},
                {"isRelative", {{"type", "boolean"}, {"description", "Relative to current position (default false)"}}},
                {"propagationMode", {{"type", "string"}, {"enum", {"Parallel", "Serial", "Asymmetric"}}, {"description", "Multi-fixture propagation (default Parallel)"}}},
                {"speed", {{"description", "Duration/cycle time: integer ms OR beat string ('1/8','1/4','1/2','1','2','4'). Beat strings auto-set tempoType to Beats. Default 5000ms."}}},
                {"fadeIn", {{"description", "Fade in: integer ms OR beat string. Beat strings auto-set tempoType to Beats."}}},
                {"fadeOut", {{"description", "Fade out: integer ms OR beat string. Beat strings auto-set tempoType to Beats."}}},
                {"tempoType", {{"type", "string"}, {"enum", {"time", "beats"}}, {"description", "Time or Beats. Auto-set to Beats when beat strings used."}}},
                {"runOrder", {{"type", "string"}, {"enum", {"loop", "single", "pingpong"}}, {"description", "Run order (default loop)"}}},
                {"direction", {{"type", "string"}, {"enum", {"forward", "backward"}}, {"description", "Direction (default forward)"}}},
                {"head", {{"type", "integer"}, {"description", "Head index for multi-head fixtures (default 0)"}}}
            }}, {"required", {"name", "algorithm"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto itemsErr = validateItemsArray(args);
            if (itemsErr) return *itemsErr;
            Json results = Json::array();
            const Json &items = args.at("items");
            for (size_t index = 0; index < items.size(); ++index)
            {
                const Json &item = items[index];
                try {
                auto err = validateFields(item, {"name", "path", "fixtureIDs", "fixtureNames", "algorithm", "width", "height", "xOffset", "yOffset", "rotation", "startOffset", "xFrequency", "yFrequency", "xPhase", "yPhase", "isRelative", "propagationMode", "speed", "fadeIn", "fadeOut", "tempoType", "runOrder", "direction", "head"});
                if (err.empty())
                {
                    static const Json kEnums = {
                        {"tempoType", {{"enum", {"time", "beats"}}}},
                        {"runOrder", {{"enum", {"loop", "single", "pingpong"}}}},
                        {"direction", {{"enum", {"forward", "backward"}}}},
                        {"algorithm", {{"enum", {"Circle", "Eight", "Line", "Line2", "Diamond", "Square", "SquareChoppy", "SquareTrue", "Leaf", "Lissajous"}}}},
                        {"propagationMode", {{"enum", {"Parallel", "Serial", "Asymmetric"}}}}
                    };
                    err = validateEnums(item, kEnums);
                }
                if (!err.empty()) { results.push_back(invalid(index, Json::parse(err).value("error", err))); continue; }
                if (!item.contains("name") || !item.at("name").is_string())
                {
                    results.push_back(invalid(index, "name must be a string"));
                    continue;
                }

                // Engine ranges (EFX setters clamp silently)
                struct IntField { const char *name; int64_t lo, hi, def; };
                static const IntField kInts[] = {
                    {"width", 0, 127, 127}, {"height", 0, 127, 127},
                    {"xOffset", 0, 255, 127}, {"yOffset", 0, 255, 127},
                    {"rotation", 0, 359, 0}, {"startOffset", 0, 359, 0},
                    {"xFrequency", 0, 32, 2}, {"yFrequency", 0, 32, 3},
                    {"xPhase", 0, 359, 0}, {"yPhase", 0, 359, 0}, {"head", 0, 255, 0}
                };
                std::map<std::string, int> ints;
                std::string itemErr;
                for (const IntField &f : kInts)
                {
                    if (!item.contains(f.name)) { ints[f.name] = int(f.def); continue; }
                    auto v = mcp::jsonInteger(item.at(f.name), f.lo, f.hi);
                    if (!v) { itemErr = mcp::integerError(f.name, f.lo, f.hi); break; }
                    ints[f.name] = int(*v);
                }
                if (itemErr.empty() && item.contains("isRelative") && !item.at("isRelative").is_boolean())
                    itemErr = "isRelative must be a boolean";
                if (!itemErr.empty()) { results.push_back(invalid(index, itemErr)); continue; }

                QList<quint32> fixtureIDs;
                if (item.contains("fixtureNames"))
                    for (auto &p : item.at("fixtureNames"))
                        for (quint32 id : mcp::resolveFixturesByName(doc, QString::fromStdString(p.get<std::string>())))
                            if (!fixtureIDs.contains(id)) fixtureIDs.append(id);
                if (item.contains("fixtureIDs"))
                {
                    for (auto &fid : item.at("fixtureIDs"))
                    {
                        auto id = mcp::jsonInteger(fid, 0, mcp::kMaxId);
                        if (!id || !doc->fixture(quint32(*id))) { itemErr = "fixtureIDs entries must be IDs of patched fixtures"; break; }
                        if (!fixtureIDs.contains(quint32(*id))) fixtureIDs.append(quint32(*id));
                    }
                }
                for (quint32 fid : fixtureIDs)
                {
                    if (itemErr.empty() && ints["head"] >= doc->fixture(fid)->heads())
                        itemErr = "head " + std::to_string(ints["head"]) + " does not exist on fixture " + std::to_string(fid);
                }
                if (!itemErr.empty()) { results.push_back(invalid(index, itemErr)); continue; }

                auto timing = mcp::parseTimingFields(item, {"speed", "fadeIn", "fadeOut"});
                if (!timing.error.empty()) { results.push_back(invalid(index, timing.error)); continue; }

                QString name = QString::fromStdString(item.at("name").get<std::string>());
                Function *existing = mcp::findFunction(doc, name, Function::EFXType);
                std::unique_ptr<EFX> efx(new EFX(doc));
                if (existing)
                {
                    efx->copyFrom(existing);
                    efx->removeAllFixtures();
                }
                else
                    efx->setName(name);

                if (item.contains("path"))
                    efx->setPath(QString::fromStdString(item.at("path").get<std::string>()));

                efx->setAlgorithm(EFX::stringToAlgorithm(QString::fromStdString(item.value("algorithm", "Circle"))));

                efx->setWidth(ints["width"]);
                efx->setHeight(ints["height"]);
                efx->setXOffset(ints["xOffset"]);
                efx->setYOffset(ints["yOffset"]);
                efx->setRotation(ints["rotation"]);
                efx->setStartOffset(ints["startOffset"]);
                efx->setIsRelative(item.value("isRelative", false));
                efx->setXFrequency(ints["xFrequency"]);
                efx->setYFrequency(ints["yFrequency"]);
                efx->setXPhase(ints["xPhase"]);
                efx->setYPhase(ints["yPhase"]);

                QString propMode = QString::fromStdString(item.value("propagationMode", "Parallel"));
                if (propMode == "Serial") efx->setPropagationMode(EFX::Serial);
                else if (propMode == "Asymmetric") efx->setPropagationMode(EFX::Asymmetric);
                else efx->setPropagationMode(EFX::Parallel);

                if (timing.useBeatMode)
                    efx->setTempoType(Function::Beats);
                else if (item.contains("tempoType"))
                    efx->setTempoType(Function::Time);
                if (timing.present.count("speed")) efx->setDuration(timing.values["speed"]);
                else efx->setDuration(5000);
                if (timing.present.count("fadeIn")) efx->setFadeInSpeed(timing.values["fadeIn"]);
                if (timing.present.count("fadeOut")) efx->setFadeOutSpeed(timing.values["fadeOut"]);

                QString order = QString::fromStdString(item.value("runOrder", "loop"));
                if (order == "single") efx->setRunOrder(Function::SingleShot);
                else if (order == "pingpong") efx->setRunOrder(Function::PingPong);
                else efx->setRunOrder(Function::Loop);

                QString dir = QString::fromStdString(item.value("direction", "forward"));
                if (dir == "backward") efx->setDirection(Function::Backward);
                else efx->setDirection(Function::Forward);

                for (quint32 fid : fixtureIDs)
                {
                    EFXFixture *ef = new EFXFixture(efx.get());
                    ef->setHead(GroupHead(fid, ints["head"]));
                    efx->addFixture(ef);
                }

                results.push_back(commitStaged(doc, index, existing, efx.release()));
                } catch (const std::exception &e) {
                    results.push_back(invalid(index, e.what()));
                }
            }
            return results.dump();
            });
        }
    )
    .set_description("Create EFX position effects for moving heads (10 algorithm types). Upserts: replaces all settings on existing EFXs. Batch: wrap entries in {\"items\": [...]}.")
    .set_annotations(mcp::kAnnotIdempotent));

    // create_collections (batch)
    tm.register_tool(Tool(
        "create_collections",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"name", {{"type", "string"}}},
                {"path", {{"type", "string"}, {"description", "Folder path (e.g. 'Phase1/Collections'). Creates folders implicitly."}}},
                {"functionIDs", {{"type", "array"}, {"items", {{"type", "integer"}}}}},
                {"functionNames", {{"type", "array"}, {"items", {{"type", "string"}}}, {"description", "Function names to include. Alternative to functionIDs."}}}
            }}, {"required", {"name"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto itemsErr = validateItemsArray(args);
            if (itemsErr) return *itemsErr;
            Json results = Json::array();
            const Json &items = args.at("items");
            for (size_t index = 0; index < items.size(); ++index)
            {
                const Json &item = items[index];
                try {
                auto err = validateFields(item, {"name", "path", "functionIDs", "functionNames"});
                if (!err.empty()) { results.push_back(invalid(index, Json::parse(err).value("error", err))); continue; }
                if (!item.contains("name") || !item.at("name").is_string())
                {
                    results.push_back(invalid(index, "name must be a string"));
                    continue;
                }

                QString name = QString::fromStdString(item.at("name").get<std::string>());
                Function *existing = mcp::findFunction(doc, name, Function::CollectionType);

                std::string itemErr;
                QList<quint32> funcIDs;
                if (item.contains("functionNames"))
                    for (auto &fn : item.at("functionNames"))
                    {
                        quint32 fid = mcp::resolveFunctionByName(doc, QString::fromStdString(fn.get<std::string>()));
                        if (fid == Function::invalidId()) { itemErr = "functionNames entries must name existing functions"; break; }
                        if (!funcIDs.contains(fid)) funcIDs.append(fid);
                    }
                if (itemErr.empty() && item.contains("functionIDs"))
                    for (auto &fid : item.at("functionIDs"))
                    {
                        auto id = mcp::jsonInteger(fid, 0, mcp::kMaxId);
                        if (!id || !doc->function(quint32(*id))) { itemErr = "functionIDs entries must be IDs of existing functions"; break; }
                        if (!funcIDs.contains(quint32(*id))) funcIDs.append(quint32(*id));
                    }
                if (itemErr.empty() && existing && funcIDs.contains(existing->id()))
                    itemErr = "a collection cannot contain itself";
                if (itemErr.empty())
                    itemErr = mcp::functionCycleError(doc, existing ? existing->id() : Function::invalidId(), funcIDs);
                if (!itemErr.empty()) { results.push_back(invalid(index, itemErr)); continue; }

                std::unique_ptr<Collection> col(new Collection(doc));
                if (existing)
                {
                    col->copyFrom(existing);
                    for (quint32 fid : col->functions())
                        col->removeFunction(fid);
                }
                else
                    col->setName(name);

                if (item.contains("path"))
                    col->setPath(QString::fromStdString(item.at("path").get<std::string>()));

                for (quint32 fid : funcIDs)
                    col->addFunction(fid);
                results.push_back(commitStaged(doc, index, existing, col.release()));
                } catch (const std::exception &e) {
                    results.push_back(invalid(index, e.what()));
                }
            }
            return results.dump();
            });
        }
    )
    .set_description("Create collections (parallel function groups — use for moods/phases). Upserts. Batch: wrap entries in {\"items\": [...]}.")
    .set_annotations(mcp::kAnnotIdempotent));

    // create_rgb_matrices (batch)
    tm.register_tool(Tool(
        "create_rgb_matrices",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"name", {{"type", "string"}}},
                {"type", {{"type", "string"}, {"enum", {"RGBMatrix", "HUEMatrix"}}, {"default", "HUEMatrix"}, {"description", "HUEMatrix (default) supports HSV scripts and fork transforms. RGBMatrix uses stock algorithms. Discover with query_rgb_algorithms.matrixType."}}},
                {"path", {{"type", "string"}, {"description", "Folder path (e.g. 'Effects/RGB'). Creates folders implicitly."}}},
                {"fixtureGroupID", {{"type", "integer"}}},
                {"algorithm", {{"type", "string"}, {"description", "Algorithm name (use query_rgb_algorithms to discover)"}}},
                {"startColor", {{"type", "string"}, {"description", "Hex color e.g. #FF0000 (shortcut for colors[0])"}}},
                {"endColor", {{"type", "string"}, {"description", "Hex color e.g. #0000FF (shortcut for colors[1])"}}},
                {"colors", {{"type", "array"}, {"items", {{"type", "string"}}}, {"description", "Array of hex colors (up to 3). Overrides startColor/endColor."}}},
                {"duration", {{"description", "Step duration: integer ms OR beat string ('1/8','1/4','1/2','1','2','4'). Beat strings auto-set tempoType to Beats."}}},
                {"fadeIn", {{"description", "Fade in: integer ms OR beat string."}}},
                {"fadeOut", {{"description", "Fade out: integer ms OR beat string."}}},
                {"tempoType", {{"type", "string"}, {"enum", {"time", "beats"}}, {"description", "Time or Beats. Auto-set to Beats when beat strings used."}}},
                {"runOrder", {{"type", "string"}, {"enum", {"Loop", "SingleShot", "PingPong", "Random"}}, {"description", "Loop, SingleShot, PingPong, or Random"}}},
                {"direction", {{"type", "string"}, {"enum", {"Forward", "Backward"}}, {"description", "Forward or Backward"}}},
                {"controlMode", {{"type", "string"}, {"enum", {"RGB", "White", "Amber", "UV", "Dimmer", "Shutter", "RGBW", "RGBWBrighter"}}, {"description", "RGB, White, Amber, UV, Dimmer, Shutter, RGBW (white extraction), or RGBWBrighter (additive white)"}}},
                {"blendMode", {{"type", "string"}, {"enum", {"Normal", "Additive", "Mask", "Subtractive"}}, {"description", "Normal, Additive, Mask, or Subtractive"}}},
                {"properties", {{"type", "object"}, {"description", "Algorithm-specific properties as key-value pairs. Values may be string, integer, double, boolean, or null \u2014 all are coerced to string internally (e.g. {\"presetDecay\": 10, \"presetMode\": \"Mids\", \"enabled\": true})."}}},
                {"text", {{"type", "string"}, {"description", "Text content (for RGBText algorithm)"}}},
                {"animationStyle", {{"type", "string"}, {"enum", {"Static", "Letters", "Horizontal", "Vertical", "Animation"}}, {"description", "Static, Letters, Horizontal, Vertical, or Animation (for RGBText/RGBImage algorithms)"}}},
                {"rotation", {{"type", "integer"}, {"description", "Rotation in degrees: 0, 90, 180, or 270"}}},
                {"mirror", {{"type", "string"}, {"enum", {"Off", "Horizontal", "Vertical", "Both"}}, {"description", "Mirror mode: Off, Horizontal, Vertical, or Both"}}},
                {"mirrorBlend", {{"type", "string"}, {"enum", {"Flip", "Max", "Average", "Additive"}}, {"description", "Mirror blend algorithm: Flip (default), Max, Average, or Additive"}}},
                {"beatEffect", {{"type", "string"}, {"enum", {"Off", "Mirror", "ColorInvert", "Blackout", "Whiteout"}}, {"description", "Beat transform effect applied per-segment on beat"}}},
                {"beatSelection", {{"type", "string"}, {"enum", {"AllOnDownbeat", "Walk", "Random"}}, {"description", "Segment selection mode: AllOnDownbeat (all segments on the bar's downbeat), Walk (one per beat), Random"}}},
                {"beatOrientation", {{"type", "string"}, {"enum", {"Rows", "Columns"}}, {"description", "Segment orientation: Rows or Columns"}}}
            }}, {"required", {"name"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto itemsErr = validateItemsArray(args);
            if (itemsErr) return *itemsErr;
            Json results = Json::array();
            const Json &items = args.at("items");
            for (size_t index = 0; index < items.size(); ++index)
            {
                const Json &item = items[index];
                try {
                auto err = validateFields(item, {"name", "type", "path", "fixtureGroupID", "algorithm",
                    "startColor", "endColor", "colors", "duration", "fadeIn", "fadeOut",
                    "tempoType", "runOrder", "direction", "controlMode", "blendMode",
                    "properties", "text", "animationStyle", "rotation", "mirror", "mirrorBlend",
                    "beatEffect", "beatSelection", "beatOrientation"});
                if (!err.empty()) { results.push_back(mcp::itemErrorFromDump(index, err)); continue; }

                static const Json kEnums = {
                    {"type", {{"enum", {"RGBMatrix", "HUEMatrix"}}}},
                    {"tempoType", {{"enum", {"time", "beats"}}}},
                    {"runOrder", {{"enum", {"Loop", "SingleShot", "PingPong", "Random"}}}},
                    {"direction", {{"enum", {"Forward", "Backward"}}}},
                    {"controlMode", {{"enum", {"RGB", "White", "Amber", "UV", "Dimmer", "Shutter", "RGBW", "RGBWBrighter"}}}},
                    {"blendMode", {{"enum", {"Normal", "Additive", "Mask", "Subtractive"}}}},
                    {"animationStyle", {{"enum", {"Static", "Letters", "Horizontal", "Vertical", "Animation"}}}},
                    {"mirror", {{"enum", {"Off", "Horizontal", "Vertical", "Both"}}}},
                    {"mirrorBlend", {{"enum", {"Flip", "Max", "Average", "Additive"}}}},
                    {"beatEffect", {{"enum", {"Off", "Mirror", "ColorInvert", "Blackout", "Whiteout"}}}},
                    {"beatSelection", {{"enum", {"AllOnDownbeat", "Walk", "Random"}}}},
                    {"beatOrientation", {{"enum", {"Rows", "Columns"}}}}
                };
                err = validateEnums(item, kEnums);
                if (!err.empty()) { results.push_back(mcp::itemErrorFromDump(index, err)); continue; }

                QString name = QString::fromStdString(item.at("name").get<std::string>());
                // Preserve default HUE creation and the enum validator's case-insensitive handling.
                const bool wantHue = QString::fromStdString(item.value("type", std::string("HUEMatrix")))
                    .compare("HUEMatrix", Qt::CaseInsensitive) == 0;

                static const char *kHueOnly[] = {"rotation", "mirror", "mirrorBlend",
                                                 "beatEffect", "beatSelection", "beatOrientation"};
                if (!wantHue)
                {
                    bool rejected = false;
                    for (const char *f : kHueOnly)
                    {
                        if (item.contains(f))
                        {
                            results.push_back(invalid(index, std::string(f) + " is only supported on a HUEMatrix"));
                            rejected = true;
                            break;
                        }
                    }
                    if (rejected) continue;
                }

                // Resolve before allocating a matrix or changing an existing one.
                // The stock factory returns an empty script for unknown names, so
                // RGB membership must be checked explicitly. HUE's factory also
                // accepts legacy stock names that its offered list intentionally hides.
                RGBAlgorithm *algo = nullptr;
                std::unique_ptr<RGBAlgorithm> algoOwned;
                if (item.contains("algorithm"))
                {
                    QString algoName = QString::fromStdString(item.at("algorithm").get<std::string>());
                    if (!wantHue && !RGBAlgorithm::algorithms(doc).contains(algoName))
                    {
                        results.push_back(invalid(index, "unknown algorithm for RGBMatrix: " + algoName.toStdString()));
                        continue;
                    }
                    algo = wantHue ? HUEMatrix::createAlgorithm(doc, algoName)
                                   : RGBAlgorithm::algorithm(doc, algoName);
                    algoOwned.reset(algo);
                    if (algo == nullptr)
                    {
                        results.push_back(invalid(index, "unknown algorithm: " + algoName.toStdString()));
                        continue;
                    }
                }

                Function::Type wantType = wantHue ? Function::HUEMatrixType : Function::RGBMatrixType;
                Function *existing = mcp::findFunction(doc, name, wantType);

                // Preflight every value before touching the matrix: RGBMatrix::copyFrom()
                // drops script properties, so updates are applied in place under admission.
                std::string itemErr;
                for (const char *key : {"path", "startColor", "endColor", "text"})
                    if (itemErr.empty() && item.contains(key) && !item.at(key).is_string())
                        itemErr = std::string(key) + " must be a string";
                if (itemErr.empty() && item.contains("colors"))
                {
                    const Json &colors = item.at("colors");
                    if (!colors.is_array())
                        itemErr = "colors must be an array of strings";
                    else
                        for (const Json &c : colors)
                            if (!c.is_string()) { itemErr = "colors must be an array of strings"; break; }
                }
                if (itemErr.empty() && item.contains("fixtureGroupID")
                    && !mcp::jsonInteger(item.at("fixtureGroupID"), 0, mcp::kMaxId))
                    itemErr = mcp::integerError("fixtureGroupID", 0, mcp::kMaxId);
                if (itemErr.empty() && item.contains("rotation") && !mcp::jsonInteger(item.at("rotation"), 0, 359))
                    itemErr = mcp::integerError("rotation", 0, 359);
                if (!itemErr.empty()) { results.push_back(invalid(index, itemErr)); continue; }

                // Timing: determine beat mode first, then set tempoType, then durations.
                // This avoids the auto-conversion that setTempoType does on existing values.
                // Audio* algorithms default to beat mode.
                RGBAlgorithm *effectiveAlgo = algo != nullptr ? algo
                    : existing ? qobject_cast<RGBMatrix*>(existing)->algorithm() : nullptr;
                const bool audioBeatMode = !item.contains("tempoType") && effectiveAlgo && effectiveAlgo->usesAudio();
                auto timing = mcp::parseTimingFields(item, {"duration", "fadeIn", "fadeOut"}, audioBeatMode);
                if (!timing.error.empty())
                {
                    results.push_back(invalid(index, timing.error));
                    continue;
                }

                MasterTimer *timer = doc->masterTimer();
                std::unique_ptr<RGBMatrix> staged;
                RGBMatrix *matrix;
                MasterTimer::EditAdmission admission(timer, existing && timer->beginFunctionEdit(existing));
                if (existing)
                {
                    if (!admission)
                    {
                        results.push_back(refusal(index, "running", "Function " + std::to_string(existing->id()) +
                                                  " or a function using it is running, starting or flashing"));
                        continue;
                    }
                    matrix = qobject_cast<RGBMatrix*>(existing);
                }
                else
                {
                    staged.reset(wantHue ? new HUEMatrix(doc) : new RGBMatrix(doc));
                    matrix = staged.get();
                    matrix->setName(name);
                }
                HUEMatrix *hueMatrix = qobject_cast<HUEMatrix *>(matrix);

                if (item.contains("path"))
                    matrix->setPath(QString::fromStdString(item.at("path").get<std::string>()));

                if (item.contains("fixtureGroupID"))
                    matrix->setFixtureGroup(quint32(*mcp::jsonInteger(item.at("fixtureGroupID"), 0, mcp::kMaxId)));

                if (algo != nullptr)
                    matrix->setAlgorithm(algoOwned.release());

                // Colors: prefer 'colors' array, fall back to startColor/endColor
                if (item.contains("colors") && item.at("colors").is_array())
                {
                    auto &colorsArr = item.at("colors");
                    for (size_t i = 0; i < colorsArr.size() && i < 5; i++)
                        matrix->setColor(i, QColor(QString::fromStdString(colorsArr[i].get<std::string>())));
                }
                else
                {
                    if (item.contains("startColor"))
                        matrix->setColor(0, QColor(QString::fromStdString(item.at("startColor").get<std::string>())));
                    if (item.contains("endColor"))
                        matrix->setColor(1, QColor(QString::fromStdString(item.at("endColor").get<std::string>())));
                }

                {
                    if (timing.useBeatMode)
                        matrix->setTempoType(Function::Beats);
                    else if (item.contains("tempoType"))
                        matrix->setTempoType(Function::Time);

                    if (timing.present.count("duration")) matrix->setDuration(timing.values["duration"]);
                    if (timing.present.count("fadeIn"))
                    {
                        if (timing.present.count("duration"))
                            matrix->setFadeInSpeed(timing.values["fadeIn"]);
                        else
                            matrix->setFadeInSpeedPreservingHold(timing.values["fadeIn"]);
                    }
                    if (timing.present.count("fadeOut")) matrix->setFadeOutSpeed(timing.values["fadeOut"]);
                }

                // Run order
                if (item.contains("runOrder"))
                {
                    QString ro = QString::fromStdString(item.at("runOrder").get<std::string>());
                    matrix->setRunOrder(Function::stringToRunOrder(ro));
                }

                // Direction
                if (item.contains("direction"))
                {
                    QString dir = QString::fromStdString(item.at("direction").get<std::string>());
                    matrix->setDirection(Function::stringToDirection(dir));
                }

                // Control mode
                if (item.contains("controlMode"))
                {
                    QString cm = QString::fromStdString(item.at("controlMode").get<std::string>());
                    RGBMatrix::ControlMode mode = hueMatrix != NULL ? HUEMatrix::stringToControlMode(cm)
                                                                   : RGBMatrix::stringToControlMode(cm);
                    matrix->setControlMode(mode);
                }

                // Blend mode
                if (item.contains("blendMode"))
                {
                    QString bm = QString::fromStdString(item.at("blendMode").get<std::string>());
                    matrix->setBlendMode(Universe::stringToBlendMode(bm));
                }

                // Algorithm-specific properties (for scripts).
                // Values may be string, integer, double, boolean, or null —
                // all are coerced to string before storing on the matrix.
                if (item.contains("properties") && item.at("properties").is_object())
                {
                    for (auto &[key, val] : item.at("properties").items())
                    {
                        QString propName = QString::fromStdString(key);
                        QString propVal;
                        if (val.is_string())
                            propVal = QString::fromStdString(val.get<std::string>());
                        else if (val.is_number_integer())
                            propVal = QString::number(val.get<int64_t>());
                        else if (val.is_number_float())
                            propVal = QString::number(val.get<double>());
                        else if (val.is_boolean())
                            propVal = val.get<bool>() ? QStringLiteral("1") : QStringLiteral("0");
                        else if (val.is_null())
                            propVal = QString();
                        else
                            propVal = QString::fromStdString(val.dump());
                        matrix->setProperty(propName, propVal);

                        // Also set on the algorithm itself if it's a script
                        RGBAlgorithm *algo = matrix->algorithm();
                        if (algo && algo->type() == RGBAlgorithm::Script)
                        {
                            RGBScript *script = static_cast<RGBScript*>(algo);
                            script->setProperty(propName, propVal);
                        }
                    }
                }

                // RGBText shortcuts
                if (item.contains("text"))
                {
                    RGBAlgorithm *algo = matrix->algorithm();
                    if (algo && algo->type() == RGBAlgorithm::Text)
                    {
                        RGBText *textAlgo = static_cast<RGBText*>(algo);
                        textAlgo->setText(QString::fromStdString(item.at("text").get<std::string>()));
                    }
                }

                // Animation style for RGBText/RGBImage
                if (item.contains("animationStyle"))
                {
                    QString style = QString::fromStdString(item.at("animationStyle").get<std::string>());
                    RGBAlgorithm *algo = matrix->algorithm();
                    if (algo)
                    {
                        if (algo->type() == RGBAlgorithm::Text)
                        {
                            RGBText *textAlgo = static_cast<RGBText*>(algo);
                            textAlgo->setAnimationStyle(RGBText::stringToAnimationStyle(style));
                        }
                        else if (algo->type() == RGBAlgorithm::Image)
                        {
                            RGBImage *imgAlgo = static_cast<RGBImage*>(algo);
                            imgAlgo->setAnimationStyle(RGBImage::stringToAnimationStyle(style));
                        }
                    }
                }

                // Rotation, Mirror and Beat Transform (HUEMatrix only)
                if (hueMatrix != NULL)
                {
                    if (item.contains("rotation"))
                        hueMatrix->setRotation((item.at("rotation").get<int>() / 90) & 3);

                    if (item.contains("mirror"))
                    {
                        QString m = QString::fromStdString(item.at("mirror").get<std::string>());
                        if (m.compare("Horizontal", Qt::CaseInsensitive) == 0)
                            hueMatrix->setMirror(1);
                        else if (m.compare("Vertical", Qt::CaseInsensitive) == 0)
                            hueMatrix->setMirror(2);
                        else if (m.compare("Both", Qt::CaseInsensitive) == 0)
                            hueMatrix->setMirror(3);
                        else
                            hueMatrix->setMirror(0);
                    }

                    if (item.contains("mirrorBlend"))
                    {
                        QString mb = QString::fromStdString(item.at("mirrorBlend").get<std::string>());
                        hueMatrix->setMirrorBlend(HUEMatrix::stringToMirrorBlend(mb));
                    }

                    if (item.contains("beatEffect"))
                    {
                        QString be = QString::fromStdString(item.at("beatEffect").get<std::string>());
                        hueMatrix->setBeatEffect(HUEMatrix::stringToBeatEffect(be));
                    }
                    if (item.contains("beatSelection"))
                    {
                        QString bs = QString::fromStdString(item.at("beatSelection").get<std::string>());
                        hueMatrix->setBeatSelection(HUEMatrix::stringToBeatSelection(bs));
                    }
                    if (item.contains("beatOrientation"))
                    {
                        QString bo = QString::fromStdString(item.at("beatOrientation").get<std::string>());
                        hueMatrix->setBeatOrientation(HUEMatrix::stringToBeatOrientation(bo));
                    }
                }

                Json rec;
                if (existing)
                {
                    admission.end();
                    rec = mcp::itemOk(index, {{"id", existing->id()}, {"outcome", "updated"}});
                }
                else
                {
                    rec = commitStaged(doc, index, nullptr, staged.release());
                    if (rec.value("status", "") != "ok") { results.push_back(rec); continue; }
                    matrix = qobject_cast<RGBMatrix*>(doc->function(rec.at("id").get<quint32>()));
                }
                Json detail = mcp::rgbMatrixToJson(matrix);
                detail.update(rec);
                results.push_back(detail);
                } catch (const std::exception &e) {
                    results.push_back(invalid(index, e.what()));
                }
            }
            return results.dump();
            });
        }
    )
    .set_description("Create/update RGB matrix effects. Supports audio-reactive algorithms, beat-synced timing "
                     "(use beat strings like '1/4', '1/2', '1' for duration/fadeIn/fadeOut — auto-sets Beats tempo), "
                     "script properties (e.g. presetDecay, presetMode), blend modes (Additive for layering), "
                     "and up to 3 colors. Use query_rgb_algorithms to discover algorithms and properties. Upserts. Batch: wrap entries in {\"items\": [...]}.")
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
            auto itemsErr = validateItemsArray(args);
            if (itemsErr) return *itemsErr;
            Json results = Json::array();
            const Json &items = args.at("items");
            for (size_t index = 0; index < items.size(); ++index)
            {
                const Json &item = items[index];
                auto err = validateFields(item, {"name", "fixtureIDs", "columns", "rows"});
                if (!err.empty()) { results.push_back(mcp::itemErrorFromDump(index, err)); continue; }
                if (!item.contains("name") || !item.at("name").is_string()
                    || !item.contains("fixtureIDs") || !item.at("fixtureIDs").is_array())
                {
                    results.push_back(invalid(index, "name must be a string and fixtureIDs an array"));
                    continue;
                }

                std::string itemErr;
                QList<quint32> fixtureIDs;
                for (auto &v : item.at("fixtureIDs"))
                {
                    auto id = mcp::jsonInteger(v, 0, mcp::kMaxId);
                    if (!id || !doc->fixture(quint32(*id)) || fixtureIDs.contains(quint32(*id)))
                    {
                        itemErr = "fixtureIDs entries must be distinct IDs of existing fixtures";
                        break;
                    }
                    fixtureIDs.append(quint32(*id));
                }
                int columns = fixtureIDs.size();
                int rows = 1;
                for (const char *key : {"columns", "rows"})
                {
                    if (!itemErr.empty() || !item.contains(key))
                        continue;
                    auto v = mcp::jsonInteger(item.at(key), 1, 4096);
                    if (!v)
                        itemErr = mcp::integerError(key, 1, 4096);
                    else
                        (std::string(key) == "columns" ? columns : rows) = int(*v);
                }
                if (!itemErr.empty()) { results.push_back(invalid(index, itemErr)); continue; }

                QString name = QString::fromStdString(item.at("name").get<std::string>());
                FixtureGroup *existing = mcp::findFixtureGroup(doc, name);
                std::unique_ptr<FixtureGroup> staged(new FixtureGroup(doc));
                staged->setName(name);
                staged->setSize(QSize(columns, rows));
                int col = 0, row = 0;
                for (quint32 fid : fixtureIDs)
                {
                    staged->assignFixture(fid, QLCPoint(col, row));
                    if (++col >= columns) { col = 0; row++; }
                }

                // Matrices read their group on the timer thread (RGBMatrix::updateMapChannels),
                // and a starting matrix looks groups up in Doc: admit the edit against them.
                QList<Function*> readers;
                for (Function *f : doc->functions())
                {
                    RGBMatrix *mtx = qobject_cast<RGBMatrix*>(f);
                    if (mtx && (existing == nullptr || mtx->fixtureGroup() == existing->id()))
                        readers.append(f);
                }
                MasterTimer *timer = doc->masterTimer();
                MasterTimer::EditAdmission admission(timer, timer->beginFunctionEdit(readers));
                if (!admission)
                {
                    results.push_back(refusal(index, "running",
                        "An RGB matrix reading this fixture group is running, starting or queued"));
                    continue;
                }
                if (existing)
                {
                    existing->copyFrom(staged.get());
                    existing->setSize(staged->size()); // copyFrom() emits no change
                }
                else if (doc->addFixtureGroup(staged.get()))
                    existing = staged.release();
                admission.end();
                if (existing == nullptr)
                {
                    results.push_back(invalid(index, "Fixture group could not be added"));
                    continue;
                }
                results.push_back(mcp::itemOk(index, {{"id", existing->id()}, {"name", name.toStdString()},
                                                      {"outcome", staged ? "updated" : "created"}}));
            }
            return results.dump();
            });
        }
    )
    .set_description("Create fixture groups with grid layout for RGB matrices. Upserts. Batch: wrap entries in {\"items\": [...]}.")
    .set_annotations(mcp::kAnnotIdempotent));

    // delete_fixture_groups — remove groups by ID
    tm.register_tool(Tool(
        "delete_fixture_groups",
        Json{{"type", "object"}, {"properties", {
            {"ids", {{"type", "array"}, {"items", {{"type", "integer"}}},
                     {"description", "Fixture group IDs to delete"}}}
        }}, {"required", {"ids"}}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto err = validateFields(args, {"ids"});
            if (!err.empty()) return err;
            if (!args.contains("ids") || !args.at("ids").is_array())
                return Json({{"error", "ids must be an array of integers"}}).dump();

            Json results = Json::array();
            QMap<quint32, size_t> firstIndex;
            const Json &ids = args.at("ids");
            for (size_t i = 0; i < ids.size(); ++i)
            {
                auto parsedId = mcp::jsonInteger(ids[i], 0, mcp::kMaxId);
                if (!parsedId)
                {
                    results.push_back(refusal(i, "invalid", mcp::integerError("ids[" + std::to_string(i) + "]", 0, mcp::kMaxId)));
                    continue;
                }
                const quint32 id = quint32(*parsedId);
                // A repeat reuses the first outcome and never deletes twice
                if (firstIndex.contains(id))
                {
                    const size_t first = firstIndex.value(id);
                    Json repeat = results[first];
                    repeat["index"] = i;
                    repeat["duplicateOf"] = first;
                    if (repeat.value("status", "") == "ok")
                        repeat["outcome"] = "duplicate";
                    results.push_back(repeat);
                    continue;
                }
                firstIndex.insert(id, i);
                FixtureGroup *group = doc->fixtureGroup(id);
                if (group == NULL)
                {
                    results.push_back(refusal(i, "not_found", "Fixture group " + std::to_string(id) + " not found"));
                    continue;
                }
                const std::string name = group->name().toStdString();

                // Nothing drops an RGBMatrix's reference to its group: a running
                // one caches it as a raw pointer and only re-fetches when null
                // (use-after-free on the MasterTimer thread), and a stopped one
                // keeps a dead group id that is written straight back to the
                // project file. Refuse either way and name the matrices, rather
                // than stopping or silently repointing them.
                Json blockers = Json::array();
                for (Function *fn : doc->functions())
                {
                    RGBMatrix *matrix = qobject_cast<RGBMatrix*>(fn);
                    if (matrix != NULL && matrix->fixtureGroup() == id)
                        blockers.push_back({{"id", (int)matrix->id()},
                                            {"name", matrix->name().toStdString()},
                                            {"running", matrix->isRunning()}});
                }
                if (!blockers.empty())
                {
                    Json refused = refusal(i, "bound_matrix", "fixture group is bound to one or more matrices — "
                                                              "repoint or delete them first");
                    refused["id"] = id;
                    refused["name"] = name;
                    refused["boundMatrices"] = blockers;
                    results.push_back(refused);
                    continue;
                }

                if (doc->deleteFixtureGroup(id))
                    results.push_back(mcp::itemOk(i, {{"id", id}, {"name", name}, {"outcome", "deleted"}}));
                else
                    results.push_back(refusal(i, "invalid", "could not delete fixture group"));
            }
            return results.dump();
            });
        }
    )
    .set_description("Delete fixture groups by ID. Batch: wrap entries in {\"ids\": [...]}. "
                     "The fixtures themselves are left patched; only the grouping is removed. "
                     "A group still bound to an RGB matrix is refused — the reply lists the "
                     "matrices in boundMatrices (code bound_matrix) so they can be repointed or deleted first. "
                     "Returns one record per input id, in input order; a repeated id copies the first "
                     "record with duplicateOf (outcome duplicate when it was deleted) instead of deleting twice.")
    .set_annotations(mcp::kAnnotDestructive));

    // create_scripts (batch) — JavaScript-only
    tm.register_tool(Tool(
        "create_scripts",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"name", {{"type", "string"}}},
                {"path", {{"type", "string"}, {"description", "Folder path (e.g. 'Utilities/Scripts'). Creates folders implicitly."}}},
                {"content", {{"type", "string"}, {"description",
                    "Raw JavaScript code executed by QJSEngine. "
                    "The code runs inside (function run() { YOUR_CODE }). "
                    "Use the Engine object to control QLC+. Full JS is available: variables, loops, if/else, Math.*, closures, arrays, objects."
                }}}
            }}, {"required", {"name", "content"}}}}}}
        }}, {"required", {"items"}}},
        Json{},
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto itemsErr = validateItemsArray(args);
            if (itemsErr) return *itemsErr;
            Json results = Json::array();
            const Json &items = args.at("items");
            for (size_t index = 0; index < items.size(); ++index)
            {
                const Json &item = items[index];
                auto err = validateFields(item, {"name", "path", "content"});
                if (!err.empty()) { results.push_back(mcp::itemErrorFromDump(index, err)); continue; }
                if (!item.contains("name") || !item.at("name").is_string()
                    || !item.contains("content") || !item.at("content").is_string()
                    || (item.contains("path") && !item.at("path").is_string()))
                {
                    results.push_back(invalid(index, "name and content must be strings; path must be a string"));
                    continue;
                }

                std::string name = item.at("name").get<std::string>();
                std::string content = item.at("content").get<std::string>();
                if (content.empty())
                {
                    results.push_back(invalid(index, "content must not be empty"));
                    continue;
                }

                QString qContent = QString::fromStdString(content);
                QStringList syntaxErrors = Script::syntaxErrors(qContent);
                if (!syntaxErrors.isEmpty())
                {
                    Json errorList = Json::array();
                    for (const QString &e : syntaxErrors)
                        errorList.push_back(e.toStdString());
                    Json rec = invalid(index, "JavaScript syntax check failed");
                    rec["name"] = name;
                    rec["syntaxErrors"] = errorList;
                    results.push_back(rec);
                    continue;
                }

                // Applied in place under admission: staging a Script dirties the Doc.
                MasterTimer *timer = doc->masterTimer();
                Function *existing = mcp::findFunction(doc, QString::fromStdString(name), Function::ScriptType);
                if (existing == nullptr)
                {
                    MasterTimer::EditAdmission admission(timer, timer->beginRegistryEdit());
                    if (!admission)
                    {
                        results.push_back(refusal(index, "functions_running", kRegistryBusy));
                        continue;
                    }
                    Script *script = new Script(doc);
                    script->setName(QString::fromStdString(name));
                    if (item.contains("path"))
                        script->setPath(QString::fromStdString(item.at("path").get<std::string>()));
                    script->setData(qContent);
                    const bool added = doc->addFunction(script);
                    admission.end();
                    if (!added)
                    {
                        delete script;
                        results.push_back(invalid(index, "Function could not be added"));
                        continue;
                    }
                    results.push_back(mcp::itemOk(index, {{"id", script->id()}, {"name", name}, {"outcome", "created"}}));
                    continue;
                }
                MasterTimer::EditAdmission admission(timer, timer->beginFunctionEdit(existing));
                if (!admission)
                {
                    results.push_back(refusal(index, "running", "Function " + std::to_string(existing->id()) +
                                              " or a function using it is running, starting or flashing"));
                    continue;
                }
                Script *script = qobject_cast<Script*>(existing);
                if (item.contains("path"))
                    script->setPath(QString::fromStdString(item.at("path").get<std::string>()));
                script->setData(qContent);
                admission.end();
                results.push_back(mcp::itemOk(index, {{"id", existing->id()}, {"name", name}, {"outcome", "updated"}}));
            }
            return results.dump();
            });
        }
    )
    .set_description(
            "Create or update Script functions with raw JavaScript. Upserts by name. "
            "Syntax is validated before saving — scripts with errors are rejected with detailed error messages. Batch: wrap entries in {\"items\": [...]}.\n"
            "\n"
            "The content field is raw JavaScript executed by QJSEngine in a dedicated thread. "
            "Full JS: variables, for/while loops, if/else, switch, functions, closures, arrays, objects, "
            "Math.* (sin, cos, pow, sqrt, random, PI, floor, round, min, max, abs, log), Date.\n"
            "\n"
            "ENGINE API (all methods on the global Engine object):\n"
            "  Engine.startFunction(functionID)              Start any QLC+ function\n"
            "  Engine.stopFunction(functionID)               Stop a running function\n"
            "  Engine.isFunctionRunning(functionID) -> bool  Check if function is active\n"
            "  Engine.setFixture(fxID, ch, value)            Set fixture channel (0-255)\n"
            "  Engine.setFixture(fxID, ch, value, fadeMs)    Set with fade time\n"
            "  Engine.getChannelValue(universe, channel)     Read live pre-GM DMX value\n"
            "  Engine.waitTime(milliseconds)                 Pause execution\n"
            "  Engine.waitTime(\"2s.500\")                     Pause using time string\n"
            "  Engine.waitFunctionStart(functionID)          Block until function starts\n"
            "  Engine.waitFunctionStop(functionID)           Block until function stops\n"
            "  Engine.random(min, max) -> int                Random integer in [min,max]\n"
            "  Engine.random(\"1s.0\", \"5s.0\") -> int          Random ms from time strings\n"
            "  Engine.setBlackout(true/false)                Toggle global blackout\n"
            "  Engine.setBPM(bpm)                            Set beat generator BPM\n"
            "  Engine.systemCommand(\"program args\")          Run external process (detached)\n"
            "  Engine.stopOnExit(true/false)                 Auto-stop started functions on exit\n"
            "  Engine.getFunctionAttribute(fID, idx) -> float  Read function attribute\n"
            "  Engine.setFunctionAttribute(fID, idx, val)    Modify running function attribute\n"
            "  Engine.setFunctionAttribute(fID, \"Name\", val) By name e.g. \"Width\", \"Intensity\"\n"
            "  Engine.getBPM() -> int                        Current BPM (internal/MIDI/audio)\n"
            "  Engine.getBeatDuration() -> int               Beat period in ms (e.g. 500 at 120 BPM)\n"
            "  Engine.isBeat() -> bool                       True if current tick is on a beat\n"
            "  Engine.getAudioLevel() -> int                 Audio input volume 0-255\n"
            "  Engine.getAudioFrequency(band, numBands) -> int  Frequency band magnitude 0-255\n"
            "                                                   numBands=3: 0=bass 1=mid 2=high\n"
            "                                                   numBands=16: detailed 16-band EQ\n"
            "  Engine.getOwnID() -> int                      This script's function ID\n"
            "  Engine.getElapsed() -> int                    Ms elapsed since script started\n"
            "  Engine.getEnvelopeDuration() -> int           Total duration from parent chaser step (ms, 0 if standalone)\n"
            "  Engine.getEnvelopeFadeIn() -> int             Fade-in from parent (ms, 0 if not set)\n"
            "  Engine.getEnvelopeFadeOut() -> int            Fade-out from parent (ms, 0 if not set)\n"
            "\n"
            "IMPORTANT: ALWAYS include Engine.waitTime() inside loops to avoid blocking the engine.\n"
            "\n"
            "EXAMPLES (all tested and validated):\n"
            "\n"
            "Simple cue sequence:\n"
            "  Engine.startFunction(10);\n"
            "  Engine.waitTime(5000);\n"
            "  Engine.stopFunction(10);\n"
            "  Engine.startFunction(11);\n"
            "\n"
            "Candle flicker (Gaussian random, warm colors):\n"
            "  function gaussRand(mean, std) {\n"
            "      var u1 = Math.random(), u2 = Math.random();\n"
            "      return mean + std * Math.sqrt(-2*Math.log(u1)) * Math.cos(2*Math.PI*u2);\n"
            "  }\n"
            "  for (var tick = 0; tick < 200; tick++) {\n"
            "      for (var candle = 0; candle < 6; candle++) {\n"
            "          Engine.setFixture(candle, 0, Math.max(100, Math.min(255, Math.round(gaussRand(210, 25)))));\n"
            "          Engine.setFixture(candle, 1, Math.max(180, Math.min(255, Math.round(gaussRand(240, 10)))));\n"
            "          Engine.setFixture(candle, 2, Math.max(80, Math.min(160, Math.round(gaussRand(120, 20)))));\n"
            "          Engine.setFixture(candle, 3, Math.max(0, Math.min(30, Math.round(gaussRand(10, 8)))));\n"
            "      }\n"
            "      Engine.waitTime(Engine.random(30, 120));\n"
            "  }\n"
            "\n"
            "Eased fade (cubic ease in/out):\n"
            "  function easeInOutCubic(t) { return t < 0.5 ? 4*t*t*t : (t-1)*(2*t-2)*(2*t-2)+1; }\n"
            "  var steps = 120;\n"
            "  for (var i = 0; i <= steps; i++) {\n"
            "      var val = Math.round(255 * easeInOutCubic(i / steps));\n"
            "      Engine.setFixture(0, 0, val);\n"
            "      Engine.waitTime(25);\n"
            "  }\n"
            "\n"
            "State machine (idle -> buildup -> peak -> cooldown):\n"
            "  var state = 'idle', stateTime = 0, tick = 50;\n"
            "  for (var i = 0; i < 200; i++) {\n"
            "      switch (state) {\n"
            "          case 'idle': Engine.setFixture(0,0,30); if (stateTime > 2000) { state='buildup'; stateTime=0; } break;\n"
            "          case 'buildup': var v=Math.min(255,30+stateTime/10); Engine.setFixture(0,0,Math.round(v)); if(v>=255){state='peak';stateTime=0;} break;\n"
            "          case 'peak': Engine.setFixture(0,0,Engine.random(200,255)); if(stateTime>1000){state='idle';stateTime=0;} break;\n"
            "      }\n"
            "      Engine.waitTime(tick); stateTime += tick;\n"
            "  }\n"
            "\n"
            "Fixture cascade:\n"
            "  var fixtures = [0,1,2,3,4,5,6,7];\n"
            "  for (var i = 0; i < fixtures.length; i++) { Engine.setFixture(fixtures[i], 0, 255, 500); Engine.waitTime(200); }\n"
            "  Engine.waitTime(1000);\n"
            "  for (var i = fixtures.length-1; i >= 0; i--) { Engine.setFixture(fixtures[i], 0, 0, 500); Engine.waitTime(200); }\n"
            "\n"
            "Reactive follow (mirror leader fixture with variation):\n"
            "  for (var tick = 0; tick < 100; tick++) {\n"
            "      var r = Engine.getChannelValue(0, 1), g = Engine.getChannelValue(0, 2), b = Engine.getChannelValue(0, 3);\n"
            "      for (var i = 1; i < 4; i++) {\n"
            "          Engine.setFixture(i, 1, Math.max(0, Math.min(255, r + Engine.random(-20, 20))));\n"
            "          Engine.setFixture(i, 2, Math.max(0, Math.min(255, g + Engine.random(-20, 20))));\n"
            "          Engine.setFixture(i, 3, Math.max(0, Math.min(255, b + Engine.random(-20, 20))));\n"
            "      }\n"
            "      Engine.waitTime(50);\n"
            "  }\n"
            "\n"
            "BPM-synced strobe:\n"
            "  var beatMs = Engine.getBeatDuration();\n"
            "  for (var beat = 0; beat < 32; beat++) {\n"
            "      Engine.setFixture(0, 0, 255);\n"
            "      Engine.waitTime(50);\n"
            "      Engine.setFixture(0, 0, 0);\n"
            "      Engine.waitTime(beatMs - 50);\n"
            "  }\n"
            "\n"
            "Audio-reactive brightness (bass drives dimmer):\n"
            "  for (var tick = 0; tick < 500; tick++) {\n"
            "      var bass = Engine.getAudioFrequency(0, 3);\n"
            "      var mid = Engine.getAudioFrequency(1, 3);\n"
            "      for (var i = 0; i < 4; i++) {\n"
            "          Engine.setFixture(i, 0, bass);\n"
            "          Engine.setFixture(i, 1, mid);\n"
            "      }\n"
            "      Engine.waitTime(25);\n"
            "  }\n"
            "\n"
            "Reusable envelope-aware buildup (adapts to chaser step duration):\n"
            "  var totalMs = Engine.getEnvelopeDuration();\n"
            "  if (totalMs <= 0) totalMs = 5000; // fallback for standalone\n"
            "  var steps = Math.max(1, Math.round(totalMs / 25));\n"
            "  for (var i = 0; i <= steps; i++) {\n"
            "      var t = i / steps;\n"
            "      Engine.setFixture(0, 0, Math.round(255 * t * t));\n"
            "      Engine.waitTime(25);\n"
            "  }\n"
            "  // Same script in 2-beat step -> 500ms. In 16-beat step -> 4000ms.\n"
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
            if (!args.at("ids").is_array())
                return Json({{"error", "ids must be an array"}}).dump();
            const Json &ids = args.at("ids");
            Json results = Json::array();
            MasterTimer *timer = doc->masterTimer();
            MasterTimer::EditAdmission admitted(timer, timer->beginRegistryEdit());
            for (size_t i = 0; i < ids.size(); ++i)
            {
                auto id = mcp::jsonInteger(ids[i], 0, mcp::kMaxId);
                if (!id)
                {
                    results.push_back(refusal(i, "invalid", mcp::integerError("ids[" + std::to_string(i) + "]", 0, mcp::kMaxId)));
                    continue;
                }
                Function *f = doc->function(quint32(*id));
                if (f == nullptr)
                    results.push_back(refusal(i, "not_found", "Function " + std::to_string(*id) + " not found"));
                else if (!admitted)
                    results.push_back(refusal(i, "functions_running", kRegistryBusy));
                else if (f->flashing())
                    results.push_back(refusal(i, "flashing", "Function " + std::to_string(*id) + " is flashing"));
                // Deleting a bound Scene would make Chaser::slotFunctionRemoved wipe every referring Sequence's steps
                else if (const QList<quint32> referrers = sequenceReferrers(doc, f->id()); !referrers.isEmpty())
                {
                    Json refused = refusal(i, "bound_scene", "Scene " + std::to_string(*id) +
                                           " is bound to Sequences; delete or rebind them first");
                    refused["referrers"] = Json::array();
                    for (quint32 ref : referrers)
                        refused["referrers"].push_back(ref);
                    results.push_back(refused);
                }
                else
                {
                    if (funcMgr)
                        funcMgr->deleteFunction(quint32(*id));
                    else
                        doc->deleteFunction(quint32(*id));
                    results.push_back(mcp::itemOk(i, {{"id", *id}, {"outcome", "deleted"}}));
                }
            }
            admitted.end();
            return results.dump();
            });
        }
    )
    .set_description("Delete functions by ID. Batch: wrap entries in {\"ids\": [...]}. A Scene bound to a Sequence is "
                    "refused (code bound_scene, referrers lists the Sequence ids) until those Sequences are gone.")
    .set_annotations(mcp::kAnnotDestructive));

    {
    const Json speedSchema = {{"type", "object"},
        {"description", "Typed speed: unit ms (integer value) or beats (number value); default and infinite carry no "
                        "value. An edited speed must use the unit of the function's tempoType after the edit."},
        {"properties", {{"unit", {{"type", "string"}, {"enum", {"ms", "beats", "default", "infinite"}}}},
                        {"value", {{"type", "number"}, {"minimum", 0}}}}},
        {"required", {"unit"}}};
    const Json valueSchema = {{"type", "object"}, {"properties", {{"fixtureID", {{"type", "integer"}, {"minimum", 0}}},
        {"channel", {{"type", "integer"}, {"minimum", 0}}}, {"value", {{"type", "integer"}, {"minimum", 0}, {"maximum", 255}}}}},
        {"required", {"fixtureID", "channel", "value"}}};
    const Json modeSchema = {{"type", "string"}, {"enum", {"default", "common", "perStep"}}};
    auto efxInt = [](int max) { return Json{{"type", "integer"}, {"minimum", 0}, {"maximum", max}, {"description", "EFX"}}; };
    const Json idsSchema = {{"type", "array"}, {"items", {{"type", "integer"}, {"minimum", 0}}}};
    const Json boolSchema = {{"type", "boolean"}};
    const Json blendSchema = {{"type", "string"}, {"enum", {"Normal", "Mask", "Additive", "Subtractive"}}};
    const Json channelGroupsSchema = {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
        {"id", {{"type", "integer"}, {"minimum", 0}}},
        {"level", {{"type", "integer"}, {"minimum", 0}, {"maximum", 255}}}}}, {"required", {"id", "level"}}}}};
    const Json efxFixtureSchema = {{"type", "object"}, {"properties", {
        {"fixtureID", {{"type", "integer"}, {"minimum", 0}}},
        {"head", {{"type", "integer"}, {"minimum", 0}}},
        {"direction", {{"type", "string"}, {"enum", {"forward", "backward"}}}},
        {"startOffset", {{"type", "integer"}, {"minimum", 0}, {"maximum", 359}}},
        {"mode", {{"type", "string"}, {"enum", {"Position", "Dimmer", "RGB"}}}}}},
        {"required", {"fixtureID", "head", "direction", "startOffset", "mode"}}};
    // Both tools answer {"items": [one record per input, in order]}; a request
    // error is {"error"} instead. status error records carry code and error.
    auto outputSchema = [](Json recordProps) {
        recordProps["index"] = {{"type", "integer"}, {"minimum", 0}};
        recordProps["status"] = {{"type", "string"}, {"enum", {"ok", "error"}}};
        recordProps["code"] = {{"type", "string"}, {"enum", {"invalid", "not_found", "ambiguous", "unsupported",
                                                            "running", "bound_scene", "functions_running"}}};
        recordProps["error"] = {{"type", "string"}};
        return Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", recordProps},
                                                     {"required", {"index", "status"}}}}}},
            {"error", {{"type", "string"}, {"description", "Request error; items is absent"}}}}}};
    };
    const Json stringSchema = {{"type", "string"}};
    const Json intSchema = {{"type", "integer"}};
    const Json detailOutput = outputSchema({
        {"id", intSchema}, {"name", stringSchema}, {"type", stringSchema}, {"path", stringSchema},
        {"tempoType", {{"type", "string"}, {"enum", {"time", "beats"}}}},
        {"fadeIn", speedSchema}, {"fadeOut", speedSchema}, {"duration", speedSchema},
        {"visible", boolSchema}, {"blendMode", blendSchema},
        {"values", {{"type", "array"}, {"items", valueSchema}}}, {"fixtureIDs", idsSchema}, {"paletteIDs", idsSchema},
        {"fixtureGroupIDs", idsSchema}, {"channelGroups", channelGroupsSchema},
        {"valuesAuthoritative", {{"type", "boolean"}, {"description", "false when a Sequence binds this Scene: its values are the last ones written by Sequence playback or editing"}}},
        {"runOrder", stringSchema}, {"direction", stringSchema},
        {"fadeInMode", modeSchema}, {"fadeOutMode", modeSchema}, {"durationMode", modeSchema},
        {"boundSceneID", intSchema},
        {"steps", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
            {"functionID", intSchema}, {"values", {{"type", "array"}, {"items", valueSchema}}},
            {"fadeIn", speedSchema}, {"hold", speedSchema}, {"fadeOut", speedSchema}, {"note", stringSchema}}}}}}},
        {"algorithm", stringSchema}, {"propagationMode", stringSchema}, {"isRelative", {{"type", "boolean"}}},
        {"dimmerControl", boolSchema},
        {"width", intSchema}, {"height", intSchema}, {"rotation", intSchema}, {"xOffset", intSchema},
        {"yOffset", intSchema}, {"xFrequency", intSchema}, {"yFrequency", intSchema}, {"xPhase", intSchema},
        {"yPhase", intSchema}, {"startOffset", intSchema},
        {"fixtures", {{"type", "array"}, {"items", efxFixtureSchema}}},
        {"functionIDs", idsSchema}, {"content", stringSchema},
        {"detailTool", {{"type", "string"}, {"description", "Types without detail here name the tool that describes them"}}}});
    const Json updateOutput = outputSchema({
        {"id", intSchema}, {"name", stringSchema},
        {"outcome", {{"type", "string"}, {"enum", {"updated"}}}}});

    // query_function_details — typed detail for one ordered list of targets
    tm.register_tool(Tool(
        "query_function_details",
        Json{{"type", "object"}, {"properties", {
            {"targets", {{"type", "array"}, {"minItems", 1},
                {"description", "Ordered targets; result index i answers targets[i]. Each target is {\"id\"} or "
                                "{\"name\", optional \"type\"}; a name must resolve to exactly one function."},
                {"items", {{"type", "object"}, {"properties", {
                    {"id", {{"type", "integer"}, {"minimum", 0}}},
                    {"name", {{"type", "string"}}},
                    {"type", {{"type", "string"}, {"description", "Function type narrowing a name, e.g. Scene, Chaser"}}}
                }}}}}}
        }}, {"required", {"targets"}}},
        detailOutput,
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto err = validateFields(args, {"targets"});
            if (!err.empty()) return Json::parse(err);
            if (!args.contains("targets") || !args.at("targets").is_array() || args.at("targets").empty())
                return Json({{"error", "targets must be a non-empty array"}});

            Json results = Json::array();
            const Json &targets = args.at("targets");
            for (size_t index = 0; index < targets.size(); ++index)
            {
                Function *fn = nullptr;
                Json rejected = resolveTarget(doc, index, targets[index], fn);
                if (!rejected.is_null())
                {
                    results.push_back(rejected);
                    continue;
                }
                results.push_back(mcp::itemOk(index, functionDetails(doc, fn)));
            }
            return Json{{"items", results}};
            });
        }
    )
    .set_description("Read one function's complete authoring detail per target, in target order. Covers Scene, "
                    "Chaser, Sequence, EFX, Collection and Script; RGB/HUE matrices and Shows name the query "
                    "tool that already describes them (detailTool).")
    .set_annotations(mcp::kAnnotReadOnly));

    // update_functions — ID-primary sparse edits, validated in full before admission
    tm.register_tool(Tool(
        "update_functions",
        Json{{"type", "object"}, {"properties", {
            {"items", {{"type", "array"}, {"items", {{"type", "object"}, {"properties", {
                {"target", {{"type", "object"}, {"description", "{\"id\"} or a unique {\"name\", optional \"type\"}"},
                            {"properties", {{"id", {{"type", "integer"}, {"minimum", 0}}},
                                            {"name", {{"type", "string"}}}, {"type", {{"type", "string"}}}}}}},
                {"name", {{"type", "string"}}},
                {"path", {{"type", "string"}}},
                {"tempoType", {{"type", "string"}, {"enum", {"time", "beats"}}}},
                {"fadeIn", speedSchema}, {"fadeOut", speedSchema}, {"duration", speedSchema},
                {"visible", {{"type", "boolean"}, {"description", "false hides the function from function lists"}}},
                {"blendMode", blendSchema},
                {"values", {{"type", "array"}, {"description", "Scene: replaces all channel values"}, {"items", valueSchema}}},
                {"fixtureIDs", {{"type", "array"}, {"description", "Scene: replaces fixture membership, including palette-only fixtures without values; must include every fixture that has values"}, {"items", {{"type", "integer"}, {"minimum", 0}}}}},
                {"paletteIDs", {{"type", "array"}, {"description", "Scene: replaces palette references"}, {"items", {{"type", "integer"}, {"minimum", 0}}}}},
                {"fixtureGroupIDs", {{"type", "array"}, {"description", "Scene: replaces fixture group references"}, {"items", {{"type", "integer"}, {"minimum", 0}}}}},
                {"channelGroups", {{"type", "array"}, {"description", "Scene: replaces channel groups and their levels, in order"},
                                   {"items", channelGroupsSchema["items"]}}},
                {"runOrder", {{"type", "string"}, {"enum", {"loop", "single", "pingpong", "random"}}, {"description", "Chaser, Sequence, EFX (EFX has no random)"}}},
                {"direction", {{"type", "string"}, {"enum", {"forward", "backward"}}}},
                {"fadeInMode", modeSchema}, {"fadeOutMode", modeSchema}, {"durationMode", modeSchema},
                {"steps", {{"type", "array"}, {"description", "Chaser/Sequence: replaces all steps. Chaser steps need functionID; Sequence "
                                                            "steps need values covering every bound Scene channel exactly once."},
                           {"items", {{"type", "object"}, {"properties", {
                               {"functionID", {{"type", "integer"}, {"minimum", 0}}},
                               {"values", {{"type", "array"}, {"items", valueSchema}}},
                               {"fadeIn", speedSchema}, {"hold", speedSchema}, {"fadeOut", speedSchema},
                               {"note", {{"type", "string"}}}}}}}}},
                {"algorithm", {{"type", "string"}, {"enum", {"Circle", "Eight", "Line", "Line2", "Diamond", "Square",
                                                            "SquareChoppy", "SquareTrue", "Leaf", "Lissajous"}}}},
                {"propagationMode", {{"type", "string"}, {"enum", {"Parallel", "Serial", "Asymmetric"}}}},
                {"isRelative", {{"type", "boolean"}}},
                {"dimmerControl", {{"type", "boolean"}, {"description", "EFX: also drive the head dimmer"}}},
                {"width", efxInt(127)}, {"height", efxInt(127)}, {"rotation", efxInt(359)},
                {"xOffset", efxInt(255)}, {"yOffset", efxInt(255)}, {"xFrequency", efxInt(32)}, {"yFrequency", efxInt(32)},
                {"xPhase", efxInt(359)}, {"yPhase", efxInt(359)}, {"startOffset", efxInt(359)},
                {"fixtures", {{"type", "array"}, {"description", "EFX: replaces all fixtures; mode must suit the head"},
                              {"items", efxFixtureSchema}}},
                {"functionIDs", {{"type", "array"}, {"description", "Collection: replaces members"}, {"items", {{"type", "integer"}, {"minimum", 0}}}}},
                {"content", {{"type", "string"}, {"description", "Script: replaces the JavaScript; syntax is checked first"}}}
            }}, {"required", {"target"}}}}}}
        }}, {"required", {"items"}}},
        updateOutput,
        [doc](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto itemsErr = validateItemsArray(args);
            if (itemsErr) return Json::parse(*itemsErr);

            Json results = Json::array();
            const Json &items = args.at("items");
            for (size_t index = 0; index < items.size(); ++index)
            {
                const Json &item = items[index];
                if (!item.is_object() || !item.contains("target"))
                { results.push_back(invalid(index, "item needs a target")); continue; }
                Function *fn = nullptr;
                Json rejected = resolveTarget(doc, index, item.at("target"), fn);
                if (!rejected.is_null()) { results.push_back(rejected); continue; }

                static const QList<Function::Type> supported({ Function::SceneType, Function::ChaserType,
                    Function::SequenceType, Function::EFXType, Function::CollectionType, Function::ScriptType });
                if (!supported.contains(fn->type()))
                {
                    results.push_back(refusal(index, "unsupported", Function::typeToString(fn->type()).toStdString() +
                                              " is edited with its own create tool"));
                    continue;
                }

                const QStringList allowed = QStringList({ "target", "name", "path", "tempoType", "fadeIn", "fadeOut",
                                                          "duration", "visible", "blendMode" }) + typeFields(fn->type());
                std::string unknown;
                for (auto it = item.begin(); it != item.end() && unknown.empty(); ++it)
                    if (!allowed.contains(QString::fromStdString(it.key())))
                        unknown = it.key();
                if (!unknown.empty())
                { results.push_back(invalid(index, "Unknown field for " + Function::typeToString(fn->type()).toStdString() +
                                            ": " + unknown)); continue; }
                if (item.size() < 2)
                { results.push_back(invalid(index, "no fields to update")); continue; }

                std::string err;
                if (item.contains("name") && (!item.at("name").is_string() || item.at("name").get<std::string>().empty()))
                    err = "name must be a non-empty string";
                else if (item.contains("path") && !item.at("path").is_string())
                    err = "path must be a string";
                else if (item.contains("tempoType") && item.at("tempoType") != "time" && item.at("tempoType") != "beats")
                    err = "tempoType must be time or beats";
                else if (item.contains("visible") && !item.at("visible").is_boolean())
                    err = "visible must be a boolean";
                else if (item.contains("blendMode") && !kBlendModes.contains(QString::fromStdString(
                             item.at("blendMode").is_string() ? item.at("blendMode").get<std::string>() : "")))
                    err = "blendMode must be Normal, Mask, Additive or Subtractive";
                const bool beats = item.contains("tempoType") ? item.at("tempoType") == "beats"
                                                              : fn->tempoType() == Function::Beats;
                uint speeds[3] = { 0, 0, 0 };
                const char *speedFields[3] = { "fadeIn", "fadeOut", "duration" };
                for (int i = 0; i < 3 && err.empty(); ++i)
                    if (item.contains(speedFields[i]))
                        err = parseSpeed(item.at(speedFields[i]), speedFields[i], beats, speeds[i]);
                std::vector<StagedOp> ops;
                if (err.empty())
                    err = parseTypeFields(doc, fn, item, beats, ops);
                if (!err.empty()) { results.push_back(invalid(index, err)); continue; }

                auto apply = [&](Function *f)
                {
                    if (item.contains("name"))
                        f->setName(QString::fromStdString(item.at("name").get<std::string>()));
                    if (item.contains("path"))
                        f->setPath(QString::fromStdString(item.at("path").get<std::string>()));
                    if (item.contains("tempoType"))
                        f->setTempoType(beats ? Function::Beats : Function::Time);
                    if (item.contains("visible"))
                        f->setVisible(item.at("visible").get<bool>());
                    if (item.contains("blendMode"))
                        f->setBlendMode(Universe::stringToBlendMode(QString::fromStdString(item.at("blendMode").get<std::string>())));
                    if (item.contains("fadeIn")) f->setFadeInSpeed(speeds[0]);
                    if (item.contains("fadeOut")) f->setFadeOutSpeed(speeds[1]);
                    if (item.contains("duration")) f->setDuration(speeds[2]);
                    for (const StagedOp &op : ops)
                        op(f);
                };

                MasterTimer *timer = doc->masterTimer();
                if (fn->type() == Function::ScriptType)
                {
                    // Script setters dirty the Doc, so a Script is edited in place once admitted
                    MasterTimer::EditAdmission admission(timer, timer->beginFunctionEdit(fn));
                    if (!admission)
                    { results.push_back(refusal(index, "running", "Function " + std::to_string(fn->id()) +
                                                " or a function using it is running, starting or flashing")); continue; }
                    apply(fn);
                    admission.end();
                    doc->setModified();
                    results.push_back(mcp::itemOk(index, {{"id", fn->id()}, {"name", fn->name().toStdString()}, {"outcome", "updated"}}));
                    continue;
                }
                Function *staged = fn->createCopy(doc, false);
                apply(staged);
                if (fn->type() == Function::SceneType && isSequenceBound(doc, fn->id()) &&
                    channelSet(qobject_cast<Scene*>(fn)) != channelSet(qobject_cast<Scene*>(staged)))
                {
                    delete staged;
                    results.push_back(refusal(index, "bound_scene", "Scene " + std::to_string(fn->id()) +
                        " is bound to a Sequence; its channel set cannot change while it is referenced"));
                    continue;
                }
                results.push_back(commitStaged(doc, index, fn, staged));
            }
            return Json{{"items", results}};
            });
        }
    )
    .set_description("Sparse edit of existing Scene, Chaser, Sequence, EFX, Collection and Script functions. Each item "
                    "names one target ({id}, or a name that is unique, optionally narrowed by type) and only the "
                    "fields to change. The whole item is validated before anything is applied; a function that "
                    "is running, starting, queued or flashing, or used by one that is, is refused (code running). "
                    "Timing uses typed speeds as returned by query_function_details. List fields (values, "
                    "paletteIDs, steps, functionIDs) replace the whole list. A Sequence-bound Scene keeps its "
                    "channel set (code bound_scene). Batch: {\"items\": [...]}.")
    .set_annotations(mcp::kAnnotIdempotent));
    }
}
