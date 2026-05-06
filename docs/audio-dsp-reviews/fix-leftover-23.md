# Fix Leftovers #2 and #3 — summary

## Fix #2 — Redundant `setAnalyzer()` on every `Doc::audioInputCapture()` call

### Problem
`Doc::audioInputCapture()` unconditionally called
`m_inputCapture->setAnalyzer(audioAnalyzer())` on every invocation, even when
the capture object already existed. This re-published the same analyzer
pointer on every access, taking the `AudioCapture::m_mutex` and writing the
member each time.

### Change
- `engine/src/doc.cpp` — moved the `setAnalyzer(audioAnalyzer())` call inside
  the `if (!m_inputCapture)` lazy-creation branch. The analyzer is now wired
  exactly once per capture object lifetime (paired with the
  `setAnalyzer(nullptr)` in `destroyAudioCapture()`).

### Why this is safe
- `audioAnalyzer()` lazily creates exactly one `AudioAnalyzer` per `Doc`.
- The analyzer is destroyed only by `clearContents()`, which calls
  `destroyAudioCapture()` first — so no live `AudioCapture` ever outlives its
  analyzer pointer.
- No code path replaces `m_audioAnalyzer` while keeping the same capture
  alive (verified in review).

No new `AudioCapture::analyzer()` getter was needed — the plan-recommended
"move into create branch" approach is simpler than the guard-with-getter
alternative and avoids exposing the member.

## Fix #3 — `m_prevMagnitudes` not reset on FFT bin-count change

### Problem
`AudioAnalyzer::computeSpectralFlux()` only reallocated `m_prevMagnitudes`
when the new `binCount` exceeded `m_prevMagnitudeCapacity`. On a *shrink*
(switching to a smaller FFT or lower sample rate), the buffer was kept and
`m_prevBinCount` was not reset. As the critique pointed out, this is **not**
an out-of-bounds bug — `highBin` is always clamped to `binCount - 1` and the
buffer is always at least that large. It is a **correctness** bug: bin index
`k` maps to a different physical frequency before and after the FFT-size
switch, so the diff produced for the first frame after the switch is
**garbage** (not "onset-like flux against zeros" as the original plan
wording claimed).

### Change
- `engine/audio/src/audioanalyzer.h`
  - Replaced raw owned `double *m_prevMagnitudes` + `m_prevMagnitudeCapacity`
    with `QVector<double> m_prevMagnitudes`. Kept `int m_prevBinCount`.
  - Added a comment documenting the reset-on-size-change semantics.
- `engine/audio/src/audioanalyzer.cpp`
  - Removed `kPreallocatedBinCount` and the constructor preallocation.
  - Removed `delete[]` from the destructor (RAII via `QVector`).
  - In `computeSpectralFlux()`: if `m_prevMagnitudes.size() != binCount`, call
    `m_prevMagnitudes.fill(0.0, binCount)` and reset `m_prevBinCount = 0`.
    Reset on **both grow and shrink**.
  - Updated the post-loop `std::copy` to write into `m_prevMagnitudes.begin()`.

### Why this is safe
- Allocation only happens on actual size change, not per audio frame.
- First post-resize frame produces a finite (possibly large) flux because
  `previousSum == 0` clamps the denominator to `kMinLinear` — same boundary
  case as the very first frame ever processed. A subsequent identical frame
  converges to ~0 (verified by the new regression test).
- `QVector` ownership eliminates manual `new[]` / `delete[]` and removes any
  leak/double-delete risk.

### Test
Added `AudioAnalyzerTest::testSpectralFluxBinCountChange` to
`engine/test/audioanalyzer/audioanalyzer_test.{h,cpp}`. The test:
1. Processes a frame at `kFftSize=2048` / `kBinCount=1025` with one in-band
   bin populated.
2. Processes a frame at `kFftSize=512` / `kBinCount=257` (shrink).
3. Asserts the resulting flux is finite and non-negative.
4. Processes an identical frame at the new small size and asserts flux
   converges to ~0 (previous magnitudes match → no positive delta).
5. Grows back to the large size and asserts flux remains finite.

The test follows the `_data()`/`QFETCH` parameterized convention only where
useful — here a single linear scenario covers shrink, steady-state, and grow.

## Verification

```bash
cd build && cmake --build . --target qlcplus-qml -j8
# → succeeds

cd build && cmake --build . --target audioanalyzer_test -j8 \
  && ./engine/test/audioanalyzer/audioanalyzer_test
# → 11 passed, 0 failed (was 10 before the new test)
```

## Files touched
- `engine/src/doc.cpp` — fix #2
- `engine/audio/src/audioanalyzer.h` — fix #3 member change
- `engine/audio/src/audioanalyzer.cpp` — fix #3 logic change
- `engine/test/audioanalyzer/audioanalyzer_test.h` — new test slot
- `engine/test/audioanalyzer/audioanalyzer_test.cpp` — new test body
