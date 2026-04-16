# OS2L Plugin - Enhanced Features

This document describes the enhanced features added to the QLC+ OS2L plugin.

## Overview

The OS2L (Open Sound 2 Light) plugin enables bidirectional communication between QLC+ and DJ software like VirtualDJ. The enhanced version adds automatic service discovery, comprehensive logging, and song metadata support.

## New Features

### 1. Automatic Service Discovery (Bonjour/mDNS)

The plugin now automatically discovers OS2L-compatible hosts on the local network using mDNS/Bonjour protocol.

- **Service Type**: `_os2l._tcp.local.`
- **Default Port**: 9996
- **Auto-discovery**: Starts when the plugin input is enabled
- **Periodic Queries**: Sends mDNS queries every 5 seconds
- **Timeout**: Services not seen for 30 seconds are removed

**Benefits**:
- No manual IP configuration needed
- Automatic detection of VirtualDJ instances on the network
- Works across different subnets (if mDNS is properly configured)

### 2. Enhanced Logging

All OS2L messages are now logged with full details:

- Raw JSON message content
- All JSON field names and values
- Message size and sender information
- Structured output for easy debugging

**Example Output**:
```
[OS2L] Received 156 bytes from 192.168.1.100
[OS2L] Raw message: {"evt":"song","name":"Shape of You","artist":"Ed Sheeran",...}
[OS2L] Parsed JSON keys: ["evt", "name", "artist", "bpm", "key"]
[OS2L]   evt = song
[OS2L]   name = Shape of You
[OS2L]   artist = Ed Sheeran
```

### 3. Song Metadata Support

The plugin now parses and displays comprehensive song information:

**Supported Fields**:
- `name` - Song title
- `artist` - Artist name
- `album` - Album name
- `genre` - Music genre
- `year` - Release year
- `remix` - Remix version
- `status` - Playback status (play/pause/stop)
- `bpm` - Beats per minute
- `key` - Musical key (Camelot notation)
- `elapsed` - Time elapsed in seconds
- `duration` - Total track duration
- `deck` - Deck number (1 or 2)

**Example Output**:
```
[OS2L] ==================== SONG METADATA ====================
[OS2L] Song Name: Shape of You
[OS2L] Artist: Ed Sheeran
[OS2L] Album: ÷ (Deluxe)
[OS2L] BPM: 96.0
[OS2L] Key: 8B
[OS2L] Elapsed: 32.5 seconds
[OS2L] Duration: 233.7 seconds
[OS2L] Deck: 1
[OS2L] ======================================================
```

### 4. Bidirectional Communication

The plugin now supports output mode for sending feedback messages to VirtualDJ:

- **Output Universe**: Can be configured separately from input
- **Feedback Commands**: Placeholder for DMX-to-OS2L command mapping
- **Future Expansion**: Will support triggering cues, controlling crossfader, etc.

### 5. Event Type Support

**Currently Supported Events**:
- `btn` - Button events (on/off)
- `cmd` - Command events with parameters
- `beat` - Beat/tempo synchronization
- `song` - Song metadata (NEW)

## Architecture

### Class Structure

```
OS2LPlugin (main plugin)
├── OS2LDiscovery (mDNS service discovery)
│   ├── QUdpSocket (multicast DNS)
│   ├── QTimer (periodic queries)
│   └── Service list management
└── QTcpServer (incoming OS2L messages)
    └── QTcpSocket (per-client connection)
```

### Files Added

- `os2ldiscovery.h` - mDNS discovery interface
- `os2ldiscovery.cpp` - mDNS discovery implementation
- `README.md` - This documentation file

### Files Modified

- `os2lplugin.h` - Added discovery, output support, new member variables
- `os2lplugin.cpp` - Enhanced logging, song metadata parsing, output methods
- `os2lconfiguration.cpp` - (no changes needed)
- `CMakeLists.txt` - Added new source files
- `resources/docs/html_en_EN/os2lplugin.html` - Updated documentation

## Configuration

### VirtualDJ Setup

1. Open VirtualDJ settings
2. Search for "os2l" in options
3. Set `os2lDirectIp` to QLC+ IP address and port
   - Example: `192.168.1.50:9996`
   - For same machine: `127.0.0.1:9996`
4. Restart VirtualDJ

### QLC+ Setup

1. Open Input/Output panel
2. Enable OS2L plugin on desired universe
3. (Optional) Configure port in OS2L settings if different from 9996
4. Watch for joystick icon to blink when receiving messages

### Viewing Logs

Run QLC+ with debug flag to see all OS2L messages:
```bash
qlcplus-qml -d
```

Or on macOS:
```bash
/Applications/QLC+.app/Contents/MacOS/qlcplus -d
```

## OS2L Protocol Reference

### Message Format

OS2L uses JSON over TCP on port 9996:

```json
{
  "evt": "song",
  "name": "Song Title",
  "artist": "Artist Name",
  "bpm": 128.0,
  "key": "8B",
  "elapsed": 15.5,
  "duration": 210.0
}
```

### Feedback Commands (Future)

Commands that can be sent TO VirtualDJ:
- `/os2l/midi/cue N` - Trigger cue point N
- `/os2l/midi/play_pause` - Toggle playback
- `/os2l/sampler/slot N play` - Trigger sampler slot N
- `/os2l/button "hotcue1"` - Trigger hot cue

## Development Notes

### Building

```bash
mkdir build && cd build
cmake .. -Dqmlui=ON
cmake --build . --target os2l -j8
```

### Testing Without VirtualDJ

You can test OS2L messages using netcat or telnet:

```bash
echo '{"evt":"song","name":"Test Song","artist":"Test Artist","bpm":120}' | nc 127.0.0.1 9996
```

### Debugging mDNS

Monitor mDNS traffic:
```bash
# Linux
tcpdump -i any port 5353

# macOS
sudo tcpdump -i any port 5353
```

Query for OS2L services:
```bash
# Using avahi (Linux)
avahi-browse -a

# Using dns-sd (macOS)
dns-sd -B _os2l._tcp
```

## Known Limitations

1. **mDNS Implementation**: The current mDNS discovery uses a simplified implementation. For production use on macOS, consider using the native Bonjour APIs (DNSService*) or KDNSSD framework.

2. **Output Commands**: The output/feedback functionality is a placeholder. Actual command mapping from DMX channels to OS2L commands needs to be implemented based on user requirements.

3. **Service Registration**: The plugin currently only discovers services, it does not register itself as an OS2L service. This could be added in the future.

4. **Connection Management**: The plugin doesn't automatically connect to discovered services. It only logs them for information.

## Future Enhancements

1. **Native Bonjour**: Use platform-specific APIs for more reliable service discovery
2. **Auto-connect**: Automatically connect to discovered OS2L hosts
3. **Command Mapping**: Implement DMX channel to OS2L command translation
4. **Service Registration**: Advertise QLC+ as an OS2L service
5. **Metadata Channels**: Map song metadata to specific DMX channels
6. **Web Interface**: Provide a web UI for monitoring OS2L messages

## References and Sources

### OS2L Protocol Specification

The OS2L protocol is documented at:
- **OS2L Official Website**: https://os2l.org — primary protocol reference for all message types
  (`evt`, `btn`, `cmd`, `beat`, `song`) and the JSON-over-TCP framing on port 9996.

#### Message Types Reference

All OS2L message types and their fields were derived from the OS2L specification at https://os2l.org:

| Event | Source |
|-------|--------|
| `btn` | https://os2l.org — button on/off events with `name` and `state` fields |
| `cmd` | https://os2l.org — numeric command with `id` (integer) and `param` (0.0–1.0) fields |
| `beat` | https://os2l.org — beat/tempo synchronization event |
| `song` | https://os2l.org — song metadata event (see below) |

#### Song Metadata Fields

The `song` event fields and their semantics are documented by the OS2L specification (https://os2l.org) and the VirtualDJ OS2L implementation (https://www.virtualdj.com/wiki/OS2L.html):

| Field | Type | Source | Description |
|-------|------|--------|-------------|
| `name` | string | OS2L spec / VirtualDJ wiki | Track title |
| `artist` | string | OS2L spec / VirtualDJ wiki | Artist name |
| `album` | string | VirtualDJ wiki | Album name |
| `genre` | string | VirtualDJ wiki | Music genre |
| `year` | string | VirtualDJ wiki | Release year |
| `remix` | string | VirtualDJ wiki | Remix/edit version |
| `status` | string | OS2L spec | Playback status: `play`, `pause`, or `stop` |
| `bpm` | number | OS2L spec / VirtualDJ wiki | Beats per minute (float) |
| `key` | string | VirtualDJ wiki | Musical key in Camelot notation (e.g. `8B`) |
| `elapsed` | number | VirtualDJ wiki | Elapsed playback time in seconds |
| `duration` | number | VirtualDJ wiki | Total track duration in seconds |
| `deck` | integer | VirtualDJ wiki | Deck number (1 or 2) |

#### Feedback / Output Commands

OS2L output messages (commands sent back to VirtualDJ) are documented in the VirtualDJ OS2L wiki:
- **VirtualDJ OS2L Documentation**: https://www.virtualdj.com/wiki/OS2L.html

Supported feedback commands include:
- `/os2l/button "<name>"` — triggers a named button
- `/os2l/midi/cue N` — triggers cue point N
- `/os2l/midi/play_pause` — toggle playback
- `/os2l/sampler/slot N play` — trigger sampler slot N
- `/os2l/crossfader <value>` — set crossfader position (0.0–1.0)

### Automatic Discovery (Bonjour/mDNS)

The mDNS-based service discovery implementation follows these specifications:

- **RFC 6762 — Multicast DNS**: https://tools.ietf.org/html/rfc6762
  Defines the mDNS protocol (UDP port 5353, multicast group 224.0.0.251) used to discover
  services on the local link without a DNS server.

- **RFC 6763 — DNS-Based Service Discovery (DNS-SD)**: https://tools.ietf.org/html/rfc6763
  Defines the `_service._tcp.local.` naming convention for service types. The OS2L service
  type is `_os2l._tcp.local.` following this convention.

- **Apple Bonjour / Zero-configuration networking**: https://developer.apple.com/bonjour/
  Bonjour is Apple's brand name for the combination of mDNS (RFC 6762) and DNS-SD (RFC 6763).
  The native macOS APIs for Bonjour are `DNSServiceBrowse`, `DNSServiceResolve`, and
  `DNSServiceGetAddrInfo` from `<dns_sd.h>` (part of the Bonjour SDK / system library).

- **DNS packet format**: https://tools.ietf.org/html/rfc1035 — RFC 1035 defines the binary
  DNS wire format used when constructing mDNS query packets in `OS2LDiscovery::sendQuery()`.

### Qt Networking APIs

The implementation uses the following Qt 6 classes:
- `QUdpSocket` — for mDNS multicast (Qt docs: https://doc.qt.io/qt-6/qudpsocket.html)
- `QTcpServer` / `QTcpSocket` — for OS2L TCP server (Qt docs: https://doc.qt.io/qt-6/qtcpserver.html)
- `QJsonDocument` / `QJsonObject` — for parsing OS2L JSON messages (Qt docs: https://doc.qt.io/qt-6/qjsondocument.html)

---

## License

This plugin is part of QLC+ and is licensed under the Apache License 2.0.

## Author

Original OS2L plugin: Massimo Callegari  
Enhanced features (Bonjour/mDNS discovery, song metadata, bidirectional support): Added as part of the QLC+ project

## Support

For issues or questions:
- QLC+ Forum: https://www.qlcplus.org/forum
- GitHub Issues: https://github.com/mcallegari/qlcplus/issues
