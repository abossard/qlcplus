# Fix Leftover #6 — AudioProfile Version validation on load

## Summary

Added explicit XML schema-version validation to `AudioProfile::loadXML()` so
that older, newer, and malformed `Version` attributes are handled gracefully
and produce informative warnings instead of being silently ignored.

## Changes

### `engine/src/audioprofile.cpp` — `loadXML()`
After parsing the root attributes (ID, Name, IsDefault) and before iterating
child elements, the loader now interprets the `Version` attribute:

| Input                     | Behavior                                                                                  |
| ------------------------- | ----------------------------------------------------------------------------------------- |
| Attribute missing         | Treated as legacy `version 0`; existing per-element default fallback covers missing tags. |
| `Version="1"`             | Supported; loaded normally.                                                               |
| `Version > 1` (e.g. `2`)  | `qWarning` logged ("…newer than supported (1). Some settings may be lost."), load continues. |
| `Version < 0`             | `qWarning` logged, treated as legacy version 0.                                           |
| Non-numeric (e.g. `abc`)  | `qWarning` logged, treated as legacy version 0.                                           |

The supported version is encoded as `constexpr int kSupportedVersion = 1`
inside the function. Loading is never aborted by a version mismatch — the
existing per-attribute default fallback (each `doubleAttribute()` call passes
the current default as fallback) already provides safe legacy migration.

### `engine/src/audioprofile.cpp` — `saveXML()`
Unchanged. Already emits `Version="1"` (line 280, pre-existing).

### `engine/test/audioprofile/audioprofile_test.{h,cpp}`
Added a parameterized `testVersionValidation()` (`_data()` + `QFETCH`) covering:

- `missing` Version attribute
- `Version="0"`
- `Version="1"`
- `Version="2"` (future)
- `Version="-1"` (negative)
- `Version="abc"` (garbage)

Each row writes a minimal AudioProfile XML, parses it via `loadXML()`, and
asserts:

1. `loadXML()` returns `true` (load is permissive — never aborts on version).
2. ID and Name round-trip correctly.
3. The `Envelope.Attack="33.0"` element is applied.
4. All other fields fall back to `AudioChannelConfig::defaults()`.

## Verification

```bash
cd build && cmake --build . --target audioprofile_test -j8 \
  && ./engine/test/audioprofile/audioprofile_test
```

Result: **13 passed, 0 failed** (5 ms). Warnings for `two`, `negative`, and
`garbage` rows show up as expected `QWARN` lines in the test log.

## Design notes / deviation from original plan

The plan in `plan-leftovers.md §6` proposed making `loadXML()` **fail** on any
unsupported version. The user task and the critique in
`review-plan-leftovers.md §6` together steered the implementation toward a
**permissive** policy:

- Forward compatibility: a `Version > 1` profile still loads with a clear
  warning, so opening a future-saved workspace doesn't drop the entire
  audio-profile node.
- Backward compatibility: missing/legacy versions parse with defaults, matching
  the existing fallback semantics already baked into each attribute reader.
- Negative / unparseable values are warned about and demoted to legacy rather
  than rejected outright.

This keeps the loader robust against forward/backward schema drift while still
surfacing diagnostic warnings to the log.
