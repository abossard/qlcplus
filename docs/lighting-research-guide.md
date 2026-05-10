# Lighting Research Guide for QLC+ MCP Agents

This guide is for AI agents (Copilot CLI, Claude, or any MCP-capable assistant) that create and refine LED effects through QLC+ MCP tools. QLC+ ships with **45 audio-reactive RGB Matrix algorithms** covering spectrum meters, organic textures, beat-synced color fills, scanners, particle bursts, strobes, and more. The agent is the researcher: develop a hypothesis, create batches of experiments, preview with the user, learn from feedback, and keep only the winners.

---

## Research Workflow

### Phase 1 — Theory

Develop a hypothesis with the user before touching any tools.

- **Genre / mood / energy**: techno drop vs. ambient build vs. house groove
- **Rig constraints**: fixture count, layout (strip, matrix, scattered), color capabilities (RGB, RGBW, UV)
- **Success criteria**: what does "good" mean? Subtle? Aggressive? Readable from afar? Beat-locked?

### Phase 2 — Discover

Query the rig and available algorithms.

- `query_fixtures` — what's patched, what channels are available
- `query_fixture_groups` — which groups map to which physical zones
- `query_rgb_algorithms` — which algorithms exist in this build, with their properties

### Phase 3 — Design Batch

Design 3–5 experiments spanning different approaches. Each gets:
- A **name**: `EXP-{round}-{A..E} {description}`
- A **hypothesis**: why this combination might work for the user's goal

Aim for diversity in the first round: different algorithm families, different color palettes, different energy levels.

### Phase 4 — Create

Create all experiments in one `create_rgb_matrices` call. Use `query_fixture_groups` first to get the `fixtureGroupID` — it's required for every entry.

```json
{
  "items": [
    {
      "name": "EXP-1-A Bass Fire Neon",
      "fixtureGroupID": 1,
      "algorithm": "Audio Fire",
      "colors": ["#ff00aa", "#00d4ff"],
      "duration": "1/4",
      "controlMode": "RGB"
    },
    {
      "name": "EXP-1-B Ambient Plasma",
      "fixtureGroupID": 1,
      "algorithm": "Audio Plasma",
      "colors": ["#4400ff", "#00ffcc", "#ffffff"],
      "duration": "2",
      "fadeIn": "1",
      "fadeOut": "1"
    },
    {
      "name": "EXP-1-C Beat Colors Techno",
      "fixtureGroupID": 1,
      "algorithm": "Audio Beat Colors",
      "colors": ["#ff0000", "#00ff00", "#0000ff", "#ffffff"]
    },
    {
      "name": "EXP-1-D Spectrum Wall",
      "fixtureGroupID": 1,
      "algorithm": "Audio Spectrum",
      "colors": ["#ff0000", "#ffff00", "#00ff00", "#00ffff", "#0000ff"]
    },
    {
      "name": "EXP-1-E Glitch Strobe",
      "fixtureGroupID": 1,
      "algorithm": "Audio Glitch",
      "colors": ["#ffffff", "#ff00aa"],
      "duration": "1/4"
    }
  ]
}
```

### Phase 5 — Preview

Ask the user to test each experiment. Collect quick feedback:
- Which ones feel right? Which are wrong?
- What's good: colors? movement? energy? beat sync?
- What's bad: too fast? too dim? wrong mood?

### Phase 6 — Refine

Take the top 2–3 winners and create 3–5 variations of each. Mutate one axis at a time:
- Colors (same algorithm, different palette)
- Speed/timing (same look, different energy)
- Properties (algorithm-specific knobs)
- Control mode (RGB → RGBW, White, etc.)

### Phase 7 — Export

- Rename finalists with descriptive permanent names
- Delete `EXP-` leftovers with `delete_functions`
- Save winning recipes to `autolight-presets.json`

---

## MCP Tools for This Workflow

- `query_fixtures` — inspect patched fixtures
- `query_fixture_groups` — find target fixture groups for RGB matrices
- `query_rgb_algorithms` — list available algorithms and their properties
- `create_rgb_matrices` — create effect experiments (batch-capable)
- `create_scenes` — create static looks or chaser steps
- `create_chasers` — create beat-timed scene sequences
- `create_collections` — combine effects in parallel
- `query_functions` — audit existing functions and find experiments
- `delete_functions` — remove rejected `EXP-` experiments

---

## Effect Selection Cheat Sheet

Quick-pick by scenario:

| I want… | Try these |
|---------|-----------|
| Bass-driven drop | Audio Strobe, Audio Shockwave, Audio Bass Laser, Audio Fire |
| Ambient texture | Audio Plasma, Audio Water, Audio Lava Lamp, Audio Soap |
| Beat-synced color cycling | Audio Beat Colors, Audio Chaser, Audio Hue Shift |
| Spectrum visualization | Audio Spectrum, Audio Equalizer, Audio Wavelength |
| Scanner / beam look | Audio Scan, Audio Scan Multi, Audio DJ Light |
| Particle bursts | Audio Fireworks, Audio Shot, Audio Shockwave |
| Organic / fluid motion | Audio Vortex, Audio Tunnel, Audio Reaction-Diffusion |
| Digital / glitch aesthetic | Audio Glitch, Audio Glitch 2, Audio Cellular |
| Intensity buildup → drop | Audio Buildup, Audio Reactor, Audio Energy |
| Background fill / wash | Audio Aurora, Audio Hue Shift, Audio Energy 2 |

---

## Audio Quick Reference (Top 10 Properties)

For normal MCP effect creation, use `query_rgb_algorithms` to discover algorithm properties. The raw `audio.*` API below is for script authors writing custom algorithms.

| Feature | Path | Range | Best for |
|---------|------|-------|----------|
| Bass power | `audio.power.low` | 0–1 | Bass-driven effects |
| Beat trigger | `audio.beat.fired` | bool | Beat-synced accents |
| Beat pulse | `audio.beat.cosPulse` | 0–1 | Smooth beat-locked LFO |
| Onset | `audio.onset.fired` | bool | Transient-driven accents |
| Bar beat | `audio.bar.beat` | 0–3 | 4-beat color cycling |
| Downbeat | `audio.bar.downbeatFired` | bool | Drop triggers |
| Kick | `audio.beat.kick` | bool | Kick-driven strobes |
| Volume | `audio.volume.normalized` | 0–1 | Overall loudness |
| Spectrum | `audio.spectrum.full` | array | Per-frequency visualization |
| Dominant band | `audio.power.dominant` | `"low"` / `"mid"` / `"high"` | Adaptive effects |

---

## Full Effect Catalog

45 audio-reactive algorithms in 7 groups. **EDM**: H = high suitability, M = medium, L = low. **LedFX**: ✦ = ported from LedFX. **Perf**: Light / Medium / Heavy.

### Group 1: Meters / Spectrum / Equalizers

| Effect | Description | EDM | LedFX | Perf |
|--------|-------------|-----|-------|------|
| Audio Spectrum | Multi-channel spectrum display with filtered and unfiltered blend | H | ✦ | Medium |
| Audio Equalizer | Vertical frequency bars rising from bottom with peak markers | H | ✦ | Medium |
| Audio Power | Full-width gradient spectrum with bass boost and random spark bursts | H | ✦ | Medium |
| Audio Wavelength | Smooth gradient-colored spectrum wave across display with optional roll | M | ✦ | Medium |
| Audio Scroll | Scrolling waveform with three frequency bands advancing from one edge | M | ✦ | Medium |
| Audio Split Tower | Multiple vertical audio bars (2–5 bands) with smoothing and peak markers | H | — | Light |
| Audio Barcode | Scrolling vertical lines triggered by beats that fade out over time | M | — | Light |
| Audio Gravimeter | Physics-simulated bars that accelerate down and decelerate, with peak flash | M | — | Light |
| Audio Waterfall | Scrolling spectrogram showing frequency history moving across display | M | — | Medium |

### Group 2: Fills / Color / Chases

| Effect | Description | EDM | LedFX | Perf |
|--------|-------------|-----|-------|------|
| Audio Beat Colors | Four colors that pulse and transition across the grid in sync with beats | H | — | Light |
| Audio Energy | Horizontal bars growing from left based on low/mid/high power levels | H | ✦ | Medium |
| Audio Energy 2 | Vertical glowing bands shifting through hues with undulating saturation | M | ✦ | Light |
| Audio Chaser | Colored dots with trailing glows chasing across the grid on the beat | H | — | Light |
| Audio Hue Shift | Color gradients shifting hue based on pitch and energy with wave ripples | M | — | Light |
| Audio Chromatic Keyboard | Chromatic keyboard visualization displaying detected musical notes | L | — | Light |
| Audio Buildup | Layered idle breathing state that builds intensity toward a dramatic drop | H | — | Light |
| Audio Aurora | Drifting sine wave layers blending frequencies into aurora-like patterns | M | — | Light |

### Group 3: Scanners / Beams / Lasers

| Effect | Description | EDM | LedFX | Perf |
|--------|-------------|-----|-------|------|
| Audio Scan | Audio-driven horizontal scanner with optional blur and bounce | H | ✦ | Medium |
| Audio Scan Multi | Three independent low/mid/high frequency scanners moving across strip | H | ✦ | Light |
| Audio Scan and Flare | Audio-driven scanner with white sparkle particles trailing behind | H | ✦ | Light |
| Audio Bass Laser | Bass-triggered laser beams shooting horizontally/vertically with glowing trails | H | — | Light |
| Audio DJ Light | Three bouncing colored blobs driven by low/mid/high frequency bands | M | — | Light |

### Group 4: Particles / Bursts / Impacts

| Effect | Description | EDM | LedFX | Perf |
|--------|-------------|-----|-------|------|
| Audio Fireworks | Particles burst outward from center/bottom with gravity, colored by frequency | H | — | Light |
| Audio Shot | Rapid light spots spawning at random locations and fading with glowing halos | H | — | Light |
| Audio Shockwave | Concentric expanding rings from center with ambient background glow | H | — | Light |
| Audio Fire | Vertical flames rising from bottom with heat diffusion, driven by bass | H | ✦ | Heavy |
| Audio Melt and Sparkle | Flowing colored lava with onset-triggered white sparkle bursts | M | ✦ | Medium |

### Group 5: Organic / Fluid / Atmospheric

| Effect | Description | EDM | LedFX | Perf |
|--------|-------------|-----|-------|------|
| Audio Water | Animated water ripples expanding from audio-driven drop points | M | ✦ | Light |
| Audio Lava Lamp | Flowing lava lamp blobs with swirling organic wave patterns | M | ✦ | Light |
| Audio Plasma | Swirling plasma energy with animated wave interference patterns | M | ✦ | Medium |
| Audio Soap | Smooth flowing soap bubble patterns with organic morphing shapes | M | ✦ | Light |
| Audio Puddles | Expanding concentric ripple rings from audio trigger points | M | — | Light |
| Audio Tunnel | Expanding/contracting concentric rings in tunnel perspective from center | M | — | Light |
| Audio Vortex | Rotating spiral arms radiating outward from center in a vortex pattern | H | — | Light |
| Audio Reaction-Diffusion | Organic evolving reaction-diffusion patterns with emergent formations | L | — | Heavy |

### Group 6: Textures / Fields / Motion

| Effect | Description | EDM | LedFX | Perf |
|--------|-------------|-----|-------|------|
| Audio Blocks | Repeating colored blocks shifting and reflecting with audio-driven hue | M | ✦ | Light |
| Audio Blurz | Blurred bursts from spectral peak frequency with motion blur trails | H | — | Medium |
| Audio Melt | Flowing, melting waveforms with organic sine-wave interference | M | ✦ | Light |
| Audio Crawler | Undulating wave patterns crawling across display with sway and chop | M | ✦ | Light |
| Audio Cellular | Cellular automata patterns scrolling downward with rule-based evolution | L | — | Heavy |
| Audio Flow Field | Particles flowing through animated vector field with trails and decay | M | — | Heavy |

### Group 7: Strobes / Glitch / Aggressive

| Effect | Description | EDM | LedFX | Perf |
|--------|-------------|-----|-------|------|
| Audio Strobe | Strobing flashes triggered by bass and onsets with decaying overlay | H | ✦ | Light |
| Audio Glitch | Digital glitch artifacts with modular interference creating jagged patterns | H | ✦ | Light |
| Audio Glitch 2 | Algorithmic noise bands with saturation modulation, digital complexity | H | ✦ | Light |
| Audio Reactor | Multi-scene display with frequency zones, beat ripples, shimmer, sparkles | H | — | Light |

---

## Genre Guide

### EDM

**Techno**
- High-contrast, minimal palette (white + one accent)
- Audio Strobe, Audio Glitch, Audio Scan for driving energy
- Fast duration (`1/4`), no fade
- Kick and onset triggers for hard-hitting accents

**House**
- Warmer palette (amber, magenta, gold)
- Audio Beat Colors, Audio Chaser, Audio Hue Shift for groove
- Medium duration (`1/2`), short fades
- Beat pulse (`cosPulse`) for smooth pumping

**Drum & Bass**
- Electric palette (cyan, magenta, white)
- Audio Bass Laser, Audio Shockwave, Audio Fireworks for impact
- Fast duration (`1/4`), zero fade
- Bass power triggers for rolling bassline response

**Trance**
- Cool palette (blue, cyan, purple, white)
- Audio Plasma, Audio Wavelength, Audio Aurora for flowing motion
- Slow-to-medium duration (`1`–`2`), long fades
- Gradual buildup effects with Audio Buildup

**Bass / Dubstep**
- Aggressive palette (red, purple, white)
- Audio Fire, Audio Strobe, Audio Reactor for drops
- Fastest duration (`1/8`–`1/4`), no fade
- Downbeat triggers for section-level drops

**Breaks**
- Varied palette (neon green, orange, white)
- Audio Chaser, Audio Shot, Audio Scan Multi
- Mixed durations, syncopated feel
- Onset detection for break-driven accents

**Hardstyle**
- High-energy palette (red, white, black)
- Audio Strobe, Audio Bass Laser, Audio Shockwave
- Very fast (`1/8`), aggressive strobing
- Kick trigger for every hit

**Ambient / Downtempo**
- Muted palette (deep blue, soft purple, warm white)
- Audio Water, Audio Lava Lamp, Audio Reaction-Diffusion
- Very slow (`4`–`8`), long fades
- Volume and spectrum for gentle undulation

**Psytrance**
- Psychedelic palette (UV, neon green, hot pink)
- Audio Vortex, Audio Tunnel, Audio Cellular
- Medium duration (`1/2`), moderate fade
- Beat phase for spiraling motion

### Non-EDM

**Rock / Metal**
- Fire palette (red, orange, yellow, white)
- Audio Fire, Audio Strobe, Audio Energy
- Beat-driven, onset for crashes/fills

**Pop / Dance-pop**
- Bright palette (pink, cyan, yellow, white)
- Audio Beat Colors, Audio Hue Shift, Audio DJ Light
- Medium groove timing, crowd-friendly readability

**Ambient / Installation**
- Soft palette (deep blue, warm amber, soft white)
- Audio Reaction-Diffusion, Audio Plasma, Audio Soap
- Very slow, organic motion, gentle audio coupling

**Theater / Corporate**
- Professional palette (warm white, amber, subtle blue)
- Audio Aurora, Audio Wavelength, Audio Energy 2
- Slow, understated, non-distracting

---

## Color Palettes by Mood

- **Techno / club**
  - Neon: `#ff00aa`, `#00d4ff`, `#ffffff`
  - Electric: `#4400ff`, `#00ffcc`, `#ffffff`
- **Warm / chill**
  - Amber: `#ff6600`, `#ffaa00`, `#ffd080`
  - Sunset: `#ff4400`, `#ff0066`, `#ffcc00`
- **Dark / dramatic**
  - Fire: `#ff0000`, `#ff6600`, `#ffff00`
  - Deep: `#000080`, `#9400d3`, `#220044`
- **UV-heavy**
  - Ultraviolet: `#7700ff`, `#bb00ff`, `#000000`
  - UV + Accent: `#9900ff`, `#ff00ff`, `#220044`
- **Festival pastel**
  - Pastel: `#ff99cc`, `#99ccff`, `#ccff99`
  - Candy: `#ff66b2`, `#66b2ff`, `#ffcc66`
- **Monochrome strobe**
  - White strobe: `#ffffff`, `#000000`
  - Red strobe: `#ff0000`, `#000000`
  - Cyan strobe: `#00ffff`, `#000000`

Use two colors for focused experiments. Add a third color only when the user asks for richness or sparkle.

---

## Speed and Timing Guidelines

Use `duration` and `fadeIn`/`fadeOut` fields (not "hold") when calling `create_rgb_matrices`:

- **Fast energy**: `duration: "1/4"`, `fadeIn: "0"`, `fadeOut: "0"`. Good for techno, DnB, drops.
- **Medium groove**: `duration: "1/2"`, `fadeIn: "1/4"`, `fadeOut: "1/4"`. Good for house, pop.
- **Slow ambient**: `duration: "2"`, `fadeIn: "1"`, `fadeOut: "1"`. Good for chill, intros.

When refining, adjust speed before changing everything else. Users often react to timing before they can judge algorithm quality.

## Control Modes

Use the `controlMode` field in `create_rgb_matrices`. Valid values match the MCP schema:

- `RGB` — standard color output (default)
- `White` — drives only the White channel (grayscale)
- `RGBW` — drives R, G, B, and White with accurate color extraction
- `RGBWBrighter` — drives R, G, B, and White for maximum brightness
- `Amber`, `UV`, `Dimmer`, `Shutter` — specialized modes

Use `RGB` as default. Use `RGBW` or `RGBWBrighter` when fixtures have a White LED channel.

## Algorithm Properties

Many algorithms have tunable properties (speed presets, intensity, orientation, etc.). To discover them:

1. Call `query_rgb_algorithms` to list all algorithms and their properties
2. Each property has a name, type (list/range/integer/float), and default value
3. Pass properties via the `properties` object in `create_rgb_matrices`
4. Vary one property at a time during experiments

## Advanced RGB Matrix Parameters

These are also available in `create_rgb_matrices`:

- `runOrder`: `Loop`, `SingleShot`, `PingPong`, `Random`
- `direction`: `Forward`, `Backward`
- `blendMode`: `Normal`, `Additive`, `Mask`, `Subtractive`
- `rotation`: `0`, `90`, `180`, `270` — rotates the pattern
- `mirror`: `Off`, `Horizontal`, `Vertical`, `Both`
- `mirrorBlend`: `Flip`, `Max`, `Average`, `Additive`

---

## Experiment Templates

### Template 1: EDM Drop

**Goal**: Maximum impact at the bass drop.

| Experiment | Algorithm | Colors | Key settings |
|------------|-----------|--------|--------------|
| EXP-1-A Drop Strobe | Audio Strobe | `#ffffff`, `#000000` | duration `1/8` |
| EXP-1-B Drop Fire | Audio Fire | `#ff0000`, `#ff6600` | duration `1/4` |
| EXP-1-C Drop Shockwave | Audio Shockwave | `#ff00aa`, `#00d4ff` | duration `1/4` |
| EXP-1-D Drop Laser | Audio Bass Laser | `#00ff00`, `#ffffff` | duration `1/4` |

**Mutation axes**: color temperature (warm↔cold), speed (`1/8`↔`1/2`), algorithm intensity properties.

**Expand to 10 by**: combining top 2 algorithms with 5 different color palettes from the palette guide.

### Template 2: Ambient Build

**Goal**: Slow, textured background that breathes with the music.

| Experiment | Algorithm | Colors | Key settings |
|------------|-----------|--------|--------------|
| EXP-1-A Ambient Plasma | Audio Plasma | `#000080`, `#9400d3`, `#220044` | duration `4`, fadeIn `2`, fadeOut `2` |
| EXP-1-B Ambient Water | Audio Water | `#003366`, `#006699`, `#99ccff` | duration `4`, fadeIn `2` |
| EXP-1-C Ambient Lava | Audio Lava Lamp | `#ff6600`, `#ffaa00`, `#ffd080` | duration `4`, fadeIn `2` |
| EXP-1-D Ambient R-D | Audio Reaction-Diffusion | `#4400ff`, `#00ffcc` | duration `8`, fadeIn `4` |

**Mutation axes**: color saturation (vivid↔muted), speed (very slow↔moderate), fade length.

**Expand to 10 by**: trying each winner with UV-heavy vs. warm palettes, and mirror modes.

### Template 3: Beat-Synced Colors

**Goal**: Clear color changes locked to the beat.

| Experiment | Algorithm | Colors | Key settings |
|------------|-----------|--------|--------------|
| EXP-1-A Beat 4-Color | Audio Beat Colors | `#ff0000`, `#00ff00`, `#0000ff`, `#ffffff` | — |
| EXP-1-B Beat Chase | Audio Chaser | `#ff00aa`, `#00d4ff` | duration `1/2` |
| EXP-1-C Beat Hue | Audio Hue Shift | `#ff6600`, `#ffaa00` | duration `1` |

**Mutation axes**: number of colors (2↔5), palette mood, chase direction.

**Expand to 10 by**: varying color count and palette per genre (techno = minimal, pop = rainbow).

### Template 4: Spectrum / Meter Wall

**Goal**: Visualize the music frequency content.

| Experiment | Algorithm | Colors | Key settings |
|------------|-----------|--------|--------------|
| EXP-1-A Spectrum Classic | Audio Spectrum | `#ff0000`, `#ffff00`, `#00ff00`, `#00ffff`, `#0000ff` | — |
| EXP-1-B Equalizer Bars | Audio Equalizer | `#00ff00`, `#ffff00`, `#ff0000` | — |
| EXP-1-C Power Meter | Audio Power | `#ff00aa`, `#00d4ff` | — |
| EXP-1-D Waterfall Scroll | Audio Waterfall | `#000080`, `#00ffcc`, `#ffffff` | — |

**Mutation axes**: color gradient direction, rotation (`0`↔`90`), mirror mode.

**Expand to 10 by**: adding Split Tower and Gravimeter, varying rotation for vertical vs. horizontal layouts.

### Template 5: Club Texture

**Goal**: Background texture that fills visual space without dominating.

| Experiment | Algorithm | Colors | Key settings |
|------------|-----------|--------|--------------|
| EXP-1-A Texture Blocks | Audio Blocks | `#4400ff`, `#00ffcc` | duration `1` |
| EXP-1-B Texture Crawler | Audio Crawler | `#ff00aa`, `#ffffff` | duration `2` |
| EXP-1-C Texture Soap | Audio Soap | `#000080`, `#9400d3` | duration `2`, fadeIn `1` |
| EXP-1-D Texture Melt | Audio Melt | `#ff6600`, `#ffaa00` | duration `2` |

**Mutation axes**: opacity (via dimmer control mode), speed, mirror symmetry.

**Expand to 10 by**: trying each with `mirror: "Both"` + `mirrorBlend: "Average"` for kaleidoscope look.

---

## Audio API Appendix (Script Authors)

Full `audio.*` property tree available to custom JavaScript algorithms via `rgbMap(width, height, rgb, step, audio)`. This is the raw API — agents creating effects through MCP tools do not need this; use `query_rgb_algorithms` instead.

```
audio
├── power
│   ├── low              (0–1) bass energy
│   ├── mid              (0–1) mid energy
│   ├── high             (0–1) treble energy
│   ├── total            (0–1) sum of low+mid+high
│   ├── dominant          ("low"/"mid"/"high") strongest band
│   ├── dominantValue    (0–1) value of dominant band
│   ├── bands[]          (array) N power bands
│   └── detail
│       ├── beat         (0–1) beat-range power
│       └── bass         (0–1) sub-bass power
├── onset
│   ├── fired            (bool) onset detected this frame
│   ├── intensity        (0–1) onset strength
│   ├── method           (string) active onset method name
│   └── methods
│       └── {name}
│           ├── fired    (bool) this method fired
│           └── intensity (0–1)
├── beat
│   ├── fired            (bool) beat detected this frame
│   ├── kick             (bool) kick drum this frame
│   ├── kickHeld         (bool) kick sustained
│   ├── kickIntensity    (0–1) kick trigger value
│   ├── phase            (0–2π) position within beat
│   ├── cosPulse         (0–1) cos(phase) clamped — smooth LFO
│   ├── bpm              (float) detected BPM (0 = no tracking)
│   ├── confidence       (0–1) beat detection confidence
│   └── tatum            (float) sub-beat division
├── bar
│   ├── phase            (0–4) position within 4-beat bar
│   ├── phase01          (0–1) bar phase normalized
│   ├── beat             (0–3) current beat index in bar
│   ├── downbeat         (bool) on beat 1 right now
│   └── downbeatFired    (bool) downbeat transition this frame
├── spectrum
│   ├── low              {values[], raw[], novelty[], mean, max}
│   ├── mid              {values[], raw[], novelty[], mean, max}
│   ├── high             {values[], raw[], novelty[], mean, max}
│   ├── full[]           (array) all mel bins concatenated
│   ├── ranges
│   │   ├── low          {minHz, maxHz, bands}
│   │   ├── mid          {minHz, maxHz, bands}
│   │   └── high         {minHz, maxHz, bands}
│   └── novelty
│       ├── mean         (float) spectral flux average
│       └── max          (float) spectral flux peak
├── volume
│   ├── raw              (float) unprocessed RMS
│   ├── smoothed         (float) smoothed RMS
│   ├── normalized       (0–1) auto-gain normalized volume
│   ├── ledfx            (0–1) LedFX-compatible volume
│   ├── rmsDb            (float) RMS in dB
│   ├── peakDb           (float) peak in dB
│   ├── crestFactor      (float) peak/RMS ratio
│   ├── trigger          {value, active, fired, released, heldMs, cooldownMs}
│   ├── fired            (bool) volume trigger this frame
│   └── held             (bool) volume trigger sustained
├── bands
│   ├── low              {value, active, fired, released, heldMs, cooldownMs}
│   ├── mid              {value, active, fired, released, heldMs, cooldownMs}
│   └── high             {value, active, fired, released, heldMs, cooldownMs}
├── features
│   ├── centroidHz       (float) spectral centroid in Hz
│   ├── spread           (float) spectral spread
│   ├── rolloffHz        (float) spectral rolloff in Hz
│   ├── flux             (float) spectral flux
│   ├── hfc              (float) high-frequency content
│   ├── flatness         (float) spectral flatness
│   └── mfcc[]           (array) MFCC coefficients
├── pitch
│   ├── hz               (float) detected pitch
│   ├── midi             (int) MIDI note number
│   └── confidence       (0–1)
├── note
│   ├── midi             (int) detected note
│   ├── velocity         (0–127) note velocity
│   ├── on               (bool) note-on this frame
│   └── off              (bool) note-off this frame
├── gate
│   ├── closed           (bool) noise gate active
│   └── brightnessFloor  (0–1) minimum brightness when gate closed
├── timing
│   ├── audioDtMs        (float) ms since last audio frame
│   └── consumerDtMs     (float) master timer tick interval
└── colors
    ├── gradient[]       (array) user-set gradient colors (packed RGB)
    └── bands[]          (array) per-power-band colors
```

---

## Experiment Naming

Use this exact pattern:

```text
EXP-{round}-{letter} {description}
```

Examples:

- `EXP-1-A Audio Fire Neon`
- `EXP-1-B Starfield Deep Purple`
- `EXP-2-C Audio Fire Slower Fade`

Why:

- `EXP-` makes temporary work easy to find with `query_functions`
- Round and letter preserve comparison history
- A human-readable description helps the user remember what they saw

## Good Agent Behavior

- Create 3–5 experiments per round for meaningful comparison.
- Keep experiments small and reversible.
- Change only a few parameters between variations.
- Ask the user to preview and compare; do not assume from code alone.
- Clean up rejected experiments with `delete_functions` after the user agrees.
- Save winning recipes to `autolight-presets.json` so future agents can reuse them.
