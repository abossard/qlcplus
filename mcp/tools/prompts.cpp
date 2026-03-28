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

#include <fastmcpp/tools/manager.hpp>
#include <fastmcpp/tools/tool.hpp>

void registerPrompts(fastmcpp::tools::ToolManager &tm)
{
    using Json = nlohmann::json;
    using Tool = fastmcpp::tools::Tool;

    // TODO: Register MCP prompts via fastmcpp prompt API when available.
    // For now, expose as a tool that returns the prompt text.

    tm.register_tool(Tool(
        "get_show_design_guide",
        Json{{"type", "object"}, {"properties", Json::object()}},
        Json{},
        [](const Json &) -> Json {
            std::string guideText = R"(
# QLC+ Show Design Guide

## How to Use This Guide
1. Call query_fixtures to discover available fixtures and capabilities
2. Ask the user about their event type and comfort level
3. Pick the right TIER and VENUE TEMPLATE below
4. Propose a show plan, get user confirmation, then build
5. After building, create a show-setup.md documentation file (see Post-Build section)

---

## TIER 1: "Just Works" (Beginner / Volunteer)
For: Church volunteer, school event, house party, first-time user

Structure:
- 4-6 pre-built Scenes (complete looks, no layering needed)
- 1 SoloFrame with big buttons (only one look active at a time)
- 1 master slider (submaster) for overall brightness
- 1 blackout button

Naming: Simple descriptive — "Bright", "Warm", "Cool", "Party", "Calm"

VC Layout:
  [Moods SoloFrame: Bright | Warm | Cool | Party | Calm]
  [Master slider]  [BLACKOUT]

Key: No layering, no chasers, no complexity. Just click a button.

---

## TIER 2: "Flexible Control" (Intermediate / Semi-Pro)
For: Band gig, club DJ, worship team, small festival

Structure:
- Color/mood layer: 6-12 Scenes in a SoloFrame (mutually exclusive moods)
- Effect layer: 3-6 Chasers/EFX as toggle/flash buttons in a Frame (layer on top)
- Position: XY pad for moving heads (if available)
- Master controls: Submaster + Blackout + Stop All

Naming: "[Layer] - [Description]" — "Wash - Deep Blue", "FX - Rainbow Chase"

VC Layout:
  [Moods SoloFrame]        [FX Frame]
  [Warm] [Cool] [Intense]  [Chase] [Strobe] [Pulse]
  [Sunset] [Ocean] [Fire]  [Sweep] [Snap]   [Slow]
  
  [XY Pad]     [Master]  [BLACKOUT]  [STOP ALL]

Key: Moods are exclusive (SoloFrame), effects layer on top (regular Frame).

---

## TIER 3: "Full Production" (Advanced / Professional)
For: Concert tour, theatre, festival main stage, installed venue

Structure:
- Fixture groups by position: Front Wash, Back Light, Side, Overhead, Moving Spots, Moving Wash, Effect/Strobe
- Per-group scenes: Color-only, intensity-only, position-only (maximum flexibility)
- Collections for phases: Each phase = color scene + position scene + effect chaser
- Cue lists per song/act: Chaser with one step per cue, manual advance
- Audio-reactive inputs: OSC faders mapped to level sliders
- Per-group submasters for live mixing

Naming: "[Position]_[Purpose]_[ID]" — "FRONT_WASH_01", "US_MH_BEAM_03"

VC Layout (multiple pages):
  Page 1 "Overview": Mood selector + FX + Masters + Blackout
  Page 2 "Cue Lists": Per-song/act cue lists
  Page 3 "Audio": OSC input faders + mode selector
  Page 4 "Groups": Per-fixture-group submasters + color pickers

Key: Separate color from position from intensity. Maximum layering flexibility.

---

## Fixture Grouping Strategy

Group fixtures by position AND function:
| Group | Typical Fixtures | Purpose |
|-------|-----------------|---------|
| Front Wash | Front-truss pars/washes | Face lighting, visibility |
| Back Light | Upstage spots/beams | Depth, silhouettes |
| Side Light | Side-truss washes | Movement accent, sculpting |
| Top Wash | Overhead pars | General area coverage |
| Moving Spot | MH spots | Gobos, beams, specials |
| Moving Wash | MH washes | Color aerials |
| Effect | Strobes, blinders | Impact moments |

Auto-detect from capabilities: RGB → color wash, Pan/Tilt → moving, Shutter → effect

---

## Venue Templates

### Church / House of Worship
- Tier 1 or 2 (volunteer-friendly)
- Warm tones for sermon, cool for worship, dim for prayer
- Smooth transitions (2-3s fades), no strobes
- Scenes: "Sermon", "Worship", "Prayer", "Song", "Welcome"

### Concert / Live Band
- Tier 2 or 3
- Cue list per song in setlist
- Energy curve per song: intro → verse → chorus → bridge → outro
- Fast transitions, movement, color changes

### Club / DJ Set
- Tier 2 or 3
- Beat-synced chasers (tempoType: "beats")
- Audio-reactive OSC inputs for bass/mid/treble
- Continuous flow, no hard cue stops

### Theatre
- Tier 3
- Precise cue-to-cue with GO button (VCCueList)
- Subtle mood shifts, long crossfades (5-10s)
- Per-act cue lists, minimal live improvisation

---

## HTP/LTP Rules
- Intensity/dimmer = HTP (highest wins) — safe to layer
- Pan/Tilt/Color wheel/Gobo = LTP (latest wins) — separate from intensity
- Separate intensity scenes from position/color scenes for layering

## Beat/Tempo System
- tempoType "beats": 1000 = 1 beat, 500 = 1/2, 250 = 1/4, 125 = 1/8
- Scales automatically with BPM (internal clock, MIDI, or OS2L)
- Strobe buildup: Scene with shutter channel + Chaser fadeIn of N beats

## Audio-Reactive (OSC)
- Map OSC channels to VCSlider level mode → fixtures pulse with audio
- Layer: submaster (manual cap) + level (OSC-driven) + playback (chaser intensity)
- SoloFrame to switch between audio-reactive modes
- Split by frequency: bass → warm pars, mid → accents, treble → strobes

---

## Post-Build: Create Documentation

IMPORTANT: After building a show, ALWAYS create a documentation file called
show-setup.md in the project directory. Include:

1. Quick Start — how to open and operate the show
2. Fixture Summary — table of all fixtures with DMX addresses
3. Virtual Console Guide — what each button/slider/frame does
4. Modification Guide — how to add a mood, change colors, adjust timing

Adapt documentation detail to the user's tier:
- Tier 1: Simple "press this button for this look" guide
- Tier 2: Layer explanation + how to customize
- Tier 3: Full technical reference with fixture groups, channel assignments, cue structure

---

## MIDI Controller Integration (Launchpad Mini MK3)

### Setup Sequence (fully automated via MCP)
1. query_midi_devices → find "Launchpad Mini MK3" in MIDI plugin, pick the SECOND port (higher line number)
2. configure_universes → set input to MIDI line 2, feedbackEnabled: true
3. set_input_profile → apply "Novation Launchpad Mini MK3"
4. configure_plugin_params → set initmessage to "Novation Launchpad Mini MK3 Developer Mode" (enters Programmer Mode automatically)
5. Ask user to enter Programmer Mode on Launchpad (hold Session → orange pad → release) — only needed first time
6. patch_fixtures + create_scenes → build show content
7. add_vc_buttons → create Virtual Console
8. map_vc_inputs → map pads to buttons
9. configure_vc_feedback → set LED colors + modes per pad

### Why the second port?
The Launchpad Mini MK3 has two USB MIDI ports:
- Port 1 (MIDI): Does NOT receive pad presses in Programmer Mode
- Port 2 (DAW): Receives all pad input + accepts LED feedback

### Launchpad Mini MK3 Pad Grid (Programmer Mode)
Note numbers for each pad:
     [91][92][93][94][95][96][97][98]   Top row (CC)
[89] [81][82][83][84][85][86][87][88]   Row 8
[79] [71][72][73][74][75][76][77][78]   Row 7
[69] [61][62][63][64][65][66][67][68]   Row 6
[59] [51][52][53][54][55][56][57][58]   Row 5
[49] [41][42][43][44][45][46][47][48]   Row 4
[39] [31][32][33][34][35][36][37][38]   Row 3
[29] [21][22][23][24][25][26][27][28]   Row 2
[19] [11][12][13][14][15][16][17][18]   Row 1
QLC+ channel = 128 + note number (e.g., pad note 81 = channel 209)

### LED Feedback: 3 Parameters Per Pad
- idleValue: LED color when button is NOT active (shows what it does)
- activeValue: LED color when button IS active (confirms it's running)
- ledMode: static (solid) / flashing (blink) / pulsing (breathe)

### Color Palette (key values from profile — doubled for QLC+)
0=Off  2=White30%  4=White60%  6=White100%
8-14=Red shades    16-20=Orange   22-26=Yellow
28-34=Green        36-42=Cyan     44-54=Blue
56-66=Purple       68-76=Pink

### Row Assignment Strategy
- Row 8: System (Blackout, Stop All, Master)
- Row 7: Moods/Color palette (pad color = scene color)
- Row 6: Effects (Chase, Strobe, Pulse)
- Row 5: Movement/EFX
- Row 4-1: Songs, cues, or custom per show

### Ask the user how they want the pads to behave:
- Which colors for idle vs active?
- Flashing for warnings? Pulsing for active effects?
)";
            return guideText;
        },
        std::nullopt,
        std::string("Get the professional lighting show design guide including audio-reactive patterns, beat/tempo system, and OSC integration. Call this before designing a show."),
        std::nullopt
    ));
}
