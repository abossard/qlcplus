# HUE audio scripts: fix plan

Branch `fix/hue-audio-scripts` (from `mcp-server` at e1dfd6a59). Findings come from the
2026-10-24 review of `resources/huescripts/` (6 parallel reviewers, verified by node repro or trace).

Status keys: `[ ]` open, `[x]` done, `[~]` in progress, `[-]` dropped (reason given).

## Phases

1. Boundary tests (green): pin current behaviour at the script boundary so refactors are provable.
2. Simplification and consolidation: behaviour-preserving only. Equivalence run must stay exact.
3. Red tests: one parametrized test per bug, at the boundary where the user sees it. Must fail.
4. Bug fixes: make phase 3 green. Equivalence diffs are expected only for scripts touched by a fix.

## Ground rules

- Boundaries: (a) script API under `tests/test_audio_scripts.js` (`setX` setters, `rgbMap`,
  `rgbMapGetColors`), compared in RGB space, the engine's own output boundary;
  (b) engine `HUEMatrix` in `engine/test/huematrix` for bugs only visible with the shared QJSEngine
  and `RGBMatrix::setProperty`.
- Parametrized tests: case tables run through one runner; one failing case does not stop the others.
- Phase 2 must keep `Math.random` call order and count, otherwise the equivalence run diverges.
- No behaviour change in phase 2. If a "simplification" changes output, it moves to phase 4 with a test.
- LedFx parity questions are checked against `/Users/abossard/Desktop/projects/LedFx/ledfx/effects/`
  before changing (marked `LedFx?`).
- Agents work in named worktrees `/Users/abossard/Desktop/projects/qlcplus-worktrees/<branch-with-dashes>`
  on branches `fix/hue-audio-scripts-<group>`. Only the coordinator edits `hsvutil.js`, the shared parts
  of the harness and the engine tests. Groups merge back into `fix/hue-audio-scripts`.

## Decisions

- D1 Frame-rate: per-render constants become time-based, normalized to 50 Hz (MasterTimer tick 20 ms),
  so the look at 50 Hz is unchanged. Scripts already using `pow(k, dt*60)` keep 60.
- D2 Setter input: one helper `HSVUtil.parseNumber(value, previous, min, max)`. Non-finite input keeps the
  previous value; finite input is clamped to script-chosen bounds.
- D3 Hue mixing: blurs and blends use a circular hue mean weighted by brightness; black pixels carry no hue.
- D4 `rgbMapGetColors`: HSV scripts return `[]` (as the other ~55 do). The engine path stays upstream.
- D5 Dead harness-only setters (not in `algo.properties`) are removed; harness calls move to declared setters.

## Phase 0

- [x] Remove the simulated-audio preview (reverted, never committed).

## Phase 1: boundary tests

- [x] P1.1 `loadScript` loads `hsvutil.js` from the same directory as the script (baseline trees bring their own).
- [x] P1.2 Equivalence mode `--baseline-ref <git-ref>`: exports `resources/huescripts` of the ref, renders both
  trees over the same deterministic matrix, compares RGB per pixel within 1e-5.
  Matrix: every audio script x (defaults, each list-property value, one numeric-nudge config)
  x sizes 1x1, 12x1, 1x7, 7x5 x one stimulus sequence (quiet, sustain, transients, event deltas,
  tempo valid/invalid, unavailable frame, dt 0/0.02/0.04/0.5, resize, source change). `--only a.js,b.js` filter.
  Run: `node tests/test_audio_scripts.js --baseline-ref <ref> [--only a.js,b.js]` (63 scripts, ~16 s).
  Engine-shaped input comes from `engineAudio()`; reuse it for phase 3 tests.
- [x] P1.3 Case runner `runCases(title, cases, fn)` (per-case FAIL lines, sets exit code, continues).
- [x] P1.4 Commit; record the commit as the phase 2 baseline ref here: `BASELINE=90447f48c`

## Phase 2: simplification and consolidation

Central (coordinator, `hsvutil.js`), each verified by equivalence. Helpers added (adoption is per group):
`lerpHue`, `hueMean`, `addPixel`, `hexToHsv`, `dominantBand`, `smoothFollow(current, target, smoothing, frames?)`,
`barEnvelope(state{bar,flash}, energy, dt, downbeat, flashDecay)`, `ledFxGradientStops(algo)`,
`referenceOutput(algo, pixels, width, height)`. Tests: `assertSharedEffectBuildingBlocks`.
- [x] H1 `ledFxGradientStops` (crawler, water, fire, lava, blocks, glitch, melt, meltsparkle, ...): one helper.
- [x] H2 `referenceOutput` (barcode, energy, energy2, spectrum, strobe): one helper.
- [x] H3 Smoothing rise/decay alpha from `presetSmoothing` (~17 copies): one helper, per-frame for now.
- [x] H4 Bar build-up/release envelope (tunnel, vortex, chaser, energy, scan, scanflare, scanmulti): one helper
  if the copies are identical, otherwise per group.
- [x] H5 Additive brightness blend with hue takeover (~6 copies) and weighted circular hue mean
  (aurora, blurz, chaser): one helper each.
- [x] H6 Dominant band (argmax low/mid/high) (~6 copies): one helper.
- [x] H7 Hex to HSV (vumeter, strobe, equalizer2d, hierarchy): one helper.
- [x] H8 Remove unused exports `beatTime`, `beatAngle`, `beatPosition`, `noiseField2d`.
- [x] H9 `hsvutil.js` local cleanups: dead `stops.length <= 1` (142), repeated indexing/min guards (157, 165-167).

Per group (agents), each verified by equivalence for the group's scripts:
- [x] G-A aurora, bands, bandsmatrix, barcode, basslaser, beatcolors, bladepower, bleep, blocks, blurz
- [x] G-B bpmbar, buildup, cellular, chaser, concentric, crawler, digitalrain, djlight, energy, energy2, equalizer, filter
- [x] G-C equalizer2d, fire, fireworks, flame, flowfield, gameoflife, glitch, glitch2, gravimeter, hierarchy, hueshift, lava, magnitude
- [x] G-D marching, melt, meltsparkle, multicolorbar, noise, pitchspectrum, plasma, power, puddles, reaction, reactor
- [x] G-E scan, scanflare, scanmulti, shockwave, shot, smoke, soap, spectralblocks
- [x] G-F spectrum, splittower, spotlight, strobe, tunnel, vortex, vumeter, water, waterfall

Per-group work list: adopt H1-H7 and existing helpers (`audioSeconds`, `powerForRange`, `sin01`, `time01`,
`mod1`, `triangle`, `rgbToHsv`, `gradientAt`, `setPixel`, `clamp01`); inline shallow single-use functions;
delete dead code (palette fallbacks the engine never reaches, legacy `version >= 6` / missing-`events`
branches, `frameSequence` dedupe, unused vars, harness-only setters per D5, `peakEnergy == barEnergy`,
glitch2 T1 == T2); see the group sections below for file:line items.

## Phase 3: red tests

Cross-cutting (coordinator), parametrized over all audio scripts. Written while phase 2 runs, committed after
the phase 2 merge. JS: `assertCrossCuttingContracts` (`--contracts-only`, ~60 s). Engine:
`audioScriptPropertyWriteKeepsThePalette`, `audioScriptInstancesKeepTheirOwnProperties`.
Red at 8a1b0bdbe: X1 8 scripts (JS and engine), X2 buildup (JS and engine), X3 119/393 cases,
X4 23/63 (measures decay after the music stops, not motion speed), X5 blurz/scan/tunnel/vortex,
X6 vumeter, water x2, spotlight, power, equalizer2d ring x2. Not reproduced by the generic setup, so left to
group tests: X5 power/flame/strobe, X6 reactor Nx1 (D4), splittower narrow (F3, masked by F1).
- [~] X1 `rgbMapGetColors` returns only finite numbers (JS), and `HUEMatrix::setProperty` keeps the palette
  for every audio script (engine). Red: reaction, blurz, chaser, energy, djlight, gravimeter, scan, scanflare.
- [~] X2 Instance isolation: two instances in one shared context (JS) and two `HUEMatrix` with the same
  script (engine) do not affect each other. Red: buildup (no IIFE), energy2 (`referenceState` global).
- [~] X3 Invalid setter input ("", "abc", "Infinity", "-1e9", "1e9") on every float/range property:
  no throw, finite output, bounded time; restoring the default gives the output of a fresh instance.
- [~] X4 Frame-rate: same stimulus at 25/50/100 Hz gives the same state after 1 s (table of scripts and measures).
- [~] X5 Hue mixing: red palette (h 0.95/0.05) stays red; a blue burst keeps its hue in blur halos.
  Red: blurz, scan, power, flame, tunnel, vortex, strobe tint.
- [~] X6 Degenerate geometry under loud input (1xN, Nx1, narrow): lit and not uniform where the effect has a spatial axis.

- [x] H1 `mod1(-1e-20)` returns 1; H2 `gradientAt(NaN)` throws; H3 `walkLine` loops forever on a NaN/Infinity endpoint (found by G-C) (`assertSharedHelperBugs`, red, 69c85d855).

Per group (agents): one parametrized test per bug in the group sections below, inside `assertGroupABugs`..
`assertGroupFBugs` (stubs committed in 9531f83b5 so parallel agents never touch the same lines).

## Phase 4: bug fixes

- [x] Central: `parseNumber(value, previous, min, max)` (D2), `elapsedFrames(audio)` for D1 (pass to
  smoothFollow/attackRelease frames, or Math.pow(k, frames)), `blurStrip(strip, radius)` RGB mixing (D3),
  `gradientAt` NaN-safe (H2), `mod1` never returns 1 (H1), `noiseFieldRenderer` impulse time-based
  (D9 noise, E14 smoke; harness noise oracle updated).
- [x] G-A  - [x] G-B  - [x] G-C  - [x] G-D  - [x] G-E  - [ ] G-F
- [ ] Engine tests green, harness green, equivalence diffs limited to fixed scripts.
- [ ] Docs: update only where documented behaviour changed (`docs/audio-data-usage.md`, `docs/hue-effects/README.md`).

## Open, not confirmed

- [-] O1 `HUEScript::rgbMapSetColors` queues a lambda holding raw `this` (huescript.cpp:268); `~RGBMatrix`
  deletes the algorithm inline (rgbmatrix.cpp:107) and huematrix.cpp:1171 claims it is deferred.
  A test harness segfaulted at teardown after 64 create/destroy cycles. Not reproduced under AddressSanitizer
  (Doc-owned and stack-owned matrices, scripts at 90447f48c and df4f8e221, 200 previews per pattern): not fixed.
  The huematrix.cpp comment claiming ~RGBMatrix uses deferDeleteAlgorithm is still wrong.

## Group bug lists (phase 3 test, then phase 4 fix)

Group A
- [ ] A1 blurz.js:119-161 box blur averages raw HSV (X5).
- [ ] A2 bleep.js:238 non-mirror Fill paints above the trace; silence lights the whole map. Harness encodes it.
- [ ] A3 bandsmatrix.js:141 Matrix layout runs strip transforms on a 2D map (mirror/flip/blur scramble rows).
- [ ] A4 beatcolors.js:135,172 tests `bpm > 0` (always true, fallback 120) instead of `tempo.valid`.
- [ ] A5 basslaser.js:206-219 beam motion, life and EMA per render (X4).
- [ ] A6 basslaser.js:77-82 palette taken from `rgbMapSetColors`; default palette gives a black trail.
- [ ] A7 basslaser.js:212 `beatFired || beatFired`. LedFx? intended second trigger.
- [ ] A8 bands.js:68 invert align `floor(vol/2)` vs LedFx `ceil`. LedFx?
- [ ] A9 bleep.js:111-117 history survives a source change.
- [ ] A10 NaN setters: aurora 43-57, blocks 47-50, blurz 30,37 (fractional radius, decay 0), beatcolors 76-81,
  barcode 318-381 (X3).
- [ ] A11 blurz.js:219-225 peak slot 2/5 unreachable (low never beats max(beat,bass)).
- [ ] A12 aurora 105-113, blocks 140, blurz 199-213 per-render constants (X4).

Group B
- [ ] B1 buildup.js:14 no IIFE, all state global (X2).
- [ ] B2 buildup.js:152,177,181,197 `% 1.0` makes DROP/COOLDOWN never end.
- [ ] B3 chaser 125, djlight 77, energy 314 `rgbMapGetColors` objects (X1).
- [ ] B4 energy2.js:30,191 `referenceState` implicit global (X2).
- [ ] B5 chaser.js:173 "Bass" driver reads mid, "Volume" reads low.
- [ ] B6 cellular.js:61,153 `setScrollHz` unbounded: "Infinity" hangs the JS thread, "x" stops scrolling (X3).
- [ ] B7 NaN setters: chaser 47,68, djlight 36,43,57, energy 289, energy2 221,230, equalizer 60,66,70,
  buildup 44,46, crawler 43-52 (X3).
- [ ] B8 per-render: djlight 101-108, chaser 145-166, equalizer 198-229, crawler 143, digitalrain 96-99 (X4).
- [ ] B9 energy.js:354,373 `min(1, beatBoost + ...)` always 1; beat pulse has no effect.
- [ ] B10 energy2.js:213,254,276 "Speed (cyc/beat)" is per second, not tempo-locked.
- [ ] B11 djlight.js:91-93 reflection handles one overshoot; speed > 50 diverges.
- [ ] B12 buildup.js:211-213 edge-detects `beatFired`, ignores `events.delta.beat`.

Group C
- [ ] C1 gravimeter.js:84-87 `rgbMapGetColors` objects (X1).
- [ ] C2 equalizer2d.js:308-313 Center=On fills -peak..+peak; LedFx draws two markers. LedFx?
- [ ] C3 gameoflife.js:157 Health "All" resets only on a full board; LedFx means Dead + Oscillating. LedFx?
- [ ] C4 glitch.js:93 triangle inverted vs LedFx. LedFx?
- [ ] C5 flame.js:141-147 fractional births floored; nothing spawns below power x intensity 0.75.
- [ ] C6 flame.js:170-173 forcing saturation on white gives red.
- [ ] C7 fire.js:43,45,79 `setIntensity`/`setColorShift` NaN or negative: RangeError every render (X3).
- [ ] C8 flame.js:188-193 top trip dims h and s, not only v.
- [ ] C9 flame.js:92-117 `blurMap` linear H/S including black (X5).
- [ ] C10 equalizer2d.js:137-138 peak decays below the current level for a frame.
- [ ] C11 equalizer2d.js:181,192-207 Ring mode with 1-2 bands draws nothing.
- [ ] C12 hueshift.js:120 wave time uses `step`, which is always 0; never animates.
- [ ] C13 flowfield.js:197 "Trail Half-life" uses exp(-dt/T).
- [ ] C14 flowfield.js:160-161,263 `Math.round` positions: out of bounds at the right edge, half coverage at column 0.
- [ ] C15 fire.js:166-171 excess accumulator kept after `MAX_STEPS_PER_FRAME`: fast-forward after a stall.
- [ ] C16 hierarchy.js:96 `midBelowSec` reset on lows; LedFx keeps `last_mid`. LedFx?
- [ ] C17 NaN latch: glitch 48, glitch2 43, hueshift 52,65, flowfield 50, lava 43,118-119 (reactivity > 1) (X3).
- [ ] C18 per-render: equalizer2d 144, fire 126,130,191, glitch 201,256, glitch2 83, gameoflife 178,
  flowfield 190-191, hueshift 102,110, lava 123-124 (X4).

Group D
- [ ] D1 reaction.js:161 `rgbMapGetColors` objects (X1).
- [ ] D2 NaN setters: plasma 70-79,100, reaction 50-120, meltsparkle 91-117 (fractional strobeBlur),
  power 50-54,66, reactor 50-61, melt 37,39, puddles 47-89 (X3).
- [ ] D3 reactor.js:141-148 bar energy grows without downbeats and survives audio loss.
- [ ] D4 reactor.js:196-207 Nx1 strip renders black (X6).
- [ ] D5 power.js:249-325 Artistic ignores strip transforms, `frequency_range` and `sparks_color`.
- [ ] D6 power.js:256 `numSparks` has no minimum; no sparks under 20 px (X6).
- [ ] D7 power.js:127-141 `boxBlur` linear hue (X5).
- [ ] D8 puddles.js:265 Rain Pulse decay drops the `round(dt*50)` remainder; no decay at 120 Hz.
- [ ] D9 per-render: marching 58 (also at dt 0), noise 76, plasma 271, power 251-294, reactor 133-164, puddles 261 (X4).
- [ ] D10 reaction.js:182 PDE step count uncapped after a stall.
- [ ] D11 puddles.js:291-296 ripples kept on resize.
- [ ] D12 reactor.js:98-104 "Pitch Hue" never reads `audio.pitch`.

Group E
- [ ] E1 scan 170, scanflare 186-188 `rgbMapGetColors` objects (X1).
- [ ] E2 scanflare.js:264-266, scanmulti.js:362-365 non-bounce wrap never drawn.
- [ ] E3 scanflare.js:34,249-251, scanmulti.js:34,285-287 LedFx mode speed default 0.5 % per second; almost still.
- [ ] E4 shot.js:101,104 Trigger ignored; every mode fires on onset, Mids and Highs identical.
- [ ] E5 scan.js:151-166 `boxBlur` linear hue (X5).
- [-] E6 smoke.js:95-106 (and noise) zoom axes swapped. Dropped: LedFx noise2d.py/smoke2d.py do the same.
- [ ] E7 shockwave.js:117 dominant tint needs 3 colors, `acceptColors` is 2.
- [ ] E8 shockwave.js:77-82 clearing the palette never restores defaults.
- [ ] E9 shockwave.js:224,227 decay x waveDecay >= 1 gives NaN intensity.
- [ ] E10 shockwave.js:180 fill waves spawn per render (X4).
- [ ] E11 scanflare.js:285 Artistic sparkle spawn wraps to the far end.
- [ ] E12 scan.js:123-125 default gradient unreachable; should depend on `hasUserColors`.
- [ ] E13 NaN setters: scanflare 92, scanmulti 88, soap 65, scan 68, shockwave 99 (X3).
- [ ] E14 per-render: scan 208-224, scanflare 227-243, scanmulti 311-344, smoke 88-89, soap 138, soap smear (X4).

Group F
- [ ] F1 splittower.js:105-131 zero bar draws the peak at full v on the bottom row.
- [ ] F2 splittower.js:95-111 fall, hold and peak step per call (X4).
- [ ] F3 splittower.js:115-116 low bands get zero width when width < bands (X6).
- [ ] F4 splittower.js:41,45 `setBands("abc")` throws, `setDecay("abc")` latches NaN (X3).
- [ ] F5 tunnel.js:130-150 Diamond `normDist > 1`: HSV out of [0,1] at corners.
- [ ] F6 tunnel 148, vortex 137 linear hue lerp; strobe.js:466-470 linear tint (X5).
- [ ] F7 tunnel 88,103, vortex 87,99 per-call EMA and flash decay (X4).
- [ ] F8 tunnel 43,55,61, vortex 43-57 NaN setters (X3).
- [ ] F9 spectrum.js:239 Artistic `rgbMixes[presetRgbMix]` unguarded; `setRgbMix("2.5")` throws.
- [ ] F10 spectrum.js:168-173 Decay control never read.
- [ ] F11 strobe.js:259,386 `setColorStep("abc")` throws (X3).
- [ ] F12 vumeter.js:104-124 1xN always dark (X6).
- [-] F13 vumeter.js:107 peak marker 0 px under width 100 at the default 1%. Dropped: LedFx vumeter.py does the same.
- [ ] F14 vumeter.js:98-100 per-call alphas (X4).
- [ ] F15 water.js:155,168 Artistic black when width < 5 (X6).
- [ ] F16 water.js:212-214 ripple steps per call (X4).
- [ ] F17 waterfall.js:72-76,107-111 bands capped by width*height; columns overwrite each other.
- [ ] F18 waterfall.js:183-184 Center fade wrong for even heights.
- [ ] F19 spotlight.js:187 position uses x only; 1xN uniform (X6).
- [ ] F20 spotlight.js:140-141 activity EMA per call (X4).

## Log

- 2026-10-24 Plan written. Preview removed.
- 2026-10-24 Phase 1 committed (90447f48c). Main checkout is shared with another session: all work happens in
  `/Users/abossard/Desktop/projects/qlcplus-worktrees/fix-hue-audio-scripts`.
- 2026-10-24 Phase 2 central helpers committed; copies verified identical (ledFxGradientStops x8 by md5,
  referenceOutput x5 whitespace-only, smoothing x18 same formula, bar envelope x7, dominant band ties equal).
- 2026-10-24 G-C phase 2 done (a1e3aab8a, 13/13 equivalent). Skipped: fireworks randomWeightedBand (consumes
  Math.random), eq2d peak/bar loop merge (changes overwrite order), gameoflife frameSequence dedupe (harness
  C22 needs it), glitch triangle and hueshift lerpHue and lava time01 (not equivalent). Proposed helpers:
  attackRelease(current, target, rise, fall); LedFx saturation color; LedFx-mode reset key; Bresenham walker.
- 2026-10-24 G-D phase 2 done (36053892c, 11/11 equivalent, -308 lines). Skipped: multicolorbar beatFired
  fallback and puddles events-only (legacy v5 harness frames lack events), plasma powerFor (harness sets low
  without beat/bass), reaction audioSeconds (1 ulp flips Math.round). Proposed helpers: strip box blur,
  upper-third bank peak, one-pole follower 1-pow(0.9, dtSec*50).
- 2026-10-24 G-E phase 2 done (10c2e25f4, 8/8 equivalent, -230 lines). Skipped: shot audioSeconds (no-timing
  frames step 1), soap powerForRange (engine lows is min(1,(p0+p1)/2), not a clamped mean), scanmulti wrap.
  Proposed helpers: blendPixel (arc-weighted additive blend, scanmulti/shockwave/shot), wrap(x, n),
  RGB strip to HSV map. smoke and noise still near-identical.
- 2026-10-24 G-A phase 2 done (1559090a8, 10/10 equivalent, -326 lines; own 780k-render check max diff 0).
  Skipped: barcode attachStripTransformControls (descriptors differ), beatcolors mixHsv->gradientAt (gradientAt
  throws on NaN t), beatcolors 142 min (output changes for sweep width > 3.27: phase 4), blurz DEFAULT_BANDS
  (live: rgbMapGetColors may run before colors are injected), barcode legacy events branch.
  Proposed helpers: bandPeaks(r, count), reference strip-options reader.
- Note: editing scripts makes docs/hue-effects/media/generation.json source hashes stale (gallery tool only).
- 2026-10-24 G-F phase 2 done (e26a81a7a, 9/9 equivalent, -276 lines). Skipped: spectrum/strobe reference
  controls via attachStripTransformControls (descriptors differ), strobe artistic version>=6 branches (v5
  harness frames), spotlight stops[0] per frame (spots keep spawn-time color). Proposed: eventCount helper.
- 2026-10-24 Phase 2 groups merged (ad10b0993): equivalence 63/63 exact vs 90447f48c, harness 63/63, engine
  tests green apart from the new red ones. Scripts: 1137+/2741-. Cross-cutting red tests committed next.
- 2026-10-24 Red tests committed (f6497ece6, 69c85d855). Running in parallel: phase 2 round 2 (cross-file
  helpers proposed by the group agents, fix/hue-audio-scripts-shared-helpers) and group red tests
  (fix/hue-audio-scripts-red-a..f).
- 2026-10-24 G-F red tests (a9cb51fcc): F1, F3, F5, F6, F10, F16, F17, F18 red; F13 dropped (LedFx parity).
  F17/F18: LedFx waterfall2d.py has the same code; fix anyway unless the script has a mode promising LedFx
  parity (then keep it there). F16 fix must time-scale Artistic emitter drift too and keep harness row61
  (one ripple iteration at 1/60 s for speed 1.2 and 1.8).
- 2026-10-24 G-B red tests (4d1e98be5): B2, B5, B9, B10 (Artistic only; LedFx mode stays seconds-based like
  LedFx time()), B11, B12, B8 crawler Artistic + digitalrain motion. Assumption to confirm in the fix:
  chaser "Volume" follows volume.normalized.
- 2026-10-24 G-E red tests (557890929): E2 (Artistic), E3, E4, E7, E8, E9, E11, E12, E14 smoke. E6 dropped
  (LedFx parity). Soap rate dependence real but no stable frame-level assertion; left as is.
  Harness assertions the fixes must update: smoke oracle `expectedNoiseLikeMap` per-render alpha (C54, plain
  assert, aborts the run; shared with noise); EVENT_STIMULUS['audioshot.js'] = 'onset' with default Trigger
  Beat; shot decay check spawns with an onset under Trigger Beat (would pass vacuously).
- 2026-10-24 G-A red tests (219e2c48e): A2, A3, A4, A5, A6, A7 (second trigger = kickFired, per the pre-45f16b507
  `audio.beat.kick`), A8 (LedFx np.roll parity), A9, A11 (keeps the 5-slot layout). Aurora/blocks/blurz rate
  dependence already red in X4. Harness assertion the A2 fix must update: line ~3000 'row05 fill: must fill
  toward row 0' (plain assert, aborts the run).
- 2026-10-24 G-D red tests (6dffa286e): D3, D4, D5, D7, D8, D9 (marching, noise, plasma, puddles baseline),
  D10, D11 (new ripples get the index-8 color and old rings cross the grid), D12. Harness assertions the fixes
  must update: noise/smoke oracle `expectedNoiseLikeMap` per-render alpha (C36, plain assert); marching
  row31 tolerance 1e-3 (0.0016 with a time-based alpha). Power blur through attachStripTransformControls
  changes the X3 failure text; X4 reactor stays red after D3 alone.
- 2026-10-24 G-C red tests (878bf14ac): C2, C3, C4, C5, C6, C8, C9, C10, C12, C13, C14, C15, C16, C18 (glitch,
  glitch2, hueshift, eq2d spin). Harness assertions the fixes must update: C22/life/health-all (encodes the
  wrong "All"); X5 flame case is invalid (flame has acceptColors 0 and its own orange Mid color): remove it at
  merge, C9 covers flame blur.
- 2026-10-24 Group red tests merged (ffce2aa00). Default run: 62 red tables (X1-X6, A2..F18, H1-H2), RESULTS
  63 passed. X5 now covers blurz, scan, tunnel, vortex; flame/power/strobe hue are C9, D7, F6.
- 2026-10-24 Phase 2 round 2 merged (17770400e): 11 more helpers (wrap, audioStateChanged x23, eventCount,
  blendPixel, walkLine, rangeSlice, bandPeaks, attackRelease(frames?), ledFxColor, writeRgbPixels,
  noiseFieldRenderer for noise+smoke). Equivalence 63/63, red set unchanged.
- 2026-10-24 Phase 4 central fixes: H1, H2, E14 and D9 noise green; X3 274 -> 304 of 393.
- 2026-10-24 G-B fixed (68de1102b): B2, B5, B8, B9, B10, B11, B12, X1-X4 for its scripts. No harness assertion
  changed. Also clamped chaser first-dot v (could reach 1.04). Visible changes: Buildup cycles drop 2 beats +
  cooldown 2 beats; Chaser Bass/Volume drivers; Energy Artistic rests at 80% and peaks on the beat; Energy 2
  Artistic speed is cycles per beat with tempo (2x faster at 120 BPM); DJ Light blobs stay on the strip;
  setter input clamped (crawler to LedFx maxima).
- 2026-10-24 G-D fixed (a6e678425): D3-D12, X1-X6 for its scripts. Harness change: marching row31 tolerance
  1e-3 -> 2e-3. Visible changes: Power Artistic applies Flip/Mirror/Background/Brightness/Frequency Range,
  sparks use Spark Color (default white), >=1 spark under 20 px, RGB blur; Reactor build-up drains ~1 bar
  after the last downbeat, resets on source/resize, Pitch Hue follows the note, Nx1 shows the low scene;
  Puddles Rain Pulse rate-independent, resize clears ripples; Reaction catches up at most 0.25 s; integer
  setters round instead of truncating.
- 2026-10-24 G-C fixed (e3ae11546): C2-C18, X1/X3/X4/X6 for its scripts (LedFx parity for C2, C3, C4, C16).
  Harness changes: C22/life/health-all now seeds a dead board; C22/life/impulse-decay drop render 0.21 -> 0.25 s.
  Visible: eq2d two center peak lines, peaks never sink into the bar, ring with 1-2 bands; flame lights at low
  power, white stays white, blur/trip keep hue; flowfield true half-life (trails last longer), even edges;
  gameoflife "All" restarts dead/frozen boards; LedFx Glitch colors match LedFx; hierarchy one switch time;
  hueshift wave moves; gravimeter holds the peak; fire no fast-forward after a pause.
- 2026-10-24 H3 walkLine guard: red test, then `isFinite` check (central).
