# Core engineering principles

[Requirements](requirements.md)

## Functional decisions, explicit effects

**C0-10:** Given the same authored data, normalized event and transport state, the existing command FSM returns the same next state and append/dispatch effects. A control-state calculation uses the supplied native control state; neither calculation performs a VC operation.

Keep value calculations in the FSM. Let the adapter publish accepted data and let the GUI executor invoke controls. UI notifications do not authorize another recording.

## Shared workflow state

Show editor, VC and DJ controls send intent to one recording state. Derive target labels, edit permission and REC indicators from it; do not introduce a recording session per view.

Treat Save as a checkpoint, not a stop/start command. Separate its accepted-data boundary from later input so successful saving does not clear newer unsaved changes.

Keep recording's runtime lifetime separate from persisted duration. Save and finalization use the last retained clip/command, not the time spent waiting with REC armed.

## Reuse the native control path

Capture the normalized input command before execution, with its source and acceptance time. Keep that provenance through deferred application; generic property notifications cannot authorize recording. Query existing VC state at application and use native start/stop/value effects. The recorder keeps no running-function mirror or output-correction loop.

Separate display feedback from output requests. Attribute/stopped feedback updates a control's presentation; explicit input and the native function-start reapply remain output-producing operations.

## Transport intent

Distinguish ordinary Playing progress from cursor repositioning. Advance processes crossed events. Paused movement applies nothing. ~~Play from the new cursor sends the recorded prefix through the same native dispatcher.~~ Superseded by C1: forward plays the crossed interval, backward returns only the window this traversal changed, Play from T replays nothing before T. Elapsed-time differences cannot establish user intent.

Sequence commands rather than predicting their effects. Keep SoloFrame, Collection, ownership and feedback behavior in the native controls. Do not build a separate history reducer or simulated engine.

## Thread and lifetime ownership

Keep QObject/widget access on the GUI thread. Send ordered values from the playback host with workspace, Show, session, traversal and destination identity; validate again before execution.

Distinguish queued work from attempted execution. Preserve due actions or report dispatch failure under backpressure. Use the existing queue/thread mechanisms rather than per-event threads.

A successful final traversal check claims one native operation. Cancellation rejects unclaimed work; an operation already claimed may finish. Recheck after any wait and before the next operation. Do not use future restoration as proof of cancellation.

Order transport boundaries, accepted manual input and replay effects together. An older crossed replay must finish before Pause completes, rather than overwrite newer paused busking on Resume.

Deferred user requests carry their accepted target, time, origin and desired value. Do not resolve a relative click again after waiting behind replay work.

## One validation rule

Share pure target/role/binding resolution between replay and the referenced-controls view. Derive the view from Show references and a GUI-thread configuration snapshot.

Use native change notifications while the view is open. Avoid polling, a background status registry and output scans.

Keep the operation-failure summary separate from detailed diagnostic capture. Hidden views retain no event history.

## Small extensions

Extend the existing command FSM, Show storage, event list and Tardis history. Add typed actions with defined validation and traversal behavior; avoid a general message framework or property bag.

Undo accepted edit deltas by stable event identity. Preserve intervening capture and reject conflicts before history movement; do not restore an old whole-track snapshot.
