# Fix: Beat Arithmetic Accuracy in QLC+ v5

## The Problem

Beat values are stored as integers where 1 beat = 1000. The canonical sixteenth table is:
```
s_beatSixteenths = {0, 63, 125, 188, 250, 313, 375, 438, 500, 563, 625, 688, 750, 813, 875, 938}
```

These are `round(n × 62.5)`. But `63 + 63 = 126 ≠ 125`. Plain integer addition
drifts off the canonical grid, causing cascading bugs in display, conversion, and playback.

## Fixes

### Fix 1: `beatsToTime` rounding (BLOCKING — affects real playback)

**File**: `engine/src/function.cpp:592`

Current: implicit float→uint truncation
```cpp
return ((float)beats / 1000.0) * beatDuration;  // truncates: 63*500/1000 = 31.5 → 31
```

Fix:
```cpp
return qRound(((double)beats / 1000.0) * beatDuration);  // rounds: 31.5 → 32
```

- **Verify**: `beatsToTime(63, 500)` returns 32 (not 31)
- **Verify**: `beatsToTime(125, 500)` returns 63 (not 62)
- **Impact**: fixes real DMX timing in chaserrunner and rgbmatrix playback

### Fix 2: Accurate beat arithmetic via sixteenths domain (BLOCKING)

**Files**: `engine/src/function.h`, `engine/src/function.cpp`

Add helpers that convert to sixteenths, operate, and convert back:

```cpp
// Convert internal beat value to sixteenths count
static int beatToSixteenths(quint32 beatValue);
// Convert sixteenths count to canonical beat value
static quint32 sixteenthsToBeat(int sixteenths);
// Beat-aware add: convert to sixteenths, add, convert back
static quint32 beatSpeedAdd(quint32 a, quint32 b);
// Beat-aware subtract: convert to sixteenths, subtract, convert back (clamp to 0)
static quint32 beatSpeedSubtract(quint32 a, quint32 b);
```

Logic:
```cpp
int Function::beatToSixteenths(quint32 beatValue) {
    int whole = beatValue / 1000;
    int frac = beatValue % 1000;
    int best = 0;
    for (int i = 1; i < 16; i++) {
        if (qAbs(frac - s_beatSixteenths[i]) < qAbs(frac - s_beatSixteenths[best]))
            best = i;
    }
    // snap-up: if closer to next whole beat than to 15/16
    if (qAbs(frac - 1000) < qAbs(frac - s_beatSixteenths[best])) {
        whole++;
        best = 0;
    }
    return whole * 16 + best;
}

quint32 Function::sixteenthsToBeat(int sixteenths) {
    if (sixteenths <= 0) return 0;
    int whole = sixteenths / 16;
    int rem = sixteenths % 16;
    return whole * 1000 + s_beatSixteenths[rem];
}

quint32 Function::beatSpeedAdd(quint32 a, quint32 b) {
    if (a == infiniteSpeed() || b == infiniteSpeed()) return infiniteSpeed();
    return sixteenthsToBeat(beatToSixteenths(a) + beatToSixteenths(b));
}

quint32 Function::beatSpeedSubtract(quint32 a, quint32 b) {
    if (a == infiniteSpeed()) return infiniteSpeed();
    if (b == infiniteSpeed()) return 0;
    int sa = beatToSixteenths(a);
    int sb = beatToSixteenths(b);
    return (sb >= sa) ? 0 : sixteenthsToBeat(sa - sb);
}
```

- **Verify**: `beatSpeedAdd(63, 63)` = 125 (not 126)
- **Verify**: `beatSpeedAdd(938, 63)` = 1000
- **Verify**: `beatSpeedSubtract(125, 63)` = 63

### Fix 2b: Dispatch via tempoType at call sites

Update Phase 1 helpers to use beat-aware math when in Beats mode:

**File**: `engine/src/function.cpp`

```cpp
void Function::setFadeInSpeedPreservingHold(quint32 fadeIn) {
    quint32 h = holdSpeed();
    setFadeInSpeed(fadeIn);
    if (tempoType() == Beats)
        setDuration(beatSpeedAdd(fadeIn, h));
    else
        setDuration(speedAdd(fadeIn, h));
}

void Function::setHoldSpeed(quint32 hold) {
    if (tempoType() == Beats)
        setDuration(beatSpeedAdd(fadeInSpeed(), hold));
    else
        setDuration(speedAdd(fadeInSpeed(), hold));
}

quint32 Function::holdSpeed() const {
    if (tempoType() == Beats)
        return beatSpeedSubtract(duration(), fadeInSpeed());
    return speedSubtract(duration(), fadeInSpeed());
}
```

### Fix 2c: Update other call sites

| File | Line | Current | Fix |
|------|------|---------|-----|
| `chasereditor.cpp:669` | `step.duration = step.fadeIn + step.hold` | Use `beatSpeedAdd` when chaser tempoType is Beats |
| `chaser.cpp:239` | `step.fadeIn = speedSubtract(...)` | Use `beatSpeedSubtract` when tempoType is Beats |
| `chaserstep.cpp:260` | `hold = speedSubtract(...)` | Use `beatSpeedSubtract` (needs tempoType passed in or defaulted) |
| `mcp/tools/function_tools.cpp:559` | `static_cast<uint>(rawFadeIn * 1000.0)` | Use `qRound` + snap to canonical via `sixteenthsToBeat` |

### Fix 3: RGB Matrix preview sub-beat timing (BLOCKING)

**File**: `qmlui/rgbmatrixeditor.cpp` (~line 896-911)

Current: preview only advances on whole-beat pulses (`m_gotBeat`).
1/16 duration shows as 1-step-per-beat.

Fix: convert beat duration to ms and use tick-driven accumulator:

```cpp
void RGBMatrixEditor::slotPreviewTimeout()
{
    uint effectiveDuration = m_matrix->duration();
    if (m_matrix->tempoType() == Function::Beats)
        effectiveDuration = Function::beatsToTime(effectiveDuration, m_currentBeatDuration);

    m_previewElapsed += MasterTimer::tick();
    if (m_previewElapsed >= effectiveDuration) {
        m_previewElapsed = 0;
        // advance step...
    }
}
```

- **Verify**: 1/16 duration at 120 BPM → steps advance every ~32ms, not every 500ms

### Fix 4: Formatter tolerance (NON-BLOCKING)

**File**: `qmlui/js/TimeUtils.js` — `timeToQlcString` beat branch (lines 279-294)

Replace exact `===` matching with ±1 tolerance or iterate the canonical array:

```js
// Instead of: if (value === 125) timeString += " 1/8";
// Use: find closest canonical value
var subBeat = [0, 63, 125, 188, 250, 313, 375, 438, 500, 563, 625, 688, 750, 813, 875, 938];
var labels = ["", "1/16", "1/8", "3/16", "1/4", "5/16", "3/8", "7/16",
              "1/2", "9/16", "5/8", "11/16", "3/4", "13/16", "7/8", "15/16"];
var best = 0;
for (var i = 1; i < 16; i++) {
    if (Math.abs(value - subBeat[i]) < Math.abs(value - subBeat[best]))
        best = i;
}
if (best > 0) timeString += " " + labels[best];
```

- **Verify**: value 126 displays as "1/8" (snaps to nearest 125)
- No risk of time-mode collision since this branch only runs when type===Beats

### Fix 5: `timeToBeats` snap-up to next whole beat (NON-BLOCKING)

**File**: `engine/src/function.cpp` — `timeToBeats` (~line 558-585)

Current snap loop only considers `s_beatSixteenths[0..15]` (0 to 938).
Value 998 snaps to 938 instead of 1000.

Fix: add 1000 as a snap candidate. If 1000 wins, increment whole beat count.

Already handled by `beatToSixteenths()` in Fix 2 (includes snap-up logic).
Just make `timeToBeats` use `beatToSixteenths` internally for consistency.

---

## Dependencies

```
Fix 1 (beatsToTime rounding) ← independent
Fix 5 (timeToBeats snap-up)  ← independent
Fix 2 (beat arithmetic)      ← independent
Fix 3 (preview timing)       ← depends on Fix 1
Fix 4 (formatter tolerance)  ← independent
```

## Implementation Order

1. Fix 1: `beatsToTime` qRound (5 min)
2. Fix 2: `beatSpeedAdd/Subtract` + `beatToSixteenths/sixteenthsToBeat` (1.5h)
3. Fix 2b: Update `holdSpeed/setHoldSpeed/setFadeInSpeedPreservingHold` to dispatch (30 min)
4. Fix 2c: Update call sites (chasereditor, chaser, chaserstep, MCP) (30 min)
5. Fix 5: `timeToBeats` snap-up via `beatToSixteenths` (15 min)
6. Fix 3: Preview sub-beat timing (30 min)
7. Fix 4: Formatter tolerance (15 min)

## E2E Tests

### Test 1: beatsToTime round-trip (C++, headless)

| Beat value | beatDuration | Expected time | Round-trip beat |
|-----------|-------------|---------------|----------------|
| 63 | 500 | 32 | 63 |
| 125 | 500 | 63 | 125 |
| 250 | 500 | 125 | 250 |
| 500 | 500 | 250 | 500 |
| 938 | 500 | 469 | 938 |
| 1000 | 500 | 500 | 1000 |
| 63 | 333 | 21 | 63 |
| 500 | 333 | 167 | 500 |

Must fail today (truncation), pass after Fix 1.

### Test 2: Beat addition is canonical (C++, headless)

| Expression | Expected |
|-----------|----------|
| `beatSpeedAdd(63, 63)` | 125 |
| `beatSpeedAdd(63, 125)` | 188 |
| `beatSpeedAdd(938, 63)` | 1000 |
| `beatSpeedAdd(500, 500)` | 1000 |
| `beatSpeedAdd(813, 250)` | 1063 |
| `beatSpeedAdd(938, 938)` | 1875 |
| `beatSpeedSubtract(125, 63)` | 63 |
| `beatSpeedSubtract(1000, 938)` | 63 |
| `beatSpeedSubtract(63, 125)` | 0 |

Must fail today (plain addition), pass after Fix 2.

### Test 3: timeToBeats snap-up (C++, headless)

| Input (internal) | Expected beat value |
|-----------------|---------------------|
| 998 | 1000 |
| 969 | 1000 |
| 953 | 938 |

Must fail today (snaps to 938), pass after Fix 5.

### Test 4: Function helper round-trip (C++, headless)

```cpp
Function f;
f.setTempoType(Function::Beats);
f.setFadeInSpeed(63);   // 1/16
f.setHoldSpeed(63);     // 1/16
QCOMPARE(f.duration(), 125u);  // canonical 1/8, not 126
```

### Test 5: Chaser Beat→Time→Beat (MCP script)

Create chaser with beat steps, switch tempo to Time, switch back to Beats.
Verify fadeIn and hold return to exact original values.

### Test 6: MCP round-trip (MCP script)

Create function with beat timing, query back, verify values on canonical grid.

## Effort

| Fix | Estimate |
|-----|----------|
| Fix 1 | 5 min |
| Fix 2 + 2b + 2c | 2.5h |
| Fix 3 | 30 min |
| Fix 4 | 15 min |
| Fix 5 | 15 min |
| Tests | 1.5h |
| **Total** | **~5h** |

## What This Plan Does NOT Cover

- Changing the canonical table (62 vs 63) — breaks all workspaces
- Sixteenths-int storage — massive refactor, separate project
- Beat editor UI redesign beyond fraction input
- Tardis undo history panel

---

## Appendix: Review Findings & Plan Updates

### Blocking Fix: Snap-up operator
`beatToSixteenths` must use `<=` not `<` for the snap-up comparison.
At frac=969, distance to 938 = 31, distance to 1000 = 31. `<=` snaps up (half-up convention).

### Blocking Fix: Member-level dispatch helpers

Instead of fixing 25 individual call sites, add TWO member-level helpers to `Function`:

```cpp
quint32 Function::addSpeed(quint32 a, quint32 b) const {
    return (tempoType() == Beats) ? beatSpeedAdd(a, b) : speedAdd(a, b);
}

quint32 Function::subtractSpeed(quint32 a, quint32 b) const {
    return (tempoType() == Beats) ? beatSpeedSubtract(a, b) : speedSubtract(a, b);
}
```

Then mechanically replace all 25 call sites:
- `Function::speedAdd(a, b)` → `m_function->addSpeed(a, b)` (or `func->addSpeed(a,b)`)
- `Function::speedSubtract(a, b)` → `m_function->subtractSpeed(a, b)`

For static contexts (e.g., ChaserStep XML load where no Function* is available), keep using
`speedSubtract` and do a post-load fixup in `Chaser::postLoad()`.

### Complete call site audit (25 sites)

**speedAdd (13 sites)**:
- `function.cpp:761` — `setHoldSpeed` → uses `this->addSpeed`
- `function.cpp:768` — `setFadeInSpeedPreservingHold` → uses `this->addSpeed`
- `functioneditor.cpp:193` — → `m_function->addSpeed`
- `functioneditor.cpp:227` — → `m_function->addSpeed`
- `chasereditor.cpp:545` — → `m_chaser->addSpeed`
- `chasereditor.cpp:554` — → `m_chaser->addSpeed`
- `chasereditor.cpp:571` — → `m_chaser->addSpeed`
- `chasereditor.cpp:579` — → `m_chaser->addSpeed`
- `chasereditor.cpp:799` — → `m_chaser->addSpeed`
- `chasereditor.cpp:822` — → `m_chaser->addSpeed`
- `mcp/function_tools.cpp:669` — → `seq->addSpeed`

**speedSubtract (12 sites)**:
- `function.cpp:756` — `holdSpeed()` → uses `this->subtractSpeed`
- `efx.cpp:148` — → delegate to `Function::holdSpeed()` or use `this->subtractSpeed`
- `chaserstep.cpp:260` — static, keep `speedSubtract` + post-load fixup
- `chaser.cpp:239` — → `m_steps[i]` doesn't have tempoType; use parent chaser's
- `chasereditor.cpp:460` — → `m_chaser->subtractSpeed`
- `chasereditor.cpp:470` — → `func->subtractSpeed`
- `chasereditor.cpp:600` — → `m_chaser->subtractSpeed`
- `chasereditor.cpp:771` — → `m_chaser->subtractSpeed`
- `functioneditor.cpp:171` — → `m_function->subtractSpeed`

### Blocking Fix: Preview beat duration source

`m_currentBeatDuration` does NOT exist. Instead, inline the call each tick:

```cpp
uint beatDur = m_doc->masterTimer()->beatTimeDuration();
uint effectiveDuration = Function::beatsToTime(m_matrix->duration(), beatDur);
```

No new member needed. Remove the old `m_gotBeat`-only branch entirely.

### Non-blocking: ChaserStep XML load
Defer fixup to `Chaser::postLoad()` after tempoType is known.
Don't change `ChaserStep::loadXML` signature.

### Non-blocking: `defaultSpeed()` sentinel
Add short-circuit in `beatSpeedAdd/Subtract`: treat `defaultSpeed()` same as `infiniteSpeed()`.

### Test 3 update
Add row: frac=969 → expected 1000 (with `<=` operator).
