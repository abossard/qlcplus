# p3 Vertical Slice + p4 audio_common.js Cleanup

## Part 1 — Vertical-slice integration test (`engine/test/audioslice/`)

Added `AudioSlice_Test` covering the full audio pipeline:

```
AudioFrame -> AudioAnalyzer -> AudioChannel -> AudioSnapshot
                                            -> (buildAudioDataObject -> JS audio object)
```

### Tests

| Test | What it covers |
|---|---|
| `testEndToEndPipeline` | Synthetic 1 kHz / -20 dBFS sine -> `analyzer.processFrame(frame)` decorates the frame (rmsDb, centroid, bands32) and updates the channel; the resulting `AudioSnapshot` has non-zero perceptual bands (mid > 0), feature values mirror the frame, all triggers expose finite non-negative values, and no trigger spuriously fires on the first non-silent frame. |
| `testProfileResolutionChain` | `Doc::audioProfileForFunction()` returns the bound `AudioProfile` for an `RGBMatrix` whose `audioProfileId` is set, falls back to the default profile when the function id is `Function::invalidId()`, when the id is unknown, and when the matrix's profile id is reset to `AudioProfile::invalidId()`. |
| `testLegacyFieldsPreserved` | Source-inspects `engine/src/rgbscriptv4.cpp` to assert that `spectrum`, `volume`, `beat`, `bpm`, and `maxMagnitude` are still set unconditionally in `buildAudioDataObject()` before the `if (channel == NULL)` early return — i.e. the legacy contract holds even when no `AudioChannel` is bound. |

### `buildAudioDataObject()` direct-call gap (documented)

The JS-object build step depends on the per-thread static `QJSEngine` that
`RGBScript::evaluate()` wires up the first time a script runs (`s_jsThread`).
Spinning that up from a unit test would require either re-implementing the
script-cache + JS-thread initialization or pulling in the entire
`rgbmatrix_test` fixture, which would obscure rather than reinforce the
slice. The slice test therefore stops at the `AudioSnapshot` boundary and
verifies the legacy-field contract via source inspection. Coverage of the
full JS object payload remains the responsibility of the higher-level
`rgbmatrix_test` suite.

### Verification

```bash
cd build && cmake .. -Dqmlui=ON \
  && cmake --build . --target audioslice_test -j8 \
  && ./engine/test/audioslice/audioslice_test
```

Result: **5 passed, 0 failed** (3 test slots + init/cleanup).

## Part 2 — `resources/rgbscripts/audio_common.js`

Added `// DEPRECATED:` markers (no behavioural changes — scripts in transition
still call these helpers):

| Symbol | Status before | Action |
|---|---|---|
| `AudioParams.gainFactor` | present | inline `// DEPRECATED: gain now controlled by AudioProfile` |
| `AudioParams.filterRise` | present | inline `// DEPRECATED: reactivity now controlled by AudioProfile` |
| `AudioParams.createFilter` | present | inline `// DEPRECATED: use AudioDSP.Filter for per-pixel smoothing only` |
| `AudioParams.adaptiveGain` | present (task said "already removed, verify") | **NOT removed** in tree — added DEPRECATED note in JSDoc pointing at `AudioChannel::updateAgc` / `audio.volume.agc` |
| `AudioParams.hysteresisTrigger` | present (task said "already removed, verify") | **NOT removed** in tree — added DEPRECATED note in JSDoc pointing at `AudioChannel::updateTriggers` / `audio.triggers.*` |
| `AudioParams.frameNormalizedDecay` | present (task said "already removed, verify") | **NOT removed** in tree — added DEPRECATED note in JSDoc pointing at `AudioChannelConfig` ms-based smoothing |

> Discrepancy with task brief: the three "already removed" helpers are still
> present in `audio_common.js`. They were not stripped during prior work, so
> we kept them in place (per the explicit "Do NOT remove the functions yet"
> instruction) and instead annotated them as deprecated with pointers to the
> C++ replacements. Removal can land in a follow-up once all bundled scripts
> stop calling them.

`AudioParams.installContinuous()` and `AudioParams.installTrigger()` are
unchanged: they still register the `presetGain` / `presetReactivity` /
`presetFloor` / `presetSensitivity` UI properties that bundled scripts read
during the transition. `applyFloor`, `triggerThreshold`, `logScaleBands`,
and `softSaturate` are also untouched — they are not duplicated by the C++
pipeline.
