# Record VC states through current bindings

Status: accepted design, implementation pending. Updated: 2026-09-23.

[Requirements](../vc-performance-recording/requirements.md)

## Capture after input mapping

Record accepted control states so mouse, keyboard and MIDI share a recording format. Raw MIDI would repeat profile, pickup and relative-input interpretation during replay. Resolved function commands remain useful for fixed targets, but do not preserve the behavior of every VC slider mode.

The user chose current control bindings and native behavior. That choice permits effects outside the Show, including SoloFrame sibling stops and global fader changes. It does not promise the original output after controls or their configuration change.

## Store desired states, not relative clicks

The requirements interview replaced `ActivateToggle` with desired On/Off state. Replaying a click could turn an already-on button off; state application follows the [Active/Monitoring/Inactive rules](../vc-performance-recording/requirements.md#button-state-application).

Store slider position as `0..1` so a range change preserves its relative position. Capture accepted VC changes rather than waiting for native output coalescing.

## Preserve live busking

The user chose no automatic initial snapshot and no reset on Pause or Stop. A seek restores known history; controls without prior values retain their current state. [Console research](../timecode-busking-research.md) did not establish a universal baseline rule, so this is a product choice.

Explicit seek intent and late clock updates require different FSM transitions. The first release uses QLC+ timeline seek requests; the current VirtualDJ connector does not identify seek intent, so that integration remains deferred.

## Use internal messages

Borrow OS2L's small action/value vocabulary, but retain the Show clock and command storage. The [OS2L specification](https://os2l.org) defines mapped command IDs, named button press/release messages and beat synchronization, not recording persistence or seeking.

The [local OS2L plugin](../../plugins/os2l/os2lplugin.cpp) hashes button names into input channels and converts command parameters to byte values. Reinjection would add mapping and conversion work without providing a persistent VC destination.

## Persist control identity

Numeric widget IDs can be reused after deletion. Assign a persisted recording identity to referenced controls using Qt UUID support; copies receive fresh identities. Keep existing numeric IDs for UI internals, and reject ambiguous persistent identities.

This prevents a deleted destination from redirecting playback to an unrelated replacement. Source: [`VirtualConsole::newWidgetId`](../../qmlui/virtualconsole/virtualconsole.cpp) and [VC widget XML handling](../../qmlui/virtualconsole/vcwidget.cpp).

## Preserve legacy recordings

Add versioned, discriminated VC state records beside existing function commands. Old records lack a control identity, so automatic conversion would guess their destination. Preserve their existing target, ownership and seek behavior.

## Keep editing and diagnostics distinct

The user chose a Recordings tab in the Show editor and rejected extra lanes. A shared docked debug panel supplies read-only Events and Referenced controls; it is not a second editor.

REC arms capture independently of Play. Save checkpoints the current capture without interrupting busking. Performance views share the same target/status controls so the operator can disarm without leaving the VC.
