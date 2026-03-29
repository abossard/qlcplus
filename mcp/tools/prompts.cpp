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
# QLC+ Show Design Guide — DJ / Club / Live

## Step-by-Step Build Order

1. `query_fixtures` → discover what you have (RGB pars, moving heads, strobes, hazer)
2. Ask user: event type, energy level, fixture placement
3. Design **orthogonal layers** (see Layer Architecture below)
4. Build layers bottom-up: scenes → chasers → collections → VC page
5. Use `build_show_page` for the VC — one call per page

---

## Layer Architecture (Orthogonal — No Channel Conflicts)

Each layer owns specific DMX channels. Layers combine freely via LTP.

| Layer | Owns | Controls | VC Widget |
|-------|------|----------|-----------|
| **Mood** | RGB/CMY/White channels | Color palette | SoloFrame buttons |
| **Texture** | Gobo, prism, color wheel, focus | Pattern/atmosphere | SoloFrame buttons |
| **Energy** | Pan/tilt speed, gobo rotation | Movement aggression | SoloFrame buttons |
| **Position** | Pan, tilt | Where the beam points | SoloFrame or XY pad |
| **Dimmer** | Intensity/dimmer channels | Brightness | Slider (submaster or audio-reactive) |
| **Strobe** | Shutter/strobe channels | Flash accents | Flash buttons (hold to fire) |
| **Haze** | Hazer output/fan | Atmosphere density | Toggle buttons |

**Rule:** A channel appears in exactly ONE layer. No HTP conflicts.

---

## Phase System (4-Phase DJ Set)

| Phase | Energy | Palette | Motion |
|-------|--------|---------|--------|
| P1 Starter | Low-Med | Green, yellow, warm amber | Smooth, legible groove |
| P2 Buildup | Med-High | Blue, cyan, purple | Tighter, sharper, directional |
| P3 Peak | Maximum | Purple, magenta, red, black contrast | Fast snaps, high contrast |
| P4 Release | Medium | Blue, lavender, aqua | De-escalate, flow, melody |

Phase = Collection bundling: texture scene + default mood + energy preset + haze level.

---

## Beat-Synced Chasers (tempoType: "beats")

Timing in ms where 1000 = 1 beat (scales with BPM via OS2L/MIDI/internal clock):

| Feel | Hold | FadeIn | FadeOut | Total |
|------|------|--------|---------|-------|
| Ambient drift | 8000 | 4000 | 4000 | 16 beats |
| Musical flow | 4000 | 2000 | 2000 | 8 beats |
| Driving pulse | 2000 | 1000 | 1000 | 4 beats |
| Aggressive snap | 1000 | 250 | 250 | ~2 beats |
| Glitch stutter | 500 | 0 | 0 | 1 beat |
| Seizure (use sparingly) | 250 | 0 | 0 | 1/2 beat |

### Mood Chase per Phase
Create a chaser that cycles through phase-appropriate colors:
- P1: Deep Jungle → Amber → Tropical Cyan (hold=8 beats, fade=4 beats)
- P3: Blood Moon → Violet → Arctic White (hold=4 beats, fade=1 beat)

---

## Common Lighting Patterns

### Phantom Scan (Dark Swipe + Beam Reveal)
2-step chaser: dark snap → beam reveal.
- Step 1 "Dark": dimmer=0, pan/tilt snap to new position, pt_speed=0 (instant), arm gobo+color. Hold=0ms.
- Step 2 "Beam": dimmer=255, pt_speed=220 (slow crawl back), beam is ON. Hold=4 beats.
- Result: light appears to teleport, then slowly sweeps. Dramatic and musical.
- Variations: change gobo/color per variant (Red Phantom, Blue Phantom, etc.)

### Color Chase
Chaser cycling through mood scenes with offset timing.
- Use propagationMode "Serial" on EFX for moving heads (each fixture starts at different phase).
- For pars: chaser with 2-4 color scenes, 2-beat hold, 1-beat fade.

### Strobe Accent
Flash-mode buttons (hold to fire). Never automate sustained strobe.
- Map to Launchpad bottom row with flashing red active LED.
- Layer: strobe channel only, no color/position — those come from other layers.

### Position Snap
Scene with pan+tilt values, pt_speed=0 (instant). Use as chaser steps for beat-synced snaps.
- 4-position chaser: Left→Right→Up→Down, 1-beat hold, 0 fade = staccato impact.

### Wash Fade
Crossfade between two mood scenes using chaser with long fades.
- Hold=8 beats, fadeIn=4 beats. runOrder: pingpong for continuous breathing.

### EFX Movement Patterns
| Pattern | Algorithm | Width | Height | Speed | Feel |
|---------|-----------|-------|--------|-------|------|
| Gentle sway | Eight | 40 | 30 | 8000 | Background drift |
| Rhythmic scan | Lissajous (2:3) | 80 | 60 | 4000 | Musical groove |
| Bar sweep | Line | 120 | 20 | 2000 | Horizontal wipe |
| Beat snap | SquareTrue | 100 | 80 | 1000 | Corner-to-corner hits |
| Glitch jitter | Lissajous (3:2) | 60 | 60 | 250 | Nervous twitching |
| Chaos engine | Lissajous (7:11) | 200 | 150 | 333 | Polyrhythmic mayhem |

---

## Audio-Reactive Sliders

Map OSC or audio-trigger input to VCSlider level mode for live response.
The user connects these to their audio source (OS2L, audio input, or external OSC).

| Slider | Channel Group | Purpose |
|--------|--------------|---------|
| Bass Pulse | Dimmer channels (pars/wash) | Fixtures pulse with kick drum |
| Mid Drive | Gobo rotation + prism rotation | Texture and beam spread breathe with melody |
| Treble Flash | Strobe/shutter channels | High-frequency accents trigger flashes |
| Master | Submaster (all) | Overall brightness cap |

**Setup:** Create level-mode sliders targeting specific fixture channels.
Agent does NOT assign the OSC/audio source — user configures that in QLC+ I/O settings.

---

## VC Page Layout (use build_show_page)

```
Page "Show Control":
  [Phases] solo=true     → P1 Jungle | P2 Buildup | P3 Peak | P4 Release
  [Moods] solo=true      → Deep Jungle | Amber | Midnight Blue | Blood Moon | Violet | Cyan
  [Energy] solo=true     → Entry | Flow | Build | Bullet | Peak | Accent
  [FX] solo=false        → Gentle Sway | Bar Sweep | Beat Snap | Glitch
  [Quick Shots] flash    → UV Burst | Strobe Hit | Snap Left | Snap Right
  [Controls]             → Master (submaster) | BLACKOUT | STOP ALL
```

---

## Non-Negotiable Rules

1. **Palette discipline**: 80%+ runtime inside phase palette. Max 1 accent color outside.
2. **Contrast rhythm**: Every scene needs rest + hit state. No full-intensity >45 seconds.
3. **Strobe restraint**: Accents only, never sustained >8 seconds.
4. **Audio-reactivity**: Every phase should have at least one audio-reactive element active.
5. **Layer separation**: Never put the same DMX channel in two different layer scenes.
6. **Name clarity**: Phase-prefix names (P1-, P2-) or layer-prefix (Mood-, FX-, Energy-).

---

## Launchpad Mini MK3

Use `configure_launchpad` (single call) to auto-detect and set up.

Row assignment:
- Row 8: Phase presets (purple LEDs, pulsing)
- Row 7: Energy levels (cyan LEDs, pulsing)
- Row 6: Mood colors (LED = output color, pulsing)
- Row 5: Texture/gobo (orange LEDs, pulsing)
- Row 4-3: EFX/chasers (cyan/yellow, pulsing)
- Row 2: Quick shots (white LEDs, static — flash mode)
- Row 1: System (BLACKOUT=flashing red, STOP ALL=flashing red, Haze=green)

**LED colors match output**: green scene = green LED, blue = blue, etc.
**Idle LEDs always visible** at 30% brightness (never off).
**Pulsing** = toggle/persistent, **Flashing** = danger/intense, **Static** = momentary.

Pad note → QLC+ channel: `128 + note` (e.g., pad 81 = channel 209).

### Color Palette (velocity values)
```
0=Off  6=White  2=White30%
10=Red  14=Red30%  18=Orange  22=Orange30%
26=Yellow  30=Yellow30%  42=Green  46=Green30%
74=Cyan  78=Cyan30%  82=Blue  86=Blue30%
98=Purple  102=Purple30%  106=Magenta  110=Magenta30%
```
)";
            return guideText;
        },
        std::nullopt,
        std::string("Get the professional lighting show design guide including audio-reactive patterns, beat/tempo system, and OSC integration. Call this before designing a show."),
        std::nullopt
    ));
}
