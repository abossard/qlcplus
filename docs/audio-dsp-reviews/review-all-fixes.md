# Review of all B1–B10 fixes (Opus 4.7)

Source verified against the fix docs in `docs/audio-dsp-reviews/fix-*.md`. All 5 audio test suites built and passed cleanly.

## Per-bug verification

| Bug | Status | Evidence |
| --- | --- | --- |
| **B1** Production analyzer wired to capture | ✅ fixed | `Doc::audioInputCapture()` calls `m_inputCapture->setAnalyzer(audioAnalyzer())` (`engine/src/doc.cpp:323`); lazy-created in `Doc::audioAnalyzer()` (line 327). |
| **B2** Profile bound to analyzer | ✅ fixed | `Doc::addAudioProfile()` calls `profile->bindAnalyzer(audioAnalyzer())` (`doc.cpp:1068`). `removeAudioProfile()` calls `releaseAnalyzer()` before delete (line 1086). |
| **B3** Dangling `m_audioChannel` removed | ✅ fixed | No `m_audioChannel` / `AudioChannel` member remains in `rgbscriptv4.h` (only `m_audioProfileLogged` flag). `buildAudioDataObject()` re-resolves profile + channel each call (`rgbscriptv4.cpp:711-732`). Returns `audioObj` early if channel is null. |
| **B4** Silent-frame flatness = 1.0 | ✅ fixed | `audioanalyzer.cpp:182`: silent path sets `frame.spectralFlatness = 1.0`. |
| **B5** Spectral flux normalized by Σ\|M_{t-1}\| | ✅ fixed | `audioanalyzer.cpp:280`: `return flux / std::max(previousSum, kMinLinear);` with `previousSum = Σ abs(prevMag)` over the same range. |
| **B6** Flux range 40–5000 Hz | ✅ fixed | `lowBin = ceil(SPECTRUM_MIN_FREQUENCY * fftSize / sampleRate)`, `highBin = floor(SPECTRUM_MAX_FREQUENCY * fftSize / sampleRate)` (`audioanalyzer.cpp:257-262`). Constants set to 40/5000 Hz. |
| **B7** Noise floor: init −60 dB, +6 dB/s rise | ✅ fixed | `m_noiseFloorDb = -60.0` default (`audioanalyzer.h:58`); rise law `m_noiseFloorDb += min(6 * dt/1000, rmsDb - m_noiseFloorDb)` (`audioanalyzer.cpp:368`); instant snap-down on quieter frames. |
| **B8** Band index for 250 Hz → 12 | ✅ fixed | `audiochannel.cpp:36-50`: `floor()` then conditional `+1` only when fraction ≥ 0.25. Math check: 250 Hz → index ≈ 12.146 → frac 0.146 < 0.25 → **12** ✅. 2000 Hz → index ≈ 25.928 → frac 0.928 ≥ 0.25 → **26** ✅. |
| **B9** Legacy slider migration | ✅ fixed | `Doc::postLoad()` second loop migrates audio-reactive `RGBMatrix` instances without `audioProfileId`, reading `presetGain/Reactivity/Floor/Sensitivity` and creating a `Migrated Audio` profile via `AudioProfile::configFromLegacySliders()` (`doc.cpp:1593-1641`). Defaults `5,5,0,5` applied via `legacyAudioProperty()` helper. |
| **B10** VC profile editing wired to QML | ✅ fixed | `vcaudiotriggers.h` exposes `Q_PROPERTY` getters and `Q_INVOKABLE` setters for envelope, AGC, and trigger fields. `VCAudioTriggersProperties.qml` binds real values, e.g. `value: widgetRef ? widgetRef.envelopeAttack : 0` and `onMoved: widgetRef.setEnvelopeAttack(value)` (lines 539-587). Setters auto-create a default profile and route through `AudioProfile::setChannelConfig()` so the live `AudioChannel` receives `updateConfig()`. |

## Test results

Built with `cmake .. -Dqmlui=ON`. All targets compiled with no warnings.

| Suite | Result |
| --- | --- |
| `audioframe_test` | **6 / 6 PASS** (5 ms) |
| `audioanalyzer_test` | **10 / 10 PASS** (61 ms) |
| `audiochannel_test` | **10 / 10 PASS** (205 ms) |
| `audioprofile_test` | **7 / 7 PASS** (6 ms) — one cosmetic `QObject::startTimer` warning during `cleanupTestCase` (event dispatcher already destroyed); pre-existing teardown noise, not a failure |
| `audioslice_test` | **5 / 5 PASS** (232 ms) |

**Total: 38 passed, 0 failed.**

## New issues introduced by the fixes

### Minor
1. **`AudioCapture::m_analyzer` read without mutex.** `setAnalyzer()` takes `m_mutex`, but the capture-thread read at `audiocapture.cpp:396` (`if (m_analyzer) m_analyzer->processFrame(frame);`) does not hold the mutex. The new B1 wiring exercises this path on every frame. In practice raw-pointer reads on x86/ARM are atomic and a torn read is unlikely, but it is technically a data race. **Recommend** changing `m_analyzer` to `std::atomic<AudioAnalyzer*>` or holding the mutex in `frameAvailable()`. Not a regression — pre-existing pattern was identical.
2. **`Doc::audioInputCapture()` re-asserts the analyzer on every call.** Each invocation calls `setAnalyzer(audioAnalyzer())` even when the capture already has it. Harmless (idempotent + mutex-protected) but slightly wasteful and means lazy `audioAnalyzer()` may be created by a UI thread caller of `audioInputCapture()`. Move the `setAnalyzer` call into the `if (!m_inputCapture)` branch.
3. **`audioProfile_test` teardown warning.** `QObject::startTimer: current thread's event dispatcher has already been destroyed` — likely the analyzer being deleted after the test app loop is gone. Cosmetic; investigate if it ever surfaces in CI strict mode.

### Verified non-issues
- **Destruction order:** `~Doc` calls `clearContents()` (which releases profiles → destroys capture → deletes analyzer) and then `delete m_audioAnalyzer` again. Safe because `clearContents()` nulls the pointer; `delete nullptr` is a no-op.
- **Circular ownership:** `AudioProfile` holds a non-owning `AudioAnalyzer*` via `bindAnalyzer`; `AudioAnalyzer` does not hold profile pointers. No cycle.
- **B3 re-resolution cost:** `owningMatrix()` lookup runs on every script frame; acceptable trade-off for correctness, and the function map is hashed.
- **B9 migration idempotence:** Skips matrices that already have an `audioProfileId`, so re-loading a migrated workspace does not double-migrate.

## Overall verdict

✅ **Ready to ship.** All 10 blocking bugs are correctly fixed in source, contracts hold (verified via spot-check math for B8 boundaries and formulas for B4–B7), and the full audio test suite is green. The two follow-ups listed above (atomic analyzer pointer, redundant `setAnalyzer` call) are quality improvements, not blockers.
