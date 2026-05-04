/*
  Q Light Controller Plus - Unit test
  script_tool_test.cpp

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

#include "script_tool_test.h"
#include "tool_registry.h"
#include "idempotency.h"
#include "doc.h"
#include "scriptv4.h"

#include <fastmcpp/tools/manager.hpp>

using Json = nlohmann::json;

// ─── Helper: dispatch the real create_scripts MCP tool ─────────────────────

Json ScriptTool_Test::callCreateScript(const std::string &name,
                                        const std::string &content,
                                        const std::string &path)
{
    Json item = {{"name", name}, {"content", content}};
    if (!path.empty())
        item["path"] = path;

    Json args = {{"items", Json::array({item})}};
    Json raw = m_tm->invoke("create_scripts", args);

    Json parsed = raw.is_string() ? Json::parse(raw.get<std::string>()) : raw;

    // Top-level error (e.g. missing items array) — return as-is so callers see error key.
    if (parsed.is_object() && parsed.contains("error"))
        return parsed;

    // Batch tools return a JSON array — extract the single result.
    if (parsed.is_array() && !parsed.empty())
        return parsed[0];

    return parsed;
}

// ─── Test setup ─────────────────────────────────────────────────────────────

void ScriptTool_Test::init()
{
    m_doc = new Doc(this);
    m_tm = new fastmcpp::tools::ToolManager();
    registerFunctionTools(*m_tm, m_doc);
}

void ScriptTool_Test::cleanup()
{
    delete m_tm;
    m_tm = nullptr;
    delete m_doc;
    m_doc = nullptr;
}

// ═══════════════════════════════════════════════════════════════════════════
// Real-dispatch error shape sanity
// ═══════════════════════════════════════════════════════════════════════════

void ScriptTool_Test::dispatch_missingItemsRejected()
{
    Json raw = m_tm->invoke("create_scripts", Json::object());
    Json parsed = raw.is_string() ? Json::parse(raw.get<std::string>()) : raw;
    QVERIFY(parsed.is_object());
    QVERIFY(parsed.contains("error"));
}

void ScriptTool_Test::dispatch_unknownFieldRejected()
{
    Json args = {{"items", Json::array({
        {{"name", "Bad"}, {"content", "Engine.waitTime(1);"}, {"bogus", 42}}
    })}};
    Json raw = m_tm->invoke("create_scripts", args);
    Json parsed = raw.is_string() ? Json::parse(raw.get<std::string>()) : raw;
    QVERIFY(parsed.is_array());
    QVERIFY(!parsed.empty());
    QVERIFY(parsed[0].contains("error"));
}

// ═══════════════════════════════════════════════════════════════════════════
// Basic CRUD
// ═══════════════════════════════════════════════════════════════════════════

void ScriptTool_Test::create_simpleScript()
{
    auto result = callCreateScript("Hello", "Engine.setBlackout(true);");
    QVERIFY(result.contains("id"));
    QCOMPARE(result["status"].get<std::string>(), std::string("created"));

    Function *fn = mcp::findFunction(m_doc, "Hello", Function::ScriptType);
    QVERIFY(fn != nullptr);
    Script *s = qobject_cast<Script*>(fn);
    QVERIFY(s != nullptr);
    QVERIFY(s->data().contains("Engine.setBlackout(true)"));
}

void ScriptTool_Test::create_upsertReplaces()
{
    auto r1 = callCreateScript("Upsert", "Engine.setBPM(120);");
    QCOMPARE(r1["status"].get<std::string>(), std::string("created"));
    int id1 = r1["id"].get<int>();

    auto r2 = callCreateScript("Upsert", "Engine.setBPM(140);");
    QCOMPARE(r2["status"].get<std::string>(), std::string("updated"));
    QCOMPARE(r2["id"].get<int>(), id1);

    Script *s = qobject_cast<Script*>(m_doc->function(id1));
    QVERIFY(s != nullptr);
    QVERIFY(s->data().contains("140"));
    QVERIFY(!s->data().contains("120"));
}

void ScriptTool_Test::create_emptyContentRejected()
{
    auto result = callCreateScript("Empty", "");
    QVERIFY(result.contains("error"));
    QVERIFY(mcp::findFunction(m_doc, "Empty", Function::ScriptType) == nullptr);
}

void ScriptTool_Test::create_missingContentRejected()
{
    auto result = callCreateScript("NoContent", "");
    QVERIFY(result.contains("error"));
}

void ScriptTool_Test::create_withPath()
{
    auto result = callCreateScript("Pathed", "Engine.setBlackout(false);", "Utilities/Scripts");
    QCOMPARE(result["status"].get<std::string>(), std::string("created"));

    Script *s = qobject_cast<Script*>(mcp::findFunction(m_doc, "Pathed", Function::ScriptType));
    QVERIFY(s != nullptr);
    QVERIFY(s->path().endsWith("Utilities/Scripts"));
}

// ═══════════════════════════════════════════════════════════════════════════
// Syntax validation (blocking)
// ═══════════════════════════════════════════════════════════════════════════

void ScriptTool_Test::syntax_validJsAccepted()
{
    auto result = callCreateScript("Valid", "var x = 1; Engine.waitTime(x);");
    QVERIFY(!result.contains("error"));
    QVERIFY(result.contains("id"));
}

void ScriptTool_Test::syntax_invalidJsRejected()
{
    auto result = callCreateScript("Invalid", "function { broken syntax !!!!");
    QVERIFY(result.contains("error"));
    QVERIFY(result.contains("syntaxErrors"));
    QVERIFY(result["syntaxErrors"].is_array());
    QVERIFY(!result["syntaxErrors"].empty());
    // Script should NOT be created
    QVERIFY(mcp::findFunction(m_doc, "Invalid", Function::ScriptType) == nullptr);
}

void ScriptTool_Test::syntax_unclosedParenRejected()
{
    auto result = callCreateScript("Unclosed", "Engine.startFunction(1");
    QVERIFY(result.contains("error"));
    QVERIFY(result.contains("syntaxErrors"));
    QVERIFY(mcp::findFunction(m_doc, "Unclosed", Function::ScriptType) == nullptr);
}

void ScriptTool_Test::syntax_errorReportsLineNumber()
{
    // Line 1 is valid, line 2 has an error
    auto result = callCreateScript("LineNum",
        "Engine.waitTime(100);\n"
        "var x = {;");
    QVERIFY(result.contains("syntaxErrors"));
    // Error message should contain a line number reference
    std::string errMsg = result["syntaxErrors"][0].get<std::string>();
    QVERIFY(!errMsg.empty());
}

void ScriptTool_Test::syntax_rejectedScriptNotCreated()
{
    auto result = callCreateScript("Ghost", "{{{{");
    QVERIFY(result.contains("error"));
    QVERIFY(mcp::findFunction(m_doc, "Ghost", Function::ScriptType) == nullptr);
    // Verify no orphan functions in doc
    QCOMPARE(m_doc->functions().count(), 0);
}

void ScriptTool_Test::syntax_rejectedUpdateRestoresOriginal()
{
    // Create a valid script first
    auto r1 = callCreateScript("Restore", "Engine.setBPM(120);");
    QVERIFY(r1.contains("id"));

    // Try to update with invalid JS — should fail and restore
    auto r2 = callCreateScript("Restore", "{{{{invalid}}}}");
    QVERIFY(r2.contains("error"));

    // Original content should be preserved
    Script *s = qobject_cast<Script*>(mcp::findFunction(m_doc, "Restore", Function::ScriptType));
    QVERIFY(s != nullptr);
    QVERIFY(s->data().contains("Engine.setBPM(120)"));
}

// ═══════════════════════════════════════════════════════════════════════════
// Engine API coverage — all methods must pass syntax check
// ═══════════════════════════════════════════════════════════════════════════

void ScriptTool_Test::engineApi_startStopFunction()
{
    auto r = callCreateScript("StartStop",
        "Engine.startFunction(1);\n"
        "Engine.stopFunction(1);");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

void ScriptTool_Test::engineApi_setFixtureBasic()
{
    auto r = callCreateScript("SetFixture", "Engine.setFixture(0, 0, 255);");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

void ScriptTool_Test::engineApi_setFixtureWithFade()
{
    auto r = callCreateScript("SetFixtureFade", "Engine.setFixture(0, 0, 255, 2000);");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

void ScriptTool_Test::engineApi_waitTimeMs()
{
    auto r = callCreateScript("WaitMs", "Engine.waitTime(1000);");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

void ScriptTool_Test::engineApi_waitTimeString()
{
    auto r = callCreateScript("WaitStr", "Engine.waitTime(\"2s.500\");");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

void ScriptTool_Test::engineApi_blackout()
{
    auto r = callCreateScript("Blackout",
        "Engine.setBlackout(true);\n"
        "Engine.setBlackout(false);");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

void ScriptTool_Test::engineApi_setBPM()
{
    auto r = callCreateScript("BPM", "Engine.setBPM(128);");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

void ScriptTool_Test::engineApi_randomInt()
{
    auto r = callCreateScript("RandomInt", "var x = Engine.random(0, 255);");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

void ScriptTool_Test::engineApi_randomString()
{
    auto r = callCreateScript("RandomStr", "var x = Engine.random(\"1s.0\", \"5s.0\");");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

void ScriptTool_Test::engineApi_systemCommand()
{
    auto r = callCreateScript("SysCmd", "Engine.systemCommand(\"/bin/echo hello\");");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

void ScriptTool_Test::engineApi_stopOnExit()
{
    auto r = callCreateScript("StopOnExit", "Engine.stopOnExit(false);");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

void ScriptTool_Test::engineApi_isFunctionRunning()
{
    auto r = callCreateScript("IsRunning", "var running = Engine.isFunctionRunning(1);");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

void ScriptTool_Test::engineApi_getChannelValue()
{
    auto r = callCreateScript("GetCh", "var val = Engine.getChannelValue(0, 0);");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

void ScriptTool_Test::engineApi_waitFunctionStartStop()
{
    auto r = callCreateScript("WaitFunc",
        "Engine.waitFunctionStart(1);\n"
        "Engine.waitFunctionStop(1);");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

void ScriptTool_Test::engineApi_getFunctionAttribute()
{
    auto r = callCreateScript("GetAttr", "var v = Engine.getFunctionAttribute(1, 0);");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

void ScriptTool_Test::engineApi_setFunctionAttributeByIndex()
{
    auto r = callCreateScript("SetAttrIdx", "Engine.setFunctionAttribute(1, 0, 0.5);");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

void ScriptTool_Test::engineApi_setFunctionAttributeByName()
{
    auto r = callCreateScript("SetAttrName", "Engine.setFunctionAttribute(1, \"Intensity\", 0.5);");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

// ═══════════════════════════════════════════════════════════════════════════
// Advanced JavaScript patterns — verify these pass syntax check
// ═══════════════════════════════════════════════════════════════════════════

void ScriptTool_Test::js_forLoop()
{
    auto r = callCreateScript("ForLoop",
        "for (var i = 0; i < 8; i++) {\n"
        "    Engine.setFixture(i, 0, 255);\n"
        "}\n");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

void ScriptTool_Test::js_whileLoopWithWait()
{
    auto r = callCreateScript("WhileWait",
        "var count = 0;\n"
        "while (count < 10) {\n"
        "    Engine.setFixture(0, 0, Engine.random(0, 255));\n"
        "    Engine.waitTime(100);\n"
        "    count++;\n"
        "}\n");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

void ScriptTool_Test::js_ifElseConditional()
{
    auto r = callCreateScript("IfElse",
        "var running = Engine.isFunctionRunning(1);\n"
        "if (running) {\n"
        "    Engine.stopFunction(1);\n"
        "} else {\n"
        "    Engine.startFunction(1);\n"
        "}\n");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

void ScriptTool_Test::js_mathFunctions()
{
    auto r = callCreateScript("MathFns",
        "var a = Math.sin(0.5);\n"
        "var b = Math.cos(Math.PI);\n"
        "var c = Math.pow(2, 8);\n"
        "var d = Math.sqrt(144);\n"
        "var e = Math.round(3.7);\n"
        "var f = Math.floor(3.9);\n"
        "var g = Math.ceil(3.1);\n"
        "var h = Math.min(10, 20);\n"
        "var i = Math.max(10, 20);\n"
        "var j = Math.abs(-42);\n"
        "var k = Math.log(Math.E);\n"
        "var l = Math.random();\n"
        "var m = Math.atan2(1, 1);\n"
        "var n = Math.PI;\n");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

void ScriptTool_Test::js_variablesAndState()
{
    auto r = callCreateScript("VarState",
        "var brightness = 0;\n"
        "var increment = 5;\n"
        "for (var step = 0; step < 51; step++) {\n"
        "    brightness = Math.min(255, brightness + increment);\n"
        "    Engine.setFixture(0, 0, Math.round(brightness));\n"
        "    Engine.waitTime(100);\n"
        "}\n");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

void ScriptTool_Test::js_helperFunctions()
{
    auto r = callCreateScript("Helpers",
        "function clamp(val, lo, hi) {\n"
        "    return Math.max(lo, Math.min(hi, val));\n"
        "}\n"
        "function setAll(ch, val) {\n"
        "    for (var i = 0; i < 4; i++) {\n"
        "        Engine.setFixture(i, ch, clamp(val, 0, 255));\n"
        "    }\n"
        "}\n"
        "setAll(0, 200);\n");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

void ScriptTool_Test::js_arraysAndObjects()
{
    auto r = callCreateScript("ArrayObj",
        "var fixtures = [0, 1, 2, 3];\n"
        "var config = {red: 255, green: 128, blue: 0};\n"
        "for (var i = 0; i < fixtures.length; i++) {\n"
        "    Engine.setFixture(fixtures[i], 1, config.red);\n"
        "    Engine.setFixture(fixtures[i], 2, config.green);\n"
        "    Engine.setFixture(fixtures[i], 3, config.blue);\n"
        "}\n");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

void ScriptTool_Test::js_easingFunction()
{
    auto r = callCreateScript("Easing",
        "function easeInOutSine(t) {\n"
        "    return -(Math.cos(Math.PI * t) - 1) / 2;\n"
        "}\n"
        "function easeOutBounce(t) {\n"
        "    if (t < 1/2.75) return 7.5625*t*t;\n"
        "    if (t < 2/2.75) { t -= 1.5/2.75; return 7.5625*t*t + 0.75; }\n"
        "    if (t < 2.5/2.75) { t -= 2.25/2.75; return 7.5625*t*t + 0.9375; }\n"
        "    t -= 2.625/2.75; return 7.5625*t*t + 0.984375;\n"
        "}\n"
        "var steps = 100;\n"
        "for (var i = 0; i <= steps; i++) {\n"
        "    var t = i / steps;\n"
        "    var val = Math.round(255 * easeInOutSine(t));\n"
        "    Engine.setFixture(0, 0, val);\n"
        "    Engine.waitTime(30);\n"
        "}\n");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

void ScriptTool_Test::js_nestedLoops()
{
    auto r = callCreateScript("NestedLoop",
        "for (var cycle = 0; cycle < 3; cycle++) {\n"
        "    for (var fix = 0; fix < 8; fix++) {\n"
        "        Engine.setFixture(fix, 0, 255, 500);\n"
        "        Engine.waitTime(200);\n"
        "    }\n"
        "    Engine.waitTime(1000);\n"
        "}\n");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

void ScriptTool_Test::js_closures()
{
    auto r = callCreateScript("Closures",
        "function makeCounter(start) {\n"
        "    var count = start;\n"
        "    return function() { return count++; };\n"
        "}\n"
        "var counter = makeCounter(0);\n"
        "var a = counter();\n"
        "var b = counter();\n");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

void ScriptTool_Test::js_switchStatement()
{
    auto r = callCreateScript("Switch",
        "var mode = 2;\n"
        "switch (mode) {\n"
        "    case 1: Engine.setBPM(120); break;\n"
        "    case 2: Engine.setBPM(140); break;\n"
        "    default: Engine.setBPM(100); break;\n"
        "}\n");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

// ═══════════════════════════════════════════════════════════════════════════
// Creative pattern validation — realistic scripts must pass syntax check
// ═══════════════════════════════════════════════════════════════════════════

void ScriptTool_Test::pattern_candleFlicker()
{
    auto r = callCreateScript("CandleFlicker",
        "function gaussRand(mean, std) {\n"
        "    var u1 = Math.random(), u2 = Math.random();\n"
        "    return mean + std * Math.sqrt(-2*Math.log(u1)) * Math.cos(2*Math.PI*u2);\n"
        "}\n"
        "for (var tick = 0; tick < 200; tick++) {\n"
        "    for (var candle = 0; candle < 6; candle++) {\n"
        "        var dim = Math.max(100, Math.min(255, Math.round(gaussRand(210, 25))));\n"
        "        var red = Math.max(180, Math.min(255, Math.round(gaussRand(240, 10))));\n"
        "        var green = Math.max(80, Math.min(160, Math.round(gaussRand(120, 20))));\n"
        "        var blue = Math.max(0, Math.min(30, Math.round(gaussRand(10, 8))));\n"
        "        Engine.setFixture(candle, 0, dim);\n"
        "        Engine.setFixture(candle, 1, red);\n"
        "        Engine.setFixture(candle, 2, green);\n"
        "        Engine.setFixture(candle, 3, blue);\n"
        "    }\n"
        "    Engine.waitTime(Engine.random(30, 120));\n"
        "}\n");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

void ScriptTool_Test::pattern_sunriseRamp()
{
    auto r = callCreateScript("SunriseRamp",
        "var brightness = 0;\n"
        "var steps = 600;\n"
        "var increment = 255 / steps;\n"
        "for (var i = 0; i < steps; i++) {\n"
        "    brightness = Math.min(255, Math.round(brightness + increment));\n"
        "    for (var fix = 0; fix < 12; fix++) {\n"
        "        Engine.setFixture(fix, 0, brightness);\n"
        "    }\n"
        "    Engine.waitTime(1000);\n"
        "}\n");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

void ScriptTool_Test::pattern_randomSceneSequencer()
{
    auto r = callCreateScript("SceneSeq",
        "var scenes = [\n"
        "    {id: 10, weight: 5},\n"
        "    {id: 11, weight: 3},\n"
        "    {id: 12, weight: 2},\n"
        "    {id: 13, weight: 1}\n"
        "];\n"
        "function weightedPick(items) {\n"
        "    var total = 0;\n"
        "    for (var i = 0; i < items.length; i++) total += items[i].weight;\n"
        "    var roll = Math.random() * total;\n"
        "    var cum = 0;\n"
        "    for (var i = 0; i < items.length; i++) {\n"
        "        cum += items[i].weight;\n"
        "        if (roll < cum) return items[i];\n"
        "    }\n"
        "    return items[items.length - 1];\n"
        "}\n"
        "var lastScene = -1;\n"
        "for (var cycle = 0; cycle < 20; cycle++) {\n"
        "    var pick = weightedPick(scenes);\n"
        "    if (lastScene >= 0) Engine.stopFunction(lastScene);\n"
        "    Engine.startFunction(pick.id);\n"
        "    lastScene = pick.id;\n"
        "    Engine.waitTime(Engine.random(2000, 10000));\n"
        "}\n"
        "if (lastScene >= 0) Engine.stopFunction(lastScene);\n");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

void ScriptTool_Test::pattern_stormLightning()
{
    auto r = callCreateScript("Storm",
        "function flash(fixtures, intensity, duration) {\n"
        "    for (var i = 0; i < fixtures.length; i++)\n"
        "        Engine.setFixture(fixtures[i], 0, intensity);\n"
        "    Engine.waitTime(duration);\n"
        "    for (var i = 0; i < fixtures.length; i++)\n"
        "        Engine.setFixture(fixtures[i], 0, 0);\n"
        "}\n"
        "var stormFixtures = [0, 1, 2, 3];\n"
        "for (var strike = 0; strike < 10; strike++) {\n"
        "    var waitMs = Math.round(-Math.log(1 - Math.random()) * 3000);\n"
        "    waitMs = Math.max(500, Math.min(10000, waitMs));\n"
        "    Engine.waitTime(waitMs);\n"
        "    var numFlashes = Engine.random(1, 4);\n"
        "    for (var f = 0; f < numFlashes; f++) {\n"
        "        flash(stormFixtures, Engine.random(200, 255), Engine.random(20, 80));\n"
        "        Engine.waitTime(Engine.random(30, 150));\n"
        "    }\n"
        "}\n");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

void ScriptTool_Test::pattern_sineBreathing()
{
    auto r = callCreateScript("Breathing",
        "var phase = 0;\n"
        "for (var tick = 0; tick < 200; tick++) {\n"
        "    var brightness = Math.round(127 + 127 * Math.sin(phase));\n"
        "    Engine.setFixture(0, 0, brightness);\n"
        "    phase += 0.05;\n"
        "    Engine.waitTime(30);\n"
        "}\n");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

void ScriptTool_Test::pattern_fixtureCascade()
{
    auto r = callCreateScript("Cascade",
        "var fixtures = [0, 1, 2, 3, 4, 5, 6, 7];\n"
        "for (var i = 0; i < fixtures.length; i++) {\n"
        "    Engine.setFixture(fixtures[i], 0, 255, 500);\n"
        "    Engine.waitTime(200);\n"
        "}\n"
        "Engine.waitTime(1000);\n"
        "for (var i = fixtures.length - 1; i >= 0; i--) {\n"
        "    Engine.setFixture(fixtures[i], 0, 0, 500);\n"
        "    Engine.waitTime(200);\n"
        "}\n");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

void ScriptTool_Test::pattern_bpmStrobe()
{
    auto r = callCreateScript("BpmStrobe",
        "var bpm = 128;\n"
        "Engine.setBPM(bpm);\n"
        "var beatMs = Math.round(60000 / bpm);\n"
        "for (var beat = 0; beat < 32; beat++) {\n"
        "    Engine.setFixture(0, 0, 255);\n"
        "    Engine.waitTime(50);\n"
        "    Engine.setFixture(0, 0, 0);\n"
        "    Engine.waitTime(beatMs - 50);\n"
        "}\n");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

void ScriptTool_Test::pattern_stateMachine()
{
    auto r = callCreateScript("StateMachine",
        "var state = 'idle';\n"
        "var stateTime = 0;\n"
        "var tick = 50;\n"
        "for (var i = 0; i < 100; i++) {\n"
        "    switch (state) {\n"
        "        case 'idle':\n"
        "            Engine.setFixture(0, 0, 30);\n"
        "            if (stateTime > 2000) { state = 'buildup'; stateTime = 0; }\n"
        "            break;\n"
        "        case 'buildup':\n"
        "            var intensity = Math.min(255, 30 + stateTime / 10);\n"
        "            Engine.setFixture(0, 0, Math.round(intensity));\n"
        "            if (intensity >= 255) { state = 'peak'; stateTime = 0; }\n"
        "            break;\n"
        "        case 'peak':\n"
        "            Engine.setFixture(0, 0, Engine.random(200, 255));\n"
        "            if (stateTime > 1000) { state = 'idle'; stateTime = 0; }\n"
        "            break;\n"
        "    }\n"
        "    Engine.waitTime(tick);\n"
        "    stateTime += tick;\n"
        "}\n");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

void ScriptTool_Test::pattern_reactiveFollow()
{
    auto r = callCreateScript("ReactiveFollow",
        "for (var tick = 0; tick < 100; tick++) {\n"
        "    var leaderRed = Engine.getChannelValue(0, 1);\n"
        "    var leaderGreen = Engine.getChannelValue(0, 2);\n"
        "    var leaderBlue = Engine.getChannelValue(0, 3);\n"
        "    for (var i = 1; i < 4; i++) {\n"
        "        var rVar = Math.max(0, Math.min(255, leaderRed + Engine.random(-20, 20)));\n"
        "        var gVar = Math.max(0, Math.min(255, leaderGreen + Engine.random(-20, 20)));\n"
        "        var bVar = Math.max(0, Math.min(255, leaderBlue + Engine.random(-20, 20)));\n"
        "        Engine.setFixture(i, 1, rVar);\n"
        "        Engine.setFixture(i, 2, gVar);\n"
        "        Engine.setFixture(i, 3, bVar);\n"
        "    }\n"
        "    Engine.waitTime(50);\n"
        "}\n");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

void ScriptTool_Test::pattern_easedFade()
{
    auto r = callCreateScript("EasedFade",
        "function easeInOutCubic(t) {\n"
        "    return t < 0.5 ? 4*t*t*t : (t-1)*(2*t-2)*(2*t-2)+1;\n"
        "}\n"
        "function easedFade(fixID, ch, startVal, endVal, durationMs) {\n"
        "    var steps = Math.max(1, Math.round(durationMs / 25));\n"
        "    var stepTime = Math.round(durationMs / steps);\n"
        "    for (var i = 0; i <= steps; i++) {\n"
        "        var t = i / steps;\n"
        "        var eased = easeInOutCubic(t);\n"
        "        var value = Math.round(startVal + (endVal - startVal) * eased);\n"
        "        value = Math.max(0, Math.min(255, value));\n"
        "        Engine.setFixture(fixID, ch, value);\n"
        "        Engine.waitTime(stepTime);\n"
        "    }\n"
        "}\n"
        "easedFade(0, 0, 0, 255, 3000);\n"
        "easedFade(0, 0, 255, 0, 3000);\n");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

void ScriptTool_Test::pattern_bpmReactive()
{
    auto r = callCreateScript("BpmReactive",
        "var bpm = Engine.getBPM();\n"
        "var beatMs = Engine.getBeatDuration();\n"
        "if (bpm > 0 && beatMs > 0) {\n"
        "    for (var beat = 0; beat < 16; beat++) {\n"
        "        Engine.setFixture(0, 0, 255);\n"
        "        Engine.waitTime(Math.round(beatMs / 4));\n"
        "        Engine.setFixture(0, 0, 0);\n"
        "        Engine.waitTime(Math.round(beatMs * 3 / 4));\n"
        "    }\n"
        "}\n");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

void ScriptTool_Test::pattern_audioReactive()
{
    auto r = callCreateScript("AudioReactive",
        "for (var tick = 0; tick < 200; tick++) {\n"
        "    var level = Engine.getAudioLevel();\n"
        "    var bass = Engine.getAudioFrequency(0, 3);\n"
        "    var mid = Engine.getAudioFrequency(1, 3);\n"
        "    var high = Engine.getAudioFrequency(2, 3);\n"
        "    Engine.setFixture(0, 0, level);\n"
        "    Engine.setFixture(0, 1, bass);\n"
        "    Engine.setFixture(0, 2, mid);\n"
        "    Engine.setFixture(0, 3, high);\n"
        "    Engine.waitTime(25);\n"
        "}\n");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

void ScriptTool_Test::pattern_audioSpectrum16Band()
{
    auto r = callCreateScript("Spectrum16",
        "for (var tick = 0; tick < 100; tick++) {\n"
        "    for (var band = 0; band < 16; band++) {\n"
        "        var mag = Engine.getAudioFrequency(band, 16);\n"
        "        Engine.setFixture(band, 0, mag);\n"
        "    }\n"
        "    Engine.waitTime(25);\n"
        "}\n");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

// ── New Engine API: BPM + Audio ────────────────────────────────────────────

void ScriptTool_Test::engineApi_getBPM()
{
    auto r = callCreateScript("GetBPM", "var bpm = Engine.getBPM();");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

void ScriptTool_Test::engineApi_getBeatDuration()
{
    auto r = callCreateScript("GetBeatDur", "var beatMs = Engine.getBeatDuration();");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

void ScriptTool_Test::engineApi_isBeat()
{
    auto r = callCreateScript("IsBeat", "var onBeat = Engine.isBeat();");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

void ScriptTool_Test::engineApi_getAudioLevel()
{
    auto r = callCreateScript("GetAudio", "var level = Engine.getAudioLevel();");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

void ScriptTool_Test::engineApi_getAudioFrequency()
{
    auto r = callCreateScript("GetFreq",
        "var bass = Engine.getAudioFrequency(0, 3);\n"
        "var mid = Engine.getAudioFrequency(1, 3);\n"
        "var high = Engine.getAudioFrequency(2, 3);\n"
        "var sub = Engine.getAudioFrequency(0, 16);\n");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

// ── Envelope awareness ─────────────────────────────────────────────────────

void ScriptTool_Test::engineApi_getOwnID()
{
    auto r = callCreateScript("GetOwnID", "var myID = Engine.getOwnID();");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

void ScriptTool_Test::engineApi_getElapsed()
{
    auto r = callCreateScript("GetElapsed", "var ms = Engine.getElapsed();");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

void ScriptTool_Test::engineApi_getEnvelopeDuration()
{
    auto r = callCreateScript("GetEnvDur", "var dur = Engine.getEnvelopeDuration();");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

void ScriptTool_Test::engineApi_getEnvelopeFadeInOut()
{
    auto r = callCreateScript("GetEnvFade",
        "var fi = Engine.getEnvelopeFadeIn();\n"
        "var fo = Engine.getEnvelopeFadeOut();\n");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

void ScriptTool_Test::pattern_envelopeAdaptive()
{
    auto r = callCreateScript("EnvelopeAdaptive",
        "// Reusable buildup — adapts to any envelope duration\n"
        "var totalMs = Engine.getEnvelopeDuration();\n"
        "if (totalMs <= 0) totalMs = 5000;\n"
        "var fadeIn = Engine.getEnvelopeFadeIn();\n"
        "var fadeOut = Engine.getEnvelopeFadeOut();\n"
        "var steps = Math.max(1, Math.round(totalMs / 25));\n"
        "function easeInQuad(t) { return t * t; }\n"
        "for (var i = 0; i <= steps; i++) {\n"
        "    var t = i / steps;\n"
        "    var brightness = Math.round(255 * easeInQuad(t));\n"
        "    for (var fix = 0; fix < 8; fix++) {\n"
        "        Engine.setFixture(fix, 0, brightness);\n"
        "    }\n"
        "    Engine.waitTime(25);\n"
        "}\n");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

// ═══════════════════════════════════════════════════════════════════════════
// Edge cases
// ═══════════════════════════════════════════════════════════════════════════

void ScriptTool_Test::edge_longScript()
{
    // Build a 1000+ character script
    std::string longScript;
    for (int i = 0; i < 100; i++)
        longScript += "Engine.setFixture(" + std::to_string(i % 8) + ", 0, " + std::to_string(i % 256) + ");\n";
    auto r = callCreateScript("LongScript", longScript);
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

void ScriptTool_Test::edge_commentsOnly()
{
    auto r = callCreateScript("CommentsOnly",
        "// This script does nothing\n"
        "// But it should still be valid\n"
        "/* Multi-line\n"
        "   comment */\n");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

void ScriptTool_Test::edge_unicodeComments()
{
    auto r = callCreateScript("Unicode",
        "// Löve the Ünîcödé 日本語 🎭\n"
        "Engine.waitTime(100);\n");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

void ScriptTool_Test::edge_whiteSpaceOnly()
{
    // Whitespace-only is technically valid JS (empty statements)
    auto r = callCreateScript("Whitespace", "   \n  \n  ");
    QVERIFY2(!r.contains("error"), r.dump().c_str());
}

QTEST_MAIN(ScriptTool_Test)
