# Record VC commands through current bindings

Status: accepted design, implementation pending. Updated: 2026-09-25.

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

## Use internal messages

Borrow OS2L's small action/value vocabulary, but retain the Show clock and command storage. The [OS2L specification](https://os2l.org) defines mapped command IDs, named button press/release messages and beat synchronization, not recording persistence or seeking.

The [local OS2L plugin](../../plugins/os2l/os2lplugin.cpp) hashes button names into input channels and converts command parameters to byte values. Reinjection would add mapping and conversion work without providing a persistent VC destination.

## Persist control identity

Numeric widget IDs can be reused after deletion. Assign a persisted recording identity to referenced controls using Qt UUID support; copies receive fresh identities. Keep existing numeric IDs for UI internals, and reject ambiguous persistent identities.

This prevents a deleted destination from redirecting playback to an unrelated replacement. Source: [`VirtualConsole::newWidgetId`](../../qmlui/virtualconsole/virtualconsole.cpp) and [VC widget XML handling](../../qmlui/virtualconsole/vcwidget.cpp).

## Preserve legacy recordings

Add versioned, discriminated VC commands beside existing function commands. Old records lack a control identity, so automatic conversion would guess their destination. Preserve legacy-only behavior; a mixed recording's catch-up executes its entries in saved order.

## Keep editing and diagnostics distinct

The user chose a Recordings tab in the Show editor and rejected extra lanes. A shared docked debug panel supplies read-only Events and Referenced controls; it is not a second editor.

REC arms capture independently of Play. Save checkpoints the current capture without interrupting busking. Performance views share the same target/status controls so the operator can disarm without leaving the VC.

## Record through a DJ set

The user chose automatic recording handover when VirtualDJ changes Shows: finalize A and rearm on B, while manual Show selection stays locked. This replaces the earlier fixed-target/suspend proposal for automatic handovers.

REC permits runtime growth past an old Show end. Save and finalization use the latest remaining clip or command, so waiting before disarming does not add saved duration.

## Correct feedback instead of simulating its loop

The user approved a live-behavior fix: Adjust-slider attribute/stopped feedback must not schedule a write-back. A production-linked probe showed adjacent legacy 20%/80% writes ending at 20% because the slider echoed the first feedback value. Display/controller feedback and the separate function-running reapply stay intact.

This avoids extending seek restoration to simulate the feedback-write loop. Requantized Submaster echo expectations must change to the requested value; pending replay cancellation must restore the latest accepted display baseline.
