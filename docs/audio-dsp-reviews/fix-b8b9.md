# Fix B8 + B9

## B8 — band index off-by-one

- Updated `engine/audio/src/audiochannel.cpp` so `bandIndexForFrequency()` no longer blindly applies `ceil()` to every boundary.
- The helper now derives the logarithmic 32-band position with `floor()` first, then rounds up only when the boundary is clearly closer to the next legacy band edge.
- This keeps the legacy-compatible cut points required by the contract:
  - `250 Hz -> 12` for `lowCutBin(32)` compatibility.
  - `2000 Hz -> 26` for `highCutBin(32)` compatibility.

## B9 — legacy slider migration

- Added post-load migration in `Doc::postLoad()` after all functions have been loaded and resolved.
- For each audio-reactive `RGBMatrix` without an `audioProfileId`, the migration reads legacy script slider properties:
  - `presetGain`
  - `presetReactivity`
  - `presetFloor`
  - `presetSensitivity`
- Missing or unreadable values fall back to the legacy defaults `5, 5, 0, 5`.
- The legacy values are converted through `AudioProfile::configFromLegacySliders()`.
- A new `Migrated Audio` profile is created and assigned to the matrix, preserving per-function legacy tuning instead of leaving the sliders as inert script properties.

## Verification

Requested verification command:

```bash
cd build && cmake .. -Dqmlui=ON && cmake --build . --target qlcplus-qml -j8
```
