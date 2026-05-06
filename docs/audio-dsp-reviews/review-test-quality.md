# Audio DSP Test Quality Review

## Scope note

I read the requested test/source files completely:

- `engine/test/audioframe/audioframe_test.cpp`
- `engine/test/audioframe/audioframe_test_utils.cpp`
- `engine/test/audioanalyzer/audioanalyzer_test.cpp`
- `engine/test/audiochannel/audiochannel_test.cpp`
- `engine/test/audioprofile/audioprofile_test.cpp`
- `engine/test/audioslice/audioslice_test.cpp`
- `engine/audio/src/audioanalyzer.cpp`
- `engine/audio/src/audiochannel.cpp`
- `engine/audio/src/audiochannelconfig.cpp`

One mismatch: the prompt lists 6 / 11 / 10 / 13 / 5 tests, but the files currently contain fewer test functions:

- AudioFrame: 4 test functions
- AudioAnalyzer: 9 test functions
- AudioChannel: 8 test functions
- AudioProfile: 6 test functions plus one `_data()` table with 6 rows
- AudioSlice: 3 test functions

---

# 1. Per-suite verdict

## `audioframe_test.cpp` + `audioframe_test_utils.cpp`

**Verdict: Mixed**

These tests are mostly validating the test-frame generators, not production DSP behavior. Some assertions are meaningful — sine RMS/peak/crest/centroid and impulse RMS are specific enough to catch broken synthetic frame generation. However, the noise and impulse spectral assertions are broad lower bounds, and there is no direct verification that generated spectra have the expected FFT-bin-level shape.

**What they catch well**

- Silent frame metadata and scalar fields.
- Sine frame approximate RMS, peak, crest factor, centroid.
- Impulse RMS and peak.
- Basic sanity of noise flatness.

**What they do not catch well**

- Whether the sine generator produces a peak in the exact expected FFT bin.
- Whether noise is statistically flat across bins/bands.
- Whether the impulse is spectrally flat in the way the production analyzer expects.
- Numerical edge cases: tiny amplitudes, clipped amplitudes, NaN/Inf sample values.

---

## `audioanalyzer_test.cpp`

**Verdict: Mixed, leaning real for spectral-feature unit tests**

This is the strongest suite for shared feature math. It has real checks for silence, sine centroid/bands, spectral flux formula, FFT-size changes, and noise-floor behavior. The `testSpectralFluxFormula()` test is especially valuable because it constructs magnitudes directly and checks the normalized flux value exactly.

However, several tests still use broad “reasonable range” assertions that would allow materially wrong DSP behavior, especially for white noise, impulse, and first-frame flux. There is also no direct testing for spectral rolloff correctness, NaN/Inf input handling, very small input behavior, or wall-clock envelope timing because analyzer timing is only indirectly represented through `fftSize / sampleRate`.

**What they catch well**

- Silence scalar features and zero bands.
- Sine centroid near expected frequency.
- Sine band concentration around expected 32-band index.
- Spectral flux normalization formula for a controlled bin.
- Reset behavior when FFT bin count changes.
- Noise floor fast attack / slow release behavior.

**What they do not catch well**

- Rolloff math.
- Broadband noise spectral distribution.
- Correct behavior under invalid numerical inputs.
- Exact first-frame flux behavior.
- Analyzer/channel multi-channel interactions.
- Real `AudioCapture → AudioAnalyzer` integration.

---

## `audiochannel_test.cpp`

**Verdict: Mixed**

This suite has real behavioral tests for smoothing monotonicity, AGC direction, trigger fire/release/cooldown/hold, config update staging, and snapshot locking. These are not useless tests; they would catch many gross regressions.

But they mostly assert qualitative behavior rather than exact DSP math. The envelope tests would pass if the attack/release constants were materially wrong but still monotonic. AGC only checks “boosts quiet” and “drops on loud,” not the expected gain trajectory. Trigger tests cover simple high/low transitions but not threshold-hover hysteresis, variable `audioDtMs`, sustained stability, or first-non-silent-frame edge behavior in a realistic stream.

**What they catch well**

- Envelope rises on loud input and decays on silence.
- Trigger fires once for sustained loud input.
- Trigger releases after silence.
- Cooldown blocks retriggering.
- Hold delays release.
- Config updates apply on next frame.
- Snapshot reads do not trivially crash during concurrent updates.

**What they do not catch well**

- Exact attack/release time constants.
- Wall-clock correctness for `audioDtMs = 20 / 40 / 60 ms`.
- Sustained-signal drift after long runs.
- Threshold hysteresis chatter.
- Noise gate behavior.
- Multi-channel config separation.
- Real thread-safety stress under sanitizer/invariant checks.

---

## `audioprofile_test.cpp`

**Verdict: Mostly real for serialization/config plumbing; limited DSP coverage**

The XML round-trip test is good: it uses non-default values and compares nearly every config field. The legacy migration test for `(5,5,50,5)` checks exact mapped values, which is valuable. Doc registration/default-profile tests are useful plumbing checks.

However, this suite does not test many lifecycle and integration risks: active channel using a deleted profile, profile rebinding during playback, XML compatibility with malformed/partial non-default structures beyond one envelope field, or RGBScript resolution of audio profiles at runtime.

**What they catch well**

- Profile creation defaults.
- Non-default XML save/load round-trip.
- Exact legacy slider migration for one representative tuple.
- Basic `Doc` registration and default profile creation.
- Partial XML version compatibility.

**What they do not catch well**

- Boundary legacy slider values and clamping.
- Profile deletion while active.
- Runtime `RGBScript` profile resolution.
- Interaction between `Doc`, `RGBMatrix`, `AudioAnalyzer`, and active `AudioChannel`.

---

## `audioslice_test.cpp`

**Verdict: Feel-good / shallow integration**

This suite gives some vertical-slice confidence, but it mostly verifies that objects are connected and fields are nonzero/finite. The end-to-end pipeline test uses one synthetic sine frame and broad ranges. The `buildAudioDataObject()` test does not execute JS object construction; it source-inspects `rgbscriptv4.cpp` for legacy string literals. That can catch accidental deletion of legacy field names, but it does not validate runtime object shape, nested objects, numeric values, profile resolution, or QJSEngine behavior.

**What they catch well**

- Analyzer can create a channel and process one frame without crashing.
- A sine frame produces some nonzero band output.
- `Doc::audioProfileForFunction()` follows a custom `RGBMatrix` binding and falls back to default.
- Legacy field names appear in `buildAudioDataObject()` source before the null-channel guard.

**What they do not catch well**

- Real `AudioCapture → AudioAnalyzer → AudioChannel → AudioSnapshot`.
- Actual `buildAudioDataObject()` runtime shape.
- Nested JS audio object contract.
- Multiple profiles/channels under live analyzer updates.
- Profile deletion/rebinding while active.
- Trigger/envelope behavior across multiple frames.

---

# 2. Specific weak assertions

## AudioFrame weak assertions

### `audioframe_test.cpp:65`

```cpp
QVERIFY2(frame.rms > 0.04, qPrintable(QString("rms: %1").arg(frame.rms)));
```

**Why weak:** For `makeNoiseFrame(-20.0)`, this only checks that RMS is non-trivially positive. A noise generator with the wrong distribution, wrong amplitude scaling, or biased clipping could still pass.

**Recommended fix:** Assert an expected RMS range derived from the distribution. For uniform noise in `[-A, A]`, RMS should be approximately `A / sqrt(3)` before quantization/window effects.

---

### `audioframe_test.cpp:66`

```cpp
QVERIFY2(frame.peak > 0.09, qPrintable(QString("peak: %1").arg(frame.peak)));
```

**Why weak:** This accepts almost any generator that produces at least one large sample. It does not validate amplitude distribution, clipping behavior, or expected peak statistics.

**Recommended fix:** Check that peak is near the configured amplitude and not above the expected clipping range.

---

### `audioframe_test.cpp:67`

```cpp
QVERIFY2(frame.spectralFlatness > 0.5,
         qPrintable(QString("flatness: %1").arg(frame.spectralFlatness)));
```

**Why weak:** A flatness lower bound alone does not prove the noise spectrum is statistically flat across the analyzed frequency range. Many broken broadband-ish spectra could pass.

**Recommended fix:** Add a per-band distribution check: no large band holes, max/min or percentile ratio within a realistic tolerance over 32 bands.

---

### `audioframe_test.cpp:81`

```cpp
QVERIFY2(frame.spectralFlatness > 0.7,
         qPrintable(QString("flatness: %1").arg(frame.spectralFlatness)));
```

**Why weak:** This checks that the impulse spectrum is “somewhat flat,” but not that the impulse generator actually produces a single-sample spike with the expected FFT magnitude profile.

**Recommended fix:** Directly assert exactly one nonzero sample and validate the broad FFT magnitude distribution.

---

## AudioAnalyzer weak assertions

### `audioanalyzer_test.cpp:103`

```cpp
QVERIFY2(fuzzyCompare(frame.rmsDb, -20.0, 5.0),
         qPrintable(QString("rmsDb: %1").arg(frame.rmsDb)));
```

**Why weak:** ±5 dB is wide for RMS. A regression that changes amplitude scaling by nearly 2x could pass.

**Recommended fix:** Separate sample-domain RMS from windowed/spectral expectations. If the analyzer computes RMS from `frame.rms`, use a tight tolerance around the known generated frame RMS.

---

### `audioanalyzer_test.cpp:148`

```cpp
QVERIFY2(frame.spectralFlatness > 0.4,
         qPrintable(QString("flatness: %1 (expected close to 1.0)")
                        .arg(frame.spectralFlatness)));
```

**Why weak:** The comment says “close to 1.0,” but the assertion accepts values as low as 0.4. That could hide major spectral-shape regressions.

**Recommended fix:** Use deterministic noise and assert a tighter expected range, or test a synthetic magnitude-only flat spectrum where expected flatness is exactly near 1.0.

---

### `audioanalyzer_test.cpp:155`

```cpp
QVERIFY2(frame.spectralCentroidHz > 200.0 && frame.spectralCentroidHz < 5000.0,
         qPrintable(QString("centroidHz: %1 (expected midband)").arg(frame.spectralCentroidHz)));
```

**Why weak:** This accepts almost the entire analysis range. A centroid calculation biased heavily low or high could still pass.

**Recommended fix:** Use a controlled magnitude-only flat spectrum and assert expected centroid/rolloff numerically.

---

### `audioanalyzer_test.cpp:167`

```cpp
QVERIFY2(frame.crestFactor > 5.0,
         qPrintable(QString("crestFactor: %1 (expected >> 1 for impulse)")
                        .arg(frame.crestFactor)));
```

**Why weak:** A true single-sample impulse over 2048 samples should have a crest factor near `sqrt(2048) ≈ 45`. The threshold `> 5` would allow a badly smeared impulse.

**Recommended fix:** Assert near `sqrt(fftSize)` with an appropriate quantization tolerance.

---

### `audioanalyzer_test.cpp:172`

```cpp
QVERIFY2(headroomDb > 12.0,
         qPrintable(QString("peakDb-rmsDb headroom: %1 dB (expected >12 dB)").arg(headroomDb)));
```

**Why weak:** For a single-sample impulse in 2048 samples, peak-to-RMS headroom should be roughly `20*log10(sqrt(2048)) ≈ 33 dB`. `>12 dB` is too permissive.

**Recommended fix:** Assert a realistic range around the expected impulse headroom.

---

### `audioanalyzer_test.cpp:215`

```cpp
QVERIFY(first.spectralFlux > 0.0);
```

**Why weak:** This only verifies nonzero onset flux. It does not check the actual normalized value, despite the magnitudes being constructed directly.

**Recommended fix:** Assert the expected first-frame normalized flux behavior explicitly, or document why the first-frame denominator clamp intentionally produces a large value.

---

### `audioanalyzer_test.cpp:279`

```cpp
QVERIFY2(std::isfinite(small.spectralFlux),
         qPrintable(QString("flux not finite after FFT shrink: %1")
                        .arg(small.spectralFlux)));
```

**Why weak:** Finiteness catches crashes/NaN but not incorrect reset semantics. A wrong but finite flux value passes.

**Recommended fix:** After bin-count change, assert a defined contract: either flux resets to 0 for the first frame after resize, or it computes against zero history with a bounded expected value.

---

### `audioanalyzer_test.cpp:346`

```cpp
QVERIFY(peakEnergy > 0.0);
```

**Why weak:** This is only a nonzero check. It does not prove the expected band dominates, despite the comment saying it should.

**Recommended fix:** Compare the expected/peak band against neighboring and far-away bands, similar to `testSineWave()`.

---

## AudioChannel weak assertions

### `audiochannel_test.cpp:64`

```cpp
QVERIFY2(firstSub > 0.0, "Envelope must rise above zero after a loud frame");
```

**Why weak:** Any nonzero response passes, even if attack time is wildly wrong.

**Recommended fix:** Compute expected alpha:

```cpp
expected = raw * (1.0 - exp(-dtMs / attackMs));
```

Then assert within tolerance.

---

### `audiochannel_test.cpp:65`

```cpp
QVERIFY2(firstSub < 1.0, "Envelope must not jump straight to plateau");
```

**Why weak:** This only catches the most extreme smoothing failure. An implementation using the wrong time constant could still pass.

**Recommended fix:** Assert the first-frame envelope value numerically against the configured attack time.

---

### `audiochannel_test.cpp:79`

```cpp
QVERIFY2(plateau > firstSub * 1.5, "Envelope must converge well above the first-frame value");
```

**Why weak:** This does not verify convergence to the expected steady-state value. A broken envelope that drifts slowly upward could pass.

**Recommended fix:** Feed enough frames to reach steady state and assert the value is within tolerance of the raw band value after gain/gate/clamp.

---

### `audiochannel_test.cpp:92`

```cpp
QVERIFY2(last < plateau * 0.9, "Envelope must release noticeably below plateau");
```

**Why weak:** “Noticeably below” is not enough for release-time correctness. A release implementation 10x too slow could pass depending on the initial plateau.

**Recommended fix:** Assert expected exponential decay after N frames using configured `releaseMs` and `audioDtMs`.

---

### `audiochannel_test.cpp:110`

```cpp
QVERIFY2(quietGain > 5.0,
         qPrintable(QString("AGC must boost quiet input, got %1 dB").arg(quietGain)));
```

**Why weak:** For a -40 dB signal with `maxGainDb = 18`, the expected steady gain should approach the max. `>5 dB` accepts very weak AGC.

**Recommended fix:** After 200 frames, assert near `18 dB` or whatever the exact AGC contract says.

---

### `audiochannel_test.cpp:117`

```cpp
QVERIFY2(loudGain < quietGain - 1.0,
         qPrintable(QString("AGC must drop on loud input: quiet=%1 loud=%2")
                        .arg(quietGain).arg(loudGain)));
```

**Why weak:** It only checks direction. It does not verify the immediate drop-to-target behavior implemented in `AudioChannel::updateAgc()` for louder input.

**Recommended fix:** Assert the expected target gain for the loud frame, likely `clamp(-rmsDb, 0, maxGainDb)`.

---

### `audiochannel_test.cpp:297-298`

```cpp
QVERIFY2(updates > 10, "Should have run many update() calls");
QVERIFY2(snapshotCount.load() > 10, "Reader thread should have fetched many snapshots");
```

**Why weak:** This is a smoke test, not a real thread-safety test. It does not assert consistency invariants, run multiple readers, run under TSAN, or stress config updates/destroy.

**Recommended fix:** Add a stress test with multiple reader threads, writer updates, config updates, invariant checks on snapshot ranges/finite values, and ideally run under ThreadSanitizer in CI.

---

## AudioProfile weak assertions

### `audioprofile_test.cpp:157-158`

```cpp
AudioProfile *created = doc.ensureDefaultAudioProfile();
QVERIFY(created != nullptr);
```

**Why weak:** Non-null creation is basic plumbing. It does not validate runtime DSP behavior.

**Recommended fix:** This is fine as a Doc/profile test, but do not count it as DSP confidence. Add lifecycle tests involving active audio channels using profiles.

---

### `audioprofile_test.cpp:175-180`

```cpp
QTest::newRow("missing")  << QString()              << true;
QTest::newRow("zero")     << QStringLiteral("0")    << true;
QTest::newRow("one")      << QStringLiteral("1")    << true;
QTest::newRow("two")      << QStringLiteral("2")    << true;
QTest::newRow("negative") << QStringLiteral("-1")   << true;
QTest::newRow("garbage")  << QStringLiteral("abc")  << true;
```

**Why weak/risky:** Every version value is expected to load. That may be intentional compatibility behavior, but it means this test will not catch unsupported-version rejection bugs unless the intended contract is “load everything forever.”

**Recommended fix:** If permissive loading is intentional, add assertions that unknown/garbage versions still apply only safe defaults/known fields and do not silently corrupt config.

---

## AudioSlice weak assertions

### `audioslice_test.cpp:65-70`

```cpp
QVERIFY(!frame.silent);
QVERIFY(frame.rmsDb > -40.0);
QVERIFY(frame.rmsDb < 0.0);
QVERIFY(frame.bands32 != nullptr);
QVERIFY(frame.spectralCentroidHz > 500.0);
QVERIFY(frame.spectralCentroidHz < 1500.0);
```

**Why weak:** These are broad sanity checks. A substantially wrong analyzer could still produce a non-silent frame, some bands, and a centroid somewhere in this range for a 1 kHz tone.

**Recommended fix:** Assert the same precise values as the analyzer suite: RMS, crest, peak band, leakage, and rolloff.

---

### `audioslice_test.cpp:77-79`

```cpp
QVERIFY2(bandSum > 0.0, "All perceptual bands are zero after a -20 dBFS sine frame");
QVERIFY2(snap.bands.mid > 0.0,
         qPrintable(QString("mid band must respond to 1 kHz tone, got %1").arg(snap.bands.mid)));
```

**Why weak:** Any nonzero mid response passes. This does not verify that the correct perceptual band dominates or that smoothing/gain is correct.

**Recommended fix:** Assert expected band ordering and approximate first-frame smoothed value for the configured envelope and `audioDtMs`.

---

### `audioslice_test.cpp:84`

```cpp
QVERIFY(snap.features.crestFactor > 1.0);
```

**Why weak:** Almost any non-silent signal has crest factor > 1. This would not catch a badly scaled crest-factor calculation.

**Recommended fix:** Assert near `sqrt(2)` for a sine wave.

---

### `audioslice_test.cpp:93-96`

```cpp
QVERIFY2(std::isfinite(t.value), ...);
QVERIFY2(t.value >= 0.0, ...);
```

**Why weak:** Finiteness and non-negativity are useful smoke checks, but they do not validate trigger semantics.

**Recommended fix:** Drive a multi-frame transition and assert exact fire/release counts.

---

### `audioslice_test.cpp:101`

```cpp
QVERIFY(snap.audioDtMs >= 0.0);
```

**Why weak:** This only catches negative timing. It does not verify that `audioDtMs` equals `1000 * fftSize / sampleRate`.

**Recommended fix:** Assert the exact expected value for 2048/44100, approximately `46.44 ms`.

---

### `audioslice_test.cpp:195`

```cpp
QVERIFY2(legacyRegion.contains(field),
         qPrintable(QString("Legacy field %1 missing from buildAudioDataObject() "
                            "before the channel-null guard").arg(field)));
```

**Why weak:** This is source-string inspection, not a runtime test. It can pass even if the JS object has the wrong value type, wrong nested shape, wrong runtime engine behavior, or the source contains the string in dead/commented code.

**Recommended fix:** Execute `RGBScript::buildAudioDataObject()` through a minimal QJSEngine-backed fixture and inspect the resulting `QJSValue` object.

---

# 3. Missing critical tests

Prioritized by risk to real DSP correctness.

## Blocking / high-value missing tests

### 1. Sustained same sine wave for 200 frames: envelope stability

**Exists?** No.

There is a noise-floor test with 200 loud analyzer frames, but no channel envelope/volume stability test after 200 identical frames.

**Risk:** Envelope, AGC, or smoothing math could drift, saturate incorrectly, or fail to converge and current tests would likely not catch it.

---

### 2. Variable `audioDtMs` wall-clock correctness

**Exists?** No.

All channel tests effectively use fixed `kDtMs = 10.0`.

**Risk:** `AudioChannel::alpha(dtMs, tauMs)` could be replaced by frame-count-based smoothing and most tests would still pass.

---

### 3. Threshold-hover Schmitt hysteresis / no chatter

**Exists?** No.

Current trigger tests use clearly loud and silent frames, not values around `highThreshold` and `lowThreshold`.

**Risk:** Trigger chatter near threshold is one of the most visible live-lighting bugs.

---

### 4. Runtime `buildAudioDataObject()` JS object shape

**Exists?** No.

`audioslice_test.cpp` only source-inspects legacy fields.

**Risk:** RGB scripts could receive wrong/missing `audio.bands`, `audio.triggers`, `audio.volume`, `audio.music`, or `audio.features` at runtime while tests pass.

---

### 5. Multiple channels with different configs on same analyzer

**Exists?** No.

**Risk:** A bug where analyzer/channel state leaks between channels, or where all channels use default config, would not be caught.

---

### 6. Profile deletion/rebinding while channel/script active

**Exists?** No.

**Risk:** Active RGBMatrix/audio profile lifecycle bugs could crash in live use and current profile tests would not catch them.

---

## Non-blocking but important missing tests

### 7. Single-frame transient: silence → one loud frame → silence

**Exists?** Not exactly.

`testTriggerFired()` uses sustained loud input, then silence. It does not specifically test a one-frame impulse-like loud event between silence periods.

**Risk:** One-frame transients are common in beat/onset use cases. Envelope attack and trigger hold/cooldown behavior may differ from sustained loud input.

---

### 8. NaN/Inf and extreme numerical inputs

**Exists?** No.

**Risk:** Analyzer/channel code uses arithmetic on frame fields and magnitudes without explicit sanitization in the tested paths. Bad plugin/capture data could propagate NaN/Inf into snapshots/scripts.

---

### 9. Very small inputs near `1e-10`

**Exists?** No.

**Risk:** dB clamping, spectral flatness epsilon, flux denominator, and silence classification all depend on tiny-value behavior.

---

### 10. Spectral rolloff exactness

**Exists?** No meaningful test.

Analyzer computes `spectralRolloffHz`, but tests do not assert expected rolloff for controlled spectra.

---

### 11. Noise gate behavior

**Exists?** No direct test.

`fastConfig()` disables the gate effectively with `thresholdDb = -200.0` and huge hold. That avoids gate interference but leaves gate behavior untested.

---

### 12. Beat trigger behavior

**Exists?** No direct test.

`AudioChannel::updateTriggers()` maps `m_currentBeat` to trigger index 6, but tests do not verify beat trigger fire/release semantics.

---

# 4. Required scenario checklist

## 1. Feed 200 frames of the same sine wave — is envelope stable at the end?

**Test exists?** No.

There are repeated-frame tests, but none asserts final channel envelope/volume stability after 200 identical frames.

---

## 2. Feed silence, then 1 loud frame, then silence — does trigger fire exactly once?

**Test exists?** Partial, not exact.

`audiochannel_test.cpp::testTriggerFired()` verifies one fire for sustained loud input and one release after silence, but it does not test a single loud frame transient after an initial silent history.

---

## 3. Feed signal that hovers just at the trigger threshold — does Schmitt hysteresis prevent chatter?

**Test exists?** No.

No test constructs frames or snapshot values around `highThreshold`/`lowThreshold`.

---

## 4. Two channels with different configs on the same analyzer — do they produce different results?

**Test exists?** No.

No analyzer test creates two channels with different envelope/gain/band configs and compares their snapshots after the same frame.

---

## 5. Legacy slider migration: does `fromLegacySliders(5,5,50,5)` produce the exact values from the contracts doc?

**Test exists?** Yes.

`audioprofile_test.cpp:121-129` checks:

```cpp
const AudioChannelConfig config = AudioProfile::configFromLegacySliders(5, 5, 50, 5);

QVERIFY(fuzzyEqual(config.agc.inputGainLinear, 1.6));
const double expectedAttackMs = -40.0 / std::log(1.0 - 0.55);
QVERIFY(fuzzyEqual(config.envelope.attackMs, expectedAttackMs));
QVERIFY(fuzzyEqual(config.envelope.releaseMs, 4.0 * expectedAttackMs));
QVERIFY(fuzzyEqual(config.brightnessFloor, 0.5));
QVERIFY(fuzzyEqual(config.triggers.highThreshold, 0.25));
QVERIFY(fuzzyEqual(config.triggers.lowThreshold, 0.05));
```

This is a good exact-value test for that representative tuple.

---

## 6. `buildAudioDataObject()` — is the JS object shape tested at all?

**Test exists?** No, not at runtime.

`audioslice_test.cpp::testLegacyFieldsPreserved()` only checks source text for legacy field names. It does not execute `buildAudioDataObject()` or inspect the resulting JS object shape.

---

## 7. Profile deletion while channel is active — does it crash?

**Test exists?** No.

Profile registration/default resolution is tested, but active deletion/rebinding lifecycle is not.

---

## 8. Concurrent snapshot reads during channel updates — is there a real stress test?

**Test exists?** Partial smoke test only.

`audiochannel_test.cpp::testSnapshotThreadSafety()` has one reader thread and one update loop for 200 ms. It checks only that both loops ran more than 10 times. It does not assert snapshot invariants, use multiple readers, update configs concurrently, destroy channels, or run under TSAN.

---

# 5. Recommended new tests

## A. Exact envelope attack/release math

**Suite:** `audiochannel_test.cpp`

**Test:** `testEnvelopeMatchesExponentialAlpha`

**Procedure:**

1. Disable AGC/noise gate.
2. Use a deterministic frame with known band value.
3. Set `attackMs = 20`, `releaseMs = 200`.
4. Update once with `dtMs = 10`.
5. Assert:

```cpp
expected = raw * (1 - exp(-10 / 20));
```

6. Then switch to silence and assert exponential decay using release alpha.

**Catches:**

- Wrong alpha formula.
- Ignoring `audioDtMs`.
- Attack/release swapped.
- Frame-count-based smoothing.

---

## B. Wall-clock invariance for varying `audioDtMs`

**Suite:** `audiochannel_test.cpp`

**Test:** `testEnvelopeUsesWallClockTime`

**Procedure:**

Run two channels with same config/input:

- Channel A: four updates at `20 ms`
- Channel B: two updates at `40 ms`

After equal total time, snapshots should be approximately equal.

**Catches:**

- Smoothing tied to frame count instead of elapsed audio time.
- Incorrect `dtMs` handling.

---

## C. Sustained sine stability after 200 frames

**Suite:** `audiochannel_test.cpp` or `audioslice_test.cpp`

**Test:** `testSustainedSineEnvelopeStable`

**Procedure:**

1. Feed 200 identical sine frames.
2. Record envelope/volume for frames 190-200.
3. Assert change is below small epsilon.
4. Assert final value is near expected steady-state.

**Catches:**

- Envelope drift.
- AGC runaway.
- Clamp mistakes.
- Accumulation errors.

---

## D. Single-frame transient trigger

**Suite:** `audiochannel_test.cpp`

**Test:** `testSingleFrameTransientFiresOnce`

**Procedure:**

1. Warm up with silence.
2. Feed one loud frame configured to cross high threshold.
3. Feed silence for enough hold/release time.
4. Assert:
   - `firedThisFrame` true exactly once.
   - `releasedThisFrame` true exactly once.
   - No retrigger during cooldown.

**Catches:**

- Missed transient.
- Multiple fire events from one onset.
- Incorrect hold/release interaction.

---

## E. Threshold-hover hysteresis

**Suite:** `audiochannel_test.cpp`

**Test:** `testTriggerHysteresisPreventsChatter`

**Procedure:**

Construct frames or directly controlled `bands32` values that produce trigger values:

1. Below high threshold.
2. Slightly above high threshold: should fire once.
3. Oscillate between high and low thresholds: should remain active, no repeated fire.
4. Drop below low threshold after hold: should release once.

**Catches:**

- Missing Schmitt hysteresis.
- Trigger chatter around threshold.
- Incorrect high/low threshold use.

---

## F. Multiple analyzer channels with different configs

**Suite:** `audioanalyzer_test.cpp` or `audioslice_test.cpp`

**Test:** `testTwoChannelsDifferentConfigsSameFrame`

**Procedure:**

1. Create one `AudioAnalyzer`.
2. Add two channels:
   - Channel A: low gain / slow attack / brightness floor 0.0
   - Channel B: high gain / fast attack / brightness floor 0.4
3. Process same frame.
4. Assert snapshots differ in expected fields.

**Catches:**

- Config ignored.
- Shared mutable state across channels.
- Analyzer only updating default channel.
- Snapshot config leakage.

---

## G. Runtime `buildAudioDataObject()` shape

**Suite:** likely `audioslice_test.cpp` or RGBScript test fixture

**Test:** `testBuildAudioDataObjectRuntimeShape`

**Procedure:**

1. Initialize minimal `QJSEngine`/RGBScript environment.
2. Provide an active audio channel with known snapshot.
3. Call the real `buildAudioDataObject()`.
4. Assert:
   - Legacy fields: `spectrum`, `volume`, `beat`, `bpm`, `maxMagnitude`.
   - New fields: `version`, `bands`, `triggers`, `volume`, `music`, `features`, `audioDtMs`, `brightnessFloor`.
   - Nested fields have correct numeric values/types.
   - Null-channel case still contains legacy fields.

**Catches:**

- Runtime object shape regressions.
- Wrong field types.
- Missing nested objects.
- Profile/channel resolution failures.

---

## H. Controlled spectral rolloff

**Suite:** `audioanalyzer_test.cpp`

**Test:** `testSpectralRolloffFormula`

**Procedure:**

Create magnitude-only frames:

1. One in-band bin populated: rolloff should equal that bin frequency.
2. Two bins with known energy distribution: rolloff should cross at expected bin.

**Catches:**

- Using magnitude instead of squared energy.
- Wrong 85% threshold.
- Wrong frequency range.
- Off-by-one bin iteration.

---

## I. Flat-spectrum magnitude-only test

**Suite:** `audioanalyzer_test.cpp`

**Test:** `testFlatnessForControlledSpectrum`

**Procedure:**

1. Set all in-band magnitudes to same positive value.
2. Assert flatness near 1.0.
3. Set one narrow bin high and rest near epsilon.
4. Assert flatness near 0.

**Catches:**

- Broken geometric/arithmetic mean implementation.
- Wrong frequency inclusion.
- Epsilon mistakes.

---

## J. NaN/Inf sanitization

**Suite:** `audioanalyzer_test.cpp` and `audiochannel_test.cpp`

**Test:** `testInvalidNumericalInputsDoNotPoisonSnapshot`

**Procedure:**

Construct frames with:

- `rms = NaN`
- `peak = Inf`
- magnitudes containing NaN/Inf
- extremely tiny magnitudes around `1e-12`

Then assert output fields are finite and clamped according to contract.

**Catches:**

- Script-facing NaN/Inf propagation.
- Broken dB clamp behavior.
- Flatness/flux NaN poisoning.

---

## K. Profile deletion while active

**Suite:** `audioprofile_test.cpp` or integration suite

**Test:** `testActiveProfileDeletionFallsBackSafely`

**Procedure:**

1. Create `Doc`, default profile, custom profile, and RGBMatrix bound to custom profile.
2. Resolve/use active profile.
3. Delete/remove custom profile.
4. Assert function resolution falls back to default.
5. If possible, process audio/build script object after deletion and assert no crash.

**Catches:**

- Dangling profile pointers.
- Missing fallback.
- Runtime crash during script/audio update.

---

## L. Real snapshot concurrency stress

**Suite:** `audiochannel_test.cpp`

**Test:** `testSnapshotConcurrentStress`

**Procedure:**

1. Multiple reader threads repeatedly call `snapshot()`.
2. Writer thread alternates loud/silent/noise frames.
3. Optional config thread calls `updateConfig()`.
4. Run longer than 200 ms.
5. Assert invariants:
   - All band values finite and in `[0,1]`.
   - Trigger values finite and in `[0,1]`.
   - `audioDtMs >= 0`.
   - Brightness floor matches one of known configs.
6. Run under TSAN in CI if available.

**Catches:**

- Data races not visible as crashes.
- Partially updated snapshots.
- Config/snapshot inconsistencies.

---

# 6. Test data quality assessment

## Sine generator

`audioframe_test_utils.cpp:295-304` generates a straightforward sine wave:

```cpp
const double phase = kTwoPi * frequencyHz * double(i) / double(sampleRate);
samples[i] = toInt16(amplitude * std::sin(phase));
```

**Quality:** Good enough for broad DSP tests, but many chosen frequencies are not FFT-bin-centered for `2048 @ 44100`, so leakage is expected. Current tests account for that with broad band tolerances.

**Gap:** No test directly verifies the raw FFT peak lands at the expected bin or that leakage is within expected bounds for a bin-centered sine.

---

## Noise generator

`audioframe_test_utils.cpp:307-315` uses deterministic uniform random noise:

```cpp
std::mt19937 generator(0x514c4350u + static_cast<uint32_t>(frameIndex));
std::uniform_real_distribution<double> distribution(-linearFromDb(amplitudeDb), linearFromDb(amplitudeDb));
```

**Quality:** Deterministic and useful for repeatable tests.

**Gap:** Tests do not validate statistical flatness beyond broad spectral flatness lower bounds. No per-band distribution test exists. Also, uniform sample-domain noise has predictable RMS that should be asserted more tightly.

---

## Impulse generator

`audioframe_test_utils.cpp:318-324` creates a single spike:

```cpp
if (!samples.empty())
    samples[samples.size() / 2] = 32767;
```

**Quality:** The sample-domain impulse is correct: one spike at the center.

**Gap:** Because frame construction subtracts mean before analysis, the effective FFT input includes a tiny negative offset across other samples. That is probably acceptable, but tests should explicitly assert exactly one nonzero sample and expected crest/headroom near theoretical values.

---

## Synthetic frame realism

Current data types are mostly:

- silence
- one pure sine
- white noise
- impulse
- magnitude-only artificial bins

These are useful unit-test primitives, but they are not enough for live audio behavior. Missing realistic cases include:

- amplitude ramps
- beat-like transients
- threshold-hover signals
- mixed-frequency/chord signals
- sustained bass-heavy vs treble-heavy content
- clipped/distorted input
- very low-level near-noise-floor input

---

# 7. Integration boundary assessment

## `AudioCapture → AudioAnalyzer → AudioChannel → AudioSnapshot`

**Coverage:** Partial.

`audioslice_test.cpp::testEndToEndPipeline()` covers:

```text
AudioFrame → AudioAnalyzer → AudioChannel → AudioSnapshot
```

It does **not** cover `AudioCapture`.

**Risk:** Capture-produced frame fields, timing, sample/magnitude ownership, and real callback behavior are not validated.

---

## `AudioProfile → Doc → RGBScript` resolution

**Coverage:** Partial.

`audioslice_test.cpp::testProfileResolutionChain()` validates `Doc::audioProfileForFunction()` for an `RGBMatrix`.

It does **not** execute RGBScript or confirm that `buildAudioDataObject()` uses the resolved profile/channel correctly at runtime.

---

## Thread safety

**Coverage:** Smoke only.

`audiochannel_test.cpp::testSnapshotThreadSafety()` has one reader and one writer for 200 ms, but no invariant checking or sanitizer-backed stress.

---

## XML round-trip with non-default values

**Coverage:** Good.

`audioprofile_test.cpp::testXmlRoundTrip()` uses a comprehensive non-default config and compares nearly all fields. This is one of the better tests in the reviewed set.

---

# 8. Overall assessment

If someone introduced a regression in the DSP math, **some regressions would be caught**, but many important ones would not.

## Likely caught

- Silence producing nonzero bands/features.
- Gross sine centroid/band mapping breakage.
- Spectral flux formula changes for controlled magnitudes.
- FFT bin-count resize causing NaN/OOB-like behavior.
- XML round-trip dropping config fields.
- Trigger fire/release completely broken for simple loud/silent transitions.
- Snapshot locking causing obvious crashes.

## Likely not caught

- Wrong envelope attack/release constants that remain monotonic.
- Smoothing tied to frame count instead of `audioDtMs`.
- AGC with wrong target/release curve but still “boosts quiet.”
- Trigger chatter around thresholds.
- Rolloff formula regressions.
- Noise flatness/centroid regressions within broad ranges.
- Runtime JS audio object shape regressions.
- Profile deletion/rebinding crashes during active audio use.
- Multi-channel config leakage.
- NaN/Inf poisoning of script-facing audio fields.
- Long-run envelope/AGC drift.

## Bottom line

The current tests are **not merely feel-good**; several are meaningful, especially `testSpectralFluxFormula()`, `testBands32()`, `testNoiseFloorTracking()`, `testXmlRoundTrip()`, and the basic trigger lifecycle tests.

However, the suite does **not yet provide deep DSP confidence**. It over-relies on broad sanity ranges, monotonicity, nonzero/finite checks, and one-frame synthetic examples. The biggest quality gap is that dynamic behavior — envelopes, AGC, triggers, timing, and runtime script object construction — is not tested with exact expected values over realistic multi-frame sequences.