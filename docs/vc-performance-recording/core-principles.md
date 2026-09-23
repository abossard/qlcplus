# Core engineering principles

[Requirements](requirements.md)

## Functional decisions, explicit effects

**C0-10:** Given the same authored data, normalized event and transport state, the existing command FSM returns the same next state and append/dispatch effects. A control-state calculation uses the supplied native control state; neither calculation performs a VC operation.

Keep value calculations in the FSM. Let the adapter publish accepted data and let the GUI executor invoke controls. UI notifications do not authorize another recording.

## Shared workflow state

Show editor, VC and DJ controls send intent to one recording state. Derive target labels, edit permission and REC indicators from it; do not introduce a recording session per view.

Treat Save as a checkpoint, not a stop/start command. Separate its accepted-data boundary from later input so successful saving does not clear newer unsaved changes.

## Reuse the native control path

Keep provenance through input normalization and deferred execution; generic setters also receive audio and feedback updates. Query the existing VC state when applying a command, then use native start/stop/value effects to reach the requested state. The recorder keeps no running-function mirror or output-correction loop.

## Transport intent

Distinguish `PositionUpdated` during Playing from explicit `SeekRequested`. Advance processes crossed events; seek restoration reduces authored history to destination values. Elapsed-time differences cannot establish user intent.

## Thread and lifetime ownership

Keep QObject/widget access on the GUI thread. Send ordered values from the playback host with workspace, Show, session, traversal and destination identity; validate again before execution.

Distinguish queued work from attempted execution. Preserve due actions or report dispatch failure under backpressure. Use the existing queue/thread mechanisms rather than per-event threads.

## One validation rule

Share pure target/role/binding resolution between replay and the referenced-controls view. Derive the view from Show references and a GUI-thread configuration snapshot.

Use native change notifications while the view is open. Avoid polling, a background status registry and output scans.

Keep the operation-failure summary separate from detailed diagnostic capture. Hidden views retain no event history.

## Small extensions

Extend the existing command FSM, Show storage, event list and Tardis history. Add typed actions with defined validation and traversal behavior; avoid a general message framework or property bag.
