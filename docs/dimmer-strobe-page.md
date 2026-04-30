# Dimmer & Strobe Page — QLC+ Control Architecture

## Overview

A dedicated VC page managing ALL intensity and strobe for every fixture in the rig. Works alongside the DJ Expression page which handles color and movement.

**This page answers one question: "How bright is each light, and is it strobing?"**

## Signal Flow

Each slider's value is scaled by every enclosing submaster (multiplied) **before** being
written to the DMX channel. HTP then merges all writes. The formula only holds when all
sources sit inside the same submaster chain.

```
Per-fixture signal flow:

  Floor  ─→ ×Max ─→ ×Group ─→ ×GM ─┐
  Manual ─→ ×Max ─→ ×Group ─→ ×GM ─┼─→ HTP merge → DMX output
  Audio  ─→ ×Max ─→ ×Group ─→ ×GM ─┘
```

- **HTP merge**: picks the highest scaled value from all three sources
- **Submasters**: multiply each source's output before HTP merge
- **Grand Master**: final global multiplier (Reduce mode), applied post-widget

## Layer Ownership

| This page owns | Channels | Notes |
|----------------|----------|-------|
| Dimmer | BB ch5, Hero ch13, UV ch0 | Intensity channels (HTP) |
| Strobe | BB ch6, Hero ch14 | Shutter channels (LTP) |
| Hazer output | Hazer ch0 | Separate from lighting intensity |

| This page does NOT touch | Owned by |
|--------------------------|----------|
| Color (RGBW) | DJ Expression / Mood |
| Movement (Pan/Tilt) | DJ Expression / Movement |
| Gobos, Prism, Focus | Not controlled |

## Fixtures

| Fixture | ID | Dimmer Ch | Strobe Ch | Notes |
|---------|-----|-----------|-----------|-------|
| Hero Spot Wash | 0 | ch13 | ch14 | RGBW moving head |
| Hazer | 1 | ch0 | — | Haze machine, not lighting |
| UV Wash | 2 | ch0 | — | UV bar |
| Beam Ball | 7 | ch5 | ch6 (0-9=off, 10-255=slow→fast) | 10-head RGBW ball |

## Intensity Controls (Per Fixture)

Each fixture gets an **Intensity Frame** containing:

### Manual Dimmer (Level Slider)
- Mode: `level`, targeting the fixture's dimmer channel
- The DJ's primary brightness control
- HTP: competes with Floor and Audio Pump — highest wins

### Floor Slider (Level Slider)
- Mode: `level`, targeting the SAME dimmer channel
- Sets minimum brightness **relative to peers** (Manual/Audio) — HTP picks the highest
- **Important**: Floor is inside the Max Submaster frame, so it gets scaled down.
  With Max=50% and Floor=40, effective write is 20. With Max=0%, Floor=0 → fixture dark.
- Floor is NOT an absolute minimum — it's the minimum before submaster scaling
- For a true unkillable safety floor, place a separate slider OUTSIDE the Max/Group frames

### Fixture Max (Submaster Slider)
- Mode: `submaster`, wrapping the fixture's Intensity Frame
- Scales ALL intensity sources for this fixture proportionally
- At 80%: if HTP result is 200, output is 160
- At 0%: fixture is fully dark regardless of Manual/Floor/Audio
- Use case: "The Beam Ball is too bright, cap it at 70%"

### Audio Pump (Level Slider, optional)
- Mode: `level`, targeting the SAME dimmer channel
- Driven by Audio Triggers widget (bass band)
- When audio is loud → slider value increases → HTP picks it up
- Competes with Manual and Floor — only wins if audio value is highest
- Use case: bass hits push brightness above the manual level

```
Per-fixture signal flow:

  Floor ──┐
  Manual ──┤──→ HTP (highest wins) ──→ × Max Submaster ──→ output
  Audio ──┘
```

## Audio Reactive System

### Audio Triggers Widget
- Single global Audio Triggers widget captures audio input
- Maps bass/volume bands to Audio Pump sliders per fixture
- Toggle: ON/OFF button to enable/disable audio reactivity

### Audio Amount Control
Each fixture's Audio Pump slider naturally controls the amount:
- Slider at 0: audio has no effect (below Manual/Floor)
- Slider responding to audio: only wins HTP when audio exceeds Manual level

### Audio Mode: Pump
Audio increases brightness on beats. This is the natural HTP behavior:
- Quiet → Audio Pump low → Manual/Floor wins → steady brightness
- Bass hit → Audio Pump spikes → exceeds Manual → brief brightness boost
- Beat ends → Audio Pump drops → Manual/Floor wins again

> **Note**: "Duck" mode (dimming on beats) is NOT possible with HTP.
> Ducking requires a Script that writes directly to the channel,
> bypassing HTP. This can be added as a Script function if needed.

## Strobe Controls

### Why ch6/ch14, not dimmer on/off?

Dimmer channels are HTP. If ANY source holds the dimmer at 255, a strobe scene
setting dimmer to 0 can never win — HTP always picks 255. The strobe is invisible.

Fixture-native strobe channels (ch6, ch14) are **LTP (Latest Takes Precedence)**.
They work independently of the dimmer. Push strobe speed up → fixture strobes.
Pull it down → strobe stops. No HTP conflict.

### Strobe Presets (Scenes)

Each preset writes fixture-specific strobe values because different fixtures
have different strobe ranges and off-values:

| Preset | Beam Ball ch6 | Hero ch14 | Behavior |
|--------|---------------|-----------|----------|
| Strobe Off | 0 | 0 (open) | No strobe |
| Strobe Slow | 60 | TBD | ~4 Hz |
| Strobe Medium | 130 | TBD | ~8 Hz |
| Strobe Fast | 220 | TBD | ~15 Hz |
| Strobe Max | 255 | TBD | Maximum speed |

### Per-Fixture Strobe Slider
- Level slider on each fixture's strobe channel
- Direct manual control of strobe speed
- Useful when presets don't match the desired speed

### Safety: STROBE OFF Button
- Large, prominent, always visible
- Writes 0 to ALL strobe channels
- Grand Master does NOT stop strobe (strobe channels aren't Intensity group)
- **Strobe presets MUST be in a SoloFrame** — starting STROBE OFF stops the running
  strobe scene. Without a SoloFrame, a still-running strobe scene re-writes its value
  every tick and the OFF button loses (LTP = last writer wins per tick).
- **This is the only reliable strobe kill** (when configured in a SoloFrame)

## Group Submaster

Wraps all fixture Intensity Frames:
- At 100%: pass-through
- At 50%: halves all fixture outputs
- At 0%: all lights off (but strobe still works — it's outside this frame)

Use case: "I want to bring the whole rig down to 40% for an intimate moment"

## Blackout

QLC+ has a built-in **Blackout toggle** (⊘ button) that forces ALL DMX outputs to zero.
It bypasses HTP, LTP, Grand Master, submasters — everything. This is the true emergency kill.
No need to engineer blackout through scenes or the layer system.

- **Grand Master** (Reduce mode, Intensity channel mode) is for proportional dimming
- **Blackout** is for "everything off NOW"

## Hazer

Treated separately from lighting intensity:
- NOT inside the All Intensity Group Frame
- NOT affected by Group Submaster or Grand Master (unless desired)
- Simple output slider + optional max cap
- Haze is atmospheric, not visual intensity

## Target VC Layout (Page 4)

```
┌──────────────────────────────────────────────────────────────────┐
│ 💡 DIMMER & STROBE                                        Page 4 │
│                                                                  │
│ ┌─ ALL INTENSITY (Group Frame) ──────────────┐  ┌─ GRAND ─────┐ │
│ │ [Group Submaster ████████████]              │  │  MASTER     │ │
│ │                                             │  │             │ │
│ │ ┌─ HERO ────┐ ┌─ UV ──────┐ ┌─ BEAM BALL ┐│  │    100%     │ │
│ │ │ Max  100% │ │ Max  100% │ │ Max  100%  ││  │     ██      │ │
│ │ │ Dim  ████ │ │ Dim  ████ │ │ Dim  ████  ││  │     ██      │ │
│ │ │ Floor ██─ │ │ Floor ██─ │ │ Floor ██─  ││  │     ██      │ │
│ │ │ Audio ██─ │ │ Audio OFF │ │ Audio ██─  ││  │     ██      │ │
│ │ └───────────┘ └───────────┘ └────────────┘│  │     ▼       │ │
│ └────────────────────────────────────────────┘  └─────────────┘ │
│                                                                  │
│ ┌─ STROBE ──────────────────────────────────────────────────────┐│
│ │ [⊘ STROBE OFF]  [SLOW]  [MED]  [FAST]  [MAX]                 ││
│ │                                                                ││
│ │ Hero: [████████──]     Beam Ball: [████████──]                ││
│ └────────────────────────────────────────────────────────────────┘│
│                                                                  │
│ ┌─ AUDIO REACTIVE ──────────┐  ┌─ HAZER ───────────────────────┐│
│ │ [🎙 ON/OFF]               │  │ Output: [████████──]          ││
│ │ Sensitivity: [████████──] │  │ Max:    [████──────]          ││
│ └───────────────────────────┘  └────────────────────────────────┘│
└──────────────────────────────────────────────────────────────────┘
```

## Adding a New Fixture

1. **Identify channels**: dimmer channel, strobe channel (if any)
2. **Create Intensity Frame** inside All Intensity Group:
   - Max Submaster slider
   - Manual Dimmer Level slider → dimmer channel
   - Floor Level slider → same dimmer channel
   - Audio Pump Level slider → same dimmer channel (optional)
3. **If fixture has strobe**: add per-fixture strobe slider + update preset scenes
4. **If fixture has no strobe**: skip strobe controls
5. **Connect Audio Triggers** to the new Audio Pump slider (if desired)
6. **No changes needed** to Group Submaster, Grand Master, or strobe presets (except adding the new fixture's strobe values to preset scenes)

## Interaction with DJ Expression Page

| DJ Expression controls | Dimmer & Strobe controls |
|------------------------|--------------------------|
| Color (RGBW) | Brightness (dimmer) |
| Movement (pan/tilt) | Strobe speed |
| Phase (energy structure) | Audio reactivity |
| Mood (color theme) | Min/Max/Grand Master |

These pages are fully independent. A DJ can:
- Set a mood color on DJ Expression
- Control brightness on Dimmer & Strobe
- Both work simultaneously, no conflicts

## Key Rules Summary

1. **Dimmer channels are HTP** — highest source wins, can't dim below any active source
2. **Floor slider sets minimum among peers** — but submasters scale it down. Max=0 → fully dark
3. **Max submaster caps maximum brightness** — multiplier applied before HTP merge
4. **Strobe uses native fixture channels (ch6/ch14)** — never use dimmer on/off
5. **Strobe presets must be in a SoloFrame** — otherwise OFF can't stop a running strobe scene
6. **Grand Master is intensity-only** — does not stop strobe (Shutter group ≠ Intensity group)
7. **Hazer is separate** — verify hazer channel group; if it's Intensity, GM will scale it
8. **Audio pump only adds brightness** — can't duck (dim on beats) via HTP alone
9. **Each slider has its own GenericFader** — HTP rejects writes lower than current pre-GM value each tick
