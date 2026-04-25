# QLC+ Fork — Manual Review & Testing Checklist

This checklist covers ~30 days of changes across the MCP server, Virtual Console layout engine, the new Web DMX Control Panel (`webaccess/vc-next/`), fixture definitions, and security/performance fixes. Work top-to-bottom; each section assumes the previous one passed.

> Tip: keep a terminal tailing `qlcplus-qml -d` output and the browser DevTools Console + Network tabs open throughout — most failures show up there before they're visible in the UI.

---

## 1. Prerequisites

### 1.1 Build

From the repo root:

```bash
mkdir -p build && cd build
cmake .. -Dqmlui=ON
cmake --build . -j8
```

Incremental rebuilds during testing:

```bash
cd build
cmake --build . --target qlcplus-qml -j8     # main app + MCP (statically linked)
cmake --build . --target qlcplusmcp -j8      # MCP lib only
```

If you add files or change `CMakeLists.txt`, rerun `cmake ..` first.

### 1.2 Run unit tests

```bash
cd build
cmake --build . --target mcp_vc_query_filter_test mcp_vc_validation_test -j8
./mcp/test/mcp_vc_query_filter_test
./mcp/test/mcp_vc_validation_test
```

All tests must pass (`PASS` on every line, exit code 0).

### 1.3 Start QLC+ with web access enabled

```bash
cd build
./qmlui/qlcplus-qml -w -d
```

Flags:
- `-w` — enable the embedded web server on port **9999** (required for the Web DMX Control Panel)
- `-d` — debug logging to stdout

### 1.4 Load the test workspace

In QLC+:
1. **File → Open**
2. Select `GARAGE.qxw` (the reference workspace shipped with this fork)
3. Confirm fixtures load in the Fixtures tab — you should see the HERO and HZ fixtures referenced throughout this document.

### 1.5 URLs

| Service                  | URL                              |
|--------------------------|----------------------------------|
| Web DMX Control Panel    | http://localhost:9999/vc/        |
| Legacy web access        | http://localhost:9999/           |
| MCP server (JSON-RPC)    | http://127.0.0.1:9696/mcp        |

---

## 2. MCP Server

### 2.1 Server is reachable

```bash
curl -s -o /dev/null -w "%{http_code}\n" http://127.0.0.1:9696/mcp
```

**Expected:** HTTP `405` or `400` (GET not allowed — the endpoint is JSON-RPC POST). A connection refused / `000` means the server didn't start.

### 2.2 Initialize handshake

```bash
curl -s -X POST http://127.0.0.1:9696/mcp \
  -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"manual-test","version":"1.0"}}}'
```

**Expected:** JSON response with `result.serverInfo.name == "qlcplus-mcp"` (or similar) and a `protocolVersion`.

### 2.3 Tool list

```bash
curl -s -X POST http://127.0.0.1:9696/mcp \
  -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":2,"method":"tools/list"}' | jq '.result.tools | length'
```

**Expected:** ≥ 40 tools listed.

### 2.4 Sample read-only tool call: `query_fixtures`

```bash
curl -s -X POST http://127.0.0.1:9696/mcp \
  -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"query_fixtures","arguments":{}}}' | jq '.result'
```

**Expected:**
- Response contains a `content[0].text` payload with a JSON array of fixtures.
- Each fixture has `id`, `name`, `universe`, `address`, and (new in this fork) `colorCount` + `colors` for fixtures with color channels.
- Addresses are **1-based** (the off-by-one fix). The HERO fixture's `address` should match the value shown in the QLC+ Fixtures tab — not `address - 1`.

### 2.5 Sample write tool: idempotent scene create

Call `create_scene` twice with the same `name`:

```bash
curl -s -X POST http://127.0.0.1:9696/mcp -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"create_scene","arguments":{"name":"MANUAL_TEST_SCENE","values":[]}}}'
# Run the SAME command again
```

**Expected:** Both calls succeed. The second call returns the **same scene id** (idempotent upsert), not a duplicate scene. Verify in QLC+ Functions tab that only one `MANUAL_TEST_SCENE` exists.

### 2.6 Composite tools

- `build_show_page` — call with a small spec, verify a VC page is created and populated.
- `configure_launchpad` — call against a connected (or virtual) Launchpad; verify MIDI mappings appear under Inputs/Outputs.

---

## 3. Web DMX Control Panel (`/vc/`)

Open http://localhost:9999/vc/ in a fresh browser tab.

### 3.1 Page loads directly into DMX Control (no VC tab)

- **Do:** Load the page.
- **Verify:** Top of the page shows the DMX Control surface immediately.
- **Expected:** No "Virtual Console" / "DMX" tab switcher. There is **only** the DMX panel.

### 3.2 Status indicator

- **Do:** Look at the header / status pill.
- **Verify:** Indicator reads **"Live"** with a green dot (or equivalent connected state).
- **Expected:** Toggling the QLC+ app off should flip it to "Disconnected" / red within a few seconds; bringing the app back flips it back to Live.

### 3.3 Grand Master

- **Do:** Drag the Grand Master fader from 100% down to 0%.
- **Verify:** All live DMX output drops to 0 (check a connected fixture or the QLC+ Inputs/Outputs monitor).
- **Expected:**
  - Fader value display updates live (e.g. `100% → 0%`).
  - Returning to 100% restores all channel values.

### 3.4 Fixture panels

- **Verify each panel shows:**
  - Fixture **name** (matches QLC+ Fixtures tab).
  - **Universe + DMX address** (1-based, e.g. `U1·1`).
  - **Type badge** (e.g. `Moving Head`, `Hazer`).
  - For fixtures with uncovered channels (not handled by a typed control): a **★** star next to the type badge.
  - **Raw Channels (N)** disclosure at the bottom showing the channel count.

### 3.5 Filter by model

- **Do:** Click any model chip in the toolbar above the fixture grid (e.g. `Hero Spot Wash 140 2in1 RGBW+W`).
- **Verify:** Only fixtures of that model remain visible; the active chip is highlighted; **All** chip shows as inactive.
- **Do:** Click **All**.
- **Expected:** Filter clears, all fixtures re-appear.

### 3.6 Search box

- **Do:** Type part of a fixture name (e.g. `HERO`) into the search box.
- **Verify:** Fixture list filters live as you type; case-insensitive substring match.
- **Do:** Clear the box.
- **Expected:** Full list returns.

### 3.7 Universe grouping

- **Do:** Click the **⊞** group-by-universe button in the toolbar.
- **Verify:** Fixtures group under collapsible headers like `▾ Universe 1 (N fixtures)`. The exact count depends on your workspace — GARAGE has fixtures across multiple universes.
- **Do:** Click a universe header.
- **Expected:** That universe collapses; click again to expand. Collapsed state should be visually obvious (chevron rotates / fixtures hidden).

### 3.8 HERO fixture — Color Picker

Locate the HERO fixture panel.

- **Do:** Drag the color picker crosshair around the saturation/value square.
- **Verify:**
  - **Swatch** (small color preview) updates live.
  - **Hue bar** marker stays in sync.
  - **R / G / B sliders** update with the selected color.
  - The fixture's RGB DMX values change accordingly (cross-check with QLC+ DMX monitor).
- **Do:** Drag the hue bar.
- **Expected:** Crosshair stays at its S/V position but the underlying hue rotates; swatch + RGB sliders update.
- **Do:** Drag an individual R/G/B slider.
- **Expected:** Crosshair, hue bar, and swatch all reconcile to the new color.

### 3.9 HERO fixture — Position XY pad

- **Do:** Drag the cursor on the position pad.
- **Verify:**
  - Cursor follows the pointer.
  - **Pan** and **Tilt** values display in **degrees** (not raw 0–255), matching the fixture definition's pan/tilt range.
  - The fixture's pan/tilt DMX channels update.
- **Edge case:** Click in the corners — values should hit min/max degrees exactly (e.g. `0° / 540°` or whatever the fixture spec defines).

### 3.10 HERO fixture — Dimmer fader

- **Do:** Drag the dimmer fader.
- **Verify:** Display shows **percentage** (0–100%), not raw 0–255. DMX value scales correctly (50% ≈ 127/128).

### 3.11 HZ fixture — Raw channels auto-open

The HZ hazer (2 channels) should have no typed controls (no color picker, position pad, or dimmer).

- **Verify:**
  - HZ panel shows **no** color picker, position pad, or dimmer fader above the raw section.
  - If the fixture has uncovered channels, a **★** star appears next to the type badge.
  - The **Raw Channels** section is **expanded by default** (no click needed to see faders).
  - Both raw channel faders (typically "Haze" and "Fan") are fully interactive (not read-only).

> **Note:** If the fixture definition includes a channel named "Dimmer" with the `IntensityDimmer` preset, the planner will create a typed dimmer control and raw will NOT auto-open. This is correct behavior — the star only appears for channels that have no typed control.

### 3.12 Raw channels — ALL channels visible

- **Do:** On the HERO fixture, click the **Raw Channels** disclosure to expand it.
- **Verify:**
  - Section lists **every** DMX channel of the fixture, not just untyped ones.
  - Channels already covered by typed controls (Red, Green, Blue, Pan, Tilt, Dimmer, etc.) display a **🔒 read-only** indicator and cannot be edited from the raw section.
  - Untyped channels (Strobe, Gobo, etc.) are editable as raw 0–255 sliders.
- **Do:** Drag a typed channel's read-only fader.
- **Expected:** Fader does not move; tooltip explains it's controlled by a typed control above.

### 3.13 Channel ordering

- **Verify:** Within each fixture panel, raw channel rows are sorted by **DMX channel index ascending** (channel 1 at top, channel N at bottom). Typed control order should also reflect channel order where reasonable.

### 3.13b WLED stress test (320-channel fixture)

- **Do:** Scroll to one of the **WLED - Row N** fixture panels (each has 320 channels).
- **Verify:**
  - The panel renders without crashing or freezing.
  - The **★** star indicator shows (most channels are uncovered).
  - **Raw Channels (320)** section is expandable — click to open.
  - Scrolling through the 320 faders is smooth (no jank).
  - Dragging a fader near the bottom of the list writes the correct DMX value.
- **Expected:** The browser remains responsive. If the page becomes slow with all 4× WLED raw sections open simultaneously, that is a known limitation (1280 fader elements).

### 3.14 Cross-tab sync — fader

- **Do:**
  1. Open `/vc/` in **Tab A**.
  2. Open `/vc/` in **Tab B** side-by-side.
  3. In Tab A, drag the HERO Dimmer to 50%.
- **Verify:** Tab B's HERO Dimmer fader animates to 50% within ~100 ms without any user interaction.
- **Reference screenshots:** `10-sync-fader-before-tabB.png`, `10-sync-fader-after-tabB.png`.

### 3.15 Cross-tab sync — color

- **Do:** In Tab A, change HERO color via the picker.
- **Verify in Tab B:**
  - Swatch updates.
  - Hue bar marker moves.
  - Crosshair on the S/V square moves.
  - R/G/B sliders update.
- **Reference screenshots:** `08-sync-color-*.png`.

### 3.16 Cross-tab sync — position

- **Do:** In Tab A, drag the HERO position pad cursor.
- **Verify in Tab B:** Cursor moves to match; pan/tilt degrees update.
- **Reference screenshots:** `09-sync-pos-*.png`.

### 3.17 Reset button (✕)

- **Do:** Set several controls on a fixture to non-zero values, then click the fixture's **✕** Reset button.
- **Expected:** All channels of that fixture (including raw) return to 0 / their default. Other fixtures are unaffected.

### 3.18 Preset save/recall

- **Do:**
  1. Configure HERO with a specific color + position.
  2. Click **Save Preset** (or equivalent button on the fixture panel).
  3. Reset the fixture (✕).
  4. Click the saved preset chip.
- **Expected:** All saved channel values restore exactly. Presets persist across page reload (localStorage) — reload Tab A and verify the chip is still there.

### 3.19 Compact mode toggle (▢ / ▣)

- **Do:** Click the **▢ / ▣** compact-mode button in the **top header bar** (right side, next to the status indicator).
- **Verify:** Fixture panels switch to a denser layout (smaller controls, tighter spacing). Click again to restore.
- **Expected:** State persists for the session; all controls remain functional in compact mode.

### 3.20 REST API spot check

```bash
curl -s http://localhost:9999/api/fixtures | jq 'length'
curl -s http://localhost:9999/api/channels | jq '. | length'
```

**Expected:** Non-zero counts matching the loaded workspace.

---

## 4. QML UI Changes

### 4.1 "Open Web Control" toolbar button

- **Precondition:** QLC+ started with `-w`.
- **Verify:** A button labeled **"Open Web Control"** (or browser-globe icon) appears in the main QML toolbar.
- **Do:** Click it.
- **Expected:** Default browser opens to **http://localhost:9999/vc/** (port 9999, not 9696).

### 4.2 No button without `-w`

- **Do:** Restart QLC+ **without** `-w`.
- **Expected:** "Open Web Control" button is **hidden** (or disabled with a tooltip explaining `-w` is required).

> ⚠️ **Restart with `-w` before continuing** — sections 4.3+ and all later sections require the web server.

### 4.3 Auto-layout button (VC editor)

- **Do:** Open Virtual Console editor → switch to a page with several widgets in arbitrary positions → click **Auto-Layout**.
- **Expected:** Widgets snap to a tidy grid (Grafana-style `gridCompact`). No widget overlap. Column detection groups widgets sensibly.

### 4.4 Grid layout mode

- **Do:** Use the menu / shortcut (e.g. **Ctrl+G**) to enable Grid layout mode.
- **Expected:** Grid overlay becomes visible while editing; widgets snap to grid cells when dragged.

### 4.5 Undo / Redo

- **Do:** After auto-layout or grid layout changes, press **Ctrl+Z**.
- **Expected:** Layout reverts in a single undo step (the change was wrapped in a `Tardis beginBatch/endBatch`). **Ctrl+Shift+Z** redoes.

### 4.6 MCP ↔ QML layout parity

- **Do:** Call the MCP `auto_layout_page` tool on a page, note widget positions. Then in QML editor, run Auto-Layout on the same page from a known initial state.
- **Expected:** Resulting positions are **identical** (unified reflow algorithm).

---

## 5. Fixture Definition — Stairville Beam Ball 100 Quad LED

### 5.1 Available in fixture browser

- **Do:** Fixtures tab → **Add Fixture** → search `Beam Ball`.
- **Expected:** **Stairville → Beam Ball 100 Quad LED** appears.

### 5.2 Mode + channel count

- **Do:** Select the fixture, inspect available modes.
- **Expected:** Mode list includes the documented mode(s); the standard mode reports **49 channels** (verify against the spec sheet bundled in `resources/fixtures/`).

### 5.3 3D model

- **Do:** Add the fixture to a universe, open the 3D view, point the camera at it.
- **Expected:** Fixture renders as a **ball / sphere** shape (not the default moving-head or PAR mesh).
- **Do:** Send pan/tilt values via the Web DMX panel.
- **Expected:** The 3D ball orients itself accordingly.

---

## 6. Security

### 6.1 Path traversal protection

```bash
curl -s -o /dev/null -w "%{http_code}\n" 'http://localhost:9999/vc/../../../etc/passwd'
curl -s -o /dev/null -w "%{http_code}\n" 'http://localhost:9999/vc/..%2F..%2F..%2Fetc%2Fpasswd'
```

**Expected:** Both return **404** (or 400). Definitely **NOT 200** with file contents.

```bash
curl -s 'http://localhost:9999/vc/../../../etc/passwd' | head -c 100
```

**Expected:** Empty / 404 page body. Must not contain `root:` or any `/etc/passwd` content.

### 6.2 Content-Type for `/vc/`

```bash
curl -sI http://localhost:9999/vc/ | grep -i '^content-type'
```

**Expected:** `Content-Type: text/html` (with optional `; charset=utf-8`).
**Failure mode:** `application/octet-stream` would cause the browser to download instead of render.

### 6.3 Static asset MIME types

```bash
curl -sI http://localhost:9999/vc/assets/index-<hash>.js  | grep -i content-type
curl -sI http://localhost:9999/vc/assets/index-<hash>.css | grep -i content-type
```

**Expected:** `application/javascript` and `text/css` respectively.

---

## 7. Performance & Misc

### 7.1 RGB matrix step-transition latency

- **Do:** Build an RGB matrix function with a fast step rate (e.g. 50 ms steps).
- **Verify:** Step transitions feel snappy; no audible/visible lag between scheduled step time and actual DMX output.
- **Expected:** Latency ≈ **3 ms** (was 22 ms before the fix). If you have a logic analyzer or the QLC+ output monitor, confirm the new timing.

### 7.2 OS2L diagnostics

- **Do:** Tools / Settings → **OS2L Diagnostics** dashboard.
- **Verify:** Dashboard renders, shows OS2L connection state, recent BPM, beat events.

### 7.3 Multi-plugin diagnostics

- **Do:** Open the diagnostics dashboard, locate per-plugin status.
- **Verify:** Each I/O plugin (DMX USB, ArtNet, sACN, MIDI, OSC…) has a runtime **enable/disable** toggle and live status.
- **Do:** Disable a plugin while running.
- **Expected:** Plugin stops without crashing; QLC+ continues running. Re-enabling restores function.

---

## 8. Beat-Based Timing (1/16 Subdivision)

### 8.1 Verify 1/16 in Chaser Editor

- **Do:** Create a new Chaser (Functions → + → Chaser). Add a few steps (scenes).
- **Do:** In the Chaser Editor, click the tempo toggle from **T** (Time) to **B** (Beats).
- **Verify:** Step timings switch to beat notation (e.g., `1`, `1/2`, `1/4`).

### 8.2 Set 1/16 fade time

- **Do:** Click a step's **Fade In** time to open the Time Editor.
- **Verify:** In Beats mode, there is a **+1/16** / **-1/16** button visible.
- **Do:** Click **+1/16** once.
- **Expected:** The fade time shows **1/16**. Click again → **1/8** (= 2/16). Click 4 times from zero → **1/4**.

### 8.3 Set 1/16 hold time

- **Do:** Click a step's **Hold** time in the Chaser Editor.
- **Verify:** Fraction buttons are now available (previously Hold was locked to whole beats only).
- **Do:** Set Hold to **3/16**.
- **Expected:** Display shows `3/16`. The step should hold for 3/16 of a beat.

### 8.4 Verify display of all fractions

- **Do:** Step through fraction values from 0 to 15/16 using +1/16.
- **Expected display sequence:** `1/16`, `1/8`, `3/16`, `1/4`, `5/16`, `3/8`, `7/16`, `1/2`, `9/16`, `5/8`, `11/16`, `3/4`, `13/16`, `7/8`, `15/16`, then wraps to `1`.

### 8.5 Playback at 120 BPM

- **Do:** Set BPM to 120 (VC → Speed Dial or internal tempo).
- **Do:** Create a 2-step chaser with Hold = 1/16 beat (= ~31ms at 120 BPM).
- **Do:** Start the chaser.
- **Expected:** Steps alternate very rapidly (~32 times per beat). Visually, you should see the two scenes flickering. The timing doesn't need to be sample-accurate — ±10ms jitter is acceptable for lighting.

### 8.6 Backward compatibility

- **Do:** Open an existing workspace that uses 1/8 beat timings.
- **Verify:** All timings display and play correctly (1/8 values are preserved in the new 16-step quantizer).

### 8.7 Unit tests

```bash
cd build && ./engine/test/beatquantize/beatquantize_test
```

- **Expected:** `11 passed, 0 failed`.

---

## 9. Known Issues / Limitations

- **WebSocket reconnect:** If QLC+ is restarted while `/vc/` is open, the page may take up to ~5 s to reconnect. A manual page refresh always recovers.
- **Cross-tab sync race:** Rapid simultaneous edits on the *same* control from two tabs can briefly show a flicker as the last-write-wins value propagates. Steady-state is always consistent.
- **MCP idempotency keys:** Idempotency is keyed on the human-readable **name** for most create tools. Renaming a function and re-running the original create call will produce a duplicate — by design.
- **Auto-layout column detection:** Widgets with extreme aspect ratios (very wide labels) can be assigned to an unexpected column. Run auto-layout twice or adjust manually.
- **3D Beam Ball model:** The ball mesh does not animate gobo wheels or color wheels — only pan/tilt orientation is reflected.
- **Compact mode + raw channels:** When compact mode is active and the raw section is auto-opened (★ fixtures), the panel can become tall. Scroll within the panel.
- **macOS Tahoe (26.x) signing:** Dev builds are ad-hoc signed *without* `--options runtime`. If you re-sign with hardened runtime, the app will crash on launch with a dyld Team ID mismatch.
- **Playwright E2E suite:** 35+ tests live under `webaccess/vc-next/e2e/`. A handful are timing-sensitive on slow machines; rerun once before declaring a flake.

---

## 10. Sign-off

| Area                     | Tester | Date | Pass / Fail | Notes |
|--------------------------|--------|------|-------------|-------|
| MCP server               |        |      |             |       |
| Web DMX Control Panel    |        |      |             |       |
| QML UI changes           |        |      |             |       |
| Beam Ball fixture        |        |      |             |       |
| Security                 |        |      |             |       |
| Performance / diagnostics|        |      |             |       |
| Beat timing (1/16)       |        |      |             |       |

**Overall:** ☐ Ready to merge ☐ Blockers found (list below)

---

*Last updated: aligned with the 30-day fork delta covering MCP refactor, Web DMX Control Panel, unified VC reflow, Beam Ball fixture, and security/perf fixes.*
