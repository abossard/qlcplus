# Deep Tests Added From Test-Quality Review

Five high-priority gaps from `review-test-quality.md` were closed by adding tests
to `engine/test/audiochannel/audiochannel_test.cpp`. All of them exercise
behavior that the previous suite only covered with weak monotonicity / "above
zero" assertions.

## Results

`./engine/test/audiochannel/audiochannel_test` — **15 passed / 0 failed.**

| Test | Result |
| --- | --- |
| testEnvelopeSmoothing (existing) | PASS |
| testAgc (existing) | PASS |
| testTriggerFired (existing) | PASS |
| testTriggerCooldown (existing) | PASS |
| testTriggerHold (existing) | PASS |
| testConfigUpdate (existing) | PASS |
| testSnapshotThreadSafety (existing) | PASS |
| testBrightnessFloor (existing) | PASS |
| **testEnvelopeExactAlpha (new)** | PASS |
| **testEnvelopeSteadyState (new)** | PASS |
| **testTriggerSchmittNoChatter (new)** | PASS |
| **testFrameRateIndependence (new)** | PASS |
| **testMultiChannelIsolation (new)** | PASS |

`./engine/test/audioanalyzer/audioanalyzer_test` — **11 passed / 0 failed**
(unchanged, run as a regression check).

## What each new test catches

### 1. `testEnvelopeExactAlpha`
- Pins the envelope recurrence to the contract formula
  `α = 1 - exp(-dt/τ)` and `env += α·(input − env)`.
- Feeds one frame with `bands32` constant at 0.8, `attackMs=20`, `dt=10`, AGC
  off, gain 1.0 → expects 0.31478 ±0.001.
- Then feeds zeros, `releaseMs=200`, `dt=10` → expects 0.29942 ±0.001.
- **Catches:** any drift in the smoothing filter (e.g. swapping
  attack/release, dropping the `1 − exp` form, off-by-one in dt scaling,
  accidentally squaring α). The previous suite only checked monotonicity, so
  it would happily accept a bug that halved the time constant.

### 2. `testEnvelopeSteadyState`
- 200 identical frames at `dt=40`, `attackMs=25`, `releaseMs=180`.
- Records frames 190–200; asserts max−min < 1e-3 and final value within 1 % of
  the input band value.
- **Catches:** unstable filters that chatter at steady state, residual offsets
  (`env` not converging to input), or wrong sign on the update term that would
  asymptote below the input.

### 3. `testTriggerSchmittNoChatter`
- 50 frames alternating 0.55/0.45 between thresholds (0.6 / 0.4) — must never
  fire and never go active.
- Single 0.65 frame fires and activates.
- 20 more alternations between 0.55/0.45 — Schmitt holds active, never
  re-fires.
- One 0.35 frame releases.
- **Catches:** any regression that drops the hysteresis (e.g. comparing both
  edges to the same threshold, or only checking `≥ low`), and any bug that
  re-fires while still active.

### 4. `testFrameRateIndependence`
- Two channels with identical config (`attackMs=50`, `releaseMs=200`).
- A: 20 frames @ 20 ms; B: 10 frames @ 40 ms (both 400 ms wall time).
- Final envelopes must match within ±5 %.
- **Catches:** any change that ties the smoothing to frame count instead of
  wall-clock dt (a common regression when refactoring filters or moving from
  a fixed-rate to a variable-rate scheduler).

### 5. `testMultiChannelIsolation`
- Two `AudioChannel`s with very different attack/release/threshold configs.
- Same loud frame fed to both: A's faster attack ⇒ A's envelope > B's; A's
  lower threshold ⇒ A trigger active, B's not.
- Then 10 frames of silence: A's faster release ⇒ A's envelope < B's.
- **Catches:** any accidental sharing of state between channel instances
  (statics, globals), or config look-ups bleeding between channels. Also
  guards the contract that each channel's snapshot reflects only its own
  configuration.

## Implementation notes

- New helper `makeBandsFrame(value, storage[32])` constructs a minimal
  `AudioFrame` with all 32 spectral bins set to a known value. The caller
  owns the backing array, matching `AudioFrame`'s non-owning pointer
  contract.
- New helper `makeRmsFrame(rms)` constructs a frame with controlled RMS but
  no spectral content, used to drive the volume trigger directly without
  per-band smoothing artefacts.
- Helper `exactMathConfig()` disables AGC, sets `inputGainLinear=1.0`,
  drops the noise gate threshold to −200 dB, and shrinks
  `volumeSmoothingMs` to 1 ms so the math is deterministic.
