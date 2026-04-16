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

## References

- [OS2L Official Website](https://os2l.org)
- [VirtualDJ OS2L Documentation](https://www.virtualdj.com/wiki/OS2L.html)
- [QLC+ Documentation](https://www.qlcplus.org/docs)
- [mDNS/Bonjour Specification](https://tools.ietf.org/html/rfc6762)

## License

This plugin is part of QLC+ and is licensed under the Apache License 2.0.

## Author

Original OS2L plugin: Massimo Callegari
Enhanced features: Added as part of the QLC+ project

## Support

For issues or questions:
- QLC+ Forum: https://www.qlcplus.org/forum
- GitHub Issues: https://github.com/mcallegari/qlcplus/issues
