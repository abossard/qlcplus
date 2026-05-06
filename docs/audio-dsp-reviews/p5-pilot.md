# P5 — Pilot Port of Representative Audio Scripts

Scope: port one bundled RGB script from each migration category to the new audio-facing JS API while keeping transition compatibility with legacy audio payloads where needed.

## `audiostrobe.js` — trigger-first

**Before**
- Used `LedFx.rgb`, `LedFx.hsv2rgb`, and `LedFx.createMap` for rendering helpers.
- Computed bass/mid/high/volume gates inline with `LedFx.*_power(audio) * AudioParams.gainFactor(algo) > AudioParams.triggerThreshold(algo)`.

**After**
- Uses `RGBUtil.rgb`, `RGBUtil.hsv2rgb`, and `RGBUtil.createMap`.
- Uses `audio.triggers.{bass,mid,high}.firedThisFrame`, `audio.triggers.beat.firedThisFrame`, and `audio.triggers.volume.active` when the new snapshot is available.
- Keeps `AudioParams.installTrigger()` and legacy threshold fallback for old audio objects without `audio.triggers`.
- Drops script-side gain multiplication; gain is expected to come from the C++ audio profile/channel.

## `audioaurora.js` — three-band blend

**Before**
- Used `LedFx.rgb` and `LedFx.createMap`.
- Created three `AudioParams.createFilter()` instances for lows, mids, and highs.
- Read `LedFx.lows_power`, `LedFx.mids_power`, and `LedFx.high_power`, then multiplied by `AudioParams.gainFactor(algo)`.

**After**
- Uses `RGBUtil.rgb` and `RGBUtil.createMap`.
- Reads `audio.bands.low`, `audio.bands.mid`, and `audio.bands.high` from the C++ snapshot.
- Removes per-script band filters and gain multiplication because envelope/gain are handled by the C++ audio channel.
- Keeps `AudioParams.installContinuous()` and falls back to the old `LedFx.*_power` helpers for old audio objects.

## `audiospectrum.js` — spectrum visual

**Before**
- Used `LedFx.rgb`, `LedFx.hsv2rgb`, `LedFx.createMap`, and `LedFx.melbank(audio, n)`.
- Created smoothing through `AudioParams.createFilter()`.
- Multiplied spectrum bins by `AudioParams.gainFactor(algo)`.

**After**
- Uses `RGBUtil.rgb`, `RGBUtil.hsv2rgb`, `RGBUtil.createMap`, and `RGBUtil.interpolate(audio.spectrum, n)` for display-width resampling.
- Uses `new AudioDSP.Filter(decay, rise)` for visual smoothing that is still specific to this bar display.
- Drops script-side gain multiplication; the incoming `audio.spectrum` is expected to be pre-processed by the C++ channel.
- Keeps `AudioParams.installContinuous()` during the transition.

## Loader note

`rgbscriptv4.cpp` now preloads `rgbutil.js` and `audiodsp.js` before the legacy LedFx compatibility shim and `audio_common.js`, so migrated scripts can use the new helper namespaces while unported scripts continue to use `LedFx.*`.
