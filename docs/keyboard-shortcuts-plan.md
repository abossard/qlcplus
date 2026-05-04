# QLC+ v5 Keyboard Shortcuts and Shortcut Tooltips Plan

## Goal

Bring the most useful QLC+ v4 keyboard shortcuts to the v5 QML UI and expose those shortcuts in tooltips and menu entries. The first implementation should be conservative: global project actions, undo/redo, panic/fullscreen, Function Manager editing shortcuts, and Show Manager playback shortcuts.

## Codebase findings

- `qmlui/qml/IconButton.qml` already supports `property string tooltip: ""` and a dark themed `ToolTip` shown when `tooltip && hovered`.
- `qmlui/qml/GenericButton.qml` is a `Rectangle` with `MouseArea { id: gbMouseArea; hoverEnabled: true }`, but it has no tooltip property or `ToolTip` import.
- `qmlui/qml/ContextMenuEntry.qml` currently renders icon + `entryText` in `entryRow`; it has no shortcut property, right-aligned shortcut label, or tooltip.
- `qmlui/qml/ActionsMenu.qml` owns project action flow and dialogs:
  - `handleSaveAction()` saves current file or opens `openDialog(App.SaveAsMode)`.
  - New/Open menu handlers already implement `qlcplus.docModified` save-first flow through `saveFirstPopup.action = "#NEW"` / `"#OPEN"`.
  - `openDialog(opMode)` opens either `nativeDialog` or `customDialog`.
  - Undo/redo menu entries currently call `tardis.undoAction()` / `tardis.redoAction()` directly.
  - Fullscreen menu entry calls `qlcplus.toggleFullscreen()` directly.
- `qmlui/qml/MainView.qml` is the right place for global `Shortcut` objects because it owns `actionsMenu`, `mainToolbar`, `currentContext`, `mainViewLoader`, and access-mask context switching.
- Existing v5 VC shortcuts live in `qmlui/qml/virtualconsole/VirtualConsole.qml`:
  - `Keys.onPressed`: Ctrl+C/V/D/A, Delete/Backspace, Arrow nudge, Shift+Arrow nudge.
  - `Shortcut`: `StandardKey.Undo` and `StandardKey.Redo`, guarded with `Window.activeFocusItem instanceof TextInput || TextEdit`.
- Function Manager implementation is split:
  - `qmlui/qml/fixturesfunctions/FunctionManager.qml` renders the function list and selects with `functionManager.selectFunctionID()` / `functionManager.selectFolder()`.
  - `qmlui/qml/fixturesfunctions/RightPanel.qml` owns toolbar buttons and dialogs for add/wizard/delete/clone. Delete opens `deleteItemsPopup`, then calls `functionManager.deleteSelectedFolders()` and `functionManager.deleteFunctions(functionManager.selectedFunctionsID())`; clone calls `functionManager.cloneFunctions()`; wizard opens `functionWizardPopup`.
  - `qmlui/functionmanager.h` exposes `selectedFunctionsID()`, `selectedItemNames()`, `selectFunctionID()`, `deleteFunctions()`, `deleteSelectedFolders()`, `cloneFunctions()`, `selectedFunctionCount`, and `selectedFolderCount`; no select-all QML invokable exists yet.
- Show Manager implementation:
  - `qmlui/qml/showmanager/ShowManager.qml` has buttons for delete/copy/paste/play/stop.
  - `qmlui/showmanager.h` exposes `deleteShowItems(selectedItemRefs())`, `copyToClipboard()`, `pasteFromClipboard()`, `playShow()`, `stopShow()`, `resetItemsSelection()`, `selectedItemRefs()`, `selectedItemNames()`, and `selectedItemsCount`.

## Shared rules for every phase

1. **Use wrapper functions, not direct duplicated backend calls.** Shortcuts should call the same QML wrapper functions used by buttons/menu entries so save prompts, file dialogs, confirmation dialogs, and UI state stay identical.
2. **Respect `qlcplus.accessMask`.** A shortcut must only be enabled when the equivalent visible UI action is available. Kiosk mode (`qlcplus.accessMask === App.AC_VCControl`) must not expose project/file editing shortcuts.
3. **Guard modal/text editing states.** Shortcuts must not fire while a popup/dialog is open, while a file dialog is active, or while a `TextInput`/`TextEdit` has focus.
4. **Preserve existing VC editor shortcuts.** Do not add global Ctrl+D for DMX Dump because `VirtualConsole.qml` already uses Ctrl+D for duplicate selection.
5. **Skip Ctrl+F12 for now.** There is no clean v5 Operate/Design toggle equivalent yet.
6. **Use platform-aware visible text.** Tooltip/menu text should display `⌘` on macOS (`Qt.platform.os === "osx"`) and `Ctrl` elsewhere.

## Opus 4.7 Review — Critical Fixes Applied

### B1: Remove VC Undo/Redo Shortcuts (ambiguity fix)
`VirtualConsole.qml` lines 77–96 define `Shortcut { sequence: StandardKey.Undo/Redo }`. The global ones in MainView cover the same sequences. Having both at `WindowShortcut` scope causes `QQuickShortcut: Ambiguous shortcut overload` — neither fires reliably.
**Decision:** Remove the VC-local Undo/Redo entirely. The global MainView shortcuts already include the TextInput/TextEdit focus guard, so VC editing is fully covered.

### B2: Use Shortcut Objects (not Keys.onPressed) for Phase 5
`Keys.onPressed` requires the root element to have focus. In practice, clicking the function tree, search box, or right panel moves focus away and shortcuts silently stop working.
**Decision:** Phase 5A and 5B must use `Shortcut` objects with `enabled: mainView.currentContext === "FIXANDFUNC" && ...` guards, not `Keys.onPressed`.

### N1: Global Popup Counter for shortcutsBlocked()
The `shortcutsBlocked()` guard only covers ActionsMenu's own dialogs. Other popups (color picker, MIDI learn, function wizard, delete confirm, fixture wizard) are not covered.
**Decision:** Track open popups via a `property int popupCount: 0` on `mainView`. Modify `PopupBox.qml` (or the shared popup base) to increment/decrement on `onOpened`/`onClosed`. Then `shortcutsBlocked()` checks `popupCount > 0`.

### N2: macOS Key Binding Mismatch
`Shortcut { sequence: Qt.platform.os === "osx" ? "Meta+Shift+Esc" : "Ctrl+Shift+Esc" }` on macOS binds to literal Control key, not Cmd. But `ShortcutUtils.display()` shows `⌘⇧⎋`. Display and binding mismatch.
**Decision:** For non-StandardKey string sequences, branch per platform:
```qml
sequence: Qt.platform.os === "osx" ? "Meta+Shift+Esc" : "Ctrl+Shift+Esc"
```
Or pick non-conflicting macOS bindings for Panic and Show Manager Stop.

### S4: sequences (plural) confirmed safe
Qt 6 supports `Shortcut.sequences: [...]`. Remove uncertainty note.

## Suggested shortcut helper

Add a small helper module so shortcut display strings and focus guards are consistent.

Files:

- Add: `qmlui/js/ShortcutUtils.js`
- Update: `qmlui/qmlui.qrc` under `<!-- JavaScript helpers -->`:
  - `<file alias="ShortcutUtils.js">js/ShortcutUtils.js</file>`
- Import where needed:
  - `import "ShortcutUtils.js" as ShortcutUtils`

Patterns:

```qml
// Tooltip/menu display text only; actual Shortcut.sequence still uses StandardKey or explicit strings.
ShortcutUtils.display("Ctrl+S")    // macOS: "⌘S"; others: "Ctrl+S"
ShortcutUtils.withShortcut(qsTr("Save project"), "Ctrl+S")
ShortcutUtils.isTextEditing(Window.activeFocusItem)
```

Implementation notes:

- Keep helper pure JavaScript.
- Convert only labels that are shown to users; do not translate key names separately unless already translated.
- Treat `Ctrl+Shift+Z`, `Ctrl+Space`, `Ctrl+Shift+Esc`, `F11`, and `Ctrl+F11` explicitly.

## Phase 1: Tooltip support for `GenericButton`

### What changes

Add tooltip support to `GenericButton.qml`, matching `IconButton.qml` behavior and styling.

### Files

- `qmlui/qml/GenericButton.qml`

### Code patterns

- Add `import QtQuick.Controls.Basic` alongside `import QtQuick`.
- Add `property string tooltip: ""` near existing visual properties.
- Add a `ToolTip` sibling before/after the disabled overlay:

```qml
ToolTip
{
    visible: btnRoot.tooltip && gbMouseArea.containsMouse
    text: btnRoot.tooltip
    delay: 1000
    timeout: 5000
    background: Rectangle { color: UISettings.bgMedium; border.width: 1; border.color: UISettings.bgLight }
    contentItem: Text { text: btnRoot.tooltip; color: "white" }
}
```

- Use `gbMouseArea.containsMouse` because `GenericButton` is a `Rectangle`, not a `Button` with `hovered`.
- Keep the disabled overlay behavior unchanged.

### What to test

- Existing `GenericButton` clicks still work with left and right mouse buttons.
- Existing press-and-hold repetition still works.
- Tooltip appears after ~1 second and disappears after timeout or mouse exit.
- Disabled `GenericButton` does not accept clicks.

### Risk

Low. The component already tracks hover state; this is mostly copying `IconButton.qml` tooltip styling.

## Phase 2: Shortcut text on `ContextMenuEntry`

### What changes

Show right-aligned shortcut hints in menu rows, especially `ActionsMenu.qml`. Optionally allow a tooltip on menu entries, but primary value is visible shortcut text.

### Files

- `qmlui/qml/ContextMenuEntry.qml`
- `qmlui/qml/ActionsMenu.qml`

### Code patterns

In `ContextMenuEntry.qml`:

- Add properties:

```qml
property string shortcutText: ""
property string tooltip: ""
property int shortcutLeftMargin: UISettings.iconSizeDefault
```

- Replace the current `Row { id: entryRow ... }` width calculation with a layout that allows right alignment. Keep `itemWidth` compatible by including the shortcut width:

```qml
property int itemWidth: entryContent.implicitWidth + 20

Row
{
    id: entryContent
    x: 5
    width: baseMenuEntry.width - 10
    spacing: 5
    // existing icon/fa/text elements
    Item { width: shortcutText ? shortcutLeftMargin : 0; height: 1 }
    RobotoText
    {
        visible: shortcutText !== ""
        label: shortcutText
        height: baseMenuEntry.height
        fontSize: UISettings.textSizeSmall
        color: UISettings.fgMedium
    }
}
```

Preferred refinement: use `RowLayout` with the existing label as `Layout.fillWidth: true` and the shortcut label aligned right. Keep visual spacing consistent with current `x: 5`, `spacing: 5`, `height: iconHeight + 6`.

- If adding tooltip, copy `IconButton.qml` style and use `entryMouseArea.containsMouse`.

In `ActionsMenu.qml`:

- Import `ShortcutUtils.js`.
- Set shortcut text on menu entries:
  - `fileNew`: `shortcutText: ShortcutUtils.display("Ctrl+N")`
  - `fileOpen`: `shortcutText: ShortcutUtils.display("Ctrl+O")`
  - `fileSave`: `shortcutText: ShortcutUtils.display("Ctrl+S")`
  - undo entry: `shortcutText: ShortcutUtils.display("Ctrl+Z")`
  - redo entry: `shortcutText: ShortcutUtils.display("Ctrl+Shift+Z")`
  - `fullScreen`: `shortcutText: ShortcutUtils.display("F11")`

### What to test

- Actions menu still sizes correctly with and without shortcut text.
- Recent-file submenu still opens on hover.
- Language submenu grid entries still render correctly without shortcut text.
- Disabled entries still show the disabled overlay correctly.

### Risk

Medium. `ContextMenuEntry` is reused by submenus and grids; layout changes can affect menu sizing. Keep the API additive and visually test Actions, Recent, Network, Language, and Add Function menus.

## Phase 3: Add tooltip text to existing buttons

### What changes

Add or enrich tooltip strings on existing `IconButton` and new `GenericButton.tooltip` instances so shortcut hints appear where users already click.

### Files

- `qmlui/qml/MainView.qml`
- `qmlui/qml/ActionsMenu.qml`
- `qmlui/qml/fixturesfunctions/RightPanel.qml`
- `qmlui/qml/showmanager/ShowManager.qml`
- Any visible `GenericButton` call sites chosen for first pass, e.g. `qmlui/qml/flowconsole/FlowSectionItem.qml`

### Code patterns

- Import `ShortcutUtils.js` in files that show shortcut hints.
- Use helper composition rather than hard-coded platform strings:

```qml
tooltip: ShortcutUtils.withShortcut(qsTr("Stop all the running functions"), "Ctrl+Shift+Esc")
```

Recommended first-pass updates:

- `MainView.qml`
  - `sceneDump.tooltip`: keep descriptive text only, or explicitly avoid Ctrl+D: `qsTr("Dump DMX values on a Scene")`.
  - `stopAllButton.tooltip`: add `Ctrl+Shift+Esc`.
- `ActionsMenu.qml`
  - Match shortcut text added in Phase 2.
- `RightPanel.qml`
  - `functionWizardBtn.tooltip`: add `Ctrl+W`.
  - `removeFunction.tooltip`: add `Delete`.
  - `cloneFunction.tooltip`: add `Ctrl+C` only for Function Manager context; note this conflicts with text copy when editing text, so it must be guarded.
- `ShowManager.qml`
  - `playbackBtn.tooltip`: add `Space`.
  - `stopBtn.tooltip`: add `Ctrl+Space`.
  - `copyBtn.tooltip`: add `Ctrl+C`.
  - `pasteBtn.tooltip`: add `Ctrl+V`.
  - `removeItem.tooltip`: add `Delete` if implemented in Phase 5.

### What to test

- macOS displays `⌘S`, `⌘N`, etc.; non-macOS displays `Ctrl+S`, `Ctrl+N`, etc.
- Tooltips remain translated for the descriptive part.
- No tooltip claims a shortcut before that shortcut is implemented in Phase 4/5.

### Risk

Low to medium. Mostly string changes, but misleading tooltip text is a UX risk. Add shortcut hints only when the handler exists or lands in the same change.

## Phase 4: Global keyboard shortcuts

### What changes

Add guarded global `Shortcut` objects to `MainView.qml` for project actions, undo/redo, panic/stop-all, and fullscreen.

### Files

- `qmlui/qml/MainView.qml`
- `qmlui/qml/ActionsMenu.qml`
- Optional helper: `qmlui/js/ShortcutUtils.js`
- `qmlui/qmlui.qrc` if adding the helper

### Dependency order

1. Add or refactor action wrapper functions in `ActionsMenu.qml`.
2. Add guard helper functions in `MainView.qml`.
3. Add `Shortcut` objects in `MainView.qml`.
4. Add matching `shortcutText` / tooltip hints.

### ActionsMenu wrapper functions

Refactor existing menu handlers into reusable functions. Shortcuts and menu entries should call these wrappers:

```qml
function handleNewAction()
{
    if (qlcplus.docModified) {
        saveFirstPopup.action = "#NEW"
        saveFirstPopup.open()
    } else {
        qlcplus.newWorkspace()
    }
    menuRoot.close()
}

function handleOpenAction()
{
    if (qlcplus.docModified) {
        saveFirstPopup.action = "#OPEN"
        saveFirstPopup.open()
    } else {
        openDialog(App.OpenMode)
    }
    menuRoot.close()
}

function handleUndoAction()
{
    menuRoot.close()
    tardis.undoAction()
}

function handleRedoAction()
{
    menuRoot.close()
    tardis.redoAction()
}

function handleFullscreenAction()
{
    menuRoot.close()
    qlcplus.toggleFullscreen()
}

function handleStopAllAction()
{
    menuRoot.close()
    qlcplus.stopAllFunctions()
}
```

Existing `handleSaveAction()` already exists; keep it and call it from shortcuts.

### MainView guard patterns

Add `import QtQuick.Window` if needed for `Window.activeFocusItem`.

```qml
function isTextEditingActive()
{
    var focused = Window.activeFocusItem
    return focused && (focused instanceof TextInput || focused instanceof TextEdit)
}

function shortcutsBlocked()
{
    return isTextEditingActive()
        || mainView.popupCount > 0
        || actionsMenu.opened
        || dimScreen.visible
}

function projectShortcutsAllowed()
{
    return !shortcutsBlocked()
        && qlcplus.accessMask !== App.AC_VCControl
}
```

Add a popup counter property to `MainView.qml`:
```qml
property int popupCount: 0
```

Modify `PopupBox.qml` (or `CustomPopupDialog.qml` base) to track:
```qml
onOpened: mainView.popupCount++
onClosed: mainView.popupCount--
```

Use stronger access checks per action:

- New/Open/Save: disabled in kiosk mode (`qlcplus.accessMask !== App.AC_VCControl`).
- Undo/Redo: disabled while text editing; otherwise enabled when not blocked. If undo/redo should be editor-only later, narrow per context.
- Panic/Stop all: enabled when not blocked and user has at least VC control (`qlcplus.accessMask & App.AC_VCControl`) and `qlcplus.runningFunctionsCount > 0`.
- Fullscreen: enabled when not blocked; does not require editing permissions.

### Shortcut objects

In `MainView.qml`, add near `ActionsMenu { id: actionsMenu ... }` or before the toolbar children:

```qml
Shortcut {
    sequence: StandardKey.New
    enabled: mainView.projectShortcutsAllowed()
    onActivated: actionsMenu.handleNewAction()
}
Shortcut {
    sequence: StandardKey.Open
    enabled: mainView.projectShortcutsAllowed()
    onActivated: actionsMenu.handleOpenAction()
}
Shortcut {
    sequence: StandardKey.Save
    enabled: mainView.projectShortcutsAllowed()
    onActivated: actionsMenu.handleSaveAction()
}
Shortcut {
    sequence: StandardKey.Undo
    enabled: !mainView.shortcutsBlocked()
    onActivated: actionsMenu.handleUndoAction()
}
Shortcut {
    sequence: StandardKey.Redo
    enabled: !mainView.shortcutsBlocked()
    onActivated: actionsMenu.handleRedoAction()
}
Shortcut {
    sequence: Qt.platform.os === "osx" ? "Meta+Shift+Esc" : "Ctrl+Shift+Esc"
    enabled: !mainView.shortcutsBlocked()
             && (qlcplus.accessMask & App.AC_VCControl)
             && qlcplus.runningFunctionsCount > 0
    onActivated: actionsMenu.handleStopAllAction()
}
Shortcut {
    sequences: ["F11", "Ctrl+F11"]
    enabled: !mainView.shortcutsBlocked()
    onActivated: actionsMenu.handleFullscreenAction()
}
```

**Important:** Remove the existing `Shortcut { sequence: StandardKey.Undo }` and `StandardKey.Redo` from `VirtualConsole.qml` (lines 77–96) to avoid ambiguous shortcut conflicts. The global MainView shortcuts already include the TextInput/TextEdit focus guard.

Qt 6 supports `Shortcut.sequences` (plural) — confirmed safe.

### Explicitly skipped global shortcut

- Do not add Ctrl+D globally for DMX Dump. `VirtualConsole.qml` uses Ctrl+D for `virtualConsole.duplicateSelection()` in edit mode.
- Do not add Ctrl+F12 until v5 has an explicit wrapper for Design/Operate mode switching.

### What to test

Manual tests:

- Ctrl+N with modified document opens the save-first prompt and does not discard without confirmation.
- Ctrl+O with modified document opens the save-first prompt, then file dialog after choosing No.
- Ctrl+S saves existing file or opens Save As for an unnamed project.
- Ctrl+Z/Ctrl+Shift+Z do nothing while a `TextInput`/`TextEdit` has focus.
- Ctrl+Shift+Esc stops running functions and is disabled when none are running.
- F11 and Ctrl+F11 call `qlcplus.toggleFullscreen()`.
- In kiosk mode (`qlcplus.accessMask === App.AC_VCControl`), New/Open/Save do not trigger.
- While `ActionsMenu`, `saveFirstPopup`, `nativeDialog`, or `customDialog` is open, unrelated shortcuts do not fire.

Build validation:

```bash
cd build && cmake --build . --target qlcplus-qml -j8
```

### Risk

High. Global shortcuts can bypass important dialogs or trigger while typing. The wrapper-first refactor and modal/text guards are mandatory.

## Phase 5: Context-specific shortcuts

### What changes

Add shortcuts that only activate in the relevant loaded context: Function Manager and Show Manager.

### Dependency order

1. Refactor existing toolbar button logic into local wrapper functions.
2. Add missing QML/C++ API if required (`selectAllFunctions()` for Function Manager).
3. Add guarded `Shortcut` / `Keys.onPressed` handlers in context root components.
4. Add matching tooltip/menu text.

## Phase 5A: Function Manager shortcuts

### Files

- `qmlui/qml/fixturesfunctions/FunctionManager.qml`
- `qmlui/qml/fixturesfunctions/RightPanel.qml`
- `qmlui/functionmanager.h`
- `qmlui/functionmanager.cpp`

### Shortcuts

- Delete: delete selected functions/folders via the existing confirmation popup.
- Ctrl+C: clone selected functions.
- Ctrl+A: select all visible functions/folders.
- Ctrl+W: open Function Wizard.

### Code patterns

In `RightPanel.qml`, expose wrapper functions on `rightSidePanel` so shortcuts can reuse button behavior:

```qml
function requestDeleteSelectedItems()
{
    if (!(selectedItemsCount && !functionManager.isEditing))
        return
    var selNames = functionManager.selectedItemNames()
    deleteItemsPopup.message = qsTr("Are you sure you want to delete the following items?") + "\n" + selNames
    deleteItemsPopup.open()
}

function requestCloneSelectedFunctions()
{
    if (functionManager.selectedFunctionCount && !functionManager.isEditing)
        functionManager.cloneFunctions()
}

function requestFunctionWizard()
{
    if (qlcplus.accessMask & App.AC_FunctionEditing)
        functionWizardPopup.open()
}
```

Then update buttons:

```qml
onClicked: rightSidePanel.requestDeleteSelectedItems()
onClicked: rightSidePanel.requestCloneSelectedFunctions()
onClicked: rightSidePanel.requestFunctionWizard()
```

In `FunctionManager.qml` (or its parent layout), add `Shortcut` objects instead of `Keys.onPressed` (focus-independent, per Opus 4.7 review):

```qml
Shortcut {
    sequence: "Delete"
    enabled: mainView.currentContext === "FIXANDFUNC"
             && !mainView.shortcutsBlocked()
             && (qlcplus.accessMask & App.AC_FunctionEditing)
             && functionManager.selectedFunctionCount > 0
    onActivated: rightSidePanel.requestDeleteSelectedItems()
}
Shortcut {
    sequence: StandardKey.Copy
    enabled: mainView.currentContext === "FIXANDFUNC"
             && !mainView.shortcutsBlocked()
             && (qlcplus.accessMask & App.AC_FunctionEditing)
             && functionManager.selectedFunctionCount > 0
    onActivated: rightSidePanel.requestCloneSelectedFunctions()
}
Shortcut {
    sequence: StandardKey.SelectAll
    enabled: mainView.currentContext === "FIXANDFUNC"
             && !mainView.shortcutsBlocked()
    onActivated: functionManager.selectAllFunctions()
}
Shortcut {
    sequence: "Ctrl+W"
    enabled: mainView.currentContext === "FIXANDFUNC"
             && !mainView.shortcutsBlocked()
             && (qlcplus.accessMask & App.AC_FunctionEditing)
    onActivated: rightSidePanel.requestFunctionWizard()
}
```

Accessing `RightPanel.qml` from `FunctionManager.qml` may require routing through an existing parent/right-panel ID. If there is no safe object reference, add the shortcut wrappers to the shared owner that contains both the function manager and right panel, or expose the delete/clone/wizard wrapper through `mainView` only after confirming object ownership.

For Ctrl+A, add a real API rather than walking QML delegates:

```cpp
// qmlui/functionmanager.h
Q_INVOKABLE void selectAllFunctions();
```

Implementation should select currently visible/filter-matching function items and folders in the `TreeModel`, update `m_selectedIDList` / `m_selectedFolderList`, and emit `functionsListChanged()`, `selectedFunctionCountChanged()`, and `selectedFolderCountChanged()` as needed. If selecting folders is complex, document and implement first pass as all visible functions only, then add folder support.

### What to test

- Delete opens the same confirmation dialog as the minus toolbar button.
- Confirming Delete deletes both selected folders and selected functions through existing code.
- Ctrl+C clones selected functions but does not clone while a search `TextInput` is focused.
- Ctrl+A selects all visible functions after filters/search are applied.
- Ctrl+W opens `PopupFunctionWizard` only when `App.AC_FunctionEditing` is available.
- Shortcuts do not fire while a function editor text field is focused.

### Risk

High for Ctrl+A because no current QML invokable exists. Delete/Clone/Wizard are medium if routed through wrappers and guarded.

## Phase 5B: Show Manager shortcuts

### Files

- `qmlui/qml/showmanager/ShowManager.qml`
- `qmlui/showmanager.h` / `qmlui/showmanager.cpp` only if copy/paste/delete behavior needs extra guard APIs

### Shortcuts

- Space: `showManager.playShow()` / pause/resume, same as `playbackBtn`.
- Ctrl+Space: `showManager.stopShow()`, same as `stopBtn`.
- Ctrl+C: `showManager.copyToClipboard()`.
- Ctrl+V: `showManager.pasteFromClipboard()`.
- Delete: optionally delete selected show items via existing `deleteItemsPopup`; include only if desired in first pass.

### Code patterns

In `ShowManager.qml`, add local wrappers near existing helper functions:

```qml
function requestPlayShow()
{
    if (showManager.isEditing)
        showManager.playShow()
}

function requestStopShow()
{
    if (showManager.isEditing)
        showManager.stopShow()
}

function requestCopyItems()
{
    if (showManager.selectedItemsCount)
        showManager.copyToClipboard()
}

function requestPasteItems()
{
    if (showManager.isEditing)
        showManager.pasteFromClipboard()
}

function requestDeleteItems()
{
    if (!showManager.selectedItemsCount)
        return
    deleteItemsPopup.message = qsTr("Are you sure you want to remove the following items?\n" +
                                    "(Note that the original functions will not be deleted)") + "\n" + showManager.selectedItemNames()
    deleteItemsPopup.open()
}
```

Update existing buttons to call wrappers:

```qml
onClicked: showMgrContainer.requestPlayShow()
onClicked: showMgrContainer.requestStopShow()
onClicked: showMgrContainer.requestCopyItems()
onClicked: showMgrContainer.requestPasteItems()
```

Add guarded `Shortcut` objects (not `Keys.onPressed` — per Opus 4.7 review, focus is unreliable):

```qml
Shortcut {
    sequence: "Space"
    enabled: mainView.currentContext === "SHOWMGR"
             && !mainView.shortcutsBlocked()
             && (qlcplus.accessMask & App.AC_ShowManager)
    onActivated: showMgrContainer.requestPlayShow()
}
Shortcut {
    sequence: "Ctrl+Space"
    enabled: mainView.currentContext === "SHOWMGR"
             && !mainView.shortcutsBlocked()
             && (qlcplus.accessMask & App.AC_ShowManager)
    onActivated: showMgrContainer.requestStopShow()
}
Shortcut {
    sequence: StandardKey.Copy
    enabled: mainView.currentContext === "SHOWMGR"
             && !mainView.shortcutsBlocked()
             && showManager.selectedItemsCount > 0
    onActivated: showMgrContainer.requestCopyItems()
}
Shortcut {
    sequence: StandardKey.Paste
    enabled: mainView.currentContext === "SHOWMGR"
             && !mainView.shortcutsBlocked()
             && (qlcplus.accessMask & App.AC_ShowManager)
    onActivated: showMgrContainer.requestPasteItems()
}
Shortcut {
    sequence: "Delete"
    enabled: mainView.currentContext === "SHOWMGR"
             && !mainView.shortcutsBlocked()
             && showManager.selectedItemsCount > 0
    onActivated: showMgrContainer.requestDeleteItems()
}
```

Note: `Ctrl+Space` on macOS may conflict with Spotlight/input source switching. Consider `Cmd+.` as macOS alternative if needed.

### What to test

- Space toggles Show playback only in Show Manager.
- Ctrl+Space stops/rewinds only in Show Manager.
- Ctrl+C/Ctrl+V copy/paste show items, not text, and only when no text editor is focused.
- Delete opens the same confirmation popup as the minus button.
- Shortcuts are disabled while editing the show name `CustomTextEdit`.
- Shortcuts are disabled when `qlcplus.accessMask` does not include `App.AC_ShowManager`.

### Risk

Medium. Playback shortcuts are straightforward; copy/paste/delete need strong text-edit and popup guards because they overlap common editing shortcuts.

## Phase 6: View switching and side panel toggles

Add focus-independent `Shortcut` objects for top-level v5 view navigation:

- `Alt+1` through `Alt+6` select Fixtures & Functions, Virtual Console, Simple Desk, Show Manager, Input/Output, and Flow Console respectively.
- `Ctrl+PgDown` / `Ctrl+PgUp` cycle forward/backward through visible top-level view entries.
- `Ctrl+[` toggles the Fixtures & Functions left panel.
- `Ctrl+]` toggles the active right panel in Fixtures & Functions, Virtual Console, and Show Manager.

Implementation notes:

- Keep these guarded by `!mainView.shortcutsBlocked()`.
- Use `entry.checked = true` for top-level view switches so existing `switchToContext()` behavior stays centralized.
- Use classic JavaScript callbacks (`function(e) { ... }`) in QML, not arrow functions, for compatibility with the QLC+ QML engine.
- Display shortcut hints via `ShortcutUtils.display()` / `ShortcutUtils.withShortcut()` so macOS gets glyphs such as `⌥1`.

### What to test

- `Alt+1` ... `Alt+6` switch only to visible/enabled top-level views.
- `Ctrl+PgDown` / `Ctrl+PgUp` cycle only through visible top-level views and wrap at the ends.
- `Ctrl+[` toggles the Fixtures & Functions left panel only while Fixtures & Functions is active.
- `Ctrl+]` toggles the active view's right panel in Fixtures & Functions, Virtual Console edit access, and Show Manager.
- All new shortcuts are blocked while editing text or while a popup/menu is open.

## Validation checklist before merging implementation

- `qmlui/qml/GenericButton.qml` tooltip style matches `IconButton.qml`.
- `ContextMenuEntry.qml` remains backward-compatible for all existing menus.
- All shortcut-visible strings use platform-aware display (`⌘` on macOS, `Ctrl` elsewhere).
- Every shortcut calls a wrapper that is also used by the clickable UI action.
- New/Open still go through `saveFirstPopup` when `qlcplus.docModified` is true.
- Save still goes through `handleSaveAction()` and opens Save As when unnamed.
- Undo/Redo are blocked for `TextInput` and `TextEdit`, matching the existing `VirtualConsole.qml` guard.
- Global shortcuts are disabled during popups/dialogs and in kiosk mode where appropriate.
- Ctrl+D remains VC Duplicate only; no global DMX Dump shortcut is added.
- Ctrl+F12 remains unimplemented and documented as skipped.
- Build passes:

```bash
cd build && cmake --build . --target qlcplus-qml -j8
```

## Recommended implementation order

1. Phase 1: `GenericButton` tooltip support.
2. Add `ShortcutUtils.js` and qrc entry.
3. Phase 2: `ContextMenuEntry.shortcutText`.
4. Phase 4 wrapper refactor in `ActionsMenu.qml` without adding shortcuts yet.
5. Phase 4 global shortcuts in `MainView.qml`.
6. Phase 3 tooltip/shortcut text updates for actions implemented so far.
7. Phase 5B Show Manager playback/copy/paste wrappers and shortcuts.
8. Phase 5A Function Manager delete/clone/wizard wrappers and shortcuts.
9. Function Manager Ctrl+A C++ API and tests/manual validation.

This order delivers low-risk visible UX improvements first, then global behavior, then context-specific behavior with the highest-risk API addition last.
