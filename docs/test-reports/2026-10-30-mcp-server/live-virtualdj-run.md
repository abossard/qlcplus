# Live VirtualDJ run

[Back to the report](README.md)

The DJ set work (nested song Shows in a recorded set, multi-deck Perform, the LTP mix layer, the DJ Manager deck table) checked live against VirtualDJ on `mcp-server` at `d148b372c`.

## Summary

- Every checked behaviour passed.
- Every DJ set unit test suite passes (see the [report](README.md)).
- Findings: one new low-severity defect (F1) and three low observations (O1 to O3).
- 62 screenshots, all viewed: 39 from P7's set and a two-deck Perform mix, 23 from the set recorded live today.

## What was checked

- QLC+ built at `d148b372c`, VirtualDJ 8.5 started for the run and quit after it.
- Tracks: "Betoko - Raining Again" (deck 1), "Cassian - Dun Dun" (deck 2), "CASHEW - Gadasi" (deck 1 after the eject).
- VirtualDJ driven over OSC; QLC+ read over MCP (`get_dj_state`, `get_recording_state`) every 0.5 s.
- The mix: deck 1 plays; a 4-beat loop for 5 s; a seek of +16 beats; deck 2 starts under a closed crossfader; the crossfader sweeps 0 to 100 % in 10 % steps; deck 1 pauses, is cued +8 beats, ejects, loads song C and plays; the crossfader back to 50 %; Perform off.
- Evidence (step and sample logs, the recorded workspace, logs) stays with the raw report, not in the repo.

## Results

All passed.

| Check | Result |
|---|---|
| Perform state | Armed, Suspended when the decks load, Live on play, Idle after Perform off |
| Mix weights | Deck 2: 0, 0.2 … 1 at crossfader 10 … 50 %; deck 1: 1 up to 50 %, then 0.8 … 0 up to 100 %. Each driven Show's master intensity equals its deck's weight in every sample |
| Handover | VirtualDJ made deck 2 the active deck at 60 %; in the same sample dominance and `activeShowId` moved from Show 1 to Show 3 |
| Old deck after the handover | Show 1 kept running at 0.8 … 0 until deck 1 paused |
| Pause and eject | Pause suspended Show 1, eject removed it; song C's Show 6 was Suspended when loaded and Live on play; dominance stayed on deck 2 |
| REC | Armed while deck 2 was active: bound to Show 3; the crossfader back to 40 % made deck 1 active and bound Show 6; disarm gave none |
| Recorded set | "Set 2026-10-30 18:17" in Sets. Deck 1: Betoko 0 to 151.25 s, CASHEW from 169.93 s. Deck 2: Cassian from 41.35 s. Ends at Perform off (227.3 s) |
| Set contents | 6 loop wraps about 2 s apart, the seek, two paused segments, the crossfader's level steps, dominance flipping on both decks at 88.6 s, deck 2 at VirtualDJ's pitch (rate 0.952) |
| Replay (Perform off) | Deck 1's song starts at in-point 317 ms, deck 2's at 140 ms; stopping the set stops both; no Audio Function starts |
| Clean-up | VirtualDJ's mixer back to its starting state, both decks unloaded, VirtualDJ quit; QLC+ exited through `quit_app`; its recent-files list restored |

Compared with P7 (2026-10-29): same weights, handover, REC binding, finalized set and replay in-points. The handover came at 60 % here, 75 % in P7 (VirtualDJ decides it).

## Findings

- **F1 (low, new):** after a refused left-edge trim during playback, the clip's tooltip shows the position and duration of the drag that did not happen. The clip itself is unchanged. Most likely the same cause as the pre-existing stale tooltip after an undo (`ShowItem.qml` updates the tooltip when a drag ends).
- **O1 (low, cause open):** today's set has two moments, at 88.6 s (the handover) and 200.8 s, where both decks get a 12 to 57 ms segment, because both song clocks jumped 0.11 to 0.17 s forward at once, beyond the recorder's 40 ms tolerance. Effect: extra markers and replay jumps of that size. P7's set had none.
- **O2 (low):** after Perform off, opening the set shows the cursor at the last followed song's position, not at the set's end.
- **O3 (low):** the back button from a nested song restores the set at time scale 5, not at its previous zoom.

Pre-existing, not from the DJ set work: the stale tooltip after an undo, VDJ marker labels over the ruler, the toolbar wrapping when a nested Show is open, ruler labels crowding at fractional zooms, and below UI scale 1 the Record switch, Debug button and track chevron do not scale and the Name field shows no text.

## Not checked

- DMX output: there are no fixtures in the test workspace, so no lighting, no LTP switching at the handover, no Chaser and EFX hold during a recorded pause.
- VC command replay per dominant deck: no VC presses were recorded.
- Crossfader hamster and disable, jog hold: confirmed in P7, not repeated.
- The live QLC+ window: the screenshots are offscreen renders of the same QML UI.
- Sound: speakers not checked; the engine log shows no Audio Function starting during replay.

## Screenshots

Offscreen renders of the real QML UI from a screenshot harness. Click a thumbnail for the full-size image.

### P7's set "Set 2026-10-29 15:09", recorded live on 2026-10-29

Full 1600x900 window, software backend, no crops.

<table>
<tr><td align="center" valign="top"><a href="images/djset/p7/01-functions-songs-and-sets.png"><img src="images/djset/p7/01-functions-songs-and-sets.png" width="400" alt="01: Function Manager, folders Sets and Songs"></a><br><sub>01: Function Manager, folders Sets and Songs</sub></td><td align="center" valign="top"><a href="images/djset/p7/02-functions-folders-expanded.png"><img src="images/djset/p7/02-functions-folders-expanded.png" width="400" alt="02: the set in Sets; each song&#x27;s Show and mp3 in Songs"></a><br><sub>02: the set in Sets; each song's Show and mp3 in Songs</sub></td></tr>
</table>

<table>
<tr><td align="center" valign="top"><a href="images/djset/p7/03-set-overview.png"><img src="images/djset/p7/03-set-overview.png" width="400" alt="03: overview. Fade line, segment markers (deck 1 pause 36.7 s; deck 2 at 43.4 s and 56.0 s), darker where the deck was not the master deck"></a><br><sub>03: overview. Fade line, segment markers (deck 1 pause 36.7 s; deck 2 at 43.4 s and 56.0 s), darker where the deck was not the master deck</sub></td><td align="center" valign="top"><a href="images/djset/p7/04-set-clip-1-detail.png"><img src="images/djset/p7/04-set-clip-1-detail.png" width="400" alt="04: deck 1 zoomed. Level 1, 0.5 at the 20.6 s handover, 0 at 22.5 s"></a><br><sub>04: deck 1 zoomed. Level 1, 0.5 at the 20.6 s handover, 0 at 22.5 s</sub></td></tr>
<tr><td align="center" valign="top"><a href="images/djset/p7/05-set-clip-3-detail.png"><img src="images/djset/p7/05-set-clip-3-detail.png" width="400" alt="05: deck 2 zoomed. Fade-in 1.8 s into the clip, markers at 43.4 s and 56.0 s"></a><br><sub>05: deck 2 zoomed. Fade-in 1.8 s into the clip, markers at 43.4 s and 56.0 s</sub></td><td align="center" valign="top"><a href="images/djset/p7/06-set-zoomed-overlap.png"><img src="images/djset/p7/06-set-zoomed-overlap.png" width="400" alt="06: first 28 s. Deck 2 joins at 10.3 s, handover at 20.6 s; ruler labels crowd (pre-existing)"></a><br><sub>06: first 28 s. Deck 2 joins at 10.3 s, handover at 20.6 s; ruler labels crowd (pre-existing)</sub></td></tr>
</table>

<table>
<tr><td align="center" valign="top"><a href="images/djset/p7/07-nested-song-opened.png"><img src="images/djset/p7/07-nested-song-opened.png" width="400" alt="07: double-click opens deck 1&#x27;s song Show; the back button names the set. Pre-existing: toolbar wraps, VDJ marker labels overlap the ruler"></a><br><sub>07: double-click opens deck 1's song Show; the back button names the set. Pre-existing: toolbar wraps, VDJ marker labels overlap the ruler</sub></td><td align="center" valign="top"><a href="images/djset/p7/08-nested-back-to-set.png"><img src="images/djset/p7/08-nested-back-to-set.png" width="400" alt="08: back returns to the set"></a><br><sub>08: back returns to the set</sub></td></tr>
</table>

Left-edge trim and undo: deck 2 dragged about 2 s later (start 12.3 s, in-point 89 to 2001 ms); undo restores 10.3 s and 89 ms. The tooltip still shows the trimmed values (pre-existing).

<table>
<tr><th></th><th>Before</th><th>Trimmed</th><th>Undone</th></tr>
<tr><td valign="top">09 to 11</td><td align="center" valign="top"><a href="images/djset/p7/09-trim-before.png"><img src="images/djset/p7/09-trim-before.png" width="260" alt="09-trim-before"></a></td><td align="center" valign="top"><a href="images/djset/p7/10-trim-after.png"><img src="images/djset/p7/10-trim-after.png" width="260" alt="10-trim-after"></a></td><td align="center" valign="top"><a href="images/djset/p7/11-trim-undone.png"><img src="images/djset/p7/11-trim-undone.png" width="260" alt="11-trim-undone"></a></td></tr>
</table>

<table>
<tr><td align="center" valign="top"><a href="images/djset/p7/12-replay-5s.png"><img src="images/djset/p7/12-replay-5s.png" width="280" alt="12: replay at 5 s, STOP badge 2"></a><br><sub>12: replay at 5 s, STOP badge 2</sub></td><td align="center" valign="top"><a href="images/djset/p7/13-replay-13s.png"><img src="images/djset/p7/13-replay-13s.png" width="280" alt="13: 13 s, badge 3, deck 2&#x27;s song joined at 10.3 s"></a><br><sub>13: 13 s, badge 3, deck 2's song joined at 10.3 s</sub></td><td align="center" valign="top"><a href="images/djset/p7/14-replay-trim-refused.png"><img src="images/djset/p7/14-replay-trim-refused.png" width="280" alt="14: a trim while the set plays is refused; the clip keeps its values"></a><br><sub>14: a trim while the set plays is refused; the clip keeps its values</sub></td></tr>
<tr><td align="center" valign="top"><a href="images/djset/p7/15-replay-22s.png"><img src="images/djset/p7/15-replay-22s.png" width="280" alt="15: 22 s, the tooltip shows the refused drag (F1)"></a><br><sub>15: 22 s, the tooltip shows the refused drag (F1)</sub></td><td align="center" valign="top"><a href="images/djset/p7/16-replay-40s.png"><img src="images/djset/p7/16-replay-40s.png" width="280" alt="16: 40 s, same tooltip (F1)"></a><br><sub>16: 40 s, same tooltip (F1)</sub></td></tr>
</table>

UI scale: the name strip stays above the fade line at every scale. Pre-existing: the Record switch, Debug button and track chevron do not scale; at 0.3 the chevron covers the Deck 1 header; the Name field shows no text below scale 1.

<table>
<tr><th></th><th>Overview</th><th>Deck 2's clip zoomed</th></tr>
<tr><td valign="top">Scale 0.3</td><td align="center" valign="top"><a href="images/djset/p7/17-ui-scale-03-overview.png"><img src="images/djset/p7/17-ui-scale-03-overview.png" width="360" alt="17-ui-scale-03-overview"></a></td><td align="center" valign="top"><a href="images/djset/p7/18-ui-scale-03-clip.png"><img src="images/djset/p7/18-ui-scale-03-clip.png" width="360" alt="18-ui-scale-03-clip"></a></td></tr>
<tr><td valign="top">Scale 0.6</td><td align="center" valign="top"><a href="images/djset/p7/19-ui-scale-06-overview.png"><img src="images/djset/p7/19-ui-scale-06-overview.png" width="360" alt="19-ui-scale-06-overview"></a></td><td align="center" valign="top"><a href="images/djset/p7/20-ui-scale-06-clip.png"><img src="images/djset/p7/20-ui-scale-06-clip.png" width="360" alt="20-ui-scale-06-clip"></a></td></tr>
<tr><td valign="top">Scale 1.0</td><td align="center" valign="top"><a href="images/djset/p7/21-ui-scale-10-overview.png"><img src="images/djset/p7/21-ui-scale-10-overview.png" width="360" alt="21-ui-scale-10-overview"></a></td><td align="center" valign="top"><a href="images/djset/p7/22-ui-scale-10-clip.png"><img src="images/djset/p7/22-ui-scale-10-clip.png" width="360" alt="22-ui-scale-10-clip"></a></td></tr>
</table>

### Perform with a two-deck mix (synthetic capture)

A capture fed over the VDJ Bridge port with its songs mapped to three library tracks. The feed waits while a shot is taken, so the recorded set is longer and has extra markers at the shot moments (a harness effect).

<table>
<tr><th></th><th>Show Manager</th><th>DJ Manager</th><th>Set being recorded</th></tr>
<tr><td valign="top">Deck 1 playing. PERFORM read only at 32.5 s; VDJ Connected, PERFORM Live; deck 1 Playing 100 % <code>Show 1 ★</code>, deck 2 Loaded 0 %</td><td align="center" valign="top"><a href="images/djset/p7/23-perform-deck1-playing-showmanager.png"><img src="images/djset/p7/23-perform-deck1-playing-showmanager.png" width="240" alt="23-perform-deck1-playing-showmanager"></a></td><td align="center" valign="top"><a href="images/djset/p7/24-perform-deck1-playing-djmanager.png"><img src="images/djset/p7/24-perform-deck1-playing-djmanager.png" width="240" alt="24-perform-deck1-playing-djmanager"></a></td><td></td></tr>
<tr><td valign="top">After the loops and the seek, at 35.7 s</td><td align="center" valign="top"><a href="images/djset/p7/25-perform-after-seek-showmanager.png"><img src="images/djset/p7/25-perform-after-seek-showmanager.png" width="240" alt="25-perform-after-seek-showmanager"></a></td><td align="center" valign="top"><a href="images/djset/p7/26-perform-after-seek-djmanager.png"><img src="images/djset/p7/26-perform-after-seek-djmanager.png" width="240" alt="26-perform-after-seek-djmanager"></a></td><td></td></tr>
<tr><td valign="top">Crossfade. Both decks playing, badge 2, deck 2 at 48 %. The set being recorded is read-only, its cursor at the record head (26.8 s)</td><td align="center" valign="top"><a href="images/djset/p7/27-perform-crossfade-overlap-showmanager.png"><img src="images/djset/p7/27-perform-crossfade-overlap-showmanager.png" width="240" alt="27-perform-crossfade-overlap-showmanager"></a></td><td align="center" valign="top"><a href="images/djset/p7/28-perform-crossfade-overlap-djmanager.png"><img src="images/djset/p7/28-perform-crossfade-overlap-djmanager.png" width="240" alt="28-perform-crossfade-overlap-djmanager"></a></td><td align="center" valign="top"><a href="images/djset/p7/29-perform-crossfade-overlap-recording-set.png"><img src="images/djset/p7/29-perform-crossfade-overlap-recording-set.png" width="240" alt="29-perform-crossfade-overlap-recording-set"></a></td></tr>
<tr><td valign="top">Deck 2 active. Deck 1 at 0 % (Show 1 still running), deck 2 at 100 % <code>Show 3 ★</code>; head at 34.1 s</td><td align="center" valign="top"><a href="images/djset/p7/30-perform-deck2-active-showmanager.png"><img src="images/djset/p7/30-perform-deck2-active-showmanager.png" width="240" alt="30-perform-deck2-active-showmanager"></a></td><td align="center" valign="top"><a href="images/djset/p7/31-perform-deck2-active-djmanager.png"><img src="images/djset/p7/31-perform-deck2-active-djmanager.png" width="240" alt="31-perform-deck2-active-djmanager"></a></td><td align="center" valign="top"><a href="images/djset/p7/32-perform-deck2-active-recording-set.png"><img src="images/djset/p7/32-perform-deck2-active-recording-set.png" width="240" alt="32-perform-deck2-active-recording-set"></a></td></tr>
<tr><td valign="top">Deck 1 paused. <code>Show 1</code> suspended; deck 2 keeps the ★</td><td align="center" valign="top"><a href="images/djset/p7/33-perform-deck1-paused-showmanager.png"><img src="images/djset/p7/33-perform-deck1-paused-showmanager.png" width="240" alt="33-perform-deck1-paused-showmanager"></a></td><td align="center" valign="top"><a href="images/djset/p7/34-perform-deck1-paused-djmanager.png"><img src="images/djset/p7/34-perform-deck1-paused-djmanager.png" width="240" alt="34-perform-deck1-paused-djmanager"></a></td><td></td></tr>
<tr><td valign="top">Song C. Deck 1 plays Gadasi at 50 %, <code>Show 6</code>; the CASHEW clip on Deck 1 after a gap, head at 46.6 s</td><td align="center" valign="top"><a href="images/djset/p7/35-perform-song-c-showmanager.png"><img src="images/djset/p7/35-perform-song-c-showmanager.png" width="240" alt="35-perform-song-c-showmanager"></a></td><td align="center" valign="top"><a href="images/djset/p7/36-perform-song-c-djmanager.png"><img src="images/djset/p7/36-perform-song-c-djmanager.png" width="240" alt="36-perform-song-c-djmanager"></a></td><td align="center" valign="top"><a href="images/djset/p7/37-perform-song-c-recording-set.png"><img src="images/djset/p7/37-perform-song-c-recording-set.png" width="240" alt="37-perform-song-c-recording-set"></a></td></tr>
<tr><td valign="top">Perform off. The set is editable with 3 clips; the cursor shows 23.9 s, the last followed song's position, not the set's end (O2). The Perform column shows dashes</td><td align="center" valign="top"><a href="images/djset/p7/38-set-after-perform-off.png"><img src="images/djset/p7/38-set-after-perform-off.png" width="240" alt="38-set-after-perform-off"></a></td><td align="center" valign="top"><a href="images/djset/p7/39-djmanager-perform-off.png"><img src="images/djset/p7/39-djmanager-perform-off.png" width="240" alt="39-djmanager-perform-off"></a></td><td></td></tr>
</table>

### The set recorded live today, "Set 2026-10-30 18:17"

The set from the live VirtualDJ run, rendered at overview zoom 12 (the whole 227 s) and replayed at 5, 20, 45 and 95 s.

<table>
<tr><td align="center" valign="top"><a href="images/djset/live/01-functions-songs-and-sets.png"><img src="images/djset/live/01-functions-songs-and-sets.png" width="400" alt="01: Function Manager"></a><br><sub>01: Function Manager</sub></td><td align="center" valign="top"><a href="images/djset/live/02-functions-folders-expanded.png"><img src="images/djset/live/02-functions-folders-expanded.png" width="400" alt="02: the set, plus three songs, each with a Show and its mp3"></a><br><sub>02: the set, plus three songs, each with a Show and its mp3</sub></td></tr>
</table>

<table>
<tr><td align="center" valign="top"><a href="images/djset/live/03-set-overview.png"><img src="images/djset/live/03-set-overview.png" width="400" alt="03: overview. Deck 1 Betoko: six loop wraps 12 to 22 s, seek at 32 s, handover at 88.6 s, level steps to 117.8 s, pause markers at 134.6 s and 142.9 s. Deck 2 Cassian from 41.3 s. CASHEW from 169.9 s. A marker on both decks at 200.8 s (O1)"></a><br><sub>03: overview. Deck 1 Betoko: six loop wraps 12 to 22 s, seek at 32 s, handover at 88.6 s, level steps to 117.8 s, pause markers at 134.6 s and 142.9 s. Deck 2 Cassian from 41.3 s. CASHEW from 169.9 s. A marker on both decks at 200.8 s (O1)</sub></td><td align="center" valign="top"><a href="images/djset/live/07-set-zoomed-overlap.png"><img src="images/djset/live/07-set-zoomed-overlap.png" width="400" alt="07: first 28 s. The loop wraps of VirtualDJ&#x27;s real 4-beat loop, about 2 s apart"></a><br><sub>07: first 28 s. The loop wraps of VirtualDJ's real 4-beat loop, about 2 s apart</sub></td></tr>
</table>

<table>
<tr><td align="center" valign="top"><a href="images/djset/live/04-set-clip-1-detail.png"><img src="images/djset/live/04-set-clip-1-detail.png" width="280" alt="04: clip 1 zoomed"></a><br><sub>04: clip 1 zoomed</sub></td><td align="center" valign="top"><a href="images/djset/live/05-set-clip-6-detail.png"><img src="images/djset/live/05-set-clip-6-detail.png" width="280" alt="05: clip 6 zoomed"></a><br><sub>05: clip 6 zoomed</sub></td><td align="center" valign="top"><a href="images/djset/live/06-set-clip-3-detail.png"><img src="images/djset/live/06-set-clip-3-detail.png" width="280" alt="06: clip 3 zoomed"></a><br><sub>06: clip 3 zoomed</sub></td></tr>
</table>

<table>
<tr><td align="center" valign="top"><a href="images/djset/live/08-nested-song-opened.png"><img src="images/djset/live/08-nested-song-opened.png" width="400" alt="08: nested song opened"></a><br><sub>08: nested song opened</sub></td><td align="center" valign="top"><a href="images/djset/live/09-nested-back-to-set.png"><img src="images/djset/live/09-nested-back-to-set.png" width="400" alt="09: back to the set, at time scale 5, not the overview&#x27;s 12 (O3)"></a><br><sub>09: back to the set, at time scale 5, not the overview's 12 (O3)</sub></td></tr>
</table>

Trim and undo on deck 2's clip: start 41.35 to 43.36 s, in-point 128 to 2042 ms. Boundaries, envelope and shading keep their set times. Undo restores everything; its tooltip shows the trimmed values (pre-existing).

<table>
<tr><th></th><th>Before</th><th>Trimmed</th><th>Undone</th></tr>
<tr><td valign="top">10 to 12</td><td align="center" valign="top"><a href="images/djset/live/10-trim-before.png"><img src="images/djset/live/10-trim-before.png" width="260" alt="10-trim-before"></a></td><td align="center" valign="top"><a href="images/djset/live/11-trim-after.png"><img src="images/djset/live/11-trim-after.png" width="260" alt="11-trim-after"></a></td><td align="center" valign="top"><a href="images/djset/live/12-trim-undone.png"><img src="images/djset/live/12-trim-undone.png" width="260" alt="12-trim-undone"></a></td></tr>
</table>

<table>
<tr><td align="center" valign="top"><a href="images/djset/live/13-replay-5s.png"><img src="images/djset/live/13-replay-5s.png" width="280" alt="13: replay at 5 s, badge 2"></a><br><sub>13: replay at 5 s, badge 2</sub></td><td align="center" valign="top"><a href="images/djset/live/14-replay-20s.png"><img src="images/djset/live/14-replay-20s.png" width="280" alt="14: 20 s, badge 2"></a><br><sub>14: 20 s, badge 2</sub></td><td align="center" valign="top"><a href="images/djset/live/15-replay-trim-refused.png"><img src="images/djset/live/15-replay-trim-refused.png" width="280" alt="15: the trim is refused while the set plays"></a><br><sub>15: the trim is refused while the set plays</sub></td></tr>
<tr><td align="center" valign="top"><a href="images/djset/live/16-replay-45s.png"><img src="images/djset/live/16-replay-45s.png" width="280" alt="16: 45 s, badge 3, deck 2&#x27;s song joined at 41.3 s; refused drag&#x27;s tooltip (F1)"></a><br><sub>16: 45 s, badge 3, deck 2's song joined at 41.3 s; refused drag's tooltip (F1)</sub></td><td align="center" valign="top"><a href="images/djset/live/17-replay-95s.png"><img src="images/djset/live/17-replay-95s.png" width="280" alt="17: 95 s; refused drag&#x27;s tooltip (F1)"></a><br><sub>17: 95 s; refused drag's tooltip (F1)</sub></td></tr>
</table>

UI scale: same result as P7's set.

<table>
<tr><th></th><th>Overview</th><th>Clip zoomed</th></tr>
<tr><td valign="top">Scale 0.3</td><td align="center" valign="top"><a href="images/djset/live/18-ui-scale-03-overview.png"><img src="images/djset/live/18-ui-scale-03-overview.png" width="360" alt="18-ui-scale-03-overview"></a></td><td align="center" valign="top"><a href="images/djset/live/19-ui-scale-03-clip.png"><img src="images/djset/live/19-ui-scale-03-clip.png" width="360" alt="19-ui-scale-03-clip"></a></td></tr>
<tr><td valign="top">Scale 0.6</td><td align="center" valign="top"><a href="images/djset/live/20-ui-scale-06-overview.png"><img src="images/djset/live/20-ui-scale-06-overview.png" width="360" alt="20-ui-scale-06-overview"></a></td><td align="center" valign="top"><a href="images/djset/live/21-ui-scale-06-clip.png"><img src="images/djset/live/21-ui-scale-06-clip.png" width="360" alt="21-ui-scale-06-clip"></a></td></tr>
<tr><td valign="top">Scale 1.0</td><td align="center" valign="top"><a href="images/djset/live/22-ui-scale-10-overview.png"><img src="images/djset/live/22-ui-scale-10-overview.png" width="360" alt="22-ui-scale-10-overview"></a></td><td align="center" valign="top"><a href="images/djset/live/23-ui-scale-10-clip.png"><img src="images/djset/live/23-ui-scale-10-clip.png" width="360" alt="23-ui-scale-10-clip"></a></td></tr>
</table>
