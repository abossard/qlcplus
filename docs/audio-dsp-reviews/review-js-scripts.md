
# Review — JS helpers and audio RGB script ports

## Summary

No unconditional blocking issues found. The ports are broadly consistent:

- `node --check` passed for `resources/rgbscripts/audio*.js` and `rgbutil.js`.
- No remaining `LedFx.` calls were found in `resources/rgbscripts/`.
- No ported scripts still call `AudioParams.gainFactor()` or `AudioParams.createFilter()`.
- `rgbutil.js` matches the deleted `LedFx` visual helper implementations, including `0xRRGGBB` byte order.
- `audiodsp.js` matches the old `LedFx.ExpFilter` implementation exactly.
- Preload order in `engine/src/rgbscriptv4.cpp` is correct: `rgbutil.js`, then `audiodsp.js`, then `audio_common.js`.

The main risk is not syntax or missing migration; it is **runtime compatibility if `audio.bands` / `audio.triggers` are not guaranteed to exist on every audio frame**.

---

## Blocking Issues

None found that are unconditional.

---

## Non-Blocking Issues

### 1. Several scripts directly dereference `audio.bands.*` without a fallback or `audio.bands` guard

**Severity:** Non-Blocking, potentially Blocking if legacy audio snapshots can still reach scripts.

**Impact:**  
If `audio.spectrum` exists but the newer `audio.bands` object is missing, these scripts can throw at runtime instead of falling back to spectrum-derived band power.

Affected scripts with direct `audio.bands.*` and no `!audio.bands` guard:

| Script | Lines / usage |
|---|---|
| `audioblocks.js` | `audio.bands.mid`, `audio.bands.high`, `audio.bands.low` around lines 101-104 |
| `audiofire.js` | `audio.bands.low` around line 129 |
| `audiopower.js` | `audio.bands.low` around line 75 |
| `audioscroll.js` | `audio.bands.low/mid/high` around lines 116-118 |

`audiowater.js`, `audiocrawler.js`, `audioglitch.js`, `audiomelt.js`, `audioplasma.js`, `audioscan.js`, `audiosoap.js`, `audiotunnel.js`, and `audiovortex.js` do guard `!audio.bands`, but they return a blank map rather than using the fallback path available via `AudioParams.bandPower()`.

**Why this matters:**  
`AudioParams.bandPower(audio, name)` already supports both:

1. new API: `audio.bands[name]`
2. fallback: derived average from `audio.spectrum`

So direct `audio.bands.*` usage loses the compatibility behavior that `audio_common.js` was designed to provide.

**Recommended fix:**  
For scripts that need low/mid/high power, prefer:

- `AudioParams.bandPower(audio, "low")`
- `AudioParams.bandPower(audio, "mid")`
- `AudioParams.bandPower(audio, "high")`

or at least add explicit `audio.bands` guards before dereferencing.

---

### 2. Reactivity is still applied in JS even though `AudioParams.filterRise()` is marked deprecated

**Severity:** Non-Blocking.

**Impact:**  
`audio_common.js` says:

```js
filterRise: function(algo) { return 0.1 + algo.presetReactivity * 0.09; }, // DEPRECATED: reactivity now controlled by AudioProfile
```

But many scripts still use `AudioParams.filterRise(algo)` or otherwise use `algo.presetReactivity` directly for motion/smoothing.

Examples:

| Script | Usage |
|---|---|
| `audioblocks.js` | `new AudioDSP.Filter(0.1, AudioParams.filterRise(algo))` |
| `audiopower.js` | `new AudioDSP.Filter(0.1, AudioParams.filterRise(algo))` |
| `audiofire.js` | `new AudioDSP.Filter(0.05, AudioParams.filterRise(algo))` |
| `audiobuildup.js` | helper creates `AudioDSP.Filter(baseDecay, AudioParams.filterRise(algo))` |
| `audioplasma.js`, `audiovortex.js`, `audiotunnel.js` | use both filter rise and `presetReactivity` in animation speed |

**Why this matters:**  
If the C++ `AudioProfile` now owns reactivity/smoothing, these scripts may still apply an additional JS-side reactivity layer. That may be intentional for per-effect motion, but it conflicts with the deprecation comment and makes the migration semantics unclear.

**Recommended fix:**  
Clarify the intended split:

- If JS reactivity should remain an effect-local visual control, remove or soften the “deprecated” comment for `filterRise()`.
- If C++ `AudioProfile` should fully own reactivity, migrate these scripts away from `AudioParams.filterRise()` and decide what to do with the still-exposed JS `presetReactivity` property.

---

### 3. Gain UI remains installed but no scripts consume `presetGain`

**Severity:** Non-Blocking.

**Impact:**  
All scripts still use `AudioParams.installContinuous()` or `AudioParams.installTrigger()`, which add a `Gain` property. But no ported script calls `AudioParams.gainFactor()` anymore.

This avoids double-applying gain, which is good. However, unless the C++ `AudioProfile` is wired to these per-script `presetGain` properties, the script-level “Gain” control is now likely a no-op.

**Recommended fix:**  
Decide whether script-local Gain should remain visible:

- If gain is now exclusively controlled by C++ `AudioProfile`, remove or hide the JS `Gain` property from `AudioParams.installContinuous()` / `installTrigger()`.
- If backwards-compatible UI is required, document that the JS Gain slider is deprecated/no-op.
- If script-local gain is still supposed to affect visuals, route it intentionally rather than leaving `presetGain` unused.

---

## Suggestions

### 1. Add parity tests for `RGBUtil` and `AudioDSP.Filter`

**Severity:** Suggestion.

**Impact:**  
The implementations appear correct, but they are compatibility shims. Small future edits could silently break visual parity.

**Recommended fix:**  
Add lightweight JS parity tests for:

#### `RGBUtil.rgb()`

- clamp below 0
- clamp above 255
- rounding behavior
- byte order: expected packed value is `0xRRGGBB`

#### `RGBUtil.hsv2rgb()`

- hue wrapping
- primary colors
- negative hue

#### `RGBUtil.interpolate()`

- empty input
- same-size input
- single-value input
- 2-to-N interpolation

#### `AudioDSP.Filter`

- first scalar update
- scalar rise vs decay
- first array update
- array length change
- empty array update
- `NaN` propagation, if current behavior is intentional

---

## RGBUtil parity review

`resources/rgbscripts/rgbutil.js` matches the deleted `LedFx` visual helpers reviewed in `docs/audio-dsp-reviews/p4-rgbutil.md`.

Reviewed helpers:

| Helper | Result |
|---|---|
| `RGBUtil.rgb()` | Matches `LedFx.rgb`; packs `(r << 16) | (g << 8) | b`, i.e. `0xRRGGBB` |
| `RGBUtil.hsv2rgb()` | Matches `LedFx.hsv2rgb` |
| `RGBUtil.createMap()` | Matches `LedFx.createMap`; shape is `map[y][x]` |
| `RGBUtil.interpolate()` | Matches `LedFx.interpolate` |
| `RGBUtil.simplex2d()` | Matches `LedFx.simplex2d` |
| `RGBUtil.noiseField2d()` | Matches `LedFx.noiseField2d` |

No byte-order issue found.

---

## AudioDSP.Filter review

`resources/rgbscripts/audiodsp.js` is an exact behavioral match for the old `LedFx.ExpFilter`.

### Edge cases

| Case | Current behavior | Assessment |
|---|---|---|
| Initial scalar value | First value is assigned directly | Matches `ExpFilter` |
| Initial array value | First array is copied element-by-element | Matches `ExpFilter` |
| Empty array | Returns `[]` and stores `[]` | Acceptable / parity-preserving |
| Array length change | Resets stored value to new array | Matches `ExpFilter` |
| `null` scalar input | Stores `null`; next update still treats filter as uninitialized | Matches old behavior, but worth testing if null can occur |
| `NaN` scalar input | Stores/propagates `NaN` | Matches normal JS math behavior |
| `NaN` array item | Propagates `NaN` for that item | Matches normal JS math behavior |

No mismatch found.

---

## Preload order review

`engine/src/rgbscriptv4.cpp` preloads shims in this order:

```cpp
const QStringList shimNames = {
    QStringLiteral("rgbutil.js"),
    QStringLiteral("audiodsp.js"),
    QStringLiteral("audio_common.js")
};
```

This is correct.

`audio_common.js` references `AudioDSP.Filter`, so `audiodsp.js` must load before it. That requirement is satisfied. `rgbutil.js` also loads before scripts use `RGBUtil`.

---

## Missing migration review

No remaining `LedFx.` calls were found in `resources/rgbscripts/`.

No remaining references found to:

- `LedFx.avg()`
- `LedFx.melbank()`
- `LedFx.melbank_thirds()`
- `LedFx.lows_power()`
- `LedFx.mids_power()`
- `LedFx.high_power()`
- `LedFx.beat_oscillator()`
- `LedFx.bar_oscillator()`

The functions listed in `docs/audio-dsp-reviews/p0-inventory-scripts.md` appear to have been migrated.

---

## Deprecated helper review

No ported script still calls:

- `AudioParams.gainFactor()`
- `AudioParams.createFilter()`

So I did not find a direct double-gain problem from old `gainFactor()` usage.

Remaining deprecated-ish concern: many scripts still use `AudioParams.filterRise()` or `algo.presetReactivity`, despite comments saying reactivity is now C++ `AudioProfile` controlled.

---

## Trigger field review

Scripts that use `audio.triggers.*` mostly guard the field before dereferencing.

Reviewed trigger usages:

| Script | Result |
|---|---|
| `audiobasslaser.js` | Guarded |
| `audiobuildup.js` | Guarded |
| `audiofireworks.js` | Guarded |
| `audioshockwave.js` | Guarded |
| `audioshot.js` | Guarded |
| `audiostrobe.js` | Guarded |
| `audiopower.js` | Uses `audio.triggers && audio.triggers.beat...`; safe because earlier code already checks `audio` |

No unsafe `audio.triggers.bass.firedThisFrame`-style access found.

---

## Syntax check

Command run:

```bash
cd /Users/abossard/Desktop/projects/qlcplus &&
for f in resources/rgbscripts/audio*.js resources/rgbscripts/rgbutil.js; do
  node --check "$f" || exit 1
done
```

Result: passed.

---

## Per-script review notes

| Script | Findings |
|---|---|
| `audioaurora.js` | No issue found |
| `audiobasslaser.js` | No issue found; trigger access guarded |
| `audioblocks.js` | Direct `audio.bands.*` without `audio.bands` fallback/guard |
| `audiobuildup.js` | No migration issue found; still uses JS reactivity/filter smoothing |
| `audiochaser.js` | No issue found |
| `audiocrawler.js` | Uses direct `audio.bands.low`; guarded with `!audio.bands` but no spectrum fallback |
| `audioenergy.js` | No issue found |
| `audioequalizer.js` | No issue found |
| `audiofire.js` | Direct `audio.bands.low` without `audio.bands` fallback/guard |
| `audiofireworks.js` | No issue found; trigger access guarded |
| `audioglitch.js` | Uses direct `audio.bands.low`; guarded with `!audio.bands` but no spectrum fallback |
| `audiohueshift.js` | No issue found |
| `audiolava.js` | No issue found |
| `audiomelt.js` | Uses direct `audio.bands.low`; guarded with `!audio.bands` but no spectrum fallback |
| `audioplasma.js` | Uses direct `audio.bands.low`; guarded with `!audio.bands` but no spectrum fallback |
| `audiopower.js` | Direct `audio.bands.low` without `audio.bands` fallback/guard |
| `audioscan.js` | Uses direct `audio.bands.low`; guarded with `!audio.bands` but no spectrum fallback |
| `audioscroll.js` | Direct `audio.bands.low/mid/high` without `audio.bands` fallback/guard |
| `audioshockwave.js` | No issue found; trigger access guarded |
| `audioshot.js` | No issue found; trigger access guarded |
| `audiosoap.js` | Uses direct `audio.bands.low`; guarded with `!audio.bands` but no spectrum fallback |
| `audiospectrum.js` | No issue found |
| `audiosplittower.js` | No issue found |
| `audiostrobe.js` | No issue found; trigger access guarded |
| `audiotunnel.js` | Uses direct `audio.bands.low`; guarded with `!audio.bands` but no spectrum fallback |
| `audiovortex.js` | Uses direct `audio.bands.low`; guarded with `!audio.bands` but no spectrum fallback |
| `audiowater.js` | Uses direct `audio.bands.low/mid/high`; guarded with `!audio.bands` but no spectrum fallback |
| `audiowavelength.js` | No issue found |