# P5 — Port Batch 1: Trigger, Blend, and Buildup Scripts

Scope: port the remaining trigger-first, three-band blend, and state-machine RGB audio scripts to the new audio-facing JavaScript helpers.

## Trigger-first scripts

Updated:
- `audioshot.js`
- `audiobasslaser.js`
- `audioshockwave.js`

Changes:
- Replaced rendering helpers with `RGBUtil.rgb`, `RGBUtil.hsv2rgb`, and `RGBUtil.createMap`.
- Read `audio.triggers.*.firedThisFrame` for new snapshots where the effect is trigger-driven.
- Kept legacy threshold fallbacks for old audio objects that do not expose `audio.triggers`.
- Removed script-side gain multiplication from new snapshot paths; gain is handled by the C++ audio profile/channel.

## Three-band blend scripts

Updated:
- `audiochaser.js`
- `audioenergy.js`
- `audiolava.js`
- `audiofireworks.js`
- `audiohueshift.js`

Changes:
- Replaced rendering helpers with `RGBUtil.*`.
- Read `audio.bands.low`, `audio.bands.mid`, and `audio.bands.high` for new snapshots.
- Kept legacy `LedFx.*_power` fallbacks only inside compatibility helpers for older audio payloads.
- Removed per-script band smoothing and script-side gain multiplication where the C++ channel now owns envelope/gain processing.
- For `audiofireworks.js`, burst triggers now prefer `audio.triggers.{bass,mid,high}.firedThisFrame` and retain local rising-edge fallbacks.

## State machine: `audiobuildup.js`

Changes:
- Replaced `new LedFx.ExpFilter(...)` and `AudioParams.createFilter(...)` usage with `new AudioDSP.Filter(...)`.
- Replaced RGB helpers with `RGBUtil.*`.
- Replaced band reads with `audio.bands.*` through a transition helper.
- Replaced the melbank interpolation dependency with `RGBUtil.interpolate(audio.spectrum, 16)`.
- Preserved the custom build/drop state machine, auto-tuned feature thresholds, bass-absence arming, and local rising-edge drop fallback.
- Added `audio.triggers.bass.firedThisFrame` as the preferred drop detector when a new snapshot is available.

## Verification

Ran syntax checks successfully:

```bash
node --check resources/rgbscripts/audioshot.js
node --check resources/rgbscripts/audiobasslaser.js
node --check resources/rgbscripts/audioshockwave.js
node --check resources/rgbscripts/audiochaser.js
node --check resources/rgbscripts/audioenergy.js
node --check resources/rgbscripts/audiolava.js
node --check resources/rgbscripts/audiofireworks.js
node --check resources/rgbscripts/audiohueshift.js
node --check resources/rgbscripts/audiobuildup.js
```
