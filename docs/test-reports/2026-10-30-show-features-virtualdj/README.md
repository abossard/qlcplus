# Show features with VirtualDJ

Every show feature checked live against VirtualDJ on `mcp-server` at `1839d2442`, with DMX output and Virtual Console replay. Earlier run: [live VirtualDJ run on d148b372c](../2026-10-30-mcp-server/live-virtualdj-run.md).

## Summary

- 16 of 16 live checks passed, none failed.
- New since the last run: real fixtures, so DMX output and the LTP hand-over were measured; VC presses recorded into song Shows and replayed per dominant deck; crossfader hamster and disable; a held jog; a full set replay compared sample by sample with the live run.
- Findings: one new low-severity defect in the MCP tool `read_dmx_values` (N1), and the four known low items again (F1, O1 to O3). O1 now has a pattern: it happens at active-deck changes.
- 73 screenshots, all viewed. They are offscreen renders of the real QML UI, full 1600x900 window, no crops.

## Setup

- QLC+ built from `1839d2442` (Release), started with its own scratch workspace and `--mcp-port 19696`. VirtualDJ 8.5 was started for the run, the tracks loaded from its library over OSC, and VirtualDJ quit afterwards.
- Tracks: "Betoko - Raining Again" (deck 1), "Cassian - Dun Dun" (deck 2), "CASHEW - Gadasi" (deck 1 after the eject).
- Fixtures on universe 1: three RGB washes (one per song), a shared RGB wash (every song), an RGB "Chase" head, a Stairville MH-X20 moving head ("Mover": pan, tilt, colour wheel are LTP) and two RGB "VC Lamp" heads.
- Song Shows: QLC+ built one per track on load ("Artist - Title" in Songs, the mp3 on an Audio track). Each got a Look Scene for the whole song: its own wash, the shared wash, and its own Mover position and colour. Betoko's also got a 4-step Chaser (1 s per step) on the Chase head.
- Virtual Console: buttons "Lamp A" and "Lamp B" (each toggles a white Scene on its lamp) and a level slider on Wash C's blue channel.
- VC presses recorded through REC during Perform, before the mix: Lamp A on/off/on into Betoko's Show at 8.5, 20.1 and 30.0 s; Lamp B on/off/on into Cassian's Show at 6.1, 18.2 and 24.0 s.
- During the mix, `get_dj_state`, `get_recording_state`, `read_dmx_values` and the VC button states were sampled every 0.5 s. VirtualDJ's state was read over OSC after every step.

## Results

| Check | Result |
|---|---|
| Song Shows | Built on load, one per track, with the mp3 at 0, the file's BPM and the VDJ Beat ruler |
| REC during Perform | Bound to deck 1's Show; rebound to deck 2's Show when deck 2 became active. Presses stored at song time, as listed in Setup |
| Perform states | Suspended when loaded, running on play, paused on pause, removed on eject; song C's Show Suspended when loaded, running on play; Idle after Perform off |
| Weights to HTP output | Deck 2's wash 51, 102, 153, 204, 255 at crossfader 10 … 50 % (255 × weight); deck 1's wash 204 … 0 at 60 … 100 %; the Chase head scaled the same way; the shared wash stayed at the higher of the two |
| LTP hand-over | At 60 % VirtualDJ made deck 2 active. In the same sample deck 2's Show became dominant and the Mover went from Betoko's 40/10/70 to Cassian's 200/240/120. The Mover stayed with the dominant deck at every weight of the other deck |
| VC replay per dominant deck | Lamp A followed Betoko's track: on at 8.5 s, off when the seek jumped past 20.1 s, on at 30.0 s. Lamp B stayed off while Cassian played non-dominant past 6.1 s, and came on at the hand-over: Cassian's track says on at its position (40 s) |
| Crossfader hamster | At 100 % hamster on swapped the weights (deck 1 1.0, deck 2 0); VirtualDJ moved the active deck to 1; dominance and the Mover followed. Off restored everything |
| Crossfader disable | Both weights 1.0, both washes 255. Off restored the curve |
| Held jog | VirtualDJ froze deck 2's position for the 8 s hold with play on. QLC+ kept the deck Playing and its Show running on the frozen clock. The set recorded the hold as a rate-0 segment that is not a pause |
| Deck pause | Deck 1's Show paused and the Chase head held its value for the whole 9.3 s pause. A cue while paused moved the position |
| Recorded set | "Set 2026-10-30 21:21", 266.8 s. Deck 1: Betoko 0 to 199.8 s, CASHEW from 216.5 s. Deck 2: Cassian from 56.0 s |
| Set contents | 32 segments: 6 loop wraps about 2 s apart, the seek, `Paused="1"` on both deck pauses and the cue while paused, the jog hold at rate 0.005, VirtualDJ's pitch (rate 0.95) on Cassian and CASHEW. Dominance flips at 97.2 s (hand-over), 137.2 s (hamster on) and 145.4 s (hamster off). One level step per crossfader step |
| Replay (Perform off) | 537 samples compared with the live run at the same set time: 522 equal. The 15 others are single samples: one before the start, six at a transition (sampling skew), eight torn reads (N1) |
| Replay VC and pause | The lamps changed at 8.6, 30.4, 34.0 and 97.3 s, live at 8.2, 30.2, 33.8 and 97.3 s. The Chase head held during the recorded pause |
| No audio | No Audio Function ran during the replay |
| Clean-up | VirtualDJ's mixer back to its starting state, both decks unloaded, VirtualDJ quit. QLC+ quit through `quit_app`; its preferences compared identical to the backup |

## Findings

- **N1 (low, new, MCP tool):** `read_dmx_values` sometimes returns values from the middle of an engine tick: intensity channels at 0, or the value of the losing LTP Scene. It happens about once in 7,000 reads, and about once in 100 when other MCP calls run at the same time. Reproduced with two plain Scenes, so it is not from the DJ set work. Cause: `Universe::preGMValues()` copies the buffer without a lock while the universe thread zeroes and rewrites it, and the tool copies it once per fixture. The DMX output itself was not checked (no output patched).
- **O1 (low, cause open, seen again):** at the hand-over (97.2 s) and at hamster on (137.2 s) both decks got a 97 to 148 ms catch-up segment (rate 2.1 to 3.5), the same pattern as the last run. Both moments are active-deck changes. Hamster off (145.4 s) had none.
- **F1 (low, seen again):** after a trim refused during playback, the clip's tooltip shows the refused drag (59.997 s / 3:26.811) while the clip stays at 56.0 s.
- **O2 (low, seen again):** after Perform off the set opens with its cursor at 23.9 s, the last followed song's position, not at the set's end.
- **O3 (low, seen again):** the back button from a nested song shows the set at time scale 5, not at the overview's zoom.

Not defects:

- LTP channels keep their last values after Perform off (the Mover stays where the last dominant Show left it), as in any QLC+ Show.
- A song's Chaser runs on its own clock, not on the song position: after a seek, or in the replay, it is on a different step than live. The pause hold works.

Pre-existing, not from the DJ set work: the stale tooltip after an undo, VDJ marker labels over the ruler, the toolbar wrapping when a nested Show is open, ruler labels crowding at fractional zooms, and below UI scale 1 the Record switch, Debug button and track chevron do not scale.

## Not covered

- The on-screen QLC+ window: the screenshots are offscreen renders of the same QML UI (software backend).
- Physical DMX: no output plugin was patched; values are the universe's values before the Grand Master.
- Sound: not listened to. The engine ran no Audio Function during the replay.
- Recording a VC slider during a live VirtualDJ mix: no MCP tool moves a slider. The slider recording below was made in the screenshot harness, through REC with the song Show playing.
- A held jog on a lone deck: deck 1 was playing, so VirtualDJ kept sending telemetry during the hold.
- The DJ Manager screenshots come from the harness fed with an earlier VirtualDJ capture over the bridge port, not from today's live mix.
- The per-control lane view of the slider recording: the render did not expand it. The expanded view is shown for Lamp A.

## Screenshots

Click a thumbnail for the full-size image.

### Function Manager

<table>
<tr><td align="center" valign="top"><a href="images/set/01-functions-songs-and-sets.png"><img src="images/set/01-functions-songs-and-sets.png" width="280" alt="01: folders Looks, Sets, Songs, VC"></a><br><sub>01: folders Looks, Sets, Songs, VC</sub></td><td align="center" valign="top"><a href="images/set/02-functions-folders-expanded.png"><img src="images/set/02-functions-folders-expanded.png" width="280" alt="02: the set in Sets; each song&#x27;s Show and mp3 in Songs"></a><br><sub>02: the set in Sets; each song's Show and mp3 in Songs</sub></td><td align="center" valign="top"><a href="images/songs/41-functions-all-folders.png"><img src="images/songs/41-functions-all-folders.png" width="280" alt="41: every folder open: the Looks and the Chaser, the lamp Scenes in VC"></a><br><sub>41: every folder open: the Looks and the Chaser, the lamp Scenes in VC</sub></td></tr>
</table>

### Song Shows

Betoko's song Show as QLC+ built it, with the Look and Chase tracks added and the three Lamp A presses recorded through REC (the yellow marks in the recording lane at 8.5, 20.1 and 30.0 s). Pre-existing: VDJ marker labels over the ruler.

<table>
<tr><td align="center" valign="top"><a href="images/songs/42-song-show.png"><img src="images/songs/42-song-show.png" width="400" alt="42: Audio, Look and Chase tracks, the recording lane; VDJ Beat ruler (the song Show&#x27;s default)"></a><br><sub>42: Audio, Look and Chase tracks, the recording lane; VDJ Beat ruler (the song Show's default)</sub></td><td align="center" valign="top"><a href="images/songs/48-song-lanes-expanded.png"><img src="images/songs/48-song-lanes-expanded.png" width="400" alt="48: lanes split per control: Lamp A&#x27;s three presses"></a><br><sub>48: lanes split per control: Lamp A's three presses</sub></td></tr>
</table>

### Rulers and zoom

<table>
<tr><td align="center" valign="top"><a href="images/songs/44-song-ruler-time.png"><img src="images/songs/44-song-ruler-time.png" width="400" alt="44: Time ruler"></a><br><sub>44: Time ruler</sub></td><td align="center" valign="top"><a href="images/songs/45-song-ruler-bpm-4-4.png"><img src="images/songs/45-song-ruler-bpm-4-4.png" width="400" alt="45: BPM 4/4 ruler at 120 BPM: the presses at bars 4, 10 and 15"></a><br><sub>45: BPM 4/4 ruler at 120 BPM: the presses at bars 4, 10 and 15</sub></td></tr>
<tr><td align="center" valign="top"><a href="images/songs/46-song-zoom-in.png"><img src="images/songs/46-song-zoom-in.png" width="400" alt="46: zoomed in, the first 19 s"></a><br><sub>46: zoomed in, the first 19 s</sub></td><td align="center" valign="top"><a href="images/songs/47-song-zoom-out.png"><img src="images/songs/47-song-zoom-out.png" width="400" alt="47: zoomed out to the whole 8:08 song"></a><br><sub>47: zoomed out to the whole 8:08 song</sub></td></tr>
</table>

### Song Show playing

The song Show alone, its Audio track muted (M): the playhead and the replayed Lamp A press.

<table>
<tr><td align="center" valign="top"><a href="images/songs/49-song-playing-9s.png"><img src="images/songs/49-song-playing-9s.png" width="400" alt="49: playing at 9.8 s, the playhead past the 8.5 s press"></a><br><sub>49: playing at 9.8 s, the playhead past the 8.5 s press</sub></td><td align="center" valign="top"><a href="images/songs/50-song-playing-vc.png"><img src="images/songs/50-song-playing-vc.png" width="400" alt="50: Virtual Console at the same moment: Lamp A on"></a><br><sub>50: Virtual Console at the same moment: Lamp A on</sub></td></tr>
</table>

### Show command recorder

REC armed with Cassian's song Show playing (its Audio track muted), the Wash C slider moved up and down by the harness through the same input path as the mouse. Playback started from where the cursor stood (11.7 s), so the slider recording sits at 12.2 to 16.7 s, between Lamp B's presses at 6.1 and 18.2 s.

<table>
<tr><td align="center" valign="top"><a href="images/songs/51-slider-recording.png"><img src="images/songs/51-slider-recording.png" width="400" alt="51: REC on: &#x27;REC: Recording - Cassian - Dun Dun - Playing&#x27;, the slider graph growing"></a><br><sub>51: REC on: 'REC: Recording - Cassian - Dun Dun - Playing', the slider graph growing</sub></td><td align="center" valign="top"><a href="images/songs/52-slider-lane-graph.png"><img src="images/songs/52-slider-lane-graph.png" width="400" alt="52: after REC off: the slider graph, 68 samples, 2.7 % up to 100 % and back to 0 %"></a><br><sub>52: after REC off: the slider graph, 68 samples, 2.7 % up to 100 % and back to 0 %</sub></td></tr>
<tr><td align="center" valign="top"><a href="images/songs/54-recordings-editor-list.png"><img src="images/songs/54-recordings-editor-list.png" width="400" alt="54: Recordings editor: Lamp B on at 6.108 s, then the slider positions"></a><br><sub>54: Recordings editor: Lamp B on at 6.108 s, then the slider positions</sub></td><td align="center" valign="top"><a href="images/songs/55-recordings-editor-split.png"><img src="images/songs/55-recordings-editor-split.png" width="400" alt="55: Recordings editor split with the timeline"></a><br><sub>55: Recordings editor split with the timeline</sub></td></tr>
<tr><td align="center" valign="top"><a href="images/songs/56-debug-panel-events.png"><img src="images/songs/56-debug-panel-events.png" width="400" alt="56: recorder debug panel, Events (filled only while it is open)"></a><br><sub>56: recorder debug panel, Events (filled only while it is open)</sub></td><td align="center" valign="top"><a href="images/songs/57-debug-panel-references.png"><img src="images/songs/57-debug-panel-references.png" width="400" alt="57: Referenced controls: Lamp B (3 toggle commands) and Wash C blue (68 slider commands), both Ready"></a><br><sub>57: Referenced controls: Lamp B (3 toggle commands) and Wash C blue (68 slider commands), both Ready</sub></td></tr>
</table>

### The recorded set

"Set 2026-10-30 21:21" from the live mix, at overview zoom 14 (the whole 266.8 s). Yellow marks are segment boundaries, the green line is the level, darker parts are where the deck was not dominant.

<table>
<tr><td align="center" valign="top"><a href="images/set/03-set-overview.png"><img src="images/set/03-set-overview.png" width="400" alt="03: Deck 1: Betoko with the loop wraps at 11 to 21 s, the seek at 30 s, the pause at 38 to 48 s, the level steps from 97 s, dominant again 137 to 145 s (hamster), paused from 185 s; CASHEW from 216.5 s. Deck 2: Cassian from 56 s, level steps up to 90 s, the jog hold at 170 s"></a><br><sub>03: Deck 1: Betoko with the loop wraps at 11 to 21 s, the seek at 30 s, the pause at 38 to 48 s, the level steps from 97 s, dominant again 137 to 145 s (hamster), paused from 185 s; CASHEW from 216.5 s. Deck 2: Cassian from 56 s, level steps up to 90 s, the jog hold at 170 s</sub></td><td align="center" valign="top"><a href="images/set/07-set-zoomed-overlap.png"><img src="images/set/07-set-zoomed-overlap.png" width="400" alt="07: the first 28 s: VirtualDJ&#x27;s 4-beat loop wraps about 2 s apart"></a><br><sub>07: the first 28 s: VirtualDJ's 4-beat loop wraps about 2 s apart</sub></td></tr>
<tr><td align="center" valign="top"><a href="images/set/04-set-clip-11-detail.png"><img src="images/set/04-set-clip-11-detail.png" width="400" alt="04: Betoko&#x27;s clip zoomed"></a><br><sub>04: Betoko's clip zoomed</sub></td><td align="center" valign="top"><a href="images/set/06-set-clip-13-detail.png"><img src="images/set/06-set-clip-13-detail.png" width="400" alt="06: Cassian&#x27;s clip zoomed"></a><br><sub>06: Cassian's clip zoomed</sub></td></tr>
<tr><td align="center" valign="top"><a href="images/set/05-set-clip-15-detail.png"><img src="images/set/05-set-clip-15-detail.png" width="400" alt="05: CASHEW&#x27;s clip zoomed: level 0 to 1 in steps, never dominant"></a><br><sub>05: CASHEW's clip zoomed: level 0 to 1 in steps, never dominant</sub></td><td align="center" valign="top"><a href="images/set/08-nested-song-opened.png"><img src="images/set/08-nested-song-opened.png" width="400" alt="08: double-click opens Betoko&#x27;s song Show with its Looks and recordings; the back button names the set. Pre-existing: the toolbar wraps"></a><br><sub>08: double-click opens Betoko's song Show with its Looks and recordings; the back button names the set. Pre-existing: the toolbar wraps</sub></td></tr>
<tr><td align="center" valign="top"><a href="images/set/09-nested-back-to-set.png"><img src="images/set/09-nested-back-to-set.png" width="400" alt="09: back to the set, at time scale 5, not the overview&#x27;s 14 (O3)"></a><br><sub>09: back to the set, at time scale 5, not the overview's 14 (O3)</sub></td></tr>
</table>

### Trim and undo

Cassian's left edge dragged 2 s later: start 56.046 to 58.035 s, every boundary and level step kept at its set time. Undo restores everything; its tooltip shows the trimmed values (pre-existing).

<table>
<tr><td align="center" valign="top"><a href="images/set/10-trim-before.png"><img src="images/set/10-trim-before.png" width="260" alt="10: before"></a><br><sub>10: before</sub></td><td align="center" valign="top"><a href="images/set/11-trim-after.png"><img src="images/set/11-trim-after.png" width="260" alt="11: trimmed"></a><br><sub>11: trimmed</sub></td><td align="center" valign="top"><a href="images/set/12-trim-undone.png"><img src="images/set/12-trim-undone.png" width="260" alt="12: undone"></a><br><sub>12: undone</sub></td></tr>
</table>

### Set replay

The set replayed from the Show Manager with Perform off.

<table>
<tr><td align="center" valign="top"><a href="images/replay/13-replay-5s.png"><img src="images/replay/13-replay-5s.png" width="280" alt="13: 5 s, Betoko alone"></a><br><sub>13: 5 s, Betoko alone</sub></td><td align="center" valign="top"><a href="images/replay/14-replay-20s.png"><img src="images/replay/14-replay-20s.png" width="280" alt="14: 20 s, inside the loop wraps"></a><br><sub>14: 20 s, inside the loop wraps</sub></td><td align="center" valign="top"><a href="images/replay/15-replay-trim-refused.png"><img src="images/replay/15-replay-trim-refused.png" width="280" alt="15: a trim while the set plays is refused; the clip stays at 56.0 s"></a><br><sub>15: a trim while the set plays is refused; the clip stays at 56.0 s</sub></td></tr>
<tr><td align="center" valign="top"><a href="images/replay/16-replay-45s.png"><img src="images/replay/16-replay-45s.png" width="280" alt="16: 45 s, the recorded pause; the tooltip shows the refused drag, 59.997 s (F1)"></a><br><sub>16: 45 s, the recorded pause; the tooltip shows the refused drag, 59.997 s (F1)</sub></td><td align="center" valign="top"><a href="images/replay/22-replay-75s.png"><img src="images/replay/22-replay-75s.png" width="280" alt="22: 75 s, Cassian joined at a low level"></a><br><sub>22: 75 s, Cassian joined at a low level</sub></td><td align="center" valign="top"><a href="images/replay/28-replay-100s.png"><img src="images/replay/28-replay-100s.png" width="280" alt="28: 100 s, after the hand-over at 97.2 s"></a><br><sub>28: 100 s, after the hand-over at 97.2 s</sub></td></tr>
<tr><td align="center" valign="top"><a href="images/replay/34-replay-140s.png"><img src="images/replay/34-replay-140s.png" width="280" alt="34: 140 s, the hamster window: Betoko dominant again"></a><br><sub>34: 140 s, the hamster window: Betoko dominant again</sub></td></tr>
</table>

### DMX output and VC during the replay

The same replay, viewed in Simple Desk (universe 1), the DMX view (values per fixture), the 2D view and the Virtual Console. Each view was taken about 1.5 s after the previous one, so later views can show the next Chase step or crossfader level.

<table>
<tr><td align="center" valign="top"><a href="images/dmx/02-replay-45s-simple-desk.png"><img src="images/dmx/02-replay-45s-simple-desk.png" width="400" alt="45 s, in the recorded pause: Wash A 255, Shared 255, Chase held at 64, Mover 40/10/70, Lamp A 255"></a><br><sub>45 s, in the recorded pause: Wash A 255, Shared 255, Chase held at 64, Mover 40/10/70, Lamp A 255</sub></td><td align="center" valign="top"><a href="images/dmx/04-replay-45s-dmx.png"><img src="images/dmx/04-replay-45s-dmx.png" width="400" alt="45 s + 3 s: the pause has ended, the Chase moved on to 192"></a><br><sub>45 s + 3 s: the pause has ended, the Chase moved on to 192</sub></td></tr>
<tr><td align="center" valign="top"><a href="images/dmx/05-replay-45s-2d.png"><img src="images/dmx/05-replay-45s-2d.png" width="400" alt="45 s, 2D view: Wash A, Shared and Chase red, Lamp A white, the Mover&#x27;s beam"></a><br><sub>45 s, 2D view: Wash A, Shared and Chase red, Lamp A white, the Mover's beam</sub></td><td align="center" valign="top"><a href="images/dmx/06-replay-45s-vc.png"><img src="images/dmx/06-replay-45s-vc.png" width="400" alt="45 s, VC: Lamp A on (Betoko&#x27;s recording), Lamp B off"></a><br><sub>45 s, VC: Lamp A on (Betoko's recording), Lamp B off</sub></td></tr>
<tr><td align="center" valign="top"><a href="images/dmx/08-replay-75s-simple-desk.png"><img src="images/dmx/08-replay-75s-simple-desk.png" width="400" alt="75 s: Cassian at level 0.4, Wash B 102; the Mover stays on dominant Betoko&#x27;s 40/10/70"></a><br><sub>75 s: Cassian at level 0.4, Wash B 102; the Mover stays on dominant Betoko's 40/10/70</sub></td><td align="center" valign="top"><a href="images/dmx/10-replay-75s-dmx.png"><img src="images/dmx/10-replay-75s-dmx.png" width="400" alt="75 s + 3 s: level 0.6, Wash B 153; Mover still 40/10/70"></a><br><sub>75 s + 3 s: level 0.6, Wash B 153; Mover still 40/10/70</sub></td></tr>
<tr><td align="center" valign="top"><a href="images/dmx/11-replay-75s-2d.png"><img src="images/dmx/11-replay-75s-2d.png" width="400" alt="75 s, 2D view: Wash B dim blue"></a><br><sub>75 s, 2D view: Wash B dim blue</sub></td><td align="center" valign="top"><a href="images/dmx/12-replay-75s-vc.png"><img src="images/dmx/12-replay-75s-vc.png" width="400" alt="75 s, VC: Lamp B still off, Cassian is not dominant"></a><br><sub>75 s, VC: Lamp B still off, Cassian is not dominant</sub></td></tr>
<tr><td align="center" valign="top"><a href="images/dmx/14-replay-100s-simple-desk.png"><img src="images/dmx/14-replay-100s-simple-desk.png" width="400" alt="100 s, after the hand-over: Wash A 204 (0.8), Wash B 255, Mover on Cassian&#x27;s 200/240/120"></a><br><sub>100 s, after the hand-over: Wash A 204 (0.8), Wash B 255, Mover on Cassian's 200/240/120</sub></td><td align="center" valign="top"><a href="images/dmx/16-replay-100s-dmx.png"><img src="images/dmx/16-replay-100s-dmx.png" width="400" alt="100 s + 3 s: Wash A 153 (0.6), Lamp A and Lamp B 255"></a><br><sub>100 s + 3 s: Wash A 153 (0.6), Lamp A and Lamp B 255</sub></td></tr>
<tr><td align="center" valign="top"><a href="images/dmx/17-replay-100s-2d.png"><img src="images/dmx/17-replay-100s-2d.png" width="400" alt="100 s, 2D view: both lamps white, Wash B full blue"></a><br><sub>100 s, 2D view: both lamps white, Wash B full blue</sub></td><td align="center" valign="top"><a href="images/dmx/18-replay-100s-vc.png"><img src="images/dmx/18-replay-100s-vc.png" width="400" alt="100 s, VC: Lamp B switched on at the hand-over"></a><br><sub>100 s, VC: Lamp B switched on at the hand-over</sub></td></tr>
<tr><td align="center" valign="top"><a href="images/dmx/03-replay-45s-unigrid.png"><img src="images/dmx/03-replay-45s-unigrid.png" width="400" alt="Universe view: the patch of universe 1"></a><br><sub>Universe view: the patch of universe 1</sub></td></tr>
</table>

### Perform and the DJ Manager

The harness fed the VDJ Bridge port with an earlier VirtualDJ capture whose songs are mapped to the three tracks, on the prepped workspace (song Shows with their Looks and the recorded VC presses). The feed waits while a shot is taken, so the set it records is longer than the capture.

<table>
<tr><td align="center" valign="top"><a href="images/perform/20-perform-deck1-playing-showmanager.png"><img src="images/perform/20-perform-deck1-playing-showmanager.png" width="280" alt="20: deck 1 plays: the Show Manager follows Betoko&#x27;s song Show, read-only (PERFORM), at 32.5 s"></a><br><sub>20: deck 1 plays: the Show Manager follows Betoko's song Show, read-only (PERFORM), at 32.5 s</sub></td><td align="center" valign="top"><a href="images/perform/21-perform-deck1-playing-djmanager.png"><img src="images/perform/21-perform-deck1-playing-djmanager.png" width="280" alt="21: DJ Manager: VDJ Connected, PERFORM Live; deck 1 Playing 100 % Show 11 ★, deck 2 Loaded 0 % Show 13"></a><br><sub>21: DJ Manager: VDJ Connected, PERFORM Live; deck 1 Playing 100 % Show 11 ★, deck 2 Loaded 0 % Show 13</sub></td><td align="center" valign="top"><a href="images/perform/22-perform-after-seek-showmanager.png"><img src="images/perform/22-perform-after-seek-showmanager.png" width="280" alt="22: after the loops and a seek, at 35.7 s"></a><br><sub>22: after the loops and a seek, at 35.7 s</sub></td></tr>
<tr><td align="center" valign="top"><a href="images/perform/23-perform-after-seek-djmanager.png"><img src="images/perform/23-perform-after-seek-djmanager.png" width="280" alt="23: DJ Manager after the seek"></a><br><sub>23: DJ Manager after the seek</sub></td><td align="center" valign="top"><a href="images/perform/24-perform-crossfade-overlap-showmanager.png"><img src="images/perform/24-perform-crossfade-overlap-showmanager.png" width="280" alt="24: crossfade: Betoko&#x27;s Show at 1:06 while deck 2 joins"></a><br><sub>24: crossfade: Betoko's Show at 1:06 while deck 2 joins</sub></td><td align="center" valign="top"><a href="images/perform/25-perform-crossfade-overlap-djmanager.png"><img src="images/perform/25-perform-crossfade-overlap-djmanager.png" width="280" alt="25: deck 2 Playing at 50 %, deck 1 keeps the ★"></a><br><sub>25: deck 2 Playing at 50 %, deck 1 keeps the ★</sub></td></tr>
<tr><td align="center" valign="top"><a href="images/perform/27-perform-crossfade-overlap-vc.png"><img src="images/perform/27-perform-crossfade-overlap-vc.png" width="280" alt="27: VC during the crossfade: Lamp A on; Lamp B off, Cassian is not dominant"></a><br><sub>27: VC during the crossfade: Lamp A on; Lamp B off, Cassian is not dominant</sub></td><td align="center" valign="top"><a href="images/perform/28-perform-crossfade-overlap-recording-set.png"><img src="images/perform/28-perform-crossfade-overlap-recording-set.png" width="280" alt="28: the set being recorded, read-only, its cursor at the record head (29.6 s)"></a><br><sub>28: the set being recorded, read-only, its cursor at the record head (29.6 s)</sub></td><td align="center" valign="top"><a href="images/perform/29-perform-deck2-active-showmanager.png"><img src="images/perform/29-perform-deck2-active-showmanager.png" width="280" alt="29: VirtualDJ made deck 2 active: the Show Manager switched to Cassian&#x27;s song Show"></a><br><sub>29: VirtualDJ made deck 2 active: the Show Manager switched to Cassian's song Show</sub></td></tr>
<tr><td align="center" valign="top"><a href="images/perform/30-perform-deck2-active-djmanager.png"><img src="images/perform/30-perform-deck2-active-djmanager.png" width="280" alt="30: Active: Deck 2; deck 1 at 0 % (its Show still runs), deck 2 100 % Show 13 ★"></a><br><sub>30: Active: Deck 2; deck 1 at 0 % (its Show still runs), deck 2 100 % Show 13 ★</sub></td><td align="center" valign="top"><a href="images/perform/32-perform-deck2-active-vc.png"><img src="images/perform/32-perform-deck2-active-vc.png" width="280" alt="32: VC: Lamp B switched on by Cassian&#x27;s recording at the hand-over"></a><br><sub>32: VC: Lamp B switched on by Cassian's recording at the hand-over</sub></td><td align="center" valign="top"><a href="images/perform/33-perform-deck2-active-recording-set.png"><img src="images/perform/33-perform-deck2-active-recording-set.png" width="280" alt="33: the set at 39.6 s: deck 1&#x27;s level falling, deck 2&#x27;s rising"></a><br><sub>33: the set at 39.6 s: deck 1's level falling, deck 2's rising</sub></td></tr>
<tr><td align="center" valign="top"><a href="images/perform/34-perform-deck1-paused-showmanager.png"><img src="images/perform/34-perform-deck1-paused-showmanager.png" width="280" alt="34: deck 1 paused: the Show Manager stays on Cassian&#x27;s Show"></a><br><sub>34: deck 1 paused: the Show Manager stays on Cassian's Show</sub></td><td align="center" valign="top"><a href="images/perform/35-perform-deck1-paused-djmanager.png"><img src="images/perform/35-perform-deck1-paused-djmanager.png" width="280" alt="35: deck 1 Paused, Show 11 suspended; deck 2 keeps the ★"></a><br><sub>35: deck 1 Paused, Show 11 suspended; deck 2 keeps the ★</sub></td><td align="center" valign="top"><a href="images/perform/36-perform-song-c-showmanager.png"><img src="images/perform/36-perform-song-c-showmanager.png" width="280" alt="36: deck 1 ejected and loaded song C"></a><br><sub>36: deck 1 ejected and loaded song C</sub></td></tr>
<tr><td align="center" valign="top"><a href="images/perform/37-perform-song-c-djmanager.png"><img src="images/perform/37-perform-song-c-djmanager.png" width="280" alt="37: deck 1 plays Gadasi at 50 %, Show 15"></a><br><sub>37: deck 1 plays Gadasi at 50 %, Show 15</sub></td><td align="center" valign="top"><a href="images/perform/39-perform-song-c-vc.png"><img src="images/perform/39-perform-song-c-vc.png" width="280" alt="39: VC: Lamp B off again, Cassian&#x27;s recording says off from 18.2 s"></a><br><sub>39: VC: Lamp B off again, Cassian's recording says off from 18.2 s</sub></td><td align="center" valign="top"><a href="images/perform/40-perform-song-c-recording-set.png"><img src="images/perform/40-perform-song-c-recording-set.png" width="280" alt="40: the set at 55.1 s: CASHEW&#x27;s clip on Deck 1 after a gap"></a><br><sub>40: the set at 55.1 s: CASHEW's clip on Deck 1 after a gap</sub></td></tr>
<tr><td align="center" valign="top"><a href="images/perform/41-set-after-perform-off.png"><img src="images/perform/41-set-after-perform-off.png" width="280" alt="41: Perform off: the set is editable; the cursor shows 23.9 s, the last followed song&#x27;s position, not the set&#x27;s end (O2)"></a><br><sub>41: Perform off: the set is editable; the cursor shows 23.9 s, the last followed song's position, not the set's end (O2)</sub></td><td align="center" valign="top"><a href="images/perform/42-djmanager-perform-off.png"><img src="images/perform/42-djmanager-perform-off.png" width="280" alt="42: DJ Manager with Perform off"></a><br><sub>42: DJ Manager with Perform off</sub></td></tr>
</table>

### UI scale

The set after the replay stopped (cursor at 2:20). Clip names stay above the level line at every scale. Pre-existing: the Record switch, Debug button and track chevron do not scale.

<table>
<tr><td align="center" valign="top"><a href="images/ui-scale/35-ui-scale-03-overview.png"><img src="images/ui-scale/35-ui-scale-03-overview.png" width="360" alt="35: scale 0.3, overview"></a><br><sub>35: scale 0.3, overview</sub></td><td align="center" valign="top"><a href="images/ui-scale/36-ui-scale-03-clip.png"><img src="images/ui-scale/36-ui-scale-03-clip.png" width="360" alt="36: scale 0.3, Cassian&#x27;s clip"></a><br><sub>36: scale 0.3, Cassian's clip</sub></td></tr>
<tr><td align="center" valign="top"><a href="images/ui-scale/37-ui-scale-06-overview.png"><img src="images/ui-scale/37-ui-scale-06-overview.png" width="360" alt="37: scale 0.6, overview"></a><br><sub>37: scale 0.6, overview</sub></td><td align="center" valign="top"><a href="images/ui-scale/38-ui-scale-06-clip.png"><img src="images/ui-scale/38-ui-scale-06-clip.png" width="360" alt="38: scale 0.6, Cassian&#x27;s clip"></a><br><sub>38: scale 0.6, Cassian's clip</sub></td></tr>
<tr><td align="center" valign="top"><a href="images/ui-scale/39-ui-scale-10-overview.png"><img src="images/ui-scale/39-ui-scale-10-overview.png" width="360" alt="39: scale 1.0, overview"></a><br><sub>39: scale 1.0, overview</sub></td><td align="center" valign="top"><a href="images/ui-scale/40-ui-scale-10-clip.png"><img src="images/ui-scale/40-ui-scale-10-clip.png" width="360" alt="40: scale 1.0, Cassian&#x27;s clip"></a><br><sub>40: scale 1.0, Cassian's clip</sub></td></tr>
</table>

