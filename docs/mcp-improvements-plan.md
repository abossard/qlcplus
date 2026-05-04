# MCP Server Improvements Implementation Plan

> **Status: PLAN ONLY — not yet implementing.**

## Goal

Improve the QLC+ MCP server for AI agents by tightening validation, reducing response tokens, and making common workflows easier. The work is sequenced so low-risk safety fixes land before response-shape changes and new convenience tools.

## Opus 4.7 Review — Key Corrections

The validation helpers `validateFields()` and `validateEnums()` already exist in `tool_registry.h`. The issue is **inconsistent application**, not a missing layer. Don't build a new framework — extend what's there.

**Do:**
- Add `validateRequired()` helper for required-field checks (the one gap)
- Mechanical sweep: wire `items` guard + `validateFields` + `validateEnums` into 14 batch tools
- Make invalid enums hard errors everywhere (silent fallbacks actively mislead agents)
- Add `query_workspace_summary` (biggest agent UX win — replaces 4-6 cold-start calls)
- Per-tool `detail` param (default compact) on heavy query tools
- Trim tool descriptions, move long prose to `prompts.cpp` resources
- Measure `tools/list` token size before optimizing schema

**Don't:**
- Don't build a new declarative validation meta-layer (will conflict with existing schema)
- Don't hide tools from `tools/list` (breaks MCP spec discovery)
- Don't add global session-level "compact mode" (stateful flags fight stateless retries)
- Don't refactor `conversions.h` or `vc_query_helpers.h` (134+74 tests depend on them)
- Don't add `create_basic_look` convenience tool (too opinionated, hides failure modes)
- Skip `create_function_button` unless agent traces show it's the dominant pattern

**Estimated scope:** 1-2 day mechanical sweep + `query_workspace_summary` + compact modes on 4 query tools.

## Current code map

- Tool registry and shared helpers: `mcp/tools/tool_registry.h`
- Agent-facing query tools: `mcp/tools/query_tools.cpp`
- Fixture/channel tools: `mcp/tools/channel_tools.cpp`
- Function creation and script tools: `mcp/tools/function_tools.cpp`
- VC create/update/input/layout tools: `mcp/tools/vc_create_tools.cpp`, `mcp/tools/vc_update_tools.cpp`, `mcp/tools/vc_input_tools.cpp`, `mcp/tools/vc_layout_tools.cpp`
- Flow tools: `mcp/tools/flow_tools.cpp`
- JSON serializers: `mcp/tools/conversions.h`, `mcp/tools/vc_query_helpers.h`
- MCP server/resource wiring: `mcp/mcpserver.cpp`, `mcp/mcpserver.h`
- Tests and build registration: `mcp/test/*.cpp`, `mcp/test/*.h`, `mcp/test/CMakeLists.txt`

## Phase 1: Quick Wins (low risk, high impact)

### 1a. Fix batch `items` validation (14 tools)

- **Change**
  - Add `validateItemsArray(args, "items")` in `mcp/tools/tool_registry.h`.
  - It should verify that the top-level args are an object, `items` exists, and `items` is an array.
  - Return an error JSON string consistent with existing helpers, and let call sites return it before `args.at("items")`.
  - Apply to the 14 reviewed batch tools first; use the current inventory to avoid accidental scope creep.
- **Primary files to change**
  - `mcp/tools/tool_registry.h`
  - Batch call sites in:
    - `mcp/tools/function_tools.cpp`
    - `mcp/tools/vc_create_tools.cpp`
    - `mcp/tools/vc_update_tools.cpp`
    - `mcp/tools/vc_input_tools.cpp`
    - `mcp/tools/vc_layout_tools.cpp`
    - `mcp/tools/channel_tools.cpp`
    - `mcp/tools/io_tools.cpp`
    - `mcp/tools/query_tools.cpp`
- **Likely call-site candidates from inspection**
  - `vc_create_pages`, `vc_create_widgets`, `vc_update_widgets`
  - `vc_map_inputs`, `vc_configure_feedback`, `vc_set_key_sequences`
  - `vc_reparent_widgets`, `vc_set_grid_layout`
  - `configure_channels`, plus other selected channel/I/O/function batch tools that currently call `args.at("items")`
- **Estimated scope**: ~80-140 LOC total, mostly one guard per tool plus tests.
- **Risk level**: Low.
- **Tests needed**
  - Add parameterized tests for missing `items`, non-array `items`, empty array, and valid array.
  - Prefer a focused helper test target or extend an existing MCP validation test.
  - Build/run the affected test target and `qlcplusmcp`.
- **Dependencies**: None. Do first because later phases can reuse the helper.

### 1b. Fix enum validation gaps (6 tools)

- **Change**
  - Add `validateEnums()` at tools that define enum constraints but currently parse values manually or silently fall back.
  - For batch tools, validate each item against the item schema properties; for non-batch tools, validate top-level args.
  - Ensure invalid enum returns an error before state mutation.
- **Files to change**
  - `mcp/tools/query_tools.cpp`: `query_palettes`, `query_rgb_algorithms`
  - `mcp/tools/channel_tools.cpp`: `configure_channels`
  - `mcp/tools/vc_layout_tools.cpp`: `vc_reflow_frame`, `vc_set_grid_layout`
  - `mcp/tools/flow_tools.cpp`: flow tools with constrained values, especially `sizePreset` and widget `type`
- **Estimated scope**: ~50-100 LOC.
- **Risk level**: Low to medium, because invalid values that previously no-op'ed or fell back will now fail loudly.
- **Tests needed**
  - Parameterized invalid/valid enum tests.
  - Confirm current case sensitivity expectations; do not accidentally accept values outside the schema.
  - Run `mcp_vc_validation_test`, `mcp_vc_query_filter_test`, and a flow test if one is added.
- **Dependencies**: None, but can share any structured error formatting from Phase 3 if that is pulled forward.

### 1c. Add RGBW modes to MCP schemas

- **Change**
  - No implementation work planned here because `RGBW` and `RGBWBrighter` were already added to the `controlMode` enum in this session.
  - Keep this item as verification/documentation in the improvement checklist.
- **Files to verify**
  - Tool schema location containing `controlMode`.
  - Any tests covering RGB matrix/control-mode schema validation.
- **Estimated scope**: 0-20 LOC if only a regression test or release note is added.
- **Risk level**: Low.
- **Tests needed**
  - Schema/enum test that accepts `RGBW` and `RGBWBrighter`.
- **Dependencies**: None.

## Phase 2: Token Reduction (medium effort, big impact for agents)

### 2a. Add compact query responses

- **Change**
  - Add response-shaping options to high-volume query tools while preserving current defaults.
  - `query_fixtures`: add `fields` array to select fields from `fixtureToJson()` output.
  - `query_functions`: add `includeDetails` boolean. Default should preserve current compact behavior; `true` can opt into full type-specific data if added.
  - `vc_query_pages`: keep existing `properties` filter, but improve description and examples so agents use it.
  - `query_fixture_channels`: add `compact` boolean to omit capabilities/ranges and return channel identity/classification only.
- **Files to change**
  - `mcp/tools/query_tools.cpp`
  - `mcp/tools/channel_tools.cpp`
  - `mcp/tools/conversions.h`
  - `mcp/tools/vc_query_helpers.h`
- **Estimated scope**: ~160-260 LOC.
- **Risk level**: Medium because response shapes change when new options are used.
- **Tests needed**
  - Parameterized serialization tests for field inclusion/exclusion.
  - Backward-compatibility tests proving omitted options return the same shape as today.
  - Add/extend tests around `fixtureToJson`, `functionToJson`, and `channelToJson` helpers.
- **Dependencies**
  - Phase 1 validation should land first so new parameters reject typos cleanly.

### 2b. Add `query_workspace_summary` convenience tool

- **Change**
  - Add a read-only tool that returns one compact overview for agent planning:
    - fixture count
    - function count by type
    - universe count/configured universes
    - VC page count
    - running function count
  - Keep response intentionally shallow; link agents to detailed query tools for drill-down.
- **Files to change**
  - `mcp/tools/query_tools.cpp`
  - Possibly `mcp/tools/conversions.h` if helper serializers are useful.
- **Estimated scope**: ~80-140 LOC.
- **Risk level**: Low to medium. Mostly read-only aggregation.
- **Tests needed**
  - Unit/integration test with an empty document.
  - Test with a small document containing multiple function types and VC pages if a test bridge exists.
  - Verify annotations use `mcp::kAnnotReadOnly`.
- **Dependencies**
  - Can run in parallel with 2a after Phase 1.

### 2c. Shorten `create_scripts` description

- **Change**
  - Move long Engine API documentation and examples out of the `create_scripts` tool description.
  - Register an MCP resource for script authoring documentation, then reference it from the short tool description.
  - Keep only required fields, safety notes, and a pointer to the resource in the tool text.
- **Files to change**
  - `mcp/tools/function_tools.cpp`
  - `mcp/mcpserver.cpp`, `mcp/mcpserver.h`
  - New resource registration helper if the resource manager API needs a separate function.
- **Estimated scope**: ~80-180 LOC depending on fastmcpp resource API usage.
- **Risk level**: Medium because resource support is currently allocated but not used in the inspected server wiring.
- **Tests needed**
  - Build `qlcplusmcp`.
  - If practical, MCP integration test that resource listing/reading exposes the script docs.
  - Confirm `create_scripts` schema and behavior are unchanged.
- **Dependencies**
  - Requires confirming fastmcpp resource registration API.

## Phase 3: Agent Experience (medium effort)

### 3a. Improve error messages

- **Change**
  - Replace generic failures such as `{ "status": "failed" }` or `{ "error": "failed" }` with structured errors:
    - `error`
    - `field`
    - `expected`
    - `got`
    - optional `id`/`widgetID`/`context`
  - Focus first on VC tools because their polymorphic schemas and parent/page relationships are hardest for agents.
- **Files to change**
  - `mcp/tools/tool_registry.h` for shared error helpers.
  - `mcp/tools/vc_create_tools.cpp`
  - `mcp/tools/vc_update_tools.cpp`
  - `mcp/tools/vc_input_tools.cpp`
  - `mcp/tools/vc_layout_tools.cpp`
  - `mcp/tools/vc_tools_common.h`
- **Estimated scope**: ~180-320 LOC.
- **Risk level**: Medium. Error response shapes may affect clients that compare exact strings.
- **Tests needed**
  - Parameterized tests for missing required fields, invalid parent/page references, invalid widget type, and invalid function binding.
  - Keep per-item batch behavior: one bad item should not prevent later items unless the top-level request is invalid.
- **Dependencies**
  - Phase 1 helpers first.
  - Coordinate with Phase 1/1b so validation errors use the same structure.

### 3b. Add workflow hints to tool descriptions

- **Change**
  - Add short, agent-oriented hints to key tool descriptions without making schemas noisy.
  - Examples:
    - `create_scenes`: "Tip: create palettes first, then reference them."
    - `vc_create_widgets`: "Tip: query pages first to find `parentID`."
  - Keep hints one line each and avoid repeating full tutorials.
- **Files to change**
  - `mcp/tools/function_tools.cpp`
  - `mcp/tools/vc_create_tools.cpp`
  - Possibly `mcpserver.cpp` server instructions if global workflow text should be shortened or aligned.
- **Estimated scope**: ~20-50 LOC.
- **Risk level**: Low.
- **Tests needed**
  - Build-only validation is sufficient unless a schema snapshot test exists.
- **Dependencies**
  - Can run any time, but best after 2c so descriptions remain concise.

### 3c. Add `create_function_button` convenience tool

- **Change**
  - Add a one-call tool that accepts a function ID and target page or parent frame, then creates a button widget with the function bound.
  - Auto-position using the existing VC/grid/layout helpers where possible.
  - Return the created/existing widget ID, function ID, parent/page info, and position.
- **Files to change**
  - `mcp/tools/vc_create_tools.cpp` or a new VC convenience tool file if the registry is split.
  - `mcp/tools/tool_registry.h` if a new registration function/file is introduced.
  - `mcp/mcpserver.cpp` if a new registration function is added.
  - `mcp/test/CMakeLists.txt` and a new/extended VC test.
- **Estimated scope**: ~150-280 LOC.
- **Risk level**: Medium. Auto-positioning and idempotency semantics need clear rules.
- **Tests needed**
  - Function not found.
  - Page not found / parent not found.
  - Successful create with explicit parent frame.
  - Idempotent repeat call with same function/parent/caption.
  - Positioning collision or next-slot behavior.
- **Dependencies**
  - Phase 3a structured errors should land first.
  - Reuse Phase 2/VC query improvements in documentation examples.

## Phase 4: Schema Optimization (lower priority)

### 4a. Evaluate hiding rarely-used tools

- **Change**
  - Measure actual agent/client use of low-frequency tools before hiding anything.
  - Candidate areas: flow tools, feedback profile tools, channel modifier tools.
  - If hiding is justified, gate tools behind a capability/config flag rather than deleting them.
- **Files to inspect/change**
  - `mcp/tools/flow_tools.cpp`
  - `mcp/tools/vc_input_tools.cpp`
  - `mcp/tools/channel_tools.cpp`
  - `mcp/mcpserver.cpp` for capability/config wiring
  - Any CLI/settings files that expose MCP configuration
- **Estimated scope**: investigation ~0 LOC; implementation ~120-240 LOC if gating is added.
- **Risk level**: Medium to high because hiding tools can break existing agents.
- **Tests needed**
  - Default mode exposes all currently expected tools.
  - Reduced mode hides only intended tools.
  - Tool count/schema-list integration test if available.
- **Dependencies**
  - Needs usage data or explicit product decision.

### 4b. Split polymorphic VC schemas

- **Change**
  - Evaluate splitting `vc_create_widgets` and `vc_update_widgets` into smaller type-specific tools or schemas.
  - Possible split:
    - `vc_create_button`, `vc_create_slider`, `vc_create_frame`, etc.
    - Keep unified tools for backward compatibility.
  - Avoid unsupported JSON Schema features (`oneOf`, `anyOf`, `allOf`).
- **Files to inspect/change**
  - `mcp/tools/vc_create_tools.cpp`
  - `mcp/tools/vc_update_tools.cpp`
  - `mcp/tools/vc_tools_common.h`
  - `mcp/tools/tool_registry.h`
  - `mcp/mcpserver.cpp`
- **Estimated scope**: design spike ~0 LOC; implementation ~300-700 LOC depending on how many tools are split.
- **Risk level**: High. More tools can improve schema clarity but increase tool count and maintenance.
- **Tests needed**
  - Each split tool validates only its type-specific fields.
  - Unified tool remains backward compatible.
  - Existing VC create/update tests still pass.
- **Dependencies**
  - Complete Phase 3a first so shared validation/error infrastructure is ready.
  - Decide whether Phase 4a will hide or expose advanced tools before increasing tool count.

## Suggested sequencing and parallelism

1. Phase 1a and 1b first; they are safety fixes and establish validation conventions.
2. Phase 1c verification can happen alongside 1a/1b.
3. Phase 2a and 2b can proceed in parallel after validation helpers land.
4. Phase 2c can proceed in parallel only after confirming the fastmcpp resource API.
5. Phase 3a should precede 3c so the new convenience tool uses the final error format.
6. Phase 3b is independent but should wait until long descriptions are shortened.
7. Phase 4 should be treated as product/design work after usage data is available.

## Verification checklist

- Configure/build target: `cmake --build build --target qlcplusmcp -j8`
- Run focused tests as they are added/updated:
  - `build/mcp/test/mcp_vc_validation_test`
  - `build/mcp/test/mcp_vc_query_filter_test`
  - any new validation/resource/convenience-tool tests
- For schema/tool-description changes, inspect MCP tool list output from a running local server if an integration harness exists.
- Preserve backward-compatible default responses for existing query tools unless a new parameter is explicitly used.
