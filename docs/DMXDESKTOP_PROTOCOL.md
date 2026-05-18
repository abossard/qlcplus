# DMXDesktop / VirtualDJ Telemetry Protocol

## Overview

VirtualDJ can stream real-time deck telemetry to external lighting controllers
via a protocol originally implemented by the DMXDesktop application. QLC+
implements a compatible TCP server so VDJ can connect to it directly.

## Transport

| Property    | Value                                |
|-------------|--------------------------------------|
| Protocol    | Newline-delimited JSON (NDJSON) over TCP |
| Default port| 8050                                 |
| Role        | QLC+ is the **TCP server**; VDJ connects as **client** |
| Discovery   | Bonjour/mDNS service `_os2l._tcp` on the same port |
| Encoding    | UTF-8                                |
| Line ending | `\n` (0x0A)                          |

## Connection Lifecycle

1. QLC+ listens on port 8050 (configurable).
2. VDJ discovers the service via Bonjour or uses a manually configured IP:port.
3. VDJ opens a TCP connection.
4. **Server → Client**: QLC+ sends a subscription request (see below).
5. **Client → Server**: VDJ begins streaming telemetry frames at the
   requested frequency.
6. On disconnect, QLC+ resets deck state and keeps listening for a new
   connection. VDJ reconnects automatically.

Only one client is supported at a time. A new connection replaces any
existing one (the old socket is closed).

## Handshake: Subscription Request

Immediately after accepting a connection, the server sends a single JSON line:

```json
{"evt":"subscribe","frequency":"25","trigger":["deck 1 get_position","deck 1 get_time",...]}
```

| Field       | Type   | Description                                    |
|-------------|--------|------------------------------------------------|
| `evt`       | string | Always `"subscribe"`                           |
| `frequency` | string | Requested updates per second (e.g. `"25"`)     |
| `trigger`   | array  | List of trigger strings to subscribe to         |

The trigger array contains all per-deck and global triggers the server
wants to receive (typically 108 entries for 4 decks).

## Inbound Frames (VDJ → QLC+)

### Subscribed Value Update

```json
{"evt":"subscribed","trigger":"deck 1 get_title","value":"Song Name"}
```

| Field     | Type          | Description                              |
|-----------|---------------|------------------------------------------|
| `evt`     | string        | Always `"subscribed"`                    |
| `trigger` | string        | The trigger name that was subscribed to  |
| `value`   | string/number/bool | Current value (type depends on trigger) |

### Beat Event

```json
{"evt":"beat","pos":233,"bpm":123.94,"strength":0.9,"change":false}
```

| Field      | Type   | Description                                      |
|------------|--------|--------------------------------------------------|
| `evt`      | string | Always `"beat"`                                  |
| `pos`      | int    | Beat position within the song (cumulative)       |
| `bpm`      | float  | Current BPM of the master deck                   |
| `strength` | float  | Beat strength (0.0–1.0)                          |
| `change`   | bool   | True if BPM changed since last beat              |

## Trigger Reference

### Per-Deck Triggers (×4 decks)

Deck triggers are prefixed with `deck N ` where N is 1–4.

#### Continuous (streamed at `frequency` Hz)

| Trigger                        | Value type | Description                        |
|--------------------------------|------------|------------------------------------|
| `get_position`                 | float      | Track position 0.0–1.0            |
| `get_time`                     | float      | Time remaining (seconds)           |
| `get_time elapsed absolute`    | float      | Elapsed time (seconds)             |
| `get_beatpos`                  | float      | Beat position within bar (0.0–4.0)|
| `get_vu_meter`                 | float      | VU meter level (0.0–1.0)          |
| `level`                        | float      | Output level (0.0–1.0)            |

#### On-Load (sent once per track load)

| Trigger           | Value type | Description                          |
|-------------------|------------|--------------------------------------|
| `get_filepath`    | string     | Full file path of loaded track       |
| `get_title`       | string     | Track title                          |
| `get_artist`      | string     | Track artist                         |
| `get_title_artist`| string     | Combined "Title - Artist"            |
| `get_album`       | string     | Album name                           |
| `get_genre`       | string     | Genre                                |
| `get_bpm`         | float      | Track BPM                            |
| `get_key`         | string     | Musical key (e.g. "5A", "Cm")        |
| `get_firstbeat`   | float      | First beat offset (seconds)          |
| `get_time total`  | float      | Total track duration (seconds)       |
| `loaded`          | int/bool   | 1 if a track is loaded               |
| `play`            | int/bool   | 1 if deck is playing                 |
| `volume`          | float      | Deck volume (0.0–1.0)               |

#### EQ / Mixer

| Trigger    | Value type | Description                           |
|------------|------------|---------------------------------------|
| `eq_high`  | float      | High EQ (-1.0 to 1.0, 0 = neutral)   |
| `eq_med`   | float      | Mid EQ                                |
| `eq_low`   | float      | Low EQ                                |
| `gain`     | float      | Deck gain                             |

#### Loop

| Trigger     | Value type | Description                          |
|-------------|------------|--------------------------------------|
| `loop`      | int/bool   | 1 if loop is active                  |
| `get_loop`  | float      | Loop length in beats                 |

### Global Triggers

| Trigger                         | Value type | Description                      |
|---------------------------------|------------|----------------------------------|
| `master_volume`                 | float      | Master output volume (0.0–1.0)  |
| `get_decks`                     | int        | Number of active decks           |
| `crossfader`                    | float      | Crossfader position (0.0–1.0)   |
| `headphone_volume`              | float      | Headphone volume (0.0–1.0)      |
| `masterdeck`                    | int        | Index of the current master deck |
| `get_crossfader_result full`    | float      | Computed crossfader result       |
| `mixer_order`                   | int        | Mixer deck ordering              |
| `get_vu_meter`                  | float      | Master VU meter (0.0–1.0)       |

## Value Type Coercion

VDJ sends all values as JSON primitives but types are not always consistent:
- Boolean triggers may send `true`/`false`, `1`/`0`, or `"true"`/`"false"`
- Numeric triggers typically send JSON numbers but may occasionally send strings
- Parsers should coerce defensively (try number first, then bool, then string)

## Bonjour / mDNS Discovery

VDJ discovers DMXDesktop-compatible servers via Bonjour/DNS-SD:

- **Service type**: `_os2l._tcp`
- **Domain**: `local.`
- **Port**: Must match the TCP server port (default 8050)
- **Instance name**: Arbitrary (e.g. "QLC+")

On macOS, use the native `dns_sd.h` API (`DNSServiceRegister`).
On other platforms, manual IP:port configuration in VDJ is the fallback.
