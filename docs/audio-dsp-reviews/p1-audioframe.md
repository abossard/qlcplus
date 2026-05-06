# P1 — `AudioFrame` Header

**Task:** Introduce the canonical `AudioFrame` type defined by
[`p05-contracts.md` §1](./p05-contracts.md). No call sites are wired yet —
that lands in `p1-wire-capture`.

## What landed

- New header `engine/audio/src/audioframe.h`.
- Registered in `engine/audio/src/CMakeLists.txt` so it ships with the
  `qlcplusaudio` static library and is visible to consumers via the existing
  `target_include_directories` exports.

## Shape of the struct

Plain POD-style C++ struct, no namespace, no Qt, no STL containers. All buffer
fields are **non-owning `const` pointers** that reference memory owned by
`AudioCapture` / analyzer scratch. Lifetime is exactly the synchronous analyzer
callback that receives the frame; consumers must deep-copy if they need to
retain anything.

Key field groups (full doc comments in the header):

- **Identity & timing** — `frameIndex`, `hostTimeNs`, `sampleRate`, `fftSize`,
  `binCount`, `silent`.
- **Time-domain** — `samples` (`const int16_t*`), `sampleCount`, plus
  `rms` / `peak` / `dcOffset` already normalized to `[0..1]` / `[-1..1]`.
- **Frequency-domain** — `magnitudes` (`const double*`, length `binCount`),
  `bands32` (`const double*`, length 32).
- **Scalar features** — `rmsDb`, `peakDb`, `crestFactor`, `spectralFlux`,
  `spectralCentroidHz`, `spectralRolloffHz`, `spectralFlatness`,
  `noiseFloorDb`.
- **Beat** — `beatDetected`.

## Deviations from the contracts doc

The contracts doc shows the struct with `std::vector<double>` members inside a
`QLCPlus::Audio` namespace. Per the P1 task spec we converted that to:

| Contracts form | P1 header form | Rationale |
|---|---|---|
| `std::vector<double> samples` | `const int16_t *samples; size_t sampleCount;` | Avoid per-frame allocation; expose AudioCapture's `m_audioMixdown` directly. |
| `std::vector<double> magnitudes` | `const double *magnitudes;` (length `binCount`) | Non-owning view into analyzer scratch. |
| `std::vector<double> bands32` | `const double *bands32;` (length 32) | Same — matches existing `BandsData::m_fftMagnitudeBuffer` layout. |
| `namespace QLCPlus::Audio` | _(no namespace)_ | Follows existing QLC+ engine convention. |
| `quint64` / `qint64` / `quint32` | `<cstdint>` fixed-width types | Removes Qt dependency from the header. |

Field semantics (units, ranges, defaults) are unchanged from the contracts doc.

## Verification

```bash
cd build && cmake .. -Dqmlui=ON
cmake --build . --target qlcplus-qml -j8
# → Linking CXX executable qlcplus-qml
# → [100%] Built target qlcplus-qml
```

Header compiles cleanly under the project's `-Werror -Wextra -Wall` flags.
Nothing consumes the type yet — that's the next task.

## Next

`p1-wire-capture`: add `m_frameIndex` and a peak accumulator to `AudioCapture`,
allocate the `m_fftMagnitudeScratch` buffer, and instantiate an `AudioFrame`
between `processData()` lines 354 and 356 (insertion point identified in
`p0-audit-capture.md` §2).
