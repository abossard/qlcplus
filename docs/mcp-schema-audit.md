# MCP Schema Audit: Consistency & Type Safety

> Generated 2026-04-30. Comprehensive audit of all 51 MCP tools.

## Priority 1: Schema Safety (prevent wrong calls entirely)

### P1.1: Add missing enums (agents guess wrong values)

| Tool | Field | Current | Add Enum |
|------|-------|---------|----------|
| create_scenes | tempoType | free text | `["time","beats"]` |
| create_sequences | tempoType | free text | `["time","beats"]` |
| create_efxs | tempoType | free text | `["time","beats"]` |
| create_rgb_matrices | tempoType | free text | `["time","beats"]` |
| create_rgb_matrices | runOrder | free text | `["Loop","SingleShot","PingPong","Random"]` |
| create_rgb_matrices | direction | free text | `["Forward","Backward"]` |
| create_rgb_matrices | controlMode | free text | `["RGB","White","Amber","UV","Dimmer","Shutter"]` |
| create_rgb_matrices | blendMode | free text | `["Normal","Additive"]` |
| create_rgb_matrices | animationStyle | free text | add enum per valid values |
| create_rgb_matrices | mirror | free text | add enum |
| create_rgb_matrices | mirrorBlend | free text | add enum |
| configure_beat_source | type | free text | `["disabled","internal","plugin","audio"]` |
| set_grand_master | valueMode | free text | `["limit","reduce"]` |
| set_grand_master | channelMode | free text | `["intensity","all"]` |
| query_palettes | typeFilter | free text | `["Dimmer","Color","Pan","Tilt","PanTilt"]` |
| read_dmx_values | channelFilter | free text | `["all","dimmer","color","position","gobo","shutter","beam","effect"]` |
| update_scene_from_dmx | channelFilter | free text | same as above |

### P1.2: Fix mismatched enums (schema says X, code accepts Y)

| Tool | Field | Schema Says | Code Accepts | Fix |
|------|-------|-------------|-------------|-----|
| vc_create/update_widgets | clickAndGoType | `["none","colors","preset"]` | also `"rgb"`, `"cmy"` | Add `"rgb"`, `"cmy"` to enum |
| vc_update_widgets | slider mode | `["level","playback","submaster"]` | also `"grandmaster"` | Add `"grandmaster"` |

### P1.3: Dead schema field

| Tool | Field | Issue |
|------|-------|-------|
| vc_map_inputs | mode | Schema declares `["replace","add"]` but handler NEVER reads this field |

## Priority 2: Naming Consistency (prevent key confusion)

### P2.1: Standardize response ID key

| Current State | Where | Fix |
|--------------|-------|-----|
| Create returns `widgetID` | vc_create_tools.cpp (~16 occurrences) | Change to `id` |
| Update returns `widgetID` | vc_update_tools.cpp | Change to `id` |
| Update input uses `widgetID` | vc_update_tools.cpp schema | Change to `id` |
| Reparent uses `widgetID` | vc_layout_tools.cpp | Change to `id` |
| Input tools use `widgetID` | vc_input_tools.cpp (3 tools) | Change to `id` |
| Query returns `id` | vc_query_helpers.h | Already correct |
| Delete returns `id` | vc_layout_tools.cpp | Already correct |
| update_scene_from_dmx returns `sceneID` | function_tools.cpp | Change to `id` |

### P2.2: Standardize batch input key

| Current | Tool | Fix |
|---------|------|-----|
| `widgetIDs` | vc_query_widgets | Change to `ids` |
| `fixtureIDs` | query_fixture_channels | Leave (different entity, not a batch wrapper) |
| `ids` | delete_functions, vc_delete_widgets, delete_palettes | Already consistent |
| `items` | all create/update/configure tools | Already consistent |

### P2.3: Standardize enum casing across tools

| Concept | Chasers/EFX/Sequences | RGB Matrices | Fix |
|---------|----------------------|-------------|-----|
| runOrder | `["loop","single","pingpong","random"]` | Description says `"Loop, SingleShot, PingPong, Random"` | Standardize to lowercase everywhere, make handler case-insensitive |
| direction | `["forward","backward"]` | Description says `"Forward, Backward"` | Same — lowercase + case-insensitive handler |
| tempoType | `["time","beats"]` (enum) | free text `"Time or Beats"` | Add enum `["time","beats"]` everywhere, make handler case-insensitive |

### P2.4: Standardize timing field names

| Concept | Chasers | Sequences | EFX | RGB Matrices |
|---------|---------|-----------|-----|-------------|
| Hold/duration | `hold` | `holdTime` | `speed` | `duration` |

These serve the same purpose but have 4 different names. Not fixable without breaking API.
Document clearly in each tool description.

## Priority 3: Documentation (prevent confusion)

### P3.1: Add batch wrapper note to all batch tool descriptions

Replace trailing `"Batch."` with `"Batch: wrap entries in {\"items\": [...]}."` for
items-based tools, and `"Batch: wrap IDs in {\"ids\": [...]}."` for delete tools.

### P3.2: Add type constraint to timing fields

For dual-type fields (fadeIn/fadeOut that accept int ms OR beat string), add to description:
`"Integer ms OR beat string ('1/8','1/4','1/2','1','2','4'). Beat strings auto-set tempoType to 'beats'."`

### P3.3: Document undocumented fields

| Tool | Field | Needs |
|------|-------|-------|
| vc_update_widgets | rangeLowLimit/rangeHighLimit | "Slider value range (0-255)" |
| vc_update_widgets | monitorEnabled | "Show DMX monitor bar on slider" |
| vc_update_widgets | invertedAppearance | "Invert slider/knob direction" |
| vc_update_widgets | visibilityMask | Document the bitmask values |
| vc_create_widgets | fadeInMultiplier etc | Document valid multiplier values |

## Priority 4: Launchpad fix

Remove MIDI substring requirement in configure_launchpad (io_tools.cpp:703,715).
Add port names to error message on detection failure.

## Summary Stats

- **17 fields** missing enum constraints
- **2 fields** with wrong enum values (schema vs code mismatch)
- **1 dead schema field** (never read)
- **~74 string replacements** for widgetID -> id standardization
- **4 tools** need batch wrapper documentation
- **3 enum casing** inconsistencies across tool families
- **5+ fields** missing descriptions
