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
                    "| Feel | Hold | FadeIn | FadeOut | Use |\n"
                    "|------|------|--------|---------|-----|\n"
                    "| Driving pulse | 2 | 1 | 1 | Default mood chase |\n"
                    "| Aggressive snap | 1 | 0 | 0 | Peak/drop moments |\n"
                    "| Musical flow | 4 | 1 | 1 | Buildup transition |\n";
            } else if (energyStyle == "ambient") {
                timingTable =
                    "| Feel | Hold | FadeIn | FadeOut | Use |\n"
                    "|------|------|--------|---------|-----|\n"
                    "| Ambient drift | 8 | 4 | 4 | Default mood chase |\n"
                    "| Musical flow | 4 | 2 | 2 | Accent moments |\n"
                    "| Gentle pulse | 2 | 2 | 2 | Subtle energy shifts |\n";
            } else {
                timingTable =
                    "| Feel | Hold | FadeIn | FadeOut | Use |\n"
                    "|------|------|--------|---------|-----|\n"
                    "| Musical flow | 4 | 2 | 2 | Default mood chase |\n"
                    "| Driving pulse | 2 | 1 | 1 | Energy sections |\n"
                    "| Aggressive snap | 1 | 0 | 0 | Peak/drop hits |\n"
                    "| Ambient drift | 8 | 4 | 4 | Intro/outro |\n";
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

                "## Build Order\n"
                "1. `query_fixture_channels` — get exact channel indices for your fixtures\n"
                "2. Design orthogonal layers (each DMX channel in exactly ONE layer)\n"
                "3. Create scenes bottom-up: mood colors → energy presets → position snaps\n"
                "4. Create beat-synced chasers (always use `tempoType: \"beats\"`)\n"
                "5. Bundle into phase collections (P1-P4)\n"
                "6. Build VC page with `vc_create_widgets` + `vc_reflow_frame`\n\n"

                "## Layer Architecture\n"
                "| Layer | Owns | VC Widget |\n"
                "|-------|------|-----------|\n"
                "| **Mood** | RGB/CMY/White channels | SoloFrame buttons |\n"
                "| **Energy** | Pan/tilt speed, gobo rotation speed | SoloFrame buttons |\n"
                "| **Activity** | EFX patterns, chaser run/stop | SoloFrame buttons |\n"
                "| **Dimmer** | Intensity/dimmer channels | Audio-reactive slider |\n"
                "| **Strobe** | Shutter/strobe channels | Flash buttons (hold) |\n"
                + std::string(movingHeadCount > 0 ?
                    "| **Position** | Pan, tilt | SoloFrame or XY pad |\n"
                    "| **Texture** | Gobo, prism, color wheel | SoloFrame buttons |\n" : "") +
                "\n**Rule:** A channel appears in exactly ONE layer.\n\n"

                "## DJ Phase System (Buildup / Drop)\n"
                "| Phase | Energy | Palette | Motion |\n"
                "|-------|--------|---------|--------|\n"
                "| P1 Warmup | Low-Med | Warm tones (amber, gold) | Slow drifts |\n"
                "| P2 Buildup | Med-High | Cool tones (blue, cyan) | Tightening, sharper |\n"
                "| P3 Drop/Peak | Maximum | Hot (magenta, red, white) | Fast snaps, strobes |\n"
                "| P4 Release | Medium | Ethereal (lavender, aqua) | De-escalate, flow |\n\n"
                + colorsNote + "\n\n"
                "Phase = Collection bundling: mood scene + energy preset + activity level.\n\n"

                "## Beat-Synced Timing (tempoType: \"beats\" — ALWAYS)\n"
                + timingTable + "\n"

                "## Audio-Reactive Sliders\n"
                "| Slider | Mode | Channels | Click & Go | Purpose |\n"
                "|--------|------|----------|------------|---------|\n"
                "| Bass Pulse | level | Dimmer channels | none | Fixtures pulse with kick |\n"
                + std::string(movingHeadCount > 0 ?
                    "| Mid Drive | level | Gobo/prism rotation | preset | Texture breathes with melody (clickAndGoType: preset for gobo picker) |\n" : "") +
                "| Treble Flash | level | Strobe/shutter | preset | Hi-hat triggers flashes (clickAndGoType: preset for strobe picker) |\n"
                "| Master | grandmaster | (all) | none | Overall brightness cap (gmValueMode: reduce, gmChannelMode: allchannels) |\n\n"
                "Slider options: clickAndGoType (none/colors/preset), valueDisplayStyle (dmx/percentage),\n"
                "invertedAppearance, rangeLowLimit/rangeHighLimit (0-255), monitorEnabled.\n"
                "User configures the audio/OSC source in QLC+ I/O settings.\n\n"

                "## VC Page Layout (button-based control)\n"
                "```\n"
                "Page \"DJ Control\":\n"
                "  Row 1: [Phases] solo=true  +  [Moods] solo=true       ← side by side\n"
                "  Row 2: [Position] solo=true + [Texture] solo=true    ← side by side\n"
                "  Row 3: [Strobe] frame      +  [Levels] frame         ← side by side\n"
                "```\n\n"
                "## VC Layout Best Practices\n"
                "- **Frames side-by-side**: Use manual x/y positioning to place related frames on the same row\n"
                "- **Slider height**: Use 350px minimum for usable sliders (180px is too short)\n"
                "- **XY Pads**: Always square (250×250 or 300×300)\n"
                "- **RGB color picker**: Use ONE slider with clickAndGoType='colors' mapping R+G+B channels together (not 3 separate sliders)\n"
                "- **Channel order**: Order sliders left-to-right by channel index\n"
                "- **Launchpad grid**: Align buttons in 8-column rows matching the Launchpad layout\n"
                "- **Reflow per frame**: Call vc_reflow_frame on individual frames, not the whole page, after manual positioning\n\n"

                "## Scene Creation Rules\n"
                "- **Mood scenes**: set ONLY color channels (R/G/B/CMY + dimmer)\n"
                "- **Position scenes**: set ONLY pan/tilt channels\n"
                "- **Energy scenes**: set ONLY speed channels\n"
                "- **NEVER mix** layers in one scene\n"
                "- Use `query_fixture_channels` to find exact channel indices\n\n"

                "## Non-Negotiable Rules\n"
                "1. **Palette discipline**: 80%+ runtime inside phase palette\n"
                "2. **Contrast rhythm**: Every phase needs rest + hit state\n"
                "3. **Strobe restraint**: Flash-button accents only, never sustained\n"
                "4. **Audio-reactivity**: Every phase has at least one reactive element\n"
                "5. **Layer separation**: Same channel NEVER in two layers\n"
                "6. **Beat-sync default**: All chasers use tempoType \"beats\"\n"
                "7. **Naming**: Phase-prefix (P1-, P2-) or layer-prefix (Mood-, FX-)\n"
                "8. **Idempotent tools**: All create/add tools upsert by name — safe to call repeatedly\n";

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
