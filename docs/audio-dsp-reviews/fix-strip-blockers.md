# Fix: Strip review blockers (B1, B2) + non-blocking cleanup

## B1 — `buildAudioDataObject()` always populates v2 shape

**File:** `engine/src/rgbscriptv4.cpp` (`RGBScript::buildAudioDataObject`)

**Problem:** When no `AudioChannel` was resolved (no profile assigned, or matrix
missing) the function returned early, leaving the audio object without
`bands`, `triggers.*`, `volume.*`, `features.*`, `audioDtMs`,
`consumerDtMs`, `brightnessFloor`, or `version`. Scripts that now assume the
v2 shape (e.g. `audio.bands.low`, `audio.triggers.beat.firedThisFrame`)
would throw `TypeError: cannot read property of undefined`.

**Fix:** Replaced the early `return audioObj;` with a no-channel branch that
populates zero-valued defaults:

- `bands`: all six bands at `0.0`
- `triggers`: `sub`, `bass`, `lowMid`, `mid`, `high`, `volume`, `beat` —
  each with `value=0`, all boolean flags `false`, `heldMs=0`,
  `cooldownRemainingMs=0`
- `volume`: object form `{ raw: 0, smoothed: 0, agc: 0 }` (overwrites the
  legacy numeric `volume` set above for v2 consumers)
- `features`: `rmsDb=-96`, `peakDb=-96`, `crestFactor=1`, `centroidHz=0`,
  `rolloffHz=0`, `flatness=1`, `flux=0`
- `audioDtMs=0`, `consumerDtMs=0`, `brightnessFloor=0`, `version=2`

The channel branch is unchanged — same snapshot-driven population as before.
The lambda capturing `engine` constructs zero-valued trigger objects without
duplication.

## B2 — Audio guard added to 5 scripts

Added the standard guard `if (!audio || !audio.spectrum || audio.spectrum.length === 0) return map;`
at the top of `rgbMap()` (after `RGBUtil.createMap`):

| File | Position |
|---|---|
| `resources/rgbscripts/audiobasslaser.js` | After `var map = …` |
| `resources/rgbscripts/audiobuildup.js` | Before `extractFeatures(audio)` |
| `resources/rgbscripts/audiofireworks.js` | After `var map = …` |
| `resources/rgbscripts/audiohueshift.js` | After `var map = …` |
| `resources/rgbscripts/audioshockwave.js` | After `var map = …` |

This complements B1: even with the always-populated v2 shape, scripts
short-circuit cleanly when no spectrum samples are available (audio
profile not yet running, or stale state).

## Non-blocking — Removed beat-trigger fallbacks

Replaced legacy `audio.beat` fallback chains with the canonical
`audio.triggers.beat.firedThisFrame`:

- `resources/rgbscripts/audioshot.js`: removed `beatFired(audio)` helper
  (5 lines), replaced its sole call site (preset trigger 0) with the
  direct trigger access.
- `resources/rgbscripts/audiostrobe.js`: replaced
  `(audio.triggers && audio.triggers.beat) ? audio.triggers.beat.firedThisFrame : audio.beat`
  with `audio.triggers.beat.firedThisFrame`.
- `resources/rgbscripts/audiopower.js`: replaced
  `(audio.triggers && audio.triggers.beat && audio.triggers.beat.firedThisFrame) || audio.beat`
  with `audio.triggers.beat.firedThisFrame`.

Safe because B1 guarantees `audio.triggers.beat` is always present, and
all three scripts already start with the `audio.spectrum` guard
(established in earlier strip-review fixes).

## Verification

```
$ for f in resources/rgbscripts/audio*.js; do node --check "$f" 2>&1 || echo "FAIL: $f"; done
# (no output — all scripts pass)

$ cd build && cmake --build . --target qlcplus-qml -j8
…
[100%] Built target qlcplus-qml
```
