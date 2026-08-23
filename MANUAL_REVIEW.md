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
./qmlui/qlcplus5 -d
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
>
> **Changed Aug 2026:** upstream reassigned the Left/Right pads in ten shipped input profiles to Previous/Next Page, and fixed VC button flashing across frame pages plus page-shortcut renames. Run §24.6 and §24.7 alongside this section.

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
- **Changed Aug 2026:** the mesh choice now lives in `FixtureUtils::fixtureLightResource()`, not `mainview3d.cpp` — the Stage Wizard resolves geometry through the same function. See §24.5.

## 6. Script Fader Cleanup

### 6.1 Script setFixture values clear on stop

- **Precondition:** A Script function that uses `Engine.setFixture()` to write channel values.
- **Do:**
  1. Start the script — verify fixtures respond.
  2. Stop the script (click the button again or stop from Function Manager).
- **Verify:** All channels the script was writing return to **zero** (or to whatever other functions are driving them). No residual "stuck" values.
- **Do:** Check in Simple Desk or via MCP `read_dmx_values` — the channels should not hold leftover script values.
- **Why manual:** The fix zeros the universe buffer on script stop. Previously, values persisted after stop, requiring a restart to clear.
- **Changed Aug 2026:** upstream reworked Script/ScriptRunner teardown and `stopOnExit(false)` handling. Pair this with §24.4.

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
- **HUEMatrix — existing workspaces lose audio algorithms:** The 41 HSV audio scripts moved from `resources/rgbscripts/` to `resources/huescripts/`, so pre-existing `RGBMatrix` functions that referenced them no longer resolve (56 functions across `GARAGE.qxw`, `LOADDDD.qxw`, `G2.qxw`). A warning naming both the script and the function is emitted on load. By design — no automatic migration; recreate them as `HUEMatrix` functions.
- **HUEMatrix — fork-only `<AudioProfileID>` is dropped from RGBMatrix:** `RGBMatrix` is now byte-identical to upstream, which does not know that tag, so it is discarded on load with an `Unknown RGB matrix tag` warning (5 functions in `GARAGE.qxw`). Reassign the audio profile on the recreated `HUEMatrix`.
- **HUEMatrix — "Audio Spectrum" script renamed:** The HSV script that shadowed the built-in `RGBAudio` algorithm is now "Audio Spectrum Bars", so the built-in is reachable again by name. Workspaces storing the old script name will not resolve it.
- **HUEMatrix — VC Animation widget icon:** A `HUEMatrix` can be assigned to a Virtual Console Animation widget (`HUEMatrix` IS-A `RGBMatrix`), but the widget icon is unconditionally the RGB Matrix icon. Cosmetic only.
- **HUEMatrix — editor algorithm list is not covered by tests:** `qmlui` builds an executable rather than a library and `FunctionEditor` depends on `Tardis`, so the editor cannot be constructed in a unit test. `HUEMatrixEditor::algorithms()` delegating to `HUEMatrix::availableAlgorithms()` is verified only at the cache boundary — §23.1 covers it manually.
- **HUEMatrix — shutdown drain:** Destroying a `HUEMatrix` waits up to 2 s per object for an in-flight async precompute task that never ran, then warns and continues. Many stuck matrices would add up at shutdown; not observed in practice.

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

### 12.4b Perform — FSM-driven activation, visibility, read-only [MEDIUM RISK]

> Control flow is a typed state machine (`PerformFsm`: Idle/Armed/Live/Suspended), unit-tested end to end (`performfsm_test` 12 tests, `vdjbridge_test::performAdoptsAndReleasesSyncSource`, `djmanager_test::performResolvesShowAfterWorkspaceReload` + `performShowsActiveShowInShowManager`). This verifies the visible behavior — **run both on a fresh workspace and on a saved+reopened one** (the reload case was broken before).

**Activation & status row**
- ☐ Enable **Perform** in the DJ Manager → status text appears next to the toggle: `PERFORM: Live — <show>` (green) while VDJ plays, `Suspended — <show>` (amber) when paused, `Armed — no show for active deck yet` (amber) when the song has no show
- ☐ `-d` console logs each transition: `[PerformFsm] Idle -> Live (show N)` etc., plus `adopted show` / `released show` lines
- ☐ **Reload case:** save the workspace, quit, reopen, enable Perform, play in VDJ → the show's functions actually fire on the timeline (lighting output changes), cursor follows

**Show Manager follow + read-only**
- ☐ The active deck's show loads automatically in the Show Manager; the amber **PERFORM — read only** badge shows in the toolbar
- ☐ Cursor tracks VDJ continuously; pausing VDJ freezes it; a VDJ loop jumps the cursor back each pass and re-fires the looped functions; deck switch swaps the show
- ☐ While Perform is on: items can't be dragged/resized (handles hidden), toolbar edit + play/stop buttons disabled, Space/Ctrl+Space/Ctrl+V inert, track rename/solo/mute/delete disabled, BPM/timings/insert/cut panel disabled, dragging a function from the Function Manager onto the timeline does nothing
- ☐ Still allowed read-only: navigation, zoom, selection, copy, opening other shows

**Undo is frozen during Perform** (review fix — Tardis bypassed the guards)
- ☐ Move an item, enable Perform, press Ctrl+Z and Ctrl+Shift+Z → nothing moves
- ☐ Disable Perform → Ctrl+Z now undoes the pre-Perform move as usual

**Corrupt database resilience** (review fix — grid painter had no density bound)
- ☐ Song whose database entry has an implausible Bpm value (< 12 or > 600 BPM equivalent) → VDJ Beat mode shows a plain time ruler (grid rejected), no freeze
- ☐ Zooming far out on a valid VDJ Beat show → beat lines disappear below ~2 px spacing instead of smearing/stalling the header

**Release semantics**
- ☐ Perform off → show pauses, badge disappears, everything editable again
- ☐ After Perform off, pressing Play in the Show Manager plays the show normally (autonomous clock — the adopted External sync source was restored)
- ☐ Perform off also pauses any *other* still-running external-sync show (stale-session safety net)

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

## 13b. VCAnimation — Preset Controls (upstream-restored)

> **⚠️ Changed by the June 2026 upstream merge.** The fork's previous *on-widget algorithm-parameter* UI (Range SpinBoxes / List dropdowns) was **removed** when we adopted upstream's restored **preset** model (commit `d4b099f3b`). The widget face now shows color-swatch **buttons** + RGB **knobs** + Animation/Text label buttons (`VCAnimationPreset`, `applyPreset`, `presetKnobValue`). The checks below reflect the new behaviour — there are no more algorithm-parameter mini-sliders.

**Prerequisite:** Load a workspace with an RGBMatrix function assigned to a VCAnimation widget. Configure at least one of each preset kind via the preset config popup (Color button, Color knob, Animation/Text label).

### 13b.1 Presets render on the widget face

- ☐ Configured color-swatch buttons appear (square swatches in their channel color)
- ☐ Configured RGB knobs (Color1–5 Knob) appear and are tinted to their channel color
- ☐ Animation / Text label presets appear as wider labelled buttons
- ☐ Unconfigured / empty preset list → presets area is hidden, fader + algorithm combo still shown

### 13b.2 Color button / Animation / Text presets

- ☐ Click a color preset button → the matrix color updates (verify via lighting output or Simple Desk)
- ☐ Click an Animation preset → the matrix switches to that script algorithm
- ☐ Click a Text preset → the matrix applies the configured text content
- ☐ The active preset shows a highlighted (white) border

### 13b.3 Color knob presets [DMX]

- ☐ Drag a Color knob on the widget face → the corresponding RGB channel intensity changes (verify DMX output)
- ☐ The knob value reflects the matrix's current color for that channel (re-reads when colors change)

### 13b.4 MIDI → widget face sync [MIDI]

- ☐ Assign a MIDI controller to a preset external control in the VCAnimation **properties panel**
- ☐ Move the MIDI knob → the corresponding on-widget knob/button updates in real time

### 13b.5 Preset configuration & persistence

- ☐ Open the preset config popup (PopupAnimationPreset) → add / edit / remove presets
- ☐ Save then reload the workspace → preset configuration persists (Tardis undo/redo of preset changes works)

### 13b.6 No regression

- ☐ Fader, caption label, and algorithm combo all still work correctly
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

## 16. Upstream merge — presets / dimmer / audio / stability (June 2026)

**Context:** Merged 15 upstream commits (`d4b099f3b` *restore VC animation presets*, RGBMatrix generic-dimmer + speed-phase fixes, audio `writeAudio` partial-write, threading/SIGSEGV-on-reload fixes, async side panels, Show Manager copy/paste). The conflict resolutions and behavioural changes below need a human eye.

### 16.1 VCAnimation presets swap-in [MEDIUM RISK]

We took upstream's presets and **dropped** the fork's algorithm-parameter UI. Full functional coverage is in **§13b** above — run that section.

### 16.2 RGBMatrix generic single-channel dimmer fade [DMX] [MEDIUM RISK]

Upstream fixed dimmer output for generic dimmers with no master intensity channel; resolution kept our `addEntry` refactor (`VS_GreyOrFull` if a master dimmer exists, else `VS_Grey`).

- ☐ RGBMatrix in Dimmer mode on a fixture WITHOUT a master intensity channel (single dimmer channel) → the channel **fades smoothly** through greyscale, not just 0/255 on/off blink
- ☐ RGBMatrix on a fixture WITH a master dimmer → master carries the fade, the per-head dimmer just opens fully (unchanged behaviour)

### 16.3 RGBMatrix speed phase preservation [DMX] [LOW RISK]

Upstream "preserve phase when changing RGBMatrix speed at runtime".

- ☐ While an RGBMatrix runs, change its speed → the animation continues from the same phase (no visible jump / restart)

### 16.4 Audio playback smoothness [LOW RISK]

`writeAudio` now writes only what fits in the device buffer (partial writes retried by the base renderer); we kept `qWarning` logging.

- ☐ Play an Audio function → audio is smooth, no stutter / underruns / dropouts (test Qt6 build)
- ☐ No flood of `[writeAudio] expected to write…` warnings in the `-d` console during normal playback

### 16.5 Show Manager paste badge [LOW RISK]

Paste button counter now reflects **clipboard** contents (was selection count); tooltip keeps our Ctrl+V hint and the `isEditing` paste guard.

- ☐ Copy 2 items in Show Manager → the paste button badge shows the clipboard count (2); hover shows the `Ctrl+V` tooltip
- ☐ Ctrl+V / paste button only pastes while a show is being edited

### 16.6 Reload / threading / freeze stability smoke [HIGH RISK]

Upstream included "fix threading race conditions causing SIGSEGV on file reload", "NULL checks in `Doc::saveXML`", and "make side panels asynchronous to avoid freezing".

- ☐ Open a workspace, then File → Open another, several times → no crash
- ☐ Trigger autosave / manual save during playback → no crash
- ☐ Open Fixture/Function side panels repeatedly while functions run → UI does not freeze

---

## 17. Upstream merge — EFX dimmer / XYPad ranges / VC slider catch-up (July 2026)

**Context:** Merged 12 upstream commits (EFX dimmer control + fade, XYPad per-fixture range UI restored, VC slider input catch-up fixes, VC frame page creation, video playback fixes, 3D pan/tilt speed tuning). The XYPad commit landed in the same code the fork's MCP bridge extends — that conflict resolution kept **both** upstream's QML range editor (`headsRangeInfo`/`setHeadsRange`) and the fork's MCP helpers (`setFixtureRange`/`removeHead`), so cross-surface consistency is the main thing to verify.

### 17.1 XYPad range — QML UI ↔ MCP consistency [HIGH RISK]

Both surfaces write the same `XYPadFixture` min/max/reverse fields; the QML editor scales by display mode (%, DMX, degrees), the MCP bridge writes normalized 0.0–1.0 values directly.

- ☐ In the VC XYPad properties, select a head → the restored Pan/Tilt range editor shows values; set a narrower range → the pad output respects it
- ☐ Switch display mode (% / DMX / degrees) → range values rescale sensibly; in Degrees mode with a mixed selection, the max bound is the smallest range among selected fixtures
- ☐ Set a range via MCP (widget update with `xyPadConfig` fixture ranges) → reopen the QML range editor → it shows the equivalent values (no unit mismatch)
- ☐ Set a range in the QML editor → query the widget via MCP → normalized values match
- ☐ Remove a head via MCP `removeXYPadFixture` → the QML fixture list updates; no stale rows

> **Superseded in part, Aug 2026:** upstream rewrote `vcxypad.cpp` again, adding fixture-group support and a floor/tracking mode. These checks still apply; run §24.2 straight after them.

### 17.2 EFX dimmer control [DMX] [MEDIUM RISK]

New upstream feature: EFX functions can now drive a dimmer level with fade handling.

- ☐ EFX editor shows the new dimmer control; setting it changes fixture intensity while the EFX runs
- ☐ Dimmer fades in/out smoothly on EFX start/stop (no snap to full / snap to zero)
- ☐ An EFX with dimmer control does not fight the fork's DJ Expression Intensity slider (HTP behaves — see §7.2 layer separation)

### 17.3 VC slider input catch-up [MIDI] [MEDIUM RISK]

Upstream fixed catch-up when the slider is disabled and on page change. This overlaps the fork's page-dependent input modes (§4B).

- ☐ Slider in catch-up mode: move the MIDI fader while the slider's page is inactive, switch to that page → the slider does not jump until the fader crosses the current value
- ☐ Same check with the slider disabled, then re-enabled
- ☐ Re-run §4B.2/§4B.3 (Override/Inherit) with a slider — catch-up and page input modes don't interfere

### 17.4 VC frame pages creation [LOW RISK]

- ☐ Add/remove pages on a multipage frame → widgets land on the expected page, no orphaned widgets
- ☐ Frame page switching still honours per-page external input modes (§4B) after the change

### 17.5 Video playback fixes [LOW RISK]

- ☐ Show Manager: pause then resume a show containing a Video → playback resumes at the right position, video visible
- ☐ Windowed video playback: close the window manually while playing → no crash, function state stops cleanly

### 17.6 3D preview pan/tilt speed [LOW RISK]

- ☐ Move pan/tilt (Simple Desk or XYPad) while watching the 3D preview → head reorients noticeably more reactively than before, without overshoot or jitter

---

## 18. VDJ Beat time division (July 2026)

**Context:** New `VDJ Beat` entry in the Show editor's Markers combo. Item positions stay in **milliseconds** (identical storage to Time mode — switching Time ↔ VDJ Beat never moves anything), while the beat grid and POI/cue markers are read **live from VirtualDJ's `database.xml`** (nothing is copied into the workspace). Auto-created VDJ shows default to this mode. Unit coverage: `show_test` (enum/string round-trip), `vdjdatabasereader_test` (10 tests: parsing, seconds-per-beat conversion, anchor POI, cache/mtime refresh).

**Prereq:** A VDJ-analyzed song (present in `~/Library/Application Support/VirtualDJ/database.xml` on macOS, `~/Documents/VirtualDJ/database.xml` on Windows) with a show in QLC+ (auto-created via VDJ, or set the division manually).

### 18.1 Grid accuracy [HIGH RISK]

- ☐ Open the show in Show Manager with VDJ Beat selected → beat lines appear over the timeline header, first beat NOT necessarily at 0:00 (phase anchor from VDJ)
- ☐ Zoom in on the audio waveform → beat lines land on the audible/visible transients through the WHOLE song (no drift at the end — fractional BPM honored)
- ☐ Every 4th beat from the anchor renders taller (downbeat)
- ☐ Compare a few beat positions against VDJ's own grid view → identical

### 18.2 POI markers (read-only)

- ☐ VDJ cue points appear as orange flag markers with their names in the header
- ☐ Markers cannot be moved/edited from QLC+ (display only)
- ☐ Loop/remix POIs appear too (same styling)

### 18.3 Snapping

- ☐ With grid enabled, dragging an item snaps its start to the nearest VDJ beat (anchor phase respected, not multiples of 0)
- ☐ Resizing via left/right handles snaps edges to the grid
- ☐ Snapped item start times are exact beat times in ms (check via Timings panel)

### 18.4 Database status in DJ Song View

- ☐ Open the DJ Manager view → a slim status row under the top bar shows `VDJ database: <full path>` in green (macOS: `~/Library/Application Support/VirtualDJ/database.xml`)
- ☐ Rename/move the database temporarily, reopen the DJ view → a **red banner** appears: "VirtualDJ database.xml not found — beat grids and cue markers are unavailable", with a working **Retry** link
- ☐ Restore the file, click Retry → banner turns back into the green path row
- ☐ Optional: set a custom path via QSettings key `virtualdj/databasePath` → it wins over the standard locations; an invalid override falls through to the standard locations

### 18.5 Mode behaviour

- ☐ Switching Time ↔ VDJ Beat does not move any item (same ms storage); switching to BPM 4/4 keeps the old beat-unit reinterpretation behaviour
- ☐ Song missing from database.xml → falls back gracefully: plain time ruler, `no VDJ grid data` debug line, no crash
- ☐ Save + reload → division persists as `VDJBeat` in the `.qxw`; grid reappears (re-read from database.xml)
- ☐ Re-analyze/move a cue in VDJ, save its database, reopen the show in QLC+ → updated markers appear (mtime-based cache refresh)
- ☐ Playback with Perform/external sync: cursor and functions fire at the same ms positions regardless of division mode

---

## 19. Upstream merge — controls, fixture editor, and desktop UX (July 2026)

**Context:** Merged 28 upstream commits through `187b779fd`. WebAccess escaping/upload hardening is covered by `webaccessescaping_test`, `webaccessupload_test`, and the smoke suite; fixture additions are covered by schema validation. The items below require hardware, visual inspection, or interaction with the running QML UI.

### 19.1 Fixture-group cycling shortcut [LOW RISK]

- ☐ Create at least three fixture groups, select unrelated fixtures, then press **Ctrl+Tab** repeatedly → each press clears the old selection and selects the next complete group
- ☐ Continue past the last group → selection wraps to the first group; with no groups, the shortcut is a no-op

### 19.2 Speed Dial TAP controls global BPM [MEDIUM RISK]

- ☐ Show the TAP control on a Speed Dial → the **Tap button controls the global BPM rate** option appears; hide TAP → the option is hidden
- ☐ Enable the option and tap near 120 BPM → the global BPM value settles near 120 while the Speed Dial time still follows the taps
- ☐ Disable the option and tap a different tempo → the Speed Dial changes but the global BPM does not
- ☐ Save and reload → the option persists; a TAP-only widget uses the full available width

### 19.3 MIDI encoder, XYPad feedback, and slider modes [MIDI] [MEDIUM RISK]

- ☐ Map relative encoders to XYPad pan/tilt, move the pad from the UI or a preset, then turn one encoder step → movement continues from the visible position without jumping from zero
- ☐ Move the XYPad from UI, preset, and undo/redo → controller feedback follows without feedback loops, jitter, or reversed direction
- ☐ Switch a VC Slider through Level, Adjust, Submaster, and Grand Master → monitoring/override state is cleared where unavailable and no stale active faders remain
- ☐ Assign a function to a non-Adjust slider → it switches to Adjust and controls the selected function

### 19.4 Fixture Editor aliases [MEDIUM RISK]

- ☐ Open a fixture definition with an Alias capability → the Aliases section lists it with the correct capability range and alias count
- ☐ Add, edit, and remove aliases; use **Apply to all modes** → mode/channel choices update immediately, duplicates are not created, and invalid references show a warning
- ☐ Save and reopen the definition → aliases and counts persist; deleting/renaming referenced channels or modes refreshes the list without stale rows

### 19.5 Simple Desk scroll restoration [LOW RISK]

- ☐ Scroll far across Simple Desk channels, switch to another main view, then return → the first visible channel is restored instead of jumping to the beginning
- ☐ Repeat after resizing the window and with a short channel list → restoration stays in bounds and the list remains usable

### 19.6 Off-screen window geometry recovery [LOW RISK]

- ☐ Close QLC+ on a secondary display, disconnect that display, then relaunch → the window is centered and reachable on an available display
- ☐ Relaunch with the same display layout → a valid saved position and size are preserved

### 19.7 Full color tool and animation preset creation [LOW RISK]

- ☐ With UI scaling above 100%, drag across the full color picker → the selected color remains under the pointer and the preview updates continuously
- ☐ Enter R/G/B values manually → picker preview and emitted color stay synchronized; palette editing exposes White, Amber, and UV controls
- ☐ Add a VCAnimation color preset with the full picker, drag through several colors, then close → exactly one preset is created using the final color

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
| VCAnimation presets (upstream-restored) | |      |             | §13b — knob/button presets; algorithm-param UI removed |
| Upstream 5.3.0 palette merge  |        |      |             | Verify MCP palette tools + new palette types |
| Upstream 5.3.0 3D view / sequence |    |      |             | 3D marker, lock flag, sequence step auto-select |
| June 2026 merge — dimmer / audio / paste | |   |             | §16.2–16.5 generic dimmer fade, audio smoothness, paste badge |
| June 2026 merge — reload / threading stability | | |        | §16.6 SIGSEGV-on-reload, async panels, saveXML NULL checks |
| July 2026 merge — XYPad ranges (UI ↔ MCP) |    |      |             | §17.1 conflict-resolution area — both surfaces must agree |
| July 2026 merge — EFX dimmer / slider catch-up / video | | |       | §17.2–17.6 |
| VDJ Beat time division          |        |      |             | §18 — grid accuracy, POI markers, snapping |
| July 2026 merge — controls / aliases / desktop UX | | |          | §19 — shortcuts, TAP BPM, MIDI feedback, Fixture Editor, geometry |

**Overall:** ☐ Ready to merge ☐ Blockers found (list below)

---

*Last updated: split from the full test plan — automated checks now live in `scripts/smoke-test.sh`, the `engine/test/beatquantize` + `mcp/test/*` unit tests, and `webaccess/web-dmx/e2e/` Playwright suites. This document covers only items that require human judgement.*

---

## 20. July 2026 upstream merge — QML interaction and platform review

**Context:** Automated builds, native/browser suites, fixture validation, and legacy `LightItem` XML compatibility run before this section. These checks are limited to interaction quality, visual state, language review, or unavailable target platforms.

### 20.1 Virtual Console distribution and cut/paste [MEDIUM RISK]

- ☐ Select three or more differently sized widgets and distribute horizontally, then vertically → the outermost widget boundaries do not move and every interior gap is visually equal
- ☐ Cut a frame and attempt to paste into that same frame, then repeat with an outer frame targeting one of its nested frames → no self/ancestor copy is created, the hierarchy remains intact, and no widget disappears
- ☐ Complete a valid cut/paste between unrelated frames → source widgets are removed exactly once, pasted widgets retain their children, and the clipboard count resets to zero; a later Paste does nothing

### 20.2 Function and widget drag feedback [LOW RISK]

- ☐ Drag each VC widget type from the widget list at several grab points → the reduced icon stays centered under the pointer and drops on the intended frame/location
- ☐ Drag single and multiple function rows into compatible editors/widgets → the drag preview stays centered after leaving the source row and the correct functions are added

### 20.3 Editor undo and folder-open chaser steps [MEDIUM RISK]

- ☐ Create a function, leave its editor open, then undo creation → the editor closes to the function list without a stale preview, timer activity, or crash
- ☐ In a Chaser editor, open a function folder/tree while adding a step → the embedded tree remains non-editing and one correct chaser step is added

### 20.4 3D stage reload and legacy beam origin (visual) [MEDIUM RISK]

- ☐ Load workspaces using different 3D stage types in succession → the stage mesh and selector both refresh to each loaded workspace without retaining the previous stage
- ☐ Load a legacy workspace containing monitor `<LightItem>` beam-origin metadata → fixture beams originate from the same visible offsets as after saving/reloading the canonical `<LightEmitter>` form

### 20.5 PIN Return and cue-list signals [LOW RISK]

- ☐ Enter a valid and invalid PIN by pressing **Return** in the PIN field → Return follows the same accept/validation path as the dialog button, with no double submission
- ☐ Add functions to a cue list at a chosen insertion index, then press **Enter** on a selected cue → functions are inserted at that index and only the current step plays

### 20.6 Translation review (language)

- ☐ Review the changed Catalan, Spanish, German, and Japanese strings in their corresponding QML, classic UI, Fixture Editor, and WebAccess surfaces → text is accurate, fits controls, and contains no broken placeholders or accelerators

### 20.7 Android package metadata (Android platform)

- ☐ Build/install the Android target on a clean device or emulator → package ID is `org.qlcplus.android`, version code is `1`, the launcher icon/name are correct, and install/upgrade behavior matches the intentional package reset

### 20.8 Merge sign-off

| Area | Tester | Date | Pass / Fail | Notes |
|------|--------|------|-------------|-------|
| VC distribution and cut/paste | | | | §20.1 |
| Function/widget drag and editor lifecycle | | | | §20.2–20.3 |
| 3D reload and beam origin | | | | §20.4 |
| PIN and cue-list interaction | | | | §20.5 |
| Translations | | | | §20.6 |
| Android package metadata | | | | §20.7 |

---

## 21. RGB Matrix music / visual quality

> `node tests/test_audio_scripts.js` owns type, finite/bounded HSV, dimensions,
> resize, declared-property, reset, and causal-output checks. Do not repeat
> those mechanical assertions here.

Use a familiar track with distinct low, mid, high, onset, beat, and downbeat
material. Preview on an asymmetric matrix; repeat the fixture-output item on
the intended hardware.

### 21.1 Review checks

- ☐ **Visual intent/composition:** Aurora, Plasma, Soap, Reaction-Diffusion,
  Tunnel, and Vortex form deliberate layers/fields rather than visually
  arbitrary noise; controls produce artistically useful compositions.
- ☐ **Causal musical readability:** Energy, Equalizer, Split Tower, Reactor,
  Fireworks, Puddles, and Shot make the expected band or event readable without
  needing to watch an analyzer.
- ☐ **Timing feel:** Buildup's build/drop, Glitch/Strobe hit accents, and
  Scan/Scan Multi movement feel locked at slow and fast BPM without rushing,
  dragging, or double-triggering.
- ☐ **Transitions/continuity:** Melt and Sparkle, Fire, Lava, Water, and the
  scanner family move and decay continuously; parameter changes do not create
  distracting visual jumps unless the chosen mode promises a cut/strobe.
- ☐ **Fixture output [DMX]:** palette hue, contrast, 1D extrusion, transforms,
  and physical pixel order remain legible on the target fixture; note any
  hardware-specific banding, clipping, or mapping artifact.

### 21.2 Sign-off

| Area | Tester | Date | Pass / Fail | Notes |
|------|--------|------|-------------|-------|
| RGB Matrix music / visual quality | | | | §21 — intent, causality, timing, continuity, fixture output |

---

## 22. Global beat tracker and Aubio coexistence

### 22.1 Live microphone/music BPM and silence

- ☐ Select a live microphone input, play familiar metronome-backed music at 90, 120, and 174 BPM, then compare the global BPM display with the source → each tempo settles on the audible pulse without octave doubling/halving
- ☐ Stop the source and leave the microphone at its normal noise floor → the beat indicator stops producing phantom beats; resume the source → BPM and beat indication recover without restarting capture

### 22.2 Beat-indicator timing and Aubio feature continuity

- ☐ Watch the global beat indicator against clearly audible kick transients at slow and fast tempos → flashes stay perceptually aligned, regular, and single-triggered
- ☐ While the global tracker is active, exercise an Audio Trigger profile using Aubio low/mid/high energy, onset, beat, and volume mappings → profile meters and mapped fixture output continue responding without freezes, dropouts, or missing features

### 22.3 Authoritative external BPM lock

- ☐ Lock an external VirtualDJ BPM at 128 while live audio reports a conflicting tempo → the displayed BPM remains 128 while every incoming pulse still advances the beat indicator
- ☐ Release/disconnect the external BPM lock while audio continues → the global BPM resumes following the audio source without restarting QLC+

### 22.4 Sign-off

| Area | Tester | Date | Pass / Fail | Notes |
|------|--------|------|-------------|-------|
| Live audio BPM, silence, beat timing, Aubio continuity, external lock | | | | §22.1–22.3 |
---

## 23. HUEMatrix fork and RGBMatrix upstream restore

> **Automated coverage:** `huematrix_test` (42 cases) owns the HSV `Float32Array`
> contract, the dual packed-uint contract, fork-property in-memory and XML
> round-trips, algorithm-list separation, icon-site enumeration, built-in
> reachability, bounded destructor drain, async-precompute generation checks,
> per-tick recompute for audio algorithms, and the unavailable-algorithm and
> `AudioProfileID` load warnings. `rgbmatrix_test` (9) and `rgbscript_test` (14)
> are upstream's own suites, unmodified, and prove the restore.
> `mcp_rgb_transform_test` (15) covers rotation/mirror/beat spatially.
> Do not repeat those mechanical assertions here.
>
> `engine/src/rgbmatrix.cpp` is byte-identical to `upstream/master`
> (`git diff upstream/master -- engine/src/rgbmatrix.cpp` is empty). The items
> below are the visual, hardware, and workspace-migration judgments automation
> cannot make.

### 23.1 Both matrix types are selectable and distinct [LOW RISK]

- ☐ Open the Add Function menu → both "RGB Matrix" and "HUE Matrix" are offered, with visibly different icons
- ☐ Filter the function tree by each type in turn → the two filter buttons show different icons and each lists only its own functions
- ☐ Create a HUE Matrix and open its editor → the algorithm dropdown lists the 41 audio effects **and** the upstream stock patterns
- ☐ Create an RGB Matrix and open its editor → the algorithm dropdown lists **only** upstream stock patterns, with no `Audio *` entries

### 23.2 RGBMatrix behaves as pristine upstream [MEDIUM RISK]

- ☐ Run several stock patterns (Stripes, Plasma, Gradient, Fireworks) on an RGB Matrix and compare against expected upstream behaviour → colour, motion, and step timing show no fork-specific artifacts
- ☐ Confirm the fork-only controls (rotation, mirror, beat effect, brightness, RGBW control modes) are **absent** from the RGB Matrix editor → they belong to HUE Matrix only

### 23.3 HUEMatrix audio effects on hardware [DMX] [MEDIUM RISK]

- ☐ Play familiar material through a HUE Matrix running Aurora, Equalizer, Fire, and Water → hue, saturation, and value read correctly on the physical fixture, with no banding or clipping introduced by the HSV→RGB conversion
- ☐ Exercise rotation, mirror + mirror blend, and a beat effect while audio is running → transforms compose with the audio response without tearing, stutter, or dropped frames
- ☐ Select the built-in "Audio Spectrum" (not the "Audio Spectrum Bars" script) on a HUE Matrix → it renders as the built-in algorithm

### 23.4 Existing workspace migration [HIGH RISK]

- ☐ Load a workspace that predates the fork (e.g. `GARAGE.qxw`) with the console visible → warnings name each function that lost its algorithm, and each dropped `AudioProfileID`; the app does not crash
- ☐ Recreate one affected effect as a HUE Matrix and confirm it renders as before → the migration path is workable by hand
- ☐ Save and reload the migrated workspace → the HUE Matrix keeps its algorithm, transforms, beat settings, and control mode

### 23.5 Sign-off

| Area | Tester | Date | Pass / Fail | Notes |
|------|--------|------|-------------|-------|
| Type selection and editor lists | | | | §23.1 |
| RGBMatrix upstream parity | | | | §23.2 |
| HUEMatrix audio on hardware | | | | §23.3 |
| Legacy workspace migration | | | | §23.4 |

---

## 24. August 2026 upstream merge — Stage Wizard, XYPad rework, independent servers

> **Context:** Merged 38 upstream commits (`18cf9da99..4db9faa57`). Build is clean
> under `-Werror`, all 16 `mcp/test` binaries and the 44 manual-review TS tests
> pass, and the merged `qlcplus5 --help` shows both surfaces' CLI options intact.
> The items below are the judgments automation cannot make — **plus** the places
> where a conflict resolution chose one side over the other and only a human can
> confirm the choice was right.
>
> Four of these exist *because* of a resolution, not because upstream is suspect:
> §24.2 (XYPad), §24.3 (network manager), §24.4 (Script runner) and §24.5 (Beam
> Ball mesh). Treat them as verifying **our merge**, not upstream's work.
>
> **Out of scope:** the DMX-dump-over-4-universes fix (`ui/`, v4 only — covered by
> `dmxdumpfactoryproperties_test`) and the fixture-editor "no modes" warning
> (`fixtureeditor/`, v4 only). This fork ships v5.

### 24.1 Stage Wizard vs fork-managed VC pages [HIGH RISK]

Brand new upstream feature (~10 commits, `qmlui/stagewizard/`). Reached from the **yellow wizard-hat icon** in the Fixtures & Functions right panel (visible only with `AC_FunctionEditing`). `generate()` writes fixtures, groups, Scenes, EFX, chasers, palettes, VC widgets and external-controller mappings into the same `Doc` the MCP server manipulates.

`StageWizard::pickTargetPage()` reuses **the first VC page that has no widgets**, or appends a new one. That is the collision risk: it does not know which pages `build_show_page` or the DJ Manager own.

- ☐ Open a workspace where MCP `build_show_page` has already built pages, then run the wizard → it does not overwrite a populated page; anything it appends is clearly separate from the MCP-built pages
- ☐ Run the wizard on an **empty** workspace → generated frames, solo frames, buttons, sliders and XY pads are laid out legibly, captions readable, no overlap (same bar as §2.1)
- ☐ Run the wizard twice in a row → the second run does not duplicate functions, groups or palettes on top of the first, or if it does, the duplication is obvious rather than silent
- ☐ With the wizard's generated content in place, query the VC over MCP → widget tree, types and IDs come back coherent; nothing the wizard made is invisible to the MCP surface
- ☐ Save, reload, and re-query → wizard-generated content survives the XML round-trip
- ☐ **Layering check (our merge):** the wizard overlay is a separate `Loader` (`z: 100`) sitting alongside this fork's persistent `fixAndFuncLoader` / `otherViewLoader` split — upstream had a single `mainViewLoader`. Confirm the overlay covers the whole window, sits above the persistent view, and that closing it returns to the previous context with no stale panel underneath
- **Why manual:** "laid out sensibly" and "obviously separate" are visual judgments; the collision only shows on a workspace with existing history.

### 24.2 XYPad groups + floor tracking vs the MCP XYPad surface [HIGH RISK]

Upstream rewrote `vcxypad.cpp` (~1100 lines) to add fixture-**group** support (`groupsTreeModel`, `searchFilter`) and a **floor/tracking mode** (`floorControl`, `floorPosition`, `floorSize`, `floorRangeArea`) where the pad aims at a point on the stage floor instead of driving raw pan/tilt. `vcxypadpreset` changed too. The fork's MCP bridge (`addXYPadEx`, `setXYPadPosition`, `setXYPadDisplayMode`, `addXYPadFixture`, `removeXYPadFixture`, `setXYPadPresets`) auto-merged against all of it — it compiles, which proves signatures match, **not** that semantics still line up.

This extends §17.1; run that section's checks first, then:

- ☐ Create an XY pad over MCP, then open its properties in QML → the fixture list shows what MCP added, and the new group tree does not show it as an empty/ghost group
- ☐ Add a fixture **group** to a pad in QML, then query the widget over MCP → group members are reported (or their absence is at least consistent and non-crashing), and `removeXYPadFixture` on a group member behaves predictably
- ☐ Enable floor/tracking mode on a pad, then set position over MCP `setXYPadPosition` → the pad interprets the 0.0–1.0 values in the mode it is actually in; confirm which coordinate space wins and that it is the sane one
- ☐ Set presets over MCP `setXYPadPresets` → they render on the pad face and recall correctly after the preset-struct change
- ☐ Save/reload a workspace containing both an MCP-created pad and a QML group/tracking pad → both survive the round-trip [DMX]
- **Why manual:** cross-surface coordinate-space agreement; a unit test on the structs cannot see the pad aim a real head at the wrong place.

### 24.3 Independent web + native servers, and the dropped project password [HIGH RISK]

`ServerType` is now a **bitmask** (`NativeServer = 1<<0`, `WebServer = 1<<1`) — web and native can run **at the same time**, where before they were either/or. Our `m_cliWebServer` flag was dropped in favour of upstream's `m_forcedServerTypes`, and upstream **deliberately stopped taking the session key from the project file** (it is now machine-wide config), so `setServerPassword(ioMap->networkServerPassword())` is gone from the load path.

New CLI: `-s`/`--server` and `--sa`/`--server-allow-all`.

**Since fixed:** the fork used to call `setForcedServerTypes(WebServer)` on every
launch, which broke two things now repaired in `main.cpp` — re-test both:

- ☐ Start with no flags → web server comes up on **9999** and http://localhost:9999/vc/ loads (the whole of §3 depends on this), **and `lsof -i :9998` shows nothing** — the native server must not open on a default launch
- ☐ Start with no flags, then press **Stop** on the web server in the network dialog → it actually stops and the label flips to Start. This was previously a no-op
- ☐ Enable the native server + autostart in the dialog, save, restart with no flags → the native server **comes back**. The project's setting was previously discarded on load
- ☐ Start with `--no-web` while the loaded workspace has the web bit set with autostart → web still does **not** start; the CLI wins
- ☐ Start with `-w` → web access forced on, and a workspace that requests a *different* server type does not override the CLI
- ☐ Start with `-s` → native server comes up **without** killing the web server; both reachable simultaneously
- ☐ **Behaviour change:** load a workspace that stored a server password → confirm the password is no longer applied from the project, and that this is acceptable for how you use it. If any saved workspace relied on a project-stored password, it now needs the machine-wide setting instead
- ☐ `--sa` grants every native client full access without prompting → confirm the warning is understood and that this is never left on for a venue network
- ☐ Connect two native clients concurrently → both get their own session, the access-request prompt names the right host, and disconnecting one does not drop the other or leak the socket
- **Why manual:** flag interactions and "is this new default acceptable for my rig" are judgment calls.

### 24.4 Script stopOnExit and runner teardown [MEDIUM RISK]

Upstream removed `Script::slotRunnerFinished()`, moved runner teardown into `postRun()` (`deleteLater()`), and made `write()` keep running until the function queue drains so commands issued just before the script falls off the end are not lost. The fork's `ScriptRunner(doc, this, m_data)` signature was kept — `m_script` backs the script-introspection helpers — and the now-dangling `finished()` connect was dropped.

Re-run §6.1 in full, then:

- ☐ A script that calls `stopOnExit(false)` and ends on its own → the fixtures it set **stay** set after the script's last line; the function shows as stopped without yanking the values [DMX]
- ☐ A script that issues several commands immediately before its final line → all of them take effect (the queue drains); none are lost to the runner exiting early
- ☐ Start and stop a script repeatedly (10+ cycles) → no crash on teardown, no runaway threads, no growth in the running-function count
- ☐ Scripts using the fork's introspection helpers (`m_script`-backed: function id, elapsed, override duration / fade in / fade out) still return live values
- **Why manual:** thread-teardown races surface under repetition, not in a single unit-test run.

### 24.5 Beam Ball mesh after the mesh-resolution refactor [MEDIUM RISK]

Upstream centralised 3D mesh-file selection into `FixtureUtils::fixtureLightResource()` so the Stage Wizard can resolve a fixture's geometry without a live scene. Taking upstream verbatim would have silently dropped this fork's Beam Ball case, so it was **moved into that function** rather than left in `mainview3d.cpp`.

Re-run §5.2, then:

- ☐ The Stairville Beam Ball still renders as a **ball/sphere**, not the default moving-head mesh
- ☐ Place a Beam Ball via the **Stage Wizard** (which now snaps fixtures to trusses using the same function) → it snaps against ball geometry, consistent with what the 3D view draws
- ☐ Other moving heads are unaffected → still the standard moving-head mesh
- **Why manual:** 3D rendering correctness; the two call sites must agree visually.

### 24.6 Launchpad input profile — Left/Right pads reassigned [MIDI] [MEDIUM RISK]

Upstream changed channel **106 (Left)** and **107 (Right)** in ten shipped input profiles — including `Novation-Launchpad.qxi`, `LaunchpadMK2`, `LaunchpadPro`, `LaunchPadMiniMK3`, the Akai APC family and the KORG nanoKONTROL2 — from `Type: Button` to `Previous Page` / `Next Page`.

The fork's `configure_launchpad` assigns the profile matching the connected model, so this lands automatically on anyone who runs it.

- ☐ Load an existing fork workspace that bound the Left/Right pads to a **widget** → confirm whether those bindings still fire, or are now swallowed as page navigation; if swallowed, rebind to different pads
- ☐ Run `configure_launchpad` fresh (§2.2) → Left/Right now page the Virtual Console; LED feedback still lights the expected pads
- ☐ Re-run §4B.6 (page-activation bindings are mode-independent) → prev/next-page channel types behave the same across Normal / Override / Inherit pages
- **Why manual:** needs the hardware, and the regression only shows on a workspace with pre-existing bindings.

### 24.7 VC frame pages — button flashing and shortcut rename [MEDIUM RISK]

Two upstream fixes in the code §4B extends: VC buttons no longer flash across frame pages, and renaming a page shortcut now updates its external control entry.

- ☐ A frame with several pages, buttons bound on more than one → flashing a button on page 2 does not visibly flash its counterpart on page 1 [MIDI]
- ☐ Rename a frame page shortcut → the external control mapping follows the rename; the old name is gone from the input mapping list
- ☐ The page-shortcut dropdown is wide enough to read the full names
- ☐ Re-run §4B.1–4B.3 (Normal / Override / Inherit) → no regression from either fix [MIDI]

### 24.8 Scene Editor input control and pan & tilt adjust [DMX] [MIDI] [LOW RISK]

New upstream feature: the Scene Editor gains input control and a pan/tilt adjust surface.

- ☐ Open a Scene with moving heads → the pan/tilt adjust control appears and nudges heads live, matching the 3D view [DMX]
- ☐ Bind an external input to a Scene Editor control → it responds, and does not steal input from VC widgets on the active page [MIDI]

### 24.9 3D scene reset [LOW RISK]

Third upstream pass at 3D scene reset; extends §20.4.

- ☐ Load workspaces with different stage types in succession, including one with Beam Balls → the scene fully resets each time, no fixtures or stage mesh retained from the previous workspace

### 24.10 Binary rename fallout [LOW RISK]

Upstream renamed the executable `qlcplus-qml` → **`qlcplus5`** to avoid colliding with QLC+ 4 on Linux. Every reference in this fork was swept: CI, `Makefile`, docs, the `qlcplus-dev` / `qlcplus-control` canvas extensions, the MCP python harnesses, the web-dmx e2e preambles, and §1.1 above.

- ☐ The `qlcplus-control` canvas extension's **Rebuild** and **Start** both work, and its "already running" detection finds an app you started by hand (its `pgrep` patterns now look for `qmlui/qlcplus5`)
- ☐ On Linux, install the AppImage alongside a QLC+ 4 install → both launchers appear with distinct names and neither overwrites the other's binary
- ☐ Any personal scripts, aliases, shell history or launcher entries outside this repo still pointing at `qlcplus-qml` → update them; the old binary is gone

### 24.11 Sign-off

| Area | Tester | Date | Pass / Fail | Notes |
|------|--------|------|-------------|-------|
| Stage Wizard vs MCP-owned VC pages | | | | §24.1 |
| XYPad groups / tracking ↔ MCP | | | | §24.2 |
| Independent servers + dropped project password | | | | §24.3 |
| Script stopOnExit / runner teardown | | | | §24.4 |
| Beam Ball mesh after refactor | | | | §24.5 |
| Launchpad profile page reassignment | | | | §24.6 |
| VC frame pages | | | | §24.7 |
| Scene Editor input / pan-tilt | | | | §24.8 |
| 3D scene reset | | | | §24.9 |
| Binary rename fallout | | | | §24.10 |
