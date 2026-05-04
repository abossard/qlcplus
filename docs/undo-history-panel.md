# Undo History Panel — Implementation Plan

## Design Decisions

### Scope: Highlight, Don't Hide (Option D + B)
- **Default = "All"** — show every action, no filtering
- **Scope tags** derived via `static Tardis::actionScope(int code)` — NO struct field change (avoids network serialization risk)
- Current view highlighted with accent color; out-of-scope actions dimmed but visible
- Optional "Hide other scopes" toggle for power users
- Auto-follow toggle tracks `ContextManager::currentContext`

### Undo: Always Global (Option B)
- **Undo/redo always operate on the global tip** — never skip actions by scope
- Scope filter is VIEW-ONLY — a navigator/inspector, not an undo selector
- When next-to-undo action is outside current scope, show warning: "Next undo: Fixtures action"
- This prevents state corruption from out-of-order undo

### Scopes (derived from action code ranges)

| Range | Scope | Context Match |
|-------|-------|---------------|
| `0x0000–0x008F` | Preview/3D | `FIXANDFUNC` (3D view) |
| `0x0090–0x00FF` | IO | IO Manager |
| `0x0100–0x01FF` | Fixtures | `FIXANDFUNC` |
| `0x0200–0xAFFF` | Functions | `FIXANDFUNC` |
| `0xB000–0xBFFF` | Show Manager | `SHOWMGR` |
| `0xC000–0xCFFF` | Simple Desk | SimpleDesk |
| `0xE000–0xEFFF` | Virtual Console | `VC` |
| `0xF000+` | Live/Network | excluded from drawer |

> Note: `FIXANDFUNC` is ONE context for both Fixtures and Functions.

## ASCII Drawer Design

### Full size (1920×1080, ~320px wide)
```
┌─ History ───────────────────────────── [×] ┐
│ Scope: [All▾] [Current: Fixtures]  ⟳ Auto │
│ ┌──────────────────────────────────────┐  │
│ │ Filter: ●All ○VC ○Func ○Fix ○IO ○Show│  │
│ └──────────────────────────────────────┘  │
│                                            │
│   ░ 14:02:11  IO   Add Universe "U2"      │
│   ░ 14:02:34  Fix  Create Fixture ×4 ▸    │  ← batch, expandable
│   · 14:03:01  Func Scene "Wash" set 12ch ▸│
│ ► ● 14:03:18  VC   Button caption "GO"    │  ← tip / next-to-undo
│ ─────────── undone ───────────             │
│   ○ 14:03:42  VC   Slider lowLimit 0→10   │  (greyed)
│   ○ 14:03:55  Func Chaser add step        │
│                                            │
│ Showing 6 of 24 (filter: All)              │
│ ┌──────────────┬──────────────────────┐   │
│ │ ◀ Undo (VC)  │ Redo (VC) ▶          │   │
│ └──────────────┴──────────────────────┘   │
│ ⚠ Next undo is OUTSIDE current view (Fix) │
└────────────────────────────────────────────┘
```

### Compact (1280×720, ~280px)
```
┌─ History ────────────── [×] ┐
│ Scope: [All ▾]   [Auto ⟳]  │
│ ─────────────────────────── │
│  14:02 IO  +Universe U2     │
│  14:02 Fix +4 fixtures    ▸ │
│  14:03 Fn  Scene 12ch     ▸ │
│► 14:03 VC  Button caption   │
│ ── undone ──                 │
│  14:03 VC  Slider limit     │
│  14:03 Fn  Chaser +step     │
│                              │
│ 6/24                         │
│ [◀ Undo]      [Redo ▶]      │
│ ⚠ Next: Fixtures             │
└──────────────────────────────┘
```

### Visual Legend
- `●` active tip (next to undo)
- `░` / `·` active history (zebra stripe)
- `○` undone (greyed, redoable)
- `▸` expandable batch
- `►` cursor position
- `⟳ Auto` auto-follow current context

## Implementation Checklist

### Step 0: Test Scaffolding
- [ ] Create `qmlui/tardis/test/CMakeLists.txt`
- [ ] Create `qmlui/tardis/test/tardis_history_test.h/.cpp` skeleton
- [ ] Register in `qmlui/CMakeLists.txt` under `BUILD_TESTING`
- [ ] **Verify**: `cmake --build . --target tardis_history_test && ./qmlui/tardis/test/tardis_history_test` runs (0 tests)

### Step 1: Extract Pure Grouping Function
- [ ] Create `qmlui/tardis/tardishistorystep.h` with `HistoryStep` struct
- [ ] Implement `tardisGroupSteps()` — same algorithm as `undoAction()` backward walk
- [ ] Write Suite 1 tests: 11 parametrized grouping cases (single, coalesced, boundary, batch, mixed, empty, undone flags)
- [ ] **Verify**: All Suite 1 tests pass. `tardisGroupSteps()` returns correct step boundaries for all fixtures.

### Step 2: Prove Grouping Matches undoAction()
- [ ] Add test-only `Tardis::__test_setHistory()` accessor
- [ ] Add `Tardis::actionProcessed(int code, bool undo)` signal
- [ ] Write Suite 2 tests: enqueue known actions, call undoAction(), compare with tardisGroupSteps()
- [ ] **Decision point**: Refactor `undoAction()`/`redoAction()` to use `tardisGroupSteps()` (one algorithm, one source of truth)
- [ ] **Verify**: Suite 2 green — panel grouping matches what Ctrl+Z actually undoes. No divergence.

### Step 3: Signals + Locking
- [ ] Add `Q_SIGNAL void historyChanged()` to Tardis
- [ ] Add `QRecursiveMutex m_historyMutex`
- [ ] Lock all `m_history`/`m_historyIndex` mutations (run, undoAction, redoAction, resetHistory)
- [ ] Emit `historyChanged()` from: run() end-of-iteration, undoAction() end, redoAction() end, resetHistory()
- [ ] Add `Q_PROPERTY(bool canUndo)` and `Q_PROPERTY(bool canRedo)` with `NOTIFY historyChanged`
- [ ] Add `Tardis::snapshotHistory()` returning thread-safe copy of list + index
- [ ] Write Suite 4 tests: signal emission for enqueue, undo, redo, reset, coalesce, prune
- [ ] **Verify**: QSignalSpy captures correct signal count for each operation. canUndo/canRedo are correct.

### Step 4: Scope Derivation
- [ ] Add `static int Tardis::actionScope(int code)` — lookup table from code ranges to scope enum
- [ ] Define scope enum: `All, Preview, IO, Fixtures, Functions, ShowManager, SimpleDesk, VC`
- [ ] Write tests: every action code maps to a non-zero scope, live/net codes map to excluded
- [ ] **Verify**: Full enum coverage — no action code returns "unknown" scope.

### Step 5: TardisHistoryModel
- [ ] Create `qmlui/tardis/tardishistorymodel.h/.cpp` — QAbstractListModel
- [ ] Roles: StepIndex, ActionCount, Display, Scope, IsCurrent, IsUndone, Timestamp, IsBatch
- [ ] Connect `historyChanged` → `rebuild()` using `snapshotHistory()` + `tardisGroupSteps()`
- [ ] Use `beginResetModel()/endResetModel()` for v1
- [ ] Add scope filtering: `setFilterScope(int scope)` — dims out-of-scope rows (doesn't remove)
- [ ] Register as QML context property `tardisHistory`
- [ ] Write model-level tests: rowCount, data roles, scope filter behavior
- [ ] **Verify**: Model reflects correct step count, roles resolve, filter dims but doesn't hide.

### Step 6: Action Descriptions
- [ ] Create `qmlui/tardis/tardisactiondescriptions.h/.cpp`
- [ ] Map every ActionCodes value to translatable `tr()` string
- [ ] Resolve object names via Doc lookup (handle deleted objects gracefully — show "ID: N")
- [ ] Write Suite 5 tests: every code has non-empty name, representative codes resolve object names
- [ ] **Verify**: No action type returns empty string. Deleted-object fallback works.

### Step 7: Thread Safety Hardening
- [ ] Verify all `m_history` reads in `run()` are inside lock
- [ ] Ensure `actionsToString()` and `forwardActionToNetwork()` happen OUTSIDE lock (avoid deadlock)
- [ ] Write Suite 3: producer/reader stress test (1000 enqueues + concurrent model reads)
- [ ] **Verify**: No crash, no TSAN finding, model rowCount matches expected after drain.

### Step 8: QML Panel
- [ ] Create `qmlui/qml/UndoHistoryPanel.qml` — slide-over from right
- [ ] Scope filter bar (radio buttons or dropdown), auto-follow toggle
- [ ] ListView with step delegate (timestamp, scope badge, description, batch expand)
- [ ] Active vs undone visual states (filled/greyed, opacity)
- [ ] "undone" separator line
- [ ] Undo/Redo buttons bound to tardis.canUndo/canRedo
- [ ] Warning label when next-to-undo is outside current scope
- [ ] Add toolbar toggle icon in MainView.qml
- [ ] Wire `ContextManager::currentContext` to auto-follow
- [ ] Register `UndoHistoryPanel.qml` in qmlui.qrc
- [ ] **Verify**: Panel opens/closes with animation. Steps render correctly. Filter highlights/dims. Undo/redo buttons work.

### Step 9: Fix Missing Handlers
- [ ] Add `processAction()` cases for `FixtureGroupCreate` / `FixtureGroupDelete`
- [ ] Add `processAction()` cases for `VCWidgetAllowResize` / `VCWidgetDisabled` / `VCWidgetVisible` / `VCWidgetPage`
- [ ] Write targeted undo/redo tests for each fixed handler
- [ ] **Verify**: All 6 previously-missing handlers now undo/redo correctly.

### Step 10: Integration + Polish
- [ ] Test project load/new → history resets (model clears)
- [ ] Test batch undo (beginBatch, 3 actions, endBatch → single Ctrl+Z)
- [ ] Test redo branch truncation (action, undo, new action → redo gone)
- [ ] Test coalescing (same obj+code within 150ms → single undo)
- [ ] Test overflow pruning (101 actions → oldest group removed, count stays ≤100)
- [ ] Update MANUAL_REVIEW.md with panel test procedures
- [ ] **Verify**: All integration scenarios pass. Panel behaves correctly in real use.

## Missing Test Scenarios (must be covered)

| Scenario | Why It Matters |
|----------|---------------|
| Double undo (two steps in sequence) | Trivial regression risk |
| Undo → redo → undo again | Verifies index doesn't go negative |
| Batch undo (beginBatch/endBatch) | Core batching feature |
| Coalescing (same obj+code within 150ms) | Most fragile code in Tardis |
| New action after undo truncates redo | Exercises redo-branch pruning |
| Overflow trims oldest group | TARDIS_MAX_ACTIONS_NUMBER = 100 |
| Undo when history empty / index = -1 | Boundary case |

## Effort Estimate

| Step | Estimate |
|------|----------|
| 0: Scaffolding | 0.5h |
| 1: Grouping function | 2h |
| 2: Match undo + refactor | 2h |
| 3: Signals + locking | 2.5h |
| 4: Scope derivation | 1h |
| 5: History model | 3h |
| 6: Action descriptions | 2h |
| 7: Thread safety | 2h |
| 8: QML panel | 4h |
| 9: Fix missing handlers | 2h |
| 10: Integration + polish | 2h |
| Buffer | 3h |
| **Total** | **~26h (≈3.5 dev-days)** |

## Key Constraints

- **Singleton**: Only one Tardis per process (`s_instance` with assert). Tests must cleanup between runs.
- **Worker thread**: Tardis is a QThread. `enqueueAction()` queues; `run()` processes. Tests must wait for drain via `QTRY_VERIFY` on `historyChanged` signal.
- **Managers required**: Tardis ctor needs 9 manager pointers. Tests need `QGuiApplication` + either real managers or an `ITardisHost` interface for test doubles.
- **Tests go through managers**: `doc->addFunction()` does NOT enqueue. Only manager methods trigger Tardis. Tests must call e.g. `functionManager->createFunction()`.
- **FIXANDFUNC = one context**: Fixtures and Functions are not separate top-level contexts. Combined scope.
- **Cross-scope batches**: Actions within 150ms across scopes batch together. Show as one row with mixed-scope badges.
