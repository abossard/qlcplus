# P1 Synthetic Audio Test Harness

## Summary

Implemented a deterministic `AudioFrame` test harness in `engine/test/audioframe/`.

## Added utilities

- `AudioTestUtils::makeSilentFrame()` — all-zero frame with silence-safe scalar features.
- `AudioTestUtils::makeSineFrame()` — deterministic single-frequency sine frame.
- `AudioTestUtils::makeNoiseFrame()` — deterministic white-noise frame using a fixed seed derived from `frameIndex`.
- `AudioTestUtils::makeImpulseFrame()` — single-sample impulse centered in the FFT window.

`AudioFrame` uses non-owning pointers, so the utility owns backing buffers in thread-local storage for the duration of the test thread. Each generated frame receives stable sample, magnitude, and `bands32` buffers.

## DSP behavior

- RMS, peak, DC offset, dB, crest factor, centroid, rolloff, flatness, and silence state are computed from generated samples.
- FFT magnitudes use FFTW3 when `HAS_FFTW3` is available.
- A direct DFT fallback is provided when FFTW3 is unavailable.
- `bands32` follows the legacy `fillBandsData(32)` log-band averaging contract.

## Smoke tests

Added `audioframe_test` covering:

- Silent frame: zero RMS/peak and silence-safe defaults.
- 1 kHz sine at -20 dBFS: expected RMS/peak, crest factor, centroid, and low flatness.
- White noise: non-zero RMS/peak and high flatness.
- Impulse: expected RMS, near-full peak, high crest factor, and flat-spectrum behavior.

## Verification

Passed:

```bash
cd build && cmake --build . --target audioframe_test -j8 && ./engine/test/audioframe/audioframe_test
```

Result: 6 passed, 0 failed.

The requested full reconfigure command was also attempted, but the repository currently fails during CMake generation in the pre-existing `plugins/midi/test/CMakeLists.txt` before build generation completes.
