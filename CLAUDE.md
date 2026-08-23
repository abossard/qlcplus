# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

QLC+ is a cross-platform lighting control application (C++/Qt). This is an experimental fork that adds an MCP server for AI agent integration. Two UI variants exist:
- **v4** (`ui/`): Classic Qt Widgets UI (`APPVERSION 4.x`)
- **v5** (`qmlui/`): Modern QML UI (`APPVERSION 5.x`, built with `-Dqmlui=ON`) — active development target

## Build Commands

```bash
# Configure (from repo root, one-time)
mkdir -p build && cd build
cmake .. -Dqmlui=ON

# Build everything
cd build && cmake --build . -j8

# Build specific targets
cmake --build . --target qlcplus5 -j8
cmake --build . --target qlcplusmcp -j8

# Reconfigure after adding new source files or CMakeLists.txt changes
cd build && cmake ..
```

## Running

```bash
# Run from build directory
cd build && ./qmlui/qlcplus5 -d   # -d for debug output

# Or via cmake target
cd build && cmake --build . --target run
```

**Important:** Do NOT kill/restart QLC+ automatically. The user may have unsaved state.

## Tests

Uses Qt Test framework (`QTest`, `QVERIFY`, `QCOMPARE`). Parameterized tests use `_data()` / `QFETCH` pattern.

```bash
# Build and run a single test
cd build && cmake --build . --target mcp_vc_query_filter_test -j8 && ./mcp/test/mcp_vc_query_filter_test

# Run all unit tests
cd build && cmake --build . --target check

# Engine tests are under engine/test/<testname>/
cd build && cmake --build . --target qlc_fixture_test -j8 && ./engine/test/fixture/qlc_fixture_test
```

MCP test files: `mcp/test/<name>_test.cpp` + `<name>_test.h`. Register new tests in `mcp/test/CMakeLists.txt`.

## Architecture

### Layer Dependency

```
qmlui/          (QML UI — top layer)
  ↓ uses
engine/src/     (core model — Doc, Fixtures, Functions, Universes)
  ↓ uses
plugins/        (I/O hardware: ArtNet, MIDI, DMX USB, OSC, etc.)

mcp/            (MCP API layer — parallel to qmlui)
  ↓ uses engine directly
  ↓ uses qmlui via VCBridge abstraction
```

### Engine Core (`engine/src/`)

- **Doc** — Central hub owning all project data. Holds maps of Fixtures, Functions, FixtureGroups, Palettes, and the InputOutputMap.
- **Fixture** — A lighting device instance with a fixture definition (QLCFixtureDef), mode, universe assignment, and DMX address.
- **Function** (base class) — Executable show component. Types: Scene, Chaser, Sequence, EFX, Collection, Script, RGBMatrix, Show, Audio, Video.
- **Universe** — 512 DMX channels. Has input/output patches connecting to plugins. Channels are HTP (highest takes precedence) or LTP (latest takes precedence).
- **MasterTimer** — 25Hz playback engine on a private thread. Processes running functions, blends DMX output, writes to universes.
- **InputOutputMap** — Manages N universes, plugin I/O routing, GrandMaster, beat sources.

### QML UI (`qmlui/`)

Entry point: `qmlui/main.cpp` → `App` (extends `QQuickView`). Owns `Doc` and coordinates UI managers (FixtureManager, FunctionManager, VirtualConsole, ContextManager, ShowManager, SimpleDesk, etc.). QML files are in `qmlui/qml/`.

### Plugin System (`plugins/`)

All plugins implement `QLCIOPlugin` interface (`plugins/interfaces/qlcioplugin.h`). Each plugin is a separate shared library loaded at runtime by `IOPluginCache`. Plugins provide Input, Output, and optional Feedback capabilities.

### MCP Server (`mcp/`)

HTTP JSON-RPC server on `127.0.0.1:9696/mcp` using the fastmcpp library. Exposes ~47 tools for AI agents to query/create/update fixtures, functions, Virtual Console widgets, I/O configuration, and palettes.

Key files:
- `mcpserver.h/cpp` — Server lifecycle, tool/prompt registration
- `mcpinit.h/cpp` — Startup entry point called from qmlui
- `vcbridge.h` — Abstract interface decoupling MCP from QML UI
- `vcbridgev5.h/cpp` — QML implementation of VCBridge
- `tools/tool_registry.h` — Tool registration macros, `execOnMainThread()`, `validateFields()`
- `tools/vc_query_helpers.h` — Query filtering and validation helpers
- `tools/vc_tools_common.h` — Widget type resolution and field validation

### Thread Model

- **Main thread**: Qt event loop, Doc access, UI
- **MasterTimer thread**: 25Hz function processing and DMX output
- **Universe threads**: Each Universe extends QThread
- **MCP HTTP thread**: fastmcpp server; must use `execOnMainThread(doc, [&]() { ... })` to access Qt objects

## MCP Coding Patterns

- **JSON**: `nlohmann::json` aliased as `Json`. Handlers take `const Json &args`, return `Json` as string via `.dump()`.
- **Errors**: Return `Json({{"error", "message"}}).dump()`. Use `validateFields()` to reject unknown parameters.
- **Idempotency**: Create tools resolve by name — if a function/widget with the same name exists, it's updated rather than duplicated.
- **Tool annotations**: `mcp::kAnnotReadOnly`, `mcp::kAnnotIdempotent`, `mcp::kAnnotDestructive`.
- **MCP JSON Schema**: Do NOT use `oneOf`, `anyOf`, `allOf` — MCP clients don't support them. Use `"type": "string"` with descriptive text instead.
- **Widget types** (case-sensitive): `button`, `slider`, `xypad`, `frame`, `soloframe`, `speedDial`, `cuelist`, `label`, `audioTrigger`, `matrix`, `clock`.

## Code Intelligence (CodeGraph)

A codegraph MCP server is configured in `.mcp.json` and indexes the entire codebase (~1,530 files, 24,650 symbols). **Prefer codegraph tools over grep/glob** for symbol lookups, call graph traversal, architecture questions, and impact analysis.

**Canonical reference:** `AGENTS.md` (tool catalog, anti-patterns, examples). Additional rules are auto-injected via `.claude/CLAUDE.md`.

Quick preference order: `codegraph_context` (start here) → `codegraph_files` (replaces glob) → `codegraph_explore` (multi-symbol source) → individual tools → `grep`/`glob` (fallback for literal text).

## Build Flags

- `-Dqmlui=ON` — Build v5 QML UI (produces `qlcplus5`)
- `-Dmcp_server=ON` — Build MCP server. Defaults ON whenever `qmlui` is ON; pass `-Dmcp_server=OFF` to opt out
- Compiler flags: `-Werror -Wextra -Wall` (non-iOS Unix builds)

## Platform Notes

- **macOS**: Ad-hoc codesigning with `--options runtime` causes dyld crashes on Tahoe (26.x). Don't use `--options runtime` for dev builds.
- App binary is at `build/qmlui/qlcplus5` (no `.app` bundle in dev builds).
- Workspace files use `.qxw` extension (XML format).
