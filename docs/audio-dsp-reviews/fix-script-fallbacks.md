# Fix: Old-engine fallback for `audio.bands.*` in 13 RGB scripts

## Problem
13 audio RGB scripts read `audio.bands.{low,mid,high}` directly or returned an empty map when `audio.bands` was missing. On engines that don't populate `audio.bands` (old engine path / pre-Phase-6), this caused either runtime errors or blank output.

## Fix
Adopted the established pattern from `audioaurora.js` / `audiostrobe.js`:

```js
function bandValue(audio, name) {
    if (audio && audio.bands && typeof audio.bands[name] === "number")
        return audio.bands[name];
    return AudioParams.bandPower(audio, name);
}
```

`AudioParams.bandPower` (in `audio_common.js:98`) falls back to averaging the matching slice of `audio.spectrum` using log-frequency crossovers, so the scripts work on both new and old engines.

## Changes per script

For each file: added the `bandValue()` helper just before `algo.rgbMap = function(...)`, replaced every `audio.bands.<band>` read with `bandValue(audio, "<band>")`, and (where present) removed the `|| !audio.bands` clause from the early-return guard so the spectrum fallback can run.

| Script | Direct reads replaced | Guard relaxed |
|---|---|---|
| audioblocks.js | low, mid, high | – |
| audiocrawler.js | low | ✓ |
| audiofire.js | low | – |
| audioglitch.js | low | ✓ |
| audiomelt.js | low | ✓ |
| audioplasma.js | low | ✓ |
| audiopower.js | low | – |
| audioscan.js | low | ✓ |
| audioscroll.js | low, mid, high | – |
| audiosoap.js | low | ✓ |
| audiotunnel.js | low | ✓ |
| audiovortex.js | low | ✓ |
| audiowater.js | low, mid, high | ✓ |

## Verification
- `AudioParams.bandPower` confirmed at `resources/rgbscripts/audio_common.js:98`.
- `node --check` passes for all `resources/rgbscripts/audio*.js`.
- `grep "!audio\.bands"` returns no remaining matches across audio scripts.
- `grep "audio\.bands\."` outside `bandValue()` helpers returns no remaining direct reads.
