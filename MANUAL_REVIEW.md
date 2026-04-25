# QLC+ Fork — Manual Review Checklist

This checklist contains ONLY items that require human judgment (visual inspection, UX assessment, timing perception, 3D rendering). Anything that can be asserted automatically lives elsewhere:

- **Unit tests:** `cd build && ./engine/test/beatquantize/beatquantize_test && ./mcp/test/mcp_conversions_test && ./mcp/test/mcp_vc_query_filter_test && ./mcp/test/mcp_vc_validation_test`
- **Smoke test:** `./scripts/smoke-test.sh` — covers MCP server reachability, handshake, tool list, REST API spot checks, path-traversal protection, MIME types
- **E2E tests:** `cd webaccess/vc-next && npx playwright test` — covers DOM structure, filter/search behaviour, raw-channel disclosure, cross-tab sync wiring

> Run all automated tests FIRST. Only proceed with manual review once they pass — this document is for the things a machine cannot judge.

---

## 1. Setup

### 1.1 Start QLC+

```bash
cd build
./qmlui/qlcplus-qml -d
```

Flags:
- `-d` — debug logging to stdout
- Web server is **enabled by default** on port **9999**. Use `--no-web` to disable.

### 1.2 Load the test workspace

In QLC+:
1. **File → Open**
2. Select `GARAGE.qxw` (the reference workspace shipped with this fork)
3. Confirm fixtures load in the Fixtures tab — you should see the HERO and HZ fixtures referenced throughout this document.

### 1.3 URLs

| Service                  | URL                              |
|--------------------------|----------------------------------|
| Web DMX Control Panel    | http://localhost:9999/vc/        |
| Legacy web access        | http://localhost:9999/           |
| MCP server (JSON-RPC)    | http://127.0.0.1:9696/mcp        |

---

## 2. MCP Server — Composite Tools

> Reachability, handshake, `tools/list`, read-only queries, and idempotent upserts are covered by `scripts/smoke-test.sh` and the `mcp_vc_*` unit tests. The items below need a running app + hardware/complex state and a human to interpret the result.

### 2.1 `build_show_page`

- **Do:** Call with a small spec (one frame, a couple of buttons bound to existing functions).
- **Verify in QML:** A new VC page is created with widgets laid out sensibly, captions readable, no overlap.

### 2.2 `configure_launchpad`

- **Do:** With a Novation Launchpad connected (or a virtual MIDI device emulating one), call the tool.
- **Verify:** MIDI mappings appear under Inputs/Outputs; LED feedback lights up the expected pads when bound widgets are active.

---

## 3. Web DMX Control Panel — Visual / UX Review

Open http://localhost:9999/vc/ in a fresh browser tab.

> Structural DOM checks (panel rendering, filter chips, search box, universe grouping, raw-channel disclosure, read-only typed channels, reset, preset save/recall, compact-mode toggle) are covered by Playwright tests in `webaccess/vc-next/e2e/`. The items below are about how the surface **feels** — only a human can judge that.

### 3.1 Status indicator transitions

- **Do:** Kill the QLC+ process, wait, then restart.
- **Verify:** Status pill flips Live → Disconnected → Live within a few seconds of each transition. The colour change should be obvious at a glance, not subtle.

### 3.2 Color picker — feel

- **Do:** Drag the HERO color picker crosshair around the saturation/value square; drag the hue bar; nudge individual R/G/B sliders.
- **Verify:** Swatch, hue marker, crosshair, and RGB sliders all stay in sync **with no perceptible lag**. The selected colour on a real fixture matches what the swatch shows.
- **Why manual:** "Feels responsive" and "colour matches reality" are perceptual.

### 3.3 Position XY pad — cursor tracking

- **Do:** Drag the cursor across the HERO position pad, including fast flicks and into the corners.
- **Verify:** Cursor tracks the pointer smoothly (no stutter), pan/tilt degrees update live, the physical fixture moves without visible step-quantisation.

### 3.4 Dimmer fader smoothness

- **Do:** Drag the HERO dimmer fader slowly from 0 → 100 → 0.
- **Verify:** No flicker or jumps in the actual fixture output; the percentage display tracks the fader without lag.

### 3.5 WLED 320-channel stress test

- **Do:** Expand the **Raw Channels (320)** section on a WLED row. Scroll through it, drag a fader near the bottom.
- **Verify:** Scrolling is smooth (no jank). Dragging a fader feels responsive even with hundreds of sibling rows.
- **Why manual:** This is about perceived performance; Playwright cannot judge "smooth".

### 3.6 Cross-tab sync — visual instantness

- **Do:** Open `/vc/` in two tabs side by side. In Tab A, drag HERO dimmer / change colour / move the position pad.
- **Verify in Tab B:** The corresponding control animates to match within ~100 ms — fast enough that it feels instantaneous side-by-side.
- **Why manual:** Playwright asserts that sync **happens**; only a human can judge whether it feels real-time when watching both tabs at once.

### 3.7 Compact mode — readability

- **Do:** Toggle compact mode (▢ / ▣) in the top header.
- **Verify:** Dense layout is still legible — labels readable, controls still hittable, no clipped text, fixtures with the ★ raw-open behaviour don't become awkwardly tall.

---

## 4. QML UI

### 4.1 "Open Web Control" toolbar button

- **Precondition:** QLC+ started (web server is on by default).
- **Verify:** A button labeled **"Open Web Control"** (or browser-globe icon) appears in the main QML toolbar.
- **Do:** Click it.
- **Expected:** Default browser opens to **http://localhost:9999/vc/** (port 9999, not 9696).

### 4.2 Button hidden with `--no-web`

- **Do:** Restart QLC+ with `--no-web`.
- **Expected:** "Open Web Control" button is hidden (or disabled with a tooltip explaining web server is disabled).

> ⚠️ **Restart without `--no-web` before continuing.**

### 4.3 Auto-layout (VC editor) — looks tidy

- **Do:** Open Virtual Console editor → page with several widgets in arbitrary positions → click **Auto-Layout**.
- **Verify:** Widgets snap to a tidy grid (Grafana-style `gridCompact`). No overlaps. Column groupings look sensible to a human eye.

### 4.4 Grid layout mode — overlay

- **Do:** Enable Grid layout mode (e.g. **Ctrl+G**).
- **Verify:** Grid overlay is visible while editing; widgets visibly snap to cells when dragged.

### 4.5 Undo / Redo after auto-layout

- **Do:** After an auto-layout or grid change, press **Ctrl+Z** then **Ctrl+Shift+Z**.
- **Expected:** Single-step undo reverts the entire batch; redo re-applies it.
- **Note:** Engine-layer batching (`Tardis beginBatch/endBatch`) is unit-tested. The visual single-step behaviour is what we're checking here.

### 4.6 MCP ↔ QML layout parity

- **Do:** From a known initial state, snapshot widget positions. Call MCP `auto_layout_page`, snapshot. Reset, run Auto-Layout from the QML editor on the same page, snapshot.
- **Expected:** Resulting positions are visually identical (unified reflow algorithm).
- **Why manual:** Cross-surface parity comparison; small pixel deltas may be acceptable but only a human can decide.

---

## 5. Fixture — Stairville Beam Ball 100 Quad LED

> Existence of the fixture XML and channel count are verifiable by file inspection / `query_available_fixtures`. The items below need a human eye on the QML browser and the 3D viewport.

### 5.1 Appears in fixture browser

- **Do:** Fixtures tab → **Add Fixture** → search `Beam Ball`.
- **Verify:** **Stairville → Beam Ball 100 Quad LED** appears in the browser with sensible mode names and channel counts.

### 5.2 3D model

- **Do:** Add the fixture to a universe, open the 3D view, frame the camera on it.
- **Verify:** Fixture renders as a **ball / sphere** (not the default moving-head or PAR mesh).
- **Do:** Send pan/tilt values via the Web DMX panel.
- **Verify:** The 3D ball reorients in real time and matches the commanded angles.
- **Why manual:** 3D rendering correctness.

---

## 6. Performance & Diagnostics

### 6.1 RGB matrix step-transition latency

- **Do:** Build an RGB matrix function with a fast step rate (e.g. 50 ms steps). Run it.
- **Verify:** Step transitions feel snappy. With a logic analyser or scope on the DMX line, latency should be ≈ **3 ms** (was 22 ms before the fix).
- **Why manual:** Perceptual snappiness; precise measurement requires hardware.

### 6.2 OS2L diagnostics dashboard

- **Do:** Tools / Settings → **OS2L Diagnostics**.
- **Verify:** Dashboard renders, shows OS2L connection state, recent BPM, beat events ticking in real time.

### 6.3 Multi-plugin diagnostics — runtime toggle

- **Do:** Open the diagnostics dashboard, locate per-plugin status. Disable a plugin (DMX USB / ArtNet / sACN / MIDI / OSC) while functions are running. Re-enable.
- **Verify:** Plugin stops without crashing the app, status updates, re-enable restores function. No hang or zombie state.
- **Why manual:** Live runtime behaviour with real hardware.

---

## 7. Beat Timing — UI Interaction

> Quantizer math, fraction display formatting, allowFractions gating, and MCP beat-string round-trips are covered by `beatquantize_test` and `mcp_conversions_test`. The items below need a human at the Time Editor.

### 7.1 Tempo toggle visible in Chaser Editor

- **Do:** Create a new Chaser, add a couple of scene steps. In the Chaser Editor, click the tempo toggle from **T** (Time) to **B** (Beats).
- **Verify:** Step timing columns visibly switch to beat notation (e.g. `1`, `1/2`, `1/4`).

### 7.2 Set 1/16 fade time via Time Editor

- **Do:** Click a step's **Fade In** time to open the Time Editor (Beats mode).
- **Verify:** A **+1/16** / **−1/16** button is visible.
- **Do:** Click **+1/16** once → display shows `1/16`. Click again → `1/8`. Click 4× from zero → `1/4`.

### 7.3 Set 1/16 hold time

- **Do:** Click a step's **Hold** time in the Chaser Editor.
- **Verify:** Fraction buttons (including 1/16) are available — Hold is no longer locked to whole beats.
- **Do:** Set Hold to **3/16** → display shows `3/16`.

### 7.4 Playback at 120 BPM — visible flicker

- **Do:** Set BPM to 120. Build a 2-step chaser with Hold = 1/16 beat (~31 ms at 120 BPM). Start it.
- **Verify:** Steps alternate very rapidly (~32× per beat). You should see the two scenes flickering. ±10 ms jitter is acceptable for lighting.
- **Why manual:** The point is whether it *looks* right on real fixtures.

### 7.5 TimeEditTool subdivision selector

- **Do:** Open Chaser Editor → click a step's **Fade In** time (Beats mode).
- **Verify:** Bottom row shows subdivision buttons (1/1, 1/2, 1/4, 1/8). Selected subdivision is highlighted.
- **Do:** Select **1/8**, set count to 3 → display shows **3 × 1/8**. Switch to **1/4**, count stays 3 → display shows **3 × 1/4**.

### 7.6 Fine fractions (1/16) on Hold/Duration

- **Do:** In the Chaser Editor, click a step's **Hold** or **Duration** time (Beats mode).
- **Verify:** Subdivision row includes the **1/16** button (FineFractions mode), and selecting it lets you build values like `5 × 1/16`.

---

## 8. Known Issues / Limitations

- **WebSocket reconnect:** If QLC+ is restarted while `/vc/` is open, the page may take up to ~5 s to reconnect. A manual page refresh always recovers.
- **Cross-tab sync race:** Rapid simultaneous edits on the *same* control from two tabs can briefly show a flicker as the last-write-wins value propagates. Steady-state is always consistent.
- **MCP idempotency keys:** Idempotency is keyed on the human-readable **name** for most create tools. Renaming a function and re-running the original create call will produce a duplicate — by design.
- **Auto-layout column detection:** Widgets with extreme aspect ratios (very wide labels) can be assigned to an unexpected column. Run auto-layout twice or adjust manually.
- **3D Beam Ball model:** The ball mesh does not animate gobo wheels or color wheels — only pan/tilt orientation is reflected.
- **Compact mode + raw channels:** When compact mode is active and the raw section is auto-opened (★ fixtures), the panel can become tall. Scroll within the panel.
- **macOS Tahoe (26.x) signing:** Dev builds are ad-hoc signed *without* `--options runtime`. If you re-sign with hardened runtime, the app will crash on launch with a dyld Team ID mismatch.
- **Playwright E2E suite:** 35+ tests live under `webaccess/vc-next/e2e/`. A handful are timing-sensitive on slow machines; rerun once before declaring a flake.

---

## 9. Sign-off

| Area                          | Tester | Date | Pass / Fail | Notes |
|-------------------------------|--------|------|-------------|-------|
| Automated tests (unit + smoke + E2E) |  |      |             | Must pass before manual review |
| MCP composite tools           |        |      |             |       |
| Web DMX visual / UX           |        |      |             |       |
| QML UI visual                 |        |      |             |       |
| Beam Ball 3D model            |        |      |             |       |
| Performance / diagnostics     |        |      |             |       |
| Beat timing UI                |        |      |             |       |

**Overall:** ☐ Ready to merge ☐ Blockers found (list below)

---

*Last updated: split from the full test plan — automated checks now live in `scripts/smoke-test.sh`, the `engine/test/beatquantize` + `mcp/test/*` unit tests, and `webaccess/vc-next/e2e/` Playwright suites. This document covers only items that require human judgement.*
