# Fix: Duration = FadeIn + Hold Invariant

## The Rule
> **Duration = fadeIn + hold.** When fadeIn changes, hold stays, duration adjusts.
> FadeOut is separate (overlaps with the next step).

## Enforcement: Engine Helpers + Safe Adoption

Add convenience methods to `Function`. Editors, SpeedDial, MCP, and display code use them.
Raw setters remain for XML load, undo replay, and tempo conversion.

### New Engine API (`engine/src/function.h`)

```cpp
quint32 holdSpeed() const;                          // speedSubtract(duration, fadeIn)
void setHoldSpeed(quint32 hold);                    // duration = speedAdd(fadeIn, hold)
void setFadeInSpeedPreservingHold(quint32 fadeIn);  // old hold preserved, duration adjusts
```

**Important**: helpers must call the VIRTUAL `setDuration()` / `setFadeInSpeed()` so subclass side effects (signals, Tardis, etc.) are preserved.

---

## Verified Bug Inventory

| # | Location | Description | Severity |
|---|----------|-------------|----------|
| **B1** | `functioneditor.cpp` | `setFadeInSpeed` doesn't extend duration → hold silently lost | **High** |
| **B2** | `chasereditor.cpp` | Chaser common-mode fadeIn drops hold | **High** |
| **B3** | `chasereditor.cpp` | Chaser common-mode hold writes wrong duration | **High** |
| **B4** | `rgbmatrixeditor.cpp` | Sequence conversion: `duration - fadeIn` underflow | **Medium** |
| **B5** | `functioneditor.cpp` | hold getter can underflow | **Medium** |
| **B6** | `efx.cpp` | `loopDuration()` underflow → runtime impact | **Medium** |
| **B8** | `chaserstep.cpp` | XML load fallback underflow | **Low** |
| **B10** | MCP `create_sequences` | `holdTime` written as duration, ignoring fadeIn | **High** |
| **B11** | MCP `create_rgbmatrix` | Sets fadeIn alone, duration unchanged | **Medium** |
| **B12** | `vcspeeddial.cpp` | SpeedDial sets fadeIn, doesn't adjust duration | **High** |
| **B15** | `TimeUtils.js` / `TimeEditTool.qml` | Beat text input mis-parses `12/16`; invalid text becomes bad value | **Medium** |

### Removed/Downgraded from original plan
- ~~B7~~: `setTotalDuration` scaling is correct math, but rewrite derivation order for clarity.
- ~~B9~~: Tempo conversion doesn't create invalid state, only preserves it. Defensive guard only.
- ~~B14~~: PerStep Hold already updates duration correctly.
- B13: SpeedDial reset is non-blocking, follow-up.

---

## Implementation Checklist

### Phase 1: Engine Helpers

- [ ] Add `Function::holdSpeed()` using `speedSubtract(duration(), fadeInSpeed())`.
- [ ] Add `Function::setHoldSpeed(quint32 hold)` using `setDuration(speedAdd(fadeInSpeed(), hold))`.
- [ ] Add `Function::setFadeInSpeedPreservingHold(quint32 fadeIn)` preserving old `holdSpeed()` and then setting new duration.
- [ ] Add unit tests for normal values, zero, clamped negative hold, and ∞ sentinels.

**Files to change**: `engine/src/function.h`, `engine/src/function.cpp`, relevant engine test files/CMake entries.

**Verification**: helper tests prove `hold = speedSubtract(duration, fadeIn)`, duration becomes `fadeIn + hold`, negative hold clamps to 0, and virtual setters are used.

**Dependencies**: none.

**Estimated effort**: 2h.

### Phase 2: Safe Math

- [ ] Replace all raw `duration - fadeIn` calculations with `Function::speedSubtract()` / `holdSpeed()` (6 locations).
- [ ] Fix `EFX::loopDuration()` so fadeIn >= duration returns 0 instead of underflow.
- [ ] Fix `ChaserStep` XML load fallback for legacy duration < fadeIn.
- [ ] Fix RGBMatrix sequence conversion hold calculation.
- [ ] Rewrite Chaser total-duration scaling to derive scaled fadeIn and scaled hold independently, then recombine with `speedAdd()`.

**Files to change**: `engine/src/efx.cpp`, `engine/src/chaserstep.cpp`, `engine/src/chaser.cpp`, `qmlui/functioneditor.cpp`, `qmlui/rgbmatrixeditor.cpp`.

**Verification**: legacy/invalid data with `fadeIn > duration` loads and runs without huge durations; EFX loop duration clamps to 0; Chaser scaling preserves proportions without underflow.

**Dependencies**: Phase 1 for `holdSpeed()` convenience; `speedSubtract()` can be used immediately where available.

**Estimated effort**: 1h.

### Phase 3: Editor Adoption

- [ ] Update `FunctionEditor::setFadeInSpeed()` to call `setFadeInSpeedPreservingHold()`.
- [ ] Emit `fadeInSpeedChanged`, `durationChanged`, and `holdSpeedChanged` after compound edits.
- [ ] Add Tardis batching for compound fadeIn+duration edits so undo/redo is atomic.
- [ ] Fix `ChaserEditor` common-mode fadeIn to preserve hold.
- [ ] Fix `ChaserEditor` common-mode hold to store `duration = speedAdd(fadeIn, newHold)`.

**Files to change**: `qmlui/functioneditor.cpp`, `qmlui/functioneditor.h` if needed, `qmlui/chasereditor.cpp`, Tardis call sites if batching API integration is needed.

**Verification**: RGBMatrix/EFX editor fadeIn increase keeps hold unchanged and grows duration; fadeIn decrease shrinks duration; chaser common-mode never displays negative hold; Ctrl+Z undoes fadeIn+duration together.

**Dependencies**: Phases 1 and 2.

**Estimated effort**: 2.5h.

### Phase 4: SpeedDial Fix

- [ ] Update `VCSpeedDial::applyFunctionsTime()` fadeIn writes to use `setFadeInSpeedPreservingHold()`.
- [ ] Verify Add/Subtract mode preserves hold when fadeIn changes.
- [ ] Verify Multiply mode behavior: fadeIn scales as requested and resulting duration is `newFadeIn + oldHold`.

**Files to change**: `qmlui/vcspeeddial.cpp`.

**Verification**: SpeedDial setting fadeIn from 500→2000 with hold=500 produces duration=2500; multiply mode with fadeIn=500, hold=500, factor=2 produces fadeIn=1000, duration=1500.

**Dependencies**: Phase 1; Phase 2 recommended.

**Estimated effort**: 1h.

### Phase 5: MCP Fix

- [ ] Fix `create_sequences` so `holdTime` means actual hold, not total duration.
- [ ] Use `Function::setHoldSpeed(holdTime)` or explicit `duration = speedAdd(fadeIn, holdTime)`.
- [ ] Fix `create_rgbmatrix` so fadeIn-only updates also adjust duration and preserve hold.
- [ ] Add `hold` / `holdTime` and `totalDuration` to function query responses.
- [ ] Add MCP tests covering create/query round-trips.

**Files to change**: `mcp/tools/*create*`, `mcp/tools/query_tools.cpp`, `mcp/tools/vc_query_helpers.h` if serialization lives there, `mcp/test/*`.

**Verification**: MCP-created sequence with fadeIn=1000 and holdTime=500 queries as fadeIn=1000, hold=500, totalDuration/duration=1500; RGBMatrix fadeIn update never leaves duration below fadeIn.

**Dependencies**: Phase 1; Phase 2 recommended for shared safe math.

**Estimated effort**: 1.5h.

### Phase 6: Duration Display

- [ ] Add read-only duration label to RGBMatrix editor.
- [ ] Add read-only duration label to EFX editor.
- [ ] Add tooltip: `Duration = fade-in + hold. Fade-out overlaps with the next step.`
- [ ] Ensure labels update when fadeIn, hold, or tempo type changes.

**Files to change**: `qmlui/qml/RGBMatrixEditor.qml`, `qmlui/qml/EFXEditor.qml`, related editor/controller bindings if needed.

**Verification**: duration label always equals fadeIn + hold and updates live; tooltip explains why fadeOut is not included.

**Dependencies**: Phases 1–3, so displayed duration matches fixed editor behavior.

**Estimated effort**: 1h.

### Phase 7: Beat Editor Text Input

- [ ] Keep `TimeUtils.qlcStringToTime()` beat branch accepting fraction-only `N/D` input (`1/16`, `12/16`, `3/4`, `18/16`, etc.). Already implemented; add tests.
- [ ] Keep `TimeEditTool.updateTime()` rejecting `NaN` / invalid parsed values and reverting display. Already implemented.
- [ ] Add parametrized tests for fraction parsing, including whole-beat overflow and invalid denominators.
- [ ] Verify round-trip: type `12/16` → display normalizes correctly → value is 750 ms.

**Files to change**: `qmlui/js/TimeUtils.js`, `qmlui/qml/TimeEditTool.qml`, QML/JS test files or nearest existing UI utility test target.

**Verification**: fraction-only text input no longer becomes whole beats; invalid text does not update `timeValue`; `12/16` round-trips to `3/4`/750 ms in Beats mode.

**Dependencies**: none functionally; include in same PR/plan because beat fractions expose the same fadeIn/hold timing UX.

**Estimated effort**: 1h.

---

## Combined Test Plan

### Engine helpers
| fadeIn | duration | holdSpeed() | notes |
|-------|----------|-------------|-------|
| 500 | 1500 | 1000 | normal |
| 1000 | 500 | 0 | clamped |
| 0 | 1000 | 1000 | no fade |
| ∞ | ∞ | 0 | sentinel |
| 500 | ∞ | ∞ | infinite hold |

### Preserving fadeIn setter
| old fadeIn | old duration | new fadeIn | expected duration | expected hold |
|-----------|-------------|-----------|-------------------|---------------|
| 500 | 1500 | 1000 | 2000 | 1000 |
| 1000 | 2000 | 500 | 1500 | 1000 |
| 1000 | 1000 | 2000 | 2000 | 0 |
| 63 | 188 | 125 | 250 | 125 |
| 250 | 500 | 63 | 313 | 250 |

### Safe math / underflow
| scenario | expected |
|----------|----------|
| EFX duration=500, fadeIn=1000 | loopDuration=0 |
| Legacy ChaserStep XML duration=500, fadeIn=1000 | load succeeds, hold=0 |
| RGBMatrix conversion duration < fadeIn | no underflow, hold=0 |
| Chaser total-duration scaling | scaled duration = scaled fadeIn + scaled hold |

### Editor and SpeedDial
| action | expected |
|--------|----------|
| FunctionEditor fadeIn 500→2000, old hold=500 | duration=2500, hold=500 |
| FunctionEditor fadeIn 2000→500, old hold=500 | duration=1000, hold=500 |
| Chaser common hold=750, fadeIn=250 | duration=1000 |
| SpeedDial sets fadeIn=2000, old hold=500 | duration=2500, hold=500 |
| SpeedDial multiply 2x on fadeIn=500, hold=500 | fadeIn=1000, duration=1500, hold=500 |

### MCP round-trip
| create/update | query expectation |
|---------------|-------------------|
| Sequence fadeIn=1000, holdTime=500 | fadeIn=1000, hold=500, totalDuration=1500 |
| RGBMatrix existing hold=500, update fadeIn=1000 | totalDuration=1500 |
| Query functions | includes duration/totalDuration and hold/holdTime |

### Beat editor text input
| input | type | expected value | expected display |
|-------|------|----------------|------------------|
| `1/16` | Beats | 63 | `1/16` |
| `12/16` | Beats | 750 | `3/4` |
| `18/16` | Beats | 1125 | `1 1/8` |
| `3/4` | Beats | 750 | `3/4` |
| `1/3` | Beats | rejected | previous value |
| `abc` | Beats | rejected | previous value |

---

## Manual QA Checklist

- [ ] Open RGBMatrix editor; set hold to a visible value; increase/decrease fadeIn; confirm hold stays fixed and duration changes.
- [ ] Repeat in EFX editor.
- [ ] Use Chaser common timing mode; edit fadeIn and hold; confirm no negative hold and saved duration is correct.
- [ ] Use SpeedDial Add/Subtract and Multiply modes against a function with non-zero hold; confirm hold preservation.
- [ ] Save and reload a project with fadeIn/hold values; confirm no timing drift.
- [ ] Load a legacy/invalid project where duration < fadeIn; confirm no huge hold/duration appears.
- [ ] Create/query Sequence and RGBMatrix via MCP; confirm hold and totalDuration semantics.
- [ ] In beat mode, type `12/16`; confirm it displays as `3/4` and stores 750 ms.
- [ ] Type invalid beat text; confirm the editor reverts and does not emit a bad value.
- [ ] Verify undo/redo treats fadeIn+duration editor changes as one logical action.

---

## Effort Estimate

| Phase | Estimate |
|-------|----------|
| 1: Engine helpers + tests | 2h |
| 2: Safe math (6 locations) | 1h |
| 3: Editor adoption + Tardis batching | 2.5h |
| 4: SpeedDial fix | 1h |
| 5: MCP fix | 1.5h |
| 6: Duration display | 1h |
| 7: Beat editor text input tests/QA | 1h |
| Manual QA | 1h |
| **Total** | **~11h (~1.5 dev-days)** |

---

## What This Plan Does NOT Cover (separate work)

- Tardis undo history panel UI.
- Undo handler gaps outside the compound timing edit path (FixtureGroup, VCWidget states).
- SpeedDial reset behavior (non-blocking follow-up).
- Duration display in Function Manager list (follow-up).
- Broader beat editor redesign beyond the `N/D` fraction parsing and invalid-input guard.
