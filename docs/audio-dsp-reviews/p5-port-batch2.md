# P5 — Batch 2 Audio Script Port

Scope: port the low-energy driver scripts and the water spatial simulation to the new audio-facing JavaScript API.

## Scripts ported

- `audiomelt.js`
- `audioplasma.js`
- `audiosoap.js`
- `audiotunnel.js`
- `audiovortex.js`
- `audioscan.js`
- `audiocrawler.js`
- `audioglitch.js`
- `audiowater.js`

## Changes

- Replaced RGB helper calls with `RGBUtil` equivalents:
  - `RGBUtil.rgb()`
  - `RGBUtil.hsv2rgb()`
  - `RGBUtil.createMap()`
  - `RGBUtil.noiseField2d()`
- Replaced low-band driver reads with `audio.bands.low` in the eight low-energy scripts.
- Replaced script-side filter construction with `new AudioDSP.Filter(decay, AudioParams.filterRise(algo))` where visual low-band smoothing is still local to the effect.
- Removed script-side audio gain multiplication from the migrated low-energy and water paths; gain is expected to be handled by the C++ audio channel/profile.
- Ported `audiowater.js` from legacy melbank thirds/averaging to `audio.bands.low`, `audio.bands.mid`, and `audio.bands.high`.
- Updated `audiowater.js` ripple damping to scale by `audio.audioDtMs` when present, with a 40 ms fallback for legacy or missing timing data.

## Compatibility notes

- `usesAudio = true` is unchanged for all scripts.
- Existing `AudioParams.installContinuous(...)` calls are unchanged.
- The migrated render paths require the new audio snapshot shape (`audio.spectrum` plus `audio.bands`).

## Verification

- Passed: `node --check` for all nine scripts.
