# Virtual Console Analysis: Bugs, QoL, and Flow Console Backports

> Generated 2026-04-29. Verified against source code and upstream GitHub issues.

---

## 1. CONFIRMED BUGS

### B1: `connect()` instead of `disconnect()` in CueList chaser switching

- **File**: `qmlui/virtualconsole/vccuelist.cpp:476`
- **Verified**: YES — line 476 reads `connect(current, SIGNAL(stepChanged(int)), ...)` in a block
  where every other line is `disconnect(...)`. This is a copy-paste typo.
- **Code**:
  ```cpp
  // Lines 470-477: should all be disconnect()
  disconnect(current, SIGNAL(running(quint32)), ...);
  disconnect(current, SIGNAL(stopped(quint32)), ...);
  disconnect(current, SIGNAL(currentStepChanged(int)), ...);
  connect(current, SIGNAL(stepChanged(int)), ...);  // BUG: should be disconnect
  ```
- **Impact**: Every time a CueList's chaser is switched, one extra signal connection
  accumulates. After N switches, `slotStepChanged()` fires N times per step change.
  This causes:
  - **Step list model corruption** — duplicate updates to ListModel rows
  - **Unbounded memory growth** — old signal connections never cleaned up
  - **Performance degradation** — O(N) slot invocations per step change
  - Likely contributes to the "cuelist acts weird after editing" reports users describe
- **Upstream**: No exact match found, but [Issue #79](https://github.com/mcallegari/qlcplus/issues/79)
  reports "strange behavior" after creating cuelists with chasers — could be related.
- **Fix**: Change `connect` to `disconnect` on line 476.

---

### B2: QQmlComponent leaked in every widget `render()` call

- **File**: `qmlui/virtualconsole/vcslider.cpp:150-163`, `vcbutton.cpp:92-105`, `vcframe.cpp:98-117`
- **Verified**: YES — all three files follow this pattern:
  ```cpp
  QQmlComponent *component = new QQmlComponent(view->engine(), QUrl("qrc:/VCSliderItem.qml"));
  if (component->isError()) { delete component; return; }
  m_item = qobject_cast<QQuickItem*>(component->create());
  m_item->setParentItem(parent);
  // component is NEVER deleted after successful create()
  ```
- **Impact**: Each widget rendered leaks one `QQmlComponent` (~few KB each). For a console
  with 50+ widgets across multiple pages, this adds up. On page switches or project reloads,
  new components are created without cleaning up old ones.
  - **Slow memory growth** over long sessions (live show scenarios)
  - Exacerbated by page switching or workspace reloads
- **Upstream**: Not reported. The [Qt documentation](https://doc.qt.io/qt-6/qqmlcomponent.html)
  explicitly states components can be safely deleted after `create()`.
- **Fix**: Add `delete component;` after the `create()` call (or use `QScopedPointer`).
  Also add a null check on `m_item` before `setParentItem()`.

---

### B3: Null dereference of `buttonObj` in property binding

- **File**: `qmlui/qml/virtualconsole/VCButtonItem.qml:32`
- **Verified**: YES — line 32:
  ```qml
  property string activeColor: buttonObj.flashOverrides || buttonObj.flashForceLTP ? "#FF0000" : "#00FF00"
  ```
  While lines 30-31 correctly guard with `buttonObj ? ... : default`, line 32 does not.
  `buttonObj` is declared as `property VCButton buttonObj: null` (line 28).
- **Impact**: QML runtime warning `TypeError: Cannot read property 'flashOverrides' of null`
  during component initialization and cleanup. In rare cases this can cascade to break
  the widget's visual state (stuck colors).
  - **Cosmetic** in most cases, but produces noisy console output
  - Potential for inconsistent button color state after rapid operations
- **Upstream**: Not reported specifically.
- **Fix**: `property string activeColor: buttonObj ? (buttonObj.flashOverrides || buttonObj.flashForceLTP ? "#FF0000" : "#00FF00") : "#00FF00"`

---

### B4: `getPrevIndex()` returns -2 when chaser hasn't started

- **File**: `qmlui/virtualconsole/vccuelist.cpp:576-586`
- **Verified**: YES — when `m_playbackIndex == -1` (chaser not yet started) and direction
  is `Backward`:
  ```cpp
  // Forward: m_playbackIndex == 0 ? stepsCount()-1 : m_playbackIndex - 1
  // When m_playbackIndex == -1: returns -1 - 1 = -2
  return m_playbackIndex == 0 ? ch->stepsCount() - 1 : m_playbackIndex - 1;
  ```
- **Impact**: The -2 index is passed to `ChaserAction.m_stepIndex`, causing:
  - **Out-of-bounds step access** in ChaserRunner
  - Potential crash or undefined behavior in step lookup
  - User-visible: pressing Previous on a stopped cuelist in backward mode does nothing
    or shows wrong step
- **Upstream**: Changelog mentions "fix off-by-one offset error in VC Cue List steps mode".
- **Fix**: Add guard at top: `if (m_playbackIndex < 0) return getFirstIndex();`

---

### B5: CueList state not reset when chaser stops

- **File**: `qmlui/virtualconsole/vccuelist.cpp:911-928`
- **Verified**: YES — `slotFunctionStopped()` stops the progress timer and clears QML
  progress properties but does NOT reset:
  - `m_playbackIndex` (remains at last step)
  - `m_nextStepIndex` (remains at computed next)
  - `m_primaryTop` (crossfade direction persists)
  - `m_sideFaderLevel` (fader position persists)
- **Impact**:
  - **Restart plays from last step** instead of first (confusing for live operators)
  - **Crossfade slider shows stale labels** after stop/start cycle
  - **Side fader position persists** across stop/start, potentially causing unexpected
    intensity jumps on restart
  - Related to [Issue #1595](https://github.com/mcallegari/qlcplus/issues/1595):
    "Cuelist with Submaster doesn't work until QLC+ is reloaded" — stale state after stop.
- **Fix**: Add to `slotFunctionStopped()`:
  ```cpp
  setPlaybackIndex(-1);
  m_primaryTop = true;
  m_sideFaderLevel = sideFaderMode() == Crossfade ? 100 : 255;
  ```

---

### B6: Coordinate transformation bug in nested frame drag

- **File**: `qmlui/qml/virtualconsole/VCWidgetItem.qml:181-194`
- **Verified**: YES — the drag remap logic:
  ```qml
  var remappedPos = wRoot.mapToItem(virtualConsole.currentPageItem(), 0, 0);
  wObj.geometry = Qt.rect(remappedPos.x, remappedPos.y, wRoot.width, wRoot.height)
  wRoot.parent = virtualConsole.currentPageItem()  // parent changed AFTER mapToItem
  ```
  Problems:
  1. `mapToItem()` maps to PAGE level, but the widget may be dropped into a nested frame
  2. `wRoot.parent` is set AFTER coordinate calculation (should be before)
  3. No null check on `virtualConsole.currentPageItem()` return value
- **Impact**: **Widget appears at wrong position** when dragging between nested frames.
  The frame drag freeze issue is a known upstream bug:
  - [Issue #1316](https://github.com/mcallegari/qlcplus/issues/1316): "Freeze when moving
    frame in virtual console" — frame jumps to (0,0) then app freezes. Maintainer confirmed
    root cause in VCFrameItem.qml and VCWidget.cpp (endless recursion from corrupted
    parent hierarchy). Was partially fixed but the coordinate logic remains fragile.
  - [Issue #835](https://github.com/mcallegari/qlcplus/issues/835): "Virtual console frame:
    size resets when properties are edited" — related geometry/parent corruption.
- **Fix**: Map coordinates AFTER setting new parent. Add null check for `currentPageItem()`.
  Consider mapping to the actual drop target frame instead of always to the page.

---

### B7: Cross-thread access to `m_monitorValue` in VCSlider

- **File**: `qmlui/virtualconsole/vcslider.cpp:1217-1304`
- **Verified**: YES — `writeDMXLevel()` runs on the **MasterTimer thread** (called from
  `MasterTimer::timerTick()`). It:
  1. Reads `m_value` (line 1223) — also written from main thread via `setValue()`
  2. Reads `m_levelValueChanged` (line 1248) — set from main thread at line 477
  3. Writes `m_monitorValue` (line 1295) — read by QML on main thread
  4. Emits `monitorValueChanged()` (line 1296) — signal from wrong thread
  5. Reads `m_isOverriding` (line 1298) — set from main thread
  6. Calls `setValue()` (line 1300) — updates m_value from MasterTimer thread

  Only `m_levelChannels` is protected by `m_levelValueMutex`.
- **Impact**:
  - **Data race** on `m_value`, `m_monitorValue`, `m_isOverriding`, `m_levelValueChanged`
  - **Signal emitted from wrong thread** — QML property bindings may execute on MasterTimer thread,
    violating Qt's threading model
  - Can cause **slider jumping/flickering** as QML reads partially-updated values
  - Related upstream: [Commit 0435145](https://github.com/mcallegari/qlcplus/commit/04351458b156693286e094c8ee774f690e84b0f2)
    fixed a crash in the same `writeDMXLevel()` function (null channel access). The threading
    issues around it remain unaddressed.
  - [Issue #1011](https://github.com/mcallegari/qlcplus/issues/1011): macOS crash on play
    in virtual mode — backtrace shows crash in `VCFrame::sendFeedback()` during mode change,
    potentially related to cross-thread signal emission patterns.
- **Fix**: Use `QMetaObject::invokeMethod(this, [=]{ emit monitorValueChanged(); }, Qt::QueuedConnection)`
  for the signal. Protect `m_monitorValue` and `m_isOverriding` with atomic operations or mutex.

---

### B8: Crossfade `m_primaryTop` race condition

- **File**: `qmlui/virtualconsole/vccuelist.cpp:331-354`
- **Verified**: YES — `stopStepIfNeeded()` flips `m_primaryTop` and emits `primaryTopChanged()`.
  Meanwhile, `slotCurrentStepChanged()` (line 930) calls `setPlaybackIndex()` which
  recalculates `m_nextStepIndex` based on `m_primaryTop`. If both fire close together
  (user moves fader to 0 while MasterTimer processes step change), the state can be
  inconsistent.
- **Impact**:
  - **Crossfade labels show wrong step numbers** (top/bottom swapped)
  - **Wrong step stopped** when fader reaches endpoint
  - Related to [Issue #439](https://github.com/mcallegari/qlcplus/issues/439): "Crossfade
    sliders problem when transitioning and fixture didn't change" — demonstrates that
    crossfade state management has edge cases. The fix addressed HTP blending but not
    the state race.
- **Fix**: Guard `m_primaryTop` changes with the same mutex used for `m_sideFaderLevel`,
  or serialize all state changes through the main thread event loop.

---

### B9: Progress bar divide-by-zero

- **File**: `qmlui/virtualconsole/vccuelist.cpp:972`
- **Verified**: YES — `progress = ((double)step.m_elapsed / (double)step.m_duration);`
  When `step.m_duration == 0` (infinite hold or duration-less step), this produces
  infinity or NaN.
- **Impact**:
  - **Progress bar shows garbage** or infinite value
  - QML property receives NaN, causing visual glitches in the progress indicator
  - Low severity in practice since most chasers have non-zero duration, but
    infinite-hold steps are a valid configuration
- **Upstream**: Not reported specifically.
- **Fix**: `if (step.m_duration > 0) progress = ...; else progress = 0;`

---

### B10: Scale factor double-application during drag (NEEDS VERIFICATION)

- **File**: `qmlui/virtualconsole/vcwidget.cpp:263-272`
- **Verified**: PARTIALLY — The geometry getter applies `m_scaleFactor`, the setter divides
  by it. During drag, QML coordinates are screen-space. On release, `updateGeometry()` passes
  screen coords to `setGeometry()` which divides by scale. Binding restoration then reads
  back `geometry()` which multiplies by scale. The round-trip should be correct IF scale
  factor doesn't change mid-drag.
- **Impact**: If `m_scaleFactor != 1.0` (e.g., when VC is zoomed), there MAY be floating-point
  drift causing **sub-pixel position jitter** after repeated drag operations. Likely benign
  at scale 1.0 but could become visible with zoom.
- **Upstream**: [Issue #1316](https://github.com/mcallegari/qlcplus/issues/1316) may partly
  stem from scale-related coordinate corruption.
- **Status**: Needs testing with non-unity scale factors. Low confidence this is a real bug.

---

### B11: Binding restored before C++ geometry sync

- **File**: `qmlui/qml/virtualconsole/VCWidgetItem.qml:237-240`
- **Verified**: YES — duplicate binding restoration exists:
  - Lines 128-131 in `updateGeometry()` (called on resize release)
  - Lines 237-240 in drag `onReleased` handler

  The drag handler restores bindings at line 237-240. But `updateGeometry()` at line 126
  sets `wObj.geometry = Qt.rect(x, y, width, height)` which triggers `setGeometry()` in C++.
  The binding restoration on the NEXT line reads back from `wObj.geometry` which should now
  have the updated value. In practice, Qt property updates are synchronous within the same
  thread, so this is likely safe.
- **Impact**: **Low** — The duplication is a code smell more than a bug. The real risk is
  during resize (not drag), where `snapToGrid()` modifies x/y/width/height and then
  `updateGeometry()` reads them. If `snapToGrid()` triggers a binding loop or deferred
  evaluation, the values could be stale.
- **Upstream**: Not reported.
- **Fix**: Remove duplicate binding restoration from `onReleased` (keep only in `updateGeometry()`).

---

## 2. QUALITY OF LIFE IMPROVEMENTS

### Q1: No minimum size enforcement during resize drag

- **File**: `qmlui/qml/virtualconsole/VCWidgetItem.qml:260-278`
- **Verified**: YES — the resize drag handlers modify `wRoot.width` and `wRoot.height`
  directly without clamping. The minimum size check (`UISettings.iconSizeMedium`) only
  exists in `updateGeometry()` (lines 121-124), which runs on release. During drag,
  width/height can go negative.
- **Impact**: Widget visually collapses or inverts during resize, then snaps to minimum
  on release. Confusing UX, especially for new users.
- **Proposed fix**: Add inline clamp in each resize handler:
  ```qml
  wRoot.width = Math.max(wRoot.width - tlHandle.x, UISettings.iconSizeMedium)
  ```

### Q2: Flickable steals mouse events during widget drag

- **File**: `qmlui/qml/virtualconsole/VCPageArea.qml:32-47`
- **Verified**: YES — `VCPageArea` wraps content in a `Flickable`. The deselect `MouseArea`
  doesn't set `preventStealing: true`. When `enableFlicking(false)` is called during drag
  (VCWidgetItem.qml:172), it sets `interactive: false` on the Flickable, but existing
  momentum isn't cancelled.
- **Impact**: Fast drag gestures can be intercepted by the Flickable, causing the page to
  scroll while the widget drag is in progress. Results in widget placed at wrong position.
- **Proposed fix**: Set `Flickable.flick(0, 0)` when disabling flicking to cancel momentum.

### Q3: Grid snap recalculated every mouse move frame

- **File**: `qmlui/qml/virtualconsole/VCWidgetItem.qml:196-219`
- **Verified**: YES — `getGridFrame()` walks the parent chain on every `onPositionChanged`
  event during drag (60+ fps). Division and rounding per frame.
- **Impact**: Unnecessary CPU work during drag. Snapping causes visual jitter because
  position is quantized every frame rather than smoothly interpolated.
- **Proposed fix**: Cache the grid frame reference on `onPressed`. Use `Qt.callLater()` for
  snap to reduce per-frame overhead.

### Q4: No bounds clamping on widget drop

- **File**: `qmlui/qml/virtualconsole/VCFrameItem.qml:295`
- **Verified**: YES — `var pos = drag.source.mapToItem(frameRoot, 0, 0)` can produce
  negative values or positions outside the frame bounds. No clamping is applied.
- **Impact**: Widgets can be dropped outside frame boundaries, becoming invisible or
  only partially visible. User must undo or manually reposition.
- **Proposed fix**: Clamp `pos` to `[0, frame.width - widget.width]` before passing to
  `addWidget()` or `moveWidget()`.

### Q5: Undo/redo too granular for resize operations

- **File**: `qmlui/qml/virtualconsole/VCWidgetItem.qml:265-277`
- **Verified**: YES — each `onPositionChanged` during resize calls `snapToGrid()` which
  calls `updateGeometry()` which calls `setGeometry()` which enqueues a Tardis action.
  A single resize drag can produce dozens of undo entries.
- **Impact**: Pressing Ctrl+Z after a resize undoes a single pixel movement instead of
  the entire resize operation. Users must press undo many times.
- **Proposed fix**: Record geometry on `onPressed`, set geometry on `onReleased` only.
  Single Tardis action per resize operation.

### Q6: Auto-scroll to current step in CueList

- **File**: `qmlui/qml/ChaserWidget.qml:115-117`, `qmlui/qml/virtualconsole/VCCueListItem.qml`
- **Verified**: YES — `scrollToItem(index)` method exists but is never called from the
  VCCueList widget during playback.
- **Impact**: During live playback, the current step can scroll out of view. Operator
  must manually scroll to see which step is active. Major usability issue for long cuelists.
- **Proposed fix**: Call `chWidget.scrollToItem(playbackIndex)` on `playbackIndexChanged`.

### Q7: Steps count can shrink while side fader calculates step index

- **File**: `qmlui/virtualconsole/vccuelist.cpp:260-270`
- **Verified**: YES — `setSideFaderLevel()` computes `newStep` from `ch->stepsCount()`,
  but if steps are deleted concurrently (e.g., via chaser editor), `newStep` can exceed
  the new count.
- **Impact**: Out-of-bounds step index passed to ChaserAction, causing undefined behavior
  or crash. Edge case — requires editing chaser while side fader is active.
- **Proposed fix**: `newStep = qMin(newStep, ch->stepsCount() - 1);`

### Q8: Page loader race condition on document load

- **File**: `qmlui/qml/virtualconsole/VirtualConsole.qml:104-110`
- **Verified**: YES — `onDocLoadedChanged` sets `pageLoader.active = false; active = true`
  to force a reload. This is a known Qt anti-pattern: the Loader may not fully tear down
  before re-creating, causing `contentItem` to be null during `renderPage()`.
- **Impact**: Widgets occasionally fail to render on first page load after opening a project.
  A second page switch usually fixes it.
- **Proposed fix**: Use Loader's `onLoaded` signal or `Qt.callLater()` to sequence teardown
  and recreation.

### Q9: Canvas paint flicker from multiple signal sources

- **File**: `qmlui/qml/virtualconsole/VCFrameItem.qml:199-214`
- **Verified**: YES — four different Connections targets fire `gridCanvas.requestPaint()`
  independently: `snappingSizeChanged`, `snappingChanged`, `editModeChanged`, `layoutModeChanged`.
- **Impact**: Multiple `requestPaint()` calls in the same frame cause unnecessary redraws
  and potential flicker of the grid overlay.
- **Proposed fix**: Debounce with `Timer { interval: 16; onTriggered: gridCanvas.requestPaint() }`.

### Q10: Missing null check on `component->create()` in all render() methods

- **File**: `vcslider.cpp:159-162`, `vcbutton.cpp:101-104`, `vcframe.cpp:107-110`
- **Verified**: YES — after `component->create()`, `m_item` is used immediately
  (`m_item->setParentItem(parent)`) without checking for null.
- **Impact**: If QML component instantiation fails (corrupt resource, missing import),
  null pointer dereference crashes the app. Low probability but fatal.
- **Proposed fix**: `if (m_item == nullptr) { qWarning() << "Failed to create" << ...; delete component; return; }`

---

## 3. FLOW CONSOLE BACKPORT OPPORTUNITIES

The Flow Console (`qmlui/flowconsole/`, `qmlui/qml/flowconsole/`) is a new widget layout
system that addresses many VC pain points. Key architectural files:

- **C++**: `flowconsole.h/cpp` (815 lines), `flowlayoutdata.h` (52 lines)
- **QML**: `FlowConsole.qml` (200 lines), `FlowSectionItem.qml` (357 lines),
  `FlowWidgetItem.qml` (148 lines), plus 9 widget-type QML files

### F1: Responsive grid layout (replaces freeform positioning)

- **Flow Console**: Uses Qt `GridLayout` with `Layout.columnSpan` and `Layout.fillWidth`.
  Sections use `Flow` for automatic wrapping. Widget positions are managed by the layout
  engine, not by manual x/y coordinates.
- **VC Today**: Absolute positioning with `x`, `y`, `width`, `height` properties. Manual
  drag-and-drop. Grid snapping is an approximation, not true grid layout.
- **Backport impact**: Adding an optional "grid layout mode" to VCFrame would eliminate
  the entire drag/resize subsystem for widgets that opt in. Would fix B6 (coordinate bugs),
  Q1 (min size), Q3 (snap overhead), Q4 (bounds), Q5 (undo granularity) in one stroke.
- **Effort**: Medium-high. Requires adding `GridLayout` as alternative to freeform in VCFrame.

### F2: Size presets for frames/sections

- **Flow Console**: Sections offer "full", "half", "third", "quarter" width presets.
  Width calculated as `(parentFlowWidth - spacing) / N`.
- **VC Today**: Manual resize only. No quick-sizing options.
- **Backport impact**: Quick buttons in frame properties to set common widths. Reduces
  tedious pixel-level resizing. Especially useful for tablet/touch interfaces.
- **Effort**: Low. Add a few preset buttons to VCFrame properties panel.

### F3: Centralized edit toolbar (replaces per-widget drag/resize)

- **Flow Console**: `FlowWidgetItem.qml` (148 lines) provides a floating toolbar overlay
  with move-up/down, span adjust, and delete buttons. Every widget inherits it.
  Compare VC's `VCWidgetItem.qml` at 387 lines with 8 resize handles and complex drag logic.
- **VC Today**: Each widget has its own resize handles, drag areas, and context menus.
  Edit operations spread across right panel, context menus, and keyboard shortcuts.
- **Backport impact**: Extract a `VCEditOverlay` component. Reduces per-widget QML by ~60%.
  Consistent edit UX across all widget types.
- **Effort**: Medium. Refactor VCWidgetItem to compose rather than inherit overlay.

### F4: Section collapse/expand

- **Flow Console**: Click section header to collapse. Only header visible when collapsed.
  State persisted in XML.
- **VC Today**: VCFrame pages exist but no simple collapse. Frame pages are a different
  concept (switching between sets of widgets, not hiding a frame).
- **Backport impact**: Allow frames to collapse to just their header. Saves screen real
  estate during live shows. Operators can expand only the sections they need.
- **Effort**: Low. Add a `collapsed` bool property to VCFrame, hide children when true.

### F5: Column span per widget

- **Flow Console**: Widgets have `colSpan` (1 to section.columns). Adjustable via +/- buttons.
  Persisted in XML. Central concept for responsive layout.
- **VC Today**: No concept of column span. Widgets have fixed absolute width.
- **Backport impact**: Only useful if grid layout (F1) is implemented. Together they enable
  responsive widget sizing without manual pixel adjustment.
- **Effort**: Low (given F1). Add `colSpan` property to VCWidget, expose in properties.

### F6: Flat widget lookup with QHash

- **Flow Console**: `QHash<quint32, VCWidget*> m_widgetsMap` for O(1) access by ID.
  No tree traversal needed.
- **VC Today**: Already partially implemented — `VirtualConsole::m_widgetsMap` exists.
  But some operations still traverse the VCFrame tree.
- **Backport impact**: Ensure all widget lookups go through the hash map. Eliminates O(N)
  tree traversals that can cause lag with many widgets.
- **Effort**: Low. Already mostly in place, just needs consistent usage.

### F7: Inline widget controls (move up/down, adjust span, delete)

- **Flow Console**: Selected widget shows a compact toolbar overlay with action buttons.
  No context menus needed. No right panel required for basic operations.
- **VC Today**: Edit operations require opening a right panel or using context menus.
  Keyboard shortcuts exist but are not discoverable.
- **Backport impact**: Better discoverability of edit operations. Faster workflow for
  touch interfaces. Can coexist with existing keyboard shortcuts and context menus.
- **Effort**: Low-medium. Add a `Row` overlay to VCWidgetItem shown when selected in edit mode.

### F8: Data-driven section model (replaces imperative `renderPage()`)

- **Flow Console**: `sectionsForPage()` returns `QVariantList` of maps. QML uses
  `Repeater` with model data. Declarative pattern.
- **VC Today**: `renderPage()` imperatively creates QML items via `QQmlComponent::create()`.
  Manual parent/property setup. Error-prone (see B2, Q10).
- **Backport impact**: Eliminates the QQmlComponent leak pattern entirely. QML engine
  manages component lifecycle. Easier to add/remove widgets dynamically.
- **Effort**: High. Would require significant refactoring of VirtualConsole rendering.

---

## Upstream Issue Cross-References

| Our Finding | Upstream Issue | Relationship |
|-------------|---------------|-------------|
| B1 (connect typo) | [#79](https://github.com/mcallegari/qlcplus/issues/79) | Possibly related — "strange behavior" after cuelist setup |
| B5 (state not reset) | [#1595](https://github.com/mcallegari/qlcplus/issues/1595) | Related — cuelist needs reload to work properly |
| B6 (coord transform) | [#1316](https://github.com/mcallegari/qlcplus/issues/1316) | **Direct match** — frame jumps to (0,0), app freezes |
| B6 (coord transform) | [#835](https://github.com/mcallegari/qlcplus/issues/835) | Related — frame size resets on property edit |
| B7 (thread safety) | [commit 0435145](https://github.com/mcallegari/qlcplus/commit/04351458b156693286e094c8ee774f690e84b0f2) | Same function — crash fix for null channel in `writeDMXLevel()` |
| B7 (thread safety) | [#1011](https://github.com/mcallegari/qlcplus/issues/1011) | Related — macOS crash on play, feedback from wrong thread |
| B8 (crossfade race) | [#439](https://github.com/mcallegari/qlcplus/issues/439) | Related — crossfade slider dip at 50% due to HTP blending |
| B5 (state not reset) | [#1793](https://github.com/mcallegari/qlcplus/issues/1793) | Tangential — state persistence issues across reload |

---

## Priority Roadmap

### Phase 1: One-line fixes (immediate, zero risk)
- **B1**: `connect` -> `disconnect` at vccuelist.cpp:476
- **B2**: Add `delete component;` in all render() methods
- **B3**: Add null guard on VCButtonItem.qml:32
- **B4**: Add bounds check in getPrevIndex()
- **B9**: Add `duration > 0` guard at vccuelist.cpp:972
- **Q10**: Add `m_item` null check after create()

### Phase 2: State management fixes (next sprint, moderate risk)
- **B5**: Reset CueList state on function stop
- **B7**: Thread-safe monitor value access
- **B8**: Serialize crossfade state changes
- **Q7**: Clamp step index to valid range

### Phase 3: Drag/drop hardening (medium effort)
- **B6**: Fix coordinate transformation for nested frames
- **B10/B11**: Clean up binding restoration
- **Q1-Q5**: Resize bounds, flickable fix, snap caching, drop clamping, undo batching

### Phase 4: UX improvements
- **Q6**: Auto-scroll cuelist to current step
- **Q8**: Fix page loader race
- **Q9**: Debounce canvas repaints

### Phase 5: Flow Console backports (feature work)
- **F1+F5**: Optional grid layout mode for VCFrame
- **F2**: Size presets
- **F3+F7**: Centralized edit toolbar
- **F4**: Collapsible frames
