# Changelog

All notable changes to this QLC+ fork (`mcp-server` branch).

This file documents two things: the upstream **5.3.0** changes pulled into the fork,
and the fork-specific features that still need **manual verification** (tracked in
[`MANUAL_REVIEW.md`](MANUAL_REVIEW.md)). Section numbers/titles below mirror that
checklist so the two documents stay cross-referenced.

---

## 2026-06-21

### Upstream 5.3.0 sync

Merged 7 upstream commits (range `b625e9d8f..0d97d6538`, upstream-priority).
User-facing summary of each:

- **Palette overhaul** (`26661f9`) — Introduced a new **3D position palette** and
  **shutter palette** support. Fixed the palette-manager filters, and stopped
  showing the fanning box while a palette is being edited.
- **Sequence editor preview & step auto-select** (`db8151f`) — Added a
  **channels-preview toggle** to the Sequence editor, and a newly added step is now
  **auto-selected** so entered values go straight into it.
- **3D marker visibility fix** (`71bc12e`) — Fixed the **3D position marker** not
  being visible in the 3D view.
- **2D/3D drag lock** (`e4115da`) — Added a **lock flag** that prevents fixtures
  from being dragged in the 2D and 3D previews when locked.
- **More live Tardis actions** (`0d97d65`) — Wired additional **live network-sync
  (Tardis) actions** across speed dial, clock, cue list, slider, XY pad, and audio
  trigger widgets, so more edits propagate in real time.
- **Version 5.3.0** (`ad851fb`) — Bumped the project version back to **5.3.0**
  (debug) across CMake, the Windows installer script, and `variables.cmake`.
- **Android deployment fixes** (`251bcf4`) — Additional fixes for the **Android
  deployment** build.

### Fork features pending manual verification

These fork additions are covered by automated tests where possible but still require
a human review pass (visual / UX / timing / 3D / hardware). Each entry maps to a
top-level section of `MANUAL_REVIEW.md`.

- **§2 MCP Server — Composite Tools** — Higher-level MCP tools: `build_show_page`
  (lays out a Virtual Console page from a spec) and `configure_launchpad` (maps a
  Novation Launchpad with LED feedback).
- **§3 Web DMX Control Panel** — Browser-based DMX control surface at `/vc/`:
  connection status pill, color picker, position XY pad, dimmer faders, WLED
  320-channel raw section, cross-tab sync, and compact mode.
- **§4 QML UI** — "Open Web Control" toolbar button (with `--no-web` hiding),
  Grafana-style VC **auto-layout** and grid layout mode, single-step undo/redo for
  batched layout changes, **Update Scene from Live** (scoped to existing channels,
  preserving color/dimmer layer separation), and MCP↔QML layout parity.
- **§4B Page-Dependent External Input Mappings** — Per-page external input modes
  (**Normal / Override / Inherit**) governing how MIDI and keyboard input is
  dispatched across VC pages, with XML persistence, MCP query exposure, and a QML
  mode-selection combo box.
- **§5 Fixture — Stairville Beam Ball 100 Quad LED** — New fixture definition that
  appears in the fixture browser and renders as a ball/sphere in the 3D view,
  reorienting to pan/tilt.
- **§6 Script Fader Cleanup** — Channels written by a Script via `Engine.setFixture()`
  now reset to zero when the script stops, instead of leaving stuck values.
- **§7 DJ Expression System** — VC Page 3 performance layout (ENERGY/PHASE, MOOD,
  MOVEMENT, FX/MOMENTS, MASTER) with HTP color-vs-dimmer layer separation and
  fixture-native strobe.
- **§8 Performance & Diagnostics** — RGB matrix step-transition latency improvement
  (≈22 ms → ≈3 ms), an OS2L diagnostics dashboard, and runtime multi-plugin
  enable/disable diagnostics.
- **§9 Beat Timing — UI Interaction** — Time/Beats tempo toggle in the Chaser
  editor, fine **1/16** fractions on Fade/Hold/Duration, and the TimeEditTool
  subdivision selector.
- **§10 Keyboard Shortcuts & Tooltips** — Global and context-specific shortcuts with
  focus/popup guards, platform-correct tooltip glyphs, Speed Dial **multiply mode**,
  and beat-subdivision (FineFractions) buttons.
- **§11 Song Manager — VDJ Integration** — Song list populated from the `Songs/`
  folder with currently-playing indicator, timestamp persistence, real-time
  search/filter, and sort modes.
- **§12 VDJ Beat-Synced Show Playback** — External-sync show playback driven by VDJ
  telemetry: disconnect freeze/recovery, backward-seek/loop handling, tempo scaling,
  auto-creation of shows, and auto-start/auto-pause.
- **§13b VCAnimation — On-Widget Algorithm Parameter Controls** — On-widget Range
  sliders and List dropdowns for an RGBMatrix script algorithm's parameters, with
  MIDI→widget-face sync, a visibility toggle, and no regression to existing controls.
- **§15 Upstream 5.3.0 merge regressions** — Post-merge regression checks for areas
  the fork extends: palette query/create round-trips, 3D view smoke (marker + drag
  lock), and the sequence editor preview/auto-select.

#### Fork maintenance / merge adaptations

Two latent issues surfaced while merging upstream 5.3.0 were fixed to keep the fork
upstream-compatible:

- **`mcp/tools/io_tools.cpp`** — Fixed a patch call to match the upstream patch
  signature.
- **`VCAudioTriggers::remapChannels`** — Implemented the previously missing method.
