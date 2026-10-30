# Review of the manual-review update

A max-effort code review of the uncommitted manual-review work on `mcp-server` at `de357b71a`, with every proven bug shown failing in a test before any fix.

## Summary

- Reviewed: the update of `MANUAL_REVIEW.md` (sections 4.9 to 4.11, 11.6, 12.x, 45 to 47, the evidence appendix), the new test `C2_importFunctionVisibility` and the `parser-real.test.ts` rows for sections 45 to 47. They are committed unchanged as `5867d4fdd`, the first commit of `fix/manual-review-findings`.
- 13 finder agents (10 angles, the line scan split in four), 10 verifier agents and one gap sweep. 32 findings reported, all confirmed. A few related findings are merged in the tables below.
- 19 code bugs, 18 of them shown by a failing test on this branch. Most are in the workspace import (`qmlui/importmanager.cpp`), the code the new C2 test drives. All predate the reviewed change. The VDJ Bridge, clip tooltip and canvas parser bugs are in fork code; the others come from upstream.
- 15 defects in the reviewed document and tests. The structural ones fail in `parser-real.test.ts`; the others are shown by code and captured logs.
- Nothing is fixed in this commit. The fixes follow in later commits on the same branch.

## Baseline

Release build of `5867d4fdd`, offscreen, each suite run from its own folder:

| Suite | Passed | Failed | Skipped |
|---|---|---|---|
| `upstreamintegration_test` | 668 | 15 | 8 |
| `vdjtelemetryclient_test` | 115 | 0 | 0 |
| `showrunner_test` | 319 | 0 | 0 |
| `show_test` | 49 | 0 | 0 |
| `upstream3d_test` | 53 | 0 | 0 |

The 15 failures are the known `timelineViewport` rows.

## Code bugs shown by a failing test

Each red test passes its control rows and fails the rows that hit the bug. "Signal 11" means the test process crashed.

| # | Bug | Where | Red test | Red result |
|---|---|---|---|---|
| 1 | The ImportManager destructor calls `clear()` on trees that are only built when the import popup reads them. A refused import file (a `.qxf`, broken or missing file) followed by another import or a cancel crashes QLC+. A failing `QVERIFY` early in the C2 test crashes the whole test binary the same way. | `qmlui/importmanager.cpp:87` | `importRefusedFile` | 3 of 3 rows: signal 11 |
| 2 | Ticking a folder in the import Functions tree returns before the update guard is reset: none of its Functions is selected and every later tick is ignored. | `qmlui/importmanager.cpp:886` | `importFunctionFolders` | 3 rows: guard stuck |
| 3 | After a tree rebuild (search, clear) Functions inside folders show unticked although they stay selected: the check pass skips folder nodes before recursing. | `qmlui/importmanager.cpp:850` | `importFunctionFolders` | 2 rows: wrong tick |
| 4 | Every search compares the lowercased name with the filter as typed, so an uppercase letter empties the list: import Functions and fixtures, Function Manager, Fixture Manager, Palette Manager. | `qmlui/importmanager.cpp:810` and the managers | `searchIgnoresCase` | 10 of 15 rows (the lowercase rows pass) |
| 5 | The import fixture tree is rebuilt after a search with other flags than when it opened: multi-head fixtures and linked copies disappear. | `qmlui/importmanager.cpp:760` | `importFixtureTreeSearch` | 2 of 2 rows: 7 rows become 2 |
| 6 | Imported EFX heads are remapped through the Function ID map, so the EFX drives the wrong fixture. | `qmlui/importmanager.cpp:581` | `importEfxHeads` | 2 of 3 rows: head 0 or 3, expected 1 |
| 7 | A palette matched by name is written into the fixture ID map: the imported Scene loses the palette and a fixture mapping can be overwritten. | `qmlui/importmanager.cpp:403` | `importScenePalettes` | 1 of 2 rows: no palette |
| 8 | An imported Sequence keeps the source Scene ID in its steps. Playing it uses whatever target Function has that ID; a Chaser there crashes the runner. `f3acd8394` made the hidden bound Scene reachable in the tree. | `qmlui/importmanager.cpp:569` | `importSequenceSteps` | 1 of 2 rows: step 0, bound Scene 1 |
| 9 | An imported Show brings none of its clip Functions, and `Show::copyFrom` looks up the clips in the target Doc: it copies an unrelated target Function or drops the clip. | `engine/src/show.cpp:240`, `qmlui/importmanager.cpp:994` | `importShowClips` | 2 of 2 rows: clip Function not imported |
| 10 | `Script::functionList()` returns pairs of Function ID and line number; the import reads every number as a Function ID. A Script selects unrelated Functions, or a missing ID that crashes Apply. | `qmlui/importmanager.cpp:989`, `:452` | `importStaleReferences` | 2 rows: 3 selected, expected 2 |
| 11 | A Scene of a deleted fixture group crashes the dialog when ticked (no null check). | `qmlui/importmanager.cpp:1007` | `importStaleReferences` | signal 11 |
| 12 | A Scene of a deleted palette, or a Script of a deleted fixture, crashes Apply: dependency IDs are never checked against the source workspace. | `qmlui/importmanager.cpp:389`, `:234` | `importStaleReferences` | 2 rows: signal 11 |
| 13 | The VDJ Bridge only rebinds when the port number changes: once a busy port is freed, confirming the same port keeps the input deaf. | `plugins/vdjbridge/vdjbridgeplugin.cpp:206` | `theSamePortBindsOnceItIsFree` | 1 of 2 rows: Idle, expected Advertising |
| 14 | A Show applies its i-th track intensity to its i-th track in ID order. Moving a track, undoing a track deletion or renaming and deleting an earlier track makes a VC slider dim another track. | `engine/src/show.cpp:1243` | `trackIntensityFollowsItsTrack` | 4 of 5 rows |
| 15 | The 2D preview's universe filter stops at an item whose fixture is gone (`return` where the other checks `continue`); the 3D view has the same line. MCP `delete_fixtures` leaves such items. | `qmlui/mainview2d.cpp:93`, `qmlui/mainview3d.cpp:362` | `previewUniverseFilter` | 2 of 3 rows |
| 16 | A Click & Go preset slider stops at the first level channel its fixture no longer has instead of trying the next one. | `qmlui/virtualconsole/vcslider.cpp:1234` | `vcSliderClickAndGoPreset` | 1 of 2 rows: no colour |
| 17 | After a left-edge trim refused during playback the clip snaps back but its tooltip keeps the refused position. | `qmlui/qml/showmanager/ShowItem.qml:919` | `nestedClipTrim(a playing show refuses)` | tooltip `00:04.282 / 00:07.717`, expected `00:04 / 00:08` |
| 18 | The manual-review canvas parser only knows numeric headings; with "13" now in front of "13b" it drops all 16 checks of 13b. | `.github/extensions/manual-review-canvas/parser.mjs:47` | vitest `the review canvas parses section 13b too` | fails |

## Code bug shown by code trace only

| # | Bug | Where |
|---|---|---|
| 19 | A VC slider in Adjust mode on a Show track (case 4.11): a Show has no Intensity attribute, so its first track is attribute 0, which the slider treats as Intensity: at 0 it stops the whole Show and restarts it from the start. The other tracks are a 0 to 1 range on an integer fader, so they are only off or full. | `qmlui/virtualconsole/vcslider.cpp:1331`, `:1859`, `:1467` |

The fix changes how the slider handles Show attributes in an upstream class. It is left for a separate change.

## Defects in the reviewed document and tests

| # | Defect | Where | Proof |
|---|---|---|---|
| D1 | The unit-test `&&` command stops at `mixlayer_test`, which finds its fixtures relative to its own folder, so the 22 suites after it never run. In a default build it also stops at `mcp_live_control_tools_test`, which only exists with `-DMCP_AUTHORING_OWNER_TESTS=ON`. | `MANUAL_REVIEW.md:5` | Run as documented: `initTestCase` fails at `mixlayer_test.cpp:56`, exit 1. `mcp/test/CMakeLists.txt:622` defaults the option to OFF |
| D2 | 47.3 is tagged `[DMX]` only and 47.4 and 47.5 have no tag, but all three need the set recorded with VirtualDJ in 47.2. | `MANUAL_REVIEW.md:2995` | vitest row 47 |
| D3 | Sections 45 to 47 have nine per-case Reviewer tables instead of a Sign-off case, so the runner cannot record their sign-off. The new note says sections 20 to 47 all have sign-off tables; section 34 has none. | `MANUAL_REVIEW.md:2927`, `:1136` | vitest rows 45, 46, 47 |
| D4 | 12.3 still nests its checks, which neither parser reads: the runners show only an automatic "verified" box. | `MANUAL_REVIEW.md:663` | vitest `gives every VDJ playback case checks of its own` |
| D5 | Stale log lines: PerformFsm logs `Idle -> 5@1 Live*` since `7af430525`, and the adopt line reads `adopted show "<name>" on deck <n>` and appears when the deck loads, not at Play. | `MANUAL_REVIEW.md:684`, `:675` | `qmlui/performfsm.cpp:215`, `qmlui/vdjbridge.cpp:445`, `vdjbridge_test` log of the 2026-10-30 run |
| D6 | A paused or stopped deck pauses its Show (still lit, orange), it does not stop it. | `MANUAL_REVIEW.md:767`, `:2974` | `qmlui/performfsm.cpp:46-50`, `:149-151` |
| D7 | "Ended" never appears for a song Show while its Audio clip spans the whole song: the clip counts towards the Show's end. | `MANUAL_REVIEW.md:683` | `qmlui/showfactory.cpp:165-171`, `engine/src/showrunner.cpp:95-112` |
| D8 | 12.9 reads the plugin's input info, which no qlcplus5 view shows. | `MANUAL_REVIEW.md:793` | Only `vcoutlinemodel.cpp:875` reads it, as search text |
| D9 | Section 47: the Show Manager leaves the opened set at every master-deck change; catch-up segments (open issue O1) add markers nobody caused; the Perform column legend is wrong. | `MANUAL_REVIEW.md:2983`, `:2986`, `:3035` | `qmlui/performfsm.cpp:167-169`, `DjManager.qml:312-319` |
| D10 | 4.10 asks for File > Exit, which the v5 menu lacks; 4.11 cites `show_test`, but the coverage is in `showrunner_test`; 4.9's message bar only appears when the VC change comes within 30 s of the first change. | `MANUAL_REVIEW.md:216`, `:222`, `:186` | `ActionsMenu.qml:447-531`, `qmlui/app.cpp:1795` |
| D11 | Suite counts disagree: `djmanager_test` is "35 passing rows" in section 11 and 41 in section 47; the run says 41. | `MANUAL_REVIEW.md:559` | 2026-10-30 log: `Totals: 41 passed` |
| D12 | The appendix move leaves sections 40 and 41 with intro claims their correction notes contradict; six "remain an exact prefix" SHA-256 claims are false for the current file; section 44 has two evidence pointers. | `MANUAL_REVIEW.md:2706`, `:3218`, `:2911` | `head -c N MANUAL_REVIEW.md \| shasum -a 256` differs for all six |
| D13 | New text breaks the user-level writing rules: em dashes in four Known Issues labels, compliance narration in the new appendix entries ("Nothing was pushed" is now false). Several new checks repeat others (12.5 and 12.4b, 12.4 and 11.3) or re-check passing tests (47.4). | `MANUAL_REVIEW.md:845`, `:3191`, `:724` | `~/.claude/CLAUDE.md` rules, `nestedClipTrim` rows |
| D14 | `C2_importFunctionVisibility` seeds the selection in the private list and compares the ticks with that list after the rebuild, so a rebuild that drops the selection still passes. | `upstreamintegration_test.cpp:4737` | A model of the ten rows with `m_functionIDList.clear()` in the rebuild passes all ten |
| D15 | The `parser-real.test.ts` table titled "upstream $commit chaser and plugin review cases" now also holds sections 45 to 47. | `parser-real.test.ts:33` | Renamed in this commit |

## Refuted or left as is

- Refuted: the `parser-real` table is not skipped when the document is missing (the file is tracked and older rows behave the same); the C2 rows' setup cost (13 ms for all ten).
- Plausible, left as is: C2 has no "hidden last and matching" row and copies the production filter as its oracle; 12.7 and 47.1 overlap; the seek rule is written out in four places; 4.9, 4.10, 11.6 and 12.9 are partly covered by passing tests.

## Fix plan

1. Delete the trees without calling `clear()` on a null pointer.
2. Folder ticks select and unselect every Function below the folder; the guard is always reset.
3. The check pass recurses into folder nodes.
4. Searches compare with `Qt::CaseInsensitive`.
5. One flag set for both fixture tree builds.
6. EFX heads through the fixture map, matched palettes into the palette map.
7. Sequence steps take the new bound Scene ID and remapped fixture IDs.
8. A Show copied into another Doc keeps its clip IDs; the import collects and remaps them.
9. Script dependencies are read as pairs; IDs missing in the source workspace are skipped.
10. The VDJ Bridge listens again when a busy port is confirmed.
11. A Show maps each track intensity to the track that registered the attribute.
12. `continue` in both universe filters and in the Click & Go resource lookup.
13. The clip tooltip is refreshed after a refused trim.
14. The canvas parser accepts alphanumeric section numbers.
15. Document fixes D1 to D13; D14 seeds the selection through the tree and compares with the seeded ID.

## Fix results

The next two commits fix bugs 1 to 18 and D1 to D15. Bug 19 stays open: section 13 lists it and 4.11 points to it.

| Suite | Before | After |
|---|---|---|
| `upstreamintegration_test` | 668 passed, 15 failed, 8 skipped | 713 passed, the same 15 `timelineViewport` failures, 8 skipped (45 new rows) |
| `showrunner_test` | 319 | 324 |
| `vdjtelemetryclient_test` | 115 | 117 |
| `show_test`, `upstream3d_test` | 49, 53 | 49, 53 |
| `vdjbridge_test`, `djmanager_test`, `djsetrecorder_test`, `vceditor_test`, `fixturecontrols_test`, `mcp_show_tools_test` | 95, 41, 80, 164, 5257, 72 (2026-10-30 run) | the same, 0 failed |
| `tools/manual-review` vitest | 59 passed, 5 failed | 64 passed |

- Every red row above passes after the fix. Each failed for the bug it names, except the canvas test, which failed in the red commit because vitest cannot load a module outside its package. It now runs the canvas parser in its own `node` process and fails with "expected 0 to be greater than or equal to 4" against the unfixed parser.
- The C2 test now ticks the row through the tree, as the dialog does, and compares the tick and the selection with that row.
- Left as is: an imported Show's recorded VC commands keep the source Function IDs (not tested); the 12.7 and 47.1 overlap; the seek rule in four places; the checks that tests cover only in part.

## Reproduce

```sh
cmake --build build --target upstreamintegration_test vdjtelemetryclient_test showrunner_test -j8
cd build/qmlui/test/upstreamintegration && QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software ./upstreamintegration_test importEfxHeads
cd tools/manual-review && npx vitest run src/parser-real.test.ts
```

Run crash-prone rows one at a time (`./upstreamintegration_test "importRefusedFile:missing file"`), since a crash ends the process.
