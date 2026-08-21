# Copilot Instructions for QLC+

## Project Overview

QLC+ is a cross-platform lighting control application built with C++/Qt. The codebase has two UI variants:
- **v4** (`ui/`): Classic Qt Widgets UI (`APPVERSION 4.x`)
- **v5** (`qmlui/`): Modern QML UI (`APPVERSION 5.x`, built when `-Dqmlui=ON`)

The active development target is v5 (QML). The build produces `qlcplus5`.

## Build System

CMake-based. Qt 6 is required.

### Configure & Build (macOS)

```bash
# One-time configure (from repo root)
mkdir -p build && cd build
cmake .. -Dqmlui=ON

# Build everything
cd build && cmake --build . -j8

# Build specific target
cmake --build . --target qlcplus5 -j8
cmake --build . --target qlcplusmcp -j8
cmake --build . --target mcp_vc_query_filter_test -j8
```

### Dev Cycle: Rebuild & Restart

After making code changes, rebuild the specific library/target that changed, then restart the app:

```bash
# 1. Rebuild (from build/ directory)
cmake --build . --target qlcplus5 -j8

# 2. Kill the running instance (find PID first)
ps aux | grep qlcplus5 | grep -v grep
kill <PID>

# 3. Restart
cd build && nohup ./qmlui/qlcplus5 -d > /dev/null 2>&1 &
```

**Important:** Do NOT kill/restart QLC+ automatically without being asked. The user may have unsaved state.

If only the MCP server code changed, rebuilding `qlcplusmcp` alone is sufficient, but a restart is still needed since it's statically linked.

### Reconfigure

If you add new source files or CMakeLists.txt changes:
```bash
cd build && cmake ..
```

### Run Tests

```bash
# Run a specific test suite
cd build && ./mcp/test/mcp_vc_query_filter_test
cd build && ./mcp/test/mcp_vc_validation_test

# Build and run
cmake --build . --target mcp_vc_query_filter_test -j8 && ./mcp/test/mcp_vc_query_filter_test
```

Tests use Qt Test framework (`QTest`, `QVERIFY`, `QCOMPARE`). Parameterized tests use the `_data()` / `QFETCH` pattern.

## Architecture

### Key Directories

| Directory | Purpose |
|-----------|---------|
| `engine/src/` | Core engine: fixtures, functions, universes, DMX |
| `qmlui/` | QML UI (v5): editors, virtual console, 3D view |
| `mcp/` | MCP server: AI agent tool interface |
| `mcp/tools/` | MCP tool implementations (query, create, update, etc.) |
| `mcp/test/` | MCP unit tests |
| `plugins/` | I/O plugins (DMX USB, ArtNet, sACN, MIDI, OSC, etc.) |
| `resources/` | Fixture definitions, RGB scripts, translations |

### MCP Server (`mcp/`)

The MCP server exposes QLC+ functionality to AI agents via HTTP JSON-RPC on `http://127.0.0.1:9696/mcp`.

Key files:
- `vcbridge.h` — Abstract interface for Virtual Console operations (structs: `WidgetInfo`, `WidgetDetails`, `PageInfo`)
- `vcbridgev5.cpp` — QML implementation of VCBridge
- `tools/tool_registry.h` — Tool registration declarations, `execOnMainThread()` helper, `validateFields()`, annotation constants
- `tools/vc_query_helpers.h` — Query filtering, validation, serialization helpers
- `tools/query_tools.cpp` — Query tools (fixtures, functions, pages, widgets, universes, palettes)
- `tools/vc_tools_common.h` — Widget type resolution, field validation for create/update

### Coding Patterns

- **Thread safety**: All tool handlers run on the MCP server thread but must access QLC+ data on the main thread. Use `execOnMainThread(doc, [&]() { ... })`.
- **JSON**: Use `nlohmann::json` (aliased as `Json`). Tool handlers take `const Json &args` and return `Json` (as string via `.dump()`).
- **Error handling**: Return `Json({{"error", "message"}}).dump()` for errors. Use `validateFields()` to reject unknown parameters.
- **Validation**: Type-specific validation lives in `vc_tools_common.h` (VCType, VCFields, VCValidate namespaces). Query validation lives in `vc_query_helpers.h` (VCQueryPages namespace).
- **Tool annotations**: Use `mcp::kAnnotReadOnly`, `mcp::kAnnotIdempotent`, `mcp::kAnnotDestructive`.
- **MCP tool schemas**: Must be valid JSON Schema. Do NOT use `oneOf`, `anyOf`, or `allOf` — they are not supported by MCP clients. Use `"type": "string"` with a description explaining accepted formats instead.

## Code Intelligence (CodeGraph)

A codegraph MCP server is configured in `.mcp.json` and indexes the entire codebase (~1,530 files, 24,650 symbols). **Prefer codegraph tools over grep/glob** for symbol lookups, call graph traversal, architecture questions, and impact analysis.

**Canonical reference:** `AGENTS.md` (tool catalog, anti-patterns, examples).

Quick preference order: `codegraph_context` (start here) → `codegraph_files` (replaces glob) → `codegraph_explore` (multi-symbol source) → individual tools → `grep`/`glob` (fallback for literal text).

### Widget Types

Known types (case-sensitive strings used in MCP): `button`, `slider`, `xypad`, `frame`, `soloframe`, `speedDial`, `cuelist`, `label`, `audioTrigger`, `matrix`, `clock`.

### Testing Conventions

- Test files: `mcp/test/<name>_test.cpp` + `<name>_test.h`
- Use `QTEST_MAIN(ClassName)` at the end of `.cpp`
- Header declares test class with `Q_OBJECT` and `private slots:`
- Parameterized tests: `void testName_data()` populates columns, `void testName()` fetches with `QFETCH`
- Register in `mcp/test/CMakeLists.txt` with `add_executable`, include dirs, and link libs

## Platform Notes

- **macOS Tahoe (26.x)**: Ad-hoc codesigning with `--options runtime` causes dyld Team ID mismatch crashes. Do NOT use `--options runtime` for ad-hoc signed dev builds.
- The app binary is at `build/qmlui/qlcplus5` (no `.app` bundle in dev builds).
- Use `-d` flag for debug output when running.


## Lighting Design Research

When the user asks to create or improve lighting effects:
1. Read `docs/lighting-research-guide.md` for the full workflow.
2. Use MCP tools to query fixtures, create effects, and iterate.
3. Always ask the user to preview and give feedback before refining.
4. Name experiments with the `EXP-` prefix for easy cleanup.
5. Save winners as permanent named functions and recipes.

## Sub-Agent Model Policy

When using the task tool to spawn sub-agents (explore, task, general-purpose, rubber-duck, code-review), always set the `model` parameter to match the model currently in use for the main conversation. Do not rely on default sub-agent models.
