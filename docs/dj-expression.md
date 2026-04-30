# DJ Expression — QLC+ Lighting Control System

## Overview

DJ Expression is a **fixture-agnostic, emotion-driven** lighting control system for QLC+. It's designed for live DJ performance where you control **energy, mood, and movement** — not individual fixtures.

## Design Philosophy

- **Phases** describe energy levels (Chill → Freeze → Build → Drop)
- **Moods** describe color emotion (Jungle, Ocean, Fire, Neon)
- **Patterns** describe per-LED geometry on the Beam Ball (which heads are lit, how they move)
- **Movement** describes spatial behavior (None, Slow, Fast, Audio-reactive)
- **FX** are momentary accents (Strobe, White Hit)
- Each layer is **independent** — mix any mood with any phase, any pattern, and any movement

## Global Rules

**Every scene in this project that sets color (RGBW) channels must NOT set dimmer channels (Beam Ball ch5, Hero ch13).** This applies to ALL scenes — including Colors/, BB Patterns/, and Ball/ folders.

This rule exists because dimmer channels are HTP (Highest Takes Precedence). If any color scene sets dimmer=255, the strobe's dimmer=0 can never win, breaking the strobe layer entirely.

Only these 4 scenes may set dimmer channels:
- `DJX Dimmer Off` (ch5=0, ch13=0)
- `DJX Dimmer Low` (ch5=80, ch13=80)
- `DJX Dimmer Medium` (ch5=160, ch13=160)
- `DJX Dimmer Full` (ch5=255, ch13=255)

### Layer Separation (Critical)

Each DMX channel is owned by exactly one layer. Layers never cross boundaries.

```
┌─────────────┬──────────────────────────────────────┬───────────────┐
│ Layer       │ Channels Owned                        │ Controlled By │
├─────────────┼──────────────────────────────────────┼───────────────┤
│ Color       │ RGBW per head (BB ch7-46, Hero ch15-18) │ MOOD buttons │
│ BB Pattern  │ RGBW per head (BB ch7-46) — per-LED   │ PATTERN btns │
│             │ geometry with color baked in           │              │
│ Dimmer      │ Master dimmer (BB ch5, Hero ch13)     │ PHASE + slider│
│ Movement    │ Pan/Tilt/Speed (ch0-4)                │ MOVEMENT btns │
│ Strobe/FX   │ Strobe ch6, dimmer rapid on/off       │ FX buttons    │
└─────────────┴──────────────────────────────────────┴───────────────┘
```

**Note**: Color and BB Pattern both write RGBW on the Beam Ball. They are **alternative** controls for the same channels — use MOOD for uniform whole-fixture color, or PATTERN for per-LED geometry with color. Don't run both simultaneously on the Beam Ball (HTP merge will produce unexpected results).

**Rules**:
- Color/Pattern scenes NEVER set dimmer channels.
- Dimmer scenes NEVER set color channels.
- BB Pattern scenes are generated from a concept × color matrix (see `docs/ball-effects-plan.md`).

### Why This Matters

QLC+ uses HTP (Highest Takes Precedence) for intensity channels. If a color scene sets dimmer=255 and a strobe scene sets dimmer=0, the strobe can never turn the light off — 255 always wins. By separating layers, each control works independently.

## Fixtures

| Fixture | ID | Type | Color Channels | Dimmer | Movement |
|---------|-----|------|---------------|--------|----------|
| Beam Ball | 7 | 10-head RGBW, moving | ch7-46 (R/G/B/W × 10 heads) | ch5 | ch0-4 |
| Hero Spot Wash | 0 | RGBW wash+spot, moving | ch15-18 (R/G/B/W) | ch13 | ch0-4 |

### Adding a New Fixture

1. Patch the fixture in QLC+
2. Create/update color scenes to include the new fixture's RGBW channels
3. Create/update dimmer scenes to include the new fixture's dimmer channel
4. If it's a mover, add it to movement EFX patterns
5. No changes needed to chasers, collections, or VC — they reference scenes by name

## Function Inventory

### Color Scenes (18 total)

All set ONLY RGBW channels. No dimmer. Both fixtures targeted.
Beam Ball uses center-bright gradients across 10 heads.

#### 🌿 Jungle Theme
| Function | ID | RGB | Character |
|----------|-----|-----|-----------|
| DJX JG Deep Green | 96 | #006030 | Deep emerald, mysterious |
| DJX JG Teal | 97 | #004040 | Dark cyan, underwater |
| DJX JG Amber | 98 | #CC8000 | Warm gold, firelight |
| DJX JG Canopy | 99 | #20CC40 | Bright green, lush |

#### 🌊 Ocean Theme
| Function | ID | RGB | Character |
|----------|-----|-----|-----------|
| DJX OC Deep Blue | 100 | #000080 | Navy, deep water |
| DJX OC Aqua | 101 | #008080 | Turquoise, tropical |
| DJX OC Storm | 102 | #303060 | Grey-purple, tension |
| DJX OC Seafoam | 103 | #40C0A0 | Light teal, surface |

#### 🔥 Fire Theme
| Function | ID | RGB | Character |
|----------|-----|-----|-----------|
| DJX FR Red | 104 | #CC0000 | Deep red, heat |
| DJX FR Amber | 105 | #CC6600 | Warm orange, campfire |
| DJX FR Orange | 106 | #FF4000 | Bright orange, flames |
| DJX FR Heat | 107 | #FFCC40 | Yellow-white, intense |

#### ⚡ Neon Theme
| Function | ID | RGB | Character |
|----------|-----|-----|-----------|
| DJX NE Magenta | 108 | #FF00CC | Hot pink, club |
| DJX NE Cyan | 109 | #00FFCC | Electric teal, futuristic |
| DJX NE UV Blue | 110 | #4400FF | Deep UV, blacklight |
| DJX NE Acid | 111 | #80FF00 | Acid green, rave |

#### Utility Colors
| Function | ID | Path | Description |
|----------|-----|------|-------------|
| DJX Color White Hit | 112 | Color/Utility | White palette ref (empty channels — use DJX White Hit instead) |
| DJX White Hit | 118 | FX | All RGBW=255, flash accent (NO dimmer). Used by WHITE HIT button. |
| DJX Blackout Color | 113 | Color | All RGBW=0, color blackout |

### Dimmer Scenes (4 total)

Set ONLY dimmer channels (BB ch5, Hero ch13). No color.

| Function | ID | Dimmer Value | Use Case |
|----------|-----|-------------|----------|
| DJX Dimmer Off | 114 | 0 | Blackout |
| DJX Dimmer Low | 115 | 80 (~31%) | Chill, ambient |
| DJX Dimmer Medium | 116 | 160 (~63%) | Standard, build |
| DJX Dimmer Full | 117 | 255 (100%) | Drop, impact |

### Color Chasers (8 total)

Each theme has a Slow and Fast variant. All beat-timed, looping.

| Function | ID | Steps | Hold | Fade | Use Case |
|----------|-----|-------|------|------|----------|
| DJX Jungle Slow | 119 | 4 colors | 8 beats | 4 beats | Chill/Freeze |
| DJX Jungle Fast | 120 | 4 colors | 2 beats | 1 beat | Build/Drop |
| DJX Ocean Slow | 121 | 4 colors | 8 beats | 4 beats | Chill/Freeze |
| DJX Ocean Fast | 122 | 4 colors | 2 beats | 1 beat | Build/Drop |
| DJX Fire Slow | 123 | 4 colors | 8 beats | 4 beats | Chill/Freeze |
| DJX Fire Fast | 124 | 4 colors | 2 beats | 1 beat | Build/Drop |
| DJX Neon Slow | 125 | 4 colors | 8 beats | 4 beats | Chill/Freeze |
| DJX Neon Fast | 126 | 4 colors | 2 beats | 1 beat | Build/Drop |

### Movement EFX (4 total)

Target both movers (Beam Ball + Hero). Pan/tilt only, no color/dimmer.

| Function | ID | Algorithm | Speed | Character |
|----------|-----|-----------|-------|-----------|
|----------|-----|-----------|-------|-----------|
| DJX Move Slow Circle | 129 | Circle | 10s | Gentle orbit |
| DJX Move Slow Sweep | 130 | Line2/Pingpong | 8s | Lazy horizontal sweep |
| DJX Move Fast Circle | 131 | Circle | 4s | Energetic orbit |
| DJX Move Fast Sweep | 132 | Line2/Pingpong | 3s | Aggressive sweep |
| DJX Move Freeze | 220 | Empty scene | — | Stops EFX, position latches (LTP) |

### FX Functions (2 total)

| Function | ID | Type | Behavior |
|----------|-----|------|----------|
| DJX Strobe Build | 127 | Chaser (SingleShot) | 8-beat escalation: 1×/beat → 2×/beat → 4×/beat |
| DJX Fast Strobe | 128 | Chaser (Loop) | Continuous 4×/beat strobe |

### Phase Collections (4 total)

Combine dimmer + movement. **No color** — mood is independent.

| Phase | ID | Contains | Energy Level |
|-------|-----|----------|-------------|
| DJX Phase Chill | 133 | Dimmer Low + Slow Circle | 🟢 Low |
| DJX Phase Freeze | 134 | Dimmer Medium (no movement) | 🟡 Tension |
| DJX Phase Build | 135 | Dimmer Medium + Fast Sweep + Strobe Build | 🟠 Rising |
| DJX Phase Drop | 136 | Dimmer Full + Fast Circle | 🔴 Peak |

### Beam Ball Patterns (generated)

Per-LED pattern effects for the Beam Ball. Generated from a **concept × color × speed matrix** — see [`docs/ball-effects-plan.md`](ball-effects-plan.md) for the full generator design.

**Pattern concepts** (39): Singles (10), Odd/Even/All/Off (4), Builds (10), Opposites (5), Comets (10)
**Colors**: White, Warm, Fire Red, Ocean Blue, Neon Cyan, etc.
**Speeds**: Slow (1 beat), Medium (½ beat), Fast (¼ beat)

Each combination produces a scene (`DJX BB {Color} {Pattern}`) or chaser (`DJX BB {Color} {Chase} {Speed}`). Start with White, add colors by re-running the MCP generator.

**Chase concepts** (7): Single Chase, Ping Pong, Comet, Opposite, Odd/Even Flip, Build Up, Random Sparkle

**Scripts** (2, hand-written):
- `DJX BB Fire Flicker` — random warm per-LED glow, infinite loop
- `DJX BB Buildup Explode` — fire-like buildup → full blast → blackout

All BB Pattern functions set RGBW per head. **Never set ch5 (master dimmer).**

## Virtual Console — Page 3 "DJ Expression"

```
┌──────────────────────────────────────────────────────────────────┐
│ 🎧 DJ EXPRESSION                                         Page 3 │
│                                                                  │
│ ┌─ ENERGY / PHASE (SoloFrame) ───────────────┐  ┌─ MASTER ────┐ │
│ │ [🌿 CHILL] [🧊 FREEZE] [🔥 BUILD] [⚡ DROP] │  │ [Intensity] │ │
│ └────────────────────────────────────────────┘  │  ████████    │ │
│                                                  │  ████████    │ │
│ ┌─ MOOD (Frame, 4 theme rows) ───────────────┐  │              │ │
│ │ Jungle: [Deep Green][Teal][Amber][Canopy]   │  │ [⊘ BLACKOUT]│ │
│ │ Neon:   [Magenta][Cyan][UV Blue][Acid]      │  └─────────────┘ │
│ │ Fire:   [Red][Amber][Orange][Heat]          │                  │
│ │ Ocean:  [Deep Blue][Aqua][Storm][Seafoam]   │                  │
│ └────────────────────────────────────────────┘                  │
│                                                                  │
│ ┌─ PATTERN / BALL (SoloFrame) ───────────────────────────────────┐│
│ │ [OFF][Sparkle][Geometry][Comet][Fire Embers][Rise Burst]       ││
│ └────────────────────────────────────────────────────────────────┘│
│                                                                  │
│ ┌─ MOVEMENT (SoloFrame) ─────────────────────┐  ┌─ TEMPO ─────┐ │
│ │ [Slow Circle][Slow Sweep][Fast Circle][Fast]│  │ (placeholder)│ │
│ └────────────────────────────────────────────┘  └─────────────┘ │
│                                                                  │
│ ┌─ FX / MOMENTS ────────────────────────────────────────────────┐│
│ │ [STROBE BUILD] [FAST STROBE] [WHITE HIT] [⊘ RESET]           ││
│ └────────────────────────────────────────────────────────────────┘│
└──────────────────────────────────────────────────────────────────┘
```

### How to Use

1. **Pick a PHASE** — sets energy level (dimmer + movement defaults)
2. **Pick a MOOD color** — sets the color hue (RGB on all fixtures)
3. **Pick a PATTERN** — sets which Beam Ball LEDs are lit (W geometry)
4. **Optionally override MOVEMENT** — pick a different motion pattern
5. **Use FX for accents** — strobe build for tension, white hit for impact
6. **Intensity slider** — master brightness override

### Typical DJ Set Flow

| Song Section | Phase | Mood / Pattern | Movement | FX |
|-------------|-------|---------------|----------|----|
| Intro | CHILL | Ocean Deep Blue (mood) | Slow Circle | — |
| Verse | CHILL | BB White Single Slow (pattern) | Slow Sweep | — |
| Pre-chorus | FREEZE | Hold color | FREEZE | — |
| Build-up | BUILD | BB Fire Red Build Med (pattern) | Fast Sweep | Strobe Build |
| Drop | DROP | BB Neon Cyan Comet Fast (pattern) | Fast Circle | — |
| Breakdown | CHILL | Jungle Teal (mood) | Slow Circle | — |

> **Mood vs Pattern**: Use MOOD for uniform whole-fixture color. Use BB Pattern for per-LED geometry. Don't stack both on the Beam Ball — they write to the same channels.

> **Note**: Color chasers (Jungle Slow/Fast, Ocean Slow/Fast, etc.) exist in the Function Manager under `DJ Expression/Chasers/` but are not currently exposed as buttons on Page 3. They can be started from the Function Manager or added as buttons later.

## Dimmer & Strobe Architecture

### The HTP Problem (Lessons Learned)

QLC+ uses **HTP (Highest Takes Precedence)** for intensity/dimmer channels. This means:
- If ANY source writes ch5=255, nothing else can dim it below 255
- A Level slider at 100% on ch5 will prevent any scene from darkening the light
- Strobe effects using dimmer on/off (ch5=255/0) will NEVER work if another source holds ch5=255

**This is why strobe must use ch6 (fixture-native strobe), not dimmer on/off.**

### Current Control Model

```
┌─────────────────────────────────────────────────────┐
│ INTENSITY (ch5) — HTP                               │
│                                                     │
│  Intensity Slider (Level mode) → Sets brightness    │
│  Phase Dimmer Scenes           → Sets brightness    │
│  HTP picks the HIGHEST value from all sources       │
│  Each tick zeros intensity channels, re-merges fresh│
│                                                     │
│  Result: slider = minimum brightness                │
│          (can't go darker than slider position)     │
└─────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────┐
│ Strobe (ch6) — LTP, independent                     │
│                                                     │
│  Strobe Slider (Level mode)──→ 0=off, 10-255=speed  │
│  Strobe Preset Buttons     ──→ Slow/Medium/Fast/Off │
│  LTP = whichever source wrote most recently wins     │
│  Slider and preset buttons override each other       │
│  ⚠ Put presets in a SoloFrame so OFF stops running   │
│    strobe scene (otherwise both contend per tick)     │
└─────────────────────────────────────────────────────┘
```

### Strobe Functions (ch6 based)

| Function | ID | ch6 Value | Speed |
|----------|-----|-----------|-------|
| DJX Strobe Off | 221 | 0 | Off |
| DJX Strobe Slow | 222 | 60 | ~4 Hz |
| DJX Strobe Medium | 223 | 130 | ~8 Hz |
| DJX Strobe Fast | 224 | 220 | ~15 Hz |
| DJX Strobe Max | 225 | 255 | Maximum |

### Old Strobe Functions (dimmer-based, DEPRECATED)

These use ch5 on/off and **do not work** when any other source holds ch5 high:
- `DJX Strobe Build` (ID 127) — kept for reference but unreliable
- `DJX Fast Strobe` (ID 128) — kept for reference but unreliable

### Rules
1. **Intensity slider = brightness floor** — push up for minimum brightness, never fully dark
2. **Strobe slider = strobe speed** — independent of dimmer, uses ch6
3. **Never use dimmer on/off for strobe effects** — HTP makes it impossible
4. **Blackout** — QLC+ built-in toggle (⊘ button). Forces ALL DMX outputs to zero, bypasses HTP/LTP/Grand Master entirely. This is the true emergency kill — always available, no configuration needed.

## Known Limitations & Layout Notes

- **Page 3 layout needs manual cleanup** — built via MCP, positions are functional but not pixel-perfect. Open VC editor to align.
- **MOOD frame is not a SoloFrame** — multiple mood buttons can be active simultaneously.
- **TEMPO section is a placeholder** — needs BPM source (OS2L/VirtualDJ or manual tap).
- **⊘ RESET is a Stop-All panic button** — stops ALL running functions.
- **EFX speed is in milliseconds, not beats** — won't beat-align at BPMs other than 120.
- **Color chasers not on Page 3** — 8 chasers exist in Function Manager but have no buttons yet.
- **Old dimmer-based strobe (DJX Strobe Build/Fast Strobe) is deprecated** — use ch6 strobe instead.

### Target VC Layout (Page 3)
```
┌──────────────────────────────────────────────────────────────────┐
│ 🎧 DJ EXPRESSION                                         Page 3 │
│                                                                  │
│ ┌─ ENERGY / PHASE (SoloFrame) ───────────────┐  ┌─ MASTER ────┐ │
│ │ [CHILL] [FREEZE] [BUILD] [DROP]             │  │ [Intensity] │ │
│ └────────────────────────────────────────────┘  │ [Strobe   ] │ │
│                                                  │              │ │
│ ┌─ MOOD ─────────────────────────────────────┐  │ [⊘ BLACKOUT]│ │
│ │ Jungle: [Deep Green][Teal][Amber][Canopy]   │  └─────────────┘ │
│ │ Ocean:  [Deep Blue][Aqua][Storm][Seafoam]   │                  │
│ │ Fire:   [Red][Amber][Orange][Heat]          │                  │
│ │ Neon:   [Magenta][Cyan][UV Blue][Acid]      │                  │
│ └────────────────────────────────────────────┘                  │
│                                                                  │
│ ┌─ PATTERN / BALL (SoloFrame) ───────────────────────────────────┐│
│ │ [OFF][Sparkle][Geometry][Comet][Fire Embers][Rise Burst]       ││
│ └────────────────────────────────────────────────────────────────┘│
│                                                                  │
│ ┌─ MOVEMENT (SoloFrame) ─────────────────────┐  ┌─ TEMPO ─────┐ │
│ │ [Slow ○][Slow ↔][Fast ○][Fast ↔][❄ FREEZE] │  │ (placeholder)│ │
│ └────────────────────────────────────────────┘  └─────────────┘ │
│                                                                  │
│ ┌─ FX / MOMENTS ────────────────────────────────────────────────┐│
│ │ [STROBE SLOW][STROBE MED][STROBE FAST][STROBE OFF]            ││
│ │ [WHITE HIT] [⊘ RESET]                                         ││
│ └────────────────────────────────────────────────────────────────┘│
└──────────────────────────────────────────────────────────────────┘
```

## Extending the System

### Adding a Color Theme

1. Define 4 colors with hex values
2. Create 4 color scenes (RGBW, both fixtures) in `DJ Expression/Color/{ThemeName}`
3. Create Slow chaser (8 beat hold, 4 beat fade) and Fast chaser (2 beat hold, 1 beat fade)
4. Add buttons to the MOOD section on Page 3
5. Optionally: add the color to `ball-effects-plan.md` color palette and re-run the BB pattern generator

### Adding a Phase

1. Create a Collection combining: dimmer scene + movement EFX + optional FX
2. Add a button to the ENERGY SoloFrame on Page 3

### Adding a Fixture

1. Patch in QLC+
2. Update all DJX color scenes to include new fixture's RGBW channels
3. Update DJX dimmer scenes to include new fixture's dimmer channel
4. If mover: add to EFX patterns via fixtureIDs
5. Add new fixture's dimmer channel to the Page 3 **Intensity slider**
6. No chaser/collection changes needed — they reference scenes by name

### Adding a BB Pattern

See [`docs/ball-effects-plan.md`](ball-effects-plan.md) — add a row to the pattern definitions table, then re-run the generator for all desired colors.

## File Organization

```
DJ Expression/
├── Color/
│   ├── Jungle/        (4 color scenes)
│   ├── Ocean/         (4 color scenes)
│   ├── Fire/          (4 color scenes)
│   ├── Neon/          (4 color scenes)
│   └── Utility/       (White Hit, Blackout Color)
├── Dimmer/            (Off, Low, Medium, Full)
├── Chasers/           (8 color chasers: Slow+Fast × 4 themes)
├── Movement/          (4 EFX + Freeze)
├── FX/                (Strobe scenes)
├── BB Patterns/       (generated: pattern × color scenes)
│   ├── White/
│   ├── Fire Red/
│   └── .../
├── BB Chasers/        (generated: chase × color × speed)
│   ├── White/
│   └── .../
├── BB Scripts/        (Fire Flicker, Buildup Explode)
└── Phases/            (4 collections: Chill, Freeze, Build, Drop)
```
