# Fixture Channel Preview Panel — Design Plan

## Goal

Add a new "Fixture Profiles" page to the existing `webaccess/vc-next` React dashboard. This page provides read-only visual panels that preview each fixture's configuration, capabilities, and channel layout — a fixture inspector/profile viewer.

## What exists today

The `webaccess/vc-next` app has a **DmxView** that renders `FixturePanel` cards with interactive controls (faders, color pickers, XY pads, etc.) for live DMX manipulation. The data layer (`dmx-store.ts`, `dmx-api.ts`, `control-mapper.ts`) already loads full fixture metadata including channels, capabilities, head maps, and physical info.

## Proposed visual panels per fixture feature

| Feature | Visual Panel | What it shows |
|---|---|---|
| **RGB / RGBW / RGBWA+UV** | Color capability badge with colored dots (R, G, B, W, A, UV) + mini color wheel icon | Which color channels exist, which head they belong to, channel indices |
| **Dimmer** | Vertical bar icon with percentage label | Channel index, 8-bit vs 16-bit (coarse/fine), per-head or master |
| **Pan/Tilt (Position)** | Mini top-down movement range arc diagram | Pan range (e.g. 540deg), tilt range (270deg), 8 vs 16-bit, continuous rotation capabilities |
| **Strobe/Shutter** | Pulsing icon + capability list (open/closed/strobe speed range) | DMX ranges for each mode, speed sweep range |
| **Gobo** | Thumbnail grid of gobo images (from `cap.image`) | All gobo slots with their DMX ranges |
| **Color Macro** | Horizontal strip of color swatches (using `cap.color1`/`color2`) | Named color presets with DMX ranges |
| **Heads (multi-pixel)** | Visual grid/layout showing head indices, with per-head capability badges | Which channels belong to which head, RGB per head |
| **Generic/Other** | Simple labeled channel list with capability range bars | Channel name, group, DMX value ranges, capability names |
| **Physical** | Schematic info card | Lens degrees, pan/tilt max, fixture type badge |

## Data requirements

- **No new data needed** — everything is already in `FixtureInfo`, `ChannelInfo`, `CapabilityInfo`, `HeadInfo`, and `PhysicalInfo` types
- **No new API calls** — `fetchFixturesWithChannels()` already returns all channel details including capabilities, head maps, physical info, and presets

## Navigation

`App.tsx` currently hardcodes `<DmxView />`. Add a simple hash-based tab/route system (no extra dependencies) to switch between views:
- `#dmx` — existing DMX control panel (default)
- `#profiles` — new fixture profile viewer

## Files to create

```
src/views/FixtureProfileView.tsx          — main page: grid of fixture profile cards
src/components/profile/ProfileCard.tsx    — one card per fixture, orchestrates sub-panels
src/components/profile/ColorCapabilityBadge.tsx  — RGB/RGBW dots + channel indices
src/components/profile/PositionDiagram.tsx       — pan/tilt range arc visualization
src/components/profile/HeadLayoutGrid.tsx        — multi-head pixel layout
src/components/profile/CapabilityRangeBar.tsx    — generic DMX range visualization
src/components/profile/PhysicalInfoCard.tsx      — lens, dimensions, type
```

Plus CSS additions to the existing stylesheet.

## Panel detail: how each feature maps to a visual

### Color (RGB/RGBW/RGBWA+UV)
- Row of colored circles: red, green, blue, white, amber, UV — only those present
- Each dot labeled with its channel index (e.g. "ch3")
- If multi-head: grouped per head with head index label

### Dimmer
- Single vertical bar icon
- Label: "8-bit" or "16-bit (coarse ch5 / fine ch6)"
- Badge if it's a master dimmer vs per-head dimmer

### Position (Pan/Tilt)
- SVG arc showing pan range (e.g. 540deg sweep) and tilt range (270deg sweep)
- Labels for degrees and channel indices
- Indicator if continuous rotation is available (CW/CCW preset ranges)
- 8-bit vs 16-bit notation

### Strobe/Shutter
- List of capability modes: Open, Closed, Strobe (with DMX range)
- Speed range bar for strobe capabilities
- Visual indicator of which mode is "default"

### Gobo
- Grid of gobo thumbnails (images from capability data)
- Each labeled with name and DMX range
- Fallback text label when no image available

### Color Macro
- Horizontal strip of color swatches using `color1`/`color2` from capabilities
- Each labeled with macro name and DMX range

### Heads (multi-pixel)
- Grid layout: one cell per head
- Each cell shows which capability badges that head has (color, dimmer, etc.)
- Channel index ranges per head

### Physical Info
- Type badge (Moving Head, LED Bar, Dimmer, etc.)
- Lens degrees (min-max)
- Pan/tilt max degrees
- Fixture mode name

### Generic/Other channels
- Simple list: channel name, group, DMX range bar showing capability segments
- Each segment colored/labeled with capability name
