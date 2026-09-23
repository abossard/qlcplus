# Timecode recording and live busking

Research date: 2026-09-23. Discussion evidence, not an approved requirement.

## Question

A slider's first recorded value is 80% at 10 seconds. When playback rewinds to
5 seconds, should it leave that control alone or restore an initial snapshot?

The user expects to mix live busking with recording and tentatively prefers
leaving controls alone when no earlier recorded state exists.

## Primary-source findings

| Product | Documented behavior | Limit of the evidence |
|---|---|---|
| [grandMA3 timecode settings](https://help.malighting.com/grandMA3/2.3/HTML/timecode_settings.html) | **Assert Previous Events** asserts events preceding the cursor. **Switch Off** offers stopping playbacks started by the timecode show or keeping them. | The settings do not define a value for a control with no preceding event. Assertion and stop cleanup are separate policies. |
| [grandMA3 fader tracks](https://help.malighting.com/grandMA3/2.3/HTML/timecode_tracks.html) | Fader events contain values used to recreate movement. Master-fader movement to/from zero can also record cue events under Auto Start/Auto Stop. | Native fader side effects matter; this does not establish a baseline snapshot or manual takeover rule. |
| MagicQ [timecode](https://secure.chamsys.co.uk/docs/magicq/manual/timecode.html) and [timeline](https://secure.chamsys.co.uk/docs/magicq/manual/timeline.html) | Timeline recording includes playback faders, Flash and Go actions. Recording a playback deactivation causes deactivation again at that recorded time. | These pages do not specify missing initial values or manual-versus-recorded fader arbitration. A fader point can describe a fade target, so it is not evidence for instantaneous assignments. |
| [Ableton automation](https://www.ableton.com/en/live-manual/12/automation-and-editing-envelopes/#overriding-automation), a DAW comparison | A manual parameter change outside recording overrides automation until the user re-enables it. | This is a takeover policy, not evidence that lighting recorders behave the same way or resume at the next event. |

grandMA3's **Manual Events** setting distinguishes user-triggered actions from
automatic follows. It does not define how live busking overrides automation.

No reviewed source specifies an automatic whole-console baseline for the
before-first-event case. The documentation does not establish a universal rule.

## Recommendation for discussion

Restore a known recorded state at the destination. Leave a control unchanged
when the recording has no earlier value for it. This preserves live busking
without adding an initial snapshot.

That choice makes the result depend on prior live activity. Rewinding before
the first event does not undo a value applied later in the song. A recorded
initial value could provide a baseline when the operator wants one.

This recommendation does not add Ableton-style takeover latching. The proposed
QLC+ model leaves manual changes alone between events; the next recorded event
can change the control again.

## Candidate TDD scenarios

These expectations require user confirmation:

- First event A=80% at 10s; B has no events. Current A=35%, B=65%. Rewind to 5s:
  issue no assignment and retain both values.
- At 2s record button On and slider 20%; at 8s record Off and 80%. Rewind to 5s:
  apply On and 20%. Applying On to an already-on control must not toggle it off.
- Slider events at 2s and 10s. The operator moves it to 55% at 6s: leave 55%
  between events, then apply the 10s value.

Fresh-play initialization and stop cleanup remain separate interview decisions.
