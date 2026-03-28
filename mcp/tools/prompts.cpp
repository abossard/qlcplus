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
# Professional Lighting Show Design Guide

## Layered Approach
Design shows in independent layers that can be mixed:
- **Ambient/Wash**: Base illumination with broad coverage (pars, washes)
- **Color**: Mood-setting color washes (RGB fixtures)
- **Accent**: Focused highlights, specials (spots, gobos)
- **Effects**: Dynamic movement, chases, strobes (moving heads, strobes)

Not every show needs all layers — adapt to available fixtures.

## Energy Curve
Shape intensity and complexity over time:
- **Intro**: Low intensity (20-40%), cool/neutral colors, minimal movement
- **Buildup**: Gradually increase intensity, add movement, warmer colors
- **Peak/Drop**: Maximum intensity + effects, then sudden change (blackout, color snap)
- **Sustain**: Medium-high energy, active effects
- **Outro**: Gradual fade, return to cool/neutral

## Color Psychology
- Warm (red, amber, orange): Energy, passion, excitement
- Cool (blue, cyan, teal): Calm, mystery, depth
- Green: Nature, freshness
- Purple/magenta: Luxury, creativity
- White: Clarity, openness

## QLC+ Implementation Patterns
- **Scene** = Static look (one "moment" of lighting)
- **Collection** = Mood/Phase (multiple functions running in parallel)
- **Chaser** = Sequential animation (color chase, intensity build)
- **EFX** = Algorithmic position movement (circle, figure-8 for moving heads)
- **SoloFrame** = Mutually exclusive choices (only one mood active at a time)

## HTP/LTP Channel Rules
- Intensity/dimmer channels use HTP (Highest Takes Precedence) — safe to layer
- Position (Pan/Tilt), color wheels, gobos use LTP (Latest Takes Precedence)
- Separate intensity scenes from position/color scenes for maximum flexibility

## Beat/Tempo System
QLC+ has a beat-synchronized timing system. When tempoType is "beats":
- 1000 = 1 beat, 500 = 1/2 beat, 250 = 1/4 beat, 125 = 1/8 beat
- All timing (fadeIn, fadeOut, holdTime) scales automatically with BPM
- BPM can come from internal clock, MIDI, audio detection, or OS2L

### Strobe Buildup Pattern
To create a strobe that accelerates over N beats:
1. Create a Scene targeting the fixture's Shutter/Strobe DMX channel at max value
2. Create a single-step Chaser with tempoType "beats", fadeIn = N × 1000
   The chaser fades the strobe channel from 0 to max over N beats

### Beat-Synced Chase
Create a Chaser with tempoType "beats", holdTime 1000 (on-beat) or 500 (double-time).
Use runOrder "loop" for continuous, "pingpong" for back-and-forth.

## Audio-Reactive Patterns (OSC/External Input)

### Architecture: OSC Input → VC Slider → Fixture Control
External audio analysis (e.g., from Ableton, Resolume, custom analyzers) sends
OSC messages with audio-reactive values (bass level, treble, BPM, beat trigger).
Map these to VC sliders to create audio-reactive lighting.

### Pattern 1: Direct Audio-Reactive Intensity
Setup: OSC sends bass level (0-255) on channel 1
- Create a VCSlider in "level" mode controlling dimmer channels of wash fixtures
- Use map_vc_inputs to map OSC universe/channel to this slider
- Result: Wash fixtures pulse with the bass

### Pattern 2: Audio-Reactive with Manual Override (Layered Faders)
Setup: Three faders in a Frame (NOT SoloFrame — they stack):
1. **Submaster fader** (mode: "submaster") — manual master intensity cap
2. **Level fader** (mode: "level") — maps to fixture dimmer channels, OSC-driven
3. **Playback fader** (mode: "playback") — controls a color chaser's intensity

How they interact:
- The submaster sets a ceiling (e.g., 80%)
- The OSC-driven level fader pulses 0-255 with the music
- HTP means the highest of (level fader, playback fader) wins for each channel
- The submaster multiplies the result: final = max(level, playback) × submaster%

### Pattern 3: Audio-Reactive Effect Selection (SoloFrame)
Setup: SoloFrame with buttons, each linked to a different effect Collection:
- Button "Bass Pulse": Collection = bass-driven intensity scene
- Button "Treble Sparkle": Collection = treble-driven strobe scene
- Button "Full Reactive": Collection = all audio channels active
Only one can be active at a time (SoloFrame ensures mutual exclusion).
Map OSC buttons to these VC buttons for remote triggering.

### Pattern 4: Fixture Group Separation for Audio
Split fixtures into audio-reactive groups:
- **Bass group** (floor pars): OSC bass channel → level slider → warm colors
- **Mid group** (side washes): OSC mid channel → level slider → accent colors
- **High group** (strobes/beams): OSC treble channel → level slider → white/strobe
Each group has its own OSC channel mapping, creating frequency-split lighting.

### Pattern 5: OSC-Driven Scene Blending
Create multiple color scenes (Warm, Cool, Intense) and map OSC faders to
playback sliders controlling each scene's intensity. By mixing slider values,
the operator (or audio analyzer) can blend between moods smoothly.

## Virtual Console Layout Patterns

### Basic Show Layout
- **Moods SoloFrame**: Mutually exclusive color/mood buttons
- **FX Frame**: Effect triggers (chase, strobe, position), can layer on top of mood
- **Masters Frame**: Submaster slider + Blackout + Stop All
- **Position Frame**: XY pad + EFX buttons for moving heads

### Audio-Reactive Layout
- **Audio Input Frame**: Level sliders mapped to OSC channels (bass, mid, treble)
- **Manual Override Frame**: Submaster slider to cap audio intensity
- **Mode SoloFrame**: Buttons to switch between audio-reactive modes
- **Scenes Frame**: Manual scene buttons for non-reactive moments

### Naming Convention
Use: "[Layer] - [Description]"
Examples: "Wash - Warm Amber", "FX - Fast Strobe", "Audio - Bass Pulse", "OSC - Treble Input"
)";
            return Json(guideText);
        },
        std::nullopt,
        std::string("Get the professional lighting show design guide including audio-reactive patterns, beat/tempo system, and OSC integration. Call this before designing a show."),
        std::nullopt
    ));
}
