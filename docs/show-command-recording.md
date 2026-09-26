# Show command tracks

A command track stores timed instructions alongside a Show's ordinary function
clips. Legacy commands reference functions by ID. VC commands reference a
persistent control identity and replay through its current compatible binding,
so rebinding that control affects VC replay but not legacy commands.

For the complete behavior contract, see
[VC performance recording](vc-performance-recording/requirements.md).

## Commands

| Action | Value | Playback |
|---|---|---|
| `Start` | None | Start the target through the Show. |
| `Stop` | None | Release the Show's ownership of the target. Other owners may keep it running. |
| `SetIntensity` | A number from `0` to `1` | Apply the intensity without starting a stopped target. |
| `SetButtonState` | On or Off | Apply the desired state through the current compatible Toggle control. |
| `SetSliderPosition` | A number from `0` to `1` | Map the accepted position into the current compatible slider range. |

Recording accepts user commands, not audio-mapped changes, playback feedback or
generic property changes. It does not inspect running functions to reconstruct a
performance. The existing engine executes each command.

Manual intervention remains possible. If an operator stops a function before a
later intensity command, that command does not restart it. Replaying commands
does not guarantee identical output from random or audio-reactive functions.

## Saved format

The optional `CommandTrack` element belongs inside the Show's `Function` element
in the workspace XML. Times and `Extent` use elapsed milliseconds, independently
of the Show editor's ruler. `Extent` is the command-track playback end. Recording
checkpoints, finalization and editor changes derive it from retained commands;
the Show also retains any later clip endpoint. Legacy files may contain an
explicit longer extent before those boundaries normalize it.

Legacy-only tracks use `Version="1"`. Tracks containing VC commands use
`Version="2"`, with `Control` and `Role` plus `State` or `Position`.

```xml
<CommandTrack Version="1" Extent="12000">
  <Command ID="1" Time="1000" Action="Start" Function="12"/>
  <Command ID="2" Time="1500" Action="SetIntensity" Function="12" Value="0.25"/>
  <Command ID="3" Time="4000" Action="SetIntensity" Function="12" Value="0.8"/>
  <Command ID="4" Time="10000" Action="Stop" Function="12"/>
</CommandTrack>
```

Event IDs are unique within the track. Equal-time commands preserve their stored
order. Each command keeps its equal-time place when it moves, so an event moved
away and back sits where it was. When the file order alone cannot express that
place (for example after an earlier event was moved later), every command
carries an `Order` attribute; otherwise the document is written exactly as
before, and a document without `Order` uses its file order. Unsupported
versions/actions and invalid values, including `Order` on only some commands or
used twice, are errors; they must not load as an empty successful track.

Edit XML only while the workspace is closed or saved elsewhere. Keep the target
function definitions in the same workspace. A function reference does not freeze
that function's definition.

## Position changes

Forward clock movement applies the crossed commands in their recorded order.
A forward jump and a delayed position update use the same path; playback does
not guess which caused the new position.

Moving the cursor while paused or stopped applies nothing. On Play at the moved
cursor, a VC or mixed recording executes its prefix through the destination
once, in stored order, using normal native controls. Mixed prefixes include
legacy commands. Resume without moving the cursor does not repeat the prefix.
An explicit local seek while playing uses the same serial catch-up.

Legacy-only tracks, and tracks without a VC executor, retain their legacy seek
behavior: restore the latest preceding intensity values without firing
historical Start/Stop commands. Destination commands remain eligible once;
missing earlier values do not imply a reset. Ordinary function clips retain
their native seek behavior. External clock updates are not explicit VirtualDJ
seek intent; a backward external clock jump follows the native seek path,
including serial catch-up for VC or mixed tracks.

## Recording and editing

The Show Timeline includes one **Recordings** lane after the function tracks.
Consecutive saved slider samples with the same control UUID, role and attribute
form one object. Every other saved command breaks the run, including a different
slider or a legacy command. Time gaps and recording off/on do not break a run
unless another command was stored. Groups are typed `ShowCommandGroup` values
derived by `ShowCommandTrack::groups()`, not new playback tracks or XML objects.
All samples, values and equal-time ordering remain unchanged.

Click an object to select its samples, Ctrl/Meta-click to toggle membership, or
Shift-click to extend in command order. Tab reaches coincident objects; Space
selects and Enter or double-click opens the samples in the Recordings table.
Drag moves the selected samples horizontally by a common millisecond delta.
The shared **Move by** chooser offers Bar, Half bar, Quarter bar and an explicit
**Time delta (s)**. Enter seconds with up to three decimal places. Numeric
movement works in Time, BPM and VDJ Beat, without a musical grid; musical
movement and Snap still require a valid grid. Points have a minimum hit width, not
a synthetic duration. Objects cannot be resized or muted.

Edits recalculate adjacency, so groups may split or merge. Selection remains the
exact event IDs originally selected, with a partial-selection border when only
some members of a newly merged group are selected. A drag is cancelled when its
data, ruler, Show or edit permission changes. Delete reports the sample count and
offers the existing Show-scoped Undo. Function-clip selection is separate.

### Keyboard editing

- Tab and Shift+Tab traverse controls and timeline selection affordances. Focused
  offscreen objects scroll into view. Space on a clip's visible `+`/check control
  selects it using the existing multiple-selection setting and Shift modifier.
  Pointer selection leaves Space available for Play/Pause.
- Recording Space selects samples. Enter reveals the exact selected members.
  Tab reaches row selection and editable Time/Value cells; F2 starts the existing
  editor, Return commits and Escape cancels its draft. The Timeline tab restores
  the surviving originating sample IDs, even after regrouping.
- Left/Right moves the exact selected clip or recording set by the displayed
  amount. Invalid amounts and absent grids show a reason beside the keyboard-
  reachable chooser. Holds retain repeated edits and existing history.
- Option/Alt+Left/Right adjusts only finite, explicit ordinary clip scheduling
  durations. Starts, referenced Functions and other instances do not change.
  Recording samples have no scheduling endpoint; the operation is refused.
- Mixed Time/Beats clips use the Show tempo and nearest native units. Movement
  and endpoint batches validate every target, bounds, locks, minimum durations
  and final overlap before publishing any changes. Ordinary edits retain their
  existing permissions; recording edits still need stopped playback and REC off.
- Timing Settings opens the existing timing controls. Their spinbox changes
  publish immediately; Escape closes a timing field without rolling back an
  already published edit. Ctrl+] only toggles the currently loaded panel.
- Local key presses, repeats and releases stay with their original owner instead
  of also activating VC mappings. Fields, REC, buttons and Show-local dialogs keep
  their local keys. Global Save/Undo and other contexts retain their existing paths.

Ordinary Copy/Paste, Delete and native Undo/Redo remain available. Recording
clipboard/clone operations and keyboard drag/drop placement are not implemented.

The Show Manager, DJ view and Virtual Console share one Record switch, which
shows the take's Show and whether it is playing, pausing, paused or stopped.
Record arms capture only; it never plays, pauses or seeks. REC waits for a Show
if none has resolved yet, then binds the first resolved Show, such as the first
one selected in the Show Manager. When Perform resolves another Show, the
take on the previous one closes and REC continues on the new one; if the previous
take cannot be saved, REC turns off and displays why. While a take is bound, the
Show Manager keeps its Show selected.

Toggle buttons, including those in Solo Frames, record their desired On/Off
state. Level, Adjust, Submaster and Grand Master sliders record their position.
Input counts from the screen, keyboard bindings or a patched MIDI or OSC input,
stamped when it is accepted. The stamp is the cursor moved on a stopped or
paused Show; otherwise it is the last Show time playback processed. That can be
one engine tick (20 ms by default) earlier than the time display, which already shows the
next tick, including while paused. An external clock is always its own
position. Every valid Show time is kept, up to the format's maximum. The debug
panel's Show time for transport rows uses the same clock. Flash and
global-action buttons and the slider reset/flash buttons and inputs are not
recorded; they keep working live and display a reason.

The Show editor's Recordings tab lists the selected Show's events in order:
time, control (caption, identity and current binding, or the function of a
legacy command), action and value. Missing or disabled controls stay listed with
the reason. A click selects a row, Ctrl or Shift extends the selection, and a
double click edits the time (seconds), state (On/Off) or value (percent); Enter
commits and Escape cancels. Delete removes the selection at once and offers
Undo. Left/Right move the selection by a bar, half a bar or a quarter bar, and
Snap puts its earliest event on the nearest beat, the others by the same delta.
Both need BPM markers with a tempo or a VDJ Beat grid. Editing, undo and redo
need REC off and playback fully stopped; the Show makes that decision as it
stores the edit, together with accepting starts, so a start that wins refuses
the edit. Each edit is one undo step of its Show that restores only its own
events and refuses if they changed since. The end of the Show follows its last
retained command or clip. Visible rows follow renamed, rebound, enabled/disabled or removed
controls and renamed functions. Editor steps are not sent to connected
network clients. The workspace Save
operation stores the track with its Show, including input accepted until the
save, while REC and playback continue. The saved end is the last recorded
command or clip; time spent with REC armed is not saved. If the recording
cannot be stored, the save fails and keeps it. New, Open and Exit ask to save or
discard while REC is armed, even without other unsaved changes.

Recording is not an undo history of raw widget changes. Programmatic changes and
audio mappings do not create events, and a recorded Collection launch does not
expand into events for each child.

## Extending the format

Add a typed command only when its validation, serialization, playback and seek
behavior are defined. Do not serialize widget clicks or arbitrary executable
strings. Level and Grand Master sliders already use `SetSliderPosition`, not
function-intensity conversion. Flash, cue navigation, speed and XY controls
need defined replay semantics before recording support is added.
