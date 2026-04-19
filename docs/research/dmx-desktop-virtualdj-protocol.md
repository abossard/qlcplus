# DMX Desktop ↔ VirtualDJ Protocol: Reverse-Engineering Report

> **Date:** 2026-04-19
> **Author:** Research via live traffic capture on macOS
> **Context:** Investigating how DMX Desktop receives rich track metadata from VirtualDJ, while QLC+'s OS2L plugin only receives beat events.

## Executive Summary

DMX Desktop communicates with VirtualDJ via a **proprietary localhost WebSocket** protocol on a dynamically assigned port (observed: **52725**), completely separate from the OS2L TCP protocol (port 9996). VirtualDJ sends all data — including full track metadata, mixer state, VU meters, and beat taps — through this WebSocket channel. The standard OS2L protocol (`{"evt":"beat"}` over TCP) is limited to beat-sync only; **VirtualDJ does not send `song`, `btn`, or `cmd` events over OS2L to QLC+**.

This means DMX Desktop's rich integration is not achieved through OS2L but through an undocumented VirtualDJ plugin API that exposes a WebSocket server on localhost.

## Architecture Overview

```
┌──────────────────────┐         OS2L TCP (port 9996)          ┌──────────────┐
│                      │ ──── {"evt":"beat","bpm":123} ──────▶ │   QLC+       │
│                      │         (beat only!)                  │  (os2lplugin)│
│     VirtualDJ        │                                       └──────────────┘
│  (macOS, PID 37791)  │
│                      │   Proprietary WebSocket (localhost)    ┌──────────────┐
│                      │ ════ Full JSON messages ════════════▶ │ DMX Desktop  │
│                      │      (port 52725, bidirectional)       │  (Electron)  │
└──────────────────────┘                                       └──────────────┘
         │                                                            │
         │  Bonjour (_os2l._tcp)                                      │
         │  for service discovery                                     │
         │                                                            │
         ▼                                                            ▼
    mDNS (UDP 5353)                                          Art-Net (UDP 6454)
                                                             → DMX fixtures
```

## Protocol Details

### Transport Layer

| Property | OS2L (QLC+) | DMX Desktop WebSocket |
|----------|-------------|----------------------|
| **Transport** | TCP | WebSocket over TCP |
| **Port** | 9996 (fixed) | Dynamic (observed: 52725) |
| **Discovery** | Bonjour `_os2l._tcp` | Unknown (likely VDJ plugin registration) |
| **Direction** | VDJ → QLC+ (unidirectional) | Bidirectional |
| **Binding** | Any interface | localhost only (::1) |
| **Message format** | `{"evt":"..."}` JSON | `{"message":"..."}` JSON |
| **Framing** | Raw JSON delimited by `}` | WebSocket frames |

### Message Types Observed

From a 5-minute live capture session with VirtualDJ playing music and user interacting:

| Message Type | Count | Frequency | Purpose |
|-------------|-------|-----------|---------|
| `TAP` | ~600 | ~2/sec (every beat) | Beat synchronization |
| `DJAPP_PLUGIN_DATA` | 212 | ~0.7/sec | Full deck state + track metadata |
| `performance-stats` | ~30 | ~0.1/sec | CPU, memory, disk stats |
| `masterDimmer` | 1+ | On change | Master light intensity |
| `darkOnSilenceInactive` | 1+ | On change | Silence detection state |
| `os2lBpm` | 1+ | On change | Global BPM value |
| `online` | 1+ | On connect | Connection status |

### Message Type 1: `TAP` (Beat Sync)

Sent on every musical beat. This is the WebSocket equivalent of OS2L's `{"evt":"beat"}`.

```json
{
  "message": "TAP",
  "data": 123,
  "ts": 82139891.950083,
  "tapCount": 1
}
```

| Field | Type | Description |
|-------|------|-------------|
| `message` | string | Always `"TAP"` |
| `data` | number | Current BPM |
| `ts` | number | Timestamp (VDJ internal clock, milliseconds) |
| `tapCount` | int | Beat position within the bar (1–4) |

**Key difference from OS2L:** The `TAP` message includes BPM and beat position within the bar, while OS2L's `beat` event carries no additional data.

### Message Type 2: `DJAPP_PLUGIN_DATA` (Full State)

The richest message — sent approximately every 1.4 seconds. Contains **complete state of all decks, mixer, and playback**.

```json
{
  "message": "DJAPP_PLUGIN_DATA",
  "plugin": "os2l",
  "config": {
    "trackData": true,
    "masterLink": true,
    "numDecks": 2,
    "mixerOrder": "1234"
  },
  "data": {
    "1": { /* Deck 1 state */ },
    "2": { /* Deck 2 state */ }
  },
  "master_volume": 1,
  "crossfader": 0.5,
  "vuMeter": 0.050113,
  "deck1VuMeter": null,
  "deck2VuMeter": 0.050113,
  "deck3VuMeter": null,
  "deck4VuMeter": null,
  "deck1VuMeterPreFader": 0,
  "deck2VuMeterPreFader": 0.050113,
  "deck3VuMeterPreFader": 0,
  "deck4VuMeterPreFader": 0,
  "playState": {
    "isPlaying": false,
    "isPaused": false,
    "autoplayActive": false,
    "currentPlaylist": null,
    "currentTrackIndex": -1,
    "masterDeck": 2
  },
  "globalState": {
    "crossfader": 0.5,
    "crossfaderResult": 0.511903,
    "headphoneVolume": 1,
    "masterDeck": 0,
    "masterVolume": 1,
    "mixerOrder": "1234",
    "numDecks": 2,
    "vuMeter": 0.050113
  },
  "deck1Loaded": true,
  "deck2Loaded": true,
  "deck3Loaded": true,
  "deck4Loaded": true,
  "deck1Loading": false,
  "deck2Loading": false,
  "deck3Loading": false,
  "deck4Loading": false,
  "deck1ShowLoaded": false,
  "deck2ShowLoaded": true,
  "deck3ShowLoaded": false,
  "deck4ShowLoaded": false,
  "deck1ShowSource": "auto",
  "deck2ShowSource": "auto",
  "deck3ShowSource": null,
  "deck4ShowSource": null
}
```

#### Per-Deck Data Schema

Each deck (keyed `"1"`, `"2"`, etc.) contains:

```json
{
  "artist": "Play-N-Skillz, Gente De Zona &",
  "title": "Somos Latinos (DJ Nasa Brake I",
  "album": "Think U The Shit (Fart)",
  "title_artist": "Somos Latinos (DJ Nasa Brake I - Play-N-Skillz, Gente De Zona &",
  "elapsedTimeStr": "0m 0s",
  "totalTimeStr": "3m 7s",
  "bpm": 123.001221,
  "key": "04B",
  "isPlaying": false,
  "elapsedTime": 0.024,
  "totalTime": 187.405,
  "position": 0.000137,
  "loading": false,
  "level": 0.386047,
  "eqHigh": 0.5,
  "eqMid": 0.5,
  "eqLow": 0.5,
  "gain": 0.5,
  "beatPosition": -32.031918,
  "filepath": "/path/to/file.mp3",
  "isMaster": false,
  "hasVideo": false,
  "videoCodec": null,
  "hasCDG": false,
  "cdgPath": null
}
```

| Field | Type | Description |
|-------|------|-------------|
| `artist` | string | Track artist (from ID3 tags) |
| `title` | string | Track title |
| `album` | string | Album name |
| `title_artist` | string | Combined "Title - Artist" string |
| `elapsedTimeStr` | string | Human-readable elapsed time ("0m 15s") |
| `totalTimeStr` | string | Human-readable total duration ("4m 16s") |
| `bpm` | float | Precise BPM (e.g., 123.001236) |
| `key` | string | Camelot key notation (e.g., "04B", "05B") |
| `isPlaying` | bool | Whether this deck is currently playing |
| `elapsedTime` | float | Elapsed time in seconds |
| `totalTime` | float | Total duration in seconds |
| `position` | float | Playback position (0.0–1.0) |
| `loading` | bool | Whether a track is being loaded |
| `level` | float | Volume/output level (0.0–1.0) |
| `eqHigh` | float | EQ high band (0.0–1.0, 0.5 = center) |
| `eqMid` | float | EQ mid band (0.0–1.0) |
| `eqLow` | float | EQ low band (0.0–1.0) |
| `gain` | float | Gain/trim (0.0–1.0) |
| `beatPosition` | float | Position within beat grid |
| `filepath` | string | Full filesystem path to the audio file |
| `isMaster` | bool | Whether this deck is the sync master |
| `hasVideo` | bool | Whether the track has a video component |
| `videoCodec` | string? | Video codec if applicable |
| `hasCDG` | bool | Whether the track has CDG (karaoke) data |
| `cdgPath` | string? | Path to CDG file |

### Message Type 3: `os2lBpm`

Sent when the global BPM changes.

```json
{
  "message": "os2lBpm",
  "data": 123
}
```

### Message Type 4: `masterDimmer`

Controls the master light intensity.

```json
{
  "message": "masterDimmer",
  "value": 100
}
```

| Field | Type | Description |
|-------|------|-------------|
| `value` | int | Master dimmer level (0–100) |

### Message Type 5: `darkOnSilenceInactive`

Indicates that the "dark on silence" feature is not active.

```json
{
  "message": "darkOnSilenceInactive"
}
```

### Message Type 6: `online`

Connection heartbeat/status.

```json
{
  "message": "online",
  "data": true
}
```

### Message Type 7: `performance-stats`

DMX Desktop system health monitoring (not related to DJ data).

```json
{
  "message": "performance-stats",
  "data": {
    "cpu": {
      "current": 0.38,
      "average": 1.01,
      "singleCore": 3.82,
      "cores": 10
    },
    "memory": {
      "process": { "rss": 344.42, "rssPercent": 0.53 },
      "heap": { "used": 0.089, "total": 0.106, "percent": 83.56 },
      "system": { "total": 65536, "used": 36434, "free": 1770 }
    },
    "disk": {
      "total": 1858.19,
      "used": 1409.60,
      "free": 448.60
    },
    "timestamp": 1776586919356
  }
}
```

## OS2L vs DMX Desktop Protocol Comparison

| Feature | OS2L (TCP 9996) | DMX Desktop (WS localhost) |
|---------|-----------------|---------------------------|
| Beat sync | ✅ `{"evt":"beat"}` | ✅ `{"message":"TAP","data":123,"tapCount":1}` |
| BPM value | ❌ Not included | ✅ In TAP + DJAPP_PLUGIN_DATA |
| Beat position in bar | ❌ | ✅ `tapCount` (1–4) |
| Track title | ❌ Not sent by VDJ | ✅ Per-deck metadata |
| Artist | ❌ | ✅ |
| Album | ❌ | ✅ |
| Musical key | ❌ | ✅ (Camelot notation) |
| Elapsed/total time | ❌ | ✅ |
| Play/pause state | ❌ | ✅ |
| Crossfader position | ❌ | ✅ |
| EQ values (H/M/L) | ❌ | ✅ Per-deck |
| VU meters | ❌ | ✅ Per-deck, pre/post-fader |
| Master volume | ❌ | ✅ |
| Deck loading state | ❌ | ✅ |
| File path | ❌ | ✅ |
| Video/CDG data | ❌ | ✅ |
| Button events | Spec supports it | Not observed |
| Command events | Spec supports it | Not observed |
| Song events | Spec supports it | Not sent by VDJ over OS2L |
| Master dimmer | ❌ | ✅ `masterDimmer` |
| Silence detection | ❌ | ✅ `darkOnSilenceInactive` |

## How DMX Desktop Connects

DMX Desktop is an **Electron app** (Chromium + Node.js). Based on analysis:

1. **DMX Desktop starts a WebSocket server** on a dynamic port (observed: 52725) on localhost.
2. **VirtualDJ connects to it** — likely via a VDJ plugin that registers with the DJ software's internal plugin system. The `DJAPP_PLUGIN_DATA` messages have `"plugin":"os2l"` in the payload, suggesting VDJ internally routes OS2L data through its plugin infrastructure.
3. **Communication is bidirectional** — DMX Desktop sends `performance-stats` back to VDJ (or to its own renderer process).
4. **The WebSocket runs on `::1` (IPv6 localhost)** — this is strictly a local communication channel.

### DMX Desktop App Structure

```
/Applications/DMXDesktop.app/
├── Contents/
│   ├── MacOS/DMXDesktop              # Electron main process
│   ├── Frameworks/
│   │   ├── DMXDesktop Helper.app      # GPU, network, renderer processes
│   │   └── Electron Framework.framework/
│   └── Resources/
│       ├── app.asar                   # Packaged application code
│       └── app.asar.unpacked/
│           ├── bin/
│           │   ├── dmxengine.node     # Native DMX output module
│           │   ├── authcore.node      # Licensing/auth
│           │   ├── skey/              # Musical key detection (ONNX ML models)
│           │   └── ffmpeg/            # Audio processing
│           └── node_modules/
│               ├── better-sqlite3    # Local database
│               ├── koffi             # FFI for native code
│               ├── node-hid          # USB HID device access
│               └── macos-alias       # macOS alias resolution
```

Key observations:
- **`dmxengine.node`** — Native Node.js addon for DMX output (likely Art-Net/USB)
- **`skey/`** — Contains ONNX machine learning models for musical key detection, suggesting DMX Desktop does its own audio analysis
- **`node-hid`** — For USB DMX interface communication
- **`better-sqlite3`** — Local database for show/fixture data

## Network Ports Summary

| Port | Protocol | Process | Purpose |
|------|----------|---------|---------|
| 5353 | UDP | DMX Desktop | mDNS/Bonjour (service discovery) |
| 6454 | UDP | DMX Desktop | Art-Net (DMX over network) |
| 9996 | TCP | QLC+ | OS2L protocol (beat sync only) |
| 52725 | TCP/WS | DMX Desktop | WebSocket (full VDJ integration) |
| 9999 | TCP/HTTP | QLC+ | Web access interface |

## Implications for QLC+

### Why QLC+ Only Gets Beats

VirtualDJ's OS2L implementation over TCP port 9996 is **beat-only**. The rich data (track metadata, mixer state, etc.) flows exclusively through VirtualDJ's internal plugin WebSocket API, which DMX Desktop leverages.

### Options for Getting Rich Data in QLC+

1. **Implement a WebSocket client in QLC+** that connects to the same VDJ plugin WebSocket — this would require reverse-engineering the connection handshake and potentially installing a VDJ plugin.

2. **Build a bridge service** that listens on the VDJ plugin WebSocket and translates `DJAPP_PLUGIN_DATA` into standard OS2L `song` events on TCP port 9996.

3. **Request VirtualDJ enhancement** — ask Atomix (VDJ developer) to send `song` events over the standard OS2L TCP channel.

4. **Use VDJ scripting** — VirtualDJ has a scripting engine that can send custom OS2L messages. A VDJscript could be written to emit `{"evt":"song",...}` on track load.

## Capture Methodology

All data in this report was captured live on 2026-04-19 using:

```bash
# Bonjour service discovery
dns-sd -B _os2l._tcp

# Process and port inspection
lsof -i -P -n | grep -i dmx
ps aux | grep -i dmx

# Live TCP traffic capture (DMX Desktop WebSocket)
sudo tcpdump -i lo0 -A -s 0 'tcp port 52725'

# OS2L traffic capture
sudo tcpdump -i any -A -s 0 'tcp port 9996'

# JSON extraction and analysis
# Python script to parse balanced JSON from tcpdump output
```

Environment:
- **macOS** (Darwin)
- **VirtualDJ** (PID 37791, running on 10.0.0.66)
- **DMX Desktop** v1.0.49 (Electron app, PID 42343)
- **QLC+** 5.2.2-GIT (with OS2L Bonjour + diagnostics)

## References

- [OS2L Official Specification](https://os2l.org) — Protocol definition for `evt` messages (beat, btn, cmd, song)
- [VirtualDJ OS2L Documentation](https://www.virtualdj.com/wiki/OS2L.html) — VDJ's OS2L implementation notes
- [DMX Desktop](https://www.dmxdesktop.com/) — Commercial DMX lighting software with VDJ integration
- [DMX Desktop Features](https://www.dmxdesktop.com/features) — DJ Mode, AI-powered show generation
- [Apple DNS-SD API](https://developer.apple.com/documentation/dnssd) — Bonjour service registration
- [VirtualDJ Developer Wiki](https://www.virtualdj.com/wiki/Developers.html) — Plugin SDK documentation
- [RFC 6762 — mDNS](https://tools.ietf.org/html/rfc6762) — Multicast DNS protocol
- [RFC 6763 — DNS-SD](https://tools.ietf.org/html/rfc6763) — DNS-Based Service Discovery
- [Art-Net Protocol](https://art-net.org.uk/) — DMX over Ethernet (UDP 6454)

## Confidence Assessment

| Claim | Confidence | Basis |
|-------|------------|-------|
| DMX Desktop uses WebSocket, not OS2L TCP | **High** — directly observed in `lsof` and `tcpdump` | Live traffic capture |
| `DJAPP_PLUGIN_DATA` schema | **High** — captured complete JSON payloads | Multiple samples over 5 minutes |
| VDJ only sends `beat` over OS2L TCP | **High** — confirmed via both `tcpdump` on port 9996 and QLC+ diagnostics | Exhaustive capture, no non-beat events |
| DMX Desktop is Electron-based | **High** — process tree shows Chromium helpers, app.asar structure | Process inspection |
| WebSocket port is dynamic (not fixed) | **Medium** — observed 52725, but may change across sessions | Single capture session |
| VDJ plugin mechanism for WebSocket | **Medium** — inferred from `"plugin":"os2l"` in message + localhost binding | Message analysis + process inspection |
| `performance-stats` is DMX Desktop internal | **Medium** — sent on WebSocket but contains system metrics, not DJ data | Content analysis |
