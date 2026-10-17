# MCP authoring contracts

The MCP authoring tools inspect and repair lighting functions without replacing
unrelated configuration. Read the registered input and output schemas through
`tools/list` before constructing a request.

## Compatibility changes

Batch authoring results now have one terminal record per input item, in input
order. Each record contains `index` and `status`. Successful operation names such
as `created`, `updated`, `existing` and `deleted` are in `outcome`, not `status`.
Generated child results, such as a fixture quantity, are nested in that item's
record.

`status` is `ok` or `error` for model operations. External device or file
operations can report partial effects. Their results distinguish attempted
effects from confirmed effects; dispatching a plugin parameter does not prove
that hardware applied it.

An invalid model item is rejected before mutation. Independent valid items
continue. A failed item must not erase an existing object's values, bindings or
path, or mark an otherwise clean project modified.

For selectors supplied in separate arrays, indices follow the documented tool
order. Palette deletion processes `ids` entries first, then `names` entries.
Glob expansions stay inside their selector's record. Repeated deletion requests
either identify the earlier operation or report that the target no longer
exists. They do not claim a second deletion.

Existing query list and pagination shapes remain unchanged. Virtual Console
queries retain the prior public `type` text and expose an additive `machineType`
for stable, locale-independent identity.

## Function detail queries

`query_function_details` accepts an ordered `targets` list. A target selects an
ID, or a name with an optional type. Names must resolve uniquely; an ambiguous
name returns an error instead of choosing the first match.

The result is an object containing an `items` array. Each item's `index` refers
to its position in `targets`. Successful records include the function's stable
ID, name, type, path, tempo and timing, followed by type-specific configuration.
Visibility and blend mode are also explicit authoring fields.

| Function | Authoring details |
|----------|-------------------|
| Scene | Channel values, fixture membership, palette IDs, fixture groups, channel groups with levels and value authority |
| Chaser | Direction, run order, speed modes and timed function-reference steps |
| Sequence | Bound Scene, speed modes and timed channel-value steps |
| EFX | Geometry, algorithm, propagation, dimmer control, timing and per-head fixture configuration |
| Collection | Function IDs |
| Script | Script content |

RGB/HUE matrices and Shows point to their existing dedicated query tools.

A Scene's fixture membership is separate from explicit channel values. A
palette-only fixture can belong to a Scene without any explicit values.
`valuesAuthoritative` is false when a Sequence binds the Scene: playback can
overwrite its values. The API cannot recover an original authored baseline
that the engine no longer stores.

## Sparse function edits

`update_functions` accepts an `items` list. Each item contains a `target` and the
fields to change. Its result is an object containing indexed `items`.

Omitted fields retain their current values. Supplied list fields replace the
whole list; they are not patches to individual list elements.

Common editable fields are name, path, visibility, blend mode, tempo and timing.
Type-specific fields match the registered schema, including Scene fixture
membership, palettes and group references, Chaser steps, Sequence steps,
Collection members, Script content and EFX geometry, dimmer control and fixtures.

Scene `fixtureGroupIDs` replaces its fixture-group references. `channelGroups`
replaces entries containing a group ID and its level from 0 to 255. Each group
must exist, and repeated group entries are rejected.

EFX fixture entries identify fixture, head, direction, starting offset and mode.
The fixture and head must exist, and the mode must be supported by that head.
Repeated fixture/head/mode entries are rejected.
Modes use the engine names `Position`, `Dimmer` and `RGB`, subject to the selected
head's supported modes.

### Timing

The new detail and edit tools use a timing object with `unit`:

| Unit | Value |
|------|-------|
| `ms` | Whole milliseconds |
| `beats` | A numeric beat count |
| `default` | No value |
| `infinite` | No value |

Finite timing must match the function's final tempo mode. The detail output can
be supplied back to the edit API without converting default or infinite timing
to a large integer.

Legacy create tools retain their existing millisecond-integer and beat-string
contracts. The new timing objects do not change those inputs.

## Playback admission

MCP refuses creation and deletion of functions while any function is running,
starting or queued. It does not stop playback automatically to make an
authoring request succeed.

Existing function edits use coordinated target and referrer admission. Running,
starting or queued targets and affected readers prevent the edit. Start requests
that arrive during an admitted edit are deferred until its configuration is
committed, not discarded.

Show timeline edits use the Show's admission guard. Fixture-group replacement
also protects matrices that read that group.

Fixture deletion refuses active playback and deletion that would remove
membership or values from a Scene bound by a Sequence. Deleting a bound Scene
is also rejected with its Sequence referrers listed. Remove those references
explicitly before deleting the Scene; the delete operation does not silently
empty Sequence steps.

Deletion of other referenced functions retains native QLC+ cleanup semantics:
Chaser steps, Collection members and Show items referencing the deleted function
are removed. This is separate from the protected Sequence-bound Scene case.

New container-reference lists are checked for containment cycles before
mutation. They cannot introduce a cycle or attach an already cyclic reachable
configuration. An unrelated sparse edit does not rewrite an existing graph.

## Sequence steps

Each authored Sequence step must contain the complete channel set of its bound
Scene. Fixture/channel pairs are canonicalized; missing, repeated or stale pairs
and values outside the DMX range are rejected.

A Scene channel-set change is rejected while Sequences reference it. An edit
does not silently rewrite their steps. Explicit zero step values survive XML
save and reload.

## Script validation

Script authoring checks syntax without running the body. Runtime-only errors
are not syntax errors. Validation neither starts functions nor executes system
commands, waits or loops from the script.

The same compile-only syntax check is used by the native syntax-line interface.
Actual playback remains responsible for runtime errors.

## MCP result envelopes

Request rejection and total tool failure set MCP `isError`. Mixed batches retain
per-item failures without falsely declaring that every item failed.

The new detail and edit tools declare object output schemas and return
`structuredContent` matching their JSON text payload. Polymorphic legacy query
arrays retain their existing text representation.

These contracts do not establish physical output or device confirmation. Use
the bounded verification tools separately when checking a patch or effect.

## Bounded Virtual Console reads

The v5 bridge supports two read-only authoring queries. They do not allocate
recording UUIDs, select a Show, change recording state or send controller data.
Other bridges can return `unsupportedBridge`.

| Tool | Selection |
|------|-----------|
| `vc_query_setup` | `widgets` (default), `pages` or `subpages` |
| `vc_query_recordings` | `references` (default) or `commands`; `showFunctionID` is required |

Both return an object with `version`, `basis`, `basisScope`, `rows`, `complete`,
`nextCursor`, `limitations` and `work`. The default limit is 100, with a maximum
of 250 rows and 262,144 serialized tool-payload bytes. A byte-limited page
continues with a cursor; a single row that cannot fit returns `itemTooLarge`.
An empty cursor means a successful result is complete.

Continue with the same selector, projection and limit. Cursors are bound to the
bridge instance and authored basis, expire after 60 seconds, and are limited to
1,024 UTF-8 bytes. Malformed or mismatched cursors return `invalidCursor`;
expired cursors return `cursorExpired`; relevant authored changes return
`staleBasis`. Switching a frame's visible subpage does not invalidate them.

Setup widget queries accept `pageWidgetID` or `frameWidgetID`, not both.
Recursion defaults to true. `childSubpage` selects a subpage of the specified
frame, including its nested descendants; each returned widget still reports
its own parent-local subpage. Explicit `widgetIDs` must exist within the
requested scope. Page queries accept `pageWidgetID`; subpage queries require
`frameWidgetID` and optionally accept `childSubpage`.

`fields` contains group names, not row-column names. Setup defaults to
`identity` and `placement`; widget detail groups are `logicalGeometry`,
`functionBinding`, `inputBindings`, `feedback`, `recordPanel`, `appearance` and
`frameConfiguration`. Pages and subpages accept only identity and placement.
Optional groups apply only to compatible widget types. Logical rectangles use
fractional parent-local coordinates. A detail row's `configurationDigest`
describes its selected groups, not authorization to apply an edit.

Recording groups are `identity`, `timing`, `semanticPayload` and `readiness`.
Commands preserve event IDs, order, times and hold-pair IDs. Selected semantic
payloads retain native values such as button on/off, position, intensity and
encoded typed parameters. References remain visible when unresolved;
request `readiness` for their status and reason. No captured event is edited.

These additive queries do not change legacy query arrays or integer geometry.

`vc_query_controls` now resolves native read-only data for:
- `targets`: sparse external control slots for requested widget IDs, with
  selector-scoped continuation cursors and authored-structure basis.
- `sources`: patched input/output/feedback inventories from the current native
  patch state, with explicit limits when UID or status is unavailable.
- `profiles`: loaded profile identity, channel maps and feedback tables.

With empty `fields`, defaults are limited and explicit:
- targets: `identity` + `domains`
- sources: `identity`
- profiles: `identity`

`vc_prepare_bindings` now supports native prepare/apply for:
- Function binding on VCButton: `set` and `clear`.
- Controller binding on registered slots: `append`, `move` and `remove`.
- Keyboard binding on registered keyboard-eligible slots: `set` and `remove`.

Controller writes require a patched destination input universe with the exact
already-loaded selected profile and supported channel semantics. By default,
cross-widget reuse of the same physical input address is refused; callers must
set `allowFanout: true` to permit cross-widget fanout. Same-widget source-key
resolution and duplicate native-pair ambiguity checks remain strict.

Controller action requirements:
- `append`: requires `controlSlotID` and `destination` (`inputUniverseID`,
  unpacked physical `inputChannel`, `profileName`), and refuses Relative
  destination channels.
- `move`: requires `controlSlotID`, the exact four-field `sourceKey`
  (`controlSlotID`, `inputUniverseID`, `inputChannel`, `childSubpage`), plus
  `destination` (`inputUniverseID`, unpacked physical `inputChannel`,
  `profileName`).
- `remove`: requires `controlSlotID` and the exact four-field `sourceKey`;
  destination/profile fields are not required.

Existing Relative sources are refused for controller move/remove source-key resolution.
Append has no source-key match and validates destination semantics only.
Absolute/Encoder destination channels and Button `ExtraPress` channels remain
supported when routed through the patched selected profile. Page targets remain
supported through native page-input refresh after append, move and remove.

Feedback binding mutation now supports native prepare/apply for:
- values group (`idleValue`, `activeValue`, `monitorValue`)
- channels group (`idleChannel`, `activeChannel`, `monitorChannel`)

Feedback requests still require an exact `sourceKey` and respect the existing
feedback closure preconditions (`feedbackPatchIdle`, `feedbackNeedsUpdateIdle`).
`vc_apply_preparation` consumes tokens and is non-idempotent by design.

`vc_prepare_record_panel` now supports native prepare/apply for VCRecordPanel
configuration fields:
- `targetFolder`
- `scenePrefix`
- `chaserPrefix`
- `defaultFadeIn`
- `defaultHold`
- `defaultFadeOut`

Each item requires `widgetID` and at least one configuration field. Items are
prepared as `independentItems`, and execution still goes through
`vc_apply_preparation`.

`vc_prepare_layout` now supports native prepare/apply for these non-geometry
layout operations:
- `pageCreate` (`name`)
- `pageRename` (`pageWidgetID`, `name`)
- `pageDelete` (`pageWidgetID`)
- `framePages` (`frameWidgetID`, `totalPages`, `pageLabels`, `pagesLoop`)

`operation` is required. For `framePages`, callers must provide
`frameWidgetID` plus at least one of `totalPages`, `pageLabels`, or `pagesLoop`.
Prepared tokens are executed via `vc_apply_preparation`.

Other layout operations remain geometry-only at this checkpoint.

## Virtual Console layout

`vc_reflow_frame` keeps its existing `flow` and `gridCompact` modes.
`algorithm: align` preserves stored sibling order, hierarchy and control sizes.
It aligns local rows and columns using their occupants' dimensions rather than
forcing every control into one uniform cell.
Button/slider size overrides and `framePad` are legacy flow options; `align`
rejects them instead of ignoring them.

Use `dryRun: true` to inspect the proposed changes. Page requests still default
to preview; frame requests retain their existing apply default. Fit errors
identify insufficient space instead of applying part of a layout. An apply
refusal returns `error` and `applied: false`.

The native VC outline shows pages, frames and frame subpages beside Functions.
Search matches captions, types or widget IDs and keeps ancestor context.
Selection is shared with the canvas. Outline drops change sibling order or
move controls into the indicated frame subpage.

Layout, nesting and sibling order belong to the existing workspace. Use normal
Save or Save As, not a separate layout store. Legacy workspaces remain readable;
their initial sibling order is inferred without moving controls.

The v5 writer preserves fractional logical coordinates, including when saving
at non-unit zoom. Existing integer-coordinate workspaces remain readable.
Older v5 builds and the v4 UI may not read fractional coordinates correctly;
keep a separate copy before opening a newly saved workspace in those versions.

Sibling order and frame-subpage history are local authoring state. Existing
network reparent messages retain their parent-ID format; they do not synchronize
the new local order/subpage metadata.
