# Show command tracks

A command track stores timed instructions alongside a Show's ordinary function
clips. It references functions by ID, so reassigning a Virtual Console control
does not change an existing recording.

For the planned current-binding VC gesture extension, see
[VC performance recording](vc-performance-recording/requirements.md).

## Commands

| Action | Value | Playback |
|---|---|---|
| `Start` | None | Start the target through the Show. |
| `Stop` | None | Release the Show's ownership of the target. Other owners may keep it running. |
| `SetIntensity` | A number from `0` to `1` | Apply the intensity without starting a stopped target. |

Recording accepts user commands, not audio-mapped changes, playback feedback or
generic property changes. It does not inspect running functions to reconstruct a
performance. The existing engine executes each command.

Manual intervention remains possible. If an operator stops a function before a
later intensity command, that command does not restart it. Replaying commands
does not guarantee identical output from random or audio-reactive functions.

## Saved format

The optional `CommandTrack` element belongs inside the Show's `Function` element
in the workspace XML. Times and `Extent` use elapsed milliseconds, independently
of the Show editor's ruler. `Extent` is the authored playback duration, not the
time of the last command.

```xml
<CommandTrack Version="1" Extent="12000">
  <Command ID="1" Time="1000" Action="Start" Function="12"/>
  <Command ID="2" Time="1500" Action="SetIntensity" Function="12" Value="0.25"/>
  <Command ID="3" Time="4000" Action="SetIntensity" Function="12" Value="0.8"/>
  <Command ID="4" Time="10000" Action="Stop" Function="12"/>
</CommandTrack>
```

Event IDs are unique within the track. Equal-time commands preserve their stored
order. Unsupported versions/actions and invalid values are errors; they must not
load as an empty successful track.

Edit XML only while the workspace is closed or saved elsewhere. Keep the target
function definitions in the same workspace. A function reference does not freeze
that function's definition.

## Position changes

Forward clock movement applies the crossed commands in their recorded order.
A forward jump and a delayed position update use the same path; playback does
not guess which caused the new position.

Backward clock movement restores the latest recorded intensity values before
the destination. It does not fire earlier `Start` or `Stop` commands. Commands
at the destination remain eligible for one execution as playback continues.
Without an earlier recorded value, playback does not invent a reset value.
Ordinary function clips retain their existing seek behavior.

An explicit local seek also restores values without firing historical triggers.
Moving backward into the middle of a command-started effect therefore does not
recreate its earlier start. Use ordinary clips where offset-based playback is
required.

## Recording and editing

In the DJ view, enable Perform and REC. REC waits for a Show if none has resolved
yet, then keeps that target. Use Toggle buttons and function-intensity Adjust
sliders through the screen, keyboard bindings or a patched MIDI input.
Unsupported recording modes leave live control available and display a reason.

Open the command view from the Show Manager toolbar to edit event time, action,
target, intensity or playback extent, or delete an event. The workspace Save
operation stores the track with its Show. Stop REC to finalize the captured
duration before saving the completed recording.

Recording is not an undo history of raw widget changes. Programmatic changes and
audio mappings do not create events, and a recorded Collection launch does not
expand into events for each child.

## Extending the format

Add a typed command only when its validation, serialization, playback and seek
behavior are defined. Do not serialize widget clicks or arbitrary executable
strings. Flash, direct DMX sliders, Grand Master, cue navigation, speed and XY
controls need their own semantics rather than conversion into an intensity value.
