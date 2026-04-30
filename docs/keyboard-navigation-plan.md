# Keyboard Navigation Plan for QLC+ v5

## Current State

Keyboard navigation is nearly non-existent across the QLC+ QML UI.

### What Works
- **Global shortcuts**: Ctrl+S (save), Ctrl+Z / Ctrl+Shift+Z (undo/redo), Ctrl+A (select all fixtures), Delete (context-sensitive delete)
- **VC widget key bindings**: User-assignable key sequences to VC buttons/sliders via properties panel
- **VC edit-mode shortcuts**: Ctrl+C/V/D/A, Delete, arrow nudging
- **Ctrl+E**: Toggles VC edit mode
- **Escape**: Closes popups, tool overlays, cancels text editing
- **F2**: Starts inline text editing in CustomTextInput
- **Enter/Return**: Accepts dialogs
- **Tab**: Only works in Fixture Editor fields and PIN popup (3 places total)

### What's Completely Missing
- No keyboard shortcuts to switch between main views (toolbar is mouse-only)
- No FocusScope usage anywhere (zero instances in the codebase)
- No Tab navigation between major UI sections
- No activeFocusOnTab on any interactive component
- No arrow key navigation in function lists, fixture trees, or grids
- No focus indicators (focus rings) on any component
- ShowManager, SimpleDesk, FlowConsole, InputOutputManager: zero keyboard support
- No accessibility properties (Accessible.role, Accessible.name) anywhere
- Impossible to operate the app keyboard-only beyond VC widget key bindings

## Architecture: 3-Layer System

### Layer 1: C++ KeyboardNavigationManager (global shortcuts + zone tracking)

New class `KeyboardNavigationManager` exposed as QML context property `keyNavManager`.

```cpp
class KeyboardNavigationManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int activeFocusZone READ activeFocusZone
               WRITE setActiveFocusZone NOTIFY activeFocusZoneChanged)
    Q_PROPERTY(bool modalActive READ modalActive
               WRITE setModalActive NOTIFY modalActiveChanged)

public:
    enum FocusZone {
        ZoneToolbar = 0,
        ZoneLeftPanel,
        ZoneMainContent,
        ZoneRightPanel,
        ZoneBottomPanel
    };
    Q_ENUM(FocusZone)

    bool handleNavigationKey(QKeyEvent *e);

    Q_INVOKABLE void registerZone(int zone, QQuickItem *item);
    Q_INVOKABLE void unregisterZone(int zone);
    Q_INVOKABLE void focusNextZone();
    Q_INVOKABLE void focusPreviousZone();

signals:
    void activeFocusZoneChanged(int zone);
    void escapeNavigation();
    void modalActiveChanged();
};
```

**Integration point**: `App::keyPressEvent` (app.cpp line 350) — insert before existing ContextManager dispatch:

```cpp
void App::keyPressEvent(QKeyEvent *e)
{
    e->ignore();
    if (m_keyNavManager && m_keyNavManager->handleNavigationKey(e))
        return;
    if (m_contextManager && !isTextInputShortcut(e))
        m_contextManager->handleKeyPress(e);
    if (!e->isAccepted())
        QQuickView::keyPressEvent(e);
}
```

**View switching**: Ctrl+1-6 mapped to contexts:
- Ctrl+1 = Fixtures & Functions
- Ctrl+2 = Virtual Console
- Ctrl+3 = Simple Desk
- Ctrl+4 = Show Manager
- Ctrl+5 = Input/Output
- Ctrl+6 = Flow Console

### Layer 2: QML FocusZone Components

New `FocusZone.qml` — a FocusScope wrapper that registers with the manager:

```qml
FocusScope {
    id: focusZone
    property int zoneId: -1
    property bool isActiveZone: keyNavManager.activeFocusZone === zoneId

    Component.onCompleted: {
        if (zoneId >= 0)
            keyNavManager.registerZone(zoneId, focusZone)
    }
    Component.onDestruction: {
        if (zoneId >= 0)
            keyNavManager.unregisterZone(zoneId)
    }

    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.width: isActiveZone ? 1 : 0
        border.color: UISettings.highlight
        opacity: 0.3
        z: 100
        visible: isActiveZone
    }

    onIsActiveZoneChanged: {
        if (isActiveZone) focusZone.forceActiveFocus()
    }
}
```

Applied to each view's major regions:
- `MainView.qml`: toolbar wrapped in FocusZone
- `FixturesAndFunctions.qml`: left panel, center, right panel, bottom panel
- `VirtualConsole.qml`, `FlowConsole.qml`, `SimpleDesk.qml`, etc.: main content zone

### Layer 3: Per-Widget Focus Indicators

Add to base components:
- `IconButton.qml`: `activeFocusOnTab: true`, focus ring visual state
- `GenericButton.qml`: focus support, Enter/Space activation
- `MenuBarEntry.qml`: `activeFocusOnTab: true`, focus ring
- `CustomTextInput.qml`: Tab/Escape handling to escape focus traps

## Key Behaviors

| Key | Behavior |
|-----|----------|
| **Tab** | Cycle forward through focus zones (toolbar -> left -> content -> right) |
| **Shift+Tab** | Cycle backward through focus zones |
| **Ctrl+1-6** | Switch to view (Fixtures, VC, SimpleDesk, ShowManager, I/O, FlowConsole) |
| **Arrow keys** | Navigate within lists, trees, grids, fader rows |
| **Enter/Space** | Activate focused item |
| **Escape** | Context-sensitive: exit text edit -> close panel -> return to toolbar |
| **F11** | Toggle fullscreen |

## Escape Key Chain

Priority order (first match wins):
1. Text input focused + editing: exit editing mode, restore original text
2. Text input focused + not editing: unfocus text input, return focus to parent zone
3. Side panel open: close side panel
4. Editor open in side panel: go back to previous view
5. Nothing special: move focus to toolbar zone

## Modal/Popup Handling

- `CustomPopupDialog` already has `modal: true` and `closePolicy: Popup.CloseOnEscape`
- When modal active: suppress zone cycling, Tab/Shift+Tab work within popup only
- `KeyboardNavigationManager.modalActive` tracks state

## Text Input Focus Trap Prevention

Add to `CustomTextInput.qml`:
```qml
Keys.onTabPressed: (event) => {
    if (!readOnly) {
        editingFinished()
        event.accepted = false  // propagate Tab to zone cycling
    }
}
Keys.onEscapePressed: (event) => {
    if (!readOnly) {
        setEditingStatus(false)
        text = originalText
        event.accepted = true
    } else {
        event.accepted = false
    }
}
```

## Implementation Phases

### Phase 1: Foundation (non-breaking)
- Create `KeyboardNavigationManager` C++ class
- Register as context property in `App::startup()`
- Integrate `handleNavigationKey` in `App::keyPressEvent`
- Add Ctrl+1-6 view switching
- **Files**: keyboardnavigationmanager.h/.cpp, app.h, app.cpp, CMakeLists.txt

### Phase 2: Zone cycling
- Create `FocusZone.qml`, register in qmldir + qmlui.qrc
- Wrap toolbar in MainView.qml
- Add FocusZones to all 6 views
- Implement Tab/Shift+Tab zone cycling
- **Files**: FocusZone.qml, MainView.qml, FixturesAndFunctions.qml, VirtualConsole.qml, FlowConsole.qml, SimpleDesk.qml, ShowManager.qml, InputOutputManager.qml

### Phase 3: Focus indicators
- Add `activeFocusOnTab: true` to IconButton, MenuBarEntry
- Add focus ring visual states
- Add Enter/Space activation to GenericButton
- Add Tab/Escape handling to CustomTextInput, CustomTextEdit
- **Files**: IconButton.qml, GenericButton.qml, MenuBarEntry.qml, CustomTextInput.qml, CustomTextEdit.qml, UISettings.qml

### Phase 4: Intra-region arrow navigation
- Tree views: arrow keys in TreeNodeDelegate-based lists
- VC widget grid: arrow key cell navigation
- FlowConsole sections: arrow key widget navigation
- SimpleDesk: Left/Right fader selection, Up/Down value adjust
- ShowManager: arrow key track/item navigation
- **Files**: TreeNodeDelegate.qml, FunctionManager.qml, VCPageArea.qml, FlowConsole.qml, SimpleDesk.qml, ShowManager.qml

### Phase 5: Escape chain + modal handling
- Wire Escape chain in FocusZone
- Connect modal popups to `KeyboardNavigationManager.modalActive`
- Test all popup dialogs for correct focus trapping
- **Files**: FocusZone.qml, CustomPopupDialog.qml, ActionsMenu.qml

## Risks and Mitigations

| Risk | Mitigation |
|------|-----------|
| Breaking VC key bindings | Manager only consumes Tab, Shift+Tab, Escape, Ctrl+digit; all other keys pass through |
| Focus loss after view switch | FocusZone.onDestruction unregisters; manager defaults to ZoneMainContent after switch |
| FocusScope nesting conflicts | Keep nesting shallow: MainView -> FocusZone (toolbar) + Loader -> View -> FocusZones |
| Ctrl+1-6 conflicts with OS | Ctrl+digit is free on macOS (Cmd is used instead) and Linux |
| Text input Tab trapping | CustomTextInput.qml handles Tab to confirm+propagate |

## Files Inventory

### New files
- `qmlui/keyboardnavigationmanager.h`
- `qmlui/keyboardnavigationmanager.cpp`
- `qmlui/qml/FocusZone.qml`

### Modified files (~20)
- `qmlui/app.h` — add m_keyNavManager member
- `qmlui/app.cpp` — create manager, integrate in keyPressEvent
- `qmlui/CMakeLists.txt` — add sources
- `qmlui/qmlui.qrc` — add FocusZone.qml
- `qmlui/qml/qmldir` — register FocusZone
- `qmlui/qml/MainView.qml` — wrap toolbar in FocusZone
- `qmlui/qml/fixturesfunctions/FixturesAndFunctions.qml` — add FocusZones
- `qmlui/qml/virtualconsole/VirtualConsole.qml` — add FocusZone
- `qmlui/qml/flowconsole/FlowConsole.qml` — add FocusZone
- `qmlui/qml/SimpleDesk.qml` — add FocusZone
- `qmlui/qml/showmanager/ShowManager.qml` — add FocusZone
- `qmlui/qml/inputoutput/InputOutputManager.qml` — add FocusZone
- `qmlui/qml/IconButton.qml` — activeFocusOnTab, focus ring
- `qmlui/qml/GenericButton.qml` — focus support, keyboard activation
- `qmlui/qml/MenuBarEntry.qml` — activeFocusOnTab, focus ring
- `qmlui/qml/CustomTextInput.qml` — Tab/Escape focus escape
- `qmlui/qml/CustomTextEdit.qml` — Tab/Escape focus escape
- `qmlui/qml/UISettings.qml` — focus ring color property
- `qmlui/qml/TreeNodeDelegate.qml` — arrow key navigation
- `qmlui/qml/fixturesfunctions/FunctionManager.qml` — arrow key tree navigation
