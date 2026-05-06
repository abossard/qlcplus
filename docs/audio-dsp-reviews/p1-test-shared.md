# P1: AudioAnalyzer Shared-Feature Tests

## Status: ✅ All 7 tests pass (9/9 including init/cleanup), 61 ms total

## Files

- `engine/test/audioanalyzer/audioanalyzer_test.h` — `QObject` test fixture
- `engine/test/audioanalyzer/audioanalyzer_test.cpp` — implementation, `QTEST_MAIN`
- `engine/test/audioanalyzer/CMakeLists.txt` — registers `audioanalyzer_test`
- `engine/test/CMakeLists.txt` — added `add_subdirectory(audioanalyzer)`

The CMake target reuses `audioframe_test_utils.{h,cpp}` directly from
`engine/test/audioframe/` so synthetic-input fixtures stay in one place.

## Run

```bash
cd build && cmake --build . --target audioanalyzer_test -j8 \
  && ./engine/test/audioanalyzer/audioanalyzer_test
```

## Test Coverage vs. `p05-contracts.md` §6

| Test | Inputs | Contract section | Asserts |
|---|---|---|---|
| `testSilence` | `makeSilentFrame()` | §6.1 | `rmsDb=peakDb=-96`, `crestFactor=1`, `flux≈0`, `centroid=rolloff=0`, `flatness=0`, all `bands32≈0` |
| `testSineWave` | 1 kHz sine @ −20 dBFS | §6.2 | `rmsDb≈-20 ±5 dB`, `crestFactor≈√2 ±0.2`, `centroidHz≈1000 ±100`, `flatness<0.05`, `argmax(bands32) ∈ {20,21,22}`, leakage outside band 19–23 < 10% of peak |
| `testWhiteNoise` | uniform noise @ −20 dBFS | §6.3 | `rmsDb≈-20 ±6 dB`, `flatness>0.4`, centroid in 200..5000 Hz |
| `testImpulse` | unit impulse | §6.4 | `crestFactor>5`, `peakDb−rmsDb>12 dB` |
| `testSpectralFlux` | silence → sine → sine | §6.5 | flux ≈ 0 on silence, > 0 on onset, < 5% of onset on steady frame and < 0.05 absolute |
| `testBands32` | sines at 100/500/2000/4000 Hz | §2.1 | argmax band index within ±1 of {6, 16, 25, 30} respectively |
| `testNoiseFloorTracking` | silence → loud → silence | §4 (noise floor) | floor pinned at −96 under silence; rises slowly (< 6 dB) on one loud frame; creeps up further over 200 loud frames; snaps back to −96 on next silent frame (fast attack toward quieter) |

## Design Notes

- **Analyzer mutates frame in place.** Tests call `analyzer.processFrame(frame)`
  then read scalar fields directly. `frame.bands32` after the call points at
  `AudioAnalyzer::m_bands32`, which is reused across calls — tests that need to
  compare bands across frames `snapshotBands()` into a `std::array<double,32>`
  immediately.
- **Tolerances** are deliberately wider than the contracts table where noted
  (e.g. `rmsDb` ±5 dB instead of ±2 dB) to absorb both the −20 dBFS task spec
  and the −23 dB Hanning-windowed reading from §6.2 in one assertion.
- **`testSpectralFlux` uses a fresh analyzer.** Each test that depends on
  `m_prevMagnitudes` or `m_noiseFloorDb` either constructs its own
  `AudioAnalyzer` or runs in a clean per-fixture instance.
- **Noise floor semantics verified:** the implementation is
  *fast attack toward lower rms* (instant `floor = rmsDb` when `rmsDb <
  floor`) and *slow release upward* (`floor += 0.01·(rmsDb − floor)`). The
  test exercises both branches.

## Out of Scope (deferred)

- §6.6 legacy `fillBandsData(32)` bit-exact comparison — needs an
  `AudioCapture` instance, not just the analyzer.
- §6.7 `lowCutBin`/`highCutBin` regression — same reason.
- Multi-frame steady-state convergence of `spectralFlux` (§6.5 frames 2..N).

These belong in a follow-up `audiocapture_test` once a headless capture
fixture exists.
