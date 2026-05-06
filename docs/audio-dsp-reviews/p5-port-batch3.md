# P5 — Batch 3 Spectrum Visual Script Ports

Scope: port the remaining seven spectrum-style audio RGB scripts from the legacy `LedFx` audio helper API to the new `RGBUtil` and `AudioDSP` helper namespaces.

## Scripts ported

- `audioequalizer.js`
- `audiosplittower.js`
- `audiowavelength.js`
- `audiopower.js`
- `audiofire.js`
- `audioscroll.js`
- `audioblocks.js`

## Migration summary

- Replaced `LedFx.rgb`, `LedFx.hsv2rgb`, and `LedFx.createMap` with `RGBUtil.rgb`, `RGBUtil.hsv2rgb`, and `RGBUtil.createMap`.
- Replaced `LedFx.melbank(audio, n)` with `RGBUtil.interpolate(audio.spectrum, n)` for display-width or section-count resampling.
- Replaced legacy band helpers with direct snapshot reads from `audio.bands.low`, `audio.bands.mid`, and `audio.bands.high`.
- Replaced `AudioParams.createFilter()` usage with `new AudioDSP.Filter(decay, AudioParams.filterRise(algo))` for script-local visual smoothing.
- Removed script-side gain multiplication; spectrum and band values now use the pre-processed audio snapshot directly.
- Kept `usesAudio = true` and `AudioParams.installContinuous(...)` in every script.
- Updated `audiopower.js` beat sparks to prefer `audio.triggers.beat.firedThisFrame` while retaining `audio.beat` compatibility.
- Updated `audioscroll.js` history reinitialization so the buffer resets when the resampled band length changes.

## Validation

```bash
node --check resources/rgbscripts/audioequalizer.js
node --check resources/rgbscripts/audiosplittower.js
node --check resources/rgbscripts/audiowavelength.js
node --check resources/rgbscripts/audiopower.js
node --check resources/rgbscripts/audiofire.js
node --check resources/rgbscripts/audioscroll.js
node --check resources/rgbscripts/audioblocks.js
```

All seven checks passed.
