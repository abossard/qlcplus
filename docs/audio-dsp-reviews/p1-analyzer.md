# P1 AudioAnalyzer Implementation

## Summary

Created `AudioAnalyzer` as the synchronous per-hop DSP processor for `AudioCapture`.

Implemented shared scalar features:

- `rmsDb` and `peakDb` using clamped `20 * log10(max(x, 1e-10))` with `-96 dB` floor.
- `crestFactor` with silence default `1.0`.
- `spectralFlux` from positive per-bin magnitude deltas, normalized by bin count.
- `spectralCentroidHz`, `spectralRolloffHz`, and `spectralFlatness` over `40..5000 Hz`.
- slow-release `noiseFloorDb` tracking.
- 32 legacy-compatible log-spaced bands from raw FFT magnitudes.

## Integration

- Added `engine/audio/src/audioanalyzer.h`.
- Added `engine/audio/src/audioanalyzer.cpp`.
- Registered both files in `engine/audio/src/CMakeLists.txt`.
- Replaced the temporary local analyzer interface in `audiocapture.cpp` with `#include "audioanalyzer.h"`.
- Existing `AudioCapture::processData()` now calls `AudioAnalyzer::processFrame(AudioFrame &frame)` when an analyzer is installed.

## Hot-path notes

- No Qt signals or slots are used by `AudioAnalyzer`.
- `processFrame()` performs no steady-state heap allocation.
- Previous-frame magnitudes and the 32-band buffer are analyzer-owned scratch memory.

## Verification

Ran successfully:

```bash
cd build && cmake .. -Dqmlui=ON && cmake --build . --target qlcplus-qml -j8
```

Result: `qlcplus-qml` built successfully.
