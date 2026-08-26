# MCP Configuration Gap — Implementation Tracker

Closing the gap between what the QLC+ MCP server exposes and what the app can actually
configure. Work proceeds **one batch at a time**: implement → build green → tests green →
independent agent review → address findings → commit → next batch.

**Invariant: the repo must build and pass `check` at the end of every batch.**

## Evidence tiers

| Tier | Mechanism | How to run |
|---|---|---|
| **T1 unit** | Qt Test in `mcp/test/` — `ToolManager` + `register*Tools()` + assert on `Doc` (pattern: `dispatch_smoke_test.cpp`) | `cd build && cmake --build . --target check` |
| **T2 persistence** | XML round-trip: mutate → `saveXML` → fresh `Doc` → `loadXML` → re-assert (pattern: `mcp_vcpage_input_mode_test`) | same binaries as T1 |
| **T3 live** | JSON-RPC against a running app | `./scripts/smoke-test.sh` |
| **T4 human** | `MANUAL_REVIEW.md` entry — only where a machine cannot judge | manual-review canvas |

Rules: T1 mandatory for every batch. T2 mandatory when saved state changes. T3 mandatory for
every new tool (must appear in `tools/list` and dispatch). T4 only where noted.

## Status

Branch: `mcp-config-gap`, off `mcp-server` @ `03d0bcac9`.

| # | Batch | State | Tests | Review | Commit |
|---|---|---|---|---|---|
| 0 | Baseline | **done** | 16/16 MCP green | n/a | — |
| 1 | Universes add/remove + deletes | **done** | 39 new, all green | reviewed, 8 fixed | — |
| 2 | Workspace save / load / new | **done** | 30 new, all green | reviewed, 8 fixed | — |
| 3 | Stage placement + channel groups | code+tests done | 25 new, green | in review | — |
| 4 | Input profile authoring | code+tests done | 17 new, green | in review | — |
| 5 | Config that touches live output | code+tests done | 18 new, green | in review | — |
| 6 | Shows / tracks / show items | code+tests done | 23 new, green | in review | — |

---

## Batch 0 — Baseline — DONE

- [x] `cmake .. && cmake --build . -j8` → exit 0, zero errors
- [x] Test baseline captured

### `check` does not run tests

`check` → `unittests` → `unittest.sh`, which only stages resources and validates fixture XML;
on macOS it copies `platforms/linux/unittest.sh` and never executes a test binary. The
`CLAUDE.md` claim that `check` "runs all unit tests" is wrong in practice.

**The gate used here is running the test binaries directly**, matching the command line at the
top of `MANUAL_REVIEW.md`.

### Baseline result — 64 passed / 25 failed

All **16/16 MCP binaries pass** and must stay green:

```
mcp_conversions mcp_dispatch_smoke mcp_idempotency mcp_palette_integration
mcp_query_tools mcp_reflow_column mcp_rgb_transform mcp_script_tool
mcp_uimanager_preset mcp_vc_query_filter mcp_vc_structs mcp_vc_tool_surface
mcp_vc_validation mcp_vcpage_input_mode mcp_vcrecordpanel gridlayout
```

The 25 failures are **pre-existing and environmental**, not code defects — `build/resources/`
staging is incomplete (`resources/fixtures` holds no `.qxf`, so `fixtureDefCache()->loadMap()`
returns false; `rgbscripts` copies hit "Permission denied"). Signatures are all
`loadMap(dir) == false`, `plugins().size() != 0`, `dir.entryList().size() > 0`.

Pre-existing failures, unchanged by this work:

```
channelsgroup chaser chaserrunner cuestack doc efx efxfixture fadechannel
fixture fixturegroup genericfader huematrix inputoutputmap inputpatch
keypadparser mastertimer outputpatch qlcfile qlcfixturedefcache qlci18n
qlcinputprofile rgbalgorithm rgbmatrix rgbscript scene
```

**Consequence for this plan:** batches 1, 3 and 5 want engine-level fixtures that the broken
staging denies. Their tests must build fixture definitions in-process rather than lean on
`loadMap()`, the way `mcp_dispatch_smoke_test` already does.

**Rule for every later batch:** no binary green at baseline may go red, and all 16 MCP binaries
stay green.

---

## Batch 1 — Universes add/remove + deletes

**Why first:** smallest change, unblocks greenfield setup, and gives every later T3 smoke test a
create/delete cycle to restore state with.

### Surface

- `configure_universes` — auto-create universes when `universeID >= universesCount()`
- `delete_universes {ids}` — new, `kAnnotDestructive`
- `delete_fixtures {ids}` — new, `kAnnotDestructive`
- `delete_fixture_groups {ids}` — new, `kAnnotDestructive`
- `vc_delete_pages {pageIndexes}` — new, `kAnnotDestructive`

### Anchors

- `InputOutputMap::addUniverse` / `removeUniverse` — `engine/src/inputoutputmap.h:155`
- growth gap — `mcp/tools/io_tools.cpp:67`
- delete shape to mirror — `delete_functions` in `mcp/tools/function_tools.cpp`

### Evidence

**T1** — `mcp/test/io_tools_test.cpp`, `mcp/test/delete_tools_test.cpp`:

- `configureUniverses_idBeyondCount_createsUniverses` — count 1, request id 3 → count 4
- `configureUniverses_idBeyondCount_patchApplied` — `outputPatch(3)->isPatched()`
- `configureUniverses_growth_data` — `_data()` rows `{requestedId, startCount} → expectedCount`
- `deleteUniverses_withPatchedFixtures_rejected` — error JSON, count unchanged
- `deleteFixtures_removesFromDoc` — address range freed (re-patch at same address succeeds)
- `deleteFixtures_referencedByScene_scrubsSceneValues` — no orphan `SceneValue`s
- `deleteFixtures_unknownId_reportsPerItemError` — batch of 2, one bogus → 1 ok + 1 error
- `vcDeletePages_lastPage_rejected` — VC keeps >= 1 page

**T2** — save/reload after delete; assert the saved XML holds no reference to the deleted
fixture id, and universe count/patches survive.

**T3** — `scripts/smoke-test.sh`: `query_universes` → grow by 1 → re-query → delete back to
baseline; patch a throwaway fixture → delete → `query_fixtures` back to baseline. Script must be
idempotent.

### Done when

An empty workspace can be brought to a multi-universe rig with MCP calls only, and a full
create→delete cycle leaves `query_workspace_summary` identical to the starting summary.

### Result

Tool count 57 → **61**. Build green, full suite run: 66 passed / 25 failed, and the failure set is
**byte-identical to baseline** — no regressions.

Shipped:

| Change | File |
|---|---|
| `configure_universes` grows the universe list on demand | `mcp/tools/io_tools.cpp` |
| `delete_universes` | `mcp/tools/io_tools.cpp` |
| `delete_fixtures` | `mcp/tools/query_tools.cpp` |
| `delete_fixture_groups` | `mcp/tools/function_tools.cpp` |
| `vc_delete_pages` + `VCBridge::deletePage` | `mcp/tools/vc_layout_tools.cpp`, `mcp/vcbridge.h`, `mcp/vcbridgev5.{h,cpp}` |

Evidence:

- **T1** — `mcp_io_tools_test` (20 cases), `mcp_delete_tools_test` (12 cases), both green
- **T2** — `createdUniverses_surviveXmlRoundTrip` (universe count/name/passthrough survive
  `InputOutputMap` save→load) and `deleteFixtures_savedXmlHasNoOrphanReference` (no
  `FixtureVal ID="<deleted>"` in saved XML)
- **T3** — `scripts/smoke-test.sh` gained a `TOOL_COUNT_FLOOR=61` and check 2.3b asserting the four
  new tools are in `tools/list`. Not executed: needs a running app, and this session does not
  start or stop QLC+.
- Dispatch guard — two cases in `mcp_dispatch_smoke_test` covering registration and
  empty-`Doc` dispatch of every new tool

Notes worth carrying forward:

- `InputOutputMap::addUniverse(id)` fills the whole gap up to `id` by itself, so requesting
  universe 5 on a 1-universe doc creates 5, not 1. The parametrized test pins that arithmetic.
- `InputOutputMap::removeUniverse` only ever removes the **last** universe. `delete_universes`
  therefore sorts ids descending, so a batch `[2,3]` succeeds while `[1]` on a 4-universe doc is
  correctly refused.
- Fixture deletion needs no manual scrubbing: `Doc` wires `fixtureRemoved` to
  `Function::slotFixtureRemoved`, and Scene/EFX/Sequence/FixtureGroup/ChannelsGroup each clean
  themselves up. The tests assert that contract rather than reimplementing it.

---

## Batch 2 — Workspace save / load / new

### Surface

`save_workspace {path?}`, `load_workspace {path, discardUnsaved}`, `new_workspace`,
plus `path` reported by `query_workspace_summary`.

### Anchors

`App::saveWorkspace` / `loadWorkspace` (`qmlui/app.h:359`) are QML-layer — needs a
`WorkspaceBridge` alongside `VCBridge` so `mcp/` keeps no `qmlui/` include.

### Evidence

**T1** — against a fake bridge (pattern: `functionmanager_stub.cpp`):

- `saveWorkspace_noPath_usesCurrentPath`
- `saveWorkspace_unsavedNewDoc_returnsErrorNotCrash`
- `loadWorkspace_missingFile_returnsError`
- `loadWorkspace_dirtyDocWithoutDiscardFlag_rejected` — guards unsaved work
- `newWorkspace_clearsDoc`

**T2** — real I/O in its own case: save into `QTemporaryDir`, assert file exists, parses with a
`<Workspace>` root, reloads into a fresh `Doc` with matching function count.

**T3** — create a scene → save to temp path → assert `.qxw` on disk → reload the original
workspace → `query_functions` matches baseline. Must leave the app on the original workspace.

**T4** — title bar shows the new filename; no dangling unsaved-changes prompt.

### Result

Tool count 61 → **65**: `save_workspace`, `load_workspace`, `new_workspace`, `query_workspace_file`,
on a new `WorkspaceBridge` / `WorkspaceBridgeV5` pair wrapping `App`. `query_workspace_summary`
also reports `modified` now.

Evidence: `mcp_workspace_tools_test` (30 cases) plus a dispatch-smoke registration guard. The
round trip runs **through the tools** — the fake bridge does real XML read/write, so
save → new → load is asserted end to end rather than around the code under test.

### Review findings fixed

1. *(critical)* **`load_workspace` could destroy a project with no way back.** `App::loadWorkspace`
   calls `clearDocument()` *before* it discovers whether the file parses, and returns false with
   the document already empty — pointing it at a .txt file would have wiped every fixture,
   function and VC page unrecoverably. The tool now runs App's own acceptance check (readable
   regular file, XML, DTD `Workspace`) while the project is still intact, and refuses first.
2. *(major)* **`save_workspace` silently destroyed unrelated projects.** `App::saveXML` removes the
   target before renaming over it, so saving Gig-A over an existing Gig-B.qxw erased Gig-B. Now
   requires `overwrite: true` for any existing file that is not the current project.
3. *(major)* **Both destructive tools blacked out live output.** Clearing or loading stops the
   MasterTimer and resets the universes mid-cue, and a loaded project's startup function then
   fires unattended. Both now refuse while functions are running unless `interruptLiveOutput` is
   set — consistent with the fork's "no live-show actuation" boundary.
4. *(minor)* Failed saves left `Doc::workspacePath` repointed at a directory that does not exist,
   breaking relative asset paths. A missing target directory is now rejected up front.
5. *(minor)* Relative paths silently wrote into the app's working directory; now rejected with an
   explanation.
6. *(minor)* `file:` URLs were stripped for the existence check but the raw URL was handed to the
   bridge, working only because `App` happened to strip it again. The bridge now always receives a
   plain local path, and a test covers it.
7. *(nit)* `discardUnsaved` / `overwrite` / `interruptLiveOutput` type validation.
8. *(nit)* The round-trip test drove `Doc::saveXML` directly and passed with the whole tool file
   deleted; it now goes through the tools.

### Known limits, not fixed

- **`Doc::isModified()` is an imperfect dirty signal.** `FlowConsole` never calls `setModified()`
  at all, and `VirtualConsole::deletePage` does not either, so flow edits and VC page deletions
  can leave `modified: false` and slip past the unsaved guard. Worth a follow-up that marks the
  document dirty at those mutation points; Batch 1 already did this for universes.
- `App::newWorkspace` never recreates `m_videoProvider` (only `loadWorkspace` does), so Video
  functions have no provider after a reset. Pre-existing in `App`, now agent-reachable.

---

## Batch 3 — Monitor properties + channel groups

### Surface

- `set_fixture_placement {items:[{fixtureID, head, x, y, z, rotation{}, gelColor}]}`
- `query_fixture_placement`
- `create_channel_groups` / `query_channel_groups` / `delete_channel_groups`

### Anchors

- `MonitorProperties::setFixturePosition` / `setFixtureRotation` / `setFixtureGelColor` —
  `engine/src/monitorproperties.h:180`, reached via `Doc::monitorProperties()` (`doc.h:672`)
- `Doc::addChannelsGroup` / `deleteChannelsGroup` / `channelsGroups()` — `doc.h:423`

Both are pure engine — no bridge needed.

### Evidence

**T1**

- `setPlacement_roundTripsThroughQuery` — assert the `QVector3D` **and the unit contract (mm)**;
  this is the likeliest silent bug
- `setPlacement_multiHead_perHeadIsolation` — 4-head bar, set head 2 only, heads 0/1/3 unchanged
- `setPlacement_gelColor_hexParsing` — `_data()` rows `"#ff0000"` / `"red"` / `"#GGG"`
- `setPlacement_unknownFixture_error`
- `createChannelGroups_upsertsByName` — two calls, one group, second set wins, **id stable**
- `createChannelGroups_mixedFixtures_preservesOrder`
- `deleteChannelGroups_removesFromDoc`

**T2** — mandatory for both; `MonitorProperties` and channel groups are serialized into `.qxw`.

**T4** — mandatory for placement, it is inherently visual: place the GARAGE rig via MCP, open the
2D view, verify positions and gel colors.

---

## Batch 4 — Input profile authoring

### Surface

`create_input_profiles {items:[{name, manufacturer, model, type, channels:[{number, name, type, movement?}]}]}`

### Evidence

**T1**

- `createProfile_writesFileToUserFolder` — into a `QTemporaryDir`; assert `.qxi` reloads via
  `QLCInputProfile::loader()` with channels intact
- `createProfile_channelTypeStrings` — `_data()` over `Button/Slider/Knob/Encoder/bogus`
- `createProfile_thenSetInputProfile_appliesToUniverse` — `inputPatch(0)->profile()->name()`

**T3** — create → `query_input_profiles` includes it → `set_input_profile` → `query_universes`
shows it → delete.

**T4 [MIDI]** — with a controller connected, a mapped CC moves the bound widget.

---

## Batch 5 — Live control

### Surface

`run_functions {items:[{functionID, action}]}`, `set_grandmaster {value, valueMode, channelMode}`,
`set_blackout {enabled}`, `write_dmx {items:[{universe, channel, value}]}`.

All four exist today only inside `create_scripts`' `Engine.*` API
(`mcp/tools/function_tools.cpp:1271`) — implement over the same engine calls, so the script API
becomes the cross-check.

### Evidence

**T1** — needs a running `MasterTimer`; use `QTRY_VERIFY`, never a bare `QCOMPARE` (25 Hz tick is
async and a plain compare will flake):

- `runFunctions_start_setsRunning` / stop clears it
- `setGrandmaster_scalesUniverseOutput` — scene at 255, GM 128 → post-GM ~128 while pre-GM
  `read_dmx_values` still reports 255. Easiest thing to get backwards.
- `setBlackout_zeroesOutputNotValues` — output 0, values retained, off restores
- `writeDmx_thenReadDmxValues_roundTrip`

**T3** — start a function, `query_functions` shows running, stop it. Must stop what it starts.

**T4 [DMX]** — with a fixture attached, blackout on/off is visible on the lamp.

**Non-goal:** no scheduling/automation tool. Timed logic stays in the script escape hatch.

---

## Batch 6 — Shows / tracks / show items

Largest item, phased so evidence lands incrementally.

### Surface

- Phase 1: `create_shows {name, tracks:[{name, fixtureGroupID?}]}`, `query_shows`
- Phase 2: `add_show_items {showID, trackIndex, items:[{functionID, startTime, duration}]}`,
  `delete_show_items`

### Evidence

**T1 phase 1**

- `createShows_upsertsByName_tracksCreated`
- `createShows_existingName_updatesTracksNotDuplicates`

**T1 phase 2** — timeline invariants are where the bugs live:

- `addShowItems_overlappingOnSameTrack_rejected` — `_data()` rows covering abutting
  (`0-1000` then `1000-2000`, accepted), overlapping, and contained
- `addShowItems_startTimeBeyondShowDuration_extendsDuration`
- `addShowItems_sceneOnTrackBoundToDifferentGroup_error`

Follow `qmlui/test/showfactory/showfactory_test` conventions — that suite exists and is already
in the `MANUAL_REVIEW.md` unit list.

**T2** — mandatory; assert item start times to the millisecond after reload.

**T4** — Show Manager renders items at the right timeline position and playback follows them.

---

## Batches 3-6 — result

Tool count 65 → **83**. Build green; full suite 71 binaries passed / 25 failed, failure set
byte-identical to baseline.

| Batch | Tools added | File | Tests |
|---|---|---|---|
| 3 | `set_fixture_placement`, `query_fixture_placement`, `configure_stage`, `create_channel_groups`, `query_channel_groups`, `delete_channel_groups` | `stage_tools.cpp` | `mcp_stage_tools_test` (25) |
| 4 | `create_input_profiles`, `query_input_profile_channels` | `input_profile_tools.cpp` | `mcp_input_profile_tools_test` (17) |
| 5 | `set_grand_master`, `query_grand_master`, `set_blackout`, `write_dmx`, `run_functions`, `query_running_functions` | `live_tools.cpp` | `mcp_live_tools_test` (18) |
| 6 | `create_shows`, `query_shows`, `add_show_items`, `delete_show_items` | `show_tools.cpp` | `mcp_show_tools_test` (23) |

Engine facts the tests pinned, each of which contradicted an initial assumption:

- **The Grand Master is not persisted anywhere** — not in the project XML, not in QSettings.
  `set_grand_master` therefore does *not* call `setModified()`: marking the document dirty for a
  change that cannot be saved would make `load_workspace` refuse on a phantom edit. The docstring
  points at a VC slider in `grandMaster` mode for a Grand Master that is part of the saved show.
- **Blackout suppresses at the output patch**, not by zeroing the desk buffers. Pre- and post-GM
  values both stay put, which is why releasing blackout restores the look instantly. An initial
  test asserting `postGMValues() == 0` was wrong about the engine, not about the tool.
- **`Function::setPath` prefixes the type folder**, so a show created with `path: "Shows/2026"`
  reports `Show/Shows/2026`.
- `Show::TimeDivision` has no `Beats` member — beat timelines are `BPM_4_4` / `BPM_3_4` /
  `BPM_2_4`, so `create_shows` takes `tempoType: "4/4"` rather than `"beats"`.
- `ChannelsGroup::addChannel` only appends, so `create_channel_groups` rebuilds membership
  wholesale on update, reusing the group id.

## Cross-cutting, every batch

- Add a row to `mcp_dispatch_smoke_test` per new tool asserting it is registered and dispatches
  without throwing on an empty `Doc` — the guard against a tool that compiles but was never wired
  into `register*Tools()`.
- Bump the tool-count floor asserted by `scripts/smoke-test.sh`, so a dropped registration fails.
- Register new test binaries in `mcp/test/CMakeLists.txt` **and** in the unit-test command line at
  the top of `MANUAL_REVIEW.md` — that list is the fork's hand-maintained test manifest.
- Update the stale `~47 tools` figure in `CLAUDE.md`.

## Review log

### Batch 1 — independent review

11 findings. 8 fixed, 3 accepted as-is. Two were real bugs in the new code that the tests as
first written could not have caught.

**Fixed**

1. *(critical)* **Created universes were never started.** `InputOutputMap::addUniverse()` builds
   the `Universe` but never starts its writer thread — every other caller in the tree pairs it
   with `startUniverses()`, and this fork already documents the trap at
   `qmlui/stagewizard/stagewizard_vc.cpp:1238`. A universe added over MCP would have been patched,
   listed by `query_universes`, and silently emitted **zero DMX** until the project was reloaded.
   Fixed in `io_tools.cpp`; pinned by `configureUniverses_createdUniversesAreStarted`.
2. *(major)* **`delete_fixture_groups` could cause a use-after-free.** `RGBMatrix` caches its group
   as a raw pointer and only re-fetches when null (`rgbmatrix.cpp:334`), then dereferences it on
   the MasterTimer thread (`rgbmatrix.cpp:744`). Now refused for any group bound to a matrix,
   running or not — a stopped matrix keeps a dead group id that goes straight back into the saved
   file. The reply lists `boundMatrices`. Refusing beats stopping the matrix: this fork's MCP
   surface deliberately does not actuate a show.
3. *(major)* **`delete_fixtures` left empty groups behind.** `FixtureManager::deleteFixtures`
   drops groups emptied by a removal, citing #2063 — a stale empty group blocks creating a new one
   with the same name. The tool now does the same and reports `removedEmptyGroups`.
4. *(major)* **Tool description over-claimed.** qmlui's `VCSlider`/`VCXYPad` do *not* connect to
   `fixtureRemoved` (the v4 widgets do), so VC bindings keep stale fixture ids. Description now
   says so explicitly and points at `vc_query_widgets`.
5. *(minor)* **Cross-universe fixtures were invisible to the delete guard.** The check compared
   `Fixture::universe()` only, so a 24-channel fixture at universe 0 / address 500 did not protect
   universe 1. Now compares the whole footprint.
6. *(minor)* **Two tests were not load-bearing** — `deleteUniverses_wouldLeaveGap_rejected` and
   `vcDeletePages_lastRemaining_rejected` passed on the generic engine/bridge refusal even with
   the tool's own guard deleted. Both now assert the specific message.
7. *(minor)* **Universe create/delete never marked the doc modified**, so a workspace built over
   MCP could be lost without an unsaved-changes prompt. Both paths now call `setModified()`.
8. *(nit)* `delete_universes` now dedupes ids, matching its sibling `vc_delete_pages`; README test
   count corrected (252 → 330).

**Accepted, not fixed**

- **MCP deletes bypass qmlui's `FixtureManager`**, which connects only to `fixtureAdded`, so with
  the app running the Fixture Manager list and the 2D/3D previews keep ghost items, and there is
  no Tardis undo. Real, but it is an architectural gap in the MCP↔qmlui seam (there is no
  FixtureManager bridge the way `delete_functions` has one), not something to bolt onto this
  batch. Worth its own batch.
- **`VCBridgeV5::deletePage` has no direct coverage** — it is exercised only through a fake that
  reimplements `VirtualConsole::deletePage`'s semantics. Testing the real one needs a full
  `VirtualConsole`; consistent with how the rest of `vcbridgev5.cpp` is tested today.
- **`dispatchSmoke_deleteTools_emptyDoc_returnArrays` asserts shape only.** That is the job of a
  dispatch smoke test; behaviour is covered in the dedicated binaries.

## Batch 5 scope — decided

Commit `34a0aa7ad` had removed `set_grand_master` and `update_scene_from_dmx` and locked that in
with `dispatchSmoke_liveControlTools_notRegistered`, on the principle:

> The MCP surface is for setup, authoring, inspection, and bounded repair — not live-show
> actuation.

**Decision: completeness of setup and configuration wins.** Some configuration cannot be verified
without the rig responding — the Grand Master is a desk setting, and checking a patch means
lighting the lamp — so those are in, and the incidental live control that comes with them is
accepted. What stays out is show *operation*: no cue stepping, no timed playback, no fade
control. Timed logic remains the Script escape hatch.

The old regression test was replaced by `dispatchSmoke_setupToolsTouchingLiveOutput_registered`,
which asserts the setup-shaped tools are present **and** that operation-shaped ones
(`step_cuelist`, `fade_to`) are still absent, so the boundary stays enforced rather than dropped.
