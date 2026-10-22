# QLC+ tutorial: connect MCP, follow VirtualDJ, record a performance

This guide is for the experimental **abossard/qlcplus v5 fork**, not a promise
that an upstream QLC+ binary has these features. It covers:

1. Connecting Copilot CLI to the embedded MCP server.
2. Receiving VirtualDJ deck telemetry and understanding Perform.
3. Recording Virtual Console (VC) actions into a Show, editing and replaying them.

The matching English, narrated/captioned 1920×1080 video is **pending**. Its
approved 5:20 plan and static storyboard sketches are not a finished video or
native demonstrations. Use the shared callouts below without waiting for it.

## Safety and evidence boundary

The procedures are instructions for an operator's **separately authorized demo
session**, not a log of actions performed while writing this guide.

- Preserve the current workspace and coordinate with its app/input owner.
  Do not restart QLC+, replace a workspace or change routing as an automatic fix.
- MCP exposes query, authoring and live-output tools together. Tool availability
  is not permission to change the project or run functions. Chapter 1 needs only
  a read-only summary; it does not require fixtures or physical output.
- DJ input, new-track ingestion, Perform, VC controls, recording, playback and
  seeking can change state or cause output. Use an owner-approved demo with
  output isolation before attempting chapters 2–3. Telemetry itself needs no
  DMX output patch, but automatic song/Show creation can modify the workspace.
- The authoring/capture grant for this tutorial permits only passive observation
  of an already visible, identified QLC+ window. It does **not** permit UI
  navigation/selection, input ownership, new workspaces, Show selection, Record,
  transport, Save, MIDI/DMX/hardware output, synthetic input or app restart.
  No such actions were performed for this guide.

**Observation, 2026-10-18 10:57 +02:00:** a read-only OS process inventory
reported no QLC+ process; a TCP9696 listener check reported none. There was no
identified running QLC+ window to capture, so acquisition stopped. Screen
permission, custom-port availability and the running build could not be
established. No MCP request was sent. No screenshot or native connectivity,
DJ, recording, editing, replay, audio or hardware check was performed.

Source review supports the expected behavior below; it does not pass a native
manual check. Static sketches, mocks and passive screenshots likewise cannot
prove active behavior. No screenshot assets are included or required to read
this guide.

## Shared chapter and callout map

| Callout | Step | Video chapter/time | Operator goal |
|---------|------|--------------------|---------------|
| 01 | [MCP-1](#mcp-1) | MCP, 0:15–1:20 | Identify the local endpoint |
| 02 | [MCP-2](#mcp-2) | MCP | Configure Copilot CLI |
| 03 | [MCP-3](#mcp-3) | MCP | Request a read-only summary |
| 04 | [DJ-1](#dj-1) | DJ, 1:20–3:00 | Identify/open the telemetry input |
| 05 | [DJ-2](#dj-2) | DJ | Check real deck ingress |
| 06 | [DJ-3](#dj-3) | DJ | Resolve the song's Show |
| 07 | [DJ-4](#dj-4) | DJ | Understand Perform transport |
| 08 | [REC-1](#rec-1) | Recording, 3:00–5:10 | Prepare the intended Show and controls |
| 09 | [REC-2](#rec-2) | Recording | Arm the shared command recorder |
| 10 | [REC-3](#rec-3) | Recording | Capture a short take |
| 11 | [REC-4](#rec-4) | Recording | Edit an event |
| 12 | [REC-5](#rec-5) | Recording | Replay with Record off |
| 13 | [REC-6](#rec-6) | Recording | Diagnose and preserve |

The plan reserves 0:00–0:15 for the overview and 5:10–5:20 for review.

## Chapter 1 — Set up and use MCP

**Prerequisites:** this fork's MCP-enabled v5 build already running on the
client's computer, an existing workspace safe to inspect, and Copilot CLI with
MCP permitted by organization policy. Builds use `qmlui=ON`; `mcp_server=ON`
defaults on with that UI. A build made with MCP disabled has no embedded server.

<a id="mcp-1"></a>
### 01 / MCP-1 — Find the local endpoint

**Action:** identify the running executable/build and its existing launch
configuration. The default Streamable HTTP endpoint is
`http://127.0.0.1:9696/mcp`. An existing `--mcp-port PORT` launch option changes
the port; `--no-mcp` disables the server. There is no separate server process to
launch for the embedded implementation.

**Expected:** the endpoint uses the running app's actual port and `/mcp` path.
The listener binds to loopback, not the LAN.

**Troubleshooting:** an absent listener can mean no running app, a disabled MCP
build, `--no-mcp`, a custom port or a bind failure. Ask the owner to check those
conditions; do not restart automatically. Loopback refers to the client's own
computer: a cloud client cannot directly reach this desktop endpoint. Do not
solve that by adding a tunnel, proxy or public bind.

Sources: [startup options](../mcp/mcpinit.cpp),
[listener](../mcp/mcpserver.cpp), [build flags](../CMakeLists.txt).

<a id="mcp-2"></a>
### 02 / MCP-2 — Add the server to Copilot CLI

**Action:** in an authorized Copilot CLI session, enter `/mcp add`. Fill in:

| Field | Value |
|-------|-------|
| Server Name | `qlcplus` (or a unique name if already used) |
| Server Type | `HTTP` — Streamable HTTP, not legacy SSE |
| URL | The endpoint established in MCP-1 |
| Tools | `query_workspace_summary` only for this exercise |

Press **Ctrl+S** to save. No QLC+ restart is needed. Discover the server's
registered tools before forming a tool request; do not assume a fixed tool count.

For file-based setup, merge an entry into `~/.copilot/mcp-config.json` without
overwriting existing servers. This is an **example**, not configuration applied
by this task:

```json
{
  "mcpServers": {
    "qlcplus": {
      "type": "http",
      "url": "http://127.0.0.1:9696/mcp",
      "tools": ["query_workspace_summary"]
    }
  }
}
```

**Expected:** the client discovers the allowed QLC+ tool.

**Troubleshooting:** Copilot CLI uses **`mcpServers`**, not the VS Code
**`servers`** wrapper. `.vscode/mcp.json` is not read by CLI. Do not copy the
stale CLI `servers` example in the repository README. Project-level definitions
can override user configuration and require folder trust; organization policy
can disallow a server. Preserve existing credentials and configuration.

Source: [GitHub's Copilot CLI MCP setup](https://docs.github.com/en/copilot/how-tos/copilot-cli/customize-copilot/add-mcp-servers)
(reviewed 2026-10-18).

<a id="mcp-3"></a>
### 03 / MCP-3 — Request a read-only workspace summary

**Action:** inspect the discovered schema for `query_workspace_summary`, then
ask: “Use only QLC+'s query_workspace_summary to summarize this workspace.
Do not change anything or run functions.” Its request arguments are `{}`.
Clients may qualify the displayed tool name with the server name.

**Expected:** the actual result contains workspace entity counts. Compare them
with the intended workspace; an empty project can legitimately return zeros.
Keep the genuine request/result or exact connection error as the receipt.

**Troubleshooting:** if discovery, connection or the tool fails, record that
failure, not invented counts. A successful summary proves a read path to that
workspace; it does not prove MIDI routing, active DJ telemetry, recording or
DMX/hardware output. This task made no request because no existing listener was
reported.

Sources: [summary implementation](../mcp/tools/query_tools.cpp),
[MCP authoring/query contracts](mcp-authoring-api.md).

## Chapter 2 — Use the VirtualDJ integration

**Prerequisites:** the fork's **VDJ Bridge** plugin, compatible VirtualDJ,
permission to configure an isolated demo input, and a local track readable by
QLC+. The optional VirtualDJ `database.xml` supplies beat grids/cue markers;
it is not the TCP connection. External VirtualDJ option application/reconnect
behavior is version-dependent and has not been tested here.

<a id="dj-1"></a>
### 04 / DJ-1 — Identify the VDJ Bridge input

**Action:** in the authorized demo, open **Input/Output**, choose **Show the
universe input sources**, and patch **VDJ Bridge → 1: VDJ Telemetry** onto the
input side of a safe demo universe. Inspect **VDJ Bridge Configuration**:
**Listen port:**, **Bonjour:** and **Status:**. Do not add an output patch just
to receive telemetry.

**Expected:** opening the input starts the listener, normally on **TCP8050**.
`Idle` is not listening; `Listening` is not yet a connected deck.

**Troubleshooting:** `vdj/autoOpenInput` defaults to false. Do not assume the
listener opens at startup because of an older comment. A competing listener
or bind error needs owner-led diagnosis, not blind port changes. The legacy
**OS2L TCP9996** path carries beat/button/cmd controls; it is not the rich
deck/elapsed telemetry listener taught here. Both plugins advertise
`_os2l._tcp`, so avoid competing discovery targets in a demo.

Sources: [VDJ Bridge](../plugins/vdjbridge/vdjbridgeplugin.cpp),
[default port](../plugins/vdjbridge/vdjbridgeplugin.h),
[startup gate](../qmlui/app.cpp),
[legacy OS2L transport](../plugins/os2l/README.md).

<a id="dj-2"></a>
### 05 / DJ-2 — Check real deck telemetry

**Action:** in VirtualDJ **Settings → Options**, use `os2l = auto` for discovery,
or set `os2lDirectIp` to the bridge host and configured port when direct
addressing is needed (for example, `127.0.0.1:8050` on the same computer).
Do not set `os2lDirectIp` to “Auto” based on the older repository instructions.
Under separate playback authorization, play one prepared local track. In QLC+
**DJ Manager**, inspect **VDJ: Connected**, **Deck**, **State**, **Song**,
**BPM**, **Pos** and **Vol**.

**Expected:** the correct song and plausible deck state appear, and position
changes across timed observations. A connection badge or a still image alone
does not establish changing telemetry.

**Troubleshooting:** the vendor's older QLC+ guide uses port 9996 for legacy
OS2L. Match the fork's VDJ Bridge port instead. Check the configured host,
listener status and ingress before making any output claim. A
**VirtualDJ database.xml not found** banner means grids/cue markers are
unavailable, not necessarily that TCP telemetry failed. **Retry** refreshes
database discovery; do not prescribe a rescan or restart as an automatic fix.
Use time-based editing in chapter 3 if no musical grid is available.

Sources: [DJ Manager UI](../qmlui/qml/djmanager/DjManager.qml),
[bridge status](../qmlui/vdjbridge.cpp),
[vendor legacy setup](https://www.virtualdj.com/wiki/QLC%20with%20OS2L.html),
[vendor OS2L reference](https://www.virtualdj.com/wiki/OS2LDMX.html).

<a id="dj-3"></a>
### 06 / DJ-3 — Resolve the song's Show

**Action:** inspect the song row's **Show n** or **no show**. Prefer **Assign**
to map an existing demo Show; use **Create** deliberately if none exists.
Use **Load** to open the mapped Show and confirm its identity.

**Expected:** the row resolves to the intended Show and Show Manager displays it.

**Troubleshooting:** newly seen telemetry can trigger automatic Audio/Show
creation after a three-second defer when the local audio is available.
A missing or unreadable track can prevent creation. **New** creates another
Show; it is not a refresh/retry button. Do not ingest a new track merely to get
a screenshot. With no resolved Show, Perform can remain Armed.

Sources: [song management](../qmlui/djmanager.cpp),
[Show creation](../qmlui/showfactory.cpp).

<a id="dj-4"></a>
### 07 / DJ-4 — Let Perform own transport

**Action:** only in an authorized demo, enable **Perform** after confirming the
song's Show. Observe deck play/pause and position. Turn Perform off before
switching to the local-transport recording exercise.

**Expected:** **PERFORM: Live** follows the selected active deck's resolved
Show; **PERFORM: Suspended** freezes progression when that deck pauses.
**PERFORM: Armed — no show for active deck yet** signals missing resolution.
VirtualDJ owns transport/position while Perform is enabled; ordinary Show
Manager Play is not the external transport.

**Troubleshooting:** Perform restricts ordinary clip/transport editing, **not**
the recorded-event editor. Turning it off pauses/releases the adopted Show
and restores its prior sync source. It is not a rollback of all live lighting.
No Perform transition or external transport behavior has been observed here.

Sources: [Perform states](../qmlui/performfsm.cpp),
[adoption/release](../qmlui/vdjbridge.cpp).

## Chapter 3 — Record, edit and replay VC actions

**Prerequisites:** an owner-approved dedicated demo Show, **Perform off**, and
a prepared VC page in operation mode. Use one ordinary **Toggle** button bound
to a demo function and one compatible slider. Record their actual names,
bindings, slider mode/range and expected values in the review evidence before
acting. These controls are prerequisites, not objects created by this guide.

This records accepted **VC command intent at Show time**, not desktop video,
audio, raw DMX or every automatic property change. Replay uses current compatible
VC bindings; changing a function or binding can change its result.

<a id="rec-1"></a>
### 08 / REC-1 — Prepare the intended Show

**Action:** select the dedicated Show in Show Manager. If the demo owner needs a
new one, use **New Show** deliberately. Confirm the Show name and the prepared
VC controls before recording.

**Expected:** the intended Show and its control bindings are unambiguous.

**Troubleshooting:** the **Record Panel** widget, with **Create Scene** and
**Start Chaser / Stop Chaser**, is a different feature. Its
`vc_prepare_record_panel` MCP tool does not arm the shared Show command
**Record** switch used here. Do not replace the live workspace to prepare a demo.

<a id="rec-2"></a>
### 09 / REC-2 — Arm the shared Record switch

**Action:** turn **Record** on and read the full target/transport status, for
example the source-defined pattern **REC: Recording - [Show name] - Stopped**.
This pattern is an expected label, not an observed take.

**Expected:** the resolved Show is named. Arming alone does not start playback.
Without a resolved Show, **Armed / waiting for a show** is a waiting state, not
a captured take. A bound recorder can accept actions at a frozen paused/stopped
position, so do not operate controls accidentally before Play.

**Troubleshooting:** a focused Record switch consumes Space to toggle Record.
Use the labelled transport buttons rather than guessing the shortcut's target.
Do not change Shows mid-take for this basic exercise.

Sources for REC-1–2: [shared Record UI](../qmlui/qml/RecordControl.qml),
[recorder contract](../qmlui/showcommandrecorder.h).

<a id="rec-3"></a>
### 10 / REC-3 — Capture a short take

**Action:** with Record armed on the intended Show, click **Play or resume**.
Operate the prepared Toggle On, move the compatible slider, then turn the Toggle
Off. Turn **Record off** before **Stop or rewind**. Confirm Record really is off.

**Expected:** **Recordings** contains accepted button-state events and slider
values at Show times. Existing events remain: capture is **additive**, not
replacement. Keep the actual control names, times and values as evidence.

**Troubleshooting:** Stop or Pause alone does not disarm Record. Re-arming over
old material can add another take; this tutorial does not clear existing data.
If disarm is refused or Record remains on, read the error before proceeding.
Do not treat a moving VC control as proof that an event was authored.

<a id="rec-4"></a>
### 11 / REC-4 — Edit one recorded event

**Action:** with local transport stopped for clarity, open **Recordings** or
**Split**. Inspect **Time**, **Control**, **Action** and **Value**. Note one
event's identity and before-value. Double-click its **Time** or compatible scalar
**Value** cell; **Enter** commits and **Escape** cancels. Re-read that event and
unrelated rows after committing. For a selected offset, use **Move by →
Time delta (s)** and the move direction controls.

**Expected:** the chosen event changes; unrelated events remain. A cancellation
leaves the data unchanged. Invalid time/value input has an error rather than
being presented as an accepted edit.

**Troubleshooting:** choose an ordinary scalar row for the basic exercise.
Compound values use a typed draft editor, which may have **Commit**.
Musical moves/Snap require a suitable tempo/grid; a positive time delta in
seconds (up to three decimal places) does not require a musical grid.
Recorded events remain editable while playing, paused, recording and under
Perform; the basic stopped example is a teaching choice, not that editor's gate.

Sources: [recorded-event editor](../qmlui/qml/showmanager/ShowCommandList.qml),
[Show views/transport](../qmlui/qml/showmanager/ShowManager.qml).

<a id="rec-5"></a>
### 12 / REC-5 — Replay with Record off

**Action:** confirm **Record off** and **Perform off**. Use **Stop or rewind**
to return to the beginning, then **Play or resume**. Observe the prepared
controls as playback encounters the edited event's actual timestamp.

**Expected:** the event replays through its current compatible VC binding at
the edited time. Compare the saved edit with observed behavior and keep a dated
receipt. UI replay is not proof of physical output or timing feel.

**Troubleshooting:** missing, disabled, ambiguous or incompatible targets can
refuse/skip execution with a reason while the authored event remains.
Starting at time T does not dispatch the whole earlier prefix. Seeking during
playback can execute crossed events; backward movement can restore this
traversal's own supported changes. Seeking is not passive navigation or a
complete lighting-state rewind. Do not seek for a screenshot.

Source: [command playback and position behavior](show-command-recording.md).

<a id="rec-6"></a>
### 13 / REC-6 — Diagnose and preserve

**Action:** inspect **Debug** and, when present, **Problems: n** and its reason.
In the viewer's own authorized session, disarm Record, save the workspace, and
plan a separate safe reopen to check persistence.

**Expected:** failures identify the unresolved/incompatible control or operation.
Saving preserves Show commands together with their VC control identities in the
workspace; a successful reopen is a separate observation, not implied by Save.

**Troubleshooting:** a checkpoint/disarm error can leave Record on. Do not
assume capture ended; read the actual error. Keep the Show and its VC targets
together rather than assuming the recording is a frozen lighting snapshot.
Random/audio-reactive functions need not reproduce identical output.

Further detail: [Show command tracks](show-command-recording.md) and
[VC performance recording requirements](vc-performance-recording/requirements.md).
This guide intentionally omits recording-format and engine internals.

## Manual-check ledger

All native checks below are initially **pending** (unrun) or **blocked** (a named
prerequisite/authorization is missing). No check is passed. The 2026-10-18 passive
inventory above is an availability observation only; it is not a native test.

For each executed check, replace the observation with the actual result and
record **evidence reference, date, executable/build identity and reviewer**.
Use `passed` only when that evidence meets the expected outcome; use `failed`
for observed mismatches. A source citation, mock, static sketch or screenshot
alone cannot pass an active behavior check. Keep blocked checks blocked until
their owner supplies the missing authorization/state. The final column is
deliberately blank of execution details (`—`) until a reviewer runs the check.

| Callout / step | Manual check, including edge | Initial status | Actual observation / blocker | Evidence; date / build / reviewer |
|----------------|------------------------------|----------------|------------------------------|----------------------------------|
| 01 / MCP-1 | Identify executable and actual endpoint, including custom/disabled case | blocked | No current QLC+ process reported; running identity unavailable | —; — / — / — |
| 02 / MCP-2 | Discover allowed tool using HTTP and `mcpServers`, not CLI `servers` | pending | No client configuration or discovery performed | —; — / — / — |
| 03 / MCP-3 | Summary matches intended workspace; preserve zeros/errors truthfully | blocked | No TCP9696 listener reported; no request sent | —; — / — / — |
| 04 / DJ-1 | Distinguish Idle, Listening and Connected; identify configured 8050 path | blocked | No running window; opening/patching input not authorized here | —; — / — / — |
| 05 / DJ-2 | Correct song and changing position across timed samples; distinguish missing grid | blocked | No native/VDJ session observed; playback not authorized here | —; — / — / — |
| 06 / DJ-3 | Song resolves to intended Show; missing audio is not a successful creation | blocked | No mapping observed; ingestion/assignment/selection not authorized here | —; — / — / — |
| 07 / DJ-4 | Live follows deck, pause suspends, no Show stays Armed, off releases ownership | blocked | No Perform observation; toggles/transport not authorized here | —; — / — / — |
| 08 / REC-1 | Intended Show and actual Toggle/slider bindings identified | blocked | No prepared controls observed; selection/creation not authorized here | —; — / — / — |
| 09 / REC-2 | Named target without starting clock; no-Show arm waits | blocked | No recorder state observed; arming not authorized here | —; — / — / — |
| 10 / REC-3 | Short take adds events; Record off ends capture independently of Stop | blocked | No take performed; controls/recording/output not authorized here | —; — / — / — |
| 11 / REC-4 | Commit changes one event, Escape preserves it, invalid input is refused | blocked | No rows/edit observed; editing not authorized here | —; — / — / — |
| 12 / REC-5 | Record off; replay encounters the edited event at its new time/value | blocked | No replay observed; transport/seek/output not authorized here | —; — / — / — |
| 13 / REC-6 | Debug/Problems reason matches actual failure; Record status checked | blocked | No diagnostics observed; no errors induced | —; — / — / — |

Record these independently; none is implied by a preceding UI check:

| Additional check | Status | Actual result / evidence | Date / build / reviewer |
|------------------|--------|--------------------------|-------------------------|
| Save after disarming | blocked | Save not authorized/performed here | — / — / — |
| Safe reopen preserves commands and compatible VC targets | blocked | Workspace reopen not authorized/performed here | — / — / — |
| Optional VDJ beat grid/cue markers | pending | No database/grid observation | — / — / — |
| Audio quality/sync and subjective timing | pending | No listening/native timing check | — / — / — |
| MIDI/DMX/physical fixtures | blocked | No hardware-output grant or test | — / — / — |

Review record template: `step; status; actual vs expected; evidence reference;
date/time; executable/build identity; reviewer; blocker and responsible owner
if unresolved`. Do not copy unrelated sign-offs from `MANUAL_REVIEW.md`.

## Source and delivery status

Source basis: repository HEAD
`c3353a4f2ce676df9be11eca92866f68db171ab2` on `mcp-server`, reviewed 2026-10-18.
The links above identify the implementation and existing focused docs. They are
source-derived expectations, not proof of an installed binary or live plugin.
The running-build match is unknown because no running process was reported.

The guide stands alone. Final video, screenshots of the required DJ/Perform and
record/edit/replay states, and all active native checks remain unavailable or
unverified. No missing assets are linked and no fabricated app imagery or query
results substitute for them.
