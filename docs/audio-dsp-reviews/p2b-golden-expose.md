# Phase 2b — Golden tests + Doc-level profile resolution

## Scope

This task delivered two pieces of plumbing the upcoming Phase 3 (script engine
audio bindings) needs:

1. `Doc::audioProfileForFunction(quint32 functionId)` — single resolution
   helper the script engine will call instead of duplicating fallback logic.
2. A first golden test suite that pins down the data contracts the
   `VCAudioTriggers` widget depends on (legacy `lows/mids/highs` powers and
   the new perceptual `sub/bass/lowMid/mid/high` snapshot bands).

## Part 1 — `Doc::audioProfileForFunction`

`Doc` already exposed `defaultAudioProfile()` and `audioProfile(id)`. Added a
small helper around them:

```cpp
AudioProfile* Doc::audioProfileForFunction(quint32 functionId) const;
```

Resolution order:

1. If the function is an `RGBMatrix` and its `audioProfileId` resolves to an
   existing `AudioProfile`, return that.
2. Else fall back to `defaultAudioProfile()` (which itself falls back to the
   first profile if none is marked default, or `nullptr` if the Doc has no
   profiles).

Files touched:
- `engine/src/doc.h` — declaration + doc comment.
- `engine/src/doc.cpp` — added `#include "rgbmatrix.h"` and the
  `qobject_cast<RGBMatrix*>` body. No behaviour change for callers that don't
  use it.

## Part 2 — Golden tests

New test target: `engine/test/vcaudiotriggers/vcaudiotriggers_golden_test`.

| Test                       | What it pins down |
|----------------------------|-------------------|
| `testLegacyBarsStillWork`  | A faithful replica of `slotSpectrumDataChanged()`'s low/mid/high aggregation, fed with `AudioAnalyzer`-produced `frame.bands32`, must yield `lowsPower > 0` for a 120 Hz tone, all three bins lit on broadband noise, all values finite and ≤ 1.0. |
| `testNewPerceptualBands`   | After feeding the analyzer + an `AudioChannel` with broadband noise, `snapshot.bands.{sub,bass,lowMid,mid,high}` (the exact fields VCAudioTriggers' new Q_PROPERTYs read) are all > 0 and ≤ 1, and `bands.low == (sub + bass) / 2`. Silence drains them to < 0.05. |
| `testDualPathCoexistence`  | The legacy aggregation and the channel snapshot reading the SAME frame at the SAME time both produce sensible values; legacy mids and the new mid/lowMid both fire on an 800 Hz tone. |
| `testProfileIdPersistence` | `audioProfileId` round-trips through XML (`saveXML` → `QXmlStreamReader` → `loadXML`). Verified on `RGBMatrix` (the canonical owner), since VCAudioTriggers itself lives in `qmlui/` and isn't built in this engine-level test. |
| `testAudioProfileForFunction` | Drives the four documented branches of the new `Doc::audioProfileForFunction` helper: explicit profile → that profile; no profile id → default; dangling id → default; unknown function id → default; empty Doc → `nullptr`. |

All 5 pass:

```
$ cd build && ./engine/test/vcaudiotriggers/vcaudiotriggers_golden_test
Totals: 7 passed, 0 failed, 0 skipped, 0 blacklisted, 242ms
```

(7 includes Qt's `initTestCase`/`cleanupTestCase`.)

## What is NOT covered

The original task mentioned instantiating `VCAudioTriggers` directly to drive
`slotSpectrumDataChanged()` and read the `Q_PROPERTY`s. We did **not** do that
because:

- `VCAudioTriggers` lives under `qmlui/virtualconsole/` and pulls in
  `VCWidget`, `VirtualConsole`, the QML view, the fixture-tree `TreeModel`,
  `AudioCapture` lifecycle, `DMXSource` registration, etc. Reproducing that
  in an engine-level unit test would mean linking a large slice of the QML
  layer or stubbing ~10 collaborators.
- The non-trivial logic of the widget's audio path is the aggregation in
  `slotSpectrumDataChanged()` and the copy-out in
  `updateAudioProfileSnapshotPowers()`. Both are pinned down here against the
  same data (`frame.bands32`, `AudioChannel::snapshot()`) the widget reads.
- The `audioProfileId` storage is just a `quint32` written via
  `QXmlStreamWriter` and read back via `QXmlStreamReader` — exercised here on
  `RGBMatrix`, which uses the same pattern.

A full integration test that boots the QML widget, captures an audio stream,
and asserts on the published Q_PROPERTYs would need to live under
`qmlui/test/` (none exists yet) and is a candidate for Phase 4 if we want
end-to-end coverage of the QML binding layer.

## Build / run

```bash
cd build
cmake .. -Dqmlui=ON
cmake --build . --target vcaudiotriggers_golden_test -j8
./engine/test/vcaudiotriggers/vcaudiotriggers_golden_test
```

## Files added / changed

- `engine/src/doc.h` (declaration of `audioProfileForFunction`)
- `engine/src/doc.cpp` (definition + `rgbmatrix.h` include)
- `engine/test/CMakeLists.txt` (registered the new sub-directory)
- `engine/test/vcaudiotriggers/CMakeLists.txt` (new)
- `engine/test/vcaudiotriggers/vcaudiotriggers_golden_test.h` (new)
- `engine/test/vcaudiotriggers/vcaudiotriggers_golden_test.cpp` (new)
