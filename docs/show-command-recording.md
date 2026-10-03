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

Moving the cursor while paused or stopped applies nothing. The earlier prefix
catch-up (replaying everything from 0 on Play or seek) and the legacy seek value
restore are superseded by correction C1 (2026-10-02). Position changes follow
three rules, for every recording kind:

- **Forward:** a requested forward seek plays the commands from the current
  position to the destination once, in stored order, through native controls;
  a VC or mixed recording plays them serially (each legacy Start/Stop settles,
  each VC run is acknowledged). Live no-echo marks and command-owned Functions
  stay. A forward external clock jump plays the same crossed interval.
- **Backward** (requested seek, backward external clock jump, a VirtualDJ loop
  wrap, a seek to 0): nothing before the destination is replayed. What this
  traversal's own timeline operations (replayed events and REC captures)
  changed after the destination returns to its native state before the first
  of those changes, as one delta, Offs before Ons; native couplings follow
  (Solo Frame, Collection). Controls changed outside the timeline keep their
  state, and win in their Solo Frame. Commands at the destination play again.
  Live marks of the previous pass are cleared.
- **Play from T:** nothing before T is dispatched; commands at T play once.
  Under an external clock, T is the earliest sample set after Play (a value
  present before Play is ignored); VirtualDJ Perform sets the deck position
  right after starting the Show.

The direction of a requested seek is decided when it is requested, against the
position the runtime last reported. Ordinary function clips retain their native
seek behavior. External clock updates are not explicit VirtualDJ seek intent.

## Recording and editing

The Show Timeline includes one **Recordings** lane after the function tracks.
Consecutive saved slider samples with the same control UUID, role and attribute
form one object. Every other saved command breaks the run, including a different
slider or a legacy command. Time gaps and recording off/on do not break a run
unless another command was stored. Groups are typed `ShowCommandGroup` values
derived by `ShowCommandTrack::groups()`, not new playback tracks or XML objects.
All samples, values and equal-time ordering remain unchanged.

The list button in the Recordings header expands the lane into one row per
recorded VC control and collapses it back; it starts collapsed and is not saved.
Rows are assigned after the global grouping, so a run split by another control
stays two objects in one row. Rows follow the order in which each control first
appears; legacy function commands share a **Functions** row, a missing control
shows "Missing control" with the first 8 UUID characters, and a UUID that
matches several widgets shows "Ambiguous control" with that suffix. Two controls
with the same caption both show the suffix, as does a control captioned
"Functions" next to the legacy row. Rows show live captions. Tab order stays
chronological, so Tab can move between rows. Toggling changes no selection,
passage, draft, data or history; a focused object keeps focus and scrolls into
view, and the button keeps focus when used from the keyboard. A selection count
too long for the header shrinks the "Recordings (n)" title instead of cutting it.

Tab and Shift+Tab walk the timeline in one order under every system Tab policy:
each track's name, Solo, Mute and Delete, then the Recordings header (the list
button, then the enabled Move earlier, Move later, Snap and Delete buttons), then
every object in chronological order and the Undo button when shown. Under the
reduced macOS policies (text controls, or text and lists) Qt skips these buttons,
so the Show editor walks the same order itself on one fixed route with two
distinct ends. In the Timeline tab the route runs from the tab bar's side through
the toolbar: the colour button, the marker choice, Zoom out, Zoom in, Fit, Go,
Move by, the Move by seconds field while Move by is "Time delta (s)", then the
Show name while it can be edited (disabled or hidden controls are skipped), then
the timeline, and leaves to the Timeline tab. Shift+Tab walks the same route
back from the Timeline tab. In Split the colour button sits right before the
timeline and the Recordings filter right after it, so Shift+Tab from its first
control returns to the colour button and Tab from its last goes to the filter.
While the Show is read-only (Perform) the colour button is disabled and, under
the reduced policies, the Split tab takes its place. Any of these
controls that takes focus scrolls into the timeline's view.

Click an object to select its samples, Ctrl/Meta-click to toggle membership, or
Shift-click to extend in command order. Tab reaches coincident objects; Space
selects and Enter or double-click opens the samples in the Recordings table.
Drag moves the selected samples horizontally by a common millisecond delta.
When the selection spans time, handles at its left and right edge stretch or
compress it: the opposite edge stays where it is and every selected time scales
proportionally, rounded to the millisecond. Values, ids and equal-time order
stay; unselected events stay put, even when the selection now crosses them.
Where an unselected object lies under a handle, a press there is that object's,
as on its body.
Nothing ripples and nothing is sampled or merged. A stretch that would leave no
time span, put an event before 0 or past the end of time, or reverse the order
of selected events by rounding is refused whole, with its reason; a selection at
one time can be moved or deleted but not stretched. While dragging, the lane only
previews; release applies the edit as one undo step and Escape cancels it.
The shared **Move by** chooser offers Bar, Half bar, Quarter bar and an explicit
**Time delta (s)**. Enter seconds with up to three decimal places. Numeric
movement works in Time, BPM and VDJ Beat, without a musical grid; musical
movement and Snap still require a valid grid. Points have a minimum hit width, not
a synthetic duration. Objects cannot be muted.

Edits recalculate adjacency, so groups may split or merge. Selection remains the
exact event IDs originally selected, with a partial-selection border when only
some members of a newly merged group are selected. A drag or stretch freezes the
selected events as they are when it is pressed: events recorded meanwhile,
also on the same control, keep their own times and values and never join it. It
ends with a reason, changing nothing, when one of its events is changed or
deleted, the Show or workspace is replaced, or the selection changes. A table
cell edit is protected the same way. Delete reports the sample count and
offers the existing Show-scoped Undo. Function-clip selection is separate.

A slider object draws its samples below its summary as the recorded slider
position (0 to 100 %) over Show time: each sample is marked at its time and
position and holds until the next sample, joined by vertical steps; nothing is
interpolated, and the position is not a claim about output, brightness or
function state. Selected samples are drawn in the partial-selection colour, and
an object whose control is missing, unbound or disabled is drawn dimmed. Where
several samples fall in one pixel column (zoomed out, or samples sharing a time)
the column shows only their range and no marks; the tooltip and the accessible
description then say the drawing is display simplified, since count and order
inside the column are not shown. Hovering an object adds its recorded position
range and the sample nearest the pointer to the tooltip. The drawing is read
only: clicks, keys and drags keep acting on the object, and only objects near
the visible part of the timeline hold a drawing.

### Keyboard editing

- Tab and Shift+Tab traverse controls and timeline selection affordances. Focused
  offscreen objects scroll into view. Space on a clip's visible `+`/check control
  selects it using the existing multiple-selection setting and Shift modifier.
  Pointer selection leaves Space available for Play/Pause.
- The Show toolbar wraps within the editor when feedback or an open panel
  leaves less room. Zoom in/out remains visible, is reachable by Tab and
  activates with Space. Track names and Solo/Mute/Delete controls form a
  forward/reverse Tab sequence and scroll into view when focused. With the
  platform's text-only or text-and-list Tab preference, that sequence joins the existing
  field traversal in both directions, including the visible Move-by amount.
  No system preference is changed. F2 edits the focused track name; Return or Tab confirms it and Escape
  cancels. Space activates a track button; Delete still asks for confirmation.
- Recording Space selects samples. Enter reveals the exact selected members,
  across every group they belong to.
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
  existing permissions; recording edits are also available while the Show plays,
  is paused, records or follows Perform.
- Ordinary clip bodies and selection targets follow the elapsed-time axis when
  switching Time, BPM or VDJ Beat markers and zoom levels. Changing the displayed
  ruler does not convert stored clip units; the existing confirmation before
  entering BPM markers still aligns beat-based clips to whole beats.
- Timing Settings opens the existing timing controls. Their spinbox changes
  publish immediately; Escape closes a timing field without rolling back an
  already published edit. Ctrl+] only toggles the currently loaded panel.
- Fit selection and Go to selection start sit after Zoom in/out and act on the
  selection that owns the editor keys: the selected recorded samples, or else the
  selected clips, never both. Fit changes only the view (zoom, horizontal and
  vertical scroll, and a view-only ruler reach past the authored end, reset on a
  Show, marker, BPM or workspace change). The ruler and the lanes share one
  horizontal scroll however it changes (Fit, zoom, playhead following, a drag, the
  scroll bar or a focused object). It keeps lane folding, never seeks and
  never changes clips, events or the Show's length. When the whole selection
  cannot be shown (wider than the timeline at the zoom-out limit, or rows taller
  than the editor), it keeps the selection's start in view and says why. If the
  lanes and the ruler end up at different scroll positions it says "The lanes and
  the ruler are not aligned". Go moves
  the playhead to the earliest selected time through the existing seek: stopped
  and paused stay so, playing continues from there, and the existing playhead
  following may scroll a playing view. Both are disabled with a reason when
  nothing resolvable is selected or a beat-based clip has no Show tempo; Go is
  also disabled while Perform makes the Show read only ("Following external
  clock (Perform)" when Live or Suspended). A Show switched to an external clock
  directly in code, without Perform, is not covered: Go stays enabled and the
  external clock moves the playhead back. Recorded times above 2147483647 ms stay
  valid, but the timeline cannot display or seek them: Fit is disabled when the
  selection is beyond the display range (on BPM markers, half that time at
  120 BPM) and Go when its start is. Near that bound the drawing can be tens of
  pixels off (measured 46.5 px at 2147482647 ms, 0.5 px at 1200 ms): the stored
  and seek values stay exact and both actions refuse beyond the bounds, but pixel
  alignment near that bound is not guaranteed. Space activates the focused
  button only; Tab skips a disabled one.
- Local key presses, repeats and releases stay with their original owner instead
  of also activating VC mappings. Fields, REC, buttons and Show-local dialogs keep
  their local keys. Global Save/Undo and other contexts retain their existing paths.
  Changing the selected ordinary clip identities ends a held move or endpoint edit,
  even without changing focus. Same-selection holds and redraws keep repeating;
  already published edits remain undoable.

Ordinary Copy/Paste, Delete and native Undo/Redo remain available. While the
recordings own the keys (the Recordings tab, a recording selection or the focused
table), Ctrl+C, Ctrl+V, the toolbar Copy and Paste buttons and the Copy/Paste
shortcuts act on recorded events instead of clips; text fields keep their own
text Copy/Paste. Copy takes value snapshots of the exactly selected events, also
while REC is on or the Show plays, and keeps them for the workspace; a new or
loaded workspace empties it. Paste needs no uncommitted cell edit and also works
while the Show plays or records. It puts the earliest copy at the cursor and the others at
their copied offsets and equal-time order, after events already at those times,
in the shown Show of the same workspace. The copies get new event ids above every
id the Show has issued and make one undo step; Undo removes only them and keeps
later captures. The pasted events become the selection and the shown passage.
Controls are pasted unchanged, never matched by caption; the feedback counts
pasted events whose control is not ready. A paste that does not fit (past the
end of time, an unknown or self-referencing Function target) changes nothing.
Recording Cut and keyboard drag/drop placement are not implemented.

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
of recorded events work while the Show is stopped, playing, paused, recording or
driven by VirtualDJ Perform; Perform keeps play, pause, seek and loop, and local
seeks stay refused. An edit is published at once: the playhead processes the
events it reaches next as they are now. Editing behind the playhead changes
nothing already played; an event moved from behind to ahead of it plays again
when reached. Work the Show already reached, queued or is still settling finishes
as it was accepted, and no edit seeks or replays earlier events. Recording goes
on during an edit and only ever adds; the input played live is not played again
at its captured time, but an event moved elsewhere plays there. Each edit is one
undo step of its Show that restores only its own events and refuses if they
changed since; Undo and Redo change data, not lighting actions already
performed, and keep events recorded meanwhile. While Perform drives the Show,
Undo and Redo apply only when the next step is a recorded-event edit of the
shown Show; any other step is refused and stays where it is. To replace a
passage, delete it and record again; there is no punch-in. The end of the Show follows its last
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

### Repairing a recorded passage

- Below the Recordings lane and in the Recordings table, a readout names the
  selection: its event count, how many groups it touches and its earliest and
  latest Show time. Move and Delete name the same count in their tooltip and
  accessible name, Snap in its accessible name, so a row click that narrowed the
  selection shows before anything moves.
- Enter or double-click on a timeline object whose samples are selected opens
  the whole selection, across groups, as a passage. The table then lists only
  those events. The passage is not a selection: row clicks keep their Ctrl/Shift
  behavior, a double-click selects its row before editing, and the lane
  highlights the selected events only. A field edit changes the one row marked
  as being edited.
- The passage keeps its exact event IDs. Regrouping never adds events; a deleted
  event leaves it and its Undo does not put it back. The filter narrows the
  passage and the header says how many passage events it hides. When no event is
  left the table says so and offers no rows. **Show all** or the Recordings tab
  shows the whole recording again. Changing Show clears the passage, selection
  and draft.
- **Split** shows the timeline above the same Recordings table. Choosing objects
  on the timeline opens them as the passage below; switching from the Timeline
  tab opens its selection, switching from the Recordings tab, or while a draft is
  open, keeps the table's rows. The Timeline and Recordings
  tabs work as before; the split lasts for the editor session and is not saved.
  An open draft survives switching views. With the text-only or text-and-list
  Tab preference, Tab from the colour button enters the timeline's tracks before
  the table, Shift+Tab from the first track returns to the colour button, and
  Shift+Tab from the table filter returns to the timeline. In a read-only Show
  the Split tab stands for the colour button, and Tab from the filter goes on
  into the table's rows.

## Extending the format

Add a typed command only when its validation, serialization, playback and seek
behavior are defined. Do not serialize widget clicks or arbitrary executable
strings. Level and Grand Master sliders already use `SetSliderPosition`, not
function-intensity conversion. Flash, cue navigation, speed and XY controls
need defined replay semantics before recording support is added.
