# P2A auto default profile + tests

Implemented `Doc::ensureDefaultAudioProfile()` and AudioProfile model unit tests.

## Changes

- Added `Doc::ensureDefaultAudioProfile()`.
- When no audio profiles exist, it creates `Default Audio` with:
  - `isDefault=true`
  - `AudioChannelConfig::defaults()`
- If profiles already exist, it returns the existing default profile, falling back to the first profile through `defaultAudioProfile()`.
- Added `engine/test/audioprofile/audioprofile_test.{h,cpp}`.
- Registered `audioprofile_test` in `engine/test/CMakeLists.txt`.
- Aligned legacy migration with the contracts doc: release is `4 * attackMs`, and low trigger threshold is `max(0, high - 0.20)`.

## Test coverage

- Profile creation and default config values.
- XML save/load round trip with non-default config values.
- Legacy slider migration formulas for `(5, 5, 50, 5)`.
- Doc registration lookup/default/list behavior.
- `ensureDefaultAudioProfile()` create-once behavior.

## Verification

```bash
cd build && cmake .. -Dqmlui=ON && cmake --build . --target audioprofile_test -j8 && ./engine/test/audioprofile/audioprofile_test
```

Result: passed (`7 passed, 0 failed`). Qt emitted existing-style Doc teardown timer warnings during Doc-backed tests, but the test run completed successfully.
