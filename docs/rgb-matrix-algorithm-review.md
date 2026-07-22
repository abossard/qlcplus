# RGB Matrix algorithm technical review

## Scope and evidence labels

This review covers the RGB Matrix scripts installed by
`resources/rgbscripts/CMakeLists.txt:3-84`. Final validation uses QLC+ revision
`8c3285a795c4bd2b7460af435bae00961e864824`. The review began at
`2f02f005401fb78da7612e8aaf38591019384e62` with unrelated
`qmlui/contextmanager.cpp`/`.h` changes; an external worktree/HEAD switch
removed that baseline during execution. This phase never edited those paths.

Findings use these labels:

- **Proven defect:** a violated local invariant with a reproducible boundary
  failure and grounded expected result.
- **Validated design divergence:** a measured difference from the pinned LedFX
  implementation that does not violate a local invariant.
- **Quality risk / coverage gap:** a mechanically observed limitation without
  enough evidence to call the intended visual result wrong.
- **Subjective manual check:** visual or musical judgment reserved for
  `MANUAL_REVIEW.md`.
- **Open question:** intent or runtime behavior not established by the available
  sources.

## Executive assessment

- The installed boundary is mechanically solid after four proof-gated repairs:
  all 41 audio scripts pass exact HSV type/length/bounds checks across the
  required dimensions, resize, varied audio/tempo/events, declared properties,
  causal response, and isolated state.
- The four repaired defects were not matters of taste: Fireworks used stale
  nested events and declared an absent Note event; Reactor emitted values above
  one; Spectrum produced non-finite HSV at `1×1`; Water emitted negative hue.
- Music adaptation is uneven but explicit. Every script has at least one
  mechanically demonstrated causal hook. Water's high-band hook is reliable at
  small widths; isolated low becomes visible at width 12 and isolated mid only
  at the wider width 100 in the tested spatial mapping. Continuous low-driven
  ports such as Crawler, Lava, and Melt intentionally lack discrete hits;
  event-driven effects expose fewer continuous band controls.
- Sixteen declared LedFX ports retain a recognizable upstream core with
  QLC+-specific timing/output/topology adaptations. Blocks and Strobe are
  structurally divergent from their pinned namesakes. Difference alone is not
  a defect.
- The strongest report-only issues are the unused Spectrum `decay` control, the
  Blocks identity mismatch, Strobe's different musical model, and frame-stepped
  effects whose timing feel requires playback review.
- “Purposeful and strong” is not converted into a numeric score. The automated
  evidence establishes stability, contrast where promised, and causality; the
  focused checks in `MANUAL_REVIEW.md` decide visual and musical quality.

## Runtime contract

The engine loads only `hsvutil.js` into the shared JavaScript scope
(`engine/src/rgbscriptv4.cpp:281-300`). Before each call it injects the palette
as HSV objects, passes the primary HSV color positionally, and supplies the
audio object as the fifth argument (`engine/src/rgbscriptv4.cpp:538-573`). The
audio object has 15 flat fields; band/event values and beat-relative `dt` are
constructed at `engine/src/rgbscriptv4.cpp:849-896`.

The documented producer boundary requires a `Float32Array` of exactly
`width*height*3` interleaved HSV floats in `[0,1]`
(`engine/src/rgbscriptv4.cpp:580-589`;
`resources/rgbscripts/hsvutil.js:5-11`). The engine validates type and length,
then `hsvToRgb()` normalizes non-finite/hue inputs and clamps saturation/value
before conversion (`engine/src/rgbscriptv4.cpp:73-81,649-660`). The Node
harness enforces the documented pre-conversion array contract; it does not
claim that downstream conversion lacks clamping, and it does not emulate
fixture output or DMX. Audio algorithms are recomputed every tick, then
rotation/mirror and beat transforms are applied before channel output
(`engine/src/rgbmatrix.cpp:926-984`).

## Installed-library inventory

The installation registry contains 80 JavaScript resources: 79 algorithms and
one shared helper. A filesystem-to-registry reconciliation finds one additional
file, the uninstalled development template `empty.js`
(`resources/rgbscripts/empty.js:20-67`). The helper defines the flat interleaved
HSV `Float32Array` contract and shared numerical/time/noise operations
(`resources/rgbscripts/hsvutil.js:23-30,51-112,124-167,181-239`).

### Installed audio-reactive algorithms (41)

`audioaurora.js`, `audiobarcode.js`, `audiobasslaser.js`,
`audiobeatcolors.js`, `audioblocks.js`, `audioblurz.js`, `audiobuildup.js`,
`audiocellular.js`, `audiochaser.js`, `audiocrawler.js`, `audiodjlight.js`,
`audioenergy.js`, `audioenergy2.js`, `audioequalizer.js`, `audiofire.js`,
`audiofireworks.js`, `audioflowfield.js`, `audioglitch.js`, `audioglitch2.js`,
`audiogravimeter.js`, `audiohueshift.js`, `audiolava.js`, `audiomelt.js`,
`audiomeltsparkle.js`, `audioplasma.js`, `audiopower.js`, `audiopuddles.js`,
`audioreaction.js`, `audioreactor.js`, `audioscan.js`, `audioscanflare.js`,
`audioscanmulti.js`, `audioshockwave.js`, `audioshot.js`, `audiosoap.js`,
`audiospectrum.js`, `audiosplittower.js`, `audiostrobe.js`, `audiotunnel.js`,
`audiovortex.js`, and `audiowater.js`.

Metadata in 18 of these files declares `Ported from LedFx`:
`audioblocks.js`, `audiocrawler.js`, `audioenergy.js`, `audioenergy2.js`,
`audiofire.js`, `audioglitch.js`, `audioglitch2.js`, `audiolava.js`,
`audiomelt.js`, `audiomeltsparkle.js`, `audioplasma.js`, `audioscan.js`,
`audioscanflare.js`, `audioscanmulti.js`, `audiosoap.js`, `audiospectrum.js`,
`audiostrobe.js`, and `audiowater.js`.

### Installed non-audio algorithms (38)

`alternate.js`, `balls.js`, `blinder.js`, `circles.js`, `circular.js`,
`evenodd.js`, `fill.js`, `fillfromcenter.js`, `fillunfill.js`,
`fillunfillfromcenter.js`, `fillunfillsquaresfromcenter.js`, `fireworks.js`,
`flyingobjects.js`, `gradient.js`, `lines.js`, `marquee.js`, `noise.js`,
`onebyone.js`, `opposite.js`, `plasma.js`, `randomcolumn.js`,
`randomfillcolumn.js`, `randomfillrow.js`, `randomfillsingle.js`,
`randompixelperrow.js`, `randompixelperrowmulticolor.js`, `randomrow.js`,
`randomsingle.js`, `sinewave.js`, `snowbubbles.js`, `squares.js`,
`squaresfromcenter.js`, `starfield.js`, `stripes.js`,
`stripesfromcenter.js`, `strobe.js`, `verticalfall.js`, and `waves.js`.

These 38 are inventoried and contract-scoped; they are not represented as
audio-reactive or as LedFX comparisons.

## Proof-gated production repairs

The boundary invariant for every repair below is the installed script contract:
`rgbMap()` returns exactly `width * height * 3` finite HSV floats in `[0,1]`
(`resources/rgbscripts/hsvutil.js:5-11`;
`engine/src/rgbscriptv4.cpp:580-629`). The Node regression invokes public
`rgbMap()`, not an internal helper.

### Audio Fireworks flat-event contract

1. **Exact invariant:** every declared trigger-list option must execute against
   the flat audio object, and a Beat or Onset event must reach its named trigger.
2. **Trigger:** select each declared `triggerMode`, then call `rgbMap(7, 11, …)`
   with only the corresponding flat `beatFired` or `onset` event.
3. **Pre-repair wrong output:** `Beat` and `Onset` inspect nonexistent nested
   `.fired` members and never reach the named flat events. `Note` remains
   declared despite absent `audio.note`, so it also cannot spawn. The exact
   `git show HEAD:` reproduction returned
   `values=Beat,Onset,Note beatChanged=false onsetChanged=false` against
   preimage
   `8c3285a795c4bd2b7460af435bae00961e864824:resources/rgbscripts/audiofireworks.js:42-45,71-78,242-264`
4. **Grounded expected output:** use `audio.beatFired` and `audio.onset`, the
   events injected by `engine/src/rgbscriptv4.cpp:886-889`. Remove the
   unsupported Note option because the complete injected object at
   `engine/src/rgbscriptv4.cpp:881-896` has no note event. The final metadata,
   setters, and flat trigger table are at
   `resources/rgbscripts/audiofireworks.js:42-76,241-258`.
5. **Regression:** the table-driven property/list and trigger-response checks in
   `node tests/test_audio_scripts.js` fail before repair and must pass afterward
   through `rgbMap()`.
6. **Conflicting-intent search:** script comments, repository docs/tests, related
   event scripts, and `git log -S'SPAWN_TRIGGERS'` expose no current nested or
   note-event contract. History locates the stale table at `3810e5380`; live
   engine fields control.

Classification: **proven defect**.

## Quality rubric and limits

“Algorithmic quality” means the first eight dimensions below. “Similarity”
means dimension 9 only; it is structural, not pixel or perceptual parity.
“Solid and sensible” means a stable boundary contract, coherent declared
controls, bounded dynamics, explicit timing/state, and computational work
proportional to the effect. “Easy to adapt to music” means dimension 7 exposes
understandable causal band/event/tempo hooks and effect controls. “Purposeful
and strong” is dimension 10: automation supplies contrast and causal-response
evidence, while final composition, musical readability, and timing feel remain
human judgments.

| ID | Dimension | Mechanical pass signal | Interpretation limit |
|---|---|---|---|
| D1 | Contract/numerical stability | Exact `Float32Array`, `3WH` length, finite `[0,1]` HSV | Does not prove attractive output |
| D2 | Parameter semantics | Default validity, setter/getter round-trip, declared range endpoints and list choices render safely | Unbounded floats have no invented min/max |
| D3 | Normalization/dynamic range | Silence, unequal isolated bands, low/nominal/peak, event frames; active peak/contrast recorded | Collapse can still be intentional |
| D4 | Time/phase | Consumed event/tempo fields and multi-frame state are exercised | Feel and synchronization remain manual |
| D5 | Spatial coherence | `1×1`, `1×9`, `9×1`, `7×11`, then `7×11→12×5`; contrast required only for promised structure | 1D extrusion is not called a defect |
| D6 | Edge/state | Fresh contexts, seeded repeat, resize, and no script-order dependency | Seeded repeat does not make runtime randomness deterministic |
| D7 | Music hooks | Static consumed-field inventory plus isolated causal response; Water bands checked at widths 7, 12, and 100 | A source read is not perceptual readability; spatial overlap can mask a consumed band |
| D8 | Computational shape | Loop/state/allocation structure relative to area `A=W×H` and bounded effect state | No speed claim without a benchmark |
| D9 | LedFX similarity | Pinned paired source: signal, state, controls, timing, normalization, topology, output | “Different” does not mean “wrong” |
| D10 | Visual intent | Human check of causality, composition, contrast, continuity, and hardware output | **N/A to automation** |

The repaired harness uses fresh VM contexts, a deterministic random source, the
current 15-field audio object, and the engine palette shape. Its final run
reported `41 passed, 0 failed`, with 2,227 map-contract checks, 381 property
writes, 43 general response checks, 39 spatial-contrast checks, 41 seeded reset
checks, 82 resize checks, and 9 table-driven Water isolated-band cases. Those
Water cases classify six width/band combinations as responsive and three as
matching silence. Audio Beat Colors and Audio Strobe were not required to have
value contrast for their uniform modes; their event response and full HSV
contract were still checked. This is boundary evidence, not a visual score.

## Ten-dimension assessment of all audio scripts

In D3, `p/c` is the maximum active value and maximum across-channel spatial
contrast observed in the fixed-seed semantic sequence. D2 “green” means every
declared default was valid, every range endpoint/list edge round-tripped and
rendered, and unbounded floats/strings were checked only at their declared
default.
`A` means matrix area; `P`, `L`, `R`, and `k` mean bounded particles, lines,
ripples, and configured simulation steps.

| Script and current source | D1 contract | D2 parameters | D3 dynamics (`p/c`) | D4 time/state | D5 spatial shape | D6 edge/reset | D7 music | D8 computational shape | D9 LedFX | D10 visual |
|---|---|---|---|---|---|---|---|---|---|---|
| Audio Aurora (`resources/rgbscripts/audioaurora.js:20-97,98-184`) | Green | 5, green | .507/.992 | `dt`, pulse, EMA | True 2D layered waves | Deterministic; resize green | H01 | O(A×layers), per-frame small arrays | N/A—QLC-authored | N/A automation; check layered causality |
| Audio Barcode (`resources/rgbscripts/audiobarcode.js:20-141,142-215`) | Green | 10, green | 1/1 | `dt/bpm`, onset line state | Axis-selectable 1D lines extruded | Deterministic; line reset on resize | H02 | O(A+L×axis), bounded L | N/A—QLC-authored | N/A automation; check scrolling/decay feel |
| Audio Bass Laser (`resources/rgbscripts/audiobasslaser.js:20-187,188-254`) | Green | 5, green | .292/.951 | beat spawn plus trail state | 2D beam/trail field | Random; seeded reset green | H03 | O(A+P×trail), bounded beams | N/A—QLC-authored | N/A automation; check punch/readability |
| Audio Beat Colors (`resources/rgbscripts/audiobeatcolors.js:20-155,156-208`) | Green | 8, green | .950/0 default Cut | beat/bar phase and downbeat | Uniform Cut; optional LR/TB/radial transitions | Deterministic; resize green | H04 | O(A) | N/A—QLC-authored | N/A automation; check beat identity/transitions |
| Audio Blocks (`resources/rgbscripts/audioblocks.js:20-80,81-124`) | Green | 3, green | .974/.986 | `dt` phase | 1D blocks extruded vertically | Deterministic; resize green | H05 | O(A) | Declared port; §LedFX | N/A automation; check block legibility |
| Audio Blurz (`resources/rgbscripts/audioblurz.js:20-180,181-247`) | Green | 7, green | 1/1 | `dt/bpm`, flux bursts/decay | Axis-selectable 1D blur extruded | Random; seeded reset green | H06 | O(A×radius+bursts) | N/A—QLC-authored | N/A automation; check burst-to-blur continuity |
| Audio Buildup (`resources/rgbscripts/audiobuildup.js:14-199,200-235`) | Green | 3, green | .748/.690 | beat-counted build/drop/cooldown | Center/edge 1D composition extruded | Deterministic; lifecycle green | H07 | O(A) | N/A—QLC-authored | N/A automation; check build/drop narrative |
| Audio Cellular (`resources/rgbscripts/audiocellular.js:20-119,120-200`) | Green | 4, green | .950/1 | `dt/bpm`, beat/downbeat generation | 1D cellular strip extruded | Random seed mode; seeded reset green | H08 | O(A) plus two 1D buffers | N/A—QLC-authored | N/A automation; check rule rhythm |
| Audio Chaser (`resources/rgbscripts/audiochaser.js:20-128,129-245`) | Green | 6, green | 1/1 | `dt/bpm`, beat/downbeat, EMA | Multi-row moving dots/trails | Random placement; seeded reset green | H09 | O(A+P×trail) | N/A—QLC-authored | N/A automation; check speed causality |
| Audio Crawler (`resources/rgbscripts/audiocrawler.js:20-64,65-110`) | Green | 5, green | 1/.998 | `dt` phase | 1D chopped crawler extruded | Deterministic; resize green | H10 | O(A) | Declared port; §LedFX | N/A automation; check sway/chop purpose |
| Audio DJ Light (`resources/rgbscripts/audiodjlight.js:20-81,82-163`) | Green | 4, green | 1/.084 | `dt`, band EMA | Axis-selectable three blobs | Deterministic; resize green | H11 | O(A) | N/A—QLC-authored | N/A automation; check band separation |
| Audio Energy (`resources/rgbscripts/audioenergy.js:20-71,72-129`) | Green | 2, green | .950/1 | pulse/downbeat and bar accumulation | Three horizontal reaches extruded | Deterministic; state/resize green | H12 | O(A) | Declared port; §LedFX | N/A automation; check bar-fill hierarchy |
| Audio Energy 2 (`resources/rgbscripts/audioenergy2.js:20-63,64-100`) | Green | 3, green | .991/1 | `dt` phase and EMA | 1D energy gradient extruded | Deterministic; resize green | H13 | O(A) | Declared port; §LedFX | N/A automation; check motion strength |
| Audio Equalizer (`resources/rgbscripts/audioequalizer.js:20-85,86-164`) | Green | 6, green | .950/1 | peak hold/decay; downbeat drop | 2D band columns | Deterministic; resize green | H14 | O(A+W) | N/A—QLC-authored | N/A automation; check readable bands/peaks |
| Audio Fire (`resources/rgbscripts/audiofire.js:20-69,70-146`) | Green | 4, green | 1/1 | `dt/bpm`, spark state | 1D flame strip extruded | Random sparks; seeded reset green | H15 | O(A+W) | Declared port; §LedFX | N/A automation; check flame continuity |
| Audio Fireworks (`resources/rgbscripts/audiofireworks.js:20-247,248-293`) | Green after trigger repair | 7, green; Beat/Onset only | 1/1 | flat Beat/Onset spawn, particle life | True 2D particles | Random; seeded reset green | H16 | O(A/80+P×size²), bounded P | N/A—QLC-authored | N/A automation; check burst weight/gravity |
| Audio Flow Field (`resources/rgbscripts/audioflowfield.js:20-179,180-276`) | Green | 12, green | 1/1 | `dt/bpm`, beat burst, particle life | True 2D advected particles | Random; seeded reset green | H17 | O(A+P), bounded P | N/A—QLC-authored | N/A automation; check flow coherence |
| Audio Glitch (`resources/rgbscripts/audioglitch.js:20-100,101-176`) | Green | 4, green | 1/.085 | `dt/bpm`, event flash state | 1D glitch bands extruded | Deterministic; resize green | H18 | O(A) | Declared port; §LedFX | N/A automation; check flash/glitch balance |
| Audio Glitch 2 (`resources/rgbscripts/audioglitch2.js:20-70,71-126`) | Green | 3, green | 1/.966 HSV | six `dt/bpm` phase tracks | 1D hue/saturation stripes extruded | Deterministic; resize green | H19 | O(A) | Declared port; §LedFX | N/A automation; check constant-value palette |
| Audio Gravimeter (`resources/rgbscripts/audiogravimeter.js:20-94,95-181`) | Green | 6, green | .950/1 | `dt/bpm`, peak hold/flash | Axis-selectable meter extruded | Deterministic; resize green | H20 | O(A+bands) | N/A—QLC-authored | N/A automation; check peak timing |
| Audio Hue Shift (`resources/rgbscripts/audiohueshift.js:20-86,87-130`) | Green | 5, green | .667/.966 | pulse plus band EMA; no `dt` | 1D hue wave extruded | Deterministic; resize green | H21 | O(A) | N/A—QLC-authored | N/A automation; check static-tempo behavior |
| Audio Lava Lamp (`resources/rgbscripts/audiolava.js:20-56,57-105`) | Green | 3 defaults green | 1/.795 | `dt` phase | 1D lava field extruded | Deterministic; resize green | H22 | O(A) | Declared port; §LedFX | N/A automation; check organic continuity |
| Audio Melt (`resources/rgbscripts/audiomelt.js:20-52,53-92`) | Green | 2 defaults green | .903/.903 | `dt` phase | 1D melt gradient extruded | Deterministic; resize green | H23 | O(A) | Declared port; §LedFX | N/A automation; check melt direction |
| Audio Melt and Sparkle (`resources/rgbscripts/audiomeltsparkle.js:20-169,170-295`) | Green | 11, green | .379/.759 | `dt/bpm`, onset strobe/decay | Axis-selectable 1D melt/strobe | Random direction/segments; seeded reset green | H24 | O(A×blur) | Declared port; §LedFX | N/A automation; check strobe restraint |
| Audio Plasma (`resources/rgbscripts/audioplasma.js:20-90,91-148`) | Green | 7, green | .869/.422 | `dt`, selected-band phase | True 2D analytic plasma | Deterministic; resize green | H25 | O(A) | Declared port; §LedFX | N/A automation; check depth/contrast |
| Audio Power (`resources/rgbscripts/audiopower.js:20-103,104-184`) | Green | 4 defaults/range green | 1/.874 | `dt`, onset/downbeat envelopes | 1D bass/spark strip extruded | Random sparks; seeded reset green | H26 | O(A+W×blur) | N/A—QLC-authored | N/A automation; check bass/spark separation |
| Audio Puddles (`resources/rgbscripts/audiopuddles.js:20-101,102-196`) | Green | 7, green | 1/1 | `dt/bpm`, selectable event spawn | True 2D expanding rings | Random centers; seeded reset green | H27 | O(A×R), bounded R | N/A—QLC-authored | N/A automation; check ring overlap |
| Audio Reaction-Diffusion (`resources/rgbscripts/audioreaction.js:20-165,166-238`) | Green | 11, green | .950/.994 | `dt/bpm`, beat seed and band modulation | True 2D simulation | Random seeds; seeded reset green | H28 | O(kA), configured k | N/A—QLC-authored | N/A automation; check evolution vs flicker |
| Audio Reactor (`resources/rgbscripts/audioreactor.js:20-115,116-245`) | Green after clamp | 5, green | .950/1 | `dt`, bar/downbeat/onset/pulse state | True 2D band-dependent layouts | Random sparkles; seeded reset green | H29 | O(A) | N/A—QLC-authored | N/A automation; check causal mode changes |
| Audio Scan (`resources/rgbscripts/audioscan.js:20-150,151-256`) | Green | 11, green | .475/.500 | `dt/bpm`, selected-band EMA/downbeat | Axis-selectable scanner extruded | Deterministic; resize green | H30 | O(A+count×axis) | Declared port; §LedFX | N/A automation; check scan weight |
| Audio Scan and Flare (`resources/rgbscripts/audioscanflare.js:20-153,154-274`) | Green | 11, green | 1/1 | `dt/bpm`, low threshold flare | Axis scan plus flare extruded | Deterministic; resize green | H31 | O(A+sparkles×axis) | Declared port; §LedFX | N/A automation; check flare causality |
| Audio Scan Multi (`resources/rgbscripts/audioscanmulti.js:20-124,125-229`) | Green | 7, green | 1/1 | `dt/bpm`, three-band scan state | Axis-selectable multi-scan extruded | Deterministic; resize green | H32 | O(A) | Declared port; §LedFX | N/A automation; check band separation |
| Audio Shockwave (`resources/rgbscripts/audioshockwave.js:20-132,133-230`) | Green | 7, green | .744/.643 | `dt/bpm`, onset/beat wave life | True 2D radial waves | Deterministic; resize green | H33 | O(A×waves), bounded waves | N/A—QLC-authored | N/A automation; check impact/decay |
| Audio Shot (`resources/rgbscripts/audioshot.js:20-90,91-155`) | Green | 4, green | 1/1 | event spawn and frame decay | True 2D local shots | Random positions; seeded reset green | H34 | O(A+P×size²), bounded P | N/A—QLC-authored | N/A automation; check trigger choices |
| Audio Soap (`resources/rgbscripts/audiosoap.js:20-114,115-240`) | Green | 4 defaults/list green | .923/.999 | `dt/bpm`, selected band | True 2D warped/noise field | Random initial phase; seeded reset green | H35 | O(A) | Declared port; §LedFX | N/A automation; check fluid continuity |
| Audio Spectrum (`resources/rgbscripts/audiospectrum.js:20-95,96-127`) | Green after 1-pixel repair | 2, green | .930/.320 | frame-to-frame filter; no tempo | Flattened row-major 1D spectrum | Deterministic; resize green | H36 | O(A), several A-sized arrays | Declared port; §LedFX | N/A automation; check matrix traversal |
| Audio Split Tower (`resources/rgbscripts/audiosplittower.js:20-77,78-142`) | Green | 3, green | 1/1 | pulse, peak hold/decay | Multi-band 2D tower | Deterministic; resize green | H37 | O(A+bands) | N/A—QLC-authored | N/A automation; check tower balance |
| Audio Strobe (`resources/rgbscripts/audiostrobe.js:20-106,107-192`) | Green | 6, green | 1/0 default full-width | `dt/bpm`, selectable event, two decay overlays | 1D segment extruded; full width can be uniform | Random segment position; seeded reset green | H38 | O(A+W) | Declared port; §LedFX | N/A automation; check comfort/timing |
| Audio Tunnel (`resources/rgbscripts/audiotunnel.js:20-74,75-165`) | Green | 5, green | .748/.746 | `dt`, pulse/downbeat | True 2D selectable-distance rings | Deterministic; resize green | H39 | O(A) | N/A—QLC-authored | N/A automation; check depth/motion |
| Audio Vortex (`resources/rgbscripts/audiovortex.js:20-73,74-154`) | Green | 5, green | .733/.733 | `dt`, pulse/downbeat | True 2D arms | Deterministic; resize green | H40 | O(A×arms), bounded arms | N/A—QLC-authored | N/A automation; check arm coherence |
| Audio Water (`resources/rgbscripts/audiowater.js:20-98,99-157`) | Green after periodic-hue repair | 6 defaults green | 1/.776 combined at 12×5; isolated response is width-sensitive | frame-stepped ripple; no tempo | 1D ripple extruded vertically | Deterministic; resize green | H41, qualified | O(A+W) with two W buffers | Declared port; §LedFX | N/A automation; check ripple readability |

## Script-specific music adaptation hooks

“Missing” names absent causal categories rather than defects. For example,
“no discrete event” means the effect is continuous by design unless product
intent requests a hit response. Controls are the current script properties;
their declared range/list edges were exercised by the harness.

| ID / script | Consumed music inputs | Meaningful controls | Expected coded response | Missing hook / adaptation consequence |
|---|---|---|---|---|
| H01 Aurora | `low/mid/high`, `dt`, `cosPulse` | reactivity, speed, layers, wave size, smoothing | bands weight layer color/value; beat time moves waves; pulse lifts value | No onset/downbeat event |
| H02 Barcode | `high`, `onset/onsetIntensity`, `dt/bpm` | width, scroll, decay, trigger/threshold, HFC scale, line cap, refractory, axis | onset/flux spawns high-colored line; tempo-derived elapsed time scrolls/decays | No low/mid causal power |
| H03 Bass Laser | `low/high`, `beatFired`, `onsetIntensity` | trail, beam cap, direction, speed, smoothing | beat spawns beams; low drives body, high ambient/twinkle, intensity scales hit | No `dt/bpm`; motion is frame-stepped |
| H04 Beat Colors | `phase/barPhase`, `cosPulse`, `downbeat`, `bpm` | transition, pulse/boost/width, four beat masks | phase selects/blends beat color; downbeat boosts | No spectral-band or onset hook |
| H05 Blocks | `low`, `dt` | speed, reactivity, fixed hues | low changes block intensity/phase; `dt` rolls layout | No mid/high or discrete event |
| H06 Blurz | `beat/bass/low/mid/high`, `onsetIntensity`, `dt/bpm` | blur/decay/flux, burst density/spread/cap, axis | flux above threshold spawns at strongest band; powers color; time decays | No boolean onset/beat trigger; intensity is the trigger |
| H07 Buildup | `beatFired`, `cosPulse`, `dt` | cycle beats, drop intensity, color scheme | beat edges advance build; `dt` runs drop/cooldown; pulse shimmers | No band balance or onset intensity |
| H08 Cellular | `low`, `beatFired/downbeat`, `dt/bpm` | rules, seed mode/threshold, scroll rate | low/beat seed cells; tempo time advances generations; downbeat affects reset/seed path | No mid/high/onset |
| H09 Chaser | `low/mid/high`, `beatFired/downbeat`, `cosPulse`, `dt/bpm` | base speed, dots/trail, speed source, bounce, smoothing | selected band mix controls speed/color; beat/pulse accents chasers | No onset-intensity control |
| H10 Crawler | `low`, `dt` | speed, reactivity, sway, chop, stretch | low accelerates/deforms crawler; `dt` advances | No other bands/events/direct BPM |
| H11 DJ Light | `low/mid/high`, `dt` | blob width, speed, axis, smoothing | three bands set colored blob levels; time moves positions | No discrete event or BPM scaling |
| H12 Energy | `low/mid/high`, `cosPulse/downbeat`, `dt` | fill multiplier, smoothing | each band sets reach; pulse brightens; downbeat releases accumulated energy | No onset/explicit BPM |
| H13 Energy 2 | `low`, `dt` | speed, reactivity, smoothing | low modulates brightness/phase; time advances gradient | No mid/high/event hook |
| H14 Equalizer | `beat/bass/low/mid/high`, `downbeat` | decay, peaks, center, downbeat drop, peak hold/decay | band powers set columns/peaks; downbeat drops bars | No `dt/bpm`; decay is per frame |
| H15 Fire | `low`, `dt/bpm` | speed, color shift, intensity, fade chance | low injects heat/sparks; tempo-derived time evolves fire | No mid/high/discrete event |
| H16 Fireworks | `low/mid/high`, `beatFired/onset`, `onsetIntensity` | particle cap, gravity, origin/size, Beat/Onset trigger, kick threshold, ambient floor | selected flat event spawns dominant-band burst; intensity gates extra kick | No `dt/bpm`; particle physics is frame-stepped |
| H17 Flow Field | `low/mid/high`, `beatFired`, `dt/bpm` | particle/trail/life caps, base/high/field/morph/turbulence speeds, burst, wrap, smoothing | bands alter velocity/color/field; beat adds particles; tempo advances | No onset/downbeat |
| H18 Glitch | `low/mid/high`, `onset/beatFired`, `onsetIntensity`, `dt/bpm` | reactivity, speed, saturation, complexity | low accelerates phase; events trigger dominant-band flash | No downbeat/bar-phase hook |
| H19 Glitch 2 | `low`, `dt/bpm` | speed, reactivity, saturation | low boosts six phase tracks; time moves hue/saturation stripes | No mid/high/event hook |
| H20 Gravimeter | `low/mid/high`, `beatFired`, `dt/bpm` | gravity, peak decay/hold/flash, band count, axis | band level drives meter; beat/high state affects peak flash | No onset/downbeat |
| H21 Hue Shift | `low/mid/high`, `cosPulse` | speed, wave scale, saturation, minimum value, smoothing | band blend selects hue/value; pulse offsets/lifts wave | No `dt/bpm`; speed is not time-integrated |
| H22 Lava | `low`, `dt` | speed, contrast, reactivity | low deforms/intensifies 1D lava; time moves field | No mid/high/event/direct BPM |
| H23 Melt | `low`, `dt` | speed, reactivity | low changes melt displacement; time advances | No mid/high/event/direct BPM |
| H24 Melt+Sparkle | `low/mid/high`, `onset`, `dt/bpm` | melt speed/reactivity/background/width; strobe threshold/rate/width/decay/blur; axis/smoothing | bands drive melt/color; onset gates sparkle/strobe state | No beat/downbeat |
| H25 Plasma | `beat/bass/low/mid/high`, `dt` | density/lower/vertical/twist/radius, selected frequency, smoothing | selected band changes plasma phase/geometry; `dt` moves field | No event or direct BPM |
| H26 Power | `beat/bass/low/mid/high`, `onset/downbeat`, `dt` | blur, bass/spark decay, smoothing | bass builds strip; high/onset adds sparks; downbeat changes envelope | No explicit BPM |
| H27 Puddles | `low/mid/high`, `onset/onsetIntensity/beatFired`, `dt/bpm` | ripple cap/speed/radius/life/width, trigger, refractory | selected event spawns dominant-band ring; intensity scales it; tempo expands | No downbeat |
| H28 Reaction-Diffusion | `low/mid/high`, `beatFired`, `dt/bpm` | feed/kill/diffusion/time step/steps/seed size and band modulations/gain | bands modulate simulation coefficients; beat seeds; time scales integration | No onset/downbeat |
| H29 Reactor | `low/mid/high`, `onset/onsetIntensity`, `cosPulse/downbeat/barPhase`, `dt` | palette, sensitivity, flash, sparkles, smoothing | dominant band switches layout; onset flashes; high/pulse sparkles; bar state swells/releases | No explicit `beatFired/bpm` |
| H30 Scan | `beat/bass/mid/high`, `dt/bpm`, `downbeat` | blur/bounce/width/speed/frequency/multiplier/color flags/gradient/count/smoothing | selected band sizes/colors scan; tempo advances; downbeat state is read | `low` composite is not read directly; no onset |
| H31 Scan+Flare | `low`, `dt/bpm`, `downbeat` | speed/width/multiplier/bounce, sparkle cap/size/time/threshold, color, axis, smoothing | low drives scanner and flare threshold; tempo moves/ages; downbeat is read | No mid/high/onset |
| H32 Scan Multi | `low/mid/high`, `dt/bpm`, `downbeat` | speed/width/multiplier/bounce/color/axis/smoothing | three bands drive separate scanners; tempo moves them | No onset/beatFired |
| H33 Shockwave | `low/mid/high`, `onset/beatFired`, `dt/bpm` | wave cap/width/speed/decay, ambient speed, wave decay, smoothing | event spawns dominant-band radial wave; tempo expands/decays | No onsetIntensity/downbeat |
| H34 Shot | `low/mid/high`, `onset/onsetIntensity/beatFired` | decay, size, trigger band/event, shot cap | selected event spawns a band-located shot; intensity scales value | No `dt/bpm`; decay is per frame |
| H35 Soap | `beat/bass/low/mid/high`, `dt/bpm` | density, speed, intensity, selected frequency | selected band deforms/brightens 2D field; tempo moves it | No discrete event |
| H36 Spectrum | `low/mid/high` | RGB mix permutation, decay | bands interpolate across flattened pixels; frame delta forms a channel | No event/tempo; decay property is declared but source does not consume it—quality risk |
| H37 Split Tower | `low/mid/high`, `cosPulse` | band count, peak hold, decay | powers set tower segments/peaks; pulse brightens | No `dt/bpm` or discrete event |
| H38 Strobe | `low/mid/high`, `onset/beatFired`, `dt/bpm` | color step/delay, bass/strobe decay, width, trigger | selected event fills decay overlay; dominant band tints it; tempo forms elapsed time | No downbeat/onset intensity |
| H39 Tunnel | `low/mid/high`, `cosPulse/downbeat`, `dt` | speed, rings, shape, reactivity, smoothing | low changes ring radius/drive; high and pulse affect value; time moves depth | No onset/explicit BPM |
| H40 Vortex | `low/mid/high`, `cosPulse/downbeat`, `dt` | reactivity, speed, arm count/tightness, smoothing | lows change rotation/radius, highs/pulse value; time rotates arms | No onset/explicit BPM |
| H41 Water | `low/mid/high` | speed, vertical shift, three drop sizes, viscosity; no sensitivity control | Squared band powers inject ordered emitters. Isolated high is responsive at widths 7/12/100; isolated low matches silence at 7 and responds at 12/100; isolated mid matches silence at 7/12 and responds at 100. | No event/tempo; simulation steps per frame. Emitter overlap lets later zero-valued bands mask earlier drops on small matrices. |

### Report-only findings from the hook inventory

- **Quality risk / coverage gap:** Audio Spectrum declares `decay`, but its
  `rgbMap()` never reads `algo.presetDecay`
  (`resources/rgbscripts/audiospectrum.js:42-47,96-127`). There is no explicit
  requirement for the control's direction, so it remains report-only.
- **Quality risk / coverage gap:** Audio Hue Shift exposes `presetSpeed` yet
  consumes no `dt`, phase, or BPM (`resources/rgbscripts/audiohueshift.js:20-50,87-130`).
  Its source must be visually checked before calling the control misleading.
- **Validated design divergence:** several 1D LedFX ports deliberately extrude a
  strip over matrix height. The topology is stated per row; extrusion alone is
  not a defect.
- **Quality risk / coverage gap:** Water consumes all three bands, but its
  ordered emitters overlap at small widths. High is reliable there; low/mid
  become visible only with sufficient width under the current mapping, and the
  controls expose drop size rather than input sensitivity. This is qualified
  behavior, not authorization to change the production algorithm.

## Commit-pinned LedFX comparison

The authoritative comparison baseline is LedFX default-branch commit
[`091b72396bf71e151a552e416df98be28d71d784`](https://github.com/LedFx/LedFx/commit/091b72396bf71e151a552e416df98be28d71d784).
Every source link below contains that immutable commit. “Same” requires the
same signal, state/filter, controls, timing, normalization, topology, and output
mapping except syntax. “Adapted” retains a recognizable core while changing
platform contracts or adding behavior. “Divergent” retains the declared name
but changes the core signal or algorithm. “Cannot tell” is reserved for missing
source/intent. Pixel and perceptual parity were not measured.

The 18 declarations map to 17 upstream files because both QLC+ Glitch variants
map to `glitch.py`. Result: 16 **adapted**, 2 **divergent**, 0 **same**, and
0 **cannot tell**. These are comparative classifications, not defect counts.

| QLC+ declaration and source | Exact pinned LedFX source | Class | Retained structure | Measured/source-visible differences |
|---|---|---|---|---|
| Blocks (`resources/rgbscripts/audioblocks.js:20-123`) | [`blocks.py`](https://raw.githubusercontent.com/LedFx/LedFx/091b72396bf71e151a552e416df98be28d71d784/ledfx/effects/blocks.py) | **Divergent** | Audio input and a moving 1D colored strip remain | Upstream resamples the full filtered melbank, splits by `block_count`, scales each block by its maximum/gradient, and rolls the gradient. QLC+ uses only smoothed low power and six analytic phases with speed/reactivity/fixed-hue controls; no block count or melbank blocks remain. QLC+ extrudes width over height. |
| Crawler (`resources/rgbscripts/audiocrawler.js:20-110`) | [`crawler.py`](https://raw.githubusercontent.com/LedFx/LedFx/091b72396bf71e151a552e416df98be28d71d784/ledfx/effects/crawler.py) | **Adapted** | Same low filter and speed/reactivity/sway/chop/stretch phase equations | LedFX adds wall-clock nanoseconds and low-power time; QLC+ integrates beat-relative `audio.dt`. QLC+ emits HSV and extrudes the strip. |
| Energy (`resources/rgbscripts/audioenergy.js:20-129`) | [`energy.py`](https://raw.githubusercontent.com/LedFx/LedFx/091b72396bf71e151a552e416df98be28d71d784/ledfx/effects/energy.py) | **Adapted** | Three bands set three reach indices and colors, followed by temporal filtering | LedFX uses unfiltered melbank-third means, additive/overlap modes, optional color cycling, mirror/blur, and an RGB array filter. QLC+ uses flat powers, HSV palette, its own asymmetric EMA, pulse/downbeat bar-release state, one fill multiplier, and vertical extrusion. |
| Energy 2 (`resources/rgbscripts/audioenergy2.js:20-100`) | [`energy2.py`](https://raw.githubusercontent.com/LedFx/LedFx/091b72396bf71e151a552e416df98be28d71d784/ledfx/effects/energy2.py) | **Adapted** | Low-power filter, speed/reactivity phase, triangle-squared value, saturation threshold | QLC+ defaults are `.075/.4` versus `.1/.2`, exposes smoothing, uses beat `dt` instead of LedFX time, and extrudes HSV width over height. |
| Fire (`resources/rgbscripts/audiofire.js:20-146`) | [`fire.py`](https://raw.githubusercontent.com/LedFx/LedFx/091b72396bf71e151a552e416df98be28d71d784/ledfx/effects/fire.py) | **Adapted** | Same four defaults, low EMA, spark lifecycle, diffusion weights, and HSV heat mapping | LedFX uses elapsed seconds/NumPy arrays; QLC+ uses `dt/bpm`, JS arrays/loops, clamps all HSV channels, and extrudes the 1D flame. |
| Glitch (`resources/rgbscripts/audioglitch.js:20-176`) | [`glitch.py`](https://raw.githubusercontent.com/LedFx/LedFx/091b72396bf71e151a552e416df98be28d71d784/ledfx/effects/glitch.py) | **Adapted** | Low-driven six-rate phase system and modular hue/saturation equations | QLC+ uses beat-relative time, adds complexity, event-triggered dominant-band flash/color, nonconstant value, and vertical extrusion. |
| Glitch 2 (`resources/rgbscripts/audioglitch2.js:20-126`) | [`glitch.py`](https://raw.githubusercontent.com/LedFx/LedFx/091b72396bf71e151a552e416df98be28d71d784/ledfx/effects/glitch.py) | **Adapted** | Closest local translation: the six ratios, modular hue, layered triangle saturation, and value `1` are retained | QLC+ defaults differ (`.125/.4` versus `.5/.2`), uses beat `dt` instead of wall time, and extrudes the strip. |
| Lava Lamp (`resources/rgbscripts/audiolava.js:20-105`) | [`lava_lamp.py`](https://raw.githubusercontent.com/LedFx/LedFx/091b72396bf71e151a552e416df98be28d71d784/ledfx/effects/lava_lamp.py) | **Adapted** | Same controls/defaults, low EMA, two time waves, three-wave product, hue offset, and squared contrast value | QLC+ integrates beat time, clamps HSV, and extrudes a 1D field. |
| Melt (`resources/rgbscripts/audiomelt.js:20-92`) | [`melt.py`](https://raw.githubusercontent.com/LedFx/LedFx/091b72396bf71e151a552e416df98be28d71d784/ledfx/effects/melt.py) | **Adapted** | Same controls/defaults, low filter, two rates, reversed ramp, repeated sine, squared value | LedFX accumulates wall time plus low offset; QLC+ uses beat `dt` and a non-accumulated low offset, clamps HSV, and extrudes. |
| Melt and Sparkle (`resources/rgbscripts/audiomeltsparkle.js:20-295`) | [`melt_and_sparkle.py`](https://raw.githubusercontent.com/LedFx/LedFx/091b72396bf71e151a552e416df98be28d71d784/ledfx/effects/melt_and_sparkle.py) | **Adapted** | Melt pipeline, low/mid modulation, onset/high strobe, direction changes, overlay decay/blur, and nine upstream controls remain | QLC+ adds axis and smoothing, derives elapsed time from beat `dt`, caps strobe width, uses flat powers rather than melbank maxima, and emits an extruded HSV strip. |
| Plasma (`resources/rgbscripts/audioplasma.js:20-148`) | [`plasma2d.py`](https://raw.githubusercontent.com/LedFx/LedFx/091b72396bf71e151a552e416df98be28d71d784/ledfx/effects/plasma2d.py) | **Adapted** | Same five controls, selectable band, coordinate scale, and three-term 2D plasma equation | LedFX normalizes each frame by its observed min/max and maps RGB gradient via PIL. QLC+ maps the known raw `[-3,3]` sum directly to `[0,1]`, uses an EMA and beat time, and returns HSV. This normalization difference is a validated divergence, not a defect. |
| Scan (`resources/rgbscripts/audioscan.js:20-256`) | [`scan.py`](https://raw.githubusercontent.com/LedFx/LedFx/091b72396bf71e151a552e416df98be28d71d784/ledfx/effects/scan.py) | **Adapted** | Selected-band power drives speed/intensity; width, bounce, count, multiplier, gradient modes, and moving blocks remain | QLC+ uses HSV palette, asymmetric EMA, beat time, axis selection, and bar/downbeat width/brightness state; LedFX uses elapsed seconds, RGB/background/mirror/blur/modulation facilities. |
| Scan and Flare (`resources/rgbscripts/audioscanflare.js:20-274`) | [`scan_and_flare.py`](https://raw.githubusercontent.com/LedFx/LedFx/091b72396bf71e151a552e416df98be28d71d784/ledfx/effects/scan_and_flare.py) | **Adapted** | Low-driven scan, thresholded bounded sparkles, bounce, width/speed/multiplier, intensity, and sparkle life remain | QLC+ converts percent/ms controls, adds axis/smoothing/downbeat release, uses beat-relative time and HSV; LedFX uses wall time, RGB background, mirror/blur/gradient. |
| Scan Multi (`resources/rgbscripts/audioscanmulti.js:20-229`) | [`scan_multi.py`](https://raw.githubusercontent.com/LedFx/LedFx/091b72396bf71e151a552e416df98be28d71d784/ledfx/effects/scan_multi.py) | **Adapted** | Three independent low/mid/high scans, colors, width, bounce, multiplier, and intensity remain | LedFX selects Power/Melbank and optional attack/decay filters. QLC+ always applies its EMA, adds axis/downbeat release, uses beat time/HSV, and computes each scan step independently. |
| Soap (`resources/rgbscripts/audiosoap.js:20-240`) | [`soap2d.py`](https://raw.githubusercontent.com/LedFx/LedFx/091b72396bf71e151a552e416df98be28d71d784/ledfx/effects/soap2d.py) | **Adapted** | Same density/speed/intensity/band controls, 2D smoothed noise, persistent palette pixels, and signed row-then-column smear | QLC+ uses shared simplex noise and HSV shortest-arc interpolation; LedFX uses FastNoiseLite, NumPy RGB buffers, and PIL. QLC+ movement is beat-derived rather than elapsed seconds. |
| Spectrum (`resources/rgbscripts/audiospectrum.js:20-127`) | [`spectrum.py`](https://raw.githubusercontent.com/LedFx/LedFx/091b72396bf71e151a552e416df98be28d71d784/ledfx/effects/spectrum.py) | **Adapted** | RGB-channel permutation, raw frame delta, asymmetric filtered channel, and previous-frame state remain | LedFX uses full raw/filtered melbank arrays and multiplies RGB output by 1000. QLC+ interpolates only three flat band powers across row-major matrix pixels and converts conceptual RGB to bounded HSV. QLC+ also declares an unused `decay` control. |
| Strobe (`resources/rgbscripts/audiostrobe.js:20-192`) | [`strobe.py`](https://raw.githubusercontent.com/LedFx/LedFx/091b72396bf71e151a552e416df98be28d71d784/ledfx/effects/strobe.py) | **Divergent** | Both emit a gradient-colored strobe with decay | LedFX is oscillator/bar-pattern driven with frequency, strobe decay, beat decay, and four-beat masks, producing a full-frame value. QLC+ is onset/beat-event driven with random-width overlays, dominant-band tint, two decay overlays, refractory timing, and no oscillator pattern. |
| Water (`resources/rgbscripts/audiowater.js:20-157`) | [`water.py`](https://raw.githubusercontent.com/LedFx/LedFx/091b72396bf71e151a552e416df98be28d71d784/ledfx/effects/water.py) | **Adapted** | Same six defaults, squared three-band emitters/positions, double-buffer ripple, smoothing/damping, periodic hue, shifted value, and hot-value saturation | LedFX queues audio updates before render and uses NumPy smooth; QLC+ injects each frame directly, uses local smoothing loops, clamps HSV, and extrudes the 1D ripple vertically. |

### Comparative conclusions

- **Proven defect:** none is established merely by the two divergent
  classifications. Blocks and Strobe require a product-intent decision before
  parity work.
- **Quality risk / coverage gap:** Blocks metadata names an upstream effect whose
  current core algorithm and control surface are absent locally. Rename,
  re-port, or retain-with-documentation is an open product decision.
- **Quality risk / coverage gap:** Spectrum compresses a full melbank curve to
  three scalars and exposes an unused decay control. Restoring spectral detail
  is a feature decision, not a proof-gated repair.
- **Validated design divergence:** BPM-relative `dt`, HSV output, palette
  injection, matrix extrusion, and bar/downbeat additions are deliberate local
  adaptations where the table identifies them.
- **Open question:** whether users value current QLC+ Strobe event overlays over
  LedFX oscillator/bar patterns cannot be answered from source.

## Actionable conclusions

1. Keep `node tests/test_audio_scripts.js` as the primary audio-contract and
   causal-response gate. The current isolated C++ `runScripts` selector loads
   its own `RGBScriptsCache`, prints 80 `Evaluating script` lines, and passes;
   the Node gate adds an explicit 41-audio inventory and band-level semantics.
2. Retain the four production repairs. Each has a persisted six-part proof
   chain and a before/after public-boundary regression.
3. Decide the product identity of Blocks before changing code: either keep the
   current analytic strip and document/rename it, or re-port pinned `blocks.py`
   with a full-melbank/block-count product requirement.
4. Decide Strobe's musical model before parity work: current QLC+ emphasizes
   onset/beat hit overlays; pinned LedFX emphasizes oscillator subdivision and
   bar masks. Both are coherent, but they expose different live controls.
5. Remove or implement Spectrum `decay` only after its intended direction and
   range are specified. The present unused property is proven; the desired
   behavior is not.
6. For music adaptation, add missing hooks only per effect intent. The hook
   table identifies continuous effects without events and frame-stepped effects
   without tempo; retain those distinctions unless a product requirement says
   otherwise.
7. Complete the compact manual scenario before judging “strong”: visual
   causality, timing feel, transitions, and fixture mapping are not established
   by finite/bounds tests.

## Open questions

- Audio Blocks product identity: current analytic/glitch-like strip or pinned
  LedFX melbank blocks.
- Audio Strobe product model: LedFX-style subdivisions/bar masks, QLC+ event
  overlays, or two separately named effects.
- Spectrum `decay` intended user-visible behavior.
- Which frame-stepped scripts need beat-relative motion after manual timing
  review? Source alone cannot establish that their present feel is wrong.

## Performance scope

No wall-clock performance claim is made, so no arbitrary timing threshold was
added. D8 records structural work only: most scripts are O(A); bounded
particle/ripple/line effects add their state term; Reaction-Diffusion is
O(kA). The harness executes these paths and catches termination/contract
failures, not frame-budget regressions.

## Native-test final status

Fresh correctly rooted executions on the final source (`8c3285a795c4bd2b7460af435bae00961e864824`
plus the working-tree repairs documented below) are green:

- `build/engine/test/rgbscript/rgbscript_test`: `14 passed, 0 failed`.
- `build/engine/test/rgbmatrix/rgbmatrix_test`: `9 passed, 0 failed`.
- `node tests/test_audio_scripts.js`: `41 passed, 0 failed`.

The native binaries resolve fixture/script resources relative to their build
directories, so these totals come from running each executable in its own
directory. They are the final-tree status; superseded pre-repair failure totals
are not current-checkout results.

## Additional proof-gated production repairs

### Audio Reactor value bound

1. **Exact invariant:** emitted value-channel floats stay in `[0,1]`.
2. **Trigger:** fresh `7×11` context, default properties, the review palette,
   and repeated peak frames `{low:.93, mid:.74, high:.61, dt:.08}`.
3. **Pre-repair wrong output:** the exact `git show HEAD:` boundary reproduction
   first exceeds the bound at frame 4/index 221 with
   `1.092071294784546` and reaches `1.2030037641525269` over eight peak
   frames. The same probe on the current source finds no value above one
   (`max=0.949999988079071`). The pre-repair expression is at
   `8c3285a795c4bd2b7460af435bae00961e864824:resources/rgbscripts/audioreactor.js:189-241`
4. **Grounded expected output:** clamp the final brightness multiplier to one,
   matching the documented `rgbMap()` producer contract that interleaved HSV
   floats are in `[0,1]`
   (`engine/src/rgbscriptv4.cpp:580-583`;
   `resources/rgbscripts/hsvutil.js:5-11`). Downstream `hsvToRgb()` also
   normalizes hue and clamps saturation/value
   (`engine/src/rgbscriptv4.cpp:73-81,649-660`); the producer-side repair is not
   grounded on a claim that the engine fails to clamp. The final clamp is at
   `resources/rgbscripts/audioreactor.js:189-239`.
5. **Regression:** the repeated peak sequence in
   `node tests/test_audio_scripts.js` fails before and must pass after repair.
6. **Conflicting-intent search:** docs, tests, related algorithms, and
   `git log -S'var brightness = Math.min(1, level) * overall'` contain no
   requirement for out-of-range HSV; history locates the expression at
   `3810e5380`.

Classification: **proven defect**.

### Audio Spectrum one-pixel interpolation

1. **Exact invariant:** the supported `1×1` matrix returns three finite HSV
   floats.
2. **Trigger:** fresh `1×1` context with
   `{low:.45, mid:.32, high:.18}`.
3. **Pre-repair wrong output:** the exact `git show HEAD:` reproduction returns
   `[0,NaN,NaN]`; the full harness reports
   `audiospectrum.js dimension 1x1: non-finite value at 1`; focused output
   serializes the two non-finite entries as `[0,null,null]`. The pre-repair
   helper is at
   `8c3285a795c4bd2b7460af435bae00961e864824:resources/rgbscripts/hsvutil.js:77-90`
   and Audio Spectrum requests one pixel at
   `resources/rgbscripts/audiospectrum.js:96-121`.
4. **Grounded expected output:** a one-sample evenly spaced target selects the
   first source sample, consistent with the helper's stated NumPy interpolation
   contract and a normalized one-element target grid. The final branch is at
   `resources/rgbscripts/hsvutil.js:77-91`.
5. **Regression:** the `1×1` public `rgbMap()` dimension case fails before and
   must pass after repair.
6. **Conflicting-intent search:** the helper has only Audio Spectrum and Split
   Tower callers; docs/tests and `git log -S'HSVUtil.interpolate'` expose no
   exception for one-pixel matrices.

Classification: **proven defect**.

### Audio Water periodic hue

1. **Exact invariant:** emitted hue floats stay in `[0,1]`.
2. **Trigger:** fresh `7×11` context, default properties, and
   `{low:.93, mid:.74, high:.61}`.
3. **Pre-repair wrong output:** the exact `git show HEAD:` reproduction reports
   frame 0/index 0 as
   `-0.23260000348091125`; the raw ripple height enters a non-periodic
   expression at
   `8c3285a795c4bd2b7460af435bae00961e864824:resources/rgbscripts/audiowater.js:99-151`
4. **Grounded expected output:** use the existing periodic
   `HSVUtil.triangle()` (`resources/rgbscripts/hsvutil.js:99-108`). Pinned LedFX
   Water passes its raw ripple buffer to its periodic `triangle()`:
   https://raw.githubusercontent.com/LedFx/LedFx/091b72396bf71e151a552e416df98be28d71d784/ledfx/effects/water.py
   The final call is at `resources/rgbscripts/audiowater.js:138-151`.
5. **Regression:** the repeated peak/resize boundary cases in
   `node tests/test_audio_scripts.js` fail before and must pass after repair.
6. **Conflicting-intent search:** local helper semantics, pinned upstream code,
   docs/tests, related algorithms, and
   `git log -S'1 - 2 * Math.abs'` expose no intent for negative hue.

Classification: **proven defect**.
