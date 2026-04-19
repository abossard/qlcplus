# OS2L Plugin — Bonjour Auto-Discovery for macOS

## Overview

The OS2L (Open Sound 2 Light) plugin enables communication between QLC+ and
DJ software such as [VirtualDJ](https://www.virtualdj.com/).

On **macOS**, the plugin automatically registers QLC+ as a Bonjour service
(`_os2l._tcp`) so that VirtualDJ can discover it when OS2L is set to **Auto**.
No manual IP configuration is required.

## How It Works

1. When the OS2L input is opened, QLC+ starts a TCP server on port **9996**
   and registers the service via Bonjour (macOS native `dns_sd.h` API).
2. VirtualDJ detects the `_os2l._tcp` service on the local network.
3. VirtualDJ connects to QLC+ and starts sending OS2L messages (JSON over TCP).
4. QLC+ parses the messages and translates them to lighting events.

```
VirtualDJ                         QLC+
   |                                |
   |  Bonjour browse _os2l._tcp    |
   |------------------------------->|  (registered via DNSServiceRegister)
   |                                |
   |   TCP connect to port 9996    |
   |------------------------------->|
   |                                |
   |   {"evt":"beat"}              |
   |   {"evt":"btn","name":...}    |
   |   {"evt":"song","name":...}   |
   |------------------------------->|  -> QLC+ input events
```

## VirtualDJ Setup

Set OS2L to **Auto** in VirtualDJ settings:

1. Open VirtualDJ settings → Options → search for `os2l`.
2. Set `os2lDirectIp` to **Auto** (or leave empty).
3. VirtualDJ will find QLC+ via Bonjour — no IP needed.

If Auto mode is unavailable or you are not on macOS, enter the IP:port
manually (e.g. `127.0.0.1:9996`).

## QLC+ Setup

1. Open the Input/Output panel.
2. Enable the OS2L plugin on a universe.
3. The plugin starts listening on TCP port 9996 and registers via Bonjour.
4. Run with `-d` flag to see detailed OS2L logs in the console.

## Web Diagnostics Dashboard

When running with `-d` (debug mode) and `-w` (web access), a live diagnostics
dashboard is available at:

- **Dashboard:** `http://localhost:9999/os2l`
- **JSON API:** `http://localhost:9999/os2l.json`

The dashboard shows:

- **Connection status** — Bonjour registration, TCP client connection
- **Live event log** — scrolling table of all OS2L events with timestamps
- **Message statistics** — counts per event type (beat, btn, cmd, song)
- **Song metadata** — currently playing track info (when VDJ sends song events)
- **Beat indicator** — flashes on each beat event

A link to the dashboard also appears in the main QLC+ web interface menu
bar (only when running with `-d`).

The dashboard polls `/os2l.json` every 500ms. The plugin maintains a
ring buffer of the last 1000 diagnostic events.

## Show Control via OS2L

VirtualDJ can trigger QLC+ functions via OS2L button events:

| VDJ Script | OS2L Message | QLC+ Action |
|------------|-------------|-------------|
| `os2l_button 'fog'` | `{"evt":"btn","name":"fog","state":"on"}` | VCButton activates → starts Scene |
| `os2l_button 'fog'` (again) | `{"evt":"btn","name":"fog","state":"off"}` | VCButton deactivates → stops Scene |
| `os2l_button 'strobe' while_pressed` | on while held, off on release | VCButton flash mode |
| `os2l_cmd 1 0.5` | `{"evt":"cmd","id":1,"param":0.5}` | VCSlider → fader control |

### Song-Triggered Light Shows

Use VDJ's `on_load` script to send the song title as a button event:

```
on_load ? os2l_button get_title
```

In QLC+, create VCButtons in a **SoloFrame** (so only one runs at a time),
each bound to the OS2L channel for a song title. When a song loads in VDJ,
the matching button fires and starts its chaser/scene.

## Supported OS2L Events

All event types and fields are documented at
[https://os2l.org](https://os2l.org) and in the
[VirtualDJ OS2L wiki](https://www.virtualdj.com/wiki/OS2L.html).

| Event  | Description | Key Fields |
|--------|-------------|------------|
| `btn`  | Button press/release | `name`, `state` ("on"/"off") |
| `cmd`  | Numeric command | `id` (int), `param` (float 0.0–1.0) |
| `beat` | BPM synchronization beat | — |
| `song` | Track metadata | See table below |

### Song Metadata Fields

| Field | Type | Source | Description |
|-------|------|--------|-------------|
| `name` | string | OS2L spec / VDJ wiki | Track title |
| `artist` | string | OS2L spec / VDJ wiki | Artist name |
| `album` | string | VDJ wiki | Album name |
| `genre` | string | VDJ wiki | Music genre |
| `year` | string | VDJ wiki | Release year |
| `remix` | string | VDJ wiki | Remix/edit version |
| `status` | string | OS2L spec | `play`, `pause`, or `stop` |
| `bpm` | number | OS2L spec / VDJ wiki | Beats per minute |
| `key` | string | VDJ wiki | Camelot key notation (e.g. `8B`) |
| `elapsed` | number | VDJ wiki | Elapsed time in seconds |
| `duration` | number | VDJ wiki | Total duration in seconds |
| `deck` | integer | VDJ wiki | Deck number (1 or 2) |

## Platform Notes

| Platform | Bonjour Support | Notes |
|----------|----------------|-------|
| **macOS** | ✅ Native `dns_sd.h` | Zero-configuration; VDJ Auto just works |
| **Linux** | ❌ Stub (no-op) | Use manual `os2lDirectIp` in VDJ |
| **Windows** | ❌ Stub (no-op) | Use manual `os2lDirectIp` in VDJ |

## References

- [OS2L Official Specification](https://os2l.org) — protocol and message types
- [VirtualDJ OS2L Documentation](https://www.virtualdj.com/wiki/OS2L.html) — VDJ-specific fields and Auto mode
- [Apple DNS-SD API](https://developer.apple.com/documentation/dnssd) — `DNSServiceRegister` and related calls
- [RFC 6762 — Multicast DNS](https://tools.ietf.org/html/rfc6762) — mDNS protocol
- [RFC 6763 — DNS-Based Service Discovery](https://tools.ietf.org/html/rfc6763) — `_service._tcp.local.` naming

## License

Apache License 2.0 — part of [QLC+](https://www.qlcplus.org).
