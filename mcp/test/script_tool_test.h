/*
  Q Light Controller Plus - Unit test
  script_tool_test.h

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

#ifndef SCRIPT_TOOL_TEST_H
#define SCRIPT_TOOL_TEST_H

#include <QObject>
#include <nlohmann/json.hpp>

namespace fastmcpp { namespace tools { class ToolManager; } }
class Doc;

class ScriptTool_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    // ── Real dispatch sanity ───────────────────────────────────────────
    void dispatch_missingItemsRejected();
    void dispatch_unknownFieldRejected();

    // ── Basic CRUD ─────────────────────────────────────────────────────
    void create_simpleScript();
    void create_upsertReplaces();
    void create_emptyContentRejected();
    void create_missingContentRejected();
    void create_withPath();

    // ── Syntax validation (blocking) ───────────────────────────────────
    void syntax_validJsAccepted();
    void syntax_invalidJsRejected();
    void syntax_unclosedParenRejected();
    void syntax_errorReportsLineNumber();
    void syntax_rejectedScriptNotCreated();
    void syntax_rejectedUpdateRestoresOriginal();

    // ── Engine API coverage (all methods pass syntax check) ────────────
    void engineApi_startStopFunction();
    void engineApi_setFixtureBasic();
    void engineApi_setFixtureWithFade();
    void engineApi_waitTimeMs();
    void engineApi_waitTimeString();
    void engineApi_blackout();
    void engineApi_setBPM();
    void engineApi_randomInt();
    void engineApi_randomString();
    void engineApi_systemCommand();
    void engineApi_stopOnExit();
    void engineApi_isFunctionRunning();
    void engineApi_getChannelValue();
    void engineApi_waitFunctionStartStop();
    void engineApi_getFunctionAttribute();
    void engineApi_setFunctionAttributeByIndex();
    void engineApi_setFunctionAttributeByName();

    // ── Advanced JavaScript patterns ───────────────────────────────────
    void js_forLoop();
    void js_whileLoopWithWait();
    void js_ifElseConditional();
    void js_mathFunctions();
    void js_variablesAndState();
    void js_helperFunctions();
    void js_arraysAndObjects();
    void js_easingFunction();
    void js_nestedLoops();
    void js_closures();
    void js_switchStatement();

    // ── Creative pattern validation ────────────────────────────────────
    void pattern_candleFlicker();
    void pattern_sunriseRamp();
    void pattern_randomSceneSequencer();
    void pattern_stormLightning();
    void pattern_sineBreathing();
    void pattern_fixtureCascade();
    void pattern_bpmStrobe();
    void pattern_stateMachine();
    void pattern_reactiveFollow();
    void pattern_easedFade();
    void pattern_bpmReactive();
    void pattern_audioReactive();
    void pattern_audioSpectrum16Band();

    // ── New Engine API: BPM + Audio ────────────────────────────────────
    void engineApi_getBPM();
    void engineApi_getBeatDuration();
    void engineApi_isBeat();
    void engineApi_getAudioLevel();
    void engineApi_getAudioFrequency();

    // ── Envelope awareness ─────────────────────────────────────────────
    void engineApi_getOwnID();
    void engineApi_getElapsed();
    void engineApi_getEnvelopeDuration();
    void engineApi_getEnvelopeFadeInOut();
    void pattern_envelopeAdaptive();

    // ── Edge cases ─────────────────────────────────────────────────────
    void edge_longScript();
    void edge_commentsOnly();
    void edge_unicodeComments();
    void edge_whiteSpaceOnly();

private:
    Doc *m_doc = nullptr;
    fastmcpp::tools::ToolManager *m_tm = nullptr;

    /**
     * Helper: dispatch the real create_scripts MCP tool and return result JSON.
     * Wraps the single item in {"items": [...]} and returns the first result entry.
     */
    nlohmann::json callCreateScript(const std::string &name,
                                     const std::string &content,
                                     const std::string &path = "");
};

#endif // SCRIPT_TOOL_TEST_H
