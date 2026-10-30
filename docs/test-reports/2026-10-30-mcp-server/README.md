# Test report: mcp-server d148b372c (2026-10-30)

Every test suite of the main build tree, run on the fork's `mcp-server` branch after the 2026-10-30 merges, plus a live run against VirtualDJ.

## Summary

- **No failure comes from the 2026-10-30 merges.** Every failing row also fails in an older build or on an earlier commit (see "Failures explained").
- Tested: 133 suites (unit, integration and UI tests, the fixture definitions check), then a live VirtualDJ run of the DJ set work.
- Rows: 16,503 passed, 323 failed, 14 skipped. 122 of 133 suites pass completely.
- The 323 failed rows fall into 8 groups. Most are tests made out of date by upstream or fork changes; one group includes a real code bug (the default position of frames created over MCP).
- 2 test programs do not build (`upstream3d_test`, `vcxypad_test`), both before today too.
- Live VirtualDJ run: all 10 checks passed. One new low-severity defect (F1) and three low observations (O1 to O3). [Details](live-virtualdj-run.md).
- Screenshots: 457 images from four UI test suites and the DJ set run, reviewed during the run (see "Screenshots").
- Fixes on `fix/pre-existing-test-failures`, merged into `mcp-server` as `866961aca`: 6 of 10 failure groups fixed. Not in this fix: Show Manager timeline preview wider than the view; VC outline release probes; Show command recorder; Fixture check step needs Python lxml (known before this run).

## Results by area

| Area | Suites | Clean suites | Rows passed | Rows failed | Rows skipped |
|---|---:|---:|---:|---:|---:|
| Engine | 74 | 72 | 3,915 | 167 | 5 |
| MCP server | 30 | 29 | 1,732 | 5 | 0 |
| Plugins | 5 | 4 | 1,777 | 2 | 0 |
| QML UI | 21 | 15 | 9,070 | 149 | 9 |
| Web access | 2 | 2 | 9 | 0 | 0 |
| Fixture definitions | 1 | 0 | 1,782 files | 0 | 1 step (no lxml) |
| **Total** | **133** | **122** | **16,503** | **323** | **14** |
| Live VirtualDJ run | 1 run | 1 | 10 checks | 0 | 5 not checked |

- Clean: every row passes. Rows are test cases, counting each data row of a parameterized test.
- `showcommandrecorder_test` and `vcbutton_test` are counted from a run with one process per test function: as one process the first runs past the 30 minute cap and the second aborts.
- `upstreamintegration_visual` is a second run of `upstreamintegration_test` with screenshots on.

<details>
<summary>Engine: every suite</summary>

| Suite | Result | Passed | Failed | Skipped | Seconds |
|---|---|---:|---:|---:|---:|
| `aubio_smoke_test` | pass (no test rows) | - | - | - | 1 |
| `audioanalyzer_test` | pass | 30 | 0 | 1 | 8 |
| `audiocapture_test` | pass | 62 | 0 | 0 | 50 |
| `audiochannel_test` | pass | 7 | 0 | 0 | 0 |
| `audioconsumers_test` | pass | 665 | 0 | 2 | 10 |
| `audioprofile_test` | pass | 23 | 0 | 0 | 1 |
| `beatquantize_test` | pass | 18 | 0 | 0 | 0 |
| `beattracker_test` | pass | 26 | 0 | 0 | 3 |
| `bus_test` | pass | 8 | 0 | 0 | 0 |
| `channelmodifier_test` | pass | 4 | 0 | 0 | 1 |
| `channelsgroup_test` | pass | 8 | 0 | 0 | 1 |
| `chaser_test` | pass | 21 | 0 | 0 | 1 |
| `chaserrunner_test` | pass | 35 | 0 | 0 | 0 |
| `chaserstep_test` | pass | 10 | 0 | 0 | 0 |
| `collection_test` | pass | 15 | 0 | 0 | 0 |
| `cue_test` | pass | 6 | 0 | 0 | 0 |
| `cuestack_test` | pass | 21 | 0 | 0 | 0 |
| `dmxdumpfactoryproperties_test` | pass | 3 | 0 | 0 | 0 |
| `doc_test` | pass | 33 | 0 | 0 | 1 |
| `efx_test` | pass | 52 | 0 | 1 | 0 |
| `efxfixture_test` | pass | 19 | 0 | 0 | 1 |
| `fadechannel_test` | pass | 12 | 0 | 0 | 0 |
| `fixture_test` | pass | 27 | 0 | 0 | 1 |
| `fixturegroup_test` | pass | 16 | 0 | 0 | 0 |
| `fixtureremapper_test` | pass | 11 | 0 | 0 | 1 |
| `function_test` | pass | 35 | 0 | 0 | 2 |
| `genericdmxsource_test` | pass | 3 | 0 | 0 | 0 |
| `genericfader_test` | pass | 5 | 0 | 0 | 0 |
| `grandmaster_test` | pass | 4 | 0 | 0 | 0 |
| `huematrix_test` | pass | 255 | 0 | 0 | 15 |
| `inputoutputmap_test` | 110 failed | 510 | 110 | 0 | 56 |
| `inputpatch_test` | pass | 3 | 0 | 0 | 0 |
| `keypadparser_test` | pass | 1 | 0 | 0 | 0 |
| `mastertimer_test` | pass | 45 | 0 | 0 | 10 |
| `mastertimer_unix_test` | 57 failed | 0 | 57 | 0 | 1 |
| `mixlayer_test` | pass | 30 | 0 | 0 | 0 |
| `monitorproperties_test` | pass | 10 | 0 | 0 | 0 |
| `osc_audio_source_test` | pass | 4 | 0 | 0 | 1 |
| `outputpatch_test` | pass | 3 | 0 | 0 | 1 |
| `qlccapability_test` | pass | 526 | 0 | 0 | 0 |
| `qlcchannel_test` | pass | 16 | 0 | 0 | 0 |
| `qlcclipboard_test` | pass | 5 | 0 | 0 | 0 |
| `qlcfile_test` | pass | 5 | 0 | 0 | 1 |
| `qlcfixturedef_test` | pass | 15 | 0 | 0 | 0 |
| `qlcfixturedefcache_test` | pass | 7 | 0 | 0 | 0 |
| `qlcfixturehead_test` | pass | 8 | 0 | 0 | 1 |
| `qlcfixturemode_test` | pass | 17 | 0 | 0 | 0 |
| `qlci18n_test` | pass | 3 | 0 | 0 | 1 |
| `qlcinputchannel_test` | pass | 9 | 0 | 0 | 1 |
| `qlcinputprofile_test` | pass | 15 | 0 | 0 | 0 |
| `qlcmacros_test` | pass | 4 | 0 | 0 | 0 |
| `qlcmodifierscache_test` | pass | 1 | 0 | 0 | 1 |
| `qlcpalette_test` | pass | 20 | 0 | 0 | 0 |
| `qlcphysical_test` | pass | 20 | 0 | 0 | 0 |
| `qlcpoint_test` | pass | 3 | 0 | 0 | 1 |
| `rgbalgorithm_test` | pass | 3 | 0 | 0 | 1 |
| `rgbmatrix_test` | pass | 26 | 0 | 0 | 0 |
| `rgbplain_test` | pass | 2 | 0 | 0 | 0 |
| `rgbscript_test` | pass | 37 | 0 | 0 | 19 |
| `rgbtext_test` | pass | 16 | 0 | 0 | 1 |
| `scene_test` | pass | 18 | 0 | 0 | 1 |
| `scenevalue_test` | pass | 7 | 0 | 0 | 0 |
| `script_test` | pass | 1 | 0 | 0 | 0 |
| `sequence_test` | pass | 7 | 0 | 0 | 0 |
| `show_test` | pass | 47 | 0 | 0 | 0 |
| `showclipautomation_test` | pass | 123 | 0 | 0 | 0 |
| `showcommandtrack_test` | pass | 543 | 0 | 0 | 0 |
| `showfunction_test` | pass | 3 | 0 | 0 | 0 |
| `showrunner_test` | pass | 317 | 0 | 0 | 0 |
| `timingdiagnostics_test` | pass | 18 | 0 | 0 | 1 |
| `track_test` | pass | 6 | 0 | 0 | 0 |
| `universe_test` | pass | 24 | 0 | 0 | 2 |
| `universeperf_test` | pass | 0 | 0 | 1 | 0 |
| `video_test` | pass | 3 | 0 | 0 | 1 |

</details>

<details>
<summary>MCP server: every suite</summary>

| Suite | Result | Passed | Failed | Skipped | Seconds |
|---|---|---:|---:|---:|---:|
| `gridlayout_test` | pass | 93 | 0 | 0 | 1 |
| `mcp_conversions_test` | 5 failed | 134 | 5 | 0 | 1 |
| `mcp_delete_tools_test` | pass | 21 | 0 | 0 | 1 |
| `mcp_dispatch_smoke_test` | pass | 33 | 0 | 0 | 0 |
| `mcp_function_authoring_test` | pass | 179 | 0 | 0 | 2 |
| `mcp_idempotency_test` | pass | 94 | 0 | 0 | 1 |
| `mcp_input_profile_tools_test` | pass | 21 | 0 | 0 | 2 |
| `mcp_io_tools_test` | pass | 58 | 0 | 0 | 5 |
| `mcp_live_control_tools_test` | pass | 72 | 0 | 0 | 5 |
| `mcp_live_tools_test` | pass | 22 | 0 | 0 | 3 |
| `mcp_palette_integration_test` | pass | 17 | 0 | 0 | 0 |
| `mcp_profiling_tools_test` | pass | 8 | 0 | 0 | 0 |
| `mcp_query_tools_test` | pass | 92 | 0 | 0 | 2 |
| `mcp_reflow_column_test` | pass | 12 | 0 | 0 | 0 |
| `mcp_rgb_transform_test` | pass | 13 | 0 | 0 | 0 |
| `mcp_script_tool_test` | pass | 77 | 0 | 0 | 1 |
| `mcp_show_tools_test` | pass | 70 | 0 | 0 | 1 |
| `mcp_stage_tools_test` | pass | 34 | 0 | 0 | 0 |
| `mcp_uimanager_preset_test` | pass | 4 | 0 | 0 | 0 |
| `mcp_vc_authoring_apply_tools_test` | pass | 45 | 0 | 0 | 0 |
| `mcp_vc_authoring_input_tools_test` | pass | 75 | 0 | 0 | 0 |
| `mcp_vc_authoring_query_tools_test` | pass | 77 | 0 | 0 | 0 |
| `mcp_vc_query_filter_test` | pass | 63 | 0 | 0 | 1 |
| `mcp_vc_structs_test` | pass | 16 | 0 | 0 | 0 |
| `mcp_vc_tool_surface_test` | pass | 45 | 0 | 0 | 0 |
| `mcp_vc_validation_test` | pass | 74 | 0 | 0 | 1 |
| `mcp_vcpage_input_mode_test` | pass | 12 | 0 | 0 | 1 |
| `mcp_vcrecordpanel_test` | pass | 13 | 0 | 0 | 0 |
| `mcp_workspace_tools_test` | pass | 34 | 0 | 0 | 1 |
| `mcp_authoring_owner_test` | pass | 224 | 0 | 0 | 106 |

</details>

<details>
<summary>Plugins: every suite</summary>

| Suite | Result | Passed | Failed | Skipped | Seconds |
|---|---|---:|---:|---:|---:|
| `artnet_test` | pass | 1 | 0 | 0 | 1 |
| `ddp_partial_test` | pass | 86 | 0 | 0 | 45 |
| `enttecwing_test` | pass | 1669 | 0 | 0 | 1 |
| `midi_test` | pass | 19 | 0 | 0 | 0 |
| `velleman_test` | 2 failed | 2 | 2 | 0 | 1 |

</details>

<details>
<summary>QML UI: every suite</summary>

| Suite | Result | Passed | Failed | Skipped | Seconds |
|---|---|---:|---:|---:|---:|
| `audiobpmtag_test` | pass | 14 | 0 | 0 | 1 |
| `djfsm_test` | pass | 138 | 0 | 0 | 0 |
| `djmanager_test` | pass | 39 | 0 | 0 | 1 |
| `djsetrecorder_test` | pass | 78 | 0 | 0 | 13 |
| `fixturecontrols_test` | pass | 5255 | 0 | 1 | 3 |
| `matrixscriptfloatcontrols_test` | pass | 26 | 0 | 0 | 1 |
| `pageplanner_test` | pass | 57 | 0 | 0 | 95 |
| `palettegenerator_v5_test` | pass | 13 | 0 | 0 | 1 |
| `performfsm_test` | pass | 83 | 0 | 0 | 0 |
| `showcommandrecorder_test` | timed out as one run; per function: 54 failed | 1915 | 54 | 0 | 1800 |
| `showfactory_test` | pass | 19 | 0 | 0 | 1 |
| `upstream3d_test` | not built | - | - | - | 0 |
| `upstreamintegration_test` | 15 failed | 666 | 15 | 8 | 125 |
| `vcaudiotriggers_test` | pass | 196 | 0 | 0 | 88 |
| `vcbutton_test` | aborted as one run; per function: 78 failed | 154 | 78 | 0 | 48 |
| `vceditor_test` | pass | 162 | 0 | 0 | 129 |
| `vcxypad_test` | not built | - | - | - | 0 |
| `vdjbridge_test` | pass | 93 | 0 | 0 | 6 |
| `vdjdatabasereader_test` | pass | 10 | 0 | 0 | 0 |
| `vdjtelemetryclient_test` | pass | 113 | 0 | 0 | 12 |
| `upstreamintegration_visual` | 2 failed | 39 | 2 | 0 | 49 |

</details>

<details>
<summary>Web access: every suite</summary>

| Suite | Result | Passed | Failed | Skipped | Seconds |
|---|---|---:|---:|---:|---:|
| `webaccessescaping_test` | pass | 4 | 0 | 0 | 0 |
| `webaccessupload_test` | pass | 5 | 0 | 0 | 0 |

</details>

<details>
<summary>Fixture definitions: every suite</summary>

| Suite | Result | Passed | Failed | Skipped | Seconds |
|---|---|---:|---:|---:|---:|
| `fixtures-xmllint` | schema check passes, validator step needs Python lxml | - | - | - | 35 |

</details>

## Failures explained

All groups are older than the 2026-10-30 merges.

### 1. Chaser steps that wait for beats

- Where: Engine, `inputoutputmap_test`, 110 rows.
- What fails: The test changes the audio tempo and expects a chaser with one-beat steps to move on, but it never sends a beat.
- Cause: Upstream reworked the chaser runner (3bd4c6c9b, 2026-10-25, taken over in the 2026-10-26 merge): a beat step now ends only on a real beat. The test was written for the older timing and is out of date.
- Fix: send beats in the test.
- Status: Fixed on `fix/pre-existing-test-failures`: none of these rows fail.

### 2. Unix master timer test never starts the timer

- Where: Engine, `mastertimer_unix_test`, 57 rows.
- What fails: The timer gets no ticks, so every compared list has the wrong size.
- Cause: Fork commit 1310b97bb moved the "running" flag from `run()` into `start()`. The test calls `run()` directly, which now returns at once. The app uses `start()`, and mastertimer_test passes 47 of 47.
- Fix: set the flag before `run()` in the test.
- Status: Fixed on `fix/pre-existing-test-failures`: none of these rows fail.

### 3. Beat fractions in the time text parser

- Where: MCP server, `mcp_conversions_test`, 5 rows.
- What fails: The rows expect the old parser: 3/5 and 1/3 beat used to be invalid, and the time text had a leading space.
- Cause: Upstream 08d33ecce (2026-10-25) accepts any denominator, snaps to sixteenths and trims the text. The fork's upstreamintegration_test already expects the new behaviour.
- Fix: update the rows.
- Status: Fixed on `fix/pre-existing-test-failures`: none of these rows fail.

### 4. Velleman plugin test on macOS

- Where: Plugins, `velleman_test`, 2 rows.
- What fails: `openClose` and `writeUniverse` read mock counters that never change.
- Cause: On macOS the mock is linked into both the plugin and the test, and each side updates its own copy. Upstream skips this test on macOS. Not a code problem.
- Fix: skip it on macOS in the test runner, as `check` does.
- Status: Fixed on `fix/pre-existing-test-failures`: none of these rows fail.

### 5. VC button tests place widgets inside a frame header

- Where: QML UI, `vcbutton_test`, 78 rows.
- What fails: Widgets created through the MCP bridge are refused, so rows fail with "buttonID >= 0" or "widget creation failed". One row then reads a missing field and aborts the whole suite, so these rows are counted from the run with one process per test function.
- Cause: Fork commit 333b511ed (2026-10-14) refuses a widget outside a frame's content area (below its header, about 28 px) or smaller than its minimum size. The tests place children at y = 0 to 10, and one label is too small. One real code bug: a frame created over MCP without x and y defaults to (5, 5), inside the parent's header.
- Fix: place test widgets below the header, size the label from its minimum, give new frames the next free position, and let the aborting row fail instead of throwing.
- Status: Fixed on `fix/pre-existing-test-failures`: none of these rows fail.

### 6. Show Manager timeline preview wider than the view

- Where: QML UI, `upstreamintegration_test`, 15 rows.
- What fails: The timeline preview is wider than the visible timeline, in all three time divisions and five play states.
- Cause: Not analysed yet. It fails since 6010206a1, after the 2026-10-22 upstream merge (not bisected).
- Status: Still failing on `fix/pre-existing-test-failures`: 15 of these rows fail.

### 7. VC outline release probes

- Where: QML UI, `upstreamintegration_visual`, 2 rows.
- What fails: Two rows that only run in the screenshot pass fail when the held drag is released.
- Cause: The held drop has target ID 0 before the signal checks run. It fails since 860c7551f and is on the open issues tracker.
- Status: Pre-existing since 860c7551f.

### 8. Show command recorder

- Where: QML UI, `showcommandrecorder_test`, 54 rows.
- What fails: As one process the suite runs past the 30 minute cap and is killed. Run with one process per test function, 54 rows in 15 functions fail.
- Cause: Not analysed yet. Most rows check that a focused control scrolls into view or that the slider graph draws where expected.
- Status: Pre-existing: the same rows fail at 38d57ee04 (the DJ campaign merge), run the same way.

### 9. Two test programs do not build

- Where: QML UI, `upstream3d_test`, `vcxypad_test`.
- What fails: `vcxypad_test` and `upstream3d_test` are not built, so they did not run.
- Cause: vcxypad_test: link error, missing `VirtualConsole` and `VCFrame` symbols. upstream3d_test: upstream 0eee616b5 (2026-10-24) made `m_sceneRootEntity` a `QPointer`, and the test still assigns it to a raw pointer (upstream3d_test.cpp:500).
- Status: Builds and runs on `fix/pre-existing-test-failures`: `upstream3d_test` 51 passed, 0 failed, `vcxypad_test` 18 passed, 0 failed.

### 10. Fixture check step needs Python lxml

- Where: Fixture definitions, `fixtures-xmllint`.
- What fails: The fixture check exits with code 1.
- Cause: Step 2 (`fixtures-tool.py --validate`) needs Python `lxml`, which no local interpreter has. Step 1 (schema check of all 1,782 fixtures) and step 3 (`FixturesMap.xml`) pass. Not a fixture problem.
- Status: Environment, not a test failure.

## Fixed on branch fix/pre-existing-test-failures

Suites rerun on `fix/pre-existing-test-failures` (merged into `mcp-server` as `866961aca`) after the fixes, against the failures of this run on `d148b372c`. Row counts leave out initTestCase and cleanupTestCase.

| Suite | Failed on d148b372c | Passed after the fixes | Failed after the fixes | Skipped after the fixes |
|---|---:|---:|---:|---:|
| `inputoutputmap_test` | 110 | 620 | 0 | 0 |
| `mastertimer_test` | 0 | 45 | 0 | 0 |
| `mastertimer_unix_test` | 57 | 57 | 0 | 0 |
| `mcp_conversions_test` | 5 | 139 | 0 | 0 |
| `pageplanner_test` | 0 | 57 | 0 | 0 |
| `upstream3d_test` | not run | 51 | 0 | 0 |
| `upstreamintegration_test` | 15 | 656 | 15 | 8 |
| `vcbutton_test` | 78 | 234 | 0 | 0 |
| `vceditor_test` | 0 | 162 | 0 | 0 |
| `vcxypad_test` | not run | 18 | 0 | 0 |
| `velleman_test` | 2 | 4 | 0 | 0 |

## Screenshots

- [Page planner](screenshots-page-planner.md): 72 images
- [VC editor](screenshots-vc-editor.md): 142 images
- [VC outline and frame chrome](screenshots-vc-outline.md): 173 images
- [Show recorder](screenshots-show-recorder.md): 8 images
- [Live VirtualDJ run](live-virtualdj-run.md): 62 images

- Page planner: 72 shots, compared pixel by pixel with the last verified run (`.session-notes/page-planner-platform/shots/merge-outline-fix/`, same commit). 66 identical. 6 differ in small regions, none a layout change:
  - `mixed group-1`, `mixed group-5`: the XY pad head marker appears now (it depends on whether the grab lands before the first DMX update).
  - `G1-wizard-gaps-3-spots`: draw order inside the known overlap of the gobo grid and the Blackout button (tracker item).
  - `run-beam open-on`, `run-beam red-on`, `run-strobe flash-on`: anti-aliasing of the status dot.
- VC editor, VC outline and show recorder: viewed on contact sheets during the run, no blank, cut or misplaced frames.
- Live VirtualDJ run: every image viewed.

## How this was run

- Commit: `mcp-server` `d148b372c` ("Merge fix/vc-outline-move-rebuilds into mcp-server"), 71 commits ahead of `origin/mcp-server`, not pushed. `upstream/master` is fully merged.
- Uncommitted in the checkout: another session's 54-line `ImportManager` edit to `upstreamintegration_test.cpp` is compiled into that binary. `MANUAL_REVIEW.md` and `tools/manual-review/src/parser-real.test.ts` are also modified but not part of any C++ test.
- Toolchain: Qt 6.12.0 (Homebrew), macOS 27.2, Release, `CMAKE_OSX_SYSROOT` pinned to Xcode's SDK, `/opt/homebrew/bin/cmake`.
- Environment: `QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software`, one suite at a time from its own folder, `-maxwarnings 0`, 30 min cap per suite, the resource and test data copy the `check` target does.

- Build: `cmake --build . -j8 -- -k`, twice, so one failing target does not stop the others.
- The live VirtualDJ run: VirtualDJ 8.5 driven over OSC, QLC+ read over MCP every 0.5 s. [Details](live-virtualdj-run.md).

## Not covered

- `fixtures-tool.py --validate` (step 2 of `resources/fixtures/scripts/check`): needs Python `lxml`, which no local interpreter has. Step 1 (xmllint schema check of all 1,782 fixtures) and step 3 (`FixturesMap.xml`) pass.
- `webaccess/web-dmx` Playwright e2e and `scripts/smoke-test.sh`: they need a running QLC+ with the web and MCP servers.
- `tools/manual-review` vitest suite (not C++, under another session's uncommitted edit).

## Merge state

All 18 worktree branches of the DJSET and page planner campaigns are ancestors of `mcp-server`:

| Branch | Tip |
|---|---|
| feat/djset | a234d2106 |
| feat/djset-assessment | 85053764c |
| feat/djset-engine-clips | 85d70aef0 |
| feat/djset-live-perform | 7af430525 |
| feat/djset-ltp-layer | 661a421fa |
| feat/djset-recorder | 98f9e52fd |
| feat/djset-show-ui | 093b9a75b |
| feat/djset-vdj-mix | 6402566e6 |
| feat/fixture-control-analyzer | 8c669f769 |
| feat/fixture-page-engine | 288463352 |
| feat/page-planner-archetypes, -groups, -sections, -strips | 8c669f769 |
| fix/djset-open-findings | a5b287938 |
| fix/pageplanner-test-rgbscripts | 4641c49d9 (merged as 887c429b9) |
| fix/vc-outline-move-rebuilds | e3aa03260 (merged as d148b372c) |
| fix/vc-page-planner-platform | 7702dbe48 |

Not merged, older and unrelated to these campaigns: `claude/update-os2l-bonjour-support`, `copilot/add-github-actions-workflow-linux-builds`, `e2e`, `mcp-v4-backport`, `mouse-wheeeeeeeeellllyy`, `stability` (April to May), `preserved/abossard-manual-review-canvas-20260908112457415`, and the agent branches `worktree-agent-a874ad0702d3c2b06` (Stage Wizard, 5 commits, 2026-08-23) and `worktree-agent-ae70214b6c38dad54` (VCXYPad test, 1 commit).
