# VC performance recording: requirements

Status: planned extension. The [existing function-command recording](../show-command-recording.md) remains supported.

[Engineering principles](core-principles.md) · [Architecture decision](../adr/0001-vc-performance-recording.md) · [Implementation plan](implementation-plan.md) · [Test strategy](test-strategy.md)

## Scope

Record accepted VC state changes and apply them through the controls' current compatible bindings.

Support ordinary Toggle buttons, including SoloFrames, and the main values of Level, Adjust, Submaster and GrandMaster sliders. Report unsupported recording for Flash, global-action buttons, slider flash/reset inputs and other VC types; keep their live operation available.

Use one editable event list. Support explicit QLC+ timeline seeks first; defer VirtualDJ seek-intent handling. Extra lanes, raw MIDI/OS2L capture, initial console snapshots and automatic conversion of old recordings are outside this change.

## Capture and replay

| ID | Requirement |
|---|---|
| C0-1 | Record the resulting button On/Off state and every accepted changed slider position from pointer, keyboard or MIDI input. Timestamp at the bound Show clock after mapping and pickup; store absolute slider position normalized to `0..1`. Exclude unchanged duplicate values, but retain rapid changes that native output processing may coalesce. |
| C0-2 | Exclude audio, programmatic updates, Replay, automatic Collection-child changes and recording-off input. Retain accepted input across deferred delivery without duplication. A new pass adds changes and preserves existing later events. |
| C0-3 | Apply the recorded state through the control's current compatible configuration, using the button rules below. Rebinding a function or changing a range affects replay; type, action, mode or attribute-role changes are incompatible. MIDI remapping does not change the destination. Matching slider positions require no action. |
| C0-4 | Report and skip missing, disabled, ambiguous or incompatible destinations without pausing the Show. Preserve the event; do not substitute a control by caption or reused numeric ID. Continue valid events. |
| C0-5 | Save and reload legacy function commands and new VC states with distinct targets and preserved equal-time order. Retain legacy ownership and seek semantics. Reject unknown versions/actions and malformed data without replacing valid data. |
| C0-6 | While Playing, process every crossed event once in saved order, including after late updates. Apply the state rules without reducing intermediate events. Do not infer a seek from the size of a clock update. |
| C0-7 | On an explicit seek in either direction or Play from a stopped cursor, restore the latest recorded button and slider states at or before the destination once. With no earlier state for a control, leave it unchanged. Skip intermediate states and preserve the previous Playing/Paused state after a seek. |
| C0-8 | Suspend dispatch during seeking and cancel stale work on stop, seek, unload or target removal. Pause holds pending work for resume. Complete valid loop-tail/end work before ending that pass; do not duplicate destination events after restoration. |

### Button state application

| Current VC state | Recorded ON | Recorded OFF |
|---|---|---|
| Inactive | Start through the button; become Active | No action |
| Active | No action, including startup/SoloFrame side effects | Stop through the button; become Inactive |
| Monitoring | Acquire the button's own activation; become Active and survive a parent Collection stopping | Stop the monitored function through the control |

Monitoring is neither satisfied ON nor satisfied OFF. Apply the requested outcome without replaying two synthetic clicks.

Pause and Stop halt command dispatch, not VC-started effects. Leave live busking alone between events; the next recorded event or explicit restoration can change the control. Do not reset latched controls or enforce recorded state continuously.

## Editing

**C0-9:** Provide one ordered **Time / Control / Action / Value** list. Show control identity, caption and current binding; distinguish legacy function commands and slider-position values from function-intensity values. Filtering by control changes visibility, not event order.

Permit exact-value edits, retiming and batch deletion only while playback has stopped and Record is off. Paused playback does not permit editing. Validate the whole edit before publication, with one undo unit tied to its Show ID.

## Referenced controls

| ID | Requirement |
|---|---|
| C1-1 | Before playback, list every unique persistent VC identity referenced anywhere in the selected Show. Group repeated references and expected roles. Keep the inventory independent of event-log retention or clearing; legacy function commands do not imply VC references. |
| C1-2 | Show configuration suitability using the replay resolver: Ready, Missing control, Ambiguous identity, Incompatible type/role, Disabled, or Unbound/missing binding where applicable. Preserve multiple per-role issues. Ready does not mean running, connected hardware or verified output. |
| C1-3 | While the inspector is visible, refresh after command-track changes and VC add/remove/rename/rebind/mode/enable changes, including binding-target availability. Leave authored gestures unchanged. |
| C1-4 | On close, hide, unload or workspace reset, detach view observation and reject stale callbacks. Reopen against current configuration. Playback must still validate destinations with the inspector closed. |

The read-only table shows control caption or an available recorded hint, persistent identity, expected roles, current binding, reference count and status reason. Provide All/Problems counts, a Problems filter and missing-first ordering. Virtualize rendered rows without truncating the reference inventory.

Level-channel and GrandMaster controls need no Function target. Keep missing gestures intact; optional Locate navigation must not play, delete, fix or rebind controls.

The separate debug-event inspector remains memory-only, read-only and active only while open. This change adds its referenced-controls view, not a persistent diagnostic store.
