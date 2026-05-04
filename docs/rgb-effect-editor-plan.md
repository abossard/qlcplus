# RGB Script and Node-Based Effect Editor Plan for QLC+ v5

> **Status: PLAN ONLY — not yet implementing.** The node editor will be a ReactFlow
> web app inside `vc-next`. The QML prototype has been removed from the codebase.
> Sections marked "Historical" below are kept as design reference only.

## Architecture Decision: ReactFlow Web App (decided 2026-05-03)

### Decision

The node editor will be a **ReactFlow-based React view inside the existing `vc-next` SPA**, not a standalone QML application. The app route should live at `/vc/#/nodes` and integrate with the current webaccess frontend and QLC+ webaccess server.

### Why ReactFlow

- **Existing frontend fit**: `vc-next` is already React + Vite + TypeScript + Zustand, so ReactFlow drops straight into the active web stack.
- **Browser interaction model**: touch, pan, and zoom work natively in browsers; the QML prototype exposed touchpad and gesture bugs.
- **Simpler node definitions**: node definitions can be authored directly in JS/TS with no C++↔JS bridge.
- **Mature graph library**: ReactFlow provides custom nodes, typed edges, minimap, controls, viewport handling, and established interaction patterns.

### Architecture

```text
Browser (vc-next SPA at /vc/#/nodes)
  ├── ReactFlow canvas + custom nodes
  ├── Palette sidebar (TS node defs)
  ├── Compiler (TS — graph→JS code generation)
  ├── Preview (Web Worker + Canvas)
  └── Zustand store (graph state, undo)
        │ HTTP/JSON
        ▼
QLC+ webaccess server (port 9999)
  ├── GET/POST /api/scripts/:name  → user scripts dir
  └── GET/POST /api/graphs/:name   → graph JSON alongside
```

### File Structure

Target layout under `webaccess/vc-next/src/`:

```text
views/NodeEditorView.tsx
components/nodeeditor/
  ├── NodeCanvas.tsx, NodePalette.tsx, NodeInspector.tsx
  ├── PreviewPanel.tsx
  ├── nodes/DefinedNode.tsx
  └── edges/TypedEdge.tsx
store/nodegraph-store.ts
lib/nodeeditor/
  ├── compiler.ts, validate.ts, schema.ts
  ├── preview.worker.ts
  └── nodetypes/*.ts (17 node definitions)
```

### Keep from the C++ / QML Work

- Compiler concepts and code-generation logic, ported to TypeScript.
- Node vocabulary and node definition metadata.
- Test concepts and fixture-script golden coverage.
- Validation rules for graph structure, typed sockets, stable properties, output node constraints, and cycle prevention.

### Discard from the C++ / QML Work

- All QML node editor files after React reaches parity.
- `GraphBridge` and related C++↔QML bridge code.
- `PreviewRuntime` as a QML/C++ editor concept; preview moves to a browser worker/canvas path.
- C++ node subclasses; node behavior lives in TypeScript definitions and compiler code.

### Playwright E2E Test Coverage

Add 10 Playwright spec files covering the complete editor workflow:

1. `node-editor-routing.spec.ts` — route loads, layout renders, deep links work.
2. `node-palette.spec.ts` — search, categories, drag/drop, keyboard add.
3. `node-canvas.spec.ts` — pan, zoom, selection, multi-select, fit view.
4. `node-connections.spec.ts` — typed sockets, invalid edge rejection, reconnect.
5. `node-inspector.spec.ts` — property editing, validation messages, defaults.
6. `node-compiler.spec.ts` — compile success/failure, generated script metadata.
7. `node-preview.spec.ts` — worker preview, resize, frame stepping, timeout handling.
8. `node-persistence.spec.ts` — load/save scripts and graph JSON through webaccess APIs.
9. `node-undo-redo.spec.ts` — transaction boundaries, drag coalescing, property undo.
10. `node-security.spec.ts` — auth-required writes, path traversal rejection, sanitized names.

### Blockers from GPT-5.5 Review

- **Security**: write endpoints need SUPER_ADMIN authorization and strict path sanitization before saving scripts or graph JSON.
- **Audio**: browser FFT is for preview only; runtime scripts must still use native QLC+ audio data.
- **Compiler port**: use semantic golden tests, including pixel comparison, instead of string comparison of generated JS.
- **Staged migration**: QML prototype has been removed. No fallback exists — ReactFlow is the only path.
- **Undo/redo**: define explicit transaction boundaries and coalesce drag move events into a single undo entry.
- **Worker eval**: preview worker execution needs timeout enforcement and worker termination on runaway scripts.

### Migration Sequence

1. Add view switching to `vc-next`: either a simple `useState`-based tab switcher in `App.tsx` (DMX | Console | Nodes) or install `react-router-dom` for hash routing. `vc-next` currently has NO router — navigation is hardcoded. Create empty `views/NodeEditorView.tsx` shell.
2. Install `@xyflow/react`. Add ReactFlow canvas and the base Zustand graph store (`store/nodegraph-store.ts`).
3. Port the node schema, socket types, validation rules, and 17 node definitions to TypeScript.
4. Implement palette, canvas, typed custom nodes, typed edges, and inspector editing.
5. Implement the graph→JS compiler in TypeScript based on the spec in the "Compiler Design Notes" section below. Validate with semantic golden tests (compile graph → eval JS → assert pixel output). The C++ prototype was never committed — implement fresh from the documented contract and the 62 existing RGB scripts as reference.
6. Implement preview in a Web Worker + Canvas path with timeout and termination safeguards.
7. Add webaccess REST endpoints for script and graph load/save with SUPER_ADMIN auth and path sanitization.
8. Wire save/load from the React editor to `/api/scripts/:name` and `/api/graphs/:name`.
9. Add the 10 Playwright E2E specs and required unit/golden tests.
10. Run full Playwright E2E suite. Verify generated scripts load and run in QLC+ engine. Ship.

### Status of the QML Node Editor

The QML node editor prototype has been **removed from the codebase** (was never committed to git). The design research, compiler spec, node vocabulary, and validation rules documented below remain valid and should be used as the spec for the ReactFlow implementation.

---

## Key Design Decisions

- **User NEVER sees code.** The node editor is the only authoring interface.
- **Engine unchanged.** Output is a standard `.js` RGB script. The engine doesn't know about nodes.
- **Graph is source of truth.** JS is generated build output, never hand-edited.
- **Fullscreen editor.** Easy to use, Blender Geometry Nodes-style UX (not TouchDesigner complexity).
- **Existing 62 scripts untouched.** Node-generated scripts live alongside them. No migration.

## Phase 0 Research — Validated Architecture

```
Node Graph (.nodeGraph.json — source of truth)
    ↓ compiler (topological sort + code generation)
Standard .js RGB Script (generated output — never shown to user)
    ↓ loaded by
Existing RGBScriptV4 engine (ZERO changes)
    ↓ produces
2D RGB pixel map → RGB Matrix → fixtures
```

**Feasibility: CONFIRMED.** The compiler can emit valid API v3 scripts with metadata, properties,
colors, audio, and persistent state — all within the existing IIFE contract.

## Blockers (must resolve before implementation)

### B1: Engine Isolation (BLOCKING — Feature D preview)
`RGBScript` uses a **static singleton `QScriptEngine`** (`rgbscript.cpp:40`). Preview and live
matrix share JS state. Editing preview can corrupt running matrix output.

**Fix**: `RGBScriptPreviewRuntime` MUST own its own `QScriptEngine` instance.

### B2: RGBW Preview (BLOCKING — Feature D preview)
Preview produces RGB-only. With RGBW control modes, preview won't match fixture output.

**Fix**: Apply same RGB→RGBW conversion in preview, or warn "RGBW preview not available."

### B3: Property Name Stability (BLOCKING — node editor)
Properties are persisted in project XML by name. If graph recompile changes property names
(e.g., `param_3` instead of `speed`), saved shows break silently.

**Fix**: Parameter nodes must carry stable, user-authored IDs compiled 1:1 to property names.
Compiler must refuse to rename or reorder without explicit migration.

### B4: Step Count Semantics (BLOCKING — node editor)
`rgbMapStepCount` doesn't compose. If source A has 10 steps and source B has 7, using max=10
means B receives step values it never expected. LCM explodes. Audio returning 1 freezes
non-audio branches.

**Fix**: Define precisely before implementation. Recommended: nodes receive normalized phase
`t ∈ [0,1)` derived from their declared period. Output node chooses final step count.

### B5: Per-Node State Isolation (BLOCKING — node editor)
State lives in IIFE closure. Two instances of the same node type share closure = collision.

**Fix**: Compiler generates per-node-instance state objects keyed by node UUID.

## Node Vocabulary (Phase 0 Assessment)

Current vocabulary covers ~50% of existing scripts, not 80%. Critical gaps:

### Must-Have Nodes (before MVP)

| Node | Why | Used by |
|------|-----|---------|
| **Previous Frame** | ~15 scripts read prior frame for trails/diffusion | audiofire, audiowater, fill variants |
| **State Buffer** | First-class persistence primitive (not mega-nodes) | particles, accumulation, filters |
| **Time/Step** | Phase/time source for modulation | nearly everything |
| **Coordinates (x, y)** | Normalized UV for generative patterns | sine, plasma, circles |
| **Math (binary)** | Add, Multiply, Clamp, Lerp, Modulo | composability |
| **Math (unary)** | Sin, Cos, Abs, Floor, Sqrt | math-based effects |
| **HSV ↔ RGB** | Hue rotation, rainbow effects | common in existing scripts |
| **Polar Coordinates** | r, θ from x,y — trivializes circular effects | circles, tunnel, vortex |
| **Random (seeded)** | Deterministic random with explicit reseed | noise, sparkle |
| **Select/Conditional** | `cond ? a : b` as a node | conditional patterns |
| **Color Input** | Exposes `acceptColors` / API v3 colors | any user-colored effect |
| **Audio Source** | spectrum, volume, beat, bpm, maxMagnitude | 22 audio scripts |
| **Beat/Onset** | Separate from FFT spectrum | beat-reactive effects |
| **Parameter** | User-exposed property → compiled to script property string | runtime controls |
| **RGB Map Output** | Final output (exactly one per graph) | always |

### Later Nodes
| Node | Phase |
|------|-------|
| Particle Emitter + Physics + Renderer | After State Buffer works |
| Image/Text source | Nice-to-have |
| Sub-graph / Node Group | Decide now: yes or no. Retrofitting is painful. |

### Nodes That Replace "Mega-Nodes"
Instead of a single "Particle System" node, decompose into:
- **Emitter** (spawn rate, position, velocity range)
- **Physics Step** (gravity, drag, bounce)
- **Renderer** (point, trail, glow)
- **State Buffer** (persistence primitive)

This is how Blender Geometry Nodes handles it — composable, not monolithic.

## Compiler Design Notes

### Output Structure
Generated JS follows exact script contract: IIFE → object with apiVersion, name, author,
rgbMap(), rgbMapStepCount(), properties[], rgbMapSetColors(), rgbMapGetColors().

### Key Rules
- **Inline everything**: Compile graph into single fused per-pixel function body (SSA-style), not separate functions per node. Eliminates call overhead.
- **Clamp at output**: Final RGB values must be clamped `[0,255]` and `|0` to integer. Engine doesn't validate.
- **Sanitize property names**: No spaces, pipes, or non-ASCII in generated property descriptors (engine parser breaks).
- **Pin apiVersion**: v3 only if graph uses Color Input or Audio. Otherwise v2.
- **Per-node state**: Namespaced `__state.nodeUUID = {}` pattern inside IIFE closure.
- **Cycles disallowed**: DAG only. Feedback via explicit "Previous Frame" / "State Buffer" nodes.
- **Select node evaluates both inputs** (pure functional, no short-circuit).
- **Dynamic array sizes**: Decide now — slider-controlled particle count or compile-time constant only?

### Step Count Policy
Each source declares its period. Output node policy options:
- `max(sources)` — simple but can desync
- `LCM(sources)` capped — better but can explode
- Manual override — user sets total steps
- Normalized phase `t ∈ [0,1)` — cleanest, but requires all nodes to accept normalized input

### Graph Storage
- `.nodeGraph.json` — separate file alongside `.js`
- `.js` has marker comment: `// generated from foo.nodeGraph.json — do not edit`
- Hash of generated JS embedded in `.nodeGraph.json` for drift detection
- On open: if JS hash differs → force user to choose "discard JS edits" or "abandon graph"
- Generated scripts hidden from text editor (or shown read-only for debugging/trust)

## UX Principles (from Blender Geometry Nodes, not Blueprints)

- **Fullscreen node canvas** — no cramped side panel
- **Live preview on every node** — cursor-hover shows that node's output
- **Typed sockets with color coding** — Number=green, Color=red, Grid=blue
- **NO execution wires** — pure dataflow only (Blueprints' exec pins confuse non-programmers)
- **Starter graph templates** — empty canvas causes paralysis
- **Inline parameter sliders** on source/parameter nodes — drives live preview
- **Strong defaults** — every node produces visible output when first placed
- **"View Generated Script" button** — read-only, for debugging/trust. Not editable.

## Validation Plan (before coding)

Hand-translate 5 existing scripts into the proposed node vocabulary on paper:
1. `evenodd.js` — simple spatial
2. `sinewave.js` — math-based
3. `fill.js` — accumulative
4. `audiospectrum.js` — audio-reactive
5. `balls.js` — stateful/particle

If any can't be expressed, the vocabulary is incomplete. Fix cheaply before implementation.

## Additional Review Findings (non-blocking)
- **Two debounce paths**: Code edits 300ms; property slider changes immediate (16ms)
- **Script watchdog**: Add 100ms timeout on `rgbMap()` to prevent infinite-loop freeze
- **Perf target**: 32×32 @ 50Hz < 30% CPU. Compile to fused inline body, not per-node functions.
- **Phase 1 is a refactor**: Extracting preview from `RGBMatrixEditor` touches existing code — needs regression tests
- **Sub-graphs**: Decide now whether graphs can reference other graphs. Retrofitting is painful.
- **Coexistence**: Generated `.js` files appear in script dropdown alongside hand-written ones. `.nodeGraph.json` is ignored by engine. If user edits generated JS in text editor, graph desyncs — use hash check.

---

---

# Historical Reference (QML prototype design — superseded by ReactFlow)

> **The sections below document the QML prototype that was built and removed.**
> They contain valid design research (API contracts, node vocabulary, compiler rules,
> validation logic) that should inform the ReactFlow implementation.
> Do NOT build from these sections — use the ReactFlow Architecture Decision above.

## 1. Overview / vision

QLC+ already has a strong RGB Matrix runtime: scripts expose `rgbMap(width, height, rgb, step)` and `rgbMapStepCount(width, height)`, the engine renders a 2D `RGBMap`, and the v5 RGB Matrix editor already has a small live matrix preview driven by the selected fixture group. The missing piece is an authoring workflow: users can choose existing scripts and tweak properties, but they cannot edit scripts or build new effects visually while seeing immediate output.

This plan covers two related features:

- **Feature D — Enhanced RGB Script Editor with Live Preview**: a Pixelblaze-like authoring experience where the user edits script text, changes generated property controls, and sees a live pixel grid preview without starting output on real fixtures.
- **Feature C — Node-Based Visual Effect Editor**: a visual dataflow editor where users build effects from typed nodes, compile the graph to a QLC+ RGB script, preview it with the same preview infrastructure, and save both generated JS and graph metadata.

Recommended product direction:

1. First extract a reusable preview/runtime service from the current RGB Matrix editor.
2. Then add script editing and preview around the existing RGB script API.
3. Finally add the node editor as a higher-level authoring layer that compiles to the same script API.

The shared design keeps the engine rendering path stable and avoids changing fixture output behavior until a user explicitly saves/selects a script.

---

## 2. Current state discovered in the codebase

### 2.1 Current RGB Matrix editor UI

Primary file: `qmlui/qml/fixturesfunctions/RGBMatrixEditor.qml`.

Relevant findings:

- The editor is a `Rectangle` exposed through the global `rgbMatrixEditor` context property.
- Current preview exists at lines 176-182 through `RGBMatrixPreview`:
  - `matrixSize: rgbMatrixEditor.previewSize`
  - `matrixData: rgbMatrixEditor.previewData`
  - capped to roughly one third of the editor height.
- Script/pattern selection is the **Pattern** `CustomComboBox` around lines 202-221:
  - `model: rgbMatrixEditor.algorithms`
  - `currentIndex: rgbMatrixEditor.algorithmIndex`
  - changing selection writes `rgbMatrixEditor.algorithmIndex = currentIndex`
  - it switches the parameter section between text, image, and script components.
- Script-specific controls are generated by `scriptAlgoComponent` around lines 1140-1197.
  - QML creates an empty `GridLayout`.
  - `Component.onCompleted` calls `rgbMatrixEditor.createScriptObjects(scriptAlgoGrid)`.
  - The backend invokes QML helper functions `addLabel`, `addComboBox`, `addSpinBox`, `addDoubleSpinBox`, and `addTextEdit`.
- Property widgets call backend setters:
  - list/string: `setScriptStringProperty(propName, value)`
  - range/integer: `setScriptIntProperty(propName, value)`
  - float: `setScriptFloatProperty(propName, value)`
- Existing color controls support up to `RGBAlgorithmColorDisplayCount` colors, currently displayed as five color buttons.

### 2.2 Current RGB Matrix editor backend

Primary files: `qmlui/rgbmatrixeditor.h` and `qmlui/rgbmatrixeditor.cpp`.

Relevant findings:

- `RGBMatrixEditor` exposes QML properties:
  - `previewSize`, `previewData`
  - `algorithms`, `algorithmIndex`
  - `algoColorsCount`, `algoColors`
  - text/image-specific properties
  - blend/control/rotation/mirror controls.
- `algorithms()` returns `RGBAlgorithm::algorithms(m_doc)`.
- `setAlgorithmIndex()` resolves the selected name through `RGBAlgorithm::algorithm(m_doc, name)`, updates the matrix algorithm, initializes preview data, and emits algorithm/color change signals.
- `createScriptObjects(QQuickItem *parent)` reads `RGBScript::properties()` and dynamically asks QML to create controls based on `RGBScriptProperty::List`, `Range`, `Float`, and `String`.
- Script property setters currently mutate the actual edited `RGBMatrix` through `m_matrix->setProperty(...)` and enqueue Tardis undo actions.
- Preview is timer-driven:
  - `m_previewTimer` runs at `MasterTimer::tick()`.
  - `slotPreviewTimeout()` advances preview elapsed time, calls `m_previewStepHandler->checkNextStep(...)`, then `m_matrix->previewMap(...)`.
  - `previewData` is populated from `m_previewStepHandler->m_map` using the selected fixture group's head positions.
- The preview is already editor-side, but it is coupled to the real `RGBMatrix` object and fixture group.

### 2.3 Current RGB script execution

Primary files: `engine/src/rgbscript.h` and `engine/src/rgbscript.cpp`.

Relevant findings:

- The inspected engine code currently uses `QScriptEngine` with a static engine and recursive mutex. The feature requirements mention QJSEngine; implementation should either confirm the active branch/runtime before coding or introduce an adapter that can support a future QJSEngine migration.
- `RGBScript::load(fileName)` reads script contents, checks syntax, then calls `evaluate()`.
- `evaluate()` requires:
  - `rgbMap`
  - `rgbMapStepCount`
  - positive `apiVersion`
  - for API v2+: `properties`
  - for API v3+: `rgbMapSetColors`, with optional `rgbMapGetColors`.
- `rgbMapStepCount(size)` calls JS `rgbMapStepCount(width, height)`.
- `rgbMap(size, rgb, step, map)` calls JS `rgbMap(width, height, rgb, step)` and converts a nested JS array into `RGBMap`.
- Properties are declared as pipe-delimited strings in the JS `properties` array. Supported parsed types are `list`, `float`, `range`, and `string`.
- `setProperty(name, value)` finds the declared write method and calls it with a string value.

### 2.4 Script examples and API realities

Relevant examples:

- `resources/rgbscripts/gradient.js`
  - API v2 script with list/range properties.
  - Returns a `[height][width]` array of RGB integers.
- `resources/rgbscripts/audiospectrum.js`
  - API v3 script.
  - Declares `usesAudio = true`.
  - Defines `rgbMap(width, height, rgb, step, audio)`, so planning must account for API v3 audio input even though the inspected `RGBScript::rgbMap` currently passes only four arguments.
- `resources/rgbscripts/CMakeLists.txt`
  - Manually lists installed/copied script files.
  - Android copies scripts to package assets.
  - Non-Android dev builds create symlinks into the build directory.

---

## 3. Feature D — Enhanced RGB Script Editor with Live Preview

### 3.1 User experience target

Add an authoring mode reachable from the RGB Matrix editor when a script algorithm is selected:

- Left side: script list and script code editor.
- Right side: live pixel grid preview.
- Bottom or side panel: generated property controls and color controls.
- Status area: syntax/runtime errors, current step, FPS/preview speed, save state.
- Save actions:
  - **Save as user script** for built-in scripts and new scripts.
  - **Save** for already-user-owned scripts.
  - Optional later: export/copy generated script.

Important UX rule: editing preview source must not mutate the running matrix or selected matrix algorithm until the user explicitly applies/saves it.

### 3.2 Proposed UI files

Start by keeping the existing editor stable and adding small reusable components:

- `qmlui/qml/fixturesfunctions/RGBMatrixEditor.qml`
  - Add an **Edit Script** action beside the Pattern row for script algorithms.
  - Replace or augment the current script parameter section with a tab/split panel when edit mode is active.
  - Continue using the current compact editor when edit mode is off.
- New `qmlui/qml/fixturesfunctions/RGBScriptEditorPanel.qml`
  - Owns editor layout: code editor, preview, property controls, error panel, save buttons.
- New `qmlui/qml/fixturesfunctions/RGBScriptPreviewPanel.qml`
  - Reusable preview UI around `RGBMatrixPreview` or a richer Canvas/Grid implementation.
  - Used by both Feature D and Feature C.
- Optional later `qmlui/qml/fixturesfunctions/RGBScriptPropertyPanel.qml`
  - Declarative QML replacement for the current backend-created controls.

### 3.3 Proposed backend files/classes

Prefer adding a separate preview/edit backend rather than expanding `RGBMatrixEditor` indefinitely.

New classes:

- `qmlui/rgbscripteditormodel.h/.cpp`
  - QML-facing model for script source, dirty state, selected script path/name, errors, preview speed, preview size, and properties.
  - Owns a preview-only script instance/session.
  - Exposes save/save-as operations.
- `qmlui/rgbscriptpreviewcontroller.h/.cpp`
  - Converts script output to `QVariantList` preview data.
  - Timer-driven at preview FPS independent of fixture output.
  - Supports arbitrary preview dimensions, not only fixture-group size.
- `engine/src/rgbscriptdocument.h/.cpp` or `engine/src/rgbscriptpreviewruntime.h/.cpp`
  - Engine-side helper to evaluate script contents from a string, not only from a file.
  - Keeps syntax/runtime error details structured for UI display.
  - Avoids mutating `RGBMatrix` until save/apply.

Alternative minimal approach:

- Add `RGBScript::loadContents(name, contents)` and preview methods to existing `RGBScript`.
- Use a cloned `RGBScript` inside `RGBMatrixEditor` for preview.
- This is faster for MVP but risks coupling editor-only state to production script runtime.

Recommended approach: create a small preview runtime wrapper first, then reuse it for both features.

### 3.4 Preview panel design

Current `RGBMatrixPreview` can already render `previewSize` and `previewData`. The enhanced preview should generalize it:

- Inputs:
  - `previewSize: QSize`
  - `previewData: QVariantList` of `QColor`
  - `showGrid: bool`
  - `cellGap: real`
  - `backgroundColor`
  - optional selected/hovered pixel coordinates for debugging.
- Rendering options:
  - MVP: reuse existing `RGBMatrixPreview` if it scales well for 1x10, 10x10, 20x5, and larger grids.
  - If performance or layout is poor: implement a `Canvas`-based renderer that draws all cells in one paint pass.
  - Avoid one QML `Rectangle` per pixel for large matrices unless profiling proves it is acceptable.
- Preview size source:
  - Default to the selected fixture group dimensions.
  - Allow manual override for script development: width/height spinboxes such as 8x8, 10x10, 20x5, 32x16.
- Animation:
  - Timer independent from real output.
  - Preview speed control as FPS or hold-time multiplier.
  - Step wraps by `rgbMapStepCount(width, height)`.
  - Include pause, restart, previous/next step.

### 3.5 Code editor design

MVP editor:

- Use QML `TextArea` or `TextEdit`.
- Monospace font.
- Preserve tabs/spaces.
- Add line/column display if feasible.
- Support undo/redo through the built-in text control.
- Add actions: format not required, find optional.

Future editor options:

- `Qt WebEngineView` + CodeMirror/Monaco for syntax highlighting and stronger editing.
- This should be a later phase because WebEngine is a large dependency and may complicate Android/mobile packaging.
- If syntax highlighting is desired without WebEngine, investigate `QSyntaxHighlighter` on a `QQuickTextDocument`-backed `TextEdit` first.

Recommendation: ship the first version with native QML `TextArea/TextEdit` and defer WebEngine.

### 3.6 Live reload flow

Desired flow:

1. User edits script text.
2. QML starts/restarts a 300 ms debounce timer.
3. Debounce calls backend `evaluatePreviewSource(source)`.
4. Backend checks syntax and evaluates in an isolated preview runtime.
5. If valid:
   - parse properties,
   - preserve matching property values where possible,
   - compute step count,
   - render next preview frame,
   - clear error state.
6. If invalid:
   - report line/column/message,
   - keep the last good rendered frame,
   - do not alter the real matrix.

Runtime errors during `rgbMap` should be displayed separately from syntax/evaluation errors. A bad frame should not stop the editor; the preview timer can pause until the next successful evaluation or property change.

### 3.7 Property sliders/dropdowns

Short-term implementation can reuse the existing property schema:

- `list` -> `CustomComboBox`
- `range` -> `CustomSpinBox` initially, optional slider + numeric box later
- `float` -> `CustomDoubleSpinBox` initially, optional slider when min/max metadata exists
- `string` -> `CustomTextEdit`

Enhancement needed: current `RGBScriptProperty` has no explicit `integer` type even though the feature vision mentions it. Plan options:

- Treat current `range` as the integer slider type.
- Add a future `integer` parser alias only if existing scripts or generated node scripts need it.
- For floats, consider extending property metadata to support `values:min,max,step` later.

Property updates in preview mode must call the preview script instance, not `m_matrix->setProperty(...)`, until the user applies/saves.

### 3.8 Save/export behavior

Add explicit user-script management before allowing edits to built-in scripts.

Proposed behavior:

- Built-in resource script selected:
  - editor opens read/write copy in memory,
  - **Save** prompts/acts as **Save As User Script**,
  - default filename derived from script name and sanitized.
- User script selected:
  - **Save** writes back to the user script file,
  - **Save As** writes a copy.
- New script:
  - start from `resources/rgbscripts/empty.js` or a generated template,
  - save to the user scripts directory.

Backend tasks:

- Identify existing script search paths and user script directory used by `RGBAlgorithm`/script cache.
- Add a QML-facing list of script origin metadata: display name, file path, built-in/user, dirty state.
- Invalidate/reload the script cache after saving so the Pattern dropdown sees the new/updated script.
- If a saved script replaces the currently selected matrix algorithm, update the matrix only after explicit **Apply to Matrix** or **Save and Use**.

### 3.9 Feature D delivery phases

#### D0 — Investigation and design confirmation

- Confirm whether the active Qt 6 build path still uses `QScriptEngine`, `QJSEngine`, or compatibility wrappers.
- Identify script search paths and user script storage location.
- Inspect existing `RGBMatrixPreview` implementation and performance.
- Decide whether to add `loadContents()` to `RGBScript` or create a separate preview runtime wrapper.

Deliverable: technical design issue/PR with class boundaries and UI wireframe.

#### D1 — Extract reusable preview controller

- Decouple preview rendering from the actual `RGBMatrix` object.
- Add preview-only rendering from an `RGBAlgorithm`/`RGBScript` instance and explicit width/height.
- Preserve current `RGBMatrixEditor` preview behavior.
- Add unit tests for preview data conversion and step wrapping where practical.

Deliverable: no visible UX regression; current preview still works.

#### D2 — Native QML script editor MVP

- Add `RGBScriptEditorPanel.qml` with native text editor, preview, error panel, and property panel.
- Add debounce evaluation.
- Keep last good frame on errors.
- Support manual preview dimensions and preview speed.
- Read selected script source into the editor.

Deliverable: users can edit a copy of a selected script and see live preview.

#### D3 — User script save/apply

- Implement save-as to user scripts directory.
- Refresh algorithm list/cache after save.
- Add **Save and Use** to apply the saved script to the current matrix.
- Guard built-in scripts from accidental overwrite.

Deliverable: edited scripts persist and can be selected like existing scripts.

#### D4 — Polish and advanced authoring

- Add optional syntax highlighting.
- Add find/replace.
- Add error line highlighting.
- Add script templates.
- Add optional audio preview input/simulation for API v3 scripts.

Deliverable: production-quality authoring experience.

---

## 4. Feature C — Node-Based Visual Effect Editor

### 4.1 User experience target

Add a visual editor where users create effects by connecting nodes, preview the result live, and save the graph as a generated RGB script.

Core UX:

- A canvas with draggable nodes and typed ports.
- Inspector panel for selected node properties.
- Live preview panel shared with Feature D.
- Compile status panel with errors/warnings.
- Save generated `.js` and companion `.nodeGraph.json` metadata.
- Exactly one output node: **RGB Map**.

### 4.2 Framework choice: QuickQanava

Use **QuickQanava** as the planned node canvas framework because it is Qt/QML-native, BSD-3 licensed, and already aligned with Qt Quick.

Integration options:

1. **Git submodule / vendored third-party directory**
   - Most reproducible for contributors and CI.
   - Easier to patch if QLC+ needs compatibility fixes.
   - Adds repository maintenance overhead.
2. **CMake FetchContent**
   - Keeps repository smaller.
   - Requires network access at configure time unless cached, which may be undesirable for packaging.
3. **System package**
   - Least bundled code.
   - Risky because package availability varies by platform.

Recommendation: start with a vendored/submodule integration gated behind a CMake option, then revisit packaging once MVP stabilizes.

Proposed build flags:

- `option(effect_node_editor "Build node-based RGB effect editor" OFF)` initially.
- Enable automatically only when `qmlui=ON` and dependency is present.
- Keep Feature D independent from QuickQanava.

CMake touch points:

- Top-level `CMakeLists.txt`: add option and dependency inclusion when enabled.
- `qmlui/CMakeLists.txt`: link QuickQanava target/imports and include new source files/QML resources.
- `qmlui/qmlui.qrc`: add node editor QML files if this project continues to package QML through qrc.
- Packaging scripts: ensure QuickQanava QML plugin/modules are deployed on macOS, Windows, Linux, and Android if supported.

### 4.3 Node editor proposed files/classes

QML:

- `qmlui/qml/fixturesfunctions/RGBEffectNodeEditor.qml`
  - Main editor shell: node canvas, node palette, inspector, preview.
- `qmlui/qml/fixturesfunctions/nodes/EffectNode.qml`
  - Base visual node component.
- `qmlui/qml/fixturesfunctions/nodes/Port.qml`
  - Typed port UI.
- `qmlui/qml/fixturesfunctions/NodePalette.qml`
  - Searchable node catalog.
- `qmlui/qml/fixturesfunctions/NodeInspector.qml`
  - Property editor for selected node.

C++:

- `qmlui/rgbnodegrapheditor.h/.cpp`
  - QML-facing graph model, load/save, compile, validation, preview hooks.
- `engine/src/rgbeffectgraph.h/.cpp`
  - Data model for nodes, ports, connections, properties.
- `engine/src/rgbeffectnodecatalog.h/.cpp`
  - Catalog of node type definitions.
- `engine/src/rgbeffectcompiler.h/.cpp`
  - Graph validation and graph-to-JS compiler.

Keep graph compilation in C++ rather than QML so it is testable and can be reused for import/export tools.

### 4.4 Node type system

Each node type definition should include:

- stable type id, for example `source.solidColor`
- display name and category
- input ports: name, type, multiplicity
- output ports: name, type
- properties: name, type, default, min/max/options, display label
- compile template or compile function
- preview/editor metadata: color/icon/help text.

Port types:

- `Color`: RGB color value.
- `Intensity`: scalar 0..1.
- `Grid<Color>`: 2D RGB map.
- `Grid<Intensity>`: 2D scalar map.
- `Audio`: audio input object/spectrum.
- `Number`: scalar number.
- `Vector2`: x/y coordinate or offset.
- `Boolean`: mask/control.

Connection rules:

- Exact type match by default.
- Allow safe implicit conversion only when explicitly defined, for example `Intensity -> Color` through grayscale conversion.
- Prevent cycles for MVP unless a future feedback/delay node defines stateful behavior explicitly.
- Require exactly one **RGB Map** output node.
- Require every required input to be connected or have a default.

### 4.5 Node catalog for MVP and later phases

#### Source nodes

- Solid Color
  - Outputs `Grid<Color>` or `Color` depending on implementation choice.
  - Properties: color.
- Gradient
  - Outputs `Grid<Color>`.
  - Properties: colors, orientation, repeat/mirror.
- Noise/Perlin
  - Outputs `Grid<Intensity>`.
  - Properties: scale, speed, seed.
- Image/Text
  - Later phase due asset loading and text rasterization complexity.
- Audio FFT
  - Later phase unless audio script API is confirmed end-to-end.

#### Generator nodes

- Sine Wave
  - Outputs `Grid<Intensity>`.
  - Properties: frequency, phase, speed, direction.
- Chase/Scan
  - Outputs `Grid<Intensity>`.
  - Properties: direction, width, speed, softness.
- Stripes
  - Outputs `Grid<Intensity>` or `Grid<Color>`.
  - Properties: width, gap, direction, offset speed.
- Checkerboard
  - Outputs `Grid<Intensity>`.
  - Properties: cell size, phase/speed.
- Random
  - Outputs `Grid<Intensity>`.
  - Properties: density, seed, refresh rate.

#### Modifier nodes

- Color Map
  - Input `Grid<Intensity>`, output `Grid<Color>`.
  - Properties: gradient stops.
- Mirror
  - Input/output same grid type.
  - Properties: horizontal, vertical, both.
- Rotate
  - 0/90/180/270.
- Scale/Zoom
  - Later phase due sampling complexity.
- Shift/Scroll
  - Offset with wrapping.
- Threshold
  - `Grid<Intensity>` to binary intensity/mask.
- Invert
  - Invert color or intensity.

#### Blend nodes

All blend nodes take two same-type grid inputs and output a grid:

- Add
- Multiply
- Screen
- Max
- Min
- Mask

#### Output node

- RGB Map
  - Exactly one required.
  - Input: `Grid<Color>`.
  - Optional properties: name, author, accepted color count, generated script API version.

### 4.6 Graph to JS compilation

Compilation stages:

1. **Normalize graph**
   - Load graph JSON into typed model.
   - Assign stable internal variable names.
   - Apply defaults for unconnected optional inputs.
2. **Validate graph**
   - Check exactly one output node.
   - Check port type compatibility.
   - Check no unsupported cycles.
   - Check required properties and asset references.
3. **Topological sort**
   - Order nodes from sources to output.
   - Surface clear compile errors for disconnected/cyclic graphs.
4. **Generate helper functions**
   - Include only helpers required by used node types.
   - Examples: RGB packing, clamp, lerp, coordinate transforms, blend modes, seeded random.
5. **Generate `rgbMapStepCount(width, height)`**
   - Use least common period where possible.
   - MVP can return a configurable fixed step count, such as 100 or derived from max node period.
6. **Generate `rgbMap(width, height, rgb, step)`**
   - Allocate maps as nested arrays.
   - Evaluate generated node code in topological order.
   - Return final `Grid<Color>`.
7. **Generate properties**
   - Expose graph-level and selected node properties through the existing RGB script property API.
   - Map property setters/getters to generated script state.
8. **Generate API metadata**
   - `apiVersion = 2` for non-audio graphs.
   - `apiVersion = 3` and `usesAudio = true` only when audio nodes are used and runtime support is verified.

Compiler output should be deterministic. The same graph should generate stable JS to make diffs reviewable.

### 4.7 Persistence

Save two files:

- `effect-name.js`
  - Generated script compatible with the existing RGBScript engine.
  - Can be selected by existing RGB Matrix Pattern dropdown.
- `effect-name.nodeGraph.json`
  - Full editable graph metadata.
  - Stores nodes, connections, positions, node property values, graph version, compiler version, generated script filename, and optional UI metadata.

Proposed JSON versioning fields:

- `formatVersion`
- `qlcplusMinVersion`
- `generatorVersion`
- `graphId`
- `scriptFile`
- `nodes`
- `connections`

Save/load behavior:

- Opening a generated JS script with matching `.nodeGraph.json` offers **Edit Graph**.
- Opening a JS script without graph metadata offers **Edit Script** only.
- Changing generated JS manually should mark graph metadata as potentially stale unless regenerated.

### 4.8 Preview integration

Feature C should reuse Feature D infrastructure:

- Graph changes debounce, compile to JS, and feed the preview runtime.
- If compilation succeeds but script evaluation fails, show both generated-code error and graph context if possible.
- Keep last good frame on invalid graph or compile error.
- Allow manual preview dimensions independent of fixture group.
- Add graph-specific overlays later, such as visualizing selected node output before the final output node.

### 4.9 Feature C delivery phases

#### C0 — Dependency spike

- Add QuickQanava behind a non-default CMake option in a local branch.
- Build on macOS Qt 6.
- Verify basic QML import, canvas rendering, node drag, and connection creation.
- Assess deployment impact.

Deliverable: spike PR or branch notes; no product UI yet.

#### C1 — Graph model and compiler without UI dependency

- Implement typed graph data model.
- Implement node catalog for a tiny set:
  - Solid Color
  - Sine Wave
  - Color Map
  - Add/Max blend
  - RGB Map output
- Implement validation, topological sort, deterministic JS generation.
- Add unit tests for graph validation and generated script smoke tests.

Deliverable: command/test-level graph-to-JS compiler.

#### C2 — Minimal node editor MVP

- Add QuickQanava-based canvas.
- Add node palette and inspector.
- Add connection validation in UI.
- Compile on debounce and preview output.
- Save/load `.nodeGraph.json` plus generated `.js`.

Deliverable: users can create simple visual effects without writing code.

#### C3 — Expanded node catalog

- Add Stripes, Checkerboard, Random, Mirror, Rotate, Shift/Scroll, Threshold, Invert, Multiply/Screen/Min/Mask.
- Add reusable helper library for generated JS.
- Add property exposure for generated scripts.

Deliverable: practical visual editor for common LED effects.

#### C4 — Advanced media/audio nodes

- Add Audio FFT after confirming API v3 audio path in the script runtime.
- Add Image/Text after deciding how to package/load assets.
- Add node-output preview/debug mode.

Deliverable: advanced effect authoring comparable to lightweight visual VJ tools.

---

## 5. Shared infrastructure

### 5.1 Preview runtime

Create a shared preview runtime that both editors can use:

- Input source:
  - existing selected `RGBAlgorithm`,
  - script source string,
  - generated JS from node graph.
- Inputs:
  - width, height,
  - base RGB color/current accepted colors,
  - step,
  - property values,
  - optional audio sample/simulation.
- Output:
  - `QVariantList` colors for QML preview.
  - structured errors.
  - step count and metadata.

### 5.2 Property model

Replace backend-created QML objects over time with a declarative property model:

- Backend exposes `QAbstractListModel` or `QVariantList` of property descriptors.
- QML uses a `Repeater`/delegate to create controls.
- The same model works for:
  - existing scripts,
  - edited scripts,
  - generated node graph properties.

Suggested descriptor fields:

- `name`
- `displayName`
- `type`
- `currentValue`
- `min`
- `max`
- `step`
- `options`
- `readOnly`
- `sourceNodeId` for node-generated properties.

### 5.3 Script runtime adapter

Because the codebase currently shows `QScriptEngine` but the feature context refers to QJSEngine, add a boundary:

- `IRGBScriptRuntime` or a concrete `RGBScriptRuntime` wrapper.
- Methods:
  - evaluate from file/source,
  - get metadata,
  - get/set properties,
  - set colors,
  - render frame,
  - report errors.
- Internals can use current `RGBScript`/`QScriptEngine` first and later migrate to QJSEngine without rewriting QML/editor code.

### 5.4 Error model

Use structured errors instead of only qWarning output:

- `phase`: syntax, evaluate, property, render, compile, save.
- `message`
- `line`
- `column`
- `stack`
- optional `nodeId` for graph compile/runtime errors.

### 5.5 Tests

Recommended tests:

- RGB script source evaluation succeeds/fails with expected errors.
- Property parsing and current values for sample scripts.
- Preview runtime maps `[height][width]` JS arrays to expected flat QML colors.
- Step wrapping for fixed and dynamic step counts.
- Graph validation with parameterized cases:
  - missing output,
  - multiple outputs,
  - type mismatch,
  - disconnected required input,
  - valid simple graph.
- Deterministic compiler output for a known graph.
- Save/load graph JSON round trip.

---

## 6. Dependency analysis

### 6.1 QuickQanava

Pros:

- QML-native node graph UI.
- Suitable mental model for typed nodes and connections.
- BSD-3 license is compatible for planned integration.

Risks:

- Adds third-party dependency and packaging burden.
- May require Qt 6 compatibility work depending on upstream state.
- Mobile/Android deployment must be proven separately.
- Node editor should remain optional until stable.

Recommendation:

- Integrate behind `effect_node_editor` CMake option.
- Keep compiler and graph model independent from QuickQanava.
- Vendor/submodule for reproducibility once the spike succeeds.

### 6.2 Qt WebEngine / Monaco / CodeMirror

Pros:

- Best code editing experience.
- Syntax highlighting, search, minimap, diagnostics are easier.

Risks:

- Large dependency.
- Deployment complexity.
- Potentially unsuitable for Android/mobile builds.
- Overkill for MVP.

Recommendation:

- Do not use WebEngine for the first script editor release.
- Start with native QML editing.
- Reassess after live preview and save/apply are stable.

### 6.3 QScriptEngine vs QJSEngine

Current inspected code uses `QScriptEngine`; Qt 6 availability may depend on compatibility modules or project-specific configuration. The plan should not assume a runtime migration is complete.

Recommendation:

- Add a runtime adapter and source-evaluation helper.
- Keep generated JS conservative and compatible with the current script examples.
- Treat QJSEngine migration as a separate technical debt item unless required by the active build.

### 6.4 Audio preview

Audio scripts and nodes are valuable, but the inspected `RGBScript::rgbMap` currently passes four JS arguments while `audiospectrum.js` defines a fifth `audio` argument. Before promising audio preview, confirm the active runtime path for API v3 audio.

Recommendation:

- MVP: support non-audio scripts/graphs first.
- Show a clear placeholder or simulated input option for `usesAudio` scripts.
- Add real audio preview after runtime support is verified.

---

## 7. Implementation timeline

### Phase 0 — Research and foundations

- Confirm JS runtime path and script directories.
- Profile/currently inspect `RGBMatrixPreview`.
- Define preview runtime adapter API.
- Define property descriptor model.

Ships: no user-facing changes.

### Phase 1 — Shared live preview extraction

- Extract preview controller from `RGBMatrixEditor` coupling.
- Keep existing RGB Matrix editor preview working.
- Add configurable preview dimensions and step controls in internal API.

Ships: safer internal architecture and current preview preserved.

### Phase 2 — Enhanced script editor MVP

- Add native QML code editor panel.
- Add debounce evaluation.
- Add structured error display.
- Add generated property controls from preview script properties.
- Add live preview independent from actual matrix output.

Ships: edit-in-memory live script preview.

### Phase 3 — Script persistence

- Add save-as user script.
- Add script cache refresh.
- Add save-and-use/apply flow.
- Protect built-in resources from overwrite.

Ships: complete enhanced RGB Script editor for normal use.

### Phase 4 — Node compiler core

- Add graph model and node catalog in C++.
- Add graph validation and deterministic JS compiler.
- Add tests.

Ships: non-UI graph-to-script foundation.

### Phase 5 — Node editor MVP

- Integrate QuickQanava behind optional build flag.
- Add canvas, node palette, inspector, and connection validation.
- Reuse Feature D preview and save pipeline.

Ships: visual effect editor MVP.

### Phase 6 — Polish and expansion

- Add more nodes.
- Add graph debugging and selected-node preview.
- Add syntax highlighting or better code editor if still needed.
- Add audio/media nodes after runtime validation.

Ships: broader creative workflow.

---

## 8. Risk assessment

### Runtime safety risk

Risk: live editing could affect a running matrix or fixture output.

Mitigation:

- Use preview-only script instances.
- Do not call `m_matrix->setProperty(...)` for unsaved preview edits.
- Apply to matrix only through explicit user action.

### JS runtime mismatch risk

Risk: plan assumes QJSEngine but code uses QScriptEngine.

Mitigation:

- Introduce runtime adapter.
- Confirm active build path before implementation.
- Keep generated JS compatible with existing scripts.

### Performance risk

Risk: QML per-cell rendering may be slow for larger grids.

Mitigation:

- Reuse existing preview first.
- Profile 10x10, 20x5, 32x16, and larger grids.
- Move to Canvas/custom item if needed.
- Throttle preview FPS independently from MasterTimer.

### Dependency and packaging risk

Risk: QuickQanava and WebEngine increase build/deployment complexity.

Mitigation:

- Do not require either for Feature D MVP.
- Gate QuickQanava behind an option until packaging is proven.
- Avoid WebEngine initially.

### Graph compiler correctness risk

Risk: generated JS may be hard to debug or produce invalid RGB maps.

Mitigation:

- Deterministic compiler output.
- Strong graph validation before generation.
- Unit tests for each node template/helper.
- Include generated comments mapping code sections to node ids in debug builds or optional export mode.

### Audio support risk

Risk: API v3 audio expectations may not match current script invocation.

Mitigation:

- Treat audio nodes/scripts as later phase.
- Confirm engine support before adding real audio preview.
- Use simulated audio only as an explicit preview tool.

### Save/cache risk

Risk: saved scripts do not appear in the Pattern dropdown or overwrite built-ins.

Mitigation:

- Implement explicit user script directory handling.
- Invalidate script cache after save.
- Mark script origin as built-in/user.
- Force built-in edits through Save As.

---

## 9. Recommended MVP scope

The best first shippable increment is **Feature D without QuickQanava or WebEngine**:

- Native QML script editor.
- Preview-only source evaluation.
- Debounced live preview.
- Generated property controls.
- Structured errors.
- Save-as user script.

Then build Feature C on top of those proven foundations:

- Graph model and compiler first.
- QuickQanava UI second.
- Expanded node catalog after the simple graph workflow works end to end.
