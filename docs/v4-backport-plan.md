# Plan: Shared MCP, Audioreactive, DDP & Profiles for QLC+ v4 AND v5

## Problem Statement

The fork (abossard/qlcplus, branch `mcp-server`) has added several major features that currently only work with the v5 QML UI build (`-Dqmlui=ON`): MCP server, audio-reactive RGB scripts, DDP output plugin, and audio/profile improvements. The goal is to make **all of these work with BOTH v4 (Qt Widgets) and v5 (QML) builds**, using a shared codebase that stays easy to merge from upstream (mcallegari/qlcplus).

## Architecture: How MCP Serves Both v4 and v5

The MCP server is designed with a **bridge pattern** that makes it fully usable in both UI versions:

```
┌─────────────────────────────────────────────────────────┐
│                    100% SHARED CODE                     │
│  ┌──────────┐  ┌──────────┐  ┌───────────────────────┐ │
│  │McpServer │  │MCP Tools │  │ VCBridge (abstract)   │ │
│  │(protocol)│→ │(10 files)│→ │ 69 virtual methods    │ │
│  │          │  │          │  │ + static layout math  │ │
│  └──────────┘  └──────────┘  └───────────┬───────────┘ │
│                                          │              │
├──────────────────────────────────────────┼──────────────┤
│              UI ADAPTER LAYER            │              │
│                    ┌─────────────────────┤              │
│                    ▼                     ▼              │
│          ┌──────────────────────────────────────┐        │
│          │        VCBridgeCommon (NEW)          │        │
│          │  Shared base class: ~1400 lines      │        │
│          │  Page CRUD, widget lookup, geometry,  │        │
│          │  appearance, layout/reflow            │        │
│          └──────────────┬───────────────────────┘        │
│                    ┌────┴────────────┐                   │
│                    ▼                 ▼                   │
│          ┌──────────────┐   ┌──────────────┐            │
│          │ VCBridgeV4   │   │ VCBridgeV5   │            │
│          │ (Qt Widgets) │   │ (QML/Quick)  │            │
│          │ ~600 lines   │   │ ~600 lines   │            │
│          └──────┬───────┘   └──────┬───────┘            │
│                 ▼                  ▼                     │
│          ui/src/virtual      qmlui/virtual               │
│          console/*.h         console/*.h                 │
└─────────────────────────────────────────────────────────┘
```

**What's shared (identical in both builds, ~95% of MCP code):**
- MCP HTTP server, JSON-RPC protocol, tool registry (~5 files)
- ALL 10 tool implementation files (query, create, update, input, layout, IO, functions, channels, palettes, prompts)
- VCBridge abstract interface + static layout/reflow algorithms
- **VCBridgeCommon** (~1400 lines) — shared base with page CRUD, widget lookup, geometry, appearance, layout algorithms
- Engine integration (Doc, Functions, Fixtures, Universes, Audio)

**What differs (thin per-version adapters, ~5% of MCP code):**
- VCBridgeV4 (~600 lines) — widget creation, input source mapping (v4 API), method name adapters
- VCBridgeV5 (~600 lines) — widget creation, input source mapping (v5 API), QML-specific calls
- Build entry point (main/main.cpp vs qmlui/main.cpp)

**Why not a fully single bridge?**
Deep analysis reveals v4 and v5 VCWidget share ~40+ identical methods (caption, colors, fonts, geometry, IDs), BUT they have one fundamental architectural difference: **input source handling** — v4 uses single-source-per-widget (`setInputSource()`), v5 uses multi-source lists (`addInputSource()`). This blocks a truly unified bridge, but the ~60-70% that IS identical goes into VCBridgeCommon.

**The MCP protocol, tools, and AI agent experience are 100% identical** regardless of which UI version is running.

## Current Fork Status

- **120 commits ahead** of upstream, 0 behind
- **170 files changed**, 30,025 insertions, 2,749 deletions
- Key areas: mcp/ (37 files, all new), plugins/ddp/ (10 files, all new), resources/rgbscripts/ (24 audio scripts, all new), engine/ (15 files modified), qmlui/ (34 files modified)

---

## Feature Assessment

### Feature 1: DDP Plugin — ✅ ALREADY WORKS (Effort: 0/10)

The DDP plugin (`plugins/ddp/`) uses the standard `QLCIOPlugin` interface and links against `Qt${QT_MAJOR_VERSION}::*`. It is already built for both v4 and v5 — plugins are UI-independent. The configuration dialog uses Qt Widgets (QDialog, QTreeWidget) which works in both builds.

**Action: None required.** Verify it loads in v4 build (symlink to PlugIns/).

### Feature 2: Input Profiles — ✅ ALREADY WORKS (Effort: 0/10)

Profiles are engine-level (`QLCInputProfile` in `engine/src/`). Both v4 and v5 already have profile editors. No new v5-only profile features were added in this fork.

**Action: None required.**

### Feature 3: Audioreactive — ⚠️ MOSTLY WORKS (Effort: 2/10)

The audio stack lives entirely in the shared engine layer:
- `engine/audio/src/audiocapture.{h,cpp}` — audio input + FFT pipeline
- `engine/audio/src/beattracker.{h,cpp}` — spectral flux beat detection
- `engine/src/rgbscriptv4.{h,cpp}` — JavaScript RGB script engine with audio API
- `engine/src/rgbmatrix.cpp` — RGB matrix function execution
- `engine/src/mastertimer.{h,cpp}` — BPM delivery to scripts

All 22 audio-reactive RGB scripts live in `resources/rgbscripts/` (shared).

VCAudioTriggers widget already exists in both v4 (`ui/src/virtualconsole/vcaudiotriggers.*`) and v5 (`qmlui/virtualconsole/vcaudiotriggers.*`).

**Risks:**
- Some engine changes may use `#ifdef QMLUI` guards — need to verify these don't break v4 audio paths
- Beat tracker FFT requires `HAS_FFTW3` — check v4 build links fftw3

**Actions:**
1. Build v4 (`-Dqmlui=OFF`) and verify engine audio compiles cleanly
2. Test that audio RGB scripts execute in v4 RGB Matrix editor
3. Test VCAudioTriggers widget functions in v4

### Feature 4: MCP Server — 🔴 MAJOR WORK (Effort: 8/10)

This is the big one. The MCP server is currently v5-only.

---

## Deep Dive: MCP Server for v4

### Current Architecture

```
McpServer ──→ VCBridge (abstract, 69 virtual methods)
                  │
                  └──→ VCBridgeV5 (QML implementation, 1838 lines)
                           │
                           └──→ qmlui/virtualconsole/*.h (QML widgets)
```

Tool categories and their UI dependency:
- **Engine-only tools** (no VCBridge needed): query_tools, function_tools, io_tools, channel_tools, palette_tools — these work for both v4 and v5 already
- **VC tools** (VCBridge-dependent): vc_create_tools, vc_update_tools, vc_input_tools, vc_layout_tools, prompts — these need VCBridgeV4

### Key v4 VirtualConsole Facts (from code analysis)

1. v4 `VirtualConsole` is a **QWidget** (not QMainWindow), accessed via **singleton** `VirtualConsole::instance()`
2. v4 already has an **integer ID-based widget map**: `QHash<quint32, VCWidget*> m_widgetsMap` with `widget(quint32 id)` and `newWidgetId()`
3. v4 widgets inherit `QWidget` (vs `QObject` in v5) but share the same `quint32 id` system
4. v4 has per-widget `page()` / `setPage()` — pages are a property on widgets, NOT separate container objects like v5's `VCPage`
5. v4 `FunctionManager` is a QWidget singleton (`ui/src/functionmanager.h`)
6. v4 uses `App` (QMainWindow) as the top-level application, no `QCommandLineParser` — uses custom `QLCArgs` parsing

### Page Model Mismatch (Critical Design Decision)

This is the hardest semantic gap:

| Aspect | v4 | v5 |
|--------|----|----|
| Page container | No container — widgets have `m_page` int | Real `VCPage` QObject |
| Page creation | Implicit (set page number on widgets) | Explicit `addPage()` creates VCPage |
| Page naming | Frame captions or tab labels | `VCPage::name()` property |
| Page count | Derived from max widget page number | `VirtualConsole::pagesCount()` |
| Root frame | Single `VCFrame *m_contents` | One root frame per page |

**Proposed solution:** VCBridgeV4 treats v4's top-level VCFrame children as "pages" using their page property. `addPage()` creates a new top-level frame with `setPage(N)`. `findPageByName()` searches frame captions. This is a compatibility mapping, not a perfect match.

### Header Name Collision (Build System Design)

Both v4 and v5 have identically-named headers (`virtualconsole.h`, `vcwidget.h`, `vcframe.h`, etc.) in different directories. If both include paths are visible, the wrong header could be picked up.

**Solution:** Compile VCBridgeV4 and VCBridgeV5 as separate object libraries with isolated include paths. Only one is linked per build variant.

---

## Implementation Plan

### Phase 0: Verify v4 Baseline (engine + audio + DDP)

**Goal:** Confirm v4 builds clean and audio/DDP features work without MCP.

- [ ] Build v4: `cmake .. -Dqmlui=OFF` and `cmake --build . -j8`
- [ ] Verify DDP plugin compiles and loads
- [ ] Verify engine audio changes (beattracker, audiocapture, rgbscriptv4) compile
- [ ] Test audio RGB scripts work in v4 RGB Matrix
- [ ] Fix any `#ifdef QMLUI` issues in engine that break v4 audio paths

### Phase 1: MCP Engine-Only Tools for v4

**Goal:** Get MCP server running in v4 with engine-only tools (no VC operations).

#### 1a. v4 Entry Point Integration

**Files to modify:**
- `main/main.cpp` — Add `#ifdef HAS_MCP_SERVER` block to call `mcpInit()`
- `main/CMakeLists.txt` — Conditionally link `qlcplusmcp` when `-Dmcp_server=ON`

**Challenge:** v4 uses custom `QLCArgs` parsing, not `QCommandLineParser`. Options:
- Option A: Add `--mcp-port` and `--no-mcp` to `QLCArgs` struct
- Option B: Create a v4-specific `mcpInitV4()` that takes simpler args
- **Recommended:** Option A — minimal change, consistent interface

**Challenge:** v4 doesn't have a `functionManager()` accessor on App.
- v4 `FunctionManager` is created as a tab widget inside `App::initDoc()` — need to either store a reference or pass `nullptr` (MCP function tools can find functions via Doc directly).

#### 1b. Conditional MCP Build for v4

**Files to modify:**
- `CMakeLists.txt` (top-level) — Allow `mcp_server` option regardless of `qmlui` flag
- `mcp/CMakeLists.txt` — Conditional include paths and source files

**CMake approach:**
```cmake
# mcp/CMakeLists.txt
if(qmlui)
    target_sources(qlcplusmcp PRIVATE vcbridgev5.cpp)
    target_include_directories(qlcplusmcp PRIVATE ${CMAKE_SOURCE_DIR}/qmlui/virtualconsole)
else()
    target_sources(qlcplusmcp PRIVATE vcbridgev4.cpp)
    target_include_directories(qlcplusmcp PRIVATE ${CMAKE_SOURCE_DIR}/ui/src/virtualconsole ${CMAKE_SOURCE_DIR}/ui/src)
endif()
```

#### 1c. mcpinit.cpp Conditional Bridge

**File to modify:** `mcp/mcpinit.cpp`

```cpp
#ifdef QMLUI
    #include "vcbridgev5.h"
#else
    #include "vcbridgev4.h"
#endif

void mcpInit(Doc *doc, void *vc, void *funcMgr, ...) {
    VCBridge *vcBridge = nullptr;
    if (vc) {
#ifdef QMLUI
        vcBridge = new VCBridgeV5(doc, static_cast<QmlVirtualConsole*>(vc));
#else
        vcBridge = new VCBridgeV4(doc, static_cast<WidgetVirtualConsole*>(vc));
#endif
    }
    McpServer *server = new McpServer(doc, vcBridge, ...);
    server->startHttp(port);
}
```

**Milestone:** MCP server starts in v4, engine-only tools (query fixtures, create functions, configure universes) work.

### Phase 2: VCBridgeCommon + VCBridgeV4 Implementation

**Goal:** Extract shared logic into a common base class, then implement the thin v4 adapter.

**Key finding from API analysis:** v4 and v5 VCWidget share ~40+ identical methods (caption, colors, fonts, geometry, IDs, page numbers, disable state, intensity). The main incompatibility is input source handling (single-source v4 vs multi-source v5) and some method name differences on specific widgets (e.g., `setFunction()` vs `setFunctionID()` on VCButton).

#### 2a. Create VCBridgeCommon (Extract from VCBridgeV5)

Refactor VCBridgeV5 by extracting ~60-70% of its code into a shared base class:

**Goes into VCBridgeCommon (shared, ~1400 lines):**
- Page management (page lookup, naming, counting)
- Widget lookup by ID/caption (both use `quint32` ID maps)
- Geometry operations (setWidgetGeometry, nextWidgetPosition, nextWidgetPositionFlow)
- Appearance (setWidgetCaption, setWidgetColors, setWidgetFont, setWidgetBackgroundImage, setWidgetDisableState)
- Layout snapshots and reflow (snapshotPage, snapshotFrame, applyLayoutPlan)
- Feedback value reading/writing (QLCInputSource feedback API is identical)
- Common widget detail serialization

**Abstract hooks in VCBridgeCommon (implemented per-version):**
```cpp
protected:
    // Widget access — both versions have widget(id) maps
    virtual VCWidgetBase* findWidgetById(quint32 id) = 0;
    virtual QList<VCWidgetBase*> allWidgets() = 0;
    
    // Widget creation — fundamentally different factory patterns
    virtual int createButton(int parentID, const QRect &geo) = 0;
    virtual int createSlider(int parentID, const QRect &geo) = 0;
    virtual int createFrame(int parentID, const QRect &geo, bool solo) = 0;
    // ... one per widget type
    
    // Input mapping — architecturally incompatible between v4/v5
    virtual bool doMapInput(int widgetID, quint32 universe, quint32 channel) = 0;
    virtual bool doAddInput(int widgetID, quint32 universe, quint32 channel) = 0;
    
    // Type-specific configuration — method names differ
    virtual bool doConfigureButton(int widgetID, quint32 funcID, const QString &action) = 0;
    virtual bool doConfigureSlider(int widgetID, const QString &mode, quint32 funcID) = 0;
```

**Stays in VCBridgeV5 (thin, ~600 lines):**
- Widget creation (QML component instantiation via `m_vc->createWidget()`)
- Input source management (v5 multi-source API: `addInputSource()`, `deleteInputSurce()`)
- Method name mappings (v5 names: `setFunctionID()`, `setActionType()`, `setControlledFunction()`)
- QML-specific operations (`QQmlEngine` calls)

**New: VCBridgeV4 (thin, ~600 lines):**
- Widget creation (v4 factory: `new VCButton(parent, doc)`)
- Input source management (v4 single-source API: `setInputSource()`)
- Method name mappings (v4 names: `setFunction()`, `setAction()`, `setPlaybackFunction()`)
- Qt Widget-specific operations

#### 2b. Design Decisions

1. **Widget IDs:** Use v4's existing `quint32` ID system (`m_widgetsMap`, `newWidgetId()`). This matches v5's approach — both use integer IDs. ✅ Great alignment.

2. **Page model:** Map v4's flat page-number system to VCBridge page semantics:
   - `addPage(name)` → create top-level VCFrame with `setPage(nextPageNum)`, caption = name
   - `pages()` → enumerate distinct page numbers from widget tree, return PageInfo with frame caption as name
   - `pagesCount()` → max page number + 1
   - `findPageByName(name)` → search top-level frame captions

3. **Widget creation:** Use v4's existing factory methods. v4 widgets are `QWidget`-derived, created with `new VCButton(parent, doc)` etc. Must add to `m_widgetsMap` after creation.

4. **Thread safety:** Same pattern as v5 — `execOnMainThread()` wrapper to post lambdas to the GUI thread.

5. **Method name adapters:** Create thin inline wrappers where v4/v5 APIs differ only in naming:
   - `setFunction()` ↔ `setFunctionID()` (VCButton)
   - `setAction()` ↔ `setActionType()` (VCButton)
   - `setPlaybackFunction()` ↔ `setControlledFunction()` (VCSlider)
   - `setLevelLowLimit()` ↔ `setRangeLowLimit()` (VCSlider)

#### 2c. Incremental Implementation Order

Implement VCBridgeV4 methods in priority order:

**Tier 1 — Core (most MCP workflows depend on these):**
- `pages()`, `pagesCount()`, `addPage()`, `findPageByName()`
- `addFrame()`, `addFrameInFrame()`
- `addButton()`, `setButtonFunction()`, `setButtonAction()`
- `addSlider()`, `setSliderMode()`, `setSliderChannels()`
- `getWidgetDetails()` — query widget state
- `setWidgetCaption()`, `setWidgetColors()`
- `setWidgetGeometry()`, `nextWidgetPosition()`

**Tier 2 — Extended widgets:**
- `addXYPad()`, `addXYPadEx()`, XY pad configuration
- `addCueList()`, `configureCueList()`
- `addLabel()`
- `addSpeedDial()`, `configureSpeedDial()`
- `addClock()`, `configureClock()`
- `addAudioTriggers()`, audio trigger configuration
- `addMatrix()`, `configureMatrix()`

**Tier 3 — Input/Feedback/Layout:**
- `mapWidgetInput()`, `addWidgetInput()`, `removeWidgetInput()`
- `setWidgetFeedback()`, `getWidgetFeedback()`
- `snapshotPage()`, `snapshotFrame()`
- `reflowPage()`, `applyLayoutPlan()`
- `reparentWidget()`, `removeWidget()`

#### 2d. New Files

- `mcp/vcbridgecommon.h` (~150 lines) — shared base class header
- `mcp/vcbridgecommon.cpp` (~1400 lines) — shared implementation (extracted from VCBridgeV5)
- `mcp/vcbridgev4.h` (~100 lines) — v4 adapter header
- `mcp/vcbridgev4.cpp` (~600 lines) — v4 adapter implementation

**Modified:**
- `mcp/vcbridgev5.h` — change base class to VCBridgeCommon
- `mcp/vcbridgev5.cpp` — remove code moved to VCBridgeCommon (~1200 lines removed, ~600 remain)

#### 2e. Shared Logic in VCBridgeCommon

These move from VCBridgeV5 into the shared base:
- Layout algorithms (already in VCBridge base as static methods)
- Source name resolution / input mapping validation
- Widget detail serialization (common fields)
- Reflow/grid-snap math
- Feedback value read/write (QLCInputSource/QLCInputFeedback API is identical)
- Widget property getters (caption, colors, geometry — same API in both)

### Phase 3: Test-Driven Transition Strategy

The testing strategy is designed so that **tests are written FIRST and run CONTINUOUSLY** throughout the transition. Every phase gate is verified by tests before moving on.

#### Testing Architecture Overview

```
┌────────────────────────────────────────────────────────────────┐
│                        TEST PYRAMID                            │
│                                                                │
│  Level 4: E2E (Python, tests/mcp/test_e2e.py)                │
│    Real v4 or v5 binary, headless, HTTP JSON-RPC              │
│    ~52 tests, parameterized for both builds                    │
│    Catches: protocol issues, real UI integration bugs          │
│                                                                │
│  Level 3: Tool Integration (QtTest + MockVCBridge)             │
│    MCP tools → MockVCBridge → verify state                    │
│    ~25 tests, NO UI dependency, runs in both builds            │
│    Catches: tool logic errors, wrong bridge calls              │
│                                                                │
│  Level 2: Contract Tests (QtTest, real VCBridgeV4/V5)         │
│    Same test logic → run against BOTH bridges                  │
│    ~20 tests, verifies behavioral equivalence                  │
│    Catches: v4/v5 bridge semantic drift                        │
│                                                                │
│  Level 1: Unit Tests (existing, 6 test suites)                │
│    Struct defaults, validation, query filtering, palettes      │
│    ~80 tests, no UI dependency                                 │
│    Catches: data structure regressions                         │
└────────────────────────────────────────────────────────────────┘
```

#### 3a. FIRST: MockVCBridge (Write BEFORE any refactoring)

**Why first:** MockVCBridge lets us test ALL MCP tools without any UI. This creates a safety net BEFORE we start refactoring VCBridgeV5 or creating VCBridgeV4.

**New file:** `mcp/test/mock_vcbridge.h` (~400 lines)

```cpp
class MockVCBridge : public VCBridge {
    struct MockWidget {
        int id;
        QString type, caption;
        QRect geometry;
        quint32 functionID = Function::invalidId();
        QColor bgColor, fgColor;
        int parentID = -1;
        int pageIndex = -1;
        QMap<QString, QVariant> properties; // extensible config store
        QList<QPair<quint32, quint32>> inputSources; // universe, channel
        FeedbackInfo feedback;
    };
    
    struct MockPage {
        int index;
        QString name;
        QList<int> widgetIDs;
    };
    
    QMap<int, MockWidget> m_widgets;
    QList<MockPage> m_pages;
    int m_nextId = 1;
    
    // Records of all operations (for verification)
    QStringList m_callLog;
    
public:
    // All 69 virtual methods implemented with in-memory state
    int addPage(const QString &name) override { ... }
    int addButton(int parentID, const QRect &geo, ...) override { ... }
    WidgetDetails getWidgetDetails(int id) const override { ... }
    // ...
    
    // Test helpers
    const QStringList &callLog() const { return m_callLog; }
    int widgetCount() const { return m_widgets.size(); }
    MockWidget widgetData(int id) const { return m_widgets.value(id); }
    void reset() { m_widgets.clear(); m_pages.clear(); m_nextId = 1; m_callLog.clear(); }
};
```

**What this enables:** Every MCP tool can be tested by calling the tool handler, passing a MockVCBridge, and verifying the mock's state — no Qt Widgets, no QML, no display server needed.

#### 3b. Tool Integration Tests (Write BEFORE refactoring)

**New file:** `mcp/test/tool_integration_test.h` + `.cpp` (~800 lines)

These test the actual MCP tool handlers end-to-end through MockVCBridge:

```cpp
class ToolIntegration_Test : public QObject {
    Q_OBJECT
    
    Doc *m_doc;
    MockVCBridge *m_bridge;
    fastmcpp::tools::ToolManager *m_toolManager;
    
private slots:
    void initTestCase() {
        m_doc = new Doc(this);
        m_bridge = new MockVCBridge();
        m_toolManager = new fastmcpp::tools::ToolManager();
        // Register ALL tool providers — same as real McpServer
        registerVCCreateTools(*m_toolManager, m_doc, m_bridge);
        registerVCUpdateTools(*m_toolManager, m_doc, m_bridge);
        registerVCInputTools(*m_toolManager, m_doc, m_bridge);
        registerVCLayoutTools(*m_toolManager, m_doc, m_bridge);
        registerQueryTools(*m_toolManager, m_doc, m_bridge);
        registerFunctionTools(*m_toolManager, m_doc, nullptr);
        registerIOTools(*m_toolManager, m_doc);
        registerChannelTools(*m_toolManager, m_doc);
        registerPaletteTools(*m_toolManager, m_doc);
    }
    
    // --- CREATE TOOLS ---
    void test_createPage();          // create_vc_page → verify page exists
    void test_createButton();        // create_vc_button → verify widget, function, action
    void test_createSlider();        // create_vc_slider → verify mode, channels
    void test_createXYPad();         // create_vc_xypad → verify fixtures
    void test_createCueList();       // create_vc_cuelist → verify chaser binding
    void test_createFrame();         // create_vc_frame → verify nesting
    void test_createLabel();         // create_vc_label → verify text
    
    // --- UPDATE TOOLS ---
    void test_updateWidgetCaption(); // update_vc_widget → verify caption changed
    void test_updateWidgetColors();  // update_vc_widget → verify colors
    void test_updateButtonAction();  // update_vc_button → verify action changed
    void test_updateSliderMode();    // update_vc_slider → verify mode changed
    
    // --- INPUT TOOLS ---
    void test_mapInput();            // map_vc_input → verify input source
    void test_setFeedback();         // set_vc_feedback → verify feedback values
    
    // --- LAYOUT TOOLS ---
    void test_queryLayout();         // query_vc_layout → verify JSON output
    void test_reflowPage();          // reflow_vc_page → verify widget positions
    
    // --- IDEMPOTENCY ---
    void test_createButtonIdempotent(); // create same button twice → only 1 widget
    void test_createPageIdempotent();   // create same page twice → only 1 page
    
    // --- ERROR CASES ---
    void test_createButtonNoPage();     // no page → error
    void test_updateNonexistent();      // bad widget ID → error
    void test_invalidWidgetType();      // bad type string → error
    
    // --- ROUND-TRIP ---
    void test_createThenQuery();     // create widgets → query_vc_widgets → verify all present
    void test_fullLayoutWorkflow();  // create page → add widgets → reflow → verify positions
};
```

**When to run:** After EVERY code change during the transition. These tests catch tool-level regressions immediately.

#### 3c. Contract Tests (Write DURING Phase 2)

**Goal:** Verify VCBridgeV4 and VCBridgeV5 produce identical results for the same operations.

**Files:**
```
mcp/test/vcbridge_contract_test.h    — shared test declarations
mcp/test/vcbridge_contract_test.cpp  — shared test logic using VCBridge* pointer

# Compiled conditionally per build:
mcp/test/vcbridge_contract_v4.cpp    — v4 build: creates VCBridgeV4
mcp/test/vcbridge_contract_v5.cpp    — v5 build: creates VCBridgeV5
```

**Contract test suite** (~20 test functions):

```cpp
class VCBridgeContract_Test : public QObject {
    Q_OBJECT
    
    VCBridge *m_bridge;  // Set by v4 or v5 setup
    Doc *m_doc;
    
private slots:
    // PAGE OPERATIONS
    void addPage_returnsValidIndex();
    void addPage_appearsInPages();
    void findPageByName_findsCreatedPage();
    void renamePage_updatesName();
    void pagesCount_matchesAdded();
    
    // WIDGET CREATION
    void addButton_returnsValidId();
    void addButton_detailsMatchArgs();
    void addSlider_returnsValidId();
    void addFrame_supportsNesting();
    void addXYPad_returnsValidId();
    void addCueList_bindsChaserFunction();
    
    // WIDGET PROPERTIES
    void setCaption_reflectedInDetails();
    void setColors_reflectedInDetails();
    void setGeometry_reflectedInDetails();
    void setDisableState_reflectedInDetails();
    
    // WIDGET IDS
    void widgetIds_areUnique();
    void widgetIds_stableAfterMutation();
    
    // INPUT MAPPING
    void mapInput_roundTrips();
    void feedback_roundTrips();
    
    // LAYOUT
    void nextWidgetPosition_noOverlap();
    void snapshotPage_includesAllWidgets();
};
```

**How it catches drift:** If VCBridgeV4 returns different widget details, different IDs, or different page semantics than VCBridgeV5, the contract test fails. The same test code runs against both implementations.

#### 3d. Transition Test Gates

Each phase has explicit test gates that must pass before proceeding:

| Phase | Gate | Tests That Must Pass | What They Verify |
|-------|------|---------------------|-----------------|
| **Before refactoring** | Gate 0 | All 6 existing MCP tests + new tool integration tests | Current behavior captured |
| **Phase 0 (v4 baseline)** | Gate 1 | v4 engine tests pass, audio tests pass | Engine changes don't break v4 |
| **Phase 1 (v4 bootstrap)** | Gate 2 | MCP server starts in v4, engine-only tools respond | Basic v4 MCP connectivity |
| **Phase 2a (VCBridgeCommon)** | Gate 3 | All tool integration tests still pass with V5 | Refactoring didn't break V5 |
| **Phase 2b (VCBridgeV4 core)** | Gate 4 | Contract tests pass for V4 (Tier 1 methods) | Core V4 bridge works |
| **Phase 2c (VCBridgeV4 extended)** | Gate 5 | Contract tests pass for V4 (all tiers) | Full V4 bridge works |
| **Final** | Gate 6 | E2E tests pass for BOTH v4 and v5 builds | End-to-end confidence |

#### 3e. Parameterized E2E Tests

**Modified file:** `tests/mcp/test_e2e.py`

The existing 52 E2E tests already work against v5. Add parameterization to run against both:

```python
import os, subprocess, pytest

V5_BIN = os.environ.get("QLCPLUS_V5_BIN", "./build/qmlui/qlcplus-qml")
V4_BIN = os.environ.get("QLCPLUS_V4_BIN", "./build/main/qlcplus")

@pytest.fixture(params=[
    pytest.param(V5_BIN, id="v5-qml"),
    pytest.param(V4_BIN, id="v4-widgets"),
], scope="session")
def mcp_server(request):
    binary = request.param
    if not os.path.exists(binary):
        pytest.skip(f"{binary} not found")
    proc = subprocess.Popen(
        [binary, "--mcp-port", "19876"],
        env={**os.environ, "QT_QPA_PLATFORM": "offscreen"}
    )
    yield proc
    proc.terminate()
```

**Result:** Same 52 tests run against both builds. Any behavioral difference is caught.

#### 3f. Audio-Reactive Script Tests

**Existing file:** `tests/test_audio_scripts.js` (Node.js, 174 lines)

Already tests all 22 audio scripts with simulated data. Runs without QLC+ binary — pure JavaScript validation. Works for both v4 and v5 since scripts are UI-independent.

**Add to CI:** Run as part of every build variant.

#### 3g. Cross-Load Workspace Tests

**New file:** `mcp/test/workspace_compat_test.h` + `.cpp` (~200 lines)

```cpp
class WorkspaceCompat_Test : public QObject {
    Q_OBJECT
private slots:
    void createViaV5_loadInV4();  // Create workspace with MCP → save XML → load in Doc with v4 VC
    void roundTrip_preservesWidgets();  // Save → load → verify same widget count/types/positions
    void pageSemantics_roundTrip();     // Verify page names survive save/load across versions
};
```

#### 3h. Continuous Test Execution During Transition

**Recommended workflow for each code change:**

```bash
# 1. Quick feedback (< 5 seconds) — run unit + mock tests
cmake --build build --target mcp_tool_integration_test -j8 && ./mcp/test/mcp_tool_integration_test

# 2. Contract check (< 10 seconds) — verify bridge behavior
cmake --build build --target mcp_vcbridge_contract_test -j8 && ./mcp/test/mcp_vcbridge_contract_test

# 3. Full regression (< 60 seconds) — all MCP tests
cmake --build build --target check

# 4. E2E validation (< 120 seconds) — real binary tests
cd tests/mcp && python3 -m pytest test_e2e.py -v
```

#### 3i. Test Summary

| Test Suite | Files | Tests | UI Needed? | When to Run | What It Catches |
|------------|-------|-------|------------|-------------|----------------|
| Existing unit tests | 6 files | ~80 | No | Every commit | Data structure regressions |
| **MockVCBridge tool tests** | 2 files | ~25 | **No** | Every commit | Tool logic errors |
| **Contract tests** | 3 files | ~20 | Yes (headless) | Per-phase gate | V4/V5 behavioral drift |
| **E2E tests** | 1 file | ~52 × 2 builds | Yes (headless) | CI + phase gates | Integration/protocol bugs |
| Audio script tests | 1 file | ~22 | No | Every commit | Script execution errors |
| **Workspace compat** | 2 files | ~3 | Yes (headless) | Phase gates | Save/load compatibility |

**Total new test code:** ~1400 lines across 8 new files
**Total new test cases:** ~70 new tests + 52 parameterized E2E

### Phase 4: CI & Merge-Friendliness

#### 4a. CI Changes

**File:** `.github/workflows/build.yml`
- Add `build-v4-mcp` job: `cmake -Dmcp_server=ON` (without `-Dqmlui=ON`)
- Enable the currently disabled CI (`if: false`)
- Run engine tests + MCP tests for all variants

#### 4b. Merge-Friendliness Checklist

| Change Type | Conflict Risk | Mitigation |
|-------------|--------------|------------|
| `mcp/` (all new files) | **None** | New directory, no upstream equivalent |
| `plugins/ddp/` (all new files) | **None** | New directory |
| `resources/rgbscripts/audio*.js` (all new) | **None** | New files |
| `engine/src/*.cpp` modifications | **Medium** | Keep changes minimal, well-commented |
| `CMakeLists.txt` (top-level) | **Medium** | Use additive patterns (new options, new subdirectories) |
| `main/main.cpp` | **Low** | Only adds `#ifdef HAS_MCP_SERVER` block |
| `.github/workflows/build.yml` | **Low** | CI config is fork-specific anyway |

#### 4c. Guidelines for Staying Mergeable

1. **Never modify upstream files unnecessarily** — prefer new files over modifying existing ones
2. **Use `#ifdef HAS_MCP_SERVER`** guards in entry points only (`main/main.cpp`, `qmlui/main.cpp`)
3. **Keep engine changes surgical** — each engine modification should be a self-contained logical change
4. **Prefer additive CMake** — use `if(mcp_server)` blocks, don't restructure existing targets
5. **Rebase regularly** — stay up-to-date with upstream to catch conflicts early
6. **Separate commits** — keep MCP-specific changes in dedicated commits, not mixed with other work

---

## Effort Summary

| Feature | Effort | Status | New Code | Modified Files |
|---------|--------|--------|----------|---------------|
| DDP Plugin | 0/10 | ✅ Done | 0 lines | 0 files |
| Input Profiles | 0/10 | ✅ Done | 0 lines | 0 files |
| Audioreactive | 2/10 | ⚠️ Verify | ~0 lines | 0-2 files |
| MCP Phase 0 (baseline) | 1/10 | TODO | 0 lines | 0-2 files |
| MCP Phase 1 (bootstrap) | 3/10 | TODO | ~100 lines | 4-5 files |
| MCP Phase 2 (VCBridgeCommon + V4) | 6/10 | TODO | ~2250 lines new, ~1200 refactored | 4 new + 2 modified |
| MCP Phase 3 (tests) | 5/10 | TODO | ~1400 lines | 8 new files |
| MCP Phase 4 (CI) | 1/10 | TODO | ~50 lines | 1 file |

**Total new code:** ~3800 lines (including ~1400 lines of tests)
**Net new production code:** ~2400 lines (since ~1200 is refactored from existing VCBridgeV5)
**Total new test cases:** ~70 new C++ tests + 52 E2E tests parameterized for both builds
**Total modified files:** ~8-10 existing files
**Overall difficulty:** 6-7/10 (reduced from 8/10 by maximizing shared code)

## Risk Register

| Risk | Severity | Likelihood | Mitigation |
|------|----------|------------|------------|
| Page model mismatch causes tool failures | High | High | Define compatibility mapping upfront, test thoroughly |
| VCBridgeV4 drifts from V5 behavior | High | ~~Medium~~ Low | VCBridgeCommon shared base eliminates ~70% of duplication |
| Engine changes break v4 build | Medium | Low | Phase 0 verification, CI for both variants |
| Workspace files don't round-trip | Medium | Medium | Cross-load tests |
| Header name collisions in CMake | High | Medium | Separate object libraries per bridge |
| Merge conflicts with upstream | Medium | Medium | Rebase regularly, keep changes additive |
| v4 FunctionManager access pattern | Low | Medium | Pass nullptr, use Doc-based lookup |
