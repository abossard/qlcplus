# RGB Matrix MCP/AI Review

Three independent Opus 4.6 subagents analyzed the RGB Matrix system from an AI agent's perspective: API clarity, script authoring feasibility, and end-to-end workflow gaps.

**Last updated:** 2026-05-01 — reflects `validateEnums()` and JSON Schema `"enum"` fixes applied across all tools.

## API Quality Scores

| Dimension | Score | Key Issue |
|-----------|-------|-----------|
| Discoverability | 5/10 | Server instructions omit RGB Matrix workflow entirely |
| Type Safety | **7/10** | ~~3/10~~ — `validateEnums()` now catches invalid enum values with descriptive errors |
| Completeness | 8/10 | Design-time API solid, runtime API absent |
| Consistency | **7/10** | ~~5/10~~ — enum casing still differs (PascalCase in rgb_matrices vs lowercase in chasers) but both are now validated |

## Issue Tracker

### CRITICAL — Still Open

1. **No start/stop function tool** — Agent creates effect but can't run it. No `start_function` or `stop_function` MCP tool exists. The only way to start a function is via a VC button or Script's `Engine.startFunction()`, neither of which is MCP-accessible.

### HIGH — Still Open

2. **No operate mode toggle** — App may be in Design mode, no MCP tool to switch to Operate mode.

3. **No preview/pixel data tool** — Agent is blind to visual output. Cannot see rendered frames, only verify configuration parameters.

### HIGH — FIXED

4. ~~**Silent enum defaults**~~ **FIXED** — `validateEnums()` helper added to `tool_registry.h`. All enum parameters now return descriptive errors on invalid input, e.g.: `"invalid value for 'controlMode': \"dimmer\". Allowed: \"RGB\", \"White\", \"Amber\", \"UV\", \"Dimmer\", \"Shutter\""`. Applied to `create_rgb_matrices`, `create_chasers`, `create_efxs`, `create_sequences`, `create_scenes`, `update_scene_from_dmx`.

### MEDIUM — Still Open

5. **runOrder/direction casing mismatch between tools** — Still present but now validated:
   - `create_chasers` uses lowercase enum: `"loop"`, `"single"`, `"pingpong"`
   - `create_rgb_matrices` uses PascalCase enum: `"Loop"`, `"SingleShot"`, `"PingPong"`
   - Both now reject wrong-cased values with clear errors (rather than silently defaulting)
   - An AI that learns from one tool will get an error on the other, not silent wrong behavior

6. ~~**blendMode description incomplete**~~ **FIXED** — Now lists all 4 values: `Normal`, `Additive`, `Mask`, `Subtractive` with JSON Schema `"enum"` constraint.

7. **Color count description says 3, code allows 5** — Tool description at `function_tools.cpp:1113` still says "up to 3 colors" but code allows 5. Needs description update.

8. **No inline script upload via MCP** — RGB algorithm scripts must be `.js` files on disk. No MCP tool to write a script file or reload the script cache.

9. **Server instructions omit RGB Matrix workflow** — The workflow in `mcpserver.cpp` never mentions `query_rgb_algorithms`, `create_fixture_groups`, or `create_rgb_matrices`.

### MEDIUM — FIXED

- ~~**Enum validation with errors**~~ **FIXED** — `validateEnums()` in `tool_registry.h` validates all enum fields. JSON Schema `"enum"` constraints added to all create tools.
- ~~**blendMode description**~~ **FIXED** — Now includes Mask and Subtractive.
- ~~**animationStyle not validated**~~ **FIXED** — Now has `"enum": ["Static", "Letters", "Horizontal", "Vertical", "Animation"]`.
- ~~**mirror/mirrorBlend not validated**~~ **FIXED** — Both have `"enum"` constraints and `validateEnums()` checks.

### LOW — Still Open

10. **Properties must be strings** — Script property values must be strings (`"presetSize": "10"` not `"presetSize": 10`). Schema does not enforce this.

11. **Architecture doc claims `fixtureGroupName` exists** — `MCP-ARCHITECTURE.md` documents it but only `fixtureGroupID` is implemented.

12. **Dynamic `acceptColors` not surfaced** — Plasma's preset changes `acceptColors` from 5 to 0, but `query_rgb_algorithms` always returns the default state.

13. **Sparse upsert semantics undocumented** — When upserting, omitted parameters keep old values. No "replace" mode.

## Script Authoring Analysis

### Feasibility: Moderate (6/10)

An AI can write RGB scripts — the API is small (2-3 functions) and there are 60+ examples. But deployment and validation are hard.

### Minimum Viable Script (~20 lines)

```javascript
var testAlgo;
(function() {
    var algo = new Object;
    algo.apiVersion = 2;
    algo.name = "My Effect";
    algo.author = "AI Agent";
    algo.properties = new Array();

    algo.rgbMap = function(width, height, rgb, step) {
        var map = new Array(height);
        for (var y = 0; y < height; y++) {
            map[y] = new Array();
            for (var x = 0; x < width; x++)
                map[y][x] = rgb;
        }
        return map;
    };

    algo.rgbMapStepCount = function(width, height) {
        return width;
    };

    testAlgo = algo;
    return algo;
})();
```

### Color Format

Packed unsigned integer RGB: `(r << 16) | (g << 8) | b`. NOT ARGB — no alpha channel. Black/off = `0`. Helper available: `LedFx.rgb(r, g, b)`.

### Audio-Reactive Scripts

Set `algo.usesAudio = true`. The engine passes a 5th argument to `rgbMap()`:
- `audio.spectrum` — 32 normalized floats (0.0-1.0), frequency bands
- `audio.volume` — normalized 0.0-1.0
- `audio.beat` — boolean, consumed on read
- `audio.bpm` — integer
- `audio.maxMagnitude` — raw double

Utilities via auto-loaded `LedFx` global: `melbank()`, `hsv2rgb()`, `rgb()`, `createMap()`, `simplex2d()`, `ExpFilter()`, `beat_oscillator()`, etc.

### Property System

Pipe-delimited strings: `"name:foo|type:range|display:Foo|values:1,100|write:setFoo|read:getFoo"`

Types: `list` (enum values), `range` (min,max), `float`, `string`

### Error Feedback (4/10)

| Error Type | Feedback |
|-----------|----------|
| JS syntax error | Line number + stack trace |
| Missing rgbMap/rgbMapStepCount | Clear named error |
| Wrong return dimensions | Silently clamped, no warning |
| Invalid property values | Silently cast via toUInt() |
| Property format errors | Vague: "malformed property. Please fix it." |

### Testing Options

1. **Browser devtool** — `resources/rgbscripts/devtool.html` renders pixel grid visually
2. **User scripts dir** — Drop `.js` in `~/Library/Application Support/QLC+/RGBScripts/`, restart
3. **MCP verification** — After deploy, `query_rgb_algorithms` confirms loading

### Deployment Gap

No MCP tool to upload scripts. Workflow: write file -> restart/reload -> verify via `query_rgb_algorithms`. No fast feedback loop.

## End-to-End Workflow

### Complete Tool Call Sequence

```
1. query_available_fixtures    — find RGB LED fixtures in library
2. patch_fixtures              — patch 50 RGB fixtures
3. query_fixtures              — get fixture IDs
4. create_fixture_groups       — arrange in 10x5 grid
5. query_fixture_groups        — verify, get group ID
6. query_rgb_algorithms        — discover "Plasma" + properties
7. create_rgb_matrices         — create with all parameters
8. query_rgb_matrices          — verify configuration
9. STUCK                       — cannot start, cannot preview
```

8 tool calls deep, then impassable gap. Design-time API is solid; runtime API is absent.

## Remaining Fixes Needed

### Quick Wins (small changes, high impact)

| Fix | File | Change | Status |
|-----|------|--------|--------|
| Update color count description | `function_tools.cpp:1113` | "up to 3" -> "up to 5" | TODO |
| Add RGB Matrix to server instructions | `mcpserver.cpp` | Add workflow line | TODO |

### Medium Effort

| Fix | Files | Description | Status |
|-----|-------|-------------|--------|
| `start_function` / `stop_function` tools | New tool in `function_tools.cpp` | Start/stop any function by ID or name | TODO |
| Normalize enum casing across tools | All create tools | Decide on lowercase or PascalCase, apply consistently | TODO (low priority — both sides now validate) |

### Larger Effort

| Fix | Description | Status |
|-----|-------------|--------|
| Inline RGB script upload via MCP | New tool: write JS to user scripts dir, reload script cache, return algorithm name | TODO |
| Pixel preview tool | New tool: render one frame of an RGB Matrix, return as 2D color grid | TODO |
| Operate mode toggle | New tool or expose via existing mechanism | TODO |

### Already Fixed

| Fix | What was done |
|-----|--------------|
| Enum validation with descriptive errors | `validateEnums()` added to `tool_registry.h`, applied to all create/update tools |
| JSON Schema `"enum"` constraints | All enum parameters now have `"enum"` arrays in their JSON Schema definitions |
| `blendMode` missing values | Now lists Normal, Additive, Mask, Subtractive |
| `animationStyle` validation | Now has `"enum"` constraint |
| `mirror`/`mirrorBlend` validation | Both have `"enum"` constraints |
| Code allows 5 colors | `i < 3` changed to `i < 5` in `function_tools.cpp:960` |
| Rotation/mirror UI controls | Added to `RGBMatrixEditor.qml` (Rotation, Mirror, Mirror blend combo boxes) |

## Key Files Reference

| File | What |
|------|------|
| `mcp/tools/function_tools.cpp:886-1116` | `create_rgb_matrices` tool — schema, validation, handler |
| `mcp/tools/tool_registry.h:124-160` | `validateEnums()` helper — catches invalid enum values with descriptive errors |
| `mcp/tools/query_tools.cpp:510-727` | `query_rgb_algorithms` + `query_rgb_matrices` |
| `mcp/tools/conversions.h:227-292` | `rgbMatrixToJson` serialization |
| `mcp/mcpserver.cpp:46-78` | Server instructions (missing RGB workflow) |
| `engine/src/rgbmatrix.cpp` | Core rendering pipeline |
| `engine/src/rgbscriptv4.cpp` | JS engine, validation, audio data |
| `resources/rgbscripts/empty.js` | Official script template |
| `resources/rgbscripts/ledfx_compat.js` | Audio utility shim (auto-loaded) |
| `resources/rgbscripts/devtool.html` | Browser-based visual script tester |
| `mcp/test/rgb_transform_test.cpp` | 12 rotation/mirror unit tests |
| `qmlui/qml/fixturesfunctions/RGBMatrixEditor.qml` | Editor UI — now includes rotation/mirror/mirrorBlend controls |
