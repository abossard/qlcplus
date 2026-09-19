/*
  Q Light Controller Plus - Unit test
  profiling_tools_test.cpp

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

#include <QtTest>
#include <nlohmann/json.hpp>

#include "profiling_tools_test.h"
#include "tool_registry.h"
#include "timingdiagnostics.h"

#include <fastmcpp/tools/manager.hpp>

using Json = nlohmann::json;

static const qint64 MS = 1000000;

namespace {

Json parsed(const Json &value)
{
    if (value.is_string())
        return Json::parse(value.get<std::string>());
    return value;
}

Json invoke(const Json &args)
{
    fastmcpp::tools::ToolManager tm;
    registerProfilingTools(tm);
    return parsed(tm.invoke("timing_diagnostics", args));
}

const Json *bucketOfKind(const Json &state, const std::string &kind)
{
    for (const Json &b : state.at("buckets"))
        if (b.at("kind") == kind)
            return &b;
    return nullptr;
}

} // namespace

void McpProfilingTools_Test::init()
{
    // Deterministic window clock and a known-off, empty starting state.
    TimingDiag::setClockForTest([]() { return qint64(0); });
    TimingDiag::setEnabledForTest(false);
    TimingDiag::resetForTest();
}

void McpProfilingTools_Test::cleanup()
{
    TimingDiag::setEnabledForTest(false);
    TimingDiag::setClockForTest(nullptr);
    TimingDiag::resetForTest();
}

// ─── validation (C0-1) ───────────────────────────────────────────────────────

void McpProfilingTools_Test::missingAction_rejected()
{
    const Json r = invoke(Json::object());
    QVERIFY2(r.contains("error"), r.dump().c_str());
}

void McpProfilingTools_Test::unknownField_rejected()
{
    const Json r = invoke({{"action", "enable"}, {"bogus", 1}});
    QVERIFY2(r.contains("error"), r.dump().c_str());
    QVERIFY2(r.at("error").get<std::string>().find("bogus") != std::string::npos,
             r.dump().c_str());
}

void McpProfilingTools_Test::invalidAction_rejected()
{
    const Json r = invoke({{"action", "frobnicate"}});
    QVERIFY2(r.contains("error"), r.dump().c_str());
    // A rejected action must not have flipped the gate on.
    QVERIFY(!TimingDiag::enabled());
}

void McpProfilingTools_Test::setIntervalRequiresPositiveInterval()
{
    QVERIFY2(invoke({{"action", "set_interval"}}).contains("error"), "missing intervalMs");
    QVERIFY2(invoke({{"action", "set_interval"}, {"intervalMs", 0}}).contains("error"), "zero");
    QVERIFY2(invoke({{"action", "set_interval"}, {"intervalMs", -5}}).contains("error"), "negative");

    const Json ok = invoke({{"action", "set_interval"}, {"intervalMs", 250}});
    QVERIFY2(!ok.contains("error"), ok.dump().c_str());
    QCOMPARE(ok.at("state").at("intervalMs").get<qint64>(), qint64(250));
}

void McpProfilingTools_Test::intervalOnlyValidWithSetInterval()
{
    const Json r = invoke({{"action", "enable"}, {"intervalMs", 100}});
    QVERIFY2(r.contains("error"), r.dump().c_str());
    QVERIFY(!TimingDiag::enabled());   // inapplicable field rejected before state change
}

// ─── production round trip (C0-1) ────────────────────────────────────────────

void McpProfilingTools_Test::enableRecordSnapshotResetRoundTrip()
{
    // enable -> fresh capture, sampling on.
    const Json en = invoke({{"action", "enable"}});
    QVERIFY2(!en.contains("error"), en.dump().c_str());
    QVERIFY(en.at("state").at("enabled").get<bool>());
    const qint64 cap = en.at("state").at("captureId").get<qint64>();

    // Record through the production recorder (the same call the live scheduler
    // and HUE handoff make), tagged with the live capture id.
    TimingDiag::schedulerDispatch(20 * MS, 3 * MS, true, 5 * MS, 0, 0,
                                  TimingDiag::captureId());
    TimingDiag::hueHandoff(3, "BASSHUE", 40 * MS, 2 * MS, 35 * MS, 12 * MS,
                           TimingDiag::captureId());

    const Json snap = invoke({{"action", "snapshot"}});
    QVERIFY2(!snap.contains("error"), snap.dump().c_str());
    const Json &state = snap.at("state");
    QCOMPARE(state.at("captureId").get<qint64>(), cap);
    QCOMPARE(state.at("discardedSamples").get<qint64>(), qint64(0));
    QCOMPARE(state.at("buckets").size(), size_t(2));

    // reset -> new identity, cleared aggregates.
    const Json rst = invoke({{"action", "reset"}});
    QVERIFY(rst.at("state").at("captureId").get<qint64>() > cap);
    QCOMPARE(rst.at("state").at("buckets").size(), size_t(0));
}

void McpProfilingTools_Test::snapshotExposesRawWorkerCpuFields()
{
    invoke({{"action", "enable"}});
    TimingDiag::hueHandoff(3, "BASSHUE", 40 * MS, 2 * MS, 35 * MS, 20 * MS,
                           TimingDiag::captureId());   // cpu known
    TimingDiag::hueHandoff(3, "BASSHUE", 12 * MS, 1 * MS, 9 * MS, -1,
                           TimingDiag::captureId());    // cpu unknown

    const Json snap = invoke({{"action", "snapshot"}});
    const Json *hue = bucketOfKind(snap.at("state"), "hue");
    QVERIFY(hue != nullptr);
    QCOMPARE(hue->at("count").get<qint64>(), qint64(2));
    QCOMPARE(hue->at("workerNs").get<qint64>(), qint64(35 * MS));    // worst wait's worker
    QCOMPARE(hue->at("workerCpuNs").get<qint64>(), qint64(20 * MS)); // its paired cpu
    QCOMPARE(hue->at("workerOffCpuNs").get<qint64>(), qint64(15 * MS));
    QCOMPARE(hue->at("cpuUnknownRenders").get<qint64>(), qint64(1)); // one render unknown cpu
}

// ─── retention semantics (C0-5) ──────────────────────────────────────────────

void McpProfilingTools_Test::disableRetainsUntilResetOrReenable()
{
    invoke({{"action", "enable"}});
    TimingDiag::hueHandoff(3, "BASSHUE", 40 * MS, 2 * MS, 35 * MS, 12 * MS,
                           TimingDiag::captureId());

    const Json dis = invoke({{"action", "disable"}});
    QVERIFY(!dis.at("state").at("enabled").get<bool>());
    QCOMPARE(dis.at("state").at("buckets").size(), size_t(1));   // retained after disable

    // Non-consuming: a repeated snapshot still shows it.
    const Json snap2 = invoke({{"action", "snapshot"}});
    QCOMPARE(snap2.at("state").at("buckets").size(), size_t(1));

    // Re-enable starts a fresh capture and clears the retained aggregates.
    const Json en2 = invoke({{"action", "enable"}});
    QCOMPARE(en2.at("state").at("buckets").size(), size_t(0));
}

QTEST_APPLESS_MAIN(McpProfilingTools_Test)
