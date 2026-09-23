# Test strategy

[Requirements](requirements.md) · [Principles](core-principles.md) · [Implementation plan](implementation-plan.md)

These tests are pending. The user confirmed three public seams:

1. Authored data: capture, edit, save and reload identity, time, order and values.
2. Pure FSM: transport/input events produce next state and explicit effects.
3. Native VC integration: accepted input, state application, ownership, diagnostics and QML behavior.

Start with a failing native test: a Collection starts a Scene, its button is Monitoring, recorded ON makes it Active, and the Scene remains active after the Collection stops. Implement enough to pass, then add the next behavior. Use one red/green vertical slice at a time and existing Qt `_data()` / `QFETCH` patterns; do not bulk-write imagined implementations.

| Criterion | Interface and decisive cases |
|---|---|
| C0-1 | Equivalent pointer/MIDI states, relative-input conversion and rejected pickup. Submit 20%, 80%, 40% before one engine tick: retain three accepted positions in order; exclude an unchanged duplicate. |
| C0-2 | Audio/Programmatic/Replay and Collection-child negatives; Record-off before delivery and no replay echo. Add 50% at 12s to a take containing 20% at 10s and 80% at 15s: retain the 15s event. |
| C0-3 | Parameterize the six native-state/desired-state pairs. Verify Monitoring+ON survives Collection stop, Monitoring+OFF stops, and matching states cause no startup/SoloFrame effects. A recorded halfway position maps from value 25 in range 0..50 to 50 in range 0..100 after rebinding. |
| C0-4 | Delete/recreate numeric IDs, copy/reload controls, disable a target and change type/action/mode/attribute role. In particular, an intensity slider changed to GrandMaster must skip with a reason. Assert the Show continues valid events without substituting a target. |
| C0-5 | XML round trip with Version 1 fixtures, mixed records and equal timestamps. Reject malformed values, unknown versions/actions and ambiguous identities without replacing valid data. Compare legacy targets, ordering and behavior. |
| C0-6 | While Playing, advance from 9s to 12s across ON at 10s and OFF at 11s: process both, including after a late update. Include slider/button barriers and equal timestamps; observe native effects as well as request order. |
| C0-7 | Explicitly seek from 9s to 12s across those same events: apply destination OFF without the intermediate ON. ON at 10s/OFF at 20s restores ON when seeking or starting Play at 15s. Preserve prior pause state. A first value at 10s with current 35% stays 35% on seek to 5s. |
| C0-8 | Block dispatch, then stop, pause/resume, seek, loop/end, unload, delete or replace workspace. Check stale-work rejection and valid-tail delivery. Pause/Stop must leave VC effects intact; manual 55% between events remains until the next recorded change. No duplicate destination event after restoration. |
| C0-9 | In the Recordings tab, single-click selection must not edit. Double-click a time/state/value cell; Enter commits and Escape cancels. Delete several rows without a dialog, verify the count, then undo once. Test filtering, fixed-Show undo after selection changes, and C++ rejection while paused/running/recording. |
| C0-10 | Pure FSM transitions: equal inputs yield equal next state/effects; rejected input preserves authored data. Confirm the pure interface carries no QObject or live engine access. |
| C1-1 | Five gestures referencing three controls with counts `3/1/1`, duplicate captions with distinct identities, mixed status and no references. Exceed the event-log cap, then clear/evict log entries; the complete reference inventory remains queryable. |
| C1-2 | Compare the shared resolver's view and replay results for each status and multiple expected roles. Include valid Level-channel and GrandMaster controls without Function bindings. |
| C1-3 | Use real add/delete/rename/rebind/mode/enable and binding-target mutations with native Qt notifications. Restore the original identity and copy a control. Verify status/count changes without changing gestures or triggering replay. |
| C1-4 | Close/hide the whole panel, mutate configuration, reset the workspace and reuse a Show ID. Assert no closed-view updates, fresh state on reopen, discarded stale callbacks and continued playback validation while closed. |
| C2-1 | Arm REC through each performance view without moving the playhead or starting functions. Record at a paused/stopped cursor, verify equal-time order and shared target/status, then disarm from the VC. Include waiting with no resolved Show. |
| C2-2 | While armed, attempt manual Show selection through QML and C++; reject the switch without changing the target. Navigate to VC and back. Disarm, then select another Show. |
| C2-3 | Accept input before Save but defer its publication. Save while Playing and while paused: reopen the saved workspace and verify those events and extent. Deliver new input after the checkpoint; REC/playback stay unchanged and the workspace remains dirty. Inject save failure and retain pending data. |
| C2-4 | Exercise New/Open/Exit with REC armed through the existing dialog. Cancel preserves target, events and transport. Save/Discard settles the old session before replacement; late callbacks cannot enter the new workspace. Failed save leaves the old session available. |
| C2-5 | Open the same docked panel from each launcher. Switch tabs while events arrive and verify bounded history remains. Close/hide, deliver late callbacks, then reopen: no closed-period events or backfill. Reference inventory remains independent of log eviction. |
| C2-6 | Produce repeated skipped/unsupported operations with the panel closed. Check the failure count/latest reason, continuing playback and absence of per-event popups or log rows. Open through the indicator: details start at open, with no fabricated pre-open history. |

Use unequal times and values to expose ordering or aggregation errors. Check both accepted data and native side effects. Mutating a test snapshot or emitting a synthetic notification does not prove mutation-to-view delivery.

A test MIDI plugin proves the application mapping path, not physical controller timing. Serializer equality does not prove pickup or thread safety, and an invocation trace does not prove native effects. Explicit VirtualDJ seeks remain outside this release; do not claim their behavior from synthetic position updates.
