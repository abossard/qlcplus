# Fix plan for the 6 remaining audio/DSP leftovers

## 1. Make `AudioCapture::m_analyzer` atomic

### Files and lines
- `engine/audio/src/audiocapture.h:24-29` — add the standard header include.
- `engine/audio/src/audiocapture.h:198-202` — change the member type.
- `engine/audio/src/audiocapture.cpp:135-139` — change the setter write.
- `engine/audio/src/audiocapture.cpp:396-397` — change the capture-thread read.

### Fix
- Add `#include <atomic>` in `audiocapture.h`.
- Replace:
  - `AudioAnalyzer *m_analyzer = nullptr;`
- With:
  - `std::atomic<AudioAnalyzer*> m_analyzer{nullptr};`
- In `AudioCapture::setAnalyzer(AudioAnalyzer *analyzer)`:
  - Replace the direct assignment with `m_analyzer.store(analyzer, std::memory_order_release);`.
  - Remove the `QMutexLocker` from this setter unless it is still needed for another field; `m_analyzer` no longer needs `m_mutex`.
- In `AudioCapture::processData()`:
  - Load once into a local pointer:
    - `AudioAnalyzer *analyzer = m_analyzer.load(std::memory_order_acquire);`
    - `if (analyzer != nullptr) analyzer->processFrame(frame);`
- Keep `Doc::destroyAudioCapture()` setting the analyzer to `nullptr` before clearing `m_inputCapture` (`engine/src/doc.cpp:335-342`).

### Risk assessment
- Low risk: this only changes pointer publication semantics.
- Main lifetime risk remains unchanged: callers must still clear the analyzer before destroying the analyzer object. Current `Doc::clearContents()` already releases profiles, destroys capture, then deletes `m_audioAnalyzer` (`engine/src/doc.cpp:172-184`).
- Removing the setter mutex slightly changes synchronization timing, but the pointer is independent and the audio thread already processes under its own capture lock (`audiocapture.cpp:453-455`).

### Test strategy
- Build target: `cd build && cmake --build . --target qlcplus-qml -j8`.
- Run existing audio tests: `cd build && ./engine/test/audioanalyzer/audioanalyzer_test`.
- Optional sanitizer/manual verification: run with ThreadSanitizer if a TSan build exists and confirm no race on `AudioCapture::m_analyzer` while starting/stopping audio capture.

## 2. Avoid redundant `setAnalyzer()` in `Doc::audioInputCapture()`

### Files and lines
- `engine/src/doc.cpp:309-324` — move analyzer wiring into the lazy creation branch.
- `engine/src/doc.cpp:327-332` — keep lazy `audioAnalyzer()` creation unchanged.
- `engine/src/doc.cpp:335-342` — keep teardown clearing unchanged.

### Fix
- Change `Doc::audioInputCapture()` so `setAnalyzer(audioAnalyzer())` is called only immediately after a new `AudioCaptureQt5`/`AudioCaptureQt6` is created.
- Current shape:
  - create capture when `!m_inputCapture`
  - then always call `m_inputCapture->setAnalyzer(audioAnalyzer())`
- New shape:
  - inside the `if (!m_inputCapture)` block, after construction, call `m_inputCapture->setAnalyzer(audioAnalyzer())`
  - remove the unconditional lines `322-323`
- Leave `destroyAudioCapture()` as-is so teardown still calls `setAnalyzer(nullptr)`.

### Risk assessment
- Low risk: analyzer pointer is idempotent and only needs to be installed once per capture object.
- Positive side effect: callers that only fetch an already-created capture no longer create or re-publish the analyzer.
- Must ensure no path replaces `m_audioAnalyzer` while keeping the same capture alive. Current code deletes `m_audioAnalyzer` only during `clearContents()` after `destroyAudioCapture()`.

### Test strategy
- Add a focused unit check in an existing Doc-oriented test if practical, or verify manually by instrumenting/logging during development that repeated `audioInputCapture()` calls do not call `setAnalyzer()` again.
- Regression build: `cd build && cmake --build . --target qlcplus-qml -j8`.
- Run affected tests: `cd build && ./engine/test/audioprofile/audioprofile_test ./engine/test/rgbscript/rgbscript_test`.

## 3. Resize/reset `AudioAnalyzer::m_prevMagnitudes` when FFT bin count changes

### Files and lines
- `engine/audio/src/audioanalyzer.h:23-27` — add/remove includes as needed.
- `engine/audio/src/audioanalyzer.h:52-64` — replace raw previous-magnitude storage.
- `engine/audio/src/audioanalyzer.cpp:42` — remove `kPreallocatedBinCount` if no longer needed.
- `engine/audio/src/audioanalyzer.cpp:73-86` — simplify constructor/destructor.
- `engine/audio/src/audioanalyzer.cpp:243-280` — update `computeSpectralFlux()`.
- `engine/test/audioanalyzer/audioanalyzer_test.h:16-25` and `.cpp` after `testSpectralFluxFormula()` — add a bin-count-change regression test.

### Fix
- Prefer `QVector<double>` or `std::vector<double>` instead of manual `new[]`.
- Replace these members:
  - `double *m_prevMagnitudes = nullptr;`
  - `int m_prevBinCount = 0;`
  - `int m_prevMagnitudeCapacity = 0;`
- With:
  - `QVector<double> m_prevMagnitudes;`
  - `int m_prevBinCount = 0;`
- Remove constructor preallocation and destructor `delete[]`.
- In `computeSpectralFlux()`:
  - Compute `const int binCount = int(frame.binCount);`.
  - If `m_prevMagnitudes.size() != binCount`, assign a zero-filled vector of exactly `binCount` and set `m_prevBinCount = 0`.
  - Then compute flux using previous magnitudes only when `bin < m_prevBinCount`.
  - After computing flux, copy current magnitudes into the vector and set `m_prevBinCount = binCount`.
- Important behavior: reset previous state on both growth and shrink, not only when `binCount > capacity`. That prevents an FFT/device switch from comparing incompatible frames.

### Risk assessment
- Medium-low risk: first frame after an FFT-size/device switch will report onset-like flux against zeros or zero previous sum. That is safer than comparing incompatible spectra, but it may suppress/alter exactly one frame of flux.
- Allocation happens only when the bin count changes, not every audio frame.
- Removing raw ownership reduces leak/double-delete risk.

### Test strategy
- Add a non-trivial test to `audioanalyzer_test`:
  - Process one frame with a small synthetic `binCount` and one in-band magnitude.
  - Process a second frame with a different `binCount` and different FFT size/sample rate.
  - Assert the call does not read stale bins and produces a deterministic flux based on reset previous state.
  - Then process an identical third frame at the new size and assert flux falls near zero.
- Run: `cd build && cmake --build . --target audioanalyzer_test -j8 && ./engine/test/audioanalyzer/audioanalyzer_test`.

## 4. Connect `RGBMatrix::audioProfileIdChanged` so running scripts re-resolve profile/channel state

### Files and lines
- `engine/src/rgbmatrix.h:170-179` — signal already exists; add private helper/slot declaration near the signal or algorithm section.
- `engine/src/rgbmatrix.cpp:246-253` — setter already emits; no semantic change except optional helper call placement.
- `engine/src/rgbmatrix.cpp:267-305` — when a script algorithm is installed, wire/reset script profile state if needed.
- `engine/src/rgbmatrix.cpp:679-683` and `685-714` — ensure the currently running script also receives invalidation.
- `engine/src/rgbscriptv4.h:145-168` — add a public/private invalidation method and any cached profile fields.
- `engine/src/rgbscriptv4.cpp:111-121`, `124-136`, `666-731` — initialize/reset profile resolution state and use it in `buildAudioDataObject()`.

### Fix
- Add a small API to `RGBScript`, for Qt/QML builds:
  - `void invalidateAudioProfileBinding();`
  - It should clear any cached profile/channel pointer and set `m_audioProfileLogged = false`.
- If caching is introduced for clarity/performance, add members such as:
  - `quint32 m_cachedAudioProfileId = AudioProfile::invalidId();`
  - `AudioChannel *m_cachedAudioChannel = nullptr;` (non-owning)
- Extract the profile/channel lookup currently in `buildAudioDataObject()` (`rgbscriptv4.cpp:711-729`) into a helper that resolves via `Doc::audioProfileForFunction(matrix->id())` and refreshes the cache when invalid.
- In `RGBMatrix`, add a helper like `invalidateAudioProfileBindingLocked()`:
  - Under `m_algorithmMutex`, if `m_algorithm` is a `RGBScript`, call `invalidateAudioProfileBinding()`.
  - If `m_runAlgorithm` is a different script pointer, invalidate it too.
- Connect `audioProfileIdChanged` in the `RGBMatrix` constructor, or directly call the helper after emitting in `setAudioProfileId()`.
  - The direct-call approach is simplest and avoids tracking a `QMetaObject::Connection`.
  - Keep the signal emission for QML/observers.
- This ensures a running matrix sees profile changes immediately and the next `rgbMap()`/`buildAudioDataObject()` uses the new profile channel rather than stale or null state.

### Risk assessment
- Medium risk: `RGBMatrix::write()` holds `m_algorithmMutex` while scripts are called. Avoid calling into `QJSEngine` from a non-JS thread without the existing `RGBScript` queued invocation pattern.
- The invalidation method should only reset simple C++ state; actual JS object creation remains in `buildAudioDataObject()` on the JS thread.
- Non-owning `AudioChannel*` cache must be invalidated when the profile ID changes; do not delete it from `RGBScript`.

### Test strategy
- Add a unit test in `engine/test/rgbmatrix/rgbmatrix_test.cpp` or `engine/test/rgbscript/rgbscript_test.cpp`:
  - Create two `AudioProfile` objects with different configs, add them to `Doc`.
  - Create an audio-aware `RGBMatrix`, set its `audioProfileId` to profile A, and force one audio object build/map call.
  - Change `audioProfileId` to profile B while the same matrix/script instance remains alive.
  - Verify the script-side binding invalidates/re-resolves to profile B on the next call.
- Run: `cd build && cmake --build . --target rgbmatrix_test rgbscript_test -j8 && ./engine/test/rgbmatrix/rgbmatrix_test && ./engine/test/rgbscript/rgbscript_test`.

## 5. Remove deprecated `AudioParams.gainFactor()` and prevent script-side double gain

### Files and lines
- `resources/rgbscripts/audio_common.js:14-57` — remove the deprecated helper.
- `resources/rgbscripts/audio*.js` — verify no script uses `AudioParams.gainFactor(...)` before/after removal.
- `engine/audio/src/audiochannel.cpp:151` and `176` — C++ already applies profile gain with `m_config.agc.inputGainLinear * linearGain(m_agcGainDb)`.
- `engine/test/rgbscript/rgbscript_test.cpp:87-127` — existing syntax/load pass will catch scripts broken by helper removal.

### Fix
- Delete `gainFactor: function(algo) { return 0.6 + algo.presetGain * 0.2; },` from `audio_common.js`.
- Leave `presetGain` properties in `installContinuous()` and `installTrigger()` for legacy UI/migration compatibility unless a separate UX change removes those controls.
- Keep `filterRise()` deprecated helper only if current scripts still depend on it for smoothing; this task is specifically about gain and double-gain.
- Run a search after deletion:
  - `rg "gainFactor" resources/rgbscripts`
  - Expected result: no matches.

### Risk assessment
- Low risk in the current tree: the only current match is the helper definition in `audio_common.js:51`; none of the 28 `audio*.js` scripts call it.
- If users have custom scripts that call `AudioParams.gainFactor()`, they will fail unless the helper remains as a compatibility no-op. Safer alternative: keep `gainFactor()` but return `1.0` and update the comment to `DEPRECATED: no-op; gain is applied by AudioProfile`.
- Recommendation: choose deletion for repository scripts, or no-op for third-party script compatibility. If compatibility matters, no-op is safer than removal and still prevents double-gain for scripts that multiply by it.

### Test strategy
- Search: `rg "gainFactor" resources/rgbscripts` and confirm either no matches or only a documented no-op helper.
- Run RGB script loader test: `cd build && cmake --build . --target rgbscript_test -j8 && ./engine/test/rgbscript/rgbscript_test`.
- Manual audio preview: compare an audio script with high profile input gain and confirm brightness is not multiplied a second time by JS.

## 6. Validate `AudioProfile` XML `Version` on load

### Files and lines
- `engine/src/audioprofile.cpp:143-170` — validate the root `Version` attribute after reading `attrs` and before parsing child fields.
- `engine/src/audioprofile.cpp:272-281` — save already writes `Version="1"`; keep unchanged.
- `engine/src/audioprofile.h:38-43` — optionally add a constant for the supported version.
- `engine/test/audioprofile/audioprofile_test.h:17-23` and `.cpp` after `testXmlRoundTrip()` — add version load tests.

### Fix
- Define a supported version constant, preferably near the XML constants:
  - `static constexpr int KXMLQLCAudioProfileSupportedVersion = 1;` if acceptable in the header, or a `constexpr int kSupportedAudioProfileVersion = 1;` in the `.cpp` anonymous namespace.
- In `AudioProfile::loadXML()` after `const QXmlStreamAttributes attrs = root.attributes();`:
  - If `Version` is absent, treat it as version 1 for backward compatibility with projects saved before this attribute existed.
  - If present, parse as integer.
  - If parsing fails or value is not `1`, log `qWarning()` and return `false`.
- Keep parsing of known v1 child elements unchanged.

### Risk assessment
- Low risk for current files saved by this code: they write `Version="1"` at `audioprofile.cpp:280`.
- Backward compatibility risk is handled by accepting missing version as v1.
- Forward compatibility behavior changes intentionally: old code will reject future `Version="2"` profiles instead of silently misreading them.

### Test strategy
- Extend `audioprofile_test` with parameterized XML load cases:
  - missing `Version` loads successfully as v1
  - `Version="1"` loads successfully
  - `Version="2"` fails
  - `Version="abc"` fails
- Run: `cd build && cmake --build . --target audioprofile_test -j8 && ./engine/test/audioprofile/audioprofile_test`.

## Suggested verification order

1. Implement issues 1 and 2 together; build `qlcplus-qml`.
2. Implement issue 3; run `audioanalyzer_test`.
3. Implement issue 6; run `audioprofile_test`.
4. Implement issue 5; run `rgbscript_test` and `rg "gainFactor" resources/rgbscripts`.
5. Implement issue 4 last; run `rgbmatrix_test` and `rgbscript_test`.
6. Final smoke build: `cd build && cmake --build . --target qlcplus-qml -j8`.
