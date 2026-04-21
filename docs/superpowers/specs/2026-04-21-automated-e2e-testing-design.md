# Automated E2E Testing with MCP + AI Visual Verification

**Date:** 2026-04-21
**Status:** Approved

## Summary

Add full E2E testing for QLC+ v5 by extending the MCP server with a screenshot capture tool and using Python pytest as the test runner. Visual verification is performed by an AI model assessing screenshots rather than pixel-diffing, making tests resilient to rendering differences across platforms.

## Architecture

```
┌─────────────────────────────────────────────────────┐
│  Python E2E Test Runner (pytest)                     │
│  ┌────────────┐  ┌────────────┐  ┌───────────────┐ │
│  │ MCP Client │  │ AI Visual  │  │  Screenshot   │ │
│  │ (HTTP/JSON) │  │ Assessor   │  │  Archive      │ │
│  └─────┬──────┘  └─────┬──────┘  └───────┬───────┘ │
└────────┼───────────────┼──────────────────┼─────────┘
         │ HTTP POST     │ LLM API call    │ saved PNGs
         ▼               ▼                  ▼
┌─────────────────────────────────────────────────────┐
│  qlcplus-qml (QT_QPA_PLATFORM=offscreen)            │
│  ┌────────────┐  ┌────────────┐  ┌───────────────┐ │
│  │ MCP Server │  │ Screenshot  │  │ VirtualConsole│ │
│  │ :port/mcp  │  │ Tool (new)  │  │ + Engine      │ │
│  └────────────┘  └────────────┘  └───────────────┘ │
└─────────────────────────────────────────────────────┘
```

## Component 1: Screenshot MCP Tool

**File:** `mcp/tools/screenshot_tools.cpp`

New MCP tool `capture_screenshot`:
- Calls `QQuickWindow::grabWindow()` on the main window
- Encodes as PNG, returns base64-encoded data in JSON
- Parameters:
  - `width` (optional): resize viewport width before capture
  - `height` (optional): resize viewport height before capture
  - `delay` (optional, ms): wait before capture for animations to settle
  - `savePath` (optional): also save to disk at this path
- Returns: `{ "image": "<base64 PNG>", "width": N, "height": N }`
- Runs on main thread via `execOnMainThread()`
- Annotation: read-only

## Component 2: Python Test Framework

**Directory:** `tests/mcp/`

### MCP Client
Reuse/refactor the existing `test_e2e.py` HTTP client into a shared `mcp_client.py` module:
- `MCPClient` class with `call_tool(name, args)`, `initialize()`, `shutdown()`
- Handles session management (`Mcp-Session-Id` header)
- App lifecycle: launch `qlcplus-qml --mcp-port {port}` with `QT_QPA_PLATFORM=offscreen`, wait for ready, cleanup on exit

### AI Visual Assessor
A `visual_assessor.py` module:
- Takes a screenshot (base64 PNG) + a natural language assertion (e.g. "There should be 12 buttons arranged in a 4x3 grid with function names as captions")
- Sends to an AI vision model (GPT-4o, Claude, etc.) via API
- Returns pass/fail + reasoning
- Configurable: model, API key (env var), strictness level
- Screenshots are always saved to `tests/mcp/screenshots/` for human review regardless of pass/fail

### Test Structure
```
tests/mcp/
├── conftest.py          # pytest fixtures: app launch, MCP client, screenshot helper
├── mcp_client.py        # Shared MCP HTTP client
├── visual_assessor.py   # AI-based screenshot verification
├── test_e2e.py          # Existing logic-only E2E tests (52 cases)
├── test_visual.py       # New visual E2E tests
└── screenshots/         # Captured screenshots (gitignored)
```

## Component 3: Test Scenarios

### Logic Tests (expand existing `test_e2e.py`)
- Fixture patching lifecycle
- All function types: Scene, Chaser, EFX, RGBMatrix, Collection, Script, Audio
- VC widget CRUD for all types
- Widget property updates (colors, fonts, geometry)
- Auto-layout reflow
- Page management

### Visual Tests (`test_visual.py`)
Each test: drive via MCP → capture screenshot → AI assesses

| Test | Setup | Visual Assertion |
|---|---|---|
| `test_empty_page` | Create page | "A dark empty Virtual Console page with no widgets" |
| `test_button_grid` | Create 4×3 buttons with scene names | "12 buttons arranged in a grid, each showing a scene name" |
| `test_slider_bank` | Create 6 intensity sliders | "6 vertical sliders side by side" |
| `test_animation_colors` | Create animation widget with red/blue colors | "An animation widget showing red and blue color swatches" |
| `test_auto_layout` | Create 20 random widgets → auto-layout | "Widgets neatly arranged in rows grouped by type, no overlaps" |
| `test_full_show` | Fixtures + functions + full VC layout | "A complete show layout with buttons, sliders, and frames" |
| `test_frame_nesting` | Create nested frames with children | "Nested frames with widgets inside each frame" |

## Component 4: CI Integration

**File:** `.github/workflows/build.yml`

- Re-enable the build job (`if: true`)
- Add `test-e2e` step after build:
  ```yaml
  - name: Run E2E tests
    run: |
      cd build
      QT_QPA_PLATFORM=offscreen pytest ../tests/mcp/test_e2e.py -v
    env:
      QT_QPA_PLATFORM: offscreen
  ```
- Visual tests run separately (require AI API key):
  ```yaml
  - name: Run visual tests
    if: env.VISUAL_TEST_API_KEY != ''
    run: pytest ../tests/mcp/test_visual.py -v
    env:
      VISUAL_TEST_API_KEY: ${{ secrets.VISUAL_TEST_API_KEY }}
  ```
- Upload screenshots as artifacts on failure

## Key Design Decisions

1. **AI assessor over pixel diff:** QML rendering varies across platforms (font hinting, anti-aliasing, DPI). AI assessment is semantically meaningful ("are there 12 buttons?") rather than pixel-exact.
2. **Screenshots always saved:** Even on pass, for human review and debugging.
3. **Visual tests are optional in CI:** They require an API key. Logic tests always run.
4. **Separate test files:** `test_e2e.py` (logic) vs `test_visual.py` (visual) — can run independently.
5. **Shared MCP client:** Refactored from existing code, not duplicated.

## Out of Scope

- QML-native `TestCase {}` component tests (future work)
- Browser-based webaccess testing (separate effort)
- Performance/load testing
- Accessibility testing
