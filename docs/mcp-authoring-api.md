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
