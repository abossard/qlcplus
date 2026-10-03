# Record VC commands through current bindings

Status: implemented; native/offscreen integration verified. Updated: 2026-09-26.

[Requirements](../vc-performance-recording/requirements.md)

## Capture after input mapping

Record normalized input commands so mouse, keyboard, MIDI and OSC share a recording format. Capture before VC execution rather than observing its property changes. Raw MIDI would repeat profile, pickup and relative-input interpretation during replay.

The user chose current control bindings and native behavior. That choice permits effects outside the Show, including SoloFrame sibling stops and global fader changes. It does not promise the original output after controls or their configuration change.

## Store desired states, not relative clicks

The requirements interview replaced `ActivateToggle` with desired On/Off state. Replaying a click could turn an already-on button off; state application follows the [Active/Monitoring/Inactive rules](../vc-performance-recording/requirements.md#button-state-application).

Store slider position as `0..1` so a range change preserves its relative position. Capture accepted VC changes rather than waiting for native output coalescing.

## Preserve live busking

The user chose no automatic initial snapshot and no reset on Pause or Stop. Recorded commands act through the current controls. [Console research](../timecode-busking-research.md) did not establish a universal baseline rule.

Explicit seek intent and late clock updates require different FSM transitions. The first release uses QLC+ timeline seek requests; the current VirtualDJ connector does not identify seek intent, so that integration remains deferred.

## Execute the prefix instead of reducing it

The user rejected special reduction: “yes, no special reducer, it should just apply it in series.” Paused cursor movement has no effect. Play at the new position executes the recorded prefix through normal controls, then continues from that position.

This removes the duplicate SoloFrame/ownership/feedback model. Earlier effects may activate during catch-up; avoiding those intermediate effects is no longer the seek contract.

Superseded on 2026-10-02 by "Seek without history catch-up" below.

## Use internal messages

Borrow OS2L's small action/value vocabulary, but retain the Show clock and command storage. The [OS2L specification](https://os2l.org) defines mapped command IDs, named button press/release messages and beat synchronization, not recording persistence or seeking.

The [local OS2L plugin](../../plugins/os2l/os2lplugin.cpp) hashes button names into input channels and converts command parameters to byte values. Reinjection would add mapping and conversion work without providing a persistent VC destination.

## Persist control identity

Numeric widget IDs can be reused after deletion. Assign a persisted recording identity to referenced controls using Qt UUID support; copies receive fresh identities. Keep existing numeric IDs for UI internals, and reject ambiguous persistent identities.

This prevents a deleted destination from redirecting playback to an unrelated replacement. Source: [`VirtualConsole::newWidgetId`](../../qmlui/virtualconsole/virtualconsole.cpp) and [VC widget XML handling](../../qmlui/virtualconsole/vcwidget.cpp).

## Preserve legacy recordings

Add versioned, discriminated VC commands beside existing function commands. Old records lack a control identity, so automatic conversion would guess their destination. Preserve legacy-only behavior; a mixed recording's forward jump executes its entries in saved order.

## Keep editing and diagnostics distinct

The initial editor used a Recordings tab without extra lanes. The September 2026 recording-timeline request supersedes that presentation choice: one Recordings lane displays derived groups alongside function clips, while the table remains the sample editor. Adjacent slider samples join only when control UUID, role and attribute match; every other saved command is a barrier. Regrouping changes no samples or XML and never widens the selected event IDs. A shared docked debug panel supplies read-only Events and Referenced controls; it is not a second editor.

The user requires stable equal-time order: moving an event away and back restores its original place among tied events, including unselected peers. Appending every moved event after existing ties was rejected; Undo alone does not satisfy this rule.

REC arms capture independently of Play. Save checkpoints the current capture without interrupting busking. Performance views share the same target/status controls so the operator can disarm without leaving the VC.

## Record through a DJ set

The user chose automatic recording handover when VirtualDJ changes Shows: finalize A and rearm on B. If REC is armed without a bound Show, allow the first manual selection, then lock manual selection while bound. This replaces the earlier fixed-target/suspend proposal for automatic handovers.

REC permits runtime growth past an old Show end. Save and finalization use the latest remaining clip or command, so waiting before disarming does not add saved duration.

## Correct feedback instead of simulating its loop

The user approved a live-behavior fix: Adjust-slider attribute/stopped feedback must not schedule a write-back. A production-linked probe showed adjacent legacy 20%/80% writes ending at 20% because the slider echoed the first feedback value. Display/controller feedback and the separate function-running reapply stay intact.

This avoids extending seek restoration to simulate the feedback-write loop. Requantized Submaster echo expectations must change to the requested value; pending replay cancellation must restore the latest accepted display baseline.

## Edit recorded events while the Show plays (2026-10-01)

The recording editor first required stopped playback and REC off, and later proposals thinned or sampled dense slider takes. The user withdrew those proposals and confirmed a different contract: keep all recorded data, let recorded events be moved, stretched and edited while the Show plays, pauses, records or follows VirtualDJ Perform, and let the playhead process what it reaches as currently published.

The existing time cursor and revision snapshots already give that behaviour, so no staged timeline, parallel track or per-pass played set was added. Live no-echo became occurrence-specific (an id at its captured time) so a moved event can play again; a live Stop's clip end is retired once per captured occurrence. Edits publish through the same non-stopped path as capture; work already crossed or settling finishes as accepted. One recorder-owned edit session freezes the edited events and refuses changed targets instead of watching a global revision. Perform keeps transport; recorded-event edits and their Undo stay available, other history stays frozen.

## Seek without history catch-up (2026-10-02)

The user rejected the prefix catch-up while looping a song in VirtualDJ: "catch up from the very beginning of the song" is wrong, forward jumps replay "from the start position of the jump to the end position", and a button recorded On at 12 s in a loop 8-14 s must be Off again at the wrap. Lighting consoles have no universal backward rule (Hog, MagicQ, Avolites, grandMA).

Forward plays the crossed interval. Backward replays nothing before its target; instead each side returns what this traversal's own timeline operations changed after the target: the recorder keeps an ordered journal of control states before and after each replayed op and each REC capture (with the Solo Frame and Submaster closure), the runner one of its legacy Start/Stop/SetIntensity effects. A rollback applies, per control, the state before its first journaled change after the target, as one delta, Offs first; a control whose state differs from its last journaled one was changed outside the timeline and wins in every Solo Frame its start would reach; a button another Active button holds by the same Function returns to Monitoring instead of stopping that Function. Play from T dispatches nothing before T; under an external clock T is the earliest sample set after Play. The legacy value restore on seek is removed for every recording kind.

The journal is not keyed by event id and suppresses nothing, so it is neither a per-pass played set nor a simulated engine. Its limit: a traversal knows native states only from its first operation on, so a wrap before where it began returns controls to the earliest state it observed.
