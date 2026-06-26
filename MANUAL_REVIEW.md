# QLC+ Fork — Manual Review Checklist

This checklist contains ONLY items that require human judgment (visual inspection, UX assessment, timing perception, 3D rendering). Anything that can be asserted automatically lives elsewhere:

- **Unit tests:** `cd build && ./engine/test/beatquantize/beatquantize_test && ./engine/test/function/function_test && ./engine/test/show/show_test && ./mcp/test/mcp_conversions_test && ./mcp/test/mcp_vc_query_filter_test && ./mcp/test/mcp_vc_validation_test && ./mcp/test/mcp_vcpage_input_mode_test && ./qmlui/test/djfsm/djfsm_test && ./qmlui/test/djmanager/djmanager_test && ./qmlui/test/audiobpmtag/audiobpmtag_test && ./qmlui/test/showfactory/showfactory_test && ./qmlui/test/vdjbridge/vdjbridge_test && ./qmlui/test/vdjtelemetryclient/vdjtelemetryclient_test`
- **Smoke test:** `./scripts/smoke-test.sh` — covers MCP server reachability, handshake, tool list, REST API spot checks, path-traversal protection, MIME types
- **E2E tests:** `cd webaccess/web-dmx && npx playwright test` — covers DOM structure, filter/search behaviour, raw-channel disclosure, cross-tab sync wiring

> Run all automated tests FIRST. Only proceed with manual review once they pass — this document is for the things a machine cannot judge.

**Hardware requirement tags** — test cases are tagged with `[MIDI]`, `[DMX]`, or `[VDJ]` to indicate required equipment. Items without tags can be tested with just QLC+ running on a laptop. The [review webserver](tools/manual-review/) supports filtering by these tags.

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

### 2.2 `configure_launchpad` [MIDI]

- **Do:** With a Novation Launchpad connected (or a virtual MIDI device emulating one), call the tool.
- **Verify:** MIDI mappings appear under Inputs/Outputs; LED feedback lights up the expected pads when bound widgets are active.

---

## 3. Web DMX Control Panel — Visual / UX Review

Open http://localhost:9999/vc/ in a fresh browser tab.

> Structural DOM checks (panel rendering, filter chips, search box, universe grouping, raw-channel disclosure, read-only typed channels, reset, preset save/recall, compact-mode toggle) are covered by Playwright tests in `webaccess/web-dmx/e2e/`. The items below are about how the surface **feels** — only a human can judge that.

### 3.1 Status indicator transitions

- **Do:** Kill the QLC+ process, wait, then restart.
- **Verify:** Status pill flips Live → Disconnected → Live within a few seconds of each transition. The colour change should be obvious at a glance, not subtle.

### 3.2 Color picker — feel [DMX]

- **Do:** Drag the HERO color picker crosshair around the saturation/value square; drag the hue bar; nudge individual R/G/B sliders.
- **Verify:** Swatch, hue marker, crosshair, and RGB sliders all stay in sync **with no perceptible lag**. The selected colour on a real fixture matches what the swatch shows.
- **Why manual:** "Feels responsive" and "colour matches reality" are perceptual.

### 3.3 Position XY pad — cursor tracking [DMX]

- **Do:** Drag the cursor across the HERO position pad, including fast flicks and into the corners.
- **Verify:** Cursor tracks the pointer smoothly (no stutter), pan/tilt degrees update live, the physical fixture moves without visible step-quantisation.

### 3.4 Dimmer fader smoothness [DMX]

- **Do:** Drag the HERO dimmer fader slowly from 0 → 100 → 0.
- **Verify:** No flicker or jumps in the actual fixture output; the percentage display tracks the fader without lag.

### 3.5 WLED 320-channel stress test [DMX]

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

### 4.6 Update Scene from Live

- **Precondition:** A scene with at least 3–4 channels (e.g. a color scene with RGBW values).
- **Do:**
  1. Start the scene (it outputs its values to the fixtures).
  2. Open Simple Desk. Adjust one or two faders that belong to the scene's channels — change the look.
  3. Click the **DMX Dump** toolbar button (📷).
  4. Select **"Dump to existing Scene"** → pick the running scene.
  5. Click **"Update only scene channels from live"**.
- **Verify:**
  - The scene's channel values update to match what was live.
  - **Only channels already in the scene** are updated — no new channels added.
  - The running scene immediately reflects the new values on the fixtures.
  - Open Scene Editor → verify the fader positions match the captured values.
- **Do:** Press **Ctrl+Z** (undo).
- **Verify:** All channel values revert to their previous values. Fixtures respond.
- **Why manual:** The "only existing channels" scoping and live preview are perceptual checks.

### 4.7 Update Scene from Live — layer separation

- **Precondition:** A color-only scene (RGBW channels only, no dimmer ch5).
- **Do:**
  1. In Simple Desk, set dimmer (ch5) to 200 and change some RGBW values.
  2. DMX Dump → select the color scene → "Update only scene channels from live".
- **Verify:**
  - RGBW values are updated.
  - **Dimmer (ch5) is NOT added** to the scene — it stays color-only.
- **Why manual:** Validates that layer separation is preserved during live capture.

### 4.8 MCP ↔ QML layout parity

- **Do:** From a known initial state, snapshot widget positions. Call MCP `auto_layout_page`, snapshot. Reset, run Auto-Layout from the QML editor on the same page, snapshot.
- **Expected:** Resulting positions are visually identical (unified reflow algorithm).
- **Why manual:** Cross-surface parity comparison; small pixel deltas may be acceptable but only a human can decide.

---

## 4B. Page-Dependent External Input Mappings

> **Automated coverage:** `mcp_vcpage_input_mode_test` covers string conversion round-trips, XML save/load contract, backward compatibility (missing tag defaults to Normal), and PageInfo struct field. The items below require a running app with MIDI hardware (or virtual MIDI) and multiple VC pages.

### 4B.1 Normal mode (default) — existing behaviour preserved [MIDI]

- **Precondition:** Two VC pages, both in **Normal** mode (default). Page 1 has a button mapped to MIDI CC 1. Page 2 has a slider mapped to MIDI CC 2.
- **Do:** Switch to Page 1, send MIDI CC 1.
- **Verify:**
  - ☐ Page 1's button activates
  - ☐ Page 2's slider does NOT respond (only active page receives input)
- **Why manual:** Confirms baseline behaviour unchanged by this feature.

### 4B.2 Override mode — isolation [MIDI]

- **Precondition:** Page 1 set to **Normal**, Page 2 set to **Override**. Both have widgets mapped to the same MIDI CC.
- **Do:** Switch to Page 2 (Override active), send the shared MIDI CC.
- **Verify:**
  - ☐ Only Page 2's widget responds
  - ☐ Page 1's widget does NOT respond
- **Do:** Switch to Page 1, send the same MIDI CC.
- **Verify:**
  - ☐ Only Page 1's widget responds (Override only applies when the Override page is active)
- **Why manual:** Perceptual check that isolation works correctly with real MIDI events.

### 4B.3 Inherit mode — fallback to Normal pages [MIDI]

- **Precondition:** Page 1 (Normal) has a button on MIDI CC 10 and a slider on MIDI CC 20. Page 2 (Inherit) has a slider on MIDI CC 20 (same as Page 1) but nothing on MIDI CC 10.
- **Do:** Switch to Page 2 (Inherit active).
- **Do:** Send MIDI CC 10.
- **Verify:**
  - ☐ Page 1's button activates (fallback — Page 2 has no mapping for CC 10, so Normal pages contribute)
- **Do:** Send MIDI CC 20.
- **Verify:**
  - ☐ Only Page 2's slider responds (Page 2 owns this mapping, no fallback)
  - ☐ Page 1's slider does NOT respond (Inherit page claims the key)
- **Why manual:** Conflict resolution at key-level granularity is subtle and best verified with real MIDI.

### 4B.4 Inherit + relative controller (encoder) [MIDI]

- **Precondition:** Page 1 (Normal) has a slider set to **relative** input mode on MIDI CC 30. Page 2 (Inherit) has no mapping for CC 30.
- **Do:** Switch to Page 2 (Inherit active), turn the MIDI encoder sending CC 30.
- **Verify:**
  - ☐ Page 1's slider moves incrementally (relative mode preserved through global dispatch)
  - ☐ No jumps or resets — smooth incremental movement
- **Why manual:** Relative/encoder behaviour is perceptual and hardware-dependent.

### 4B.5 Keyboard shortcut dispatch mirrors MIDI

- **Precondition:** Page 1 (Normal) has a button bound to keyboard shortcut **F5**. Page 2 (Inherit) has no keyboard binding for F5.
- **Do:** Switch to Page 2, press F5.
- **Verify:**
  - ☐ Page 1's button activates (keyboard fallback works same as MIDI)
- **Why manual:** Keyboard dispatch follows the same mode logic; verifying with physical key presses.

### 4B.6 Page-activation bindings are mode-independent [MIDI]

- **Precondition:** A MIDI note or keyboard shortcut is bound to activate Page 2 (via the VC page-switching mechanism). Page 2 is set to Override mode.
- **Do:** Send the page-activation MIDI note from any page.
- **Verify:**
  - ☐ Page 2 activates regardless of its ExternalInputMode
  - ☐ Page-activation bindings are never suppressed by Override mode
- **Why manual:** Ensures page-switching always works — getting locked out of a page would be a showstopper in live performance.

### 4B.7 XML persistence round-trip

- **Do:** Set Page 1 to Normal, Page 2 to Override, Page 3 to Inherit.
- **Do:** Save workspace (File → Save As).
- **Do:** Close and reopen the workspace.
- **Verify:**
  - ☐ Page 1 is Normal (no `ExternalInputMode` tag in XML — backward compat)
  - ☐ Page 2 is Override
  - ☐ Page 3 is Inherit
- **Do:** Open the `.qxw` file in a text editor and confirm the `<ExternalInputMode>` tag appears only for Override/Inherit pages.
- **Why manual:** Validates full persistence through the Qt XML stack, not just the unit-tested contract.

### 4B.8 MCP query exposes mode

- **Do:** With pages set to different modes, call `vc_query_pages` via MCP.
- **Verify:**
  - ☐ Each page object in the JSON response includes `"externalInputMode"` with the correct value (`"Normal"`, `"Override"`, or `"Inherit"`)
- **Why manual:** Confirms the MCP bridge correctly reads the live property from the running app.

### 4B.9 QML combo box for input mode selection

- **Precondition:** Virtual Console is in design/edit mode.
- **Do:** Right-click a VC page header → Properties (or open page properties panel).
- **Verify:**
  - ☐ An "External Input Mode" combo box appears with options: Normal, Override, Inherit
  - ☐ Default selection is "Normal" for new pages
  - ☐ Changing the combo box to "Override" immediately updates the page's mode (verify via MCP query or save/reload)
  - ☐ The combo box reflects the correct mode when switching between page property panels
- **Why manual:** Visual/interactive QML — cannot be unit tested.

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

## 6. Script Fader Cleanup

### 6.1 Script setFixture values clear on stop

- **Precondition:** A Script function that uses `Engine.setFixture()` to write channel values.
- **Do:**
  1. Start the script — verify fixtures respond.
  2. Stop the script (click the button again or stop from Function Manager).
- **Verify:** All channels the script was writing return to **zero** (or to whatever other functions are driving them). No residual "stuck" values.
- **Do:** Check in Simple Desk or via MCP `read_dmx_values` — the channels should not hold leftover script values.
- **Why manual:** The fix zeros the universe buffer on script stop. Previously, values persisted after stop, requiring a restart to clear.

---

## 7. DJ Expression System

### 7.1 Page 3 — DJ Expression layout

- **Do:** Navigate to VC Page 3 ("DJ Expression").
- **Verify:**
  - **ENERGY / PHASE** SoloFrame with CHILL / FREEZE / BUILD / DROP buttons.
  - **MOOD** section with 4 color theme rows (Jungle / Ocean / Fire / Neon).
  - **MOVEMENT** SoloFrame with circle/sweep/freeze options.
  - **FX / MOMENTS** section with STROBE buttons + WHITE HIT + RESET.
  - **MASTER** section with Intensity slider, Strobe slider, BLACKOUT button.

### 7.2 Layer separation — color vs dimmer [DMX]

- **Do:**
  1. Press a MOOD color button (e.g. JG Deep Green).
  2. Verify the fixture shows the color.
  3. Check that the dimmer channel (BB ch5) is **not** set by the color scene.
  4. Adjust the Intensity slider → dimmer changes independently from the color.
- **Why manual:** Validates HTP layer separation works correctly in practice.

### 7.3 Phase transitions [DMX]

- **Do:** Press CHILL → verify low dimmer + slow movement. Press DROP → verify full dimmer + fast movement.
- **Verify:** SoloFrame stops the previous phase when a new one is pressed. No leftover functions from the previous phase.

### 7.4 Strobe via ch6 (fixture-native) [DMX]

- **Do:**
  1. Set Intensity slider above 0.
  2. Press STROBE FAST → fixture strobes.
  3. Press STROBE OFF → strobe stops.
  4. Push the Strobe slider up → continuous strobe speed control.
- **Verify:** Strobe works regardless of Intensity slider position (ch6 is LTP, independent of ch5 HTP).

---

## 8. Performance & Diagnostics

### 6.1 RGB matrix step-transition latency [DMX]

- **Do:** Build an RGB matrix function with a fast step rate (e.g. 50 ms steps). Run it.
- **Verify:** Step transitions feel snappy. With a logic analyser or scope on the DMX line, latency should be ≈ **3 ms** (was 22 ms before the fix).
- **Why manual:** Perceptual snappiness; precise measurement requires hardware.

### 6.2 OS2L diagnostics dashboard [VDJ]

- **Do:** Tools / Settings → **OS2L Diagnostics**.
- **Verify:** Dashboard renders, shows OS2L connection state, recent BPM, beat events ticking in real time.

### 6.3 Multi-plugin diagnostics — runtime toggle [DMX]

- **Do:** Open the diagnostics dashboard, locate per-plugin status. Disable a plugin (DMX USB / ArtNet / sACN / MIDI / OSC) while functions are running. Re-enable.
- **Verify:** Plugin stops without crashing the app, status updates, re-enable restores function. No hang or zombie state.
- **Why manual:** Live runtime behaviour with real hardware.

---

## 9. Beat Timing — UI Interaction

> Quantizer math, fraction display formatting, allowFractions gating, and MCP beat-string round-trips are covered by `beatquantize_test` and `mcp_conversions_test`. The items below need a human at the Time Editor.

### 9.1 Tempo toggle visible in Chaser Editor

- **Do:** Create a new Chaser, add a couple of scene steps. In the Chaser Editor, click the tempo toggle from **T** (Time) to **B** (Beats).
- **Verify:** Step timing columns visibly switch to beat notation (e.g. `1`, `1/2`, `1/4`).

### 9.2 Set 1/16 fade time via Time Editor

- **Do:** Click a step's **Fade In** time to open the Time Editor (Beats mode).
- **Verify:** A **+1/16** / **−1/16** button is visible.
- **Do:** Click **+1/16** once → display shows `1/16`. Click again → `1/8`. Click 4× from zero → `1/4`.

### 9.3 Set 1/16 hold time

- **Do:** Click a step's **Hold** time in the Chaser Editor.
- **Verify:** Fraction buttons (including 1/16) are available — Hold is no longer locked to whole beats.
- **Do:** Set Hold to **3/16** → display shows `3/16`.

### 9.4 Playback at 120 BPM — visible flicker

- **Do:** Set BPM to 120. Build a 2-step chaser with Hold = 1/16 beat (~31 ms at 120 BPM). Start it.
- **Verify:** Steps alternate very rapidly (~32× per beat). You should see the two scenes flickering. ±10 ms jitter is acceptable for lighting.
- **Why manual:** The point is whether it *looks* right on real fixtures.

### 9.5 TimeEditTool subdivision selector

- **Do:** Open Chaser Editor → click a step's **Fade In** time (Beats mode).
- **Verify:** Bottom row shows subdivision buttons (1/1, 1/2, 1/4, 1/8). Selected subdivision is highlighted.
- **Do:** Select **1/8**, set count to 3 → display shows **3 × 1/8**. Switch to **1/4**, count stays 3 → display shows **3 × 1/4**.

### 9.6 Fine fractions (1/16) on Hold/Duration

- **Do:** In the Chaser Editor, click a step's **Hold** or **Duration** time (Beats mode).
- **Verify:** Subdivision row includes the **1/16** button (FineFractions mode), and selecting it lets you build values like `5 × 1/16`.

---

## 10. Keyboard Shortcuts & Tooltips

> No automated QML UI shortcut harness exists today (no QML `TestCase`/`SignalSpy`, `qmltestrunner`, `qmlscene`, Selenium, or Playwright tests under `qmlui/`). These checks require manual verification in the running v5 UI.

### 10.1 Global Shortcuts

Test these from ANY context (Fixtures & Functions, Virtual Console, Show Manager, Simple Desk):

| Shortcut | Expected Behavior | Verify |
|----------|-------------------|--------|
| Ctrl+N (⌘N on Mac) | Opens save-first prompt if project modified, then new workspace | ☐ Modified project → save prompt appears |
| Ctrl+O (⌘O on Mac) | Opens save-first prompt if project modified, then file dialog | ☐ File dialog opens after save prompt |
| Ctrl+S (⌘S on Mac) | Saves current file, or Save As if unnamed | ☐ Saves without dialog if file exists |
| Ctrl+Z (⌘Z on Mac) | Undo last action | ☐ Works globally, not just in VC editor |
| Ctrl+Shift+Z (⌘⇧Z on Mac) | Redo | ☐ Works globally |
| Ctrl+Shift+Esc (⌃⇧⎋ on Mac) | Stop all running functions (Panic) | ☐ Only enabled when functions are running |
| F11 / Ctrl+F11 | Toggle fullscreen | ☐ Toggles fullscreen mode |
| Alt+1 ... Alt+6 (⌥1 ... ⌥6 on Mac) | Switches to visible main views in toolbar order | ☐ Each visible view activates |
| Ctrl+PgDown / Ctrl+PgUp (⌘PgDown / ⌘PgUp on Mac) | Cycles forward/backward through visible main views | ☐ Wraps at first/last visible view |

### 10.2 Shortcut Guards

| Scenario | Expected | Verify |
|----------|----------|--------|
| Type in a search/text field, press Ctrl+Z | Should undo TEXT, not trigger app undo | ☐ Text editing takes priority |
| Open Actions menu, press Ctrl+S | Should NOT trigger save while menu is open | ☐ No action fires |
| Open a popup/dialog, press Ctrl+N | Should NOT trigger new workspace | ☐ Popup blocks shortcuts |
| Kiosk mode (AC_VCControl only) | Ctrl+N/O/S should be disabled | ☐ No response in kiosk mode |
| Type in a text/search field, press Alt+1 or Ctrl+PgDown | Should not switch views | ☐ Text editing blocks view shortcuts |

### 10.3 Function Manager Shortcuts

Navigate to Fixtures & Functions → Function Manager:

| Shortcut | Expected | Verify |
|----------|----------|--------|
| Select function(s), press Delete | Opens delete confirmation popup | ☐ Same behavior as minus button |
| Select function(s), press Ctrl+C | Clones selected functions | ☐ Clone appears in list |
| Ctrl+W | Opens Function Wizard | ☐ Wizard popup opens |
| Ctrl+[ | Toggles Fixtures & Functions left panel | ☐ Left panel opens/closes |
| Ctrl+] | Toggles Fixtures & Functions right panel | ☐ Right panel opens/closes |

### 10.4 Show Manager Shortcuts

Navigate to Show Manager:

| Shortcut | Expected | Verify |
|----------|----------|--------|
| Space | Play/resume show | ☐ Same as play button |
| Ctrl+Space | Stop show | ☐ Same as stop button |
| Select items, Ctrl+C | Copy to clipboard | ☐ Items copied |
| Ctrl+V | Paste from clipboard | ☐ Items pasted |
| Select items, Delete | Opens delete confirmation | ☐ Same as minus button |
| Ctrl+] | Toggles Show Manager right panel | ☐ Right panel opens/closes |

### 10.5 Virtual Console Panel Shortcut

Navigate to Virtual Console with VC editing access:

| Shortcut | Expected | Verify |
|----------|----------|--------|
| Ctrl+] | Toggles Virtual Console right panel | ☐ Right panel opens/closes only with edit access |

### 10.6 Tooltips

| Element | Expected | Verify |
|---------|----------|--------|
| Hover over Stop All button | Tooltip shows shortcut hint | ☐ Shows "Stop all... (Ctrl+Shift+Esc)" |
| Hover over Actions menu entries | Menu shows right-aligned shortcut text | ☐ "New" shows "Ctrl+N" on right |
| Hover over main view toolbar entries | Tooltip shows Alt+number hint | ☐ Shows "Alt+1" etc. (⌥ glyphs on Mac) |
| Hover over side panel toggle buttons | Tooltip includes Ctrl+[ or Ctrl+] hint | ☐ Platform-correct hint appears |
| macOS: shortcut text | Shows ⌘ not Ctrl | ☐ Platform-correct glyphs |
| GenericButton with tooltip set | Tooltip appears after 1s hover | ☐ Styled same as IconButton tooltips |

### 10.7 Speed Dial Multiply Mode

| Test | Expected | Verify |
|------|----------|--------|
| Enable multiply mode in properties | Dial/knob/tap hidden, only factor buttons visible | ☐ UI changes correctly |
| Press 2x button | Controlled functions' speeds doubled | ☐ Chaser runs at 2× speed |
| Press 1/2x button | Controlled functions' speeds halved | ☐ Chaser runs at ½ speed |
| Press Reset | Speeds restored to original, factor shows 1x | ☐ Original timing restored |
| Functions with infinite/default speed | Unaffected by multiply | ☐ Sentinel values preserved |
| Toggle multiply OFF | Current multiplied speeds kept | ☐ No auto-reset |
| Toggle multiply ON again | Fresh snapshot from current speeds | ☐ New baseline captured |

### 10.8 Beat Subdivision (FineFractions)

| Editor | Expected | Verify |
|--------|----------|--------|
| RGB Matrix → Steps hold → clock icon | Shows 1/1, 1/2, 1/4, 1/8, 1/16 buttons | ☐ All 5 subdivisions visible |
| EFX → Speed fields → clock icon | Same 5 subdivisions | ☐ All visible |
| Scene → Speed fields → clock icon | Same 5 subdivisions | ☐ All visible |
| Speed Dial presets → time editor | Same 5 subdivisions | ☐ All visible |
| Select 1/16, verify value | Shows correct beat value | ☐ Value is 63ms equivalent |

---

## 11. Song Manager — VDJ Integration

> Unit tests for the model and sort/filter logic: `cd build && ./qmlui/test/djmanager/djmanager_test` (24 tests). The items below require VDJ connected or visual verification.

### 11.1 Currently playing indicator [VDJ] [HIGH RISK]

> HIGH RISK: Depends on live VDJ connection timing, TCP telemetry latency, and deck-state transitions. Hard to reproduce consistently without exact VDJ version + network conditions.

- **Prerequisite:** VDJ Bridge plugin connected (status bar shows "Connected ●" in green).
- **Do:** Play a song in VDJ that has a corresponding Show in the Songs folder.
- **Verify:**
- ☐ The playing song shows a green `▶` indicator and highlighted row
- ☐ When the song stops, the indicator clears
- ☐ Playing indicator tracks across deck changes

### 11.2 Timestamp persistence [MEDIUM RISK]

> MEDIUM RISK: Requires specific sequence of play + save + reload. State serialization bugs are subtle and may depend on workspace XML format version.

- **Do:** Play a song, note it in the list. Save workspace (`Ctrl+S`). Close and reopen QLC+. Load the workspace.
- **Verify:**
- ☐ "Recently Played" sort still shows the previously-played song with its timestamp
- ☐ "Recently Edited" sort reflects edits made before save

### 11.3 Song list population [VDJ] [MEDIUM RISK]

> MEDIUM RISK: Requires VDJ connected and ShowFactory deferred timer (3s). Depends on correct folder path matching.

- **Do:** Open Song Manager (toolbar icon). Load a workspace with Shows in the `Songs/` folder.
- **Verify:** All Songs-folder Shows appear in the list with title and duration.
- ☐ Pre-existing Songs-folder shows appear on workspace load
- ☐ Songs created mid-session by VDJ song-load appear incrementally

### 11.4 Search / filter [LOW RISK]

> LOW RISK: Deterministic UI verification — no external hardware or timing dependencies.

- **Do:** Type a partial song title in the search bar.
- **Verify:**
- ☐ List filters in real-time as you type
- ☐ Case-insensitive matching works
- ☐ Clear button (✕) resets the filter
- ☐ Empty-state message changes to "No songs match …" when filter has no results

### 11.5 Sort modes [LOW RISK]

> LOW RISK: Deterministic UI verification — sort logic is covered by unit tests; this confirms QML binding only.

| Action | Expected | Check |
|--------|----------|-------|
| Select "Alphabetical" + ▲ | Songs A→Z | ☐ |
| Toggle to ▼ | Songs Z→A | ☐ |
| Select "Recently Played" | Most recently played first, never-played at bottom | ☐ |
| Select "Recently Edited" | Most recently edited first | ☐ |

---

## 13. Known Issues / Limitations

- **WebSocket reconnect:** If QLC+ is restarted while `/vc/` is open, the page may take up to ~5 s to reconnect. A manual page refresh always recovers.
- **Cross-tab sync race:** Rapid simultaneous edits on the *same* control from two tabs can briefly show a flicker as the last-write-wins value propagates. Steady-state is always consistent.
- **MCP idempotency keys:** Idempotency is keyed on the human-readable **name** for most create tools. Renaming a function and re-running the original create call will produce a duplicate — by design.
- **Auto-layout column detection:** Widgets with extreme aspect ratios (very wide labels) can be assigned to an unexpected column. Run auto-layout twice or adjust manually.
- **3D Beam Ball model:** The ball mesh does not animate gobo wheels or color wheels — only pan/tilt orientation is reflected.
- **Compact mode + raw channels:** When compact mode is active and the raw section is auto-opened (★ fixtures), the panel can become tall. Scroll within the panel.
- **Script fader cleanup:** When a script is force-stopped (not completed naturally), the `ScriptRunner::stop()` now zeros all channels in the universe buffer. Previously values persisted until app restart.
- **DJ Expression layer separation:** All color scenes (including old Colors/, Beam Ball/, Ball/ folders) have been stripped of ch5 (master dimmer). Only DJX Dimmer scenes may set ch5.
- **macOS Tahoe (26.x) signing:** Dev builds are ad-hoc signed *without* `--options runtime`. If you re-sign with hardened runtime, the app will crash on launch with a dyld Team ID mismatch.
- **Playwright E2E suite:** 35+ tests live under `webaccess/web-dmx/e2e/`. A handful are timing-sensitive on slow machines; rerun once before declaring a flake.
- **Keyboard shortcuts — popupCount guard:** If a popup is destroyed without properly closing (crash/error), the popupCount may get stuck and block all shortcuts. Restart QLC+ to recover. (Underflow is guarded via `Math.max(0, ...)`)
- **Song Manager — timestamps are session-relative:** `lastPlayed` and `lastEdited` timestamps are persisted to the workspace XML, but only updated during the current session. They reflect the last time the workspace was used, not absolute calendar history.
- **Song Manager — artist/BPM/key metadata:** Artist, BPM, and key fields in the song list are placeholders. The Show name embeds "Artist - Title" but structured extraction is not yet implemented.
- **Song Manager — folder path:** Songs are identified by Shows in the `Songs/` function folder. Manually placing non-song Shows in that folder will cause them to appear in the Song Manager.
- **Page input modes — multiple Normal pages:** If multiple Normal pages define mappings for the same MIDI channel/key, all matching Normal pages will fire when Inherit-mode fallback triggers. This is by design (they all contribute to the "global pool"), but can cause double-triggers if the same widget binding exists on two Normal pages.

---

## 12. VDJ Beat-Synced Show Playback [VDJ]

> **Automated coverage:** `showrunner_test` (9 tests) and `show_test` (10 tests) cover the external sync engine: mode switching, position-driven function start/stop, forward jumps, backward seeks, and Show API delegation. `djfsm_test` (21 tests) covers the 4-deck FSM in isolation, including per-deck play-position (beat+time) tracking with throttled `positionChanged`. `showfactory_test` (9 tests) covers Audio+Show+Track creation and dedup. `audiobpmtag_test` (16 tests) covers reading the song's BPM from its MP3 ID3 `TBPM` tag and applying it to the created Show's time-division. `vdjbridge_test` (12 tests) covers auto-start, auto-pause, auto-resume integration, engine-BPM-follows-VDJ (jitter-locked), and Perform load/sync/switch. The items below require a live VDJ connection.

### 12.1 VDJ disconnect [HIGH RISK]

> HIGH RISK: Depends on TCP connection teardown timing, thread-safety of ShowRunner atomic state, and graceful degradation. Hard to reproduce — race conditions may only surface under load.

- **Do:** While a Show is playing, disconnect VDJ (close the app or kill the TCP connection).
- **Verify:**
  - ☐ The Show freezes at the last known position (no runaway)
  - ☐ No crash
  - ☐ Reconnecting VDJ and playing the same song resumes or restarts the Show

### 12.2 Seek / loop handling [HIGH RISK]

> HIGH RISK: Depends on VDJ position reporting rate, backward-seek detection threshold, and function lifecycle management. Functions stopped mid-execution may leak state.

- **Do:** While a Show with functions on the timeline is playing, seek backward in VDJ (scratch or jump).
- **Verify:**
  - ☐ Debug output shows `[ShowRunner] Backward seek detected: <old> -> <new>`
  - ☐ Functions that were running are stopped and re-evaluated at the new position
  - ☐ No crash or stuck functions

### 12.3 Tempo scaling [MEDIUM RISK]

> MEDIUM RISK: Requires VDJ tempo slider adjustment and visual confirmation of proportional timeline movement. Timing precision depends on VDJ telemetry update interval.

- **Do:** With a song playing, change VDJ tempo (e.g. +10% from original BPM).
- **Verify:**
  - ☐ Show timeline progresses faster/slower proportionally (visual: cursor moves faster on speedup)
  - ☐ If the Show has lighting functions placed on the timeline, they fire at the tempo-shifted times

### 12.4 Auto-creation and sync mode [MEDIUM RISK]

> MEDIUM RISK: Requires VDJ connected and depends on ShowFactory deferred creation timer (3s). Verifies the full telemetry→FSM→factory pipeline end-to-end.

- **Prereq:** VDJ running and connected to QLC+ (telemetry strip shows "Connected")
- **Do:** Load a song on VDJ deck 1 (the master deck).
- **Verify:**
  - ☐ Song Manager shows the song after ~3 seconds (ShowFactory deferred creation)
  - ☐ In Show Manager, select the auto-created Show → it should exist under the `Songs/` folder
  - ☐ The Show's sync source should be `External` (check debug output for `[ShowRunner] Sync source set to External` when playing)

### 12.5 Auto-start / auto-pause [LOW RISK]

> LOW RISK: Core logic is well-covered by unit tests. Manual verification confirms QML UI updates track the bridge state correctly.

- **Do:** Press Play on VDJ deck 1.
- **Verify:**
  - ☐ Debug output shows `[VdjBridge] Auto-starting show: <song name>`
  - ☐ Show Manager timeline cursor begins advancing in sync with VDJ playback position
- **Do:** Press Pause on VDJ deck 1.
- **Verify:**
  - ☐ Debug output shows `[VdjBridge] Auto-pausing show: <song name>`
  - ☐ Show Manager timeline cursor stops

---

## 13b. VCAnimation — On-Widget Algorithm Parameter Controls

**Prerequisite:** Load a workspace with an RGBMatrix function assigned to a VCAnimation widget. The RGBMatrix must use a **Script** algorithm that has Range and/or List properties (e.g., "Plasma", "Diamonds", or any custom script with `// Properties` block).

### 13b.1 Parameter display on algorithm change

- ☐ Switch the algorithm in the VCAnimation combo box to a Script with parameters → mini-sliders (Range) and/or dropdowns (List) appear below the algorithm combo
- ☐ Switch to a non-Script algorithm (e.g., "Full Columns") → parameter controls disappear
- ☐ Switch back to a Script with different parameters → controls update to match the new algorithm's properties

### 13b.2 Range parameter interaction

- ☐ Adjust a Range parameter SpinBox on the widget face → DMX output changes (verify via Simple Desk or universe monitor)
- ☐ SpinBox respects min/max bounds from the script property definition

### 13b.3 List parameter interaction

- ☐ Change a List parameter dropdown on the widget face → effect changes visually (verify lighting output)
- ☐ Dropdown shows all values defined in the script's List property

### 13b.4 MIDI → widget face sync [MIDI]

- ☐ Assign a MIDI controller to a parameter external control in the VCAnimation **properties panel** (External Controls section)
- ☐ Move the MIDI knob → the corresponding on-widget SpinBox/dropdown updates in real time
- ☐ MIDI assignment is ONLY available in the properties panel External Controls, NOT on the widget face controls

### 13b.5 Visibility toggle

- ☐ Open VCAnimation properties → "Visible elements" section → uncheck "Parameters" → parameter controls hide on widget face
- ☐ Re-check "Parameters" → controls reappear
- ☐ Save/load workspace → visibility setting persists

### 13b.6 No regression

- ☐ Fader, caption label, color buttons, and algorithm combo all still work correctly
- ☐ Existing MIDI mappings for Intensity and Algorithm are unaffected

---

## 15. Upstream 5.3.0 merge regressions

**Context:** Pulled 7 upstream commits (palette overhaul, 3D position tools, sequence editor preview, Tardis live actions). These touch areas the fork extends, so verify no fork regressions after the merge.

### 15.1 Palette regression [HIGH RISK]

Upstream `26661f9` reworked palettes (new 3D-position and shutter palette types, changed serialization, palette-manager filters).

- ☐ MCP `query_palettes` returns all existing palettes without error, including any new 3D-position / shutter types
- ☐ MCP palette create/update tools still succeed against a workspace saved by the merged build
- ☐ PaletteManager UI: filters work, and the fanning box is hidden while editing a palette
- ☐ Load a pre-merge `.qxw` with palettes → all palettes load and apply correctly (no silent drops)

### 15.2 3D view smoke [MEDIUM RISK]

Upstream `71bc12e`/`e4115da` added the 3D position marker and a lock flag preventing fixture drag in 2D/3D previews.

- ☐ Fixtures still load and render in the 3D view
- ☐ New lock flag toggles correctly; locked fixtures cannot be dragged in 2D or 3D preview
- ☐ Position3D marker renders and is selectable

### 15.3 Sequence editor [LOW RISK]

Upstream `db8151f` added a channels-preview toggle and auto-selects newly added steps.

- ☐ Adding a step auto-selects it; entered values land in the new step
- ☐ Channels-preview toggle works without breaking fork show/sequence editing

---

## 14. Sign-off

| Area                          | Tester | Date | Pass / Fail | Notes |
|-------------------------------|--------|------|-------------|-------|
| Automated tests (unit + smoke + E2E) |  |      |             | Must pass before manual review |
| MCP composite tools           |        |      |             |       |
| Web DMX visual / UX           |        |      |             |       |
| QML UI visual                 |        |      |             |       |
| Update Scene from Live        |        |      |             |       |
| Beam Ball 3D model            |        |      |             |       |
| Script fader cleanup          |        |      |             |       |
| DJ Expression system          |        |      |             |       |
| Performance / diagnostics     |        |      |             |       |
| Beat timing UI                |        |      |             |       |
| Keyboard shortcuts & tooltips |        |      |             |       |
| Page-dependent input mappings |        |      |             |       |
| Speed Dial multiply mode      |        |      |             |       |
| Beat subdivision (FineFractions) |     |      |             |       |
| Song Manager — VDJ integration  |        |      |             |       |
| VDJ beat-synced show playback |        |      |             | Requires live VDJ connection |
| VCAnimation param controls    |        |      |             | Requires Script algo with Range/List props |
| Upstream 5.3.0 palette merge  |        |      |             | Verify MCP palette tools + new palette types |
| Upstream 5.3.0 3D view / sequence |    |      |             | 3D marker, lock flag, sequence step auto-select |

**Overall:** ☐ Ready to merge ☐ Blockers found (list below)

---

*Last updated: split from the full test plan — automated checks now live in `scripts/smoke-test.sh`, the `engine/test/beatquantize` + `mcp/test/*` unit tests, and `webaccess/web-dmx/e2e/` Playwright suites. This document covers only items that require human judgement.*
