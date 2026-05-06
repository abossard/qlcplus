# Phase 6 Delete + Phase 7 Verification

## Phase 6 — Delete `ledfx_compat.js`

### Checklist

| # | Item | Status | Notes |
|---|------|--------|-------|
| 1 | No bundled script uses `LedFx.*` | ✅ | Migrated 11 audio scripts to `AudioParams.bandPower(audio, name)` |
| 2 | `audio_common.js` has no LedFx dependency | ✅ | `createFilter` now uses `AudioDSP.Filter`; new `AudioParams.bandPower` provides the spectrum-thirds fallback inline |
| 3 | Preload list in `engine/src/rgbscriptv4.cpp` | ✅ | Removed `ledfx_compat.js` entry; kept `rgbutil.js`, `audiodsp.js`, `audio_common.js` |
| 4 | `resources/rgbscripts/CMakeLists.txt` | ✅ | Removed `ledfx_compat.js` from install list |
| 5 | File deleted | ✅ | `resources/rgbscripts/ledfx_compat.js` removed |
| 6 | Build succeeds | ✅ | `qlcplus-qml` rebuilt cleanly with `-Werror -Wextra -Wall` |

### Migration details

**`audio_common.js` additions** (replace LedFx fallback path):
- `AudioParams._LOG_RANGE`, `_LOW_CUT_RATIO`, `_HIGH_CUT_RATIO` — log-frequency crossover constants (40/250/2000/5000 Hz, mirrors `audiocapture.h`)
- `AudioParams._bandSplitIndices(n)` — clamped split indices for an N-bin spectrum
- `AudioParams.bandPower(audio, name)` — preferred path uses engine-provided `audio.bands[name]`; falls back to averaging the matching `audio.spectrum` slice
- `AudioParams.createFilter` now constructs an `AudioDSP.Filter` instead of `LedFx.ExpFilter` (same `update`/`updateArray` API)

**Audio script migration** (mechanical rewrite of each local `bandValue`):

The 3-line LedFx fallback
```js
if (name === "low") return LedFx.lows_power(audio);
if (name === "mid") return LedFx.mids_power(audio);
return LedFx.high_power(audio);
```
collapses to a single delegating call:
```js
return AudioParams.bandPower(audio, name);
```

Files migrated (11):
- `audioaurora.js`, `audiobasslaser.js`, `audiobuildup.js`, `audiochaser.js`,
  `audioenergy.js`, `audiofireworks.js`, `audiohueshift.js`, `audiolava.js`,
  `audioshockwave.js`, `audioshot.js`, `audiostrobe.js`

**Other touch-ups:**
- `rgbutil.js` header comment about "mechanically rename `LedFx.foo` to `RGBUtil.foo`" removed (no longer relevant — RGBUtil never carried band helpers).

## Phase 7 — Final Verification

### Audio test suites

| Suite | Result | Tests |
|-------|--------|-------|
| `audioframe_test` | ✅ PASS | 6 / 6 |
| `audioanalyzer_test` | ✅ PASS | 9 / 9 |
| `audiochannel_test` | ✅ PASS | 10 / 10 |
| `audioprofile_test` | ✅ PASS | 7 / 7 |
| `audioslice_test` | ✅ PASS | 5 / 5 |
| **Total** | **✅ 37 passed, 0 failed** | |

`audioprofile_test` emitted one `QWARN` about a destroyed event dispatcher during `testEnsureDefault` cleanup — pre-existing, unrelated to this change, test still passes.

### Remaining `LedFx` references in `resources/`, `engine/`, `qmlui/`

Only attribution / metadata strings remain — these are author credits for ports of LedFX visualizers and must stay:

```
resources/rgbscripts/audio{blocks,crawler,scan,spectrum,wavelength,plasma,
  melt,lava,equalizer,water,scroll,strobe,soap,glitch,power,energy,fire}.js
  → algo.author = "Ported from LedFx"
resources/rgbscripts/audio{equalizer,energy,fire}.js
  → header: "Original by LedFX contributors: https://github.com/LedFx/LedFx"
```

No code-level `LedFx.` symbol references remain anywhere in the runtime tree.

### Build

```
cmake .. -Dqmlui=ON     → Configuring done, Generating done
cmake --build . --target qlcplus-qml -j8
  → libqlcplusengine.dylib relinked (rgbscriptv4.cpp recompiled)
  → qlcplus-qml linked
  → 100% Built target qlcplus-qml
```

No warnings or errors.

## Overall Status

✅ **COMPLETE** — `ledfx_compat.js` is fully retired. The audio script ecosystem
now stands on three preloaded shims (`rgbutil.js`, `audiodsp.js`, `audio_common.js`)
plus the C++ `AudioChannel` / `AudioAnalyzer` snapshot fields (`audio.bands`,
`audio.volume`, `audio.triggers`). The LedFx compatibility layer is no longer
on the runtime path or the install manifest.
