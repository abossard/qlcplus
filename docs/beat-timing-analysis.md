# Beat-Based Timing Analysis: Bugs and Gaps

> Generated 2026-04-29. Verified against source code across engine, QML UI, and MCP layers.
> Rubber-ducked with Opus 4.6 subagents — all 8 bugs confirmed.

## How Beat Encoding Works

Beat values are plain `uint` — **1000 = 1 beat**, quantized to a 1/16th note grid.
There is NO tag in the value itself; interpretation depends on `Function::tempoType()`.

| Musical value | Internal encoding | Notes |
|---|---|---|
| 1/16 beat | 63 | `s_beatSixteenths[1]` |
| 1/8 beat | 125 | `s_beatSixteenths[2]` |
| 1/4 beat | 250 | `s_beatSixteenths[4]` |
| 1/2 beat | 500 | `s_beatSixteenths[8]` |
| 1 beat | 1000 | whole beat |
| 2 beats | 2000 | etc. |
| 0 | 0 | instant / no fade |
| infinity | 4294967294 | `(uint)-2` |
| default | 4294967295 | `(uint)-1`, "use function's own" |

Key conversion functions in `engine/src/function.cpp`:
- `timeToBeats(ms, beatDuration)` — ms to beat encoding (line 558)
- `beatsToTime(beats, beatDuration)` — beat encoding to ms (line 587)
- `musicalBeatValue(count, subdivision)` — e.g. `(3, 4)` = 750 (line 595)
- `beatValueToMusical(value)` — decompose to `{count, subdivision}` (line 622)

---

## CONFIRMED BUGS

### BT1: FunctionEditor::setTempoType() DOUBLE-CONVERTS speed values

- **File**: `qmlui/functioneditor.cpp:144-187`
- **Verified**: YES
- **The bug**: When toggling Time->Beats:
  1. Line 151: `m_function->setTempoType(Function::Beats)` — internally calls
     `setFadeInSpeed(timeToBeats(fadeInSpeed(), beatTime))` converting ms to beats
  2. Lines 158-160: reads the ALREADY-converted beat values and converts AGAIN:
     `uint fadeIn = Function::timeToBeats(m_function->fadeInSpeed(), beatDuration)`
  3. Line 178: writes the double-converted value back:
     `m_function->setFadeInSpeed(fadeIn)`
- **Example at 120 BPM** (beatDuration = 500ms):
  - Original: 500ms
  - After step 1: `timeToBeats(500, 500)` = 1000 (1 beat) -- correct
  - After step 2: `timeToBeats(1000, 500)` = 2000 (2 beats) -- WRONG, doubled!
  - After step 3: stored as 2000. Every toggle doubles (Time->Beats) or halves (Beats->Time).
- **Impact**: **Data corruption**. Every time the user toggles tempo type in the
  Function Editor, all speed values are doubled or halved. After 3 toggles
  back and forth, a 500ms fade becomes 4000 beats (garbage).
- **Fix**: Remove lines 158-178 (the redundant conversion). `setTempoType()` already
  converts internally. Or: remove the `m_function->setTempoType()` call and only do
  the manual conversion.

---

### BT2: VCCueListItem.qml always displays in Time mode (tempoType commented out)

- **File**: `qmlui/qml/virtualconsole/VCCueListItem.qml:200`
- **Verified**: YES — line reads `//tempoType: chaserEditor.tempoType`
- **The bug**: The `ChaserWidget` inside the CueList defaults to `QLCFunction.Time`
  (`ChaserWidget.qml:36`). Since `tempoType` is never set from the chaser's actual
  tempo type, beat-encoded values (e.g., 1000 for 1 beat) are displayed as
  milliseconds: **"1s000ms"** instead of **"1"**.
- **Impact**: All beat-mode cuelist step timings display as wrong millisecond values.
  Users see "250ms" when it should say "1/4 beat". Completely misleading during live
  shows.
- **Fix**: Uncomment and fix the tempoType binding. The CueList widget needs to read
  the chaser's tempoType and pass it through.

---

### BT3: MCP `beatStringToValue()` returns 0 silently on parse failure

- **File**: `mcp/tools/conversions.h:54-91`
- **Verified**: YES — invalid input paths (`""`, `"abc"`, `"1/0"`, negative values,
  overflow > 100000) all `return 0` with no error message.
- **The bug**: When an MCP client passes an invalid beat string to `create_rgb_matrices`,
  `parseDurationField()` calls `beatStringToValue()` which returns 0. This becomes a
  valid "instant" duration — indistinguishable from intentional 0. No error JSON is
  returned to the client.
- **Impact**: **Silent data loss**. An AI agent that passes `"quarter"` instead of `"1/4"`
  gets 0 duration with no indication of failure. The function plays but with instant
  timing, which the user sees as "it just shows 0".
- **Fix**: Return an error or use a sentinel value (e.g., `UINT_MAX`) for parse failures,
  then check in the caller and return error JSON.

---

### BT4: MCP `create_chasers` setTempoType on upsert can corrupt function-level speeds

- **File**: `mcp/tools/function_tools.cpp:496-498`
- **Verified**: YES — on upsert (existing chaser found by name), the code calls
  `chaser->setTempoType(Function::Beats)`. If the chaser already has non-zero
  function-level speed values (fadeInSpeed, duration, fadeOutSpeed), `setTempoType()`
  auto-converts them from their current encoding to the new encoding using the
  current BPM. Steps are cleared first (line 460-461), but function-level speeds
  are NOT cleared.
- **Example**: Chaser has fadeInSpeed=500ms. MCP call updates it with tempoType=beats.
  `setTempoType(Beats)` converts 500ms to 1000 beat-units (at 120 BPM). If the MCP
  call doesn't explicitly set new fadeIn/fadeOut/duration, the auto-converted values
  persist — which may or may not be intended.
- **Impact**: Surprising behavior on upsert. The function-level speeds get converted
  even though the user may have intended to replace them entirely. If called
  repeatedly (idempotent upsert), the conversion is NOT re-applied because
  `setTempoType` guards `if (type == m_tempoType) return`. So the first call corrupts,
  subsequent calls are no-ops.
- **Fix**: Either clear function-level speeds before calling `setTempoType()`, or set
  tempoType FIRST (before any speeds exist), or explicitly set function-level speeds
  after `setTempoType`.

---

### BT5: RGBMatrix passes beat-encoded values as milliseconds to FadeChannel

- **File**: `engine/src/rgbmatrix.cpp:863, 870`
- **Verified**: YES — `updateFaderValues()` (line 863) and `updateMapChannels()` (line 870)
  read `fadeInSpeed()` / `fadeOutSpeed()` and pass them directly to
  `FadeChannel::setFadeTime()`. In Beats mode, these are beat-encoded values (e.g.,
  1000 for 1 beat), but `setFadeTime()` expects milliseconds. No `beatsToTime()`
  conversion is applied.
- **Contrast with correct code**: `Scene::write()` at line 778 correctly calls
  `beatsToTime(fadeIn, timer->beatTimeDuration())` before setting fade time.
  ChaserRunner at line 786 also converts correctly.
- **Impact**: In Beats mode, RGB Matrix fades take far too long or too short.
  A 1-beat fade (1000 beat-units) at 120 BPM should be 500ms, but is applied as
  1000ms. A 1/4-beat fade (250 beat-units) is applied as 250ms instead of 125ms.
- **Fix**: Add `beatsToTime()` conversion in both locations, gated on
  `tempoType() == Beats`.

---

### BT6: MCP query_functions returns raw beat encoding for chaser duration

- **File**: `mcp/tools/conversions.h:488`
- **Verified**: YES — `functionToJson()` uses `fn->totalDuration()` cast to int.
  For a beat-mode chaser with 4 beats total, this returns 4000 (the internal encoding),
  not milliseconds and not a human-readable beat string.
- **Impact**: MCP clients misinterpret the duration. An AI agent sees `duration: 4000`
  and thinks "4 seconds" when it means "4 beats". This is the likely root cause of
  "sometimes the timing is just 0 over MCP" — the agent reads a beat-encoded value,
  interprets it as ms, and uses it incorrectly in subsequent operations.
- **Fix**: In `functionToJson()`, check `fn->tempoType()` and format duration
  appropriately (either convert to ms via `beatsToTime()`, or return a beat string
  via `valueToBeatString()`).

---

### BT7: MCP create_scenes, create_sequences, create_efxs have no beat mode support

- **Files**:
  - `mcp/tools/function_tools.cpp:147-150` (scenes: fadeIn/fadeOut are `"type": "integer"`)
  - `mcp/tools/function_tools.cpp:604-606` (sequences: same)
  - `mcp/tools/function_tools.cpp:729-733` (EFX: same)
- **Verified**: YES — these tools only accept integer milliseconds. No `tempoType`
  parameter, no beat string parsing, no `parseDurationField()`.
- **Impact**: AI agents cannot create beat-mode scenes, sequences, or EFX via MCP.
  If they try to set beat values as plain integers, the values are treated as
  milliseconds (e.g., 1000 becomes 1s instead of 1 beat).
- **Fix**: Add `tempoType` parameter and beat-aware duration handling to these tools,
  following the `create_rgb_matrices` pattern.

---

### BT8: VCSpeedDialItem hardcodes Time mode for display

- **File**: `qmlui/qml/virtualconsole/VCSpeedDialItem.qml:379`
- **Verified**: YES — `TimeUtils.timeToQlcString(speedObj.currentTime, QLCFunction.Time)`
  always passes `QLCFunction.Time`, ignoring the function's actual tempoType.
- **Impact**: Speed dial widget always shows millisecond format even for beat-mode
  functions. "250ms" instead of "1/4 beat".
- **Fix**: Read the function's tempoType and pass it to `timeToQlcString()`.

---

## EDGE CASES AND MINOR ISSUES

### BT-E1: Values below 63 display as raw numbers in Beats mode

- **File**: `qmlui/js/TimeUtils.js:243-246`
- If a beat value's fractional part is between 1 and 62 (not on the 1/16 grid),
  `timeToQlcString()` returns the raw number as a string. E.g., beat value 42
  displays as "42" — neither a valid time string nor a musical value.

### BT-E2: decomposeBeats defaults to "1 x current" for value 0

- **File**: `qmlui/qml/TimeEditTool.qml:96-97`
- When `value <= 0`, the beat editor shows "1 x 1/4" (or whatever subdivision is
  selected) instead of "0". The user sees a non-zero value in the editor while the
  actual stored value is 0.

### BT-E3: m_beatResyncNeeded is set but never consumed

- **File**: `engine/src/function.cpp:677`
- `slotBPMChanged()` sets `m_beatResyncNeeded = true` but no code ever reads this
  flag. Dead code or incomplete feature.

### BT-E4: ChaserStep constructor has no overflow protection

- **File**: `engine/src/chaserstep.cpp:37`
- `duration = fadeIn + hold` can overflow for large beat values. E.g., two values
  of 2 billion each would wrap around to a small number.

---

## ROOT CAUSE ANALYSIS: "Sometimes 0 over MCP"

Based on the findings, the "sometimes it's 0" issue likely has **multiple causes**:

1. **BT3 (silent parse failure)**: If an MCP client passes a beat string that doesn't
   exactly match the expected format (e.g., `"quarter"` or `"1/4 beat"` instead of
   `"1/4"`), `beatStringToValue()` silently returns 0. No error is surfaced.

2. **BT6 (raw encoding in queries)**: The MCP client reads `duration: 4000` from
   `query_functions`, interprets it as 4000ms, and may pass it back as-is. Or it
   sees a small beat value like 250 and interprets it as 250ms (trivially small).

3. **BT4 (upsert corruption)**: On the first MCP upsert of an existing chaser with
   changed tempoType, the function-level speeds get auto-converted. On subsequent
   upserts, `setTempoType()` no-ops (same type), so the speeds from the JSON
   overwrite correctly. But the first call can leave corrupted function-level speeds
   if they're not explicitly set.

4. **BT7 (no beat support in scenes/sequences/EFX)**: If the MCP client tries to
   set beat timing on these function types, the values are silently treated as ms.

---

## PRIORITY ROADMAP

### Phase 1: Fix data corruption (immediate)
- **BT1**: Fix FunctionEditor double-conversion
- **BT3**: Return error from `beatStringToValue()` on parse failure
- **BT5**: Add `beatsToTime()` in RGBMatrix fade paths

### Phase 2: Fix MCP beat handling
- **BT6**: Format beat values correctly in `functionToJson()`
- **BT4**: Handle `setTempoType` safely on upsert
- **BT7**: Add beat mode to create_scenes, create_sequences, create_efxs

### Phase 3: Fix display
- **BT2**: Uncomment tempoType in VCCueListItem
- **BT8**: Use function's tempoType in VCSpeedDialItem

### Phase 4: Polish
- **BT-E1**: Handle off-grid values gracefully in display
- **BT-E2**: Show "0" correctly in beat editor
- **BT-E3**: Remove dead `m_beatResyncNeeded` flag

---

## KEY FILE REFERENCES

| File | Role |
|------|------|
| `engine/src/function.cpp:553-662` | Beat encoding table, all conversion functions |
| `engine/src/function.cpp:486-524` | `setTempoType()` auto-conversion (the trap) |
| `engine/src/rgbmatrix.cpp:863,870` | Missing `beatsToTime()` in fade paths |
| `qmlui/functioneditor.cpp:144-187` | Double-conversion bug |
| `qmlui/chasereditor.cpp:628-678` | Step-level tempo conversion (correct) |
| `qmlui/js/TimeUtils.js:187-276` | Beat value display formatting |
| `qmlui/qml/virtualconsole/VCCueListItem.qml:200` | Commented-out tempoType |
| `qmlui/qml/virtualconsole/VCSpeedDialItem.qml:379` | Hardcoded Time mode |
| `mcp/tools/conversions.h:54-91` | `beatStringToValue()` silent 0 return |
| `mcp/tools/conversions.h:488` | `functionToJson()` raw encoding leak |
| `mcp/tools/function_tools.cpp:496-498` | Chaser upsert tempoType trap |
| `mcp/tools/function_tools.cpp:515-532` | Chaser beat encoding (numeric * 1000) |
| `mcp/tools/function_tools.cpp:924-978` | RGB matrix beat handling (correct) |
