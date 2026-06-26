# DMXDesktop / VirtualDJ Telemetry Protocol

> **This document is derived 100% from a live packet capture** of VirtualDJ ↔ the
> real DMXDesktop app (2026-06-25, localhost, port 8050). Every claim below is
> reproducible from the capture with the `tcpdump` commands in
> [§ Reproduce & prove it](#reproduce--prove-it). Where the wire and a vendor
> doc disagreed, the wire won. QLC+ implements the DMXDesktop-compatible **server**
> role (it sends `subscribe`; VDJ streams the data).

## Overview

VirtualDJ streams real-time deck telemetry to an external receiver (the DMXDesktop
app, or QLC+ which is compatible). The receiver is a TCP **server**; VDJ connects
to it as a **client**, the server sends a `subscribe` request, and VDJ then streams
value updates at the requested frequency.

## Transport

| Property      | Value (observed)                                             |
|---------------|--------------------------------------------------------------|
| Protocol      | **Concatenated JSON objects over TCP** — `{…}{…}{…}` with **no delimiter** between them (occasionally a `\n` appears). NOT strictly newline-delimited. |
| Port          | 8050                                                          |
| Role          | Receiver (DMXDesktop / QLC+) is the **TCP server**; VDJ is the **client** |
| Localhost     | Even same-machine, VDJ connects to the host's **LAN IP** (e.g. `192.168.1.118:8050`), which macOS routes through `lo0`. |
| Discovery     | Bonjour/mDNS service `_os2l._tcp` on the same port           |
| Encoding      | UTF-8                                                         |

**Framing — how to split the stream:** VDJ does not delimit messages. Insert a
break at every `}{` boundary and parse each object. QLC+ does exactly this:
`m_lineBuffer.replace("}{", "}\n{")` then splits on `\n`
(`plugins/vdjbridge/vdjtelemetryclient.cpp:163-169`).
> Limitation: a literal `}{` inside a JSON string value (e.g. an odd track title)
> would mis-split. A string-aware/brace-depth splitter would be more robust.

## Connection lifecycle (observed frame directions & sizes)

1. Server listens on 8050. VDJ discovers it (Bonjour) or is configured manually.
2. VDJ opens the TCP connection.
3. **Server → client**: one `subscribe` frame (in the capture: `…8050 → …57522`,
   **3560 bytes**, ~112 triggers).
4. **Client → server**: VDJ streams `subscribed` value-updates and `beat` events
   (`…57522 → …8050`).
5. On disconnect the server resets deck state and keeps listening; VDJ reconnects.

Message tally over a ~5-minute session: `subscribed` **193,367**, `beat` **587**,
`subscribe` **1**, `unsubscribe` **1**.

## Server → client: the subscription request

A single JSON object, sent once on connect:

```json
{"evt":"subscribe","frequency":"25","trigger":["deck 1 get_bpm","deck 1 volume", … ]}
```

| Field       | Type   | Observed                                          |
|-------------|--------|---------------------------------------------------|
| `evt`       | string | `"subscribe"`                                     |
| `frequency` | string | `"25"` (updates per second for continuous fields) |
| `trigger`   | array  | **112 elements** = 26 distinct per-deck triggers × 4 decks + 8 globals |

The per-deck triggers requested (each prefixed `deck N `, N=1–4):
`get_bpm, volume, play, get_key, get_title_artist, get_artist, get_title,
get_album, get_genre, get_position, loaded, get_time, get_time total, eq_high,
eq_med, eq_low, gain, get_filepath, level, get_vu_meter, get_time elapsed absolute,
get_beatpos, get_firstbeat, loop, get_loop`, plus a **`loop_roll … ? constant … :`
VDJScript ternary** (returns the active loop-roll size as one number).
Globals: `master_volume, get_decks, crossfader, headphone_volume, masterdeck,
get_crossfader_result full, mixer_order, get_vu_meter`.

## Client → server: streamed frames

### Value update

```json
{"evt":"subscribed","trigger":"deck 1 get_title","value":"Song Name"}
```

| Field     | Type               | Notes                                   |
|-----------|--------------------|-----------------------------------------|
| `evt`     | string             | `"subscribed"`                          |
| `trigger` | string             | The trigger that was subscribed to      |
| `value`   | number/string/bool | Type depends on the trigger (see below). Numbers arrive as JSON int **or** float interchangeably. |

### Beat event

Real sample from the capture (note field order and that `pos` can be `-1`,
`strength` can be `0`):

```json
{"evt":"beat","change":true,"pos":-1,"bpm":128,"strength":0}
```

| Field      | Type   | Notes                                            |
|------------|--------|--------------------------------------------------|
| `evt`      | string | `"beat"`                                         |
| `change`   | bool   | True when BPM changed since the last beat        |
| `pos`      | int    | Beat counter (can be `-1`)                       |
| `bpm`      | number | Master deck's **pitch-affected** BPM             |
| `strength` | number | Beat strength                                    |

## Trigger reference (value types & units — from the wire)

> Ranges below are the **min..max actually observed** in the capture.

### Per-deck — continuous (streamed at `frequency` Hz)

| Trigger                     | JSON type | Observed range  | Unit / meaning |
|-----------------------------|-----------|-----------------|----------------|
| `get_time`                  | int       | 16,959 … 360,237 | **milliseconds remaining** |
| `get_time elapsed absolute` | int       | **−73** … 311,018 | **milliseconds elapsed** (can be slightly negative pre-start) |
| `get_beatpos`               | float     | **−0.20** … 647.94 | **cumulative beats** from track start (NOT 0–4; can be slightly negative) |
| `get_position`              | float     | 0 … 0.95        | track position fraction 0–1 |
| `get_vu_meter`              | float     | 0 … 1           | VU level |
| `level`                     | float     | 0 … 1           | output level |
| `volume`                    | float     | 0 … 1           | deck fader |

### Per-deck — on load / on change

| Trigger            | JSON type | Notes |
|--------------------|-----------|-------|
| `get_filepath`     | string    | full path of the loaded file |
| `get_title` / `get_artist` / `get_album` / `get_genre` / `get_title_artist` | string | metadata (unloaded deck title = `"Drag a song on this deck to load it"`) |
| `get_bpm`          | float     | **pitch-affected playing BPM** (120 = unloaded default). For the original file BPM, read the file's ID3 `TBPM` tag instead. |
| `get_key`          | string    | musical key (e.g. `"02B"`) |
| `get_firstbeat`    | float     | seconds (observed −2.79 … 20.52; can be negative) |
| `get_time total`   | int       | **milliseconds** total track length (298,525 … 360,262) |
| `loaded` / `play`  | string    | **`"on"` / `"off"`** |

### Per-deck — EQ / mixer / loop

| Trigger                 | JSON type | Observed | Meaning |
|-------------------------|-----------|----------|---------|
| `eq_high`/`eq_med`/`eq_low` | float | 0.5 (neutral) | **0–1, 0.5 = center** (NOT −1..1) |
| `gain`                  | float     | 0.5 (neutral) | 0–1, 0.5 = center |
| `loop`                  | string    | `"on"`/`"off"` | loop active |
| `get_loop`              | int       | 4, 8     | loop length in beats |
| `loop_roll … ? constant … :` | int  | 0        | active loop-roll size via a VDJScript ternary (0 = none) |

### Global triggers

| Trigger                      | JSON type | Observed | Meaning |
|------------------------------|-----------|----------|---------|
| `get_decks`                  | int       | **2**    | **Real deck count** (see below) |
| `masterdeck`                 | string    | `"on"`/`"off"` | **`on` ⇒ deck 1 is master, `off` ⇒ deck 2.** NOT an int index |
| `master_volume`              | number    | 1        | master volume 0–1 |
| `crossfader`                 | float     | —        | crossfader 0–1 |
| `get_crossfader_result full` | float     | 0 … 0.92 | effective crossfader blend |
| `headphone_volume`           | number    | 1        | headphone volume |
| `get_vu_meter`               | float     | 0 … 1    | master VU |
| `mixer_order`                | —         | —        | deck ordering (not consumed by QLC+) |

## Deck count & phantom decks (important)

VDJ always streams **4 decks**, but in a 2-deck configuration **decks 3 & 4 are
phantom mirrors** of decks 1 & 2 — VDJ sends *cloned* `get_filepath`/`get_title`/…
for them. The authoritative real count is the **`get_decks`** global (value `2` in
the capture).

**Rule:** treat `get_decks` as the number of real decks and ignore any deck beyond
it. This is how the real DMXDesktop avoids showing cloned decks. QLC+ drives its
tracked deck count from `get_decks`
(`qmlui/vdjbridge.cpp` `onGlobalTrigger` → `DjFsm::setDeckCount`).

## Second channel — control/cue (NOT implemented)

The real DMXDesktop also opens a **separate connection over IPv6 loopback (`::1`)**
using a different schema — `{"message":"online","data":true}`,
`{"message":"cue","action":"…"}`, `{"message":"shutdown"}`. **Cue points arrive on
this channel, not on the 8050 OS2L stream.** QLC+ does not implement this channel
(cue points are out of scope).

## Reproduce & prove it

Capture (all localhost TCP/UDP, full payload, file owned by you):

```bash
sudo tcpdump -i lo0 -s 0 -U -Z "$(whoami)" -w ~/vdj-dmxdesktop.pcap 'tcp or udp'
# …start DMXDesktop + VirtualDJ, play for ~5 min, then Ctrl-C…
```

Reading a `.pcap` does not need root. All commands below ran against the
2026-06-25 capture:

```bash
PCAP=~/vdj-dmxdesktop.pcap

# Message types & counts:
tcpdump -r "$PCAP" -A 'port 8050' | grep -oaE '"evt":"[a-z]+"' | sort | uniq -c
#   587 "evt":"beat"   1 "evt":"subscribe"   193367 "evt":"subscribed"   1 "evt":"unsubscribe"

# The subscribe request (server → client):
tcpdump -r "$PCAP" -A 'port 8050' | grep -oaE '"evt":"subscribe".*\]' | head -1

# Time fields are MILLISECONDS (e.g. ~74,700 ms remaining):
tcpdump -r "$PCAP" -A 'port 8050' | grep -oaE '"deck 1 get_time","value":[0-9]+' | tail
#   "deck 1 get_time","value":74734

# get_decks = real deck count:
tcpdump -r "$PCAP" -A 'port 8050' | grep -oaE '"get_decks","value":[0-9]+' | sort -u
#   "get_decks","value":2

# EQ neutral = 0.5 (NOT -1..1):
tcpdump -r "$PCAP" -A 'port 8050' | grep -oaE '"deck 1 eq_high","value":[0-9.]+' | sort -u
#   "deck 1 eq_high","value":0.5

# A real beat event:
tcpdump -r "$PCAP" -A 'port 8050' | grep -oaE '\{"evt":"beat"[^}]*\}' | head -1
#   {"evt":"beat","change":true,"pos":-1,"bpm":128,"strength":0}

# The separate IPv6 cue/control channel:
tcpdump -r "$PCAP" -A | grep -oaE '\{"message":"[a-z]+"[^}]*\}' | sort | uniq -c
#   126 {"message":"online","data":true}   25 {"message":"cue","action":"stop"}   1 {"message":"shutdown"}
```

## Bonjour / mDNS discovery

- **Service type**: `_os2l._tcp`, **Domain**: `local.`, **Port**: the TCP server port (8050).
- macOS: native `dns_sd.h` (`DNSServiceRegister`). Elsewhere: manual IP:port in VDJ.
