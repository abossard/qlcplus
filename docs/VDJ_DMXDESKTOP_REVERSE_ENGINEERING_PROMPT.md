# VDJ ↔ QLC+ Telemetry Gap-Fill: Research Prompt

This file is a **self-contained prompt** for a future Claude Code session that
runs on your machine (with VirtualDJ and DMXDesktop actually installed and
reachable). Open this repo in Claude Code, then paste the prompt below into a
new session.

---

## Background (what's already done)

This branch (`claude/vdj-song-manager-poc-SqKCZ`) added a VDJ telemetry
pipeline based on the **OS2L** protocol (the open standard QLC+ already speaks
via `plugins/os2l/`). The pipeline is:

```
VirtualDJ ──OS2L (JSON over TCP, port 9996)──► plugins/os2l/os2lplugin.cpp
                                                       │
                                                       ▼
                                            songReceived(QVariantMap)
                                            beatInfoReceived(bpm,pos,change)
                                                       │
                                                       ▼
                                            qmlui/vdjbridge.cpp  (Qt facade)
                                                       │
                                                       ▼
                                            ShowManager.slotVdjSongChanged
                                            ShowManager.qml telemetry strip
```

The QML telemetry strip shows: connection status, current BPM, and a
beat-pulse indicator. That's all that is wired today.

## The gap

Stock VirtualDJ + OS2L only broadcasts `evt:"beat"` (with optional
`bpm`, `pos`, `change`). `evt:"btn"` and `evt:"cmd"` exist in the spec
but fire only when the user has written a corresponding VDJ script
calling `os2l_button` / `os2l_cmd`. `evt:"song"` is documented by some
sources but **VirtualDJ does not actually broadcast it** — confirmed
by the project owner. The OS2L plugin's `song`-parsing code path
therefore never runs in practice and we have removed any qmlui-side
consumers that depended on it.

This means OS2L gives us beats and nothing more. It **does not** give
us:

- Anything that identifies the current track (title, artist, file path,
  database id).
- The full **beat grid** (an array of beat times) — only individual
  `beat` events arrive as they happen, which is fine for live sync but
  not for pre-planning cues on a timeline ahead of playback.
- Loops, hot cues, deck-internal markers.

**Plan from the user:** DMXDesktop (the lighting app from VDJ's own ecosystem)
talks to VirtualDJ over a richer, non-OS2L protocol that exposes the file
path and probably more. We want to reverse-engineer that protocol and add a
second backend that feeds the same `VdjBridge` facade — so consumers
(`ShowManager`, the QML telemetry strip, future MCP tools) don't change.

## Prerequisites on your machine

- VirtualDJ Pro 2024+ installed
- DMXDesktop installed and configured to talk to VDJ
- Wireshark, tcpdump, or equivalent
- A few audio tracks loaded into VDJ
- This repo built (`cd build && cmake --build . --target qlcplus5 -j8`)

---

## ===== Prompt to paste into a new Claude Code session =====

> I need help reverse-engineering the network protocol DMXDesktop uses to talk
> to VirtualDJ, so I can add a second backend to my QLC+ `VdjBridge` that
> fills in the gaps OS2L doesn't cover — specifically the **audio file path**
> of the playing track and, if possible, the **full beat grid**.
>
> Read `docs/VDJ_DMXDESKTOP_REVERSE_ENGINEERING_PROMPT.md` for full background.
> The existing OS2L-based pipeline lives in:
>
> - `plugins/os2l/os2lplugin.{h,cpp}` — emits
>   `beatInfoReceived(double,double,bool)`. Do NOT extend this plugin
>   with non-OS2L data; OS2L stays strictly conformant.
> - `qmlui/vdjbridge.{h,cpp}` — the Qt facade. Add a second backend here,
>   not in the OS2L plugin. The facade currently exposes only beat /
>   BPM / connection state; you will add song / path / beatgrid
>   properties as the new backend provides them.
> - `qmlui/showmanager.cpp` — does NOT yet contain any auto-create logic.
>   That work was removed because it depended on VDJ-broadcasted song
>   events that don't actually exist. Once a new backend supplies song
>   metadata + file path, the auto-create-show / attach-Audio-function
>   logic needs to be (re-)added here.
>
> ### Step 1 — Capture
>
> 1. Identify what DMXDesktop listens on (port / interface). Likely
>    candidates: a TCP/UDP port on localhost, a websocket, or a named pipe.
>    Start with `lsof -i -P -n | grep -i dmxdesktop` (mac) or
>    `netstat -anp | findstr dmxdesktop` (windows).
> 2. Start a Wireshark capture on `lo`/loopback filtered to the DMXDesktop
>    process, or use `tcpdump -i lo0 -A -s 0 port <port>` to a pcap file.
> 3. In VDJ: load a track on deck 1, start playing, scrub, change BPM,
>    activate a hot cue, set a loop, switch decks, change track. Each
>    distinct action becomes a labelled section of your capture.
> 4. Save the pcap as `vdj-dmxdesktop.pcap` somewhere I can read.
>
> ### Step 2 — Decode
>
> Read `vdj-dmxdesktop.pcap` with tshark or scapy. Most likely the wire
> format is one of: JSON-over-TCP (line-delimited), length-prefixed binary
> structs, MessagePack, or a WebSocket text frame stream. For each message
> type observed, produce a struct-style description: name, fields, types,
> typical values, frequency. Pay special attention to:
>
> - A message that arrives once per track load — should contain the file
>   path or at least a stable track id we can resolve via VDJ's database
>   (`VirtualDJ Database v6.xml` / `database.xml`).
> - A message containing a beat-time array or a CBG (computed beat grid).
> - Any handshake / hello / version-negotiation frames.
>
> Compare each message to the OS2L spec — anything OS2L can already deliver
> should be ignored (we already get it).
>
> ### Step 3 — Specify
>
> Write a markdown spec at `docs/DMXDESKTOP_PROTOCOL.md` containing:
>
> - Transport details (port, framing, handshake).
> - One section per message type with example payloads.
> - A mapping table: which DMXDesktop fields fill which `VdjBridge` slots
>   (specifically `path`, `bpm`, `pos`, `elapsed`, `duration`, `name`,
>   `artist`, plus any new fields like `beatgrid`, `loops`, `cues`).
> - Open questions (anything not figured out yet).
>
> Stop and show me the spec before writing C++ code — I want to sanity-check
> before we commit to a backend implementation.
>
> ### Step 4 — Implement the second backend
>
> Add a sibling to the OS2L plugin: probably a new `plugins/vdjbridge/`
> plugin that implements `QLCIOPlugin` and emits the same
> `songReceived(QVariantMap)` / `beatInfoReceived(...)` signals, with the
> QVariantMap fully populated (including `path` and, if available, a
> `beatgrid` array). Then in `qmlui/app.cpp` look it up the same way the
> OS2L plugin is looked up and call
> `m_vdjBridge->attachOS2LPlugin(plugin)` — rename the method to
> `attachVdjPlugin` if both can be present at once (priority: richer
> backend wins). Add a unit test that pipes a captured frame through
> the parser and asserts `songReceived` carries the expected map.
>
> ### Step 5 — Beat grid in Song Manager
>
> Once the QVariantMap has a `beatgrid` array (list of beat times in ms),
> extend `qmlui/qml/showmanager/ShowManager.qml` to overlay tick marks at
> those positions on the timeline (in addition to the BPM-derived grid that
> already renders). Add a "snap to beat" toggle.
>
> ### Constraints
>
> - **Do not** change `plugins/os2l/` to carry new fields beyond OS2L. The
>   user wants OS2L to stay strictly conformant.
> - Keep everything additive: existing Shows / playback / VC behaviour
>   must not change.
> - One commit per step. Build (`cmake --build build --target qlcplus5`)
>   and run `vdjbridge_test` between commits.

## Notes for future-me

- If DMXDesktop turns out to use proprietary obfuscation that isn't worth
  reverse-engineering, the fallback is to install a **VirtualDJ custom
  plugin** (VDJ exposes a C++ SDK) that publishes the missing fields over
  a side channel of our own design. That's a lot more work than a backend
  decoder but is a known-good escape hatch.
