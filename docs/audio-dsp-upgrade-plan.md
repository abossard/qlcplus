# Audio DSP Modernization Plan

## Goal

Build a QLC-native, scientifically grounded audio analysis pipeline for RGB scripts and completely remove the bundled dependency on `LedFx` naming, helpers, and compatibility shims.

The final state should feel like a modern VJ/audio-reactive engine: stable in quiet rooms, responsive on club systems, musically meaningful, frame-rate independent, and inspectable when it behaves oddly.

## End State

| Area | Final direction |
| --- | --- |
| Shared JS files | Replace `ledfx_compat.js` with QLC-owned helpers. No bundled script should call `LedFx.*`. |
| Audio DSP | Central `AudioDSP` API in `audio_common.js`, backed by documented C++ audio features where possible. |
| Visual helpers | Move generic color/map/noise helpers into a QLC namespace such as `RGBUtil`, not `LedFx`. |
| Script API | Scripts consume `audio` plus `AudioDSP.process()`, not raw third-split helpers. |
| Compatibility | A temporary migration shim is acceptable inside the branch, but it should not be the final shipped architecture. |
| Verification | Synthetic audio fixtures, deterministic feature tests, and live debug visualization. |

## Current Ground Truth

| Fact | Consequence |
| --- | --- |
| `rgbscriptv4.cpp` preloads `ledfx_compat.js`, then `audio_common.js` into one shared `QJSEngine`. | Loader order must change when `ledfx_compat.js` is removed. |
| 28 `audio*.js` scripts set `usesAudio = true`. | Migration must be scripted/audited, not hand-waved. |
| `rgbMap(width, height, rgb, step, audio)` receives `{ spectrum, volume, beat, bpm, maxMagnitude }`. | We already have a transport path; we should enrich it, not create a second one. |
| `audio.spectrum` is 32 log-spaced bands from 40 Hz to 5000 Hz, normalized per frame. | It is useful for spectral shape, not absolute loudness. |
| `audio.volume` is attack/release-smoothed signal power. | It is the better basis for AGC and global energy. |
| Current `LedFx.lows_power()`, `mids_power()`, `high_power()`, and `melbank_thirds()` split the log bands into equal thirds. | These helpers are the main primitive math to remove. |

## Scientific Audio Model

Treat audio analysis as a feature extraction pipeline with explicit units and stages.

### C++ Analysis Layer

Add or evolve a native analyzer around `AudioCapture` so the engine computes stable, reusable audio features once per frame.

| Feature | Why it matters |
| --- | --- |
| `rmsDb` | Absolute loudness in dBFS for noise gates, AGC, and confidence. |
| `peakDb` | Clipping/transient awareness. |
| `crestFactor` | Distinguishes punchy transients from dense sustained material. |
| `bandsLog` | Existing log-spaced spectrum, exposed with clear frequency metadata. |
| `bandsDb` | Spectrum in dB, useful for thresholds and calibrated gates. |
| `bandsNormalized` | Visual-friendly normalized spectrum for bars and matrices. |
| `spectralFlux` | Onset strength and buildup/drop detection. |
| `spectralCentroidHz` | Brightness/timbre feature. |
| `spectralRolloffHz` | Energy distribution feature. |
| `spectralFlatness` | Noise-like vs tonal material. |
| `beat`, `bpm`, `beatConfidence`, `beatPhase` | Musically stable beat-driven effects. |
| `noiseFloorDb` | Adaptive silence/noise gating. |

### JS DSP Layer

Use `audio_common.js` for script-facing convenience, state, envelopes, and trigger semantics.

| API | Purpose |
| --- | --- |
| `AudioDSP.process(algo, audio, options)` | One per-frame call returning the processed envelope. |
| `AudioDSP.extractBands(audio)` | Convert log bands into `sub`, `bass`, `lowMid`, `mid`, `high`, plus `low/mid/high` aliases. |
| `AudioDSP.envelope(algo, key, value, attackMs, releaseMs)` | Time-constant attack/release smoothing. |
| `AudioDSP.agc(algo, audio, options)` | Volume-aware adaptive gain with noise floor, slow release, and capped boost. |
| `AudioDSP.trigger(algo, key, value, options)` | Dynamic threshold, Schmitt hysteresis, cooldown, hold, and rising-edge output. |
| `AudioDSP.compress(value, options)` | Soft knee compression for visual dynamic range. |
| `AudioDSP.saturate(value, curve)` | Soft clipping instead of harsh peak clipping. |
| `AudioDSP.interpolateSpectrum(audio, size, options)` | Spectrum interpolation without `LedFx.melbank()`. |
| `AudioDSP.debug(algo, env)` | Optional debug payload for monitor scripts/panels. |

`AudioDSP.process()` should return a stable object:

| Field | Meaning |
| --- | --- |
| `raw` | Raw or engine-provided features. |
| `bands` | Smoothed, gain-adjusted perceptual bands. |
| `spectrum` | Processed spectrum for bars, waves, and matrices. |
| `triggers` | `active` and `fired` states for `sub`, `bass`, `mid`, `high`, `volume`, `beat`, and optional `onset`. |
| `volume` | Raw, smoothed, normalized, and AGC-adjusted loudness. |
| `music` | BPM, beat phase, beat confidence, and bar phase when available. |
| `dtMs` | Clamped wall-clock delta for frame-rate-independent behavior. |

## Perceptual Bands

The existing 32 QLC+ spectrum bands are already logarithmic. Group them by musical purpose rather than equal thirds.

| Group | Index range | Approximate intent |
| --- | --- | --- |
| `sub` | `0..8` | Kick fundamental, sub pressure, low-end movement. |
| `bass` | `9..12` | Bass body and low toms. |
| `lowMid` | `13..18` | Warmth, body, mud. |
| `mid` | `19..25` | Vocals, synth body, snare body. |
| `high` | `26..31` | Hats, clap edge, snare snap, brightness. |

These ranges should be verified against generated sine sweeps and then encoded as named constants, not hidden magic numbers.

## Math Standards

| Area | Requirement |
| --- | --- |
| Time constants | Use `alpha = 1 - exp(-dtMs / tauMs)`. No frame-count fade math in final scripts. |
| AGC | Use dB or volume envelope, not per-frame normalized spectrum RMS. Include max gain, release time, and noise gate. |
| Triggers | Use adaptive baseline, Schmitt hysteresis, hold time, and refractory/cooldown. Return one-shot and gated states separately. |
| Onsets | Use positive spectral flux with adaptive threshold and minimum interval. |
| Compression | Use soft-knee compression or saturating curves before mapping to brightness. |
| Silence | Gate low-confidence frames using `volume`, `rmsDb`, or `maxMagnitude`, so noise does not become visuals. |
| Units | Store constants as ms, dB, Hz, or normalized `0..1`; avoid unlabeled slider math. |

## Removed Draft Helpers

The exploratory helpers previously added to `AudioParams` were removed before any bundled script depended on them.

| Removed helper | Why it was removed | Replacement direction |
| --- | --- | --- |
| `AudioParams.adaptiveGain(algo, spectrum)` | It used RMS of `audio.spectrum`, but QLC+ normalizes `audio.spectrum` per frame, so the value mostly describes spectral shape rather than real loudness. | Implement `AudioDSP.agc(algo, audio, options)` using `audio.volume`, future `rmsDb`, noise floor, and capped gain. |
| `AudioParams.logScaleBands(spectrum)` | It assumed linear FFT bins, while C++ already provides log-spaced bands. The chosen ranges also made low too narrow and high too broad. | Implement `AudioDSP.extractBands(audio)` with named constants based on verified log-band frequency ranges. |
| `AudioParams.frameNormalizedDecay(decayMs, frameMs)` | It returned an interpolation alpha, not a decayed value. The name invited misuse in ported scripts. | Implement explicit helpers such as `AudioDSP.alpha(dtMs, tauMs)` and `AudioDSP.decayToward(value, target, dtMs, tauMs)`. |
| `AudioParams.softSaturate(value, threshold)` | It could return values above `1.0`, which is ambiguous for normalized brightness helpers. | Implement `AudioDSP.compress()` and `AudioDSP.saturate()` with documented output ranges. |
| `AudioParams.hysteresisTrigger(algo, state, value)` | It used static thresholds and returned only gate state, not one-shot edges. | Implement `AudioDSP.trigger()` with adaptive baseline, Schmitt thresholds, hold, cooldown, `active`, and `fired`. |

Keep `AudioParams` focused on existing UI parameter plumbing until the new `AudioDSP` API lands.

## Implementation Phases

### Phase 0: Remove Wrong Assumptions

1. Keep the removed draft helpers out of `AudioParams`.
2. Fix any remaining comments or docs that call the QLC+ spectrum linear.
3. Add `AudioDSP` beside `AudioParams` and move new audio math there.
4. Base `AudioDSP` on the verified model: log-spaced spectrum for shape, `audio.volume` or future `rmsDb` for loudness, and explicit time constants for envelopes.

### Phase 1: Create QLC-Native JS Helpers

1. Add `RGBUtil` for non-audio utilities currently living under `LedFx`:
   - `RGBUtil.rgb()`
   - `RGBUtil.hsv2rgb()`
   - `RGBUtil.createMap()`
   - `RGBUtil.interpolate()`
   - `RGBUtil.simplex2d()`
   - `RGBUtil.noiseField2d()`
2. Add `AudioFilter` or `AudioDSP.Filter` to replace `LedFx.ExpFilter`.
3. Add `AudioDSP.process()` and the band/envelope/trigger helpers.
4. Keep a temporary `LedFx` shim only while scripts are being ported.

### Phase 2: Modernize the Engine Audio Object

Extend the JS `audio` object in `rgbscriptv4.cpp` without breaking the current fields.

| Field | Shape |
| --- | --- |
| `audio.version` | Numeric feature schema version. |
| `audio.frameMs` | Analysis frame duration or elapsed capture frame duration. |
| `audio.frequencies` | Center or boundary frequencies for the 32 log bands. |
| `audio.spectrum` | Existing normalized spectrum, kept during migration. |
| `audio.spectrumDb` | dB-scaled bands. |
| `audio.volume` | Existing normalized smoothed power. |
| `audio.rmsDb` | RMS loudness in dBFS. |
| `audio.features` | Centroid, rolloff, flatness, flux, crest factor, confidence. |
| `audio.music` | Beat, BPM, confidence, beat phase. |

Keep the old fields until all bundled scripts are ported. The new fields allow scientific analysis without overloading `spectrum`.

### Phase 3: Port Bundled Scripts Off `LedFx`

Port by behavior pattern, not alphabetically.

| Pattern | Scripts | Main migration |
| --- | --- | --- |
| Trigger-first | `audiostrobe`, `audioshot`, `audiobasslaser`, `audioshockwave` | Use `env.triggers.*.fired` and `active`, not raw threshold checks. |
| Three-band blend | `audioaurora`, `audiochaser`, `audioenergy`, `audiolava`, `audiofireworks`, `audiohueshift` | Use `env.bands` and QLC perceptual groups. |
| Single low-energy driver | `audiomelt`, `audioplasma`, `audiosoap`, `audiotunnel`, `audiovortex`, `audioscan`, `audiocrawler`, `audioglitch` | Replace low-power calls with `env.bands.sub/bass/low`. |
| Spectrum visuals | `audiospectrum`, `audioequalizer`, `audiosplittower`, `audiowavelength`, `audiopower`, `audiofire`, `audioscroll`, `audioblocks` | Use `AudioDSP.interpolateSpectrum()` and time-based peak falloff. |
| Advanced state machine | `audiobuildup` | Keep spectral flux/state machine idea, but feed it engine flux, calibrated bands, and modern triggers. |
| Spatial simulation | `audiowater` | Keep simulation, replace equal-third bands and frame decay. |

Every port should replace:

| Old | New |
| --- | --- |
| `LedFx.rgb(...)` | `RGBUtil.rgb(...)` |
| `LedFx.hsv2rgb(...)` | `RGBUtil.hsv2rgb(...)` |
| `LedFx.createMap(...)` | `RGBUtil.createMap(...)` |
| `LedFx.noiseField2d(...)` | `RGBUtil.noiseField2d(...)` |
| `LedFx.lows_power(audio)` | `env.bands.low` or `env.bands.sub/bass` |
| `LedFx.mids_power(audio)` | `env.bands.mid` or `env.bands.lowMid/mid` |
| `LedFx.high_power(audio)` | `env.bands.high` |
| `LedFx.melbank(audio, n)` | `AudioDSP.interpolateSpectrum(audio, n)` |
| `new LedFx.ExpFilter(...)` | `AudioDSP.envelope(...)` or `new AudioDSP.Filter(...)` |

### Phase 4: Delete `ledfx_compat.js`

Delete the file only when the search results are clean.

Checklist:

1. `rg "LedFx\." resources/rgbscripts` returns no bundled script usage.
2. `audio_common.js` has no `LedFx` dependency.
3. `rgbscriptv4.cpp` no longer preloads `ledfx_compat.js`.
4. `resources/rgbscripts/CMakeLists.txt` no longer installs `ledfx_compat.js`.
5. Any docs that mention LedFx helpers are updated to QLC-native names.
6. The migration shim is removed before finalizing the feature branch.

### Phase 5: Verification and Tuning

Use objective tests before subjective live tuning.

| Test | Expected signal |
| --- | --- |
| Silence | Features stay near zero, no trigger chatter. |
| White noise | High flatness, no false beat dominance. |
| Single sine sweeps | Energy lands in the expected band group as frequency rises. |
| Kick-like impulse | Sub/bass onset fires once, cooldown prevents chatter. |
| Hat-like impulse | High onset fires without bass trigger. |
| Quiet-to-loud ramp | AGC adapts smoothly without pumping. |
| 20 ms vs 60 ms frames | Envelopes decay by wall-clock time, not frame count. |
| Threshold hover | Schmitt trigger holds state cleanly. |

Add a temporary analyzer/debug RGB script or panel that displays:

- raw 32 bands
- grouped bands
- volume/rms
- AGC gain
- beat/onset state
- trigger fired/active states

This gives fast live verification without flooding logs.

## Done Criteria

1. No bundled script references `LedFx.*`.
2. `ledfx_compat.js` is deleted from source, build install lists, and the preload list.
3. Audio scripts use `AudioDSP` and `RGBUtil` only.
4. Audio feature math is documented with units and time constants.
5. Frame-rate-dependent decays are gone from ported scripts.
6. Triggers use hysteresis/cooldown or the C++ beat signal intentionally.
7. Synthetic tests cover bands, AGC, envelopes, and triggers.
8. Live verification confirms responsiveness and stability on controlled audio.
