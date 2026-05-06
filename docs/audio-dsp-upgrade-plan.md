# Audio DSP Modernization Plan

## Goal

Build a QLC-native, scientifically grounded audio analysis pipeline for RGB scripts and completely remove the bundled dependency on `LedFx` naming, helpers, and compatibility shims.

The final state should feel like a modern VJ/audio-reactive engine: stable in quiet rooms, responsive on club systems, musically meaningful, frame-rate independent, and inspectable when it behaves oddly.

## End State

| Area | Final direction |
| --- | --- |
| Core DSP | **C++ `AudioAnalyzer`** class in engine layer. All envelope, AGC, trigger, spectral feature computation in C++. Available to any consumer. |
| Audio Profiles | **Document-level `AudioProfile`** objects hold DSP configuration (bands, envelopes, AGC, triggers, noise gate). Multiple named profiles per project (e.g., "Kick Sensitive", "Ambient Smooth"). |
| Audio Trigger Widget | **VCAudioTrigger is the primary editor and live monitor** for Audio Profiles — not the owner of script audio config. Shows envelope curves, AGC meter, trigger state lamps, spectral features. |
| Script API | Scripts read pre-computed features from enriched `audio` object (`audio.bands.sub`, `audio.triggers.bass.fired`). JS does selection, not computation. Per-script DSP sliders removed; scripts select a profile and optionally apply lightweight mapping (intensity, band selection). |
| Shared JS files | Replace `ledfx_compat.js` with `RGBUtil` (color/map/noise helpers). No bundled script should call `LedFx.*`. |
| Visual helpers | `RGBUtil` namespace for color/map/noise. |
| Compatibility | Backward-compatible XML with schema versioning. Old per-script slider values converted to generated profile on load. Legacy per-bar triggers preserved alongside new per-band triggers. |
| Verification | Synthetic audio injection, deterministic feature tests, golden comparison tests, and live debug visualization. |

## Current Ground Truth

| Fact | Consequence |
| --- | --- |
| `rgbscriptv4.cpp` preloads `ledfx_compat.js`, then `audio_common.js` into one shared `QJSEngine`. | Loader order must change when `ledfx_compat.js` is removed. |
| 28 `audio*.js` scripts set `usesAudio = true`. | Migration must be scripted/audited, not hand-waved. |
| `rgbMap(width, height, rgb, step, audio)` receives `{ spectrum, volume, beat, bpm, maxMagnitude }`. | We already have a transport path; we should enrich it, not create a second one. |
| `audio.spectrum` is 32 log-spaced bands from 40 Hz to 5000 Hz, normalized per frame. | It is useful for spectral shape, not absolute loudness. |
| `audio.volume` is attack/release-smoothed signal power. | It is the better basis for AGC and global energy. |
| Current `LedFx.lows_power()`, `mids_power()`, `high_power()`, and `melbank_thirds()` split the log-spaced bands at log-frequency crossover ratios (~250 Hz and ~2000 Hz). | These compatibility helpers still expose the old LedFx-style API surface that the migration should remove. |
| `AudioCapture` runs on its own QThread, emits `dataProcessed(double*, int, double, quint32)`. | Analyzer must receive richer internal data, not just the signal — `dataProcessed()` lacks raw RMS/peak/FFT bins. |
| `AudioCapture` has per-consumer band tracking via `registerBandsNumber(N)` with ref-counting. | Variable-band support must be preserved for VCAudioTrigger. |
| `VCAudioTrigger` does its own normalize/smooth/threshold in C++ (`slotSpectrumDataChanged()`). | Duplicates DSP that should live in the shared `AudioAnalyzer`. |
| All RGBMatrix instances share one `QJSEngine` (`s_jsThread->engine`). | Module-level JS state aliases across scripts. Per-script state must live in C++ channels. |
| `AudioCapture` computes true RMS internally but only emits smoothed `power`. | New analyzer needs raw RMS/peak before smoothing, not after. |

## Scientific Audio Model

Treat audio analysis as a feature extraction pipeline with explicit units and stages.

### C++ Analysis Layer

Add or evolve a native analyzer around `AudioCapture` so the engine computes stable, reusable audio features once per frame.

**Critical:** The analyzer must receive raw data from inside `AudioCapture`, not just the `dataProcessed()` signal. `dataProcessed()` only emits 32 log-spaced band magnitudes plus smoothed power — this is insufficient for accurate RMS, peak, spectral centroid, rolloff, or flatness. An internal `AudioFrame` struct passes raw time-domain stats and FFT bin data.

#### Internal Frame (AudioCapture → AudioAnalyzer)

```cpp
struct AudioFrame {
    const float* mono;           // time-domain mono samples
    int sampleCount;
    const float* fftMagnitudes;  // raw FFT bin magnitudes
    int fftBins;
    int sampleRate;
    double rms;                  // raw RMS (before any smoothing)
    double peak;                 // raw peak amplitude
    quint64 frameIndex;
    double dtMs;                 // time since previous audio frame
};
```

#### Shared Features (computed once per audio frame)

| Feature | Why it matters |
| --- | --- |
| `rmsDb` | Absolute loudness in dBFS for noise gates, AGC, and confidence. Computed from raw RMS, not legacy smoothed `power`. |
| `peakDb` | Clipping/transient awareness. |
| `crestFactor` | Distinguishes punchy transients from dense sustained material. |
| `bandsLog[32]` | Existing log-spaced spectrum, exposed with clear frequency metadata. |
| `bandsDb[32]` | Spectrum in dB, useful for thresholds and calibrated gates. |
| `bandsNormalized[32]` | Visual-friendly normalized spectrum for bars and matrices. |
| `perceptualBands` | Sub/bass/lowMid/mid/high grouped from log bands. |
| `spectralFlux` | Onset strength and buildup/drop detection. |
| `spectralCentroidHz` | Brightness/timbre feature. |
| `spectralRolloffHz` | Energy distribution feature. |
| `spectralFlatness` | Noise-like vs tonal material. |
| `beat`, `bpm`, `beatConfidence`, `beatPhase` | Musically stable beat-driven effects. |
| `noiseFloorDb` | Adaptive silence/noise gating. |
| `audioDtMs` | Time since previous audio frame (for envelope/trigger timing). |

#### Per-Consumer Channels (`AudioChannel`)

Each consumer creates a channel with its own configuration for envelopes, AGC, and triggers. Channel state is owned by `AudioAnalyzer` and updated on the audio thread.

```cpp
struct AudioChannelConfig {
    EnvelopeConfig envelope;   // attackMs, releaseMs per band
    AgcConfig agc;             // maxGainDb, releaseMs, noiseFloorDb
    TriggerConfig triggers;    // thresholds, hysteresis, cooldownMs, holdMs
    BandLayout bandLayout;     // which bands to track (default: perceptual 5)
    VolumeConfig volume;       // smoothing for volume meter
};

// Handle-based API (thread-safe, no raw pointer exposure)
AudioChannelHandle handle = analyzer->createChannel(config);
handle.updateConfig(newConfig);        // atomic pending config, applied at next frame boundary
AudioSnapshot snap = handle.snapshot(); // short copy, lock-free or short read lock
handle.close();                        // safe deferred unregister
```

**Snapshot** (immutable value object, returned per channel):

| Field | Meaning |
| --- | --- |
| `bands` | Smoothed, gain-adjusted perceptual bands (sub/bass/lowMid/mid/high + aliases). |
| `spectrum` | Processed spectrum for bars, waves, and matrices. |
| `triggers` | Per-band trigger state: `value`, `active`, `firedThisFrame`, `releasedThisFrame`, `heldMs`, `cooldownRemainingMs`. |
| `volume` | Raw, smoothed, normalized, and AGC-adjusted loudness. |
| `music` | BPM, beat phase, beat confidence, bar phase. |
| `audioDtMs` | Audio frame delta for envelope timing. |

#### Trigger State Machine

Triggers follow a precise state machine to avoid ambiguity across consumers:

| Field | Semantics |
| --- | --- |
| `value` | Current smoothed input value (0..1). |
| `active` | True while above low threshold (Schmitt hysteresis). |
| `firedThisFrame` | True on the frame where `active` transitions from false → true. Frame-stable (not consumed on read). |
| `releasedThisFrame` | True on the frame where `active` transitions from true → false. |
| `heldMs` | How long `active` has been true continuously. |
| `cooldownRemainingMs` | Time until next `firedThisFrame` can occur. |

### JS Layer (minimal)

JS scripts read pre-computed features. No DSP computation in JS.

| API | Purpose |
| --- | --- |
| `RGBUtil.rgb(r, g, b)` | Color packing (replacing `LedFx.rgb`). |
| `RGBUtil.hsv2rgb(h, s, v)` | HSV conversion. |
| `RGBUtil.createMap(w, h)` | Pixel map allocation. |
| `RGBUtil.interpolate(a, b, t)` | Linear interpolation. |
| `RGBUtil.simplex2d(x, y)` | Simplex noise. |
| `RGBUtil.noiseField2d(...)` | Noise field generation. |
| `AudioDSP.Filter(decay, rise)` | Optional JS-side ExpFilter matching `LedFx.ExpFilter` shape for scripts that need per-pixel smoothing. |

Scripts access pre-computed audio features directly:

```javascript
function rgbMap(width, height, rgb, step, audio) {
    var sub = audio.bands.sub;
    var fired = audio.triggers.bass.firedThisFrame;
    var vol = audio.volume.agc;
    // ... use values directly, no DSP needed
}
```

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
| `AudioParams.adaptiveGain(algo, spectrum)` | It used RMS of `audio.spectrum`, but QLC+ normalizes `audio.spectrum` per frame, so the value mostly describes spectral shape rather than real loudness. | C++ `AudioChannel` AGC using raw `rmsDb` from `AudioFrame`, noise floor, and capped gain. |
| `AudioParams.logScaleBands(spectrum)` | It applies linear-index resampling over the source band array. With QLC+'s already log-spaced spectrum, that cannot re-run AudioCapture's Hz-based log-frequency banding formula for a new band count. | C++ `AudioAnalyzer` perceptual band grouping with verified frequency ranges. |
| `AudioParams.frameNormalizedDecay(decayMs, frameMs)` | It returned an interpolation alpha, not a decayed value. The name invited misuse in ported scripts. | C++ `AudioChannel` envelope smoothing using `audioDtMs` and `alpha = 1 - exp(-dt/tau)`. |
| `AudioParams.softSaturate(value, threshold)` | It could return values above `1.0`, which is ambiguous for normalized brightness helpers. | C++ soft-knee compression in `AudioChannel` with documented output range `[0, 1]`. |
| `AudioParams.hysteresisTrigger(algo, state, value)` | It used static thresholds and returned only gate state, not one-shot edges. | C++ trigger state machine in `AudioChannel` with Schmitt hysteresis, hold, cooldown, `firedThisFrame`, and `active`. |

Keep `AudioParams` focused on existing UI parameter plumbing until the new `AudioDSP` API lands.


## Design Decisions (from four rounds of critique)

These decisions were resolved after structured critique: Opus 4.7 (plan v1), GPT 5.5 (C++ DSP review), Opus 4.7 (AudioTrigger-as-hub), and GPT 5.5 (AudioProfile decoupling).

### DD1. Audio DSP configuration lives in document-level Audio Profiles

Audio Profiles are the configuration abstraction. They are document-model objects (like Functions or FixtureGroups) — they exist independent of the Virtual Console. VCAudioTrigger is the **primary editor and live monitor** for profiles, not the owner.

Multiple named profiles per project (e.g., "Default", "Kick Sensitive", "Ambient Smooth"). Functions reference profiles by ID. Profiles can exist without visible VC widgets.

### DD2. VCAudioTrigger is the Audio Control Center UI

VCAudioTrigger edits and monitors Audio Profiles. It gains: perceptual band editor, envelope monitor, AGC meter, trigger state lamps, spectral feature readouts. Multiple AudioTrigger widgets can exist — each monitors/edits a profile.

### DD3. Core DSP in C++ `AudioAnalyzer`, configured by Audio Profiles

Envelope, AGC, trigger, and spectral feature computation lives in a C++ `AudioAnalyzer` in the engine layer. Each Audio Profile owns one `AudioChannelHandle`. The analyzer is consumer-agnostic; profiles are the configuration owner.

### DD4. AudioAnalyzer receives internal `AudioFrame`, not `dataProcessed()` signal

`dataProcessed(double*, int, double, quint32)` only emits 32 log-spaced band magnitudes plus smoothed power — insufficient for accurate RMS, peak, centroid, rolloff, or flatness. The analyzer receives an internal `AudioFrame` (raw mono samples, FFT bins, raw RMS, peak, sample rate, dtMs) directly from inside `AudioCapture::processData()`.

### DD5. Functions reference Audio Profiles, not VC widgets

`RGBMatrix` has an `audioProfileId` property — NOT a widget ID. This preserves the architectural boundary: Functions are VC-independent, runnable headless, importable across projects. Resolution chain:

1. **Explicit reference**: `audioProfileId` set → use that profile's channel.
2. **Default profile**: If no explicit reference, use the profile flagged `isDefault`.
3. **First found**: If no default flagged, use the lowest-ID profile.
4. **Anonymous fallback**: If no profiles exist, the analyzer creates an internal default channel with sensible defaults.

The fallback is a safety net, not the main UX. When an audio script is first added and no profile exists, auto-create a "Default Audio" profile in the document.

### DD6. Perceptual bands (sub/bass/lowMid/mid/high) replace low/mid/high

Five perceptual bands plus convenience aliases (`low = sub+bass`, `mid` unchanged, `high` unchanged) for backward compatibility. Band edges are configurable in the profile/widget UI with verified defaults. Legacy `lowCutBin`/`highCutBin` preserved as read-only derived values.

### DD7. Per-script DSP sliders removed; lightweight mapping controls remain

`AudioParams` no longer exposes `gain`, `reactivity`, `floor`, `sensitivity` as DSP controls. Instead, the RGBMatrix editor shows:

- **Audio Profile selector**: dropdown of available profiles (+ "Create new…")
- **Intensity scale**: lightweight post-DSP brightness multiplier (0–200%)
- **"Edit Audio Profile…"** button: opens the AudioTrigger/Profile editor

Old per-script slider values are **converted to a generated profile** on XML load (not silently ignored). Deprecated stubs log a one-shot warning for one release cycle.

### DD8. Per-consumer state via AudioChannel handles, atomic config updates

Each Audio Profile holds an `AudioChannelHandle`. Config changes from QML are queued via `handle.updateConfig(newConfig)` and applied at the next audio frame boundary. State is owned by `AudioAnalyzer`, exposed via immutable `AudioSnapshot` value objects.

### DD9. Two `dtMs` values

- `audioDtMs`: time since previous audio frame (~23ms at 43Hz). Used for envelope/AGC/trigger timing in C++.
- `consumerDtMs`: time since previous consumer frame (MasterTimer tick). Available to JS for visual animation pacing.

### DD10. Don't layer on legacy smoothed `power`

New analyzer computes raw RMS/peak from `AudioFrame` and applies its own documented smoothing. Legacy `volume` preserved for backward compat but not used as input to new AGC.

### DD11. Legacy per-bar triggers preserved alongside new per-band triggers

Two trigger systems coexist:

- **Per-bar triggers** (legacy): per-spectrum-bar DMX/Function/Widget actions with min/max thresholds.
- **Per-band triggers** (new): perceptual-band Schmitt-hysteresis triggers with hold/cooldown, consumed by scripts via `audio.triggers.*`.

Old bar triggers are not collapsed into five bands. Users opt into new trigger model by editing the profile.

### DD12. Trigger state machine — frame-stable, not consumed-on-read

Triggers expose `value`, `active`, `firedThisFrame`, `releasedThisFrame`, `heldMs`, `cooldownRemainingMs`. All fields are frame-stable so multiple consumers can read the same state.

### DD13. Backward-compatible XML with schema versioning

Audio Profile XML uses a `Version` attribute. Old documents load with default profile configs. Schema version stored in XML — no hidden "has edited" tracking state. Old per-script slider values converted to generated profile on load.

### DD14. AudioAnalyzer on audio thread with budget constraint

No heap allocation per frame, fixed-size arrays. Budget <1ms per channel. Instrumented from day one.

### DD15. Golden tests gate the migration

Golden tests capture old VCAudioTrigger output for deterministic inputs. New pipeline must match within tolerance for legacy defaults.

### DD16. Pilot scripts before full port

Port 2–3 representative scripts and validate visual parity before full migration.

### DD17. Keep `ExpFilter` shape for mechanical migration

`AudioDSP.Filter(decay, rise)` in JS matches `LedFx.ExpFilter` for per-pixel smoothing.

### DD18. Phase 2 split for risk reduction

VCAudioTrigger evolution split into: backend swap (parity), persistence model, new UI panels, script integration readiness.

---

## Implementation Phases

### Phase 0: Foundation

- [ ] Audit `AudioCapture::processData()` — identify insertion point for `AudioFrame` data.
- [ ] Audit `AudioParams` — list DSP vs non-DSP properties for removal.
- [ ] Inventory all 28 `audio*.js` scripts — tag AudioParams usage for impact analysis.
- [ ] Fix docs/comments calling spectrum linear or band cuts fixed.
- [ ] Confirm VCAudioTrigger XML round-trips so Version attribute can be added safely.
- [ ] Design `AudioProfile` document-model class: ID, name, isDefault, channel config, XML schema.

### Phase 1: C++ AudioAnalyzer + AudioChannel

- [ ] Create `AudioFrame` internal struct (mono samples, FFT bins, raw RMS, peak, sample rate, `audioDtMs`, frame index).
- [ ] Modify `AudioCapture::processData()` to populate `AudioFrame` and pass to `AudioAnalyzer` directly.
- [ ] Create `AudioAnalyzer` with shared features: 32 log bands, `bandsDb`, `bandsNormalized`, `perceptualBands`, `rmsDb`, `peakDb`, `crestFactor`, `spectralFlux`, `spectralCentroidHz`, `spectralRolloffHz`, `spectralFlatness`, `noiseFloorDb`, beat features.
- [ ] Define `AudioChannelConfig` (envelope per band, AGC, triggers, band layout, volume smoothing, noise gate).
- [ ] Implement handle-based API: `createChannel(config)`, `updateConfig()`, `snapshot()`, `close()`.
- [ ] Implement per-channel processing: envelopes, AGC, triggers (Schmitt + hold + cooldown).
- [ ] Define `AudioSnapshot` immutable value object.
- [ ] Build synthetic audio test harness — inject `AudioFrame` directly.
- [ ] Unit test shared features: silence, noise, sweep, impulse, ramp, varying intervals.
- [ ] Unit test per-channel: envelopes, AGC, trigger state machine.
- [ ] Instrument per-frame time per channel; assert <1ms.
- [ ] Implement anonymous default channel fallback.

### Phase 2A: Audio Profiles in Document Model

- [ ] Create `AudioProfile` class: ID, name, isDefault, `AudioChannelConfig`, `AudioChannelHandle`.
- [ ] Register `AudioProfile` in `Doc` (map, create/delete/lookup by ID).
- [ ] Implement XML load/save with Version attribute and children: `<Bands>`, `<Envelope>`, `<Agc>`, `<Triggers>`, `<NoiseGate>`.
- [ ] Implement auto-creation of "Default Audio" profile when first audio script is added.
- [ ] Implement migration: old per-script slider values → generated profile on XML load.
- [ ] Unit test: profile creation, config round-trip, migration from old XML.

### Phase 2B: VCAudioTrigger Backend Swap

- [ ] Associate VCAudioTrigger with an Audio Profile (create or select).
- [ ] Replace internal `slotSpectrumDataChanged()` normalize/smooth/threshold with `handle.snapshot()` reads.
- [ ] Keep existing per-bar DMX/Function/Widget trigger behavior (legacy path).
- [ ] Golden tests: deterministic inputs produce expected bar values, trigger fires, DMX writes matching old behavior.
- [ ] Expose profile resolution to `Doc` for script engine.

### Phase 2C: VCAudioTrigger New UI Panels

- [ ] **Bands panel**: band edge editors with frequency labels, presets (default/strict/wide).
- [ ] **Envelope panel**: per-band attack/release sliders (ms), live mini-graph of envelope vs raw level.
- [ ] **AGC panel**: max gain (dB), release (ms), noise floor (dB), live gain meter.
- [ ] **Triggers panel**: per-band Schmitt thresholds, hold/cooldown ms, live state lamp, fires/sec counter.
- [ ] **Spectral features panel**: live centroid, rolloff, flatness, flux readouts.
- [ ] **Runtime monitor strip**: compact live view (envelope curves, AGC gain, trigger lamps).
- [ ] Keep existing bars panel and per-bar config.

### Phase 3: Wire RGBScript to Audio Profiles

- [ ] Implement profile resolution in `rgbscriptv4.cpp` per DD5: explicit → default → first → anonymous. Log once per script start.
- [ ] Add `audioProfileId` property on `RGBMatrix` (saved to function XML).
- [ ] Surface profile selector in RGBMatrix editor: dropdown + "Create new…" + intensity scale + "Edit Audio Profile…".
- [ ] Update `buildAudioDataObject()` to read from `AudioSnapshot`: `audio.bands.*`, `audio.triggers.*`, `audio.volume.*`, `audio.music.*`, `audio.features.*`, `audio.audioDtMs`, `audio.consumerDtMs`.
- [ ] Keep legacy fields (`spectrum`, `volume`, `beat`, `bpm`, `maxMagnitude`) for compat.
- [ ] Strip DSP from `AudioParams`. Deprecated stubs with one-shot warning.
- [ ] Remove per-script audio slider UI from RGBMatrix editor.
- [ ] **Thin vertical slice**: prove end-to-end with ONE script before proceeding (AudioCapture → Analyzer → Profile → VCAudioTrigger monitor → RGBMatrix → enriched audio → script reads bands/triggers).

### Phase 4: RGBUtil + Non-Audio JS Cleanup

- [ ] Add `RGBUtil`: `rgb`, `hsv2rgb`, `createMap`, `interpolate`, `simplex2d`, `noiseField2d`.
- [ ] Verify `RGBUtil.rgb()` byte order matches engine pixel format.
- [ ] Add `AudioDSP.Filter(decay, rise)` matching `LedFx.ExpFilter` for JS per-pixel smoothing.
- [ ] Keep temporary `LedFx` shim during transition.
- [ ] Update `audio_common.js` to drop DSP helpers.

### Phase 5: Port Bundled Scripts (simpler — no per-script DSP)

Scripts become thin: read pre-computed values, decide visuals.

- [ ] **Pilot checkpoint**: port 3 representative scripts (trigger, blend, spectrum) and visually compare before continuing.

| Pattern | Scripts | Main migration |
| --- | --- | --- |
| Trigger-first | `audiostrobe`, `audioshot`, `audiobasslaser`, `audioshockwave` | `audio.triggers.*.firedThisFrame` and `active`. |
| Three-band blend | `audioaurora`, `audiochaser`, `audioenergy`, `audiolava`, `audiofireworks`, `audiohueshift` | `audio.bands.*` from C++ perceptual groups. |
| Single low-energy | `audiomelt`, `audioplasma`, `audiosoap`, `audiotunnel`, `audiovortex`, `audioscan`, `audiocrawler`, `audioglitch` | `audio.bands.sub`/`bass`. |
| Spectrum visuals | `audiospectrum`, `audioequalizer`, `audiosplittower`, `audiowavelength`, `audiopower`, `audiofire`, `audioscroll`, `audioblocks` | `audio.spectrum` from channel. |
| State machine | `audiobuildup` | `audio.features.flux` + triggers. |
| Spatial sim | `audiowater` | Perceptual bands + `audioDtMs`. |

- [ ] Port trigger-first scripts.
- [ ] Port three-band blend scripts.
- [ ] Port single low-energy driver scripts.
- [ ] Port spectrum visual scripts.
- [ ] Port state machine script.
- [ ] Port spatial simulation script.
- [ ] Replace `LedFx.*` → `RGBUtil.*` / `audio.bands.*` / `audio.triggers.*` / `AudioDSP.Filter`.

### Phase 6: Delete `ledfx_compat.js`

- [ ] `rg "LedFx\." resources/rgbscripts` returns no usage.
- [ ] `audio_common.js` has no `LedFx` dependency.
- [ ] `rgbscriptv4.cpp` no longer preloads `ledfx_compat.js`.
- [ ] CMakeLists no longer installs it.
- [ ] Docs updated. Shim removed. File deleted.

### Phase 7: Verification

- [ ] All synthetic audio tests pass:

| Test | Expected signal |
| --- | --- |
| Silence | Features near zero, AGC clamps, no trigger chatter. |
| White noise | High flatness, no false beat. |
| Sine sweeps | Energy in expected perceptual band. |
| Kick impulse | `triggers.bass.firedThisFrame` once, cooldown prevents chatter. |
| Hat impulse | `triggers.high.firedThisFrame` without bass. |
| Quiet→loud ramp | AGC adapts smoothly. |
| 20ms vs 60ms frames | Envelopes decay by `audioDtMs`. |
| Threshold hover | Schmitt holds cleanly. |
| Multiple profiles | Independent snapshots from same audio. |
| Profile resolution | Explicit → default → first → anonymous chain works. |
| Old-XML round-trip | Pre-upgrade docs load/save; old slider values converted to profile. |

- [ ] Run parameterized synthetic tests.
- [ ] Run multi-profile and resolution tests.
- [ ] Run XML round-trip and migration tests.
- [ ] Run VCAudioTrigger golden tests.
- [ ] Confirm frame budget <1ms with multiple channels.
- [ ] Confirm runtime monitor updates smoothly.
- [ ] Live test: 3+ ported scripts, varied music.
- [ ] Document migration in release notes.

---

## Done Criteria

- [ ] Document-level `AudioProfile` objects hold all DSP config.
- [ ] VCAudioTrigger is primary editor/monitor with envelope curves, AGC meter, trigger lamps, spectral readouts.
- [ ] One `AudioChannelHandle` per profile; multiple profiles coexist.
- [ ] C++ `AudioAnalyzer` computes shared features and per-channel state from `AudioFrame`.
- [ ] Scripts resolve audio via profile chain and log the source.
- [ ] `RGBMatrix` exposes `audioProfileId` with profile selector + intensity scale + "Edit Audio Profile…" in UI.
- [ ] Per-script DSP sliders removed. Old values converted to profile on load. Deprecated stubs warn.
- [ ] No bundled script computes own DSP. No script references `LedFx.*`.
- [ ] `ledfx_compat.js` deleted. `RGBUtil` + `AudioDSP.Filter` available.
- [ ] Audio Profile XML versioned. Legacy per-bar triggers preserved alongside new per-band triggers.
- [ ] Envelopes use `audioDtMs`. Triggers frame-stable.
- [ ] All tests pass. Frame budget <1ms. Docs updated. Release notes written.
