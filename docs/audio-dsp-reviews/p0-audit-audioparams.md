# P0 Audit: `AudioParams` DSP vs non-DSP properties

Source audited: `resources/rgbscripts/audio_common.js`.

## Summary

- `AudioParams` currently contains only shared audio-response/DSP controls and DSP helpers.
- No script-specific visual/mode property is defined inside `AudioParams`; those live in each `audio*.js` script and should be retained there.
- Registration happens through `AudioParams.installContinuous(algo, defaults)` or `AudioParams.installTrigger(algo, defaults)`, which appends range properties to `algo.properties`.
- If an old RGBMatrix has no saved property value, the JS initializer value remains active and the property getter returns that default. Saved XML values are cached in `RGBMatrix::m_properties` and replayed through each script property writer when the script starts.

## `AudioParams` properties

| Property | Installed by | Classification | Current default | Range | Current formula/meaning | Migration notes |
| --- | --- | --- | --- | --- | --- | --- |
| `presetGain` | `installContinuous`, `installTrigger` | DSP; remove from script UI | `defaults.gain || 5` | `1..10` | `gainFactor = 0.6 + presetGain * 0.2`; range maps to `0.8..2.6`, default `1.6`. | Generated `AudioProfile` should store the old multiplier exactly. If the profile stores dB, use `20 * log10(0.6 + value * 0.2)`. |
| `presetReactivity` | `installContinuous`, `installTrigger` | DSP/envelope attack; remove from script UI | `defaults.reactivity || 5` | `1..10` | `filterRise = 0.1 + presetReactivity * 0.09`; range maps to `0.19..1.0`, default `0.55`. Used as `LedFx.ExpFilter` rise alpha. Some scripts also use `presetReactivity / 10` for motion response. | Generated `AudioProfile` should preserve `attackAlpha = 0.1 + value * 0.09` for legacy matching. If converted to attack time, derive from alpha at the engine audio tick rate and keep script templates for direct motion-response uses. |
| `presetFloor` | `installContinuous` only | DSP/response floor per task scope; remove from script UI | `defaults.floor || 0` | `0..100` | `applyFloor(brightness) = floor + (1 - floor) * brightness`, where `floor = presetFloor / 100`. | Generated `AudioProfile` should store `brightnessFloor = value / 100.0` or equivalent output-floor config. Only `audiosplittower.js` opts into non-zero default (`15`). |
| `presetSensitivity` | `installTrigger` only | DSP/trigger threshold; remove from script UI | `defaults.sensitivity || 5` | `1..10` | `triggerThreshold = 0.45 - presetSensitivity * 0.04`; range maps to `0.41..0.05`, default `0.25`. `audiobuildup.js` additionally uses `presetSensitivity / 10` for build/drop thresholds. | Generated trigger profiles should preserve threshold with `threshold = 0.45 - value * 0.04`. For `audiobuildup.js`, also preserve build thresholds `buildEnter = lerp(0.65, 0.35, value/10)`, `peak = lerp(0.80, 0.55, value/10)`. |

## `AudioParams` functions

| Function | Purpose | Classification | Default/range affected | Scripts calling it | Migration notes |
| --- | --- | --- | --- | --- | --- |
| `installContinuous(algo, defaults)` | Registers continuous audio controls: Gain, Reactivity, Floor; installs setters/getters. | DSP registration helper; remove/replace | Gain `defaults.gain || 5`, `1..10`; Reactivity `defaults.reactivity || 5`, `1..10`; Floor `defaults.floor || 0`, `0..100` | `audioaurora.js`, `audioblocks.js`, `audiochaser.js`, `audiocrawler.js`, `audioenergy.js`, `audioequalizer.js`, `audiofire.js`, `audioglitch.js`, `audiohueshift.js`, `audiolava.js`, `audiomelt.js`, `audioplasma.js`, `audiopower.js`, `audioscan.js`, `audioscroll.js`, `audiosoap.js`, `audiospectrum.js`, `audiosplittower.js`, `audiotunnel.js`, `audiovortex.js`, `audiowater.js`, `audiowavelength.js` | Stop pushing these properties. On XML load, convert any saved values to a generated/selected `AudioProfile`. |
| `installTrigger(algo, defaults)` | Registers trigger-style audio controls: Gain, Reactivity, Sensitivity; installs setters/getters. | DSP registration helper; remove/replace | Gain `defaults.gain || 5`, `1..10`; Reactivity `defaults.reactivity || 5`, `1..10`; Sensitivity `defaults.sensitivity || 5`, `1..10` | `audiobasslaser.js`, `audiobuildup.js`, `audiofireworks.js`, `audioshockwave.js`, `audioshot.js`, `audiostrobe.js` | Stop pushing these properties. Convert saved values to profile gain/envelope/trigger config. |
| `gainFactor(algo)` | Converts gain slider to linear multiplier. | DSP; remove from scripts | Uses `presetGain`; `0.8..2.6` | All 28 bundled `audio*.js` scripts | Replace with profile-normalized audio values, or apply the generated profile gain before exposing `audio` data. |
| `filterRise(algo)` | Converts reactivity slider to filter rise alpha. | DSP/envelope; remove from scripts | Uses `presetReactivity`; `0.19..1.0` | Direct external calls: none. Indirect via `createFilter`. | Move to `AudioProfile` envelope attack config. |
| `applyFloor(algo, brightness)` | Applies floor brightness to normalized visual intensity. | Response-floor DSP per task scope; remove from scripts | Uses `presetFloor`; `0.0..1.0` floor ratio | 22 continuous scripts: `audioaurora.js`, `audioblocks.js`, `audiochaser.js`, `audiocrawler.js`, `audioenergy.js`, `audioequalizer.js`, `audiofire.js`, `audioglitch.js`, `audiohueshift.js`, `audiolava.js`, `audiomelt.js`, `audioplasma.js`, `audiopower.js`, `audioscan.js`, `audioscroll.js`, `audiosoap.js`, `audiospectrum.js`, `audiosplittower.js`, `audiotunnel.js`, `audiovortex.js`, `audiowater.js`, `audiowavelength.js` | Preserve as profile output floor or move to a retained script visual intensity parameter if product direction changes. |
| `triggerThreshold(algo)` | Converts sensitivity slider to trigger threshold. | DSP trigger; remove from scripts | Uses `presetSensitivity`; `0.41..0.05` | `audiobasslaser.js`, `audiofireworks.js`, `audioshockwave.js`, `audioshot.js`, `audiostrobe.js` | Move to profile trigger threshold. |
| `createFilter(algo, baseDecay)` | Creates `LedFx.ExpFilter(baseDecay, AudioParams.filterRise(algo))`. | DSP/envelope; remove from scripts | Rise from `presetReactivity`; decay supplied by script | 23 scripts: `audioaurora.js`, `audiobasslaser.js`, `audioblocks.js`, `audiobuildup.js`, `audiochaser.js`, `audiocrawler.js`, `audioenergy.js`, `audioequalizer.js`, `audiofire.js`, `audiofireworks.js`, `audioglitch.js`, `audiohueshift.js`, `audiolava.js`, `audiomelt.js`, `audioplasma.js`, `audiopower.js`, `audioscan.js`, `audioshockwave.js`, `audiosoap.js`, `audiospectrum.js`, `audiotunnel.js`, `audiovortex.js`, `audiowavelength.js` | Create equivalent C++ per-channel envelope filters; preserve script-specific release/base decay as profile template defaults. |
| `logScaleBands(spectrum, numBands)` | Re-bins spectrum into a requested number of log-spaced-ish bands by fractional overlap. | DSP spectral transform; remove/move to C++ | `numBands` runtime argument | No bundled audio script calls it. | Replace with C++ analyzer/profile band layouts. |
| `adaptiveGain(algo, value)` | JS AGC tracking recent peak in `algo._agcPeak`. | DSP AGC; remove/move to C++ | Internal peak starts at `0.1`, min peak `0.05`, decay `0.995/0.005` | No bundled audio script calls it. | Implement AGC in `AudioProfile`/`AudioAnalyzer`; no script migration required unless external scripts use it. |
| `softSaturate(value, knee)` | Soft-clips normalized values above a knee. | DSP dynamics helper; remove/move to C++ or RGBUtil only if visual-only use emerges | Caller-provided `knee`; clamps to `0..1` | No bundled audio script calls it. | Not needed for bundled scripts today. If kept, it belongs in analyzer dynamics or a generic math utility, not per-script DSP controls. |
| `hysteresisTrigger(algo, key, value, onThreshold, offThreshold)` | Schmitt trigger with state on `algo._hyst_<key>`. | DSP trigger state; remove/move to C++ | Caller-provided thresholds | No bundled audio script calls it. | Implement trigger state machine in `AudioProfile` channels. |
| `frameNormalizedDecay(decay, fps)` | Converts per-frame decay tuned at 25fps to another fps. | DSP timing helper; remove/move to C++ | Caller-provided decay/fps | No bundled audio script calls it. | Use `audioDtMs`/time-constant based C++ envelopes instead of frame-based JS decay. |

## Scripts using each DSP property

### `presetGain`

All 28 bundled audio scripts install/use gain via `AudioParams.gainFactor`:

| Script | Installer | Default slider | Generated profile value |
| --- | --- | ---: | --- |
| `audioaurora.js` | continuous | 3 | gain multiplier `1.2` |
| `audiobasslaser.js` | trigger | 7 | gain multiplier `2.0` |
| `audioblocks.js` | continuous | 7 | gain multiplier `2.0` |
| `audiobuildup.js` | trigger | 6 | gain multiplier `1.8` |
| `audiochaser.js` | continuous | 5 | gain multiplier `1.6` |
| `audiocrawler.js` | continuous | 5 | gain multiplier `1.6` |
| `audioenergy.js` | continuous | 5 | gain multiplier `1.6` |
| `audioequalizer.js` | continuous | 5 | gain multiplier `1.6` |
| `audiofire.js` | continuous | 3 | gain multiplier `1.2` |
| `audiofireworks.js` | trigger | 7 | gain multiplier `2.0` |
| `audioglitch.js` | continuous | 5 | gain multiplier `1.6` |
| `audiohueshift.js` | continuous | 7 | gain multiplier `2.0` |
| `audiolava.js` | continuous | 5 | gain multiplier `1.6` |
| `audiomelt.js` | continuous | 5 | gain multiplier `1.6` |
| `audioplasma.js` | continuous | 3 | gain multiplier `1.2` |
| `audiopower.js` | continuous | 7 | gain multiplier `2.0` |
| `audioscan.js` | continuous | 5 | gain multiplier `1.6` |
| `audioscroll.js` | continuous | 5 | gain multiplier `1.6` |
| `audioshockwave.js` | trigger | 7 | gain multiplier `2.0` |
| `audioshot.js` | trigger | 5 | gain multiplier `1.6` |
| `audiosoap.js` | continuous | 5 | gain multiplier `1.6` |
| `audiospectrum.js` | continuous | 7 | gain multiplier `2.0` |
| `audiosplittower.js` | continuous | 7 | gain multiplier `2.0` |
| `audiostrobe.js` | trigger | 5 | gain multiplier `1.6` |
| `audiotunnel.js` | continuous | 5 | gain multiplier `1.6` |
| `audiovortex.js` | continuous | 5 | gain multiplier `1.6` |
| `audiowater.js` | continuous | 5 | gain multiplier `1.6` |
| `audiowavelength.js` | continuous | 5 | gain multiplier `1.6` |

### `presetReactivity`

All 28 bundled audio scripts install reactivity. Most consume it through `createFilter`; these scripts also read `algo.presetReactivity` directly: `audioaurora.js`, `audiocrawler.js`, `audioenergy.js`, `audioglitch.js`, `audiomelt.js`, `audioplasma.js`, `audiosoap.js`, `audiotunnel.js`, `audiovortex.js`.

| Script | Installer | Default slider | Generated profile value |
| --- | --- | ---: | --- |
| `audioaurora.js` | continuous | 1 | attack alpha `0.19` |
| `audiobasslaser.js` | trigger | 7 | attack alpha `0.73` |
| `audioblocks.js` | continuous | 7 | attack alpha `0.73` |
| `audiobuildup.js` | trigger | 8 | attack alpha `0.82` |
| `audiochaser.js` | continuous | 5 | attack alpha `0.55` |
| `audiocrawler.js` | continuous | 5 | attack alpha `0.55` |
| `audioenergy.js` | continuous | 5 | attack alpha `0.55` |
| `audioequalizer.js` | continuous | 5 | attack alpha `0.55` |
| `audiofire.js` | continuous | 9 | attack alpha `0.91` |
| `audiofireworks.js` | trigger | 7 | attack alpha `0.73` |
| `audioglitch.js` | continuous | 3 | attack alpha `0.37` |
| `audiohueshift.js` | continuous | 7 | attack alpha `0.73` |
| `audiolava.js` | continuous | 5 | attack alpha `0.55` |
| `audiomelt.js` | continuous | 5 | attack alpha `0.55` |
| `audioplasma.js` | continuous | 2 | attack alpha `0.28` |
| `audiopower.js` | continuous | 8 | attack alpha `0.82` |
| `audioscan.js` | continuous | 5 | attack alpha `0.55` |
| `audioscroll.js` | continuous | 5 | attack alpha `0.55` |
| `audioshockwave.js` | trigger | 7 | attack alpha `0.73` |
| `audioshot.js` | trigger | 5 | attack alpha `0.55` |
| `audiosoap.js` | continuous | 5 | attack alpha `0.55` |
| `audiospectrum.js` | continuous | 7 | attack alpha `0.73` |
| `audiosplittower.js` | continuous | 7 | attack alpha `0.73` |
| `audiostrobe.js` | trigger | 5 | attack alpha `0.55` |
| `audiotunnel.js` | continuous | 5 | attack alpha `0.55` |
| `audiovortex.js` | continuous | 5 | attack alpha `0.55` |
| `audiowater.js` | continuous | 5 | attack alpha `0.55` |
| `audiowavelength.js` | continuous | 5 | attack alpha `0.55` |

### `presetFloor`

Only continuous scripts install floor. All continuous scripts use it through `applyFloor`.

| Script | Default slider | Generated profile value |
| --- | ---: | --- |
| `audioaurora.js` | 0 | floor ratio `0.00` |
| `audioblocks.js` | 0 | floor ratio `0.00` |
| `audiochaser.js` | 0 | floor ratio `0.00` |
| `audiocrawler.js` | 0 | floor ratio `0.00` |
| `audioenergy.js` | 0 | floor ratio `0.00` |
| `audioequalizer.js` | 0 | floor ratio `0.00` |
| `audiofire.js` | 0 | floor ratio `0.00` |
| `audioglitch.js` | 0 | floor ratio `0.00` |
| `audiohueshift.js` | 0 | floor ratio `0.00` |
| `audiolava.js` | 0 | floor ratio `0.00` |
| `audiomelt.js` | 0 | floor ratio `0.00` |
| `audioplasma.js` | 0 | floor ratio `0.00` |
| `audiopower.js` | 0 | floor ratio `0.00` |
| `audioscan.js` | 0 | floor ratio `0.00` |
| `audioscroll.js` | 0 | floor ratio `0.00` |
| `audiosoap.js` | 0 | floor ratio `0.00` |
| `audiospectrum.js` | 0 | floor ratio `0.00` |
| `audiosplittower.js` | 15 | floor ratio `0.15` |
| `audiotunnel.js` | 0 | floor ratio `0.00` |
| `audiovortex.js` | 0 | floor ratio `0.00` |
| `audiowater.js` | 0 | floor ratio `0.00` |
| `audiowavelength.js` | 0 | floor ratio `0.00` |

### `presetSensitivity`

Only trigger scripts install sensitivity. Five scripts use it through `triggerThreshold`; `audiobuildup.js` reads it directly for build/drop thresholds.

| Script | Default slider | Generated profile value |
| --- | ---: | --- |
| `audiobasslaser.js` | 7 | trigger threshold `0.17` |
| `audiobuildup.js` | 6 | trigger threshold `0.21`; build enter `0.47`; peak `0.65` |
| `audiofireworks.js` | 7 | trigger threshold `0.17` |
| `audioshockwave.js` | 7 | trigger threshold `0.17` |
| `audioshot.js` | 5 | trigger threshold `0.25` |
| `audiostrobe.js` | 5 | trigger threshold `0.25` |

## Non-DSP script properties to retain

These are not defined by `AudioParams`; they are registered directly by each script and should remain script-owned unless separately redesigned:

| Script | Non-DSP visual/mode properties |
| --- | --- |
| `audioaurora.js` | `presetSpeed`, `presetLayers`, `presetWaveSize` |
| `audiobasslaser.js` | `presetTrailLength`, `presetMaxBeams`, `presetDirection`, `presetSpeed` |
| `audioblocks.js` | `presetBlockSize`, `presetDecay`, `presetReactTo`, `presetFill` |
| `audiobuildup.js` | `presetDropIntensity`, `presetBuildSpeed`, `presetColorScheme`, `presetAutoTune` |
| `audiochaser.js` | `presetBaseSpeed`, `presetDotCount`, `presetTrailLength`, `presetSpeedMode`, `presetBounce` |
| `audiocrawler.js` | `presetSpeed`, `presetSway`, `presetChop` |
| `audioenergy.js` | `presetMixing`, `presetMultiplier` |
| `audioequalizer.js` | `presetDecay`, `presetPeaks`, `presetCenter`, `presetGap` |
| `audiofire.js` | `presetSpeed`, `presetIntensity`, `presetCooling`, `presetDirection`, `presetSpread` |
| `audiofireworks.js` | `presetMaxParticles`, `presetGravity`, `presetOrigin`, `presetParticleSize` |
| `audioglitch.js` | `presetSpeed`, `presetSaturation`, `presetComplexity` |
| `audiohueshift.js` | `presetSpeed`, `presetWaveScale`, `presetSaturation`, `presetMinBrightness` |
| `audiolava.js` | `presetSpeed`, `presetContrast` |
| `audiomelt.js` | `presetSpeed`, `presetColorSpeed` |
| `audioplasma.js` | `presetSpeed`, `presetDensity`, `presetTwist` |
| `audiopower.js` | `presetSparks` |
| `audioscan.js` | `presetSpeed`, `presetWidth`, `presetBounce` |
| `audioscroll.js` | `presetDecay`, `presetDirection`, `presetColorMode` |
| `audioshockwave.js` | `presetMaxWaves`, `presetWaveWidth`, `presetSpeed`, `presetDecay` |
| `audioshot.js` | `presetDecay`, `presetSize`, `presetTrigger`, `presetMaxShots`, `presetColorMode` |
| `audiosoap.js` | `presetSpeed`, `presetDensity`, `presetSmooth` |
| `audiospectrum.js` | `presetMode`, `presetSmoothing` |
| `audiosplittower.js` | `presetBands`, `presetPeakHold`, `presetDecay` |
| `audiostrobe.js` | `presetDecay`, `presetMode`, `presetRandomColor` |
| `audiotunnel.js` | `presetSpeed`, `presetRings`, `presetShape` |
| `audiovortex.js` | `presetSpeed`, `presetArms`, `presetTightness` |
| `audiowater.js` | `presetSpeed`, `presetViscosity`, `presetBassSize`, `presetHighSize` |
| `audiowavelength.js` | `presetSmoothing` |

## Property registration and missing values

- Each audio script starts with `algo.properties = new Array()`.
- Scripts call `AudioParams.installContinuous(...)` or `AudioParams.installTrigger(...)` immediately after creating `algo.properties`.
- The install helpers append `type:range` property descriptors with `write:` and `read:` method names:
  - `presetGain`: `values:1,10`, `write:setGain`, `read:getGain`
  - `presetReactivity`: `values:1,10`, `write:setReactivity`, `read:getReactivity`
  - `presetFloor`: `values:0,100`, `write:setFloor`, `read:getFloor`
  - `presetSensitivity`: `values:1,10`, `write:setSensitivity`, `read:getSensitivity`
- The install helpers set JS defaults before registration. If no saved XML property exists, the getter returns that initialized default.
- If a saved XML property exists, `RGBMatrix` replays it via `RGBScript::setProperty()`, which calls the configured writer. The writer clamps to the descriptor range.
- If a saved property no longer exists after migration, `RGBMatrix::setAlgorithm()` removes it from the cached map when `script->setProperty()` returns `false`. XML-load migration should therefore convert old DSP properties before scripts stop exposing them, otherwise user-tuned values can be silently discarded on algorithm changes/save.

## Migration strategy: old slider value to generated `AudioProfile`

When loading old XML, create or select a generated profile for each RGBMatrix that has legacy audio DSP properties. Use saved values when present; otherwise use the script's installer defaults above.

| Old slider | Read fallback | Conversion for generated profile | Notes |
| --- | --- | --- | --- |
| `presetGain = v` | Script default from installer, else `5` | `inputGainLinear = 0.6 + clamp(v, 1, 10) * 0.2`; optionally `inputGainDb = 20 * log10(inputGainLinear)` | Preserves current `gainFactor()` exactly. |
| `presetReactivity = v` | Script default from installer, else `5` | `attackAlpha = 0.1 + clamp(v, 1, 10) * 0.09` | Preserves `filterRise()`/`createFilter()` attack behavior. If C++ uses milliseconds, convert this alpha at the analyzer tick rate and store the equivalent time constant. |
| `presetFloor = v` | Continuous default from installer, else `0` | `brightnessFloor = clamp(v, 0, 100) / 100.0` | Preserves `applyFloor()`. Only generated for continuous scripts. |
| `presetSensitivity = v` | Trigger default from installer, else `5` | `triggerThreshold = 0.45 - clamp(v, 1, 10) * 0.04` | Preserves `triggerThreshold()`. Only generated for trigger scripts. |
| `audiobuildup.js` sensitivity | `6` if absent | `buildEnterThreshold = lerp(0.65, 0.35, v / 10.0)` and `peakThreshold = lerp(0.80, 0.55, v / 10.0)` | `audiobuildup.js` does not call `triggerThreshold()` for its state machine; migrate these thresholds explicitly. |

Recommended generated profile naming: derive from function/script name, e.g. `Migrated Audio - <RGBMatrix name>`, to avoid merging distinct old slider settings into one profile accidentally.
