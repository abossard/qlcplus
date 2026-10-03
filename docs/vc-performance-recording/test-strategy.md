# Test strategy

[Requirements](requirements.md) · [Principles](core-principles.md) · [Implementation plan](implementation-plan.md)

The implemented feature has passed native/offscreen integration verification at three public seams:

1. Authored data: capture, edit, save and reload identity, time, order and values.
2. Pure FSM: transport/input events produce next state and explicit effects.
3. Native VC integration: accepted input, state application, ownership, diagnostics and QML behavior.

The first native tracer is a Collection starting a Scene whose button is Monitoring: recorded ON makes it Active, and the Scene remains active after the Collection stops. Future changes should retain this proof and use public red/green slices with existing Qt `_data()` / `QFETCH` patterns.

| Criterion | Interface and decisive cases |
|---|---|
| C0-1 | Equivalent pointer/MIDI/OSC commands through their accepted routes; relative-input conversion and rejected pickup. Submit 20%, 80%, 40% before one engine tick: retain three timestamped input commands in order, regardless of deferred output; exclude an unchanged duplicate. Property/function feedback alone adds no record. |
| C0-2 | Audio/Programmatic/Replay and Collection-child negatives. Input accepted before Record-off survives late delivery; input accepted after it adds nothing. Add 50% at 12s to a take containing 20% at 10s and 80% at 15s: retain the 15s event. |
| C0-3 | Parameterize the six native-state/desired-state pairs. Verify Monitoring+ON survives Collection stop, Monitoring+OFF stops, and matching states cause no startup/SoloFrame effects. A recorded halfway position maps from value 25 in range 0..50 to 50 in range 0..100 after rebinding. |
| C0-4 | Delete/recreate numeric IDs, copy/reload controls, disable a target and change type/action/mode/attribute role. In particular, an intensity slider changed to GrandMaster must skip with a reason. Assert the Show continues valid events without substituting a target. |
| C0-5 | XML round trip with Version 1 fixtures, mixed records and equal timestamps. Reject malformed values, unknown versions/actions and ambiguous identities without replacing valid data. Compare legacy targets, ordering and behavior. |
| C0-6 | While Playing, advance from 9s to 12s across ON at 10s and OFF at 11s: process both, including after a late update. Include slider/button barriers and equal timestamps; observe native effects as well as request order. |
| C0-7 | Paused/stopped cursor movement causes no effect. Superseded by C1 for seeks: Play at a cursor dispatches nothing before it (`play_fromStoppedCursor_runsOnlyFromTheCursor`); a requested forward seek plays its crossed interval in saved order, A ON, B ON, B OFF leaving both Off through the native SoloFrame (`jump_*`); a backward move rolls back only its window (`timelineRollback_data` rows a-o, `showrunner_test::commandJumpDispatchesOnlyItsWindow_data`, `commandBackwardRollsBackOwnLegacyEffects_data`). Cover mixed ties, exact-destination once, cancellation/end, and resume without repositioning. |
| C0-8 | Cross/queue 30%, request Pause, then submit conflicting manual 55%: preserve its accepted time/value and apply it after older work, never before stale replay on Resume. An unrelated control still responds. Include late-posted batches, stop/seek/unload cancellation, final-event drain and no duplicate destination effect. |
| C0-9 | Test select/double-click/Enter/Escape and immediate counted batch delete. Delete three events, record five, disarm, then undo: retain the five and restore the three in their original tie order. Test another selected Show, changed/deleted IDs and conflict rejection without consuming history. The original undo/redo stopped gate is replaced by C5: `recordingsEditor_editsWhileLive_data` edits, undoes and redoes in every playing/paused/REC state. |
| C0-10 | Pure FSM transitions: equal inputs yield equal next state/effects; rejected input preserves authored data. Confirm the pure interface carries no QObject or live engine access. |
| C1-1 | Five gestures referencing three controls with counts `3/1/1`, duplicate captions with distinct identities, mixed status and no references. Exceed the event-log cap, then clear/evict log entries; the complete reference inventory remains queryable. |
| C1-2 | Compare the shared resolver's view and replay results for each status and multiple expected roles. Include valid Level-channel and GrandMaster controls without Function bindings. |
| C1-3 | Use real add/delete/rename/rebind/mode/enable and binding-target mutations with native Qt notifications. Restore the original identity and copy a control. Verify status/count changes without changing gestures or triggering replay. |
| C1-4 | Close/hide the whole panel, mutate configuration, reset the workspace and reuse a Show ID. Assert no closed-view updates, fresh state on reopen, discarded stale callbacks and continued playback validation while closed. |
| C2-1 | Arm without transport movement, including no Show and paused/stopped capture. Pass the prior extent while armed, then disarm after waiting: end at the latest retained clip/command, not the button-press time. Preserve earlier content, test a shorter loop pass, and execute a final ON before Show completion. |
| C2-2 | While armed but unbound, allow the first manual Show selection; reject subsequent manual selection while bound and allow VC navigation. Handover A to B settles A's pending input, then records B with its own clock. Test first resolution, a gap and rapid changes. Inject A finalization failure: preserve its accepted data, disarm/report once, and leave B playback/live controls working without recording into A. |
| C2-3 | Save while Playing/Paused with accepted input still awaiting publication. With the last retained item at 40s and armed clock at 55s, save/reopen with end 40s while live recording continues. New input after the checkpoint stays dirty without changing REC/playback; failed saving retains pending data. |
| C2-4 | Exercise New/Open/Exit with REC armed through the existing dialog. Cancel preserves target, events and transport. Save/Discard settles the old session before replacement; late callbacks cannot enter the new workspace. Failed save leaves the old session available. |
| C2-5 | Open the same docked panel from each launcher. Switch tabs while events arrive and verify bounded history remains. Close/hide, deliver late callbacks, then reopen: no closed-period events or backfill. Reference inventory remains independent of log eviction. |
| C2-6 | Produce repeated skipped/unsupported operations with the panel closed. Check the failure count/latest reason, continuing playback and absence of per-event popups or log rows. Open through the indicator: details start at open, with no fabricated pre-open history. |
| C3-1 | Native list keyboard input with bar/half/quarter steps. At 120 BPM in 4/4, move 1.2s and 1.7s forward one bar to 3.2s and 3.7s. Parameterize meter and step. Move a subset away and back while leaving tied peers unselected; verify both original before/after orders return exactly without Undo, including non-monotonic event IDs. Leave cell-edit arrow keys alone. |
| C3-2 | Snap a group whose points have off-grid spacing using a nonzero grid anchor. Assert one common delta, unchanged spacing/values and one undo. Cover missing tempo/grid, first event before zero after movement, time overflow, stale selection and repeated forward/back nudges without accumulated drift. |
| C4-1 | Native Adjust feedback with recording off: adjacent legacy 20%/80% writes end at 80%; one feedback update does not produce a slider-sourced Solo notification. Stop with a 100..200 slider range does not write a negative attribute. Keep normal input/replay writes and explicit function-running reapply. Check display and controller feedback. |
| C4-2 | Apply 80%, receive nonpending 20% feedback, queue 60% replay, then cancel: restore the 20% display baseline, not 80%. Preserve newer input, pending suppression, matching replay and Level monitoring. Replace old Submaster requantization expectations with exact requested intensity under C4-1. |

Use unequal times and values to expose ordering or aggregation errors. Check both accepted data and native side effects. Mutating a test snapshot or emitting a synthetic notification does not prove mutation-to-view delivery.

A test MIDI plugin proves the application mapping path, not physical controller timing. Serializer equality does not prove pickup or thread safety, and an invocation trace does not prove native effects. Explicit VirtualDJ seeks remain outside this release; do not claim their behavior from synthetic position updates.

The old D projection probes remain historical evidence. Requirements for inferred SoloFrame state, conditional preparation and backward-seek suppression of prefix effects were superseded by the user's serial-execution decision, not repaired by changing test expectations.

## Recording timeline extension

`showcommandtrack_test::groupsPartitionAndRegroup` verifies a literal mixed
stream with unequal runs, non-monotonic IDs, equal-time barriers, attribute
changes and long gaps. Both authored and XML-round-tripped tracks split/merge
after retime/delete/restore without changing raw samples or tie order.

Production-QML tests in `showcommandrecorder_test` cover accessible group
selection and sample drilldown, one-sample value editing, partial selection
after regrouping, musical move/Snap and Undo, and pointer drag commit/cancellation (a
changed target cancels; REC starting no longer does, per C5). Existing public recorder edit/gate/history tests cover
atomic bounds refusal, stale Show/IDs and later-capture preservation.
The parent-editor observation test follows controls across both tabs and
stops observing when the whole Show editor is hidden.

Offscreen tests do not establish Cocoa input or visible layout. Native proof
uses the normal app on a task-owned workspace, unique AX targets and guarded
foreground CGEvent input, saved XML and window-only screenshots. Original
workspaces, routing and open-only diagnostic behavior must remain untouched.

## Live recording time editing (C5)

Engine: `showcommandtrack_test::retimeSelection_data` (exact anchors, rounding ties, reversal refusal, full 32-bit range with 64-bit products) and `liveOccurrence_data`; `showrunner_test` live-occurrence, moved live Stop, once-only retirement, edit-while-playing (per-tick old-or-new snapshots, unchanged traversal) and crossed/settling work rows. Recorder/App: `recordingsEditor_editsWhileLive_data`, `recordingsEditor_pasteWhileLive_data`, `recordingsEditor_editKeepsPendingCapture_data`, `recordingsEdit_previewCommitCancel_data`, `recordingsEdit_conflicts_data`, and the UI caller tests for drafts surviving capture, drag surviving capture, the span surface and handles, refused release, key continuation, press freezing, follow pause and Perform via the VdjBridge inputs. Mutation runs record which rows fail when each rule is removed. Native VirtualDJ and the real app remain a separate verification.
