<p align="center">
  <a href="https://www.qlcplus.org/">
    <img src="resources/icons/png/qlcplus.png" alt="QLC+ Logo" height="60" />
  </a>
</p>

<h1 align="center">Q Light Controller+</h1>

> ## ⚠️ EXPERIMENTAL FORK — USE AT YOUR OWN RISK ⚠️
>
> **This fork was largely vibe-coded with AI pair-programming.**
> It has NOT been through upstream code review or formal QA.
> Do NOT submit PRs to upstream from this fork.
>
> **For the official QLC+ project, go to: [mcallegari/qlcplus](https://github.com/mcallegari/qlcplus)**
>
> This fork adds an MCP (Model Context Protocol) server that lets AI agents
> (Copilot, Claude, Cursor, etc.) design and control lighting shows via natural language.
>
> ### What this fork adds
> - `mcp/` directory: Self-contained MCP server — **65 tools**, 3 prompts, 360 unit tests
> - Streamable HTTP transport on `http://localhost:9696/mcp` (auto-starts with app)
> - `autolight/` directory: Iterative LED effect research loop (Python)
> - **Stage Wizard additions** — warm/cool colour themes, prism macros, Focus/Zoom faders, optional beat-synced chasers
> - Launchpad controller integration support
> - Audio capture / BPM detection for scripts
> - **41 audio-reactive HSV scripts** (LedFX-ported atmospheric effects, strobes, motion, EQ visualizers) — in `resources/huescripts/`, offered to **HUE Matrix** only
> - **HUE Matrix — a new function type forked from RGB Matrix.** `RGBMatrix` is restored byte-identical to upstream QLC+; all fork behaviour (HSV script contract, rotation/mirror, beat transforms, brightness, RGBW modes, audio scripts) lives in `HUEMatrix`, which inherits from it. Both are selectable side by side.
> - **HUE Matrix rotation & mirroring** (engine-level, all algorithm types)
> - **HUE Matrix low-latency step transitions** (~3ms vs ~22ms previously)
> - **Blend mode ordering fix** for Mask/Subtractive blend modes
> - **Enhanced OS2L plugin** — Bonjour/mDNS auto-discovery, song metadata, connection status LED, web diagnostics dashboard
> - **Auto-reload last workspace** on startup (no `--openlast` flag needed)
> - **Theme preset infrastructure** — switchable UI themes (includes "VS Code Dark")
> - **Stairville Beam Ball 100 Quad LED fixture** — 4 DMX modes (7/11/15/49ch), infinite tilt rotation, 10 RGBW LED heads, custom ball-shaped 3D model
> - **MCP fixture intelligence** — fixture type, RGBW/UV/Amber capabilities, per-head channel mapping (`headMap`), coarse/fine pairing, continuous rotation flags, server-instruction guidance
> - **Virtual Console grid layout** — opt-in Grafana-style grid per frame with vertical compaction, collision push-down, cell snapping on drag/resize
> - **Unified reflow** — QML auto-layout button and MCP `vc_reflow_frame` now use the same algorithm
> - **Improved column detection** — overlap-tolerance + best-match prevents false column merges
> - **Undo for layout operations** — Tardis batch support + Ctrl+Z/Ctrl+Shift+Z keyboard shortcuts
> - **ComboBox dropdown fix** — string list models now show text in dropdown items
> - **1/16 beat subdivision** — canonical quantizer table (single source of truth), `musicalBeatValue`/`beatValueToMusical` helpers exposed to QML, TimeEditTool count×subdivision UI with `allowFractions` gating, 63 unit tests (20 engine + 43 MCP conversions)
> - **FineFractions for all editors** — RGB Matrix, EFX, Scene, and Speed Dial preset editors now show 1/4, 1/8, 1/16 beat subdivisions (previously limited to 1/1 and 1/2)
> - **Update Scene from Live** — DMX Dump dialog: "Update only scene channels from live" button captures current pre-GM DMX values into an existing scene, scoped to only the channels already in the scene (preserves layer separation, with Tardis undo)
> - **Speed Dial multiply mode** — factor buttons (1/16x–16x) multiply existing function speeds instead of replacing them; preserves authored fadeIn/hold/fadeOut ratios; one-click reset to originals; works with both Time and Beats mode functions
> - **HUE Matrix RGBW mode** — `RGBW (Accurate)` and `RGBW (Brighter)` control modes drive R, G, B, AND White channels simultaneously. Accurate extracts white (`W=min(R,G,B)`, subtract from RGB); Brighter keeps RGB full and adds white on top. Works with any RGBW fixture — not fixture-specific.
> - **Keyboard shortcuts** — 20+ shortcuts ported from v4: Ctrl+N/O/S (New/Open/Save), Ctrl+Z/Shift+Z (Undo/Redo), Ctrl+Shift+Esc (Panic/Stop All), F11 (Fullscreen), Alt+1–6 (view switching), Ctrl+PgUp/PgDown (cycle views), Ctrl+[/] (drawer toggle), Function Manager (Delete/Clone/Wizard), Show Manager (Space/Ctrl+Space play/stop, copy/paste). Platform-aware tooltips (⌘ on macOS). Guards for text editing, popups, kiosk mode.
> - **DDP multi-universe sync fix** — eliminated frame-queue desync that caused 4+ DDP universes to display out of order. Replaced unreliable cross-thread batching with immediate per-universe send (sub-millisecond gap, PUSH per universe — matches Art-Net behavior).
>
> ### Recent engine changes
>
> #### Speed Dial Multiply Mode
> The VC Speed Dial widget now supports a **Multiply Mode** toggle (in widget properties).
> When enabled, the factor buttons (1/16x through 16x) **multiply** the controlled
> functions' existing speeds instead of replacing them with an absolute dial value.
>
> - **Preserves ratios** — fadeIn, hold, and fadeOut scale proportionally
> - **Reset button** — restores original speeds with one click
> - **Works with Time and Beats** — multiplies the raw stored value regardless of tempoType
> - **Sentinel-safe** — `defaultSpeed` and `infiniteSpeed` values are never modified
> - Snapshots are taken when multiply mode is toggled ON; toggling OFF keeps current values
>
> #### Beat Subdivision Fix (all editors)
> RGB Matrix, EFX, Scene, and Speed Dial preset editors previously showed only
> 1/1 and 1/2 beat subdivisions (`ByTwoFractions`). All four now use `FineFractions`,
> exposing the full 1/1, 1/2, 1/4, 1/8, 1/16 range — matching Chaser editor behavior.
>
> #### HUE Matrix RGBW Control Modes
> Two control modes for RGBW fixtures, available on **HUE Matrix** (`RGBMatrix` is
> byte-identical to upstream and does not offer them):
>
> | Mode | Algorithm | Best for |
> |------|-----------|----------|
> | RGBW (Accurate) | `W = min(R,G,B)`, subtract from RGB | Color-accurate shows |
> | RGBW (Brighter) | `W = min(R,G,B)`, keep RGB full | Maximum brightness, effects |
>
> - Works with any fixture that has R, G, B, and W channels — not fixture-specific
> - Auto-fallback to plain RGB for heads without a White channel
> - Based on WLED's proven Accurate/Brighter approach
> - Sentinel-safe: `defaultSpeed`/`infiniteSpeed` values preserved
>
> #### Keyboard Shortcuts & Tooltips
> 20+ keyboard shortcuts ported from v4, with platform-aware tooltip hints:
>
> | Category | Shortcuts |
> |----------|-----------|
> | Global | Ctrl+N/O/S, Ctrl+Z/Shift+Z, Ctrl+Shift+Esc (Panic), F11 |
> | Navigation | Alt+1–6 (views), Ctrl+PgUp/PgDown (cycle), Ctrl+[/] (drawers) |
> | Function Manager | Delete, Ctrl+C (Clone) |
> | Show Manager | Space (Play), Ctrl+Space (Stop), Ctrl+C/V, Delete |
>
> - Guards: disabled during text editing, popups, kiosk mode
> - Platform-aware: ⌘ on macOS, Ctrl elsewhere
> - `ShortcutUtils.js` helper for consistent display
> - `GenericButton` and `ContextMenuEntry` now support tooltips
>
> #### DDP Multi-Universe Sync Fix
> The DDP plugin now groups universes by destination (`IP + port + destId`) and
> emits one destination burst per refresh cycle.
>
> **Current behavior:**
> - One shared sequence per emitted burst (1..15 wrap, never 0) scoped to the endpoint (`IP + port`).
> - One terminal PUSH per burst. If the closing member is clean, QLC+ emits a standalone `len=0` PUSH.
> - Members are counted from configured destination membership, so the first callback cannot close a cycle early.
> - No queue or sleeps. If a member stalls, continuing input can close with the latest available member values after a bounded two-tick fallback.
> - Changing the controller-wide pixel count discards reports prepared with the old count. Fresh callbacks supply the new coverage; shrinking still clears abandoned pixels.
> - Destination ranges must match the intended layout. A 480-byte RGBW range starting every 320 bytes overlaps the next row by 40 pixels. Its zero tail can overwrite that row during full refreshes. DDP does not truncate configured coverage or arbitrate overlapping rows.
> - `ddp-race-test` exercises real faders, universes, output patches and the DDP plugin with concurrent callback barriers and per-PUSH pixel checks. Its finite schedules do not prove the absence of all races.
>
> #### HUE Matrix Rotation & Mirroring
> Rotation and mirroring are engine-level properties on `HUEMatrix`, available
> for **all** algorithm types (Plain, Script, Text, Image, Audio). They are not
> present on `RGBMatrix`, which is byte-identical to upstream.
>
> | Property | Values | Description |
> |----------|--------|-------------|
> | Rotation | 0°, 90°, 180°, 270° | Rotates the rendered pattern. 90°/270° swap the algorithm's input dimensions so the pattern renders in rotated coordinate space. |
> | Mirror | Off, Horizontal, Vertical, Both | Places a mirror at the midpoint of the axis. Rotation is applied first, then mirroring. |
> | Mirror Blend | Flip, Max, Average, Additive | How mirrored halves are combined. **Flip** = pure reflection (default). **Max** = brighter pixel wins. **Average** = `(a+b)/2`. **Additive** = `min(255, a+b)`. |
>
> Previously, rotation/mirroring was only available for audio-reactive scripts
> via auto-injected script properties. The 90° rotation had a bug
> (`sw` vs `sh` coordinate index) that caused most pixels to be black on non-square grids.
> That bug is now fixed.
>
> #### Blend Mode Ordering Fix (Mask / Subtractive)
> **Mask** and **Subtractive** blend modes are order-dependent — they read the
> current universe value and transform it, so the base layer must write before
> the overlay. Previously, the write order depended on fader insertion order
> (essentially random from the user's perspective), which meant Mask/Subtractive
> often had no visible effect.
>
> **Fix:** A new `BlendOverlay` fader priority ensures Mask/Subtractive faders
> always write **after** Normal/Additive faders. This makes blend modes work
> correctly regardless of function start order or collection function list order.
>
> **How blend modes work:**
>
> | Mode | Formula | Use case |
> |------|---------|----------|
> | Normal | HTP: `max(current, new)` | Default — highest value wins |
> | Additive | `min(current + new, 255)` | Layer effects on top of each other |
> | Mask | `current × (new / 255)` | Function output = brightness multiplier. White=pass, Black=block. |
> | Subtractive | `max(current − new, 0)` | Subtract function's values from existing output |
>
> **Example — RGB Matrix as a mask:**
> Set the RGB Matrix's blend mode to **Mask**. Run it alongside a chaser in a
> collection. The matrix's pixel pattern acts as a stencil — white areas show
> the chaser's colors, dark areas are blocked.
>
> #### Enhanced OS2L Plugin — Bonjour Auto-Discovery
> The OS2L (Open Sound 2 Light) plugin now supports **Bonjour/mDNS service
> discovery** on macOS. VirtualDJ's OS2L "Auto" mode finds QLC+ automatically
> — no manual IP configuration needed.
>
> | Feature | Description |
> |---------|-------------|
> | Bonjour discovery | Registers `_os2l._tcp` service via native macOS `dns_sd.h` API. VirtualDJ discovers QLC+ automatically. |
> | Song metadata | Parses `song` events — title, artist, BPM, key, duration available for scripting. |
> | Connection status LED | Input patch shows orange (advertising) / green (connected) status indicator. |
> | Bonjour checkbox | Enable/disable Bonjour from the OS2L config dialog (default: ON). |
> | Single-client enforcement | Tracks TCP connection state; only one DJ app connects at a time. |
>
> **Quick setup:** Enable OS2L on a universe → set VirtualDJ OS2L to **Auto** → done.
> See [`plugins/os2l/README.md`](plugins/os2l/README.md) for details.
>
> #### OS2L Diagnostics Dashboard
> When running with `-d` (debug mode), a live web dashboard is available at
> `http://localhost:9999/os2l` showing real-time OS2L traffic:
>
> | Feature | Description |
> |---------|-------------|
> | Event log | 1000-event ring buffer with timestamps, event types, and payloads |
> | Beat indicator | Green flash on every beat event |
> | Song metadata | Current track title, artist, BPM, key |
> | Connection stats | Message counts by type, bytes received, uptime |
> | JSON API | `/os2l.json` endpoint for programmatic access |
>
> The dashboard auto-polls at 500ms and is gated behind debug mode (not exposed in production).
>
> #### Stage Wizard additions
> The fork's separate Function Wizard has been retired; its remaining
> capabilities were ported into upstream's Stage Wizard, which covers the
> rest of what it did.
>
> **New effects (step 4):**
>
> | Effect | What it creates |
> |--------|----------------|
> | Warm Colors | 5 warm tones (Red→Orange→Amber→Yellow→WarmWhite) with a PingPong chaser |
> | Cool Colors | 5 cool tones (Blue→Cyan→Indigo→Purple→CoolWhite) with a PingPong chaser |
> | Prism Effects | One scene per prism macro, plus a chaser and a Virtual Console swatch strip |
>
> Upstream's Color Rainbow gained the fork's eighth colour (Purple), making
> it the full R→O→Y→G→C→B→P→M spectrum.
>
> **Beat-synced chasers:** an opt-in step 4 option that builds every
> generated chaser on the beat clock rather than in milliseconds, so its
> step timing follows the global BPM and the tap tempo. Pre-selected for
> Club Night and Concert shows.
>
> **Focus / Zoom faders:** groups whose fixtures carry beam focus or zoom
> channels get a fader for each on their Virtual Console page, beside the
> Intensity fader. Fine channels are excluded.
>
> **SoloFrame grouping:** Color, gobo, and prism scene buttons are wrapped
> in VCSoloFrame so only one can be active at a time.
>
> **Focus/Zoom sliders:** Automatically detected via channel preset
> (BeamFocusNearFar, BeamZoomSmallBig, etc.) and created as VCSliders.
>
> **New Step 3 options:**
> - "Beat-synced chasers" checkbox (default: on)
> - "One page per fixture" checkbox (default: on)
>
> #### RGB Matrix Step-Transition Latency
> Reduced step-transition latency from ~22ms to ~3ms. Previously, the
> GenericFader cleanup between chaser steps introduced a full frame delay.
> The fix ensures the new step's values are written in the same DMX frame
> as the old step's cleanup.
>
> #### Audio-Reactive RGB Scripts (22 effects)
> A complete library of audio-reactive RGB Matrix algorithms, including ports
> from the LedFX project. All scripts accept audio frequency data via
> `Engine.getAudioFrequency()` and support customizable parameters
> (palette, speed, sensitivity, trigger mode).
>
> | Category | Scripts |
> |----------|---------|
> | Atmospheric | aurora, lava, plasma, fire, water, soap, melt |
> | Motion | crawler, chaser, scroll, tunnel, vortex |
> | Visualizers | spectrum, equalizer, wavelength, scan |
> | Beat-reactive | strobe, shot, energy, power, glitch, blocks |
>
> All effects support engine-level rotation (0°/90°/180°/270°), mirroring,
> and customizable color palettes.
>
> #### Auto-Reload Last Workspace
> QLC+ now automatically loads the most recent workspace file on startup
> when no file is specified on the command line. Skips gracefully if the
> file no longer exists on disk.
>
> #### Theme Presets
> New theme preset infrastructure in UiManager allows switching between
> UI color schemes. Ships with a "VS Code Dark" preset. Presets control
> toolbar, frame header, and panel colors.
>
> #### AutoLight — Iterative LED Effect Research
> The [`autolight/`](autolight/) directory is a Python CLI tool that uses the MCP server to
> run structured A/B-style experiments on LED effects. It automates the
> create → preview → rate → iterate loop for finding the best-looking effects
> for your fixture setup.
>
> **How it works:**
> 1. **Setup** — builds a rating UI in QLC+ Virtual Console (star buttons, dimension ratings)
> 2. **Briefing** — interactive CLI questionnaire (genre, energy, palette, BPM)
> 3. **Rounds** — each round generates 3–4 experiments (different algorithms, colors, timing)
> 4. **Rate** — preview each experiment live, rate it 1–5 stars + per-dimension feedback
> 5. **Iterate** — analysis picks winners, next round refines based on feedback
>
> Each round creates a git branch for safe rollback. State is persisted in
> `autolight-state.json`.
>
> **Prerequisites:** QLC+ running with MCP enabled, fixtures patched, Python 3.10+.
>
> **Quick start:**
> ```bash
> # Install dependencies (one-time)
> python3 -m venv .venv && source .venv/bin/activate
> pip install -r autolight/requirements.txt
>
> # Create rating UI in QLC+ Virtual Console
> python3 -m autolight setup
>
> # Start the research loop (briefing → experiments → rating → iterate)
> python3 -m autolight
>
> # Smoke test (verify setup end-to-end, no manual interaction)
> python3 autolight/test_loop.py
> ```
>
> See [`autolight/README.md`](autolight/README.md) for full documentation
> (custom dimensions, architecture, color palettes, state file format).
>
> #### Virtual Console Grid Layout
> Frames now support an opt-in **Grafana-style grid layout** mode with vertical
> compaction and collision push-down. Widgets snap to grid cells during drag and resize.
>
> | Property | Default | Description |
> |----------|---------|-------------|
> | `layoutMode` | Free | `Free` (pixel positioning) or `Grid` (cell-based) |
> | `gridColumns` | 12 | Number of grid columns |
> | `gridRowHeight` | 0 (auto) | Row height in pixels (0 = use snapping size) |
> | `gridCompact` | true | Vertical compaction — pull widgets up to fill gaps |
>
> **MCP tools:** `vc_set_grid_layout` (set per-frame), `vc_reflow_frame` with `algorithm="gridCompact"`.
> Pixels remain the source of truth on disk — grid coordinates are derived at runtime.
> Existing `.qxw` files load unchanged (absent `GridLayout` element = `LayoutFree`).
>
> #### Unified Reflow & Column Detection
> The QML auto-layout button and MCP `vc_reflow_frame` now use the **same algorithm**
> (previously they were separate implementations with different behavior).
>
> Column detection was improved with **overlap-tolerance + best-match**:
> - Widgets with <10px overlap are treated as separate columns (fixes 1px false merges)
> - When a widget overlaps multiple columns, it joins the one with the largest overlap
> - Configurable via `overlapTolerance` in `ReflowOptions`
>
> #### Undo for Layout Operations
> Layout operations (auto-layout, MCP reflow) are now **fully undoable** via the
> existing Tardis undo engine. Keyboard shortcuts added:
>
> | Shortcut | Action |
> |----------|--------|
> | Ctrl+Z | Undo (edit mode only) |
> | Ctrl+Shift+Z | Redo (edit mode only) |
>
> A `beginBatch`/`endBatch` mechanism groups all widget moves from a single reflow
> into one undo step. Text input fields retain their own Ctrl+Z behavior.
>
> #### MCP Fixture Intelligence
> `query_fixtures` and `query_fixture_channels` now return richer data for AI agents:
>
> | New field | Where | Example |
> |-----------|-------|---------|
> | `type` | fixture | `"Moving Head"`, `"Dimmer"`, `"LED Bar (Beams)"` |
> | `capabilities` | fixture | `["RGBW", "Pan/Tilt", "ContinuousTiltRotation", "UV"]` |
> | `headMap` | fixture | `[{index: 0, channels: [7,8,9,10], rgbChannels: [7,8,9]}]` |
> | `controlByte` | channel | `"coarse"` or `"fine"` (16-bit pair identification) |
> | `headIndex` | channel | Which head/LED pixel the channel belongs to |
> | `defaultValue` | channel | DMX default value for safe scene building |
>
> Server instructions now include guidance on RGBW independence, continuous rotation
> interpretation, coarse/fine pairing, and multi-head control.
>
> ### MCP Feature Documentation
>
> #### Fixture Intelligence — Live MCP Output
>
> `query_fixtures` returns rich fixture metadata for AI agents:
> ```json
> {
>   "type": "Moving Head",
>   "capabilities": ["Shutter", "Pan/Tilt", "RGBW", "ContinuousTiltRotation"],
>   "heads": 10,
>   "headMap": [
>     {"index": 0, "channels": [7,8,9,10], "rgbChannels": [7,8,9]},
>     {"index": 1, "channels": [11,12,13,14], "rgbChannels": [11,12,13]},
>     ...
>   ],
>   "physical": {
>     "focusPanMax": 540, "focusTiltMax": 360,
>     "lensDegreesMin": 4.0, "lensDegreesMax": 11.0,
>     "focusType": "Head"
>   }
> }
> ```
>
> `query_fixture_channels` shows per-channel details including coarse/fine pairing and head assignment:
> ```
> ch0:  Pan          | controlByte=coarse | group=Pan
> ch1:  Pan Fine     | controlByte=fine   | group=Pan
> ch2:  Tilt         | controlByte=coarse | capabilities: Fixed position (0-191),
>                                           Endless rotation CW (192-222),
>                                           Stop (223-224),
>                                           Endless rotation CCW (225-255)
> ch7:  Red LED 1    | colour=Red   | headIndex=0
> ch8:  Green LED 1  | colour=Green | headIndex=0
> ch9:  Blue LED 1   | colour=Blue  | headIndex=0
> ch10: White LED 1  | colour=White | headIndex=0
> ```
>
> #### Grid Layout — MCP Tools (51 total)
>
> **`vc_set_grid_layout`** — Configure Grafana-style grid per frame:
> ```json
> {"items": [{"frameID": 0, "layoutMode": "grid", "columns": 8, "rowHeight": 40}]}
> → [{"frameID": 0, "layoutMode": "grid", "columns": 8, "rowHeight": 40, "compact": true, "status": "ok"}]
> ```
>
> **`vc_reflow_frame`** with `algorithm="gridCompact"` — Vertical compaction:
> ```json
> {"frameID": 0, "algorithm": "gridCompact", "dryRun": true}
> → {"algorithm": "gridCompact", "applied": false, "changes": [
>     {"widgetID": 1, "geometry": {"x": 0, "y": 40, "width": 105, "height": 60}},
>     {"widgetID": 4, "geometry": {"x": 0, "y": 100, "width": 60, "height": 195}},
>     ...
>   ]}
> ```
>
> **`vc_query_pages`** with `properties=["grid"]` — Read grid config:
> ```json
> {"grid": {"layoutMode": "grid", "columns": 8, "rowHeight": 40, "compact": true}}
> ```
>
> #### Stairville Beam Ball 100 Quad LED
>
> Custom fixture definition with 4 DMX modes:
>
> | Mode | Channels | Color control | Heads |
> |------|----------|---------------|-------|
> | 7-Channel | 7 | Color macros only | — |
> | 11-Channel | 11 | Master RGBW | 1 |
> | 15-Channel | 15 | Side A + Side B RGBW | 2 |
> | 49-Channel | 49 | 10 individual RGBW LEDs | 10 |
>
> **Infinite tilt rotation**: DMX 0–191 = fixed position, 192–222 = CW rotation,
> 223–224 = stop, 225–255 = CCW rotation.
>
> **Custom 3D model**: Ball-shaped head (procedurally generated `ball_moving_head.dae`)
> replaces the standard rectangular moving head model in the 3D view.
>
> #### Undo for Layout Operations
>
> All layout operations (auto-layout button, MCP reflow) are grouped into a single
> undo step via Tardis batch markers. Keyboard shortcuts:
> - **Ctrl+Z** — Undo (edit mode only, respects text field focus)
> - **Ctrl+Shift+Z** — Redo
>
> #### ComboBox Dropdown Fix
>
> Fixed blank items in `CustomComboBox` dropdown when using plain string list models
> (e.g., grid snapping size selector). The delegate now falls back to `modelData`
> when the `textRole` lookup returns undefined.
>
> ### Install from DMG (macOS)
> Download the latest DMG from [Actions artifacts](https://github.com/abossard/qlcplus/actions).
> After mounting the DMG and dragging QLC+ to `/Applications`:
> ```bash
> sudo xattr -cr /Applications/QLC+.app   # clear quarantine (ad-hoc signed)
> open /Applications/QLC+.app
> ```
>
> ### Build from source (macOS)
> ```bash
> # Configure (one-time, from repo root)
> mkdir -p build && cd build
> cmake .. -Dqmlui=ON -Dmcp_server=ON
>
> # Build
> cmake --build . -j$(sysctl -n hw.ncpu)
>
> # Run (MCP auto-starts on port 9696)
> ./qmlui/qlcplus5
> ```
>
> To assemble the local app bundle after building all targets, run `cmake --install .`
> from `build/`. The default destination is `dist/QLC+.app` in the repository.
> Qt framework and plugin paths come from the configured Qt installation; `QTDIR`
> is not required.
> To match CI with a Qt SDK, set `QTDIR` to that SDK and configure a separate
> build directory with `-DCMAKE_PREFIX_PATH="$QTDIR/lib/cmake"`. Setting `QTDIR`
> alone does not replace Qt paths already cached in an existing build.
>
> **Runtime flags:**
> | Flag | Description |
> |------|-------------|
> | `--no-mcp` | Disable MCP server |
> | `--mcp-http <port>` | Change MCP port (default: 9696) |
> | `-d` | Enable debug output to stderr |
> | `-g` | Log debug output to `~/QLC+.log` |
>
> **Timing diagnostics**: opt-in, rate-bounded measurements for the MasterTimer
> playback path. Off by default. Enable before launch (a restart is required):
> ```bash
> QLCPLUS_TIMING_DIAG=1 ./build/qmlui/qlcplus5 -d          # 1s reporting interval
> QLCPLUS_TIMING_DIAG=1 QLCPLUS_TIMING_DIAG_MS=2000 ./build/qmlui/qlcplus5 -d
> ```
> Filter logs for `[timing]`. Durations are wall time in fractional milliseconds,
> calculated in nanoseconds before formatting. `unknown` means unmeasured.
> Each bucket reports at most once per configured interval while running.
> Windows start with the first event; `in Wms` is their actual duration.
> Stopping the timer flushes any remaining partial window.
>
> - `scheduler dispatch: ticks=N in Wms period=Bms maxDispatchLate=Lms slept=yes|no priorCallback=Pms priorFinishedLate=Ems measuredTicks=M unknownTicks=U clockReadFailures=F`
>   reports the Unix scheduler, including ticks whose callbacks were cheap.
>   L is actual dispatch start minus deadline, clamped to zero. The remaining
>   fields belong to that worst dispatch: P is the preceding callback's wall
>   duration, E is how far its end exceeded this deadline, and `slept` says
>   whether this dispatch followed a sleep. `slept=yes` with E=0 and a large L
>   identifies delay after the preceding callback finished on time; `slept=no`
>   with E>0 shows the preceding callback already exceeded the deadline.
>   P includes diagnostic work inside the callback. These values do not isolate
>   OS descheduling, sleep precision, or diagnostic observer overhead.
>   M counts measured dispatch starts, U counts unknown starts, and F counts
>   failed start/end clock reads. These counters retain incomplete coverage even
>   when a known maximum is available. Clock failures share the report rate bound.
> - `callback consumed period: count=N in Wms period=Bms maxCallback=Cms functions=Fms unattributed=Ums dispatchLate=Lms maxWrite="NAME" type=T phase=write (id I) Dms`
>   counts callbacks consuming at least the scheduler period (`1e9/frequency`).
>   All fields describe the worst callback: C includes synchronous `tickReady`
>   consumers, F sums function writes, D identifies the largest single write,
>   and U=C-F includes untimed lifecycle, universe, DMX-source and signal work.
>   `maxWrite=unknown` means no write was measured. L describes this callback's
>   dispatch and can reflect preceding work; it is not necessarily wake delay.
>   It is unknown for directly driven ticks or failed scheduler clock samples.
> - `RGBMatrix step advance id=I name="NAME" advances=N in Wms` replaces the
>   unbounded `Elapsed exceeded` message. It counts ordinary between-beat steps,
>   not missed frames, without changing playback.
> - `HUEScript handoff id=I name="NAME" renders=N in Wms maxWait=Wms queue=Qms worker=Xms workerCpu=Cms workerOffCpu=Oms returnTail=Rms`
>   measures the synchronous render handoff to the JS thread, from four timestamps on one
>   clock (Q before invoke, Jstart at worker entry, Jend at worker exit, R after
>   return): `queue` = Jstart-Q (post/queue/descheduling before the worker ran),
>   `worker` = Jend-Jstart (worker-side render: script call plus marshalling, NOT
>   isolated script compute), `returnTail` = wait-queue-worker. `workerCpu` is the
>   JS worker thread's on-CPU time across that same Jstart..Jend span, sampled ON
>   the worker thread, and `workerOffCpu` = worker-workerCpu is the render time
>   NOT running on that core (descheduling/contention); both are `unknown` when
>   the platform thread-CPU clock is unsupported or a read failed (never a
>   fabricated 0). worker CPU still includes native marshalling, so it is not by
>   itself proof of script compute, and queue wait before Jstart is a separate
>   boundary, not worker descheduling. queue and worker are components of the same
>   max-wait render, not independent maxima. A direct on-thread render performs no
>   handoff and emits nothing.
>
> Different report types summarize **independent windows**. Adjacent lines or
> matching function IDs do not correlate a handoff with a particular slow tick.
> No scheduling policy changes with this option. Disabled detailed diagnostics
> skip diagnostic clock reads, aggregation, and formatting; enabled diagnostics
> add measurement and logging overhead.
>
> **Timer health**: the Unix timer independently counts its existing **pre-sleep
> deadline checks**, even with detailed profiling off. `[timer-health] pre-sleep
> deadline check: severity=debug|warning checkedTicks=N in Wms overdue=O moderate=M
> wholePeriod=F worstOverdue=Lms period=Pms` replaces both legacy incident messages.
> Only positive overdue counts; exact equality is on time for reporting, with
> the existing no-sleep/rebase scheduling unchanged. Moderate means at least
> one-quarter period but less than a full period; whole-period means at least
> one period. These bins are disjoint. A warning means any whole-period check
> or moderate checks at least 10% of all checked ticks; other nonclean windows
> are debug, and clean windows are silent. This is an engineering heuristic,
> not a perceptual guarantee. At 50Hz (20ms period), 7ms is 35% of the period:
> a rare event is debug, repeated events may warn. At 25Hz, moderate starts at
> 10ms rather than 5ms.
> Windows close at the first check reaching five seconds, including that check;
> long gaps produce one report, not catch-up reports. Normal stop flushes one
> nonclean partial window. W ends at the last valid check, not callback completion
> or shutdown, so short tails expose their actual duration and checked count.
> Durations retain nanoseconds until six-decimal millisecond formatting.
> Health accounting adds no clock reads and formats only reports. These checks
> do not cover subsequent wake or check-to-dispatch delays: zero whole-period
> checks does not rule out a late dispatch. They establish neither dropped
> output frames nor a Functions/CPU cause; use the separate detailed dispatch
> measurements for that later boundary.
>
> **Runtime control (MCP `timing_diagnostics`)**: the same diagnostics can be
> switched on and off at runtime — no restart — through the MCP tool
> `timing_diagnostics`, so an active show can be profiled on demand. The tool
> takes a single flat `action` and, only for `set_interval`, an `intervalMs`:
>
> | action | effect |
> |--------|--------|
> | `enable` | start a **fresh capture** and begin sampling |
> | `disable` | stop sampling, but **retain** the capture for a later snapshot |
> | `set_interval` | change the `[timing]` window/report interval (`intervalMs` > 0) |
> | `reset` | begin a new capture, clearing retained aggregates |
> | `snapshot` | return the current capture's bounded aggregates **without consuming them** |
>
> ```jsonc
> // enable, let the show run, then read the retained aggregates
> {"action": "enable"}
> {"action": "snapshot"}
> {"action": "disable"}   // aggregates stay readable until reset / re-enable
> ```
>
> Every response reports `status`, the `action`, and a `state` object with the
> current `captureId`, `enabled`, `intervalMs`, `elapsedMs`, `discardedSamples`,
> and a bounded per-source `buckets` array (scheduler / period / hue / rgb) of
> capture-cumulative aggregates. All durations are **nanoseconds**; `-1` means
> unknown, never a fabricated `0`. Semantics to rely on:
>
> - **Capture identity.** Each `enable` (off→on) and each `reset` starts a new
>   capture and bumps `captureId`. A sample whose render or tick straddled that
>   change is discarded (counted in `discardedSamples`), never mixed into the new
>   capture, and the first tick of a new capture carries no prior-callback state.
> - **Retention.** Results are cumulative aggregates (not a per-frame history):
>   they survive the periodic `[timing]` log flush and survive `disable`, and are
>   cleared only by `reset` or the next `enable`. `snapshot` is a pure read. A
>   disabled capture is a completed recording: its `elapsedMs` and each bucket's
>   duration **freeze at the disable moment**, so a later rate/overhead comparison
>   uses a fixed denominator rather than the still-growing wall-clock age.
> - **Cost.** Disabled, the added cost is a single acquire-load atomic gate
>   (~1 ns; no clock reads, allocation, or formatting). Enabled, the worker adds
>   two thread-CPU clock reads per HUE render (~1 µs each here) plus the existing
>   handoff wall clocks — well under ~0.4 ms of a 20 ms tick for realistic render
>   counts. A virtual/injected-clock test proves the algorithm and reporting
>   boundary, not OS wake precision.
> - **Interpretation.** These are measured boundaries only. `worker` wall minus
>   `workerCpu` is time not running on that core, not proof of any one scheduler
>   cause; independent-window maxima must not be matched as the same event.
>   The parent (owning) session preserves and restores live workspace, function,
>   and IO/audio/beat state across any capture run.
>
> **Dev cycle** — after code changes:
> ```bash
> # Rebuild only what changed
> cd build && cmake --build . --target qlcplus5 -j$(sysctl -n hw.ncpu)
>
> # If only MCP server code changed:
> cmake --build . --target qlcplusmcp -j$(sysctl -n hw.ncpu)
>
> # Run tests
> cmake --build . --target mcp_vc_query_filter_test -j8 && ./mcp/test/mcp_vc_query_filter_test
> ```
>
> ### Connect your AI agent
>
> **Copilot CLI / VS Code** — add to `~/.copilot/mcp.json`:
> ```json
> {
>   "servers": {
>     "qlcplus": { "url": "http://localhost:9696/mcp" }
>   }
> }
> ```
>
> **Claude Code** — run:
> ```bash
> claude mcp add qlcplus --transport http http://localhost:9696/mcp
> ```
>
> **Claude Desktop** — add to `claude_desktop_config.json`:
> ```json
> {
>   "mcpServers": {
>     "qlcplus": { "url": "http://localhost:9696/mcp" }
>   }
> }
> ```
>
> **Cursor** — add to `.cursor/mcp.json`:
> ```json
> {
>   "mcpServers": {
>     "qlcplus": { "url": "http://localhost:9696/mcp" }
>   }
> }
> ```
>
> ### Available tools
>
> The MCP surface is for setup, authoring, inspection, and bounded repair—not live-show actuation. `read_dmx_values` remains available as a read-only setup diagnostic.
> | Category | Tools |
> |----------|-------|
> | **Query** | `query_fixtures`, `query_available_fixtures`, `query_functions`, `query_fixture_channels`, `query_palettes`, `query_universes`, `query_input_profiles`, `query_midi_devices`, `query_osc_status`, `query_channel_modifiers`, `query_feedback_profile` |
> | **Patch** | `patch_fixtures`, `update_fixture` |
> | **Functions** | `create_scenes`, `create_chasers`, `create_sequences`, `create_efxs`, `create_collections`, `create_rgb_matrices`, `create_scripts`, `create_fixture_groups`, `delete_functions` |
> | **Palettes** | `create_palettes`, `delete_palettes` |
> | **Channels** | `configure_channels`, `read_dmx_values`, `set_channel_modifiers`, `convert_degrees_to_dmx` |
> | **I/O** | `configure_universes`, `configure_plugin_params`, `configure_osc`, `configure_beat_source`, `configure_launchpad`, `set_input_profile`, `vc_configure_feedback` |
> | **Virtual Console** | `vc_create_pages`, `vc_create_widgets`, `vc_query_pages`, `vc_query_widgets`, `vc_update_widgets`, `vc_delete_widgets`, `vc_reparent_widgets` |
> | **VC Input** | `vc_map_inputs`, `vc_set_key_sequences`, `vc_detect_overlaps` |
> | **VC Layout** | `vc_reflow_frame` |
>
> **Prompts:** `design_dj_show`, `debug_channel_conflict`, `setup_launchpad`
>
> All tools are batch-based (arrays in, arrays out) and idempotent (upsert by name).
> See [`mcp/MCP-ARCHITECTURE.md`](mcp/MCP-ARCHITECTURE.md) for full documentation.
>
> ### Script Engine (JavaScript)
>
> `create_scripts` accepts raw JavaScript executed by QJSEngine in a dedicated thread.
> Scripts are validated before saving — syntax errors are rejected with line numbers.
>
> <details>
> <summary><strong>Full Engine API (25 methods)</strong></summary>
>
> **Function Control:**
> | Method | Returns | Description |
> |--------|---------|-------------|
> | `Engine.startFunction(id)` | bool | Start any QLC+ function |
> | `Engine.stopFunction(id)` | bool | Stop a running function |
> | `Engine.isFunctionRunning(id)` | bool | Check if function is active |
> | `Engine.waitFunctionStart(id)` | bool | Block until function starts |
> | `Engine.waitFunctionStop(id)` | bool | Block until function stops |
> | `Engine.stopOnExit(bool)` | bool | Auto-stop started functions on script exit |
>
> **Fixture Control:**
> | Method | Returns | Description |
> |--------|---------|-------------|
> | `Engine.setFixture(fxID, ch, val)` | bool | Set fixture channel (0-255) |
> | `Engine.setFixture(fxID, ch, val, fadeMs)` | bool | Set with fade time |
> | `Engine.getChannelValue(universe, ch)` | int | Read live pre-GM DMX value |
>
> **Timing:**
> | Method | Returns | Description |
> |--------|---------|-------------|
> | `Engine.waitTime(ms)` | bool | Pause execution (ms) |
> | `Engine.waitTime("2s.500")` | bool | Pause using time string |
> | `Engine.random(min, max)` | int | Random integer in [min,max] |
> | `Engine.random("1s.0", "5s.0")` | int | Random ms from time strings |
>
> **BPM & Beat:**
> | Method | Returns | Description |
> |--------|---------|-------------|
> | `Engine.setBPM(bpm)` | bool | Set beat generator BPM |
> | `Engine.getBPM()` | int | Current BPM (internal/MIDI/audio) |
> | `Engine.getBeatDuration()` | int | Beat period in ms |
> | `Engine.isBeat()` | bool | True if current tick is on a beat |
>
> **Audio Input:**
> | Method | Returns | Description |
> |--------|---------|-------------|
> | `Engine.getAudioLevel()` | int | Overall volume 0-255 |
> | `Engine.getAudioFrequency(band, numBands)` | int | Frequency band 0-255 (3=bass/mid/high, 16=detailed) |
>
> **Envelope (from parent Chaser/Collection):**
> | Method | Returns | Description |
> |--------|---------|-------------|
> | `Engine.getOwnID()` | int | This script's function ID |
> | `Engine.getElapsed()` | int | Ms since script started |
> | `Engine.getEnvelopeDuration()` | int | Allocated duration from parent (ms, 0 if standalone) |
> | `Engine.getEnvelopeFadeIn()` | int | Fade-in from parent (ms, 0 if not set) |
> | `Engine.getEnvelopeFadeOut()` | int | Fade-out from parent (ms, 0 if not set) |
>
> **Function Attributes:**
> | Method | Returns | Description |
> |--------|---------|-------------|
> | `Engine.getFunctionAttribute(id, idx)` | float | Read function attribute |
> | `Engine.setFunctionAttribute(id, idx, val)` | bool | Modify running function attribute |
> | `Engine.setFunctionAttribute(id, "Name", val)` | bool | By name (e.g. "Width", "Intensity") |
>
> **System:**
> | Method | Returns | Description |
> |--------|---------|-------------|
> | `Engine.setBlackout(bool)` | bool | Toggle global blackout |
> | `Engine.systemCommand("prog args")` | bool | Run external process (detached) |
>
> </details>
>
> <details>
> <summary><strong>Example patterns</strong></summary>
>
> ```javascript
> // Candle flicker — Gaussian random, warm colors
> function gaussRand(mean, std) {
>     var u1 = Math.random(), u2 = Math.random();
>     return mean + std * Math.sqrt(-2*Math.log(u1)) * Math.cos(2*Math.PI*u2);
> }
> for (var tick = 0; tick < 200; tick++) {
>     for (var c = 0; c < 6; c++) {
>         Engine.setFixture(c, 0, Math.max(100, Math.min(255, Math.round(gaussRand(210, 25)))));
>     }
>     Engine.waitTime(Engine.random(30, 120));
> }
>
> // Envelope-adaptive buildup — reusable across different chaser step durations
> var totalMs = Engine.getEnvelopeDuration();
> if (totalMs <= 0) totalMs = 5000;
> var steps = Math.round(totalMs / 25);
> for (var i = 0; i <= steps; i++) {
>     Engine.setFixture(0, 0, Math.round(255 * i / steps));
>     Engine.waitTime(25);
> }
>
> // Audio-reactive — bass drives brightness, mid drives color
> for (var tick = 0; tick < 500; tick++) {
>     var bass = Engine.getAudioFrequency(0, 3);
>     var mid = Engine.getAudioFrequency(1, 3);
>     Engine.setFixture(0, 0, bass);
>     Engine.setFixture(0, 1, mid);
>     Engine.waitTime(25);
> }
> ```
>
> </details>

<p align="center"><em>(Often abbreviated as "QLC+")</em></p>
<p align="center">
  <strong>Open-source lighting control for DMX, Art-Net, sACN and more.</strong><br/>
  Designed for live shows, theatre, architectural installations, and venues.
</p>

<p align="center">
  <a href="https://github.com/mcallegari/qlcplus/releases/latest">
    <img src="https://img.shields.io/github/v/release/mcallegari/qlcplus" alt="Latest release version badge" /></a>
  <a href="https://github.com/mcallegari/qlcplus/releases/latest">
    <img src="https://img.shields.io/github/release-date/mcallegari/qlcplus" alt="Release date badge" /></a>
  <a href="https://github.com/mcallegari/qlcplus/commits/master/">
    <img src="https://img.shields.io/github/commits-since/mcallegari/qlcplus/latest/master" alt="Commits since latest release badge" /></a>
  <a href="https://github.com/mcallegari/qlcplus/commits/master/">
    <img src="https://img.shields.io/github/commit-activity/w/mcallegari/qlcplus" alt="Weekly commit activity badge" /></a>
  <a href="https://github.com/mcallegari/qlcplus/actions">
    <img src="https://github.com/mcallegari/qlcplus/actions/workflows/build.yml/badge.svg" alt="Build status badge" /></a>
  <a href="https://coveralls.io/github/mcallegari/qlcplus?branch=master">
    <img src="https://coveralls.io/repos/github/mcallegari/qlcplus/badge.svg?branch=master" alt="Test coverage badge" /></a>
</p>

---

<p align="center">
  <a href="https://www.qlcplus.org/download">
    <img src="https://custom-icon-badges.demolab.com/badge/-Download_QLC+-blue?style=for-the-badge&logo=download&logoColor=white" alt="Download QLC+ badge" /></a>
  <a href="https://qlcplus.org/discover/raspberry-pi">
    <img src="https://custom-icon-badges.demolab.com/badge/-Raspberry_Pi-red?style=for-the-badge&logo=cpu&logoColor=white" alt="Raspberry Pi badge" /></a>
  <a href="https://merch.qlcplus.org">
    <img src="https://custom-icon-badges.demolab.com/badge/-Store-green?style=for-the-badge&logo=home&logoColor=white" alt="Official store badge" /></a>
</p>

## Introduction

**QLC+** is powerful and user-friendly software to control lighting. QLC+ supports a [huge amount of hardware,](https://qlcplus.org/discover/compatibility) runs on Linux, Windows (10+), macOS (10.12+), and Raspberry Pi. Whether you're an experienced lighting professional or just getting started, QLC+ empowers you to take control of your lighting fixtures with ease. The primary goal of this project is to bring QLC+ to the level of available commercial software.

### Supported protocols

[![MIDI](https://img.shields.io/badge/MIDI-%23323330.svg?style=for-the-badge&logo=midi&logoColor=%23F7DF1E)](https://docs.qlcplus.org/v4/plugins/midi)
[![OSC](https://img.shields.io/badge/OSC-%23323330.svg?style=for-the-badge&logo=aiohttp&logoColor=%23F7DF1E)](https://docs.qlcplus.org/v4/plugins/osc)
[![HID](https://img.shields.io/badge/HID-%23323330.svg?style=for-the-badge&logo=applearcade&logoColor=%23F7DF1E)](https://docs.qlcplus.org/v4/plugins/hid)
[![DMX](https://img.shields.io/badge/DMX-%23323330.svg?style=for-the-badge&logo=amazonec2&logoColor=%23F7DF1E)](https://docs.qlcplus.org/v4/plugins/dmx-usb)
[![ArtNet](https://img.shields.io/badge/ArtNet-%23323330.svg?style=for-the-badge&logo=aiohttp&logoColor=%23F7DF1E)](https://docs.qlcplus.org/v4/plugins/art-net)
[![E1.31/S.ACN](https://img.shields.io/badge/E1.31%20S.ACN-%23323330.svg?style=for-the-badge&logo=aiohttp&logoColor=%23F7DF1E)](https://docs.qlcplus.org/v4/plugins/e1-31-sacn)
[![OS2L](https://img.shields.io/badge/OS2L-%23323330.svg?style=for-the-badge&logo=aiohttp&logoColor=%23F7DF1E)](https://docs.qlcplus.org/v4/plugins/os2l)

### QLC+ on social media

[![Instagram](https://img.shields.io/badge/Instagram-%23E4405F.svg?style=flat-square&logo=Instagram)](https://www.instagram.com/qlcplus/) 
[![YouTube (v4)](https://img.shields.io/badge/YouTube%20(v4)-%23FF0000.svg?style=flat-square&logo=YouTube)](https://www.youtube.com/playlist?list=PLHT-wIriuitDiW4A9oKSDr__Z_jcmMVdi) 
[![YouTube (v5)](https://img.shields.io/badge/YouTube%20(v5)-%23FF0000.svg?style=flat-square&logo=YouTube)](https://www.youtube.com/playlist?list=PLHT-wIriuitBQo0DKX9YgWVmS6LsEErE_) 
[![Facebook](https://img.shields.io/badge/Facebook-%231877F2.svg?style=flat-square&logo=Facebook)](https://www.facebook.com/qlcplus)

## Support & bug reports

We have a dedicated page to help you find support, please check out [SUPPORT.md](SUPPORT.md). To learn about a specific feature of QLC+, take a look at the [official documentation](https://docs.qlcplus.org/). To give feedback, submit new fixtures and get new ideas, go to the [forum](https://www.qlcplus.org/forum/index.php)

### Help wanted
Click the badge below to see the currently confirmed issues with QLC+. Perhaps you can find a solution?

[![Help Wanted](https://img.shields.io/github/issues/mcallegari/qlcplus/issue%20confirmed?logo=github&color=red)](https://github.com/mcallegari/qlcplus/issues?q=is%3Aopen+is%3Aissue+label%3A%22issue+confirmed%22)


## Building QLC+

Compilation guides and platform-specific instructions are available in our [GitHub Wiki](https://github.com/mcallegari/qlcplus/wiki).

#### Developers at work

If you're regularly updating QLC+ sources with git pull, you may encounter compiler warnings, errors, or unresolved symbols. We strive to keep the `master` branch free of critical errors; however, dependencies between objects can sometimes cause issues, requiring a full package recompilation rather than just updating recent changes.

## Contributing
### Software development

We welcome contributions from the community to help make QLC+ even better. If you're working on something major, start a thread in the [Development Forum](https://www.qlcplus.org/forum/viewforum.php?f=12) first. Make sure you read the [CONTRIBUTING.md](CONTRIBUTING.md) document for more.

### Financially

If you're reading this we already appreciate you. If you're just getting started with lighting you have absolutely no obligation to give us money. When QLC+ opens up revenue opportunities for you, we'd be very thankful for your support. GitHub sponsors is the preferred option.

<img src="https://img.shields.io/github/sponsors/mcallegari" alt="GitHub Sponsors"> <a href="https://github.com/sponsors/mcallegari"><img src="https://img.shields.io/badge/sponsor-30363D?logo=GitHub-Sponsors&logoColor=#white" /></a>

If you're interested, QLC+ also has an [official store](https://qlcplus-merch.myshopify.com) where you can purchase [clothing](https://qlcplus-merch.myshopify.com/collections/clothing), [themes](https://qlcplus-merch.myshopify.com/collections/themes), the [Raspberry Pi image](https://qlcplus-merch.myshopify.com/products/qlc-raspberry-pi-image) or [one-on-one consultation](https://qlcplus-merch.myshopify.com/collections/training-and-support) with an expert. 



## Thank you!

QLC+ owes its success to the dedication and expertise of numerous individuals who have generously contributed their time and skills. The following list recognizes those whose remarkable contributions have played a pivotal role in building QLC+.

![GitHub contributors](https://img.shields.io/github/contributors/mcallegari/qlcplus)

<details>
<summary>QLC+ 5</summary>
    
*   Eric Arnebäck (3D preview features)
*   Santiago Benejam Torres (Catalan translation)
*   Luis García Tornel (Spanish translation)
*   Nils Van Zuijlen, Jérôme Lebleu (French translation)
*   Felix Edelmann, Florian Edelmann (fixture definitions, German translation)
*   Jannis Achstetter (German translation)
*   Dai Suetake (Japanese translation)
*   Hannes Bossuyt (Dutch translation)
*   Aleksandr Gusarov (Russian translation)
*   Vadim Syniuhin (Ukrainian translation)
*   Mateusz Kędzierski + smaks6 (Polish translation)

</details>

<details>
<summary>QLC+ 4</summary>

*   Jano Svitok (bugfix, new features and improvements)
*   David Garyga (bugfix, new features and improvements)
*   Lukas Jähn (bugfix, new features)
*   Robert Box (fixtures review)
*   Thomas Achtner (ENTTEC wing improvements)
*   Joep Admiraal (MIDI SysEx init messages, Dutch translation)
*   Florian Euchner (FX5 USB DMX support)
*   Stefan Riemens (new features)
*   Bartosz Grabias (new features)
*   Simon Newton, Peter Newman (OLA plugin)
*   Janosch Frank (webaccess improvements)
*   Karri Kaksonen (DMX USB Eurolite USB DMX512 Pro support)
*   Stefan Krupop (HID DMXControl Projects e.V. Nodle U1 support)
*   Nathan Durnan (RGB scripts, new features)
*   Giorgio Rebecchi (new features)
*   Florian Edelmann (code cleanup, German translation)
*   Heiko Fanieng, Jannis Achstetter (German translation)
*   NiKoyes, Jérôme Lebleu, Olivier Humbert, Nils Van Zuijlen (French translation)
*   Raymond Van Laake (Dutch translation)
*   Luis García Tornel (Spanish translation)
*   Jan Lachman (Czech translation)
*   Nuno Almeida, Carlos Eduardo Porto de Oliveira (Portuguese translation)
*   Santiago Benejam Torres (Catalan translation)
*   Koichiro Saito, Dai Suetake (Japanese translation)
</details>

<details>
<summary>Q Light Controller</summary>

*   Stefan Krumm (Bugfixes, new features)
*   Christian Suehs (Bugfixes, new features)
*   Christopher Staite (Bugfixes)
*   Klaus Weidenbach (Bugfixes, German translation)
*   Lutz Hillebrand (uDMX plugin)
*   Matthew Jaggard (Velleman plugin)
*   Ptit Vachon (French translation)
</details>

---

<p align="center">
<a href="https://github.com/mcallegari/qlcplus/graphs/contributors">
  <img src="https://contrib.rocks/image?repo=mcallegari/qlcplus" />
</a>
</p>

---


## Testing

### Unit tests

```bash
cd build

# Engine tests (beat quantization)
cmake --build . --target beatquantize_test -j8 && ./engine/test/beatquantize/beatquantize_test

# HUE Matrix / RGB Matrix (run from each suite's own build dir - resource paths are cwd-relative)
cmake --build . --target huematrix_test -j8 && (cd engine/test/huematrix && ./huematrix_test)
cmake --build . --target rgbmatrix_test -j8 && (cd engine/test/rgbmatrix && ./rgbmatrix_test)

# MCP tests
cmake --build . --target mcp_conversions_test -j8 && ./mcp/test/mcp_conversions_test
cmake --build . --target mcp_vc_query_filter_test -j8 && ./mcp/test/mcp_vc_query_filter_test
cmake --build . --target mcp_vc_validation_test -j8 && ./mcp/test/mcp_vc_validation_test
```

| Suite | Tests | Covers |
|-------|-------|--------|
| `beatquantize_test` | 20 | 1/16 quantizer table, `musicalBeatValue`, `beatValueToMusical`, round-trips, overflow guards |
| `mcp_conversions_test` | 43 | Beat string parsing/formatting, decimal precision, off-grid snapping, GCD reduction, round-trips |
| `mcp_vc_query_filter_test` | — | VC widget query filtering and pagination |
| `mcp_vc_validation_test` | — | Widget type/field validation |
| `huematrix_test` | 42 | HUE Matrix: HSV `Float32Array` contract, dual packed-uint contract, algorithm-list separation, fork properties + XML round-trip, icon-site enumeration, bounded destructor drain, async precompute, per-tick audio recompute, load warnings |
| `rgbmatrix_test` | 9 | Upstream's own suite, unmodified — proves the `RGBMatrix` restore |
| `rgbscript_test` | 14 | Upstream's own suite, unmodified — packed-uint script contract |
| `mcp_rgb_transform_test` | 15 | Rotation, mirror + blend, and beat transforms (spatial) |

### E2E tests (Web DMX Control Panel)

```bash
cd webaccess/web-dmx && npx playwright test
```

35+ Playwright tests covering fixture panels, cross-tab sync, presets, and REST API.

### Manual review

See [`MANUAL_REVIEW.md`](MANUAL_REVIEW.md) for a checklist of items requiring human verification (visual layout, timing perception, 3D model rendering, OS2L diagnostics).

---


## License
<a href="https://github.com/mcallegari/qlcplus/blob/master/COPYING">
  <img alt="GitHub License badge" src="https://img.shields.io/github/license/mcallegari/qlcplus?style=flat-square" />
</a>

Licensed under the **Apache 2.0** License.  See [COPYING](COPYING) for details.

---
<p align="center">Copyright © Heikki Junnila, Massimo Callegari</p>
<p align="center">
  <img src="https://img.shields.io/badge/c++-%2300599C.svg?style=for-the-badge&logo=c%2B%2B&logoColor=white" alt="C++ badge" />
  <img src="https://img.shields.io/badge/Qt-%23217346.svg?style=for-the-badge&logo=Qt&logoColor=white" alt="Qt badge" />
  <img src="https://img.shields.io/badge/CMake-%23008FBA.svg?style=for-the-badge&logo=cmake&logoColor=white" alt="CMake badge" />
  <img src="https://img.shields.io/badge/javascript-%23323330.svg?style=for-the-badge&logo=javascript&logoColor=%23F7DF1E" alt="JavaScript badge" />
</p>
