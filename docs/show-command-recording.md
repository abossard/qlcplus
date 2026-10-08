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
| `SetButtonState` | On or Off | Apply the desired state through the current compatible Toggle, Flash, Blackout, Freeze or FreezeHold button, or slider flash. |
| `SetSliderPosition` | A number from `0` to `1` | Map the accepted position into the current compatible slider range. |
| `SetSliderChannel` | Integer channel value `0..255` and attribute binding | Apply the frozen channel value without inferring intent from the current slider range or Click & Go presentation. |
| `SetSliderReset` | Attribute binding | Release the native monitor override. |
| `SetSliderColors` | RGB, white/amber/UV and brightness | Apply the accepted Click & Go colors. |
| `SetXYPadPosition` | Fractional pan/tilt coordinates `0..255` | Apply native pad motion. |
| `SetXYPadFloor` | Metre X/Y/Z coordinates | Apply native floor targeting, including target height. |
| `SetXYPadRanges` | Horizontal and vertical endpoint pairs | Apply the native endpoint windows. |
| `SetXYPadPositionPreset` | Choice, desired active state and frozen coordinates | Apply the accepted static position and active-choice transition. |
| `SetXYPadFunctionPreset` | Choice and desired active state | Activate or deactivate the current compatible Scene/EFX binding. |
| `SetXYPadGroupPreset` | Choice and desired active state | Apply the current native group/head choice. |
| `SetAnimationFader` | Normalized playback intensity | Apply the native Animation fader. |
| `SetAnimationColor` | Slot and replace/reset/component arguments | Apply the indexed Matrix operation. Reset is distinct from valid black. |
| `SetAnimationContent` | Named algorithm, text and typed properties | Apply accepted List/Range/Float/String content arguments. |

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

Legacy-only tracks use `Version="1"`. Version 2 introduced VC commands with
`Control` and `Role` plus `State` or `Value`. Version 3 adds Flash/global
roles and `Pair` identities for momentary press/release edges. Versions 4 through 8
introduce slider reset, Click & Go colors, pad coordinates, Animation fader and
floor coordinates respectively. Version 9 adds closed native payloads in
`Arguments`: channel intent, range endpoints, pad choices and Matrix color/content.
Typed objects reject missing, extra or incorrectly typed fields. Property names
are unique, numeric values finite, and channel/color values bounded. Historical
versions remain loadable. Saving chooses the minimum version required by the
retained actions.

Only `SetButtonState` edges may carry `Pair`. Other actions reject even a
malformed `Pair` attribute instead of silently discarding it. An explicit Button
XML `Pair` must be an unsigned identity other than the unset sentinel 4294967295.
An absent `Pair` remains valid for unpaired button records. Rejected insertion,
replacement or XML loading leaves the existing commands, Order, extent and
in-memory allocation floors unchanged. Unrelated legacy metadata such as
`Comment` remains ignored.

Colors, Pan/Tilt and floor targets are also closed typed values in memory.
Their historical `Attribute` encodings and minimum file versions are unchanged.
Validation and complete payload equality compare typed fields directly, not
their encoded text. Order remains placement data and is checked separately by
frozen edits and history. XML and view drafts are boundary codecs.
Matrix content may include its native choice ID. Coordinates, colors, text and
property values are frozen when accepted. They are not reread from an edited
preset when replayed.

Version 9 channel intent stores the accepted raw native byte, including 0 and
255. A Level channel replay keeps that byte after range or Click & Go preset-mode
changes. Historical normalized slider records retain their earlier dispatch
through the current compatible presentation: CnGNone maps through the current
slider range, while CnGPreset rounds `position * 255`. For example, saved version 2
position 0.75 replays byte 152 with range [32,192] and current CnGNone, but byte
191 with current CnGPreset. These records are not rewritten as raw channel intent;
their original raw intent was not stored.

Queued input and claimed replay retain the required native destination and
configuration until execution. Removing or replacing that destination, even
under the same numeric ID, cancels the pending operation with a reason. A later
occurrence resolves the current compatible binding afresh.
Delivered Matrix receipts retain the actual Function lifetime as well. Backward
restoration cannot write through a rebound control, even when the replacement
has identical colors or the same numeric ID. Show Stop releases the delivered
Function's Show owner, not the control's replacement binding.
An accepted same-value live Animation fader request acquires its native live
owner, so Show Stop cannot remove that independent output. The production fader
submits deliberate movement/presses, not value-binding feedback from playback.
A cold Show start begins a new actual-before journal for that Show. Earlier
recording-session receipts cannot supply its backward-restoration baseline.
This leaves native live state and accepted live ownership unchanged.

The private `ShowControlAction` module plans native receipt dependencies from
copied operation and Collection/Solo topology facts. Its value plan includes
the started or stopped Function and any causal-parent member expectations.
The recorder consumes those plans while retaining request/run queues, waits,
traversal cancellation, batch acknowledgements and its authority/history journal.
Live topology discovery remains an explicit module action; pure receipt planning
does not read widgets, the document or a clock. Native effects and timer receipt
observation retain their existing owners.

Native algorithm properties are checked against their current List/Range/Float/
String metadata before initial acceptance. An invalid request changes neither
the native state nor the authored track. Algorithm activation republishes the
retained indexed colors to the new native algorithm, including colors accepted
while stopped.

The visible Animation algorithm combo submits a deliberate named content request
with the actual pointer or keyboard origin, including selection in its popup.
Programmatic algorithm setters, loading and history restoration remain unrecorded.
Floor-target preflight uses the native pad's metre window and 20 m height maximum.
An out-of-bounds authored target is refused whole, not published and later clamped.

The pad writer snapshots its resolved enabled heads and their Fixture lifetimes
with each generation. Required fixture/group changes or Pan/Tilt inversion retire
pending recorded work as cancelled, rather than writing to a changed binding.
Cancellation does not make the cached coordinates proof of delivery. A later
same-value input can publish fresh output and supersede the earlier generation.

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
Both edges of a saved hold share one `Pair` identity and keep separate event
IDs. The loader rejects orphan or mismatched pairs.

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

  Native choice restoration retains the actual prior source owner, not the owner
  of the discarded Show command. Matrix restoration also retains whether content
  was a local VC override or ordinary Matrix content. Stop only releases Show-owned
  effects. It does not perform this backward restoration.

  Matrix receipts retain managed VC content, colors, choice and brightness,
  including whether content was a local override. Backward movement restores
  actual-before values through ordinary native setters. Delivered Function,
  group and algorithm lifetimes and whole eventual-target metadata are checked
  before effects. Prior Fill properties are checked against Fill, even if the
  delivered algorithm is Fireworks. Missing or incompatible definitions refuse
  the whole restoration without probing live script writers.
  Replaced targets or algorithms and later live intent, including same-value
  reassertions, supersede the indivisible restoration.

  Accepted amount properties stay frozen. Ordinary native setters own step-count
  rescaling and animation behavior; recording does not restore indices, counts,
  continuous phase or hidden script state, and never rewinds elapsed/beat clocks.
  A valid admitted callback can still fault during execution. It reports
  Failed/notApplied with a reason, retains authored commands and retires the
  operation without reporting success. Native effects before the fault are not
  undone. Stop remains ordinary owned-effect/hold release, not before-projection.
  This includes property/read/count/color callbacks reached by content, color,
  fader start or algorithm initialization. Queued color callbacks finish on their
  existing JS thread before the controlled result is observed. A failed start
  still waits for its actual Function settlement before retiring the batch.
  Valid empty colors, unsupported optional color readers and a native zero
  count are not callback failures. Staged stopped content is applied later at
  fader start; its staging receipt does not promise future rendering will succeed.
  Later autonomous frame callbacks are outside this controlled-operation result.

On Play, or a position change during playback, QLC+ asserts a momentary hold
active at the destination without replaying its earlier edges. Leaving its
interval releases the playback-owned hold. Slider flash restores the native
attribute value from immediately before playback asserted it, not the value
from the original take.

The direction of a requested seek is decided when it is requested, against the
position the runtime last reported. Ordinary function clips retain their native
seek behavior. External clock updates are not explicit VirtualDJ seek intent.

## Recording and editing

Compound values open a complete local draft. The color picker, endpoint sliders,
numeric fields/steppers and algorithm property controls edit only that draft.
RGB/WAUV, brightness, metre height, fractional coordinates, reversed endpoint
windows, indexed reset/black/component operations and named content keep their
distinct meanings. Preview and Escape do not operate native controls or seek the
Show. Commit validates the whole frozen edit basis before one ID-delta history
publication; invalid or conflicting drafts never partially publish.
Changing a Matrix operation or content algorithm retires obsolete fields. Missing
draft paths are unavailable, not zero-valued controls, and cannot modify the draft.

Bound floor drafts retain their opening range/stage/height metadata. A change to
that basis refuses commit rather than reinterpreting the coordinates. Missing
targets remain editable without inventing native limits. Bound List properties
use their named choices, and Range/Float steppers use native metadata limits.
Other native-bound drafts retain copied required binding and property metadata
as well. Rebinding, disabling, losing or ambiguously resolving an opening target,
or changing its required metadata, refuses the whole candidate before publication.
The comparison uses current-compatible values, not original object identity.
An equivalent replacement Matrix under the same binding remains compatible.
Selected payload or Order changes cannot be overwritten by the old draft.
Reopen against the new basis to retry; unrelated accepted capture is retained.
Independent Show playback and capture continue during preview and Escape.
Undo/Redo changes only the edited IDs and preserves those unrelated records.
Group/head choices whose current fixture or head is missing are incompatible,
even if their preset still retains a nonempty head list. Display and native
preflight report the same refusal without changing the recorded command.
The full color Canvas acquires its painting context on each paint, including
after cancellation and reopening.

The Show Timeline includes one **Recordings** lane after the function tracks.
Consecutive saved slider samples with the same control UUID, role and attribute
form one object. Every other saved command breaks the run, including a different
slider or a legacy command. Time gaps and recording off/on do not break a run
unless another command was stored. Groups are typed `ShowCommandGroup` values
derived by `ShowCommandTrack::groups()`, not new playback tracks or XML objects.
Flash, slider flash and FreezeHold pairs form one hold block even when another
control's events fall between their press and release. Blackout and Freeze
latch events remain points. All samples, values and equal-time ordering remain
unchanged.

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
later captures. Pasted holds also get fresh pair identities, preserved through
Undo/Redo. Select both edges for pair edits or copy. QLC+ refuses partial or
malformed pair operations whole with a reason. The pasted events become the
selection and the shown passage.
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
state. Flash buttons, slider flash and FreezeHold record paired press/release
edges. Blackout and Freeze latch record desired global states. Freeze uses the
current native output, without saving a DMX look. FreezeHold keeps the native
shared flag: any delivered release clears momentary freeze; the latch remains
separate. Level, Adjust, Submaster and Grand Master sliders record their position.
Input counts from the screen, keyboard bindings or a patched MIDI or OSC input,
stamped when it is accepted. The stamp is the cursor moved on a stopped or
paused Show; otherwise it is the last Show time playback processed. That can be
one engine tick (20 ms by default) earlier than the time display, which already shows the
next tick, including while paused. An external clock is always its own
position. Every valid Show time is kept, up to the format's maximum. The debug
panel's Show time for transport rows uses the same clock. Slider reset records
native override release; Stop All remains live-only and displays an
unsupported-recording reason. Other
excluded widget families keep their existing live behavior and recording warnings.

Pause keeps playback-owned holds active; Resume reaches their scheduled
release. Stop releases playback-owned holds and only releases Show-owned native
owners; it does not project prior Blackout/Freeze values for a Show that never
touched those latches. If replay touched a Blackout/Freeze latch and no live
intent exists for this Show, Stop falls back to Off for that latch. FreezeHold
momentary state is only written when this Show released its own replayed hold.
Manual holds and same-value live
reassertions keep their authority until the next recorded change.

The Show editor's Recordings tab lists the selected Show's events in order:
time, control (caption, identity and current binding, or the function of a
legacy command), action and value. Missing or disabled controls stay listed with
the reason. A click selects a row, Ctrl or Shift extends the selection, and a
double click edits the time (seconds), state (On/Off) or value (percent); Enter
commits and Escape cancels. Delete removes the selection at once and offers
Undo. Left/Right move the selection by a bar, half a bar or a quarter bar, and
Snap puts its earliest event on the nearest beat, the others by the same delta.
Both need BPM markers with a tempo or a VDJ Beat grid.
Complex values open typed fields for native coordinates/units, endpoint pairs,
choice states, colors and named content properties. Their complete draft belongs
to the view, not a disposable row. Preview and Escape change no authored data,
native effect, playback time or history. Commit validates the whole frozen
selection; invalid data stays open for correction and conflicting records refuse
the whole edit and close the obsolete draft. Reopen against the changed selected
record to retry without overwriting it. A successful edit makes one existing
ID-addressed undo step; committing unchanged data adds none.
Editing, undo and redo
of recorded events work while the Show is stopped, playing, paused, recording or
driven by VirtualDJ Perform; Perform keeps play, pause, seek and loop, and local
seeks stay refused. An edit is published at once: the playhead processes the
events it reaches next as they are now. Editing behind the playhead changes
nothing already played, except an active playback hold: deleting or moving it
outside the cursor releases it after commit, never during preview or Escape.
Played Blackout/Freeze edits remain data-only until the next recorded change,
accepted position boundary or Stop. An event moved from behind to ahead of it plays again
when reached. Work the Show already reached, queued or is still settling finishes
as it was accepted, and no edit seeks or replays earlier events. Recording goes
on during an edit and only ever adds; the input played live is not played again
at its captured time, but an event moved elsewhere plays there. Each edit is one
undo step of its Show that restores only its own events and refuses if they
changed since; Undo and Redo keep events recorded meanwhile and do not replay
earlier lighting actions. Active-hold cleanup still applies. While Perform drives the Show,
Undo and Redo apply only when the next step is a recorded-event edit of the
shown Show; any other step is refused and stays where it is. To replace a
passage, delete it and record again; there is no punch-in. The end of the Show follows its last
retained command or clip. Visible rows follow renamed, rebound, enabled/disabled or removed
controls and renamed functions. Editor steps are not sent to connected
network clients. The workspace Save
operation stores the track with its Show, including input accepted until the
save, while REC and playback continue. An open hold gets a temporary release
in the saved snapshot at save time; live authoring keeps the same open pair.
REC-off closes the authored pair at that time, leaves the physical hold active
and does not record its later release. The saved end is the last recorded
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
