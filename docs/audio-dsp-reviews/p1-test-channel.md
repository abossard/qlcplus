# P1 — AudioChannel unit tests

## Scope

Cover the per-channel processing in `engine/audio/src/audiochannel.cpp` with
direct unit tests that exercise envelope smoothing, AGC, trigger state machine,
atomic config update and snapshot thread-safety, per the contracts defined in
`p05-contracts.md`.

## Files

- `engine/test/audiochannel/audiochannel_test.h`
- `engine/test/audiochannel/audiochannel_test.cpp`
- `engine/test/audiochannel/CMakeLists.txt`
- Registered in `engine/test/CMakeLists.txt` (`add_subdirectory(audiochannel)`).

The suite reuses the `AudioTestUtils` synthetic-frame generator from
`engine/test/audioframe/audioframe_test_utils.{h,cpp}` (sine, silence) — no new
fixture infrastructure required.

## Test design

`AudioChannel::update(frame, dtMs)` is called directly (no `AudioAnalyzer`
needed) so each test controls dt and the input deterministically. A
`fastConfig()` helper minimises smoothing constants and disables AGC + the
noise gate so behaviour is observable within a handful of frames; individual
tests override the fields they care about.

Trigger tests target the **volume trigger** (`snapshot().volumeTrigger`)
because `m_volumeSmoothed` follows `frame.rms` directly through
`volumeSmoothingMs`, which makes the trigger value predictable independent of
which FFT band a sine lands in.

| Test | What it asserts |
|------|-----------------|
| `testEnvelopeSmoothing` | First-frame envelope is in (0, 1); rises monotonically toward a plateau under loud sine; decays monotonically below the plateau under silence; `bands.low == (sub+bass)/2`. |
| `testAgc` | After 200 quiet frames AGC gain rises above 5 dB and stays ≤ `maxGainDb`; one loud frame drops the gain by ≥1 dB (capture branch is instant when `target < current`). |
| `testTriggerFired` | Exactly one `firedThisFrame` while loud; `active` stays true across 10 sustained loud frames with no re-fires; exactly one `releasedThisFrame` after dropping to silence. |
| `testTriggerCooldown` | After release, `cooldownRemainingMs > 0`; loud frames during the cooldown window produce 0 fires; after the cooldown elapses with silent input, a new loud burst re-fires exactly once. |
| `testTriggerHold` | After firing, `active` remains true for 9 silent steps (90 ms) when `holdMs = 100`; then transitions to `releasedThisFrame` and `!active`. |
| `testConfigUpdate` | `updateConfig()` is staged: `config()` reflects the new value but the next `snapshot()` still uses the old one until `update()` runs, after which the new `brightnessFloor` is applied. |
| `testSnapshotThreadSafety` | A reader thread calls `snapshot()` for ~200 ms while the main thread spams `update()` with alternating loud/silent frames; both threads make progress without crash or data race (smoke test, validates `QMutex` discipline). |
| `testBrightnessFloor` | `cfg.brightnessFloor = 0.3` is reflected in `snapshot().brightnessFloor`. |

## Build & run

```bash
cd build && cmake .. -Dqmlui=ON
cmake --build . --target audiochannel_test -j8
./engine/test/audiochannel/audiochannel_test
```

## Result

```
PASS   : AudioChannel_Test::testEnvelopeSmoothing()
PASS   : AudioChannel_Test::testAgc()
PASS   : AudioChannel_Test::testTriggerFired()
PASS   : AudioChannel_Test::testTriggerCooldown()
PASS   : AudioChannel_Test::testTriggerHold()
PASS   : AudioChannel_Test::testConfigUpdate()
PASS   : AudioChannel_Test::testSnapshotThreadSafety()
PASS   : AudioChannel_Test::testBrightnessFloor()
Totals: 10 passed, 0 failed, 0 skipped, 0 blacklisted, 216ms
```

All 8 tests green, runtime ~0.2 s. Suite is now part of the engine test
hierarchy and can be picked up by the global `check` target.

## Notes / follow-ups

- Per-band trigger fire-edge tests are not asserted explicitly; the volume
  trigger covers the same state machine with a more deterministic input. If
  per-band wiring regresses (e.g. wrong `triggerValues[i]` mapping), a dedicated
  band-trigger test could be added — left out to keep this suite focused on the
  state machine and to avoid coupling to FFT band-edge frequency choices.
- Thread-safety test is a smoke test (no race detector). Running under TSan in
  CI would strengthen it; not in scope here.
