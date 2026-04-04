/*
  Q Light Controller Plus
  prompts.cpp

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

#include <fastmcpp/prompts/manager.hpp>

#include <QMetaObject>
#include <QThread>

#include "doc.h"
#include "scene.h"
#include "scenevalue.h"
#include "chaser.h"
#include "chaserstep.h"
#include "collection.h"
#include "fixture.h"
#include "qlcchannel.h"

void registerPrompts(fastmcpp::prompts::PromptManager &pm, Doc *doc)
{
    using Json = nlohmann::json;
    using Prompt = fastmcpp::prompts::Prompt;
    using PromptMessage = fastmcpp::prompts::PromptMessage;
    using PromptArgument = fastmcpp::prompts::PromptArgument;

    // ── Prompt 1: design_dj_show ──────────────────────────────────────
    {
        Prompt p;
        p.name = "design_dj_show";
        p.description = "Design a complete DJ/club lighting show tailored to the currently patched fixtures. "
                         "Returns a fixture-aware guide with beat-synced timing, audio-reactive sliders, "
                         "buildup/drop phases, small color palettes, and button-based control layout.";
        p.arguments = {
            PromptArgument{"colors_per_phase", std::string("Number of colors per phase palette (default 4)"), false},
            PromptArgument{"energy_style", std::string("Overall energy: aggressive, balanced, or ambient (default balanced)"), false}
        };
        p.generator = [doc](const Json &args) -> std::vector<PromptMessage> {
            int colorsPerPhase = 4;
            if (args.contains("colors_per_phase"))
            {
                if (args["colors_per_phase"].is_number())
                    colorsPerPhase = args["colors_per_phase"].get<int>();
                else if (args["colors_per_phase"].is_string())
                    colorsPerPhase = std::stoi(args["colors_per_phase"].get<std::string>());
            }
            std::string energyStyle = args.contains("energy_style") ? args["energy_style"].get<std::string>() : "balanced";

            // Query current fixtures
            std::string fixtureSummary;
            int rgbCount = 0, movingHeadCount = 0, strobeCount = 0, dimmerOnlyCount = 0;

            QMetaObject::invokeMethod(doc, [&]() {
                for (Fixture *fxi : doc->fixtures())
                {
                    bool hasRGB = false, hasPanTilt = false, hasGobo = false, hasStrobe = false;
                    for (quint32 ch = 0; ch < fxi->channels(); ch++)
                    {
                        const QLCChannel *c = fxi->channel(ch);
                        if (!c) continue;
                        if (c->group() == QLCChannel::Intensity &&
                            (c->colour() == QLCChannel::Red || c->colour() == QLCChannel::Green || c->colour() == QLCChannel::Blue))
                            hasRGB = true;
                        if (c->group() == QLCChannel::Pan || c->group() == QLCChannel::Tilt)
                            hasPanTilt = true;
                        if (c->group() == QLCChannel::Gobo)
                            hasGobo = true;
                        if (c->group() == QLCChannel::Shutter)
                            hasStrobe = true;
                    }
                    if (hasRGB) rgbCount++;
                    if (hasPanTilt) movingHeadCount++;
                    if (hasStrobe) strobeCount++;
                    if (!hasRGB && !hasPanTilt) dimmerOnlyCount++;

                    std::string type = hasPanTilt ? (hasRGB ? "Moving Head (RGB)" : "Moving Head") :
                                       hasRGB ? "RGB Par/Wash" :
                                       hasStrobe ? "Strobe" : "Dimmer/Other";
                    std::string capList;
                    if (hasRGB) capList += "RGB ";
                    if (hasPanTilt) capList += "Pan/Tilt ";
                    if (hasGobo) capList += "Gobo ";
                    if (hasStrobe) capList += "Strobe ";
                    if (capList.empty()) capList = "Dimmer";

                    fixtureSummary += "- ID " + std::to_string(fxi->id()) + ": "
                        + fxi->name().toStdString() + " (" + type + ") — "
                        + std::to_string(fxi->channels()) + "ch [" + capList + "]\n";
                }
            }, Qt::BlockingQueuedConnection);

            if (fixtureSummary.empty())
                fixtureSummary = "(No fixtures patched — patch fixtures first with patch_fixtures)\n";

            // Build timing table based on energy style
            std::string timingTable;
            if (energyStyle == "aggressive") {
                timingTable =
                    "| Phrase | Hold | FadeIn | FadeOut | Steps | Total |\n"
                    "|--------|------|--------|---------|-------|-------|\n"
                    "| Drop | 1 | 0 | 0 | 4 | 4 beats (1 bar) |\n"
                    "| Build | 2 | 0.5 | 0.5 | 4 | 12 beats |\n"
                    "| Intro/Break | 4 | 1 | 1 | 4 | 24 beats |\n";
            } else if (energyStyle == "ambient") {
                timingTable =
                    "| Phrase | Hold | FadeIn | FadeOut | Steps | Total |\n"
                    "|--------|------|--------|---------|-------|-------|\n"
                    "| Drop | 4 | 2 | 2 | 4 | 32 beats (8 bars) |\n"
                    "| Build | 4 | 2 | 2 | 4 | 32 beats |\n"
                    "| Intro/Break | 8 | 4 | 4 | 2 | 32 beats |\n";
            } else {
                timingTable =
                    "| Phrase | Hold | FadeIn | FadeOut | Steps | Total |\n"
                    "|--------|------|--------|---------|-------|-------|\n"
                    "| Drop | 2 | 0 | 0.5 | 4 | 10 beats |\n"
                    "| Build | 2 | 1 | 1 | 4 | 16 beats (4 bars) |\n"
                    "| Intro/Break | 4 | 2 | 2 | 4 | 32 beats (8 bars) |\n";
            }

            std::string colorsNote = "Use " + std::to_string(colorsPerPhase)
                + " colors per phase. Keep palettes tight — repetition builds recognition.";

            std::string guide =
                "# DJ Show Design — Your Fixtures\n\n"
                "## Current Rig\n" + fixtureSummary +
                "\nTotals: " + std::to_string(rgbCount) + " RGB, "
                + std::to_string(movingHeadCount) + " moving heads, "
                + std::to_string(strobeCount) + " strobes, "
                + std::to_string(dimmerOnlyCount) + " dimmer-only\n"
                "\nEnergy style: **" + energyStyle + "**\n\n"

                "---\n\n"
                "# Two-Tier Architecture: Phase × Phrase\n\n"
                "Lighting has TWO independent time scales:\n\n"
                "| Tier | Scope | Controlled By | Transition Speed |\n"
                "|------|-------|---------------|------------------|\n"
                "| **Phase** | Entire set arc | Busker (manual) | Between songs (slow) |\n"
                "| **Phrase** | Song structure | Busker or chaser | Within a song (fast) |\n\n"
                "- **Phase** defines the COLOR PALETTE, brightness range, and motion style\n"
                "- **Phrase** modulates WITHIN those bounds (build tension, hit drops, rest on breakdowns)\n"
                "- A \"P3 Drop\" is the most intense moment: Peak phase + Drop phrase\n\n"

                "---\n\n"
                "## Set Phases (P1-P4)\n\n"
                "The busker walks through these across the entire set. Each phase has a distinct visual identity "
                "readable within 5-10 seconds, even from the back of the room.\n\n"
                "| Phase | Name | Energy | Brightness | Motion |\n"
                "|-------|------|--------|------------|--------|\n"
                "| P1 | Starter | Low-Med | 0.60-0.75 | Smooth, legible groove |\n"
                "| P2 | Buildup | Med-High | 0.75-0.90 | Tighter, sharper, directional |\n"
                "| P3 | Peak | Maximum | 0.90-1.00 + blackout pockets | Fast snaps, high contrast |\n"
                "| P4 | Release | Medium | 0.65-0.82 | De-escalate, flow |\n\n"

                "### Phase Color Palettes\n"
                "Create these with `create_palettes` — " + std::to_string(colorsPerPhase) + " colors per phase.\n\n"
                "| Phase | Background Anchor | Palette Colors | Accent |\n"
                "|-------|-------------------|----------------|--------|\n"
                "| P1 | #001a00 (deep forest) | #228b22, #00aa00, #ffff00 | #0096c8 (blue accent) |\n"
                "| P2 | #000a22 (deep navy) | #0044aa, #00ffff, #9900ff | — |\n"
                "| P3 | #000000 (black) | #9900ff, #ff00aa, #ff0000 | White flash |\n"
                "| P4 | #000022 (soft navy) | #3366cc, #cc99ff, #00ccff | — |\n\n"

                "**Rule:** 80%+ of runtime stays inside the active phase palette. At most ONE accent color outside it.\n\n"

                "### Phase Creative Rules\n"
                "- **P1 Starter**: Never use full black as base look. Prioritize rhythmic readability over intensity. "
                "No aggressive strobes except short punctuation.\n"
                "- **P2 Buildup**: Introduce stronger beat articulation and contrast. "
                "Strobes signal tension, don't dominate.\n"
                "- **P3 Peak**: Use short black gaps to make hits feel bigger. "
                "Alternate strobe sections with non-strobe impacts. Keep strobe bursts ≤8 seconds.\n"
                "- **P4 Release**: De-escalate without going flat. Reintroduce flow and melody emphasis. "
                "Sparse, short strobes only.\n\n"

                "---\n\n"
                "## Song Phrases (within each Phase)\n\n"
                "Within each phase, every song has its own structure. "
                "Phrases modulate intensity WITHIN the phase's bounds.\n\n"
                "| Phrase | Energy Delta | Lighting Behavior |\n"
                "|--------|-------------|-------------------|\n"
                "| **Intro** | Neutral/Low | Entry scene — lower complexity, establish the phase mood |\n"
                "| **Build** | Rising | Tighten motion, increase brightness toward phase ceiling |\n"
                "| **Drop** | Peak within phase | Full phase energy, maximum contrast, hard hits |\n"
                "| **Break** | Low/Breath | Strip back, softer textures, rest state within phase |\n\n"

                "### Phrase Defaults per Phase\n"
                "Each phase sets baseline values. Phrases override specific layers:\n\n"
                "| Phase | Intro Dimmer | Build Dimmer | Drop Dimmer | Break Dimmer | Drop Motion |\n"
                "|-------|-------------|-------------|-------------|-------------|-------------|\n"
                "| P1 | 60% | 70% | 75% | 55% | Medium pulse |\n"
                "| P2 | 70% | 85% | 90% | 65% | Tight sweep |\n"
                "| P3 | 80% | 95% | 100% | 70% | Hard snap + blackout |\n"
                "| P4 | 65% | 75% | 82% | 60% | Gentle flow |\n\n"

                "### Suggested Scene Flow (per song)\n"
                "`Intro → Build → Drop → Break → (repeat or transition)`\n\n"
                "Every phase playlist should include:\n"
                "- 1 entry scene (lower complexity, establishes mood)\n"
                "- 1 high-contrast peak scene (the \"hit\")\n"
                "- 1 exit scene (prepares next song or phase transition)\n\n"

                "---\n\n"
                "## Scene Naming Convention\n\n"
                "Format: `{Phase}-{Phrase}-{Layer}`\n\n"
                "Examples:\n"
                "- `P1-Intro-MoodGreen` — Starter phase, song intro, color layer\n"
                "- `P3-Drop-MoodMagenta` — Peak phase, song drop, color layer\n"
                "- `P2-Build-PositionSweep` — Buildup phase, song buildup, position layer\n"
                "- `P3-Drop-DimFull` — Peak phase, drop, dimmer at max\n"
                "- `P4-Break-DimLow` — Release phase, breakdown, dimmer pulled back\n\n"

                "---\n\n"
                "## Build Order\n"
                "1. `query_fixture_channels` — get exact channel indices\n"
                "2. Design orthogonal layers (each DMX channel in exactly ONE layer)\n"
                "3. **Create phase palettes** — `create_palettes` for each phase's colors:\n"
                "   - P1: `P1-Green`, `P1-Yellow`, `P1-Blue-Accent`\n"
                "   - P2: `P2-Blue`, `P2-Cyan`, `P2-Purple`\n"
                "   - P3: `P3-Purple`, `P3-Magenta`, `P3-Red`\n"
                "   - P4: `P4-SoftBlue`, `P4-Lavender`, `P4-Aqua`\n"
                "4. Create shared palettes — dimmer levels, positions\n"
                "5. **Build scenes per phase × phrase × layer** — use `create_scenes` with `paletteNames`\n"
                "6. Create phrase chasers (beat-synced sequence within one phase)\n"
                "7. Bundle into phase collections (each collection = one phase's full look)\n"
                "8. Build VC with Phase + Phrase SoloFrames\n\n"

                "---\n\n"
                "## Layer Architecture\n"
                "| Layer | Owns | VC Widget |\n"
                "|-------|------|-----------|\n"
                "| **Mood** | RGB/CMY/White channels | SoloFrame buttons |\n"
                "| **Dimmer** | Intensity/dimmer channels | SoloFrame buttons or slider |\n"
                + std::string(movingHeadCount > 0 ?
                    "| **Position** | Pan, tilt | SoloFrame or XY pad |\n"
                    "| **Texture** | Gobo, prism, color wheel | SoloFrame buttons |\n" : "") +
                "| **Strobe** | Shutter/strobe channels | Flash buttons (hold) |\n"
                "| **Energy** | Speed channels, EFX run/stop | SoloFrame buttons |\n"
                "\n**Rule:** A DMX channel appears in exactly ONE layer. Never mix.\n\n"

                "---\n\n"
                "## VC Page Layout\n"
                "```\n"
                "Page \"DJ Control\":\n"
                "  Row 1: [Set Phase] solo (P1|P2|P3|P4)  +  [Song Phrase] solo (Intro|Build|Drop|Break)\n"
                "  Row 2: [Mood Override] solo             +  [Position] solo\n"
                "  Row 3: [Strobe] frame                   +  [Levels] sliders\n"
                "```\n\n"
                "- **Set Phase frame**: 4 buttons (P1/P2/P3/P4), each triggers a collection that sets ALL layers to that phase's defaults\n"
                "- **Song Phrase frame**: 4 buttons (Intro/Build/Drop/Break), each overrides dimmer + motion layers within the current phase\n"
                "- **Mood Override**: Per-phase color buttons for manual override\n"
                "- Phase buttons are exclusive (SoloFrame) — only one phase active\n"
                "- Phrase buttons are exclusive — only one phrase active\n\n"

                "---\n\n"
                "## Beat-Synced Timing (tempoType: \"beats\" — ALWAYS)\n\n"
                "All durations MUST align to musical bar boundaries. QLC+ supports fractional beats.\n\n"
                "### Beat Values\n"
                "| Notation | Beats | Musical Meaning |\n"
                "|----------|-------|----------------|\n"
                "| 0.25 | ¼ beat | Sixteenth note — strobe/stutter |\n"
                "| 0.5 | ½ beat | Eighth note — tight accents |\n"
                "| 1 | 1 beat | Quarter note — pulse, tap |\n"
                "| 2 | 2 beats | Half bar — driving rhythm |\n"
                "| 4 | 4 beats | 1 bar — standard step |\n"
                "| 8 | 8 beats | 2 bars — musical phrase |\n"
                "| 16 | 16 beats | 4 bars — section length |\n"
                "| 32 | 32 beats | 8 bars — full phrase |\n\n"
                "### Step Timing per Phrase\n"
                + timingTable + "\n"
                "### Chaser & Collection Duration Guide\n"
                "| Structure | Total Duration | Steps × Hold | Use |\n"
                "|-----------|---------------|-------------|-----|\n"
                "| Quick color cycle | 4 beats (1 bar) | 4 × 1 beat | Tight pulse |\n"
                "| Mood rotation | 16 beats (4 bars) | 4 × 4 beats | Standard mood |\n"
                "| Phrase chase | 32 beats (8 bars) | 4 × 8 beats | Full musical phrase |\n"
                "| Build sequence | 16 beats | 8 × 2 beats | Rising tension |\n"
                "| Drop hit | 4 beats | 2 × 2 beats | Impact moment |\n\n"
                "**Rules:**\n"
                "- Total chaser duration should be 4, 8, 16, or 32 beats (bar-aligned)\n"
                "- FadeIn + Hold + FadeOut = step duration (must divide evenly into total)\n"
                "- Use 0.5-beat fades for snappy transitions, 2-beat fades for smooth\n"
                "- Drop phrases: 0 fade, 1-2 beat hold (hard cuts)\n"
                "- Build phrases: 1-beat fade, 2-4 beat hold (progressive)\n"
                "- Break phrases: 2-4 beat fade, 4-8 beat hold (breathing)\n\n"

                "## Audio-Reactive Sliders\n"
                "| Slider | Mode | Channels | Purpose |\n"
                "|--------|------|----------|---------|\n"
                "| Bass Pulse | level | Dimmer channels | Fixtures pulse with kick |\n"
                + std::string(movingHeadCount > 0 ?
                    "| Mid Drive | level | Gobo/prism speed | Texture breathes with melody |\n" : "") +
                "| Treble Flash | level | Strobe/shutter | Hi-hat triggers flashes |\n"
                "| Master | grandmaster | (all) | Overall brightness cap |\n\n"

                "## Live Capture Workflow\n"
                "- `read_dmx_values` — read current DMX output for any fixture/channel filter\n"
                "- `update_scene_from_dmx` — capture live state into a new or existing scene\n\n"

                "---\n\n"
                "## VC Layout Best Practices\n"
                "- **Frames side-by-side**: Use x/y positioning for related frames on the same row\n"
                "- **Slider height**: 350px minimum for usable sliders\n"
                "- **XY Pads**: Always square (250×250 or 300×300)\n"
                "- **RGB picker**: ONE slider with clickAndGoType='colors' (not 3 separate sliders)\n"
                "- **Reflow per frame**: Call vc_reflow_frame on individual frames after manual positioning\n\n"

                "---\n\n"
                "## Non-Negotiable Rules\n"
                "1. **Two-tier control**: Phase sets palette/bounds, Phrase modulates within them\n"
                "2. **Palette discipline**: 80%+ runtime inside the active phase palette; at most 1 accent outside\n"
                "3. **Contrast rhythm**: Every scene needs both a rest state and a hit state; avoid full intensity >45 seconds\n"
                "4. **Strobe restraint**: Accents only, never sustained >8 seconds; never two strobe behaviors at once\n"
                "5. **Transition ownership**: Entry and exit scenes per phase are softer than peak scenes\n"
                "6. **Audio-reactivity**: Every phrase must include at least one audio-reactive element\n"
                "7. **Layer separation**: Same DMX channel NEVER in two layers\n"
                "8. **Beat-aligned**: All chasers use tempoType \"beats\"; total duration = 4, 8, 16, or 32 beats\n"
                "9. **Naming**: `{Phase}-{Phrase}-{Layer}` — e.g. P2-Drop-MoodCyan\n"
                "10. **Idempotent**: All create tools upsert by name — safe to call repeatedly\n"
                "11. **Palette-first**: Create palettes → reference in scenes → channelValues only for overrides\n";

            return {PromptMessage{"user", guide}};
        };
        pm.register_prompt(p);
    }

    // ── Prompt 2: debug_channel_conflict ──────────────────────────────
    {
        Prompt p;
        p.name = "debug_channel_conflict";
        p.description = "Diagnose which scenes and functions touch a specific fixture channel. "
                         "Use when you suspect a DMX channel is being set by multiple layers or scenes.";
        p.arguments = {
            PromptArgument{"fixture_id", std::string("Fixture ID to inspect"), true},
            PromptArgument{"channel", std::string("Channel index to check"), true}
        };
        p.generator = [doc](const Json &args) -> std::vector<PromptMessage> {
            int fixtureId = 0, channelIdx = 0;
            if (args["fixture_id"].is_number()) fixtureId = args["fixture_id"].get<int>();
            else fixtureId = std::stoi(args["fixture_id"].get<std::string>());
            if (args["channel"].is_number()) channelIdx = args["channel"].get<int>();
            else channelIdx = std::stoi(args["channel"].get<std::string>());

            std::string report;

            QMetaObject::invokeMethod(doc, [&]() {
                Fixture *fxi = doc->fixture(fixtureId);
                if (!fxi)
                {
                    report = "Error: Fixture ID " + std::to_string(fixtureId) + " not found.";
                    return;
                }
                const QLCChannel *ch = fxi->channel(channelIdx);
                if (!ch)
                {
                    report = "Error: Channel " + std::to_string(channelIdx) + " not found on fixture "
                             + fxi->name().toStdString() + ".";
                    return;
                }

                report = "# Channel Conflict Report\n\n"
                         "**Fixture:** " + fxi->name().toStdString() + " (ID " + std::to_string(fixtureId) + ")\n"
                         "**Channel:** " + std::to_string(channelIdx) + " — " + ch->name().toStdString() +
                         " (Group: " + QLCChannel::groupToString(ch->group()).toStdString() +
                         ", Colour: " + QLCChannel::colourToString(ch->colour()).toStdString() + ")\n\n";

                // Find all scenes that touch this channel
                struct SceneHit {
                    quint32 sceneId;
                    std::string sceneName;
                    uchar value;
                };
                std::vector<SceneHit> hits;

                for (Function *fn : doc->functionsByType(Function::SceneType))
                {
                    Scene *scene = qobject_cast<Scene*>(fn);
                    if (!scene) continue;
                    for (const SceneValue &sv : scene->values())
                    {
                        if ((int)sv.fxi == fixtureId && (int)sv.channel == channelIdx)
                        {
                            hits.push_back({fn->id(), fn->name().toStdString(), sv.value});
                            break;
                        }
                    }
                }

                if (hits.empty())
                {
                    report += "**No scenes touch this channel.** It is unused or only controlled by manual faders.\n";
                }
                else
                {
                    report += "## Scenes Setting This Channel (" + std::to_string(hits.size()) + " found)\n\n"
                              "| Scene | ID | Value | DMX |\n"
                              "|-------|----|-------|-----|\n";
                    for (const auto &hit : hits)
                    {
                        report += "| " + hit.sceneName + " | " + std::to_string(hit.sceneId)
                                + " | " + std::to_string(hit.value)
                                + " | " + std::to_string(hit.value) + "/255 |\n";
                    }
                    report += "\n";

                    // Find chasers/collections referencing these scenes
                    std::map<quint32, std::string> sceneIds;
                    for (const auto &hit : hits)
                        sceneIds[hit.sceneId] = hit.sceneName;

                    std::string refs;
                    for (Function *fn : doc->functionsByType(Function::ChaserType))
                    {
                        Chaser *chaser = qobject_cast<Chaser*>(fn);
                        if (!chaser) continue;
                        for (int i = 0; i < chaser->stepsCount(); i++)
                        {
                            ChaserStep *step = chaser->stepAt(i);
                            if (step && sceneIds.count(step->fid))
                            {
                                refs += "- Chaser **" + fn->name().toStdString()
                                      + "** (ID " + std::to_string(fn->id())
                                      + ") uses scene " + sceneIds[step->fid]
                                      + " at step " + std::to_string(i) + "\n";
                                break;
                            }
                        }
                    }
                    for (Function *fn : doc->functionsByType(Function::CollectionType))
                    {
                        Collection *col = qobject_cast<Collection*>(fn);
                        if (!col) continue;
                        for (quint32 fid : col->functions())
                        {
                            if (sceneIds.count(fid))
                            {
                                refs += "- Collection **" + fn->name().toStdString()
                                      + "** (ID " + std::to_string(fn->id())
                                      + ") includes scene " + sceneIds[fid] + "\n";
                                break;
                            }
                        }
                    }

                    if (!refs.empty())
                        report += "## Functions Referencing These Scenes\n\n" + refs + "\n";

                    // Conflict analysis
                    if (hits.size() > 1)
                    {
                        report += "## Potential Conflict\n\n"
                                  "**" + std::to_string(hits.size()) + " scenes** set this channel. "
                                  "If these scenes belong to different layers and run simultaneously, "
                                  "they will fight for control (LTP — last one wins).\n\n"
                                  "**Fix:** Ensure only ONE layer owns this channel. "
                                  "Remove the channel from scenes in other layers, or split into "
                                  "separate layer-specific scenes.\n";
                    }
                    else
                    {
                        report += "## No Conflict\n\nOnly one scene touches this channel.\n";
                    }
                }
            }, Qt::BlockingQueuedConnection);

            return {PromptMessage{"user", report}};
        };
        pm.register_prompt(p);
    }

    // ── Prompt 3: setup_launchpad ─────────────────────────────────────
    {
        Prompt p;
        p.name = "setup_launchpad";
        p.description = "Step-by-step guide to set up a Novation Launchpad as a lighting controller in QLC+. "
                         "Covers detection, configuration, button mapping, and LED feedback.";
        p.arguments = {
            PromptArgument{"model", std::string("Launchpad model, e.g. 'Launchpad Mini MK3'"), true}
        };
        p.generator = [](const Json &args) -> std::vector<PromptMessage> {
            std::string model = args.at("model").get<std::string>();

            std::string guide =
                "# Launchpad Setup Guide: " + model + "\n\n"

                "## Step 1: Connect Hardware\n"
                "Plug the " + model + " into USB. It should power on and show its default LED pattern.\n\n"

                "## Step 2: Verify Detection\n"
                "Call `query_midi_devices` to confirm QLC+ sees the Launchpad.\n"
                "You should see **two ports** per device:\n"
                "- **MIDI port** — for notes/CC (do NOT use this one)\n"
                "- **DAW port** — for full Programmer Mode control (USE THIS)\n\n"
                "If not listed, check USB connection and restart QLC+.\n\n"

                "## Step 3: Auto-Configure\n"
                "Call `configure_launchpad` with model `\"" + model + "\"`.\n\n"
                "This single call does everything:\n"
                "1. Detects the correct DAW port (not MIDI port)\n"
                "2. Sends the Programmer Mode SysEx init message\n"
                "3. Sets the correct input profile\n"
                "4. Enables LED feedback on the same universe\n\n"
                "**After this call, the Launchpad is ready to use.**\n\n"

                "## Step 4: Verify Setup\n"
                "Call `query_universes` to confirm:\n"
                "- An input universe is patched to the Launchpad DAW port\n"
                "- Feedback is enabled on the same universe\n"
                "- The input profile is set (e.g., \"Novation " + model + "\")\n\n"

                "## Step 5: Map Buttons\n"
                "Use `vc_create_widgets` with `inputUniverse` and `inputChannel` on each button,\n"
                "or use `vc_map_inputs` after creation.\n\n"
                "### Pad-to-Channel Formula\n"
                "```\n"
                "QLC+ channel = 128 + pad_note_number\n"
                "```\n\n"
                "### Pad Layout (note numbers)\n"
                "```\n"
                "Row 8: 81 82 83 84 85 86 87 88   (top row)\n"
                "Row 7: 71 72 73 74 75 76 77 78\n"
                "Row 6: 61 62 63 64 65 66 67 68\n"
                "Row 5: 51 52 53 54 55 56 57 58\n"
                "Row 4: 41 42 43 44 45 46 47 48\n"
                "Row 3: 31 32 33 34 35 36 37 38\n"
                "Row 2: 21 22 23 24 25 26 27 28\n"
                "Row 1: 11 12 13 14 15 16 17 18   (bottom row)\n"
                "```\n"
                "Example: Pad at row 8, column 1 → note 81 → channel 209\n\n"

                "### Suggested Row Assignment\n"
                "| Row | Purpose | LED Style |\n"
                "|-----|---------|----------|\n"
                "| 8 | Phase presets | Purple, pulsing |\n"
                "| 7 | Energy levels | Cyan, pulsing |\n"
                "| 6 | Mood colors | Match output color, pulsing |\n"
                "| 5 | Texture/gobo | Orange, pulsing |\n"
                "| 4-3 | EFX/chasers | Cyan/Yellow, pulsing |\n"
                "| 2 | Quick shots (flash) | White, static |\n"
                "| 1 | System (BLACKOUT, STOP) | Red, flashing |\n\n"

                "## Step 6: Configure LED Feedback\n"
                "First, discover the available colors and animation modes for your controller:\n"
                "```\n"
                "Call query_feedback_profile with universeID (from Step 4)\n"
                "```\n"
                "This returns:\n"
                "- **colorTable**: available LED colors with velocity values, labels, and hex colors\n"
                "- **midiChannelTable**: available animation modes (e.g. 0=Static, 1=Flashing, 2=Pulsing)\n\n"

                "Use `vc_map_inputs` to set feedback per button:\n"
                "- `idleValue`: LED color velocity when button is OFF (use dimmer variants)\n"
                "- `activeValue`: LED color velocity when button is ON (full brightness)\n"
                "- `monitorValue`: LED color velocity for monitor state\n"
                "- `idleChannel`/`activeChannel`/`monitorChannel`: animation mode from midiChannelTable\n\n"
                "**Design rules:**\n"
                "- LED colors should match the output (green scene = green LED)\n"
                "- Idle LEDs always visible (never fully off) — use dimmer color variants\n"
                "- Pulsing = toggle/persistent buttons\n"
                "- Flashing = danger/intense (BLACKOUT, STOP ALL)\n"
                "- Static = momentary/flash buttons\n\n"

                "## Common Pitfalls\n"
                "- **Wrong port**: Always use the DAW port, not the MIDI port\n"
                "- **No init message**: Without Programmer Mode, LEDs won't respond to feedback\n"
                "- **Channel math**: Remember `128 + note`, not just the note number\n"
                "- **Feedback not enabled**: Verify feedback is ON in `query_universes`\n"
                "- **Profile missing**: The input profile maps pads correctly — verify it's set\n";

            return {PromptMessage{"user", guide}};
        };
        pm.register_prompt(p);
    }
}
