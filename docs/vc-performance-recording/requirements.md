# VC performance recording: requirements

Status: implemented and verified through native/offscreen integration tests. Legacy function commands remain supported alongside VC commands; see [Show command tracks](../show-command-recording.md).

[Engineering principles](core-principles.md) · [Architecture decision](../adr/0001-vc-performance-recording.md) · [Implementation plan](implementation-plan.md) · [Test strategy](test-strategy.md)

## Scope

Record accepted commands from VC user interaction or external control input, then apply them through the controls' current compatible bindings. Slider/property changes are results, not recording sources.

Support ordinary Toggle buttons, including SoloFrames, and the main values of Level, Adjust, Submaster and GrandMaster sliders. Report unsupported recording for Flash, global-action buttons, slider flash/reset inputs and other VC types; keep their live operation available.

Keep one editable event list. The recording-timeline extension also presents its samples in one Recordings lane, using chronological groups of adjacent same-UUID/role/attribute slider commands. Every other saved command breaks a run; time gaps and unrecorded gestures do not. Edits recompute groups without widening exact event-ID selection. Group movement preserves intervals, values and ordering through the same delta history. The October 2026 live-editing contract (C5 below) supersedes the earlier stopped/REC-off edit gate. Support explicit QLC+ timeline seeks first; defer VirtualDJ seek-intent handling. Raw MIDI/OS2L capture, initial console snapshots and automatic conversion of old recordings remain outside this change.

## Capture and replay

| ID | Requirement |
|---|---|
| C0-1 | After mapping/pickup, normalize pointer, keyboard, MIDI or OSC input into a desired button On/Off or slider-position command. Record the eligible command before VC execution, with its accepted target and Show timestamp. Store slider position in `0..1`; exclude unchanged duplicates but retain accepted rapid commands even if native output later coalesces them. |
| C0-2 | Decide capture eligibility, target and timestamp when GUI input passes mapping/pickup. Exclude audio, programmatic updates, Replay, automatic Collection-child changes and input accepted after Record-off. Publish earlier accepted input across deferred delivery without duplication. A new pass adds changes and preserves existing later events. |
| C0-3 | Apply the recorded state through the control's current compatible configuration, using the button rules below. Rebinding a function or changing a range affects replay; type, action, mode or attribute-role changes are incompatible. MIDI remapping does not change the destination. Matching slider positions require no action. |
| C0-4 | Report and skip missing, disabled, ambiguous or incompatible destinations without pausing the Show. Preserve the event; do not substitute a control by caption or reused numeric ID. Continue valid events. |
| C0-5 | Save and reload legacy function commands and new VC commands with distinct targets and preserved equal-time order. Retain legacy ownership and legacy-only seek behavior. Mixed VC recordings use C0-7's serial catch-up in global recorded order. Reject unknown versions/actions and malformed data without replacing valid data. |
| C0-6 | While Playing, process every crossed event once in saved order, including after late updates. Apply the state rules without reducing intermediate events. Do not infer a seek from the size of a clock update. |
| C0-7 | Cursor movement while Paused or Stopped applies nothing. ~~On Play at a new position, execute the recording's commands from the start through that position once~~ (superseded by C1 below: Play from T dispatches nothing before T). Resume without repositioning does not repeat earlier commands. Do not reduce, interpolate or infer missing commands. |
| C0-8 | Suspend ordinary dispatch during seeking and cancel stale work on stop, seek, unload or target removal. Complete already-crossed commands in order before Pause completes. Queue conflicting manual requests behind that work, preserving their accepted target, time and desired value; unrelated controls stay usable. Complete the final event before ending a pass. Do not repeat destination events or block the GUI/lighting thread. |

### Button state application

| Current VC state | Recorded ON | Recorded OFF |
|---|---|---|
| Inactive | Start through the button; become Active | No action |
| Active | No action, including startup/SoloFrame side effects | Stop through the button; become Inactive |
| Monitoring | Acquire the button's own activation; become Active and survive a parent Collection stopping | Stop the monitored function through the control |

Monitoring is neither satisfied ON nor satisfied OFF. Apply the requested outcome without replaying two synthetic clicks.

Pause halts new ordinary dispatch after settling crossed work. Pause and Stop do not halt VC-started effects. Leave live busking alone between recorded commands.

### Serial jump (was: serial catch-up)

The prefix catch-up from 0 is superseded by C1. A requested forward seek applies the commands it crosses without waiting their original time gaps, while respecting native completion order and keeping the UI responsive. Events at the destination execute once. A SoloFrame handles A ON, B ON, B OFF itself; the recorder must not duplicate its rules. Add no initial reset or console snapshot.

### C1: seek without history catch-up (2026-10-02)

| ID | Requirement |
| --- | --- |
| C1-1..2 | Forward (external clock or requested seek) from P to T dispatches exactly the commands in [consumed position at P, T] once, in saved order; a requested jump runs them serially. |
| C1-3, C1-6 | Backward from P to T (requested, external, loop wrap, seek to 0) dispatches nothing before T. Controls and command-owned Functions this traversal's timeline operations changed in (T, P] return to their native state before the first such change, as one delta, Offs first; unjournaled live changes win per Solo Frame. Commands at T play again. |
| C1-4 | Play from T dispatches nothing before T; commands at T are due at once. Under an external clock T is the earliest sample set after Play; a value present before Play is ignored. |
| C1-5 | A forward request keeps live no-echo marks; a backward one clears them for the next pass. |
| C1-7 | Ordinary advance, pause/resume, Stop, external-ignored requests, live authoring and recorded-event edits are unchanged. |

## Editing

**C0-9:** Place the ordered **Time / Control / Action / Value** list in a **Recordings** tab inside the Show editor, sharing its selected Show, playhead and transport. Show control identity, caption and current binding; distinguish legacy function commands and slider-position values from function-intensity values. Filtering changes visibility, not event order.

Single-click selects a row; double-click edits its time, state or value cell. Enter commits, Escape cancels. Validate the whole edit before publication. Originally edits needed stopped playback and REC off; C5 now permits recorded-event edits while playing, paused, recording or under Perform.

Delete selected events without a confirmation dialog. Show the deleted count and an Undo action. Each edit or batch deletion has one undo unit tied to its Show ID. Undo/redo obey the same edit gate and restore only that edit's ID-addressed delta, preserving later recordings and equal-time order. Reject conflicting replay without changing data or consuming history.

### Musical movement

| ID | Requirement |
|---|---|
| C3-1 | Offer movement steps of one bar, half a bar and a quarter bar. Left/Right moves selected events by one shared relative time delta, preserving spacing, target/value data and equal-time order. Moving an event away from and back to its original timestamp shall restore its original order among equal-time events, including unselected peers, without requiring Undo. Do not intercept arrow keys while editing a cell. |
| C3-2 | A separate Snap action aligns the earliest selected event to the current Show grid and applies the same delta to the group. Use the Show's tempo, meter and grid offset. Reject the whole edit if any result is out of range; one move/snap has one undo unit. |

Keep the existing single event list; the editing gate is the one C5 defines. If no valid musical grid is available, explain why musical actions are unavailable; exact time editing remains available. Snap changes the group anchor, not each event's spacing.

## Recording workflow and saving

| ID | Requirement |
|---|---|
| C2-1 | REC enables capture without starting or moving playback. Capture at the current cursor even while stopped or paused; with no Show, bind the first valid resolved Show. Shared REC controls show the same target and transport state. While REC is armed, continue past the old end. On disarm, finalize the end at the latest remaining item, including existing clips and retained commands, without padding. |
| C2-2 | While REC is armed with no bound Show, allow the first manual Show selection; then lock manual selection while bound, but allow other view navigation. Automatic DJ handover finishes capture on A and rearms on B with REC intent retained. Pre-boundary input belongs to A; later input belongs to B. If A cannot finalize, disarm and report, preserve its accepted data, and leave B playback/live controls working until the operator resolves the error and rearms. |
| C2-3 | Save one snapshot of events accepted through the save boundary and the content-derived end, leaving REC/playback unchanged. Runtime keepalive can advance beyond that end without padding the saved Show. Settle already-accepted deferred input, not future input. Later accepted events remain unsaved; failure retains pending work. |
| C2-4 | New/Open/Exit while REC is armed uses Save / Discard / Cancel with an active-recording notice. Cancel preserves the current session. On proceeding, finish capture before saving/discarding and replacing/closing the workspace; failed saving must not proceed or lose pending work. |

## Debug panel

| ID | Requirement |
|---|---|
| C2-5 | Open one shared docked bottom debug panel from Show editor, DJ or the shared REC control. Keep the performance view visible. Its Events and Referenced controls tabs are read-only. Collect bounded, memory-only event history while the whole panel is open, regardless of tab. Closing/hiding it stops capture and discards history; reopening starts fresh. |
| C2-6 | Show a nonmodal Problems indicator beside REC with a count and latest failure reason. Clicking opens the relevant debug view; do not interrupt each failure with a popup or toast. While the panel is closed, retain only this summary of failed operations, not detailed event history or a background control scan. |

Opening the panel begins detailed capture from that moment; the Problems summary cannot reconstruct earlier event history. Command-track edits are observed for the selected recording target only; programmatic changes to other Shows do not appear.

## Referenced controls

| ID | Requirement |
|---|---|
| C1-1 | Before playback, list every unique persistent VC identity referenced anywhere in the selected Show. Group repeated references and expected roles. Keep the inventory independent of event-log retention or clearing; legacy function commands do not imply VC references. |
| C1-2 | Show configuration suitability using the replay resolver: Ready, Missing control, Ambiguous identity, Incompatible type/role, Disabled, or Unbound/missing binding where applicable. Preserve multiple per-role issues. Ready does not mean running, connected hardware or verified output. |
| C1-3 | While the inspector is visible, refresh after command-track changes and VC add/remove/rename/rebind/mode/enable changes, including binding-target availability. Leave authored gestures unchanged. |
| C1-4 | On whole-panel close/hide, unload or workspace reset, detach reference-view observation and reject stale callbacks. Reopen against current configuration. Playback must still validate destinations with the inspector closed. |

The read-only table shows control caption or an available recorded hint, persistent identity, expected roles, current binding, reference count and status reason. Provide All/Problems counts, a Problems filter and missing-first ordering. Virtualize rendered rows without truncating the reference inventory.

Level-channel and GrandMaster controls need no Function target. Keep missing gestures intact; optional Locate navigation must not play, delete, fix or rebind controls.

## Native feedback correction

| ID | Requirement |
|---|---|
| C4-1 | Adjust-slider attribute and stopped feedback shall update the displayed value and controller feedback without scheduling another lighting write. Preserve explicit user/replay output and the existing function-running reapply. This changes live behavior as well as recording. |
| C4-2 | A feedback value accepted with no pending write shall become the cancellation rollback baseline without creating a write generation. Preserve genuine pending-input suppression and newer user/input values when cancelling replay. |

## Live recording time editing (C5, October 2026)

The user confirmed this contract after the sampling and display-only cleanup proposals were withdrawn. Recorded events (not clips or Function definitions) stay editable while the Show is stopped, playing, paused, recording or driven by VirtualDJ Perform; Perform keeps play, pause, seek and loop.

| ID | Requirement |
|---|---|
| C5-1 | Move the exact table/group selection by one delta, or stretch either edge with the opposite edge pinned, scaling selected times proportionally to the millisecond. Keep every value, id and Order; unselected events stay, overlap is allowed, nothing ripples, samples or merges. |
| C5-2 | Refuse the whole edit when a stretch has no source or target span, a time leaves 0..end of time, or rounding reverses the selected (time, Order) sequence. |
| C5-3 | Preview while dragging; release commits one Show-scoped ID-delta undo step; Escape cancels. A press freezes the Show, ids and each event with its Order. Captures meanwhile keep their own data and never join; a changed or deleted target, a Show/workspace replacement or a selection change ends the edit with a reason. Table cell drafts get the same protection. |
| C5-4 | A committed edit publishes at once. The playhead processes what it reaches as currently published; edits behind it have no retroactive effect, a passed event moved ahead plays again, and already crossed, queued or settling work finishes as accepted. No edit seeks, resets the traversal or replays a prefix. |
| C5-5 | Live input is not echoed at its captured occurrence; the same event moved elsewhere plays there. No per-pass played-id set. |
| C5-6 | Undo/Redo edit stored data under the same rule, keep later captures, refuse true per-id conflicts without consuming history and reverse no physical action. Under Perform only the next history entry is considered: a recorded-event edit of the shown Show. |
| C5-7 | Delete and record again are separate operations; capture stays additive. No punch-in/out. |
