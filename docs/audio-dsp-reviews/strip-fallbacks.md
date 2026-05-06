# Strip old-engine fallbacks — new C++ DSP engine only

## Goal

Forward-only: every audio-reactive RGB script now assumes the C++ DSP engine
(`AudioChannel` + `AudioProfile`) is always present. All JS-side fallbacks,
duplicate trigger detection, and deprecated DSP helpers have been removed.

## What was removed

### `audio_common.js` — deleted entirely

| Function                            | Replaced by                                                       |
|-------------------------------------|-------------------------------------------------------------------|
| `AudioParams.bandPower(a, n)`       | `audio.bands.{low,mid,high,sub,bass,lowMid}` (engine-provided)    |
| `AudioParams.gainFactor(algo)`      | Gain configured per-channel in `AudioProfile`                     |
| `AudioParams.createFilter(...)`     | `new AudioDSP.Filter(decay, AudioParams.filterRise(algo))` direct |
| `AudioParams.adaptiveGain(...)`     | `AudioChannel::updateAgc()` → `audio.volume.{agc,normalized}`     |
| `AudioParams.hysteresisTrigger(...)`| `AudioChannel::updateTriggers()` → `audio.triggers.<name>.*`      |
| `AudioParams.frameNormalizedDecay`  | Time-correct smoothing in C++ using `audio.audioDtMs`             |
| `AudioParams.softSaturate(v, knee)` | No longer needed — values clamped/curved in C++                   |
| `AudioParams.logScaleBands(...)`    | Engine emits log-spaced spectrum directly                         |

### Slider registrations removed

`installContinuous` and `installTrigger` previously pushed
`presetGain` / `presetReactivity` / `presetFloor` / `presetSensitivity`
sliders into `algo.properties` and bound setters/getters. All four DSP
sliders are gone — the underlying knobs are now configured globally on the
`AudioProfile` (gain) or no longer needed (script-side reactivity stays as
an implementation detail). The functions remain as pure default-seeders so
the small set of helpers below keep working.

### Helpers kept

| Helper                              | Used by (count)                                                |
|-------------------------------------|----------------------------------------------------------------|
| `AudioParams.installContinuous`     | 21 scripts (seeds `presetReactivity`, `presetFloor`)           |
| `AudioParams.installTrigger`        | 5 scripts (seeds `presetReactivity`, `presetSensitivity`)      |
| `AudioParams.filterRise(algo)`      | 16 scripts — `AudioDSP.Filter` rise-time tuning                |
| `AudioParams.applyFloor(algo, b)`   | 19 scripts — visual brightness floor                           |
| `AudioParams.triggerThreshold(algo)`| 4 scripts — script-side flux/onset thresholds                  |

`audio.spectrum`, `audio.volume` (number form), `audio.beat`, `audio.bpm`,
`audio.maxMagnitude` are still emitted by `buildAudioDataObject()` because
`audiospectrum.js` and `audioequalizer.js` consume them. No fallback code
paths remain; they are first-class engine fields.

## Per-script changes

Pattern A — **`bandValue`/`legacyBand` helper removed, calls rewritten to
`audio.bands.<name>`**:

- `audioaurora.js`, `audioblocks.js`, `audiochaser.js`, `audiocrawler.js`,
  `audioenergy.js`, `audiofire.js`, `audioglitch.js`, `audiohueshift.js`,
  `audiolava.js`, `audiomelt.js`, `audioplasma.js`, `audiopower.js`,
  `audioscan.js`, `audioscroll.js`, `audiosoap.js`, `audiotunnel.js`,
  `audiovortex.js`, `audiowater.js`

Pattern B — **trigger fallbacks (`triggerFired`, `volumeActive`, `legacyBand`)
removed; calls rewritten to `audio.triggers.<name>.firedThisFrame` /
`audio.triggers.volume.active`**:

- `audioshot.js`, `audiostrobe.js`

Pattern C — **duplicate JS-side onset detection removed** (the
`(value - prevValue) > threshold` arming and `algo.prevBass/Mids/Highs/LowsFast`
state). Script now relies solely on `audio.triggers.*.firedThisFrame`:

- `audiobasslaser.js` — dropped `prevBass`, `triggerThreshold`-flux compare
- `audiofireworks.js` — dropped `prevBass/Mids/Highs`, three flux compares
- `audioshockwave.js` — dropped `prevBass`, flux compare
- `audiobuildup.js`  — dropped `prevLowsFast` flux check from drop detection;
  still uses C++ bass trigger for the drop arm

Pattern D — **no script-level changes**, only benefit from the slimmer
`AudioParams`:

- `audioequalizer.js`, `audiospectrum.js`, `audiosplittower.js`,
  `audiowavelength.js`

## Verification

```bash
# Syntax — all 28 scripts pass
for f in resources/rgbscripts/audio*.js; do node --check "$f"; done

# No fallback symbols anywhere
rg 'bandValue|legacyBand|triggerFired|triggerActive|volumeActive' resources/rgbscripts/   # 0 hits
rg 'bandPower|gainFactor|createFilter|adaptiveGain|hysteresisTrigger|softSaturate|logScaleBands|frameNormalizedDecay' \
   resources/rgbscripts/audio_common.js   # 0 hits

# No DSP slider registrations
rg 'name:presetGain|name:presetReactivity|name:presetFloor|name:presetSensitivity' resources/rgbscripts/   # 0 hits

# Build is clean
(cd build && cmake --build . --target qlcplus-qml -j8)
```

## Consequences

- `audio_common.js` shrank from 270 → 49 lines.
- Scripts no longer paper over a missing engine — if `audio.bands` /
  `audio.triggers` are ever absent the script will throw, surfacing the
  regression instead of hiding behind silent fallbacks.
- DSP tuning is now a single source of truth: `AudioProfile` in C++.
  Per-script reactivity/floor/sensitivity knobs that survived are pure
  visual tuning, not DSP.
