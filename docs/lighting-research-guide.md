# Lighting Research Guide for QLC+ MCP Agents

This guide is for AI agents such as Copilot CLI or Claude that create and refine LED effects directly through QLC+ MCP tools. The agent is the researcher: ask questions, create small experiments, preview with the user, learn from feedback, and keep only the winners.

## Mental Model: How QLC+ Effects Work

- **RGB Matrix** = algorithm + fixture group + colors + speed/timing + control mode.
  - Best default for LED effect research because one function can explore audio, geometry, particles, waves, and fills.
  - Use `query_rgb_algorithms` before assuming an algorithm exists in the current build.
- **Chaser** = sequence of scenes with hold/fade timing.
  - Best for beat-synced color changes, drops, and predictable phrase movement.
- **Collection** = parallel group of functions.
  - Best for combining multiple layers, for example a matrix texture plus dimmer/strobe accents.
- **Scripts** = custom JavaScript algorithms.
  - Use only when built-in RGB Matrix algorithms cannot express the idea. Keep scripts simple and include waits inside loops.

## MCP Tools for This Workflow

- `query_fixtures` — inspect patched fixtures.
- `query_fixture_groups` — find target fixture groups for RGB matrices.
- `query_rgb_algorithms` — list available RGB Matrix algorithms.
- `create_rgb_matrices` — create most effect experiments.
- `create_scenes` — create static looks or chaser steps.
- `create_chasers` — create beat-timed scene sequences.
- `create_collections` — combine effects in parallel.
- `query_functions` — audit existing functions and find experiments.
- `delete_functions` — remove rejected `EXP-` experiments.

## Research Workflow

1. **Brief the user**
   - Ask for genre, energy, colors, mood, BPM range, and any hard constraints.
   - Ask what “good” means: subtle, aggressive, readable, chaotic, audio-reactive, club-like, etc.
2. **Inspect the rig**
   - Run `query_fixtures` and `query_fixture_groups`.
   - Pick the most relevant fixture group; if unsure, ask the user which group to target.
3. **Discover options**
   - Run `query_rgb_algorithms` and choose available algorithms from different families.
4. **Create 2–3 experiments**
   - Prefer RGB matrices for fast iteration.
   - Name experiments with `EXP-{round}-{letter} {description}`, for example `EXP-1-A Audio Fire Neon`.
5. **Preview each experiment**
   - Ask the user to preview one experiment at a time.
   - Start/stop through available MCP controls when present; otherwise tell the user which function to run in QLC+.
6. **Collect feedback**
   - Ask which experiment they prefer and why.
   - Capture concrete observations: colors, speed, movement, brightness, beat sync, mood fit.
7. **Refine the winner**
   - Mutate one or two variables at a time: algorithm, colors, speed, fade, intensity, control mode.
   - Create another 2–3 `EXP-` variations.
8. **Repeat until satisfied**
   - Stop when the user says a version is good enough or the next changes become tiny.
9. **Save winners**
   - Keep the winning experiments as permanent presets with descriptive names.
   - Delete rejected `EXP-` functions after confirmation.

## The Iterate Pattern

```text
Round 1: Try 3 different algorithm families
  → User picks favorite
Round 2: Try 3 variations of the winner (different colors, speeds, properties)
  → User picks favorite
Round 3: Fine-tune the winner (small parameter tweaks)
  → User confirms "this is good"
Export: Save as a named preset
```

## Algorithm Families

Choose broad families in early rounds, then narrow down.

- **Audio-reactive**: Audio Fire, Audio Plasma, Audio Spectrum, Audio Vortex, Audio Blocks, Audio Water, Audio Aurora, Audio Glitch, Audio Melt, Audio Tunnel, Audio Wavelength, Audio Equalizer, Audio Chaser, Audio Shot, Audio Strobe, Audio Energy.
- **Geometric**: Stripes, Plasma, Circles, Gradient, Marquee, Lines.
- **Fill**: Fill, FillFromCenter, FillUnfill, OneByOne.
- **Random**: RandomSingle, RandomColumn, Noise.
- **Particle**: Balls, Fireworks, Starfield, FlyingObjects, Snowbubbles.
- **Wave**: Sinewave, Waves.

If a named algorithm is missing from `query_rgb_algorithms`, choose the closest available alternative.

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

Use two colors for focused experiments. Add a third color only when the user asks for richness or sparkle.

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
- `blendMode`: how the matrix blends with other running functions
- `rotation`: `0`, `90`, `180`, `270` — rotates the pattern
- `mirror`: `Off`, `Horizontal`, `Vertical`, `Both`
- `mirrorBlend`: `Flip`, `Max`, `Average`, `Additive`

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

- `EXP-` makes temporary work easy to find with `query_functions`.
- Round and letter preserve comparison history.
- A human-readable description helps the user remember what they saw.

## Good Agent Behavior

- Ask before creating large batches; 2–3 experiments per round is usually enough.
- Keep experiments small and reversible.
- Change only a few parameters between variations.
- Ask the user to preview and compare; do not assume from code alone.
- Clean up rejected experiments with `delete_functions` after the user agrees.
- Save winning recipes to `autolight-presets.json` so future agents can reuse them.
