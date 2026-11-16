# VirtualDJ and Mixed In Key analysis markers as a reference baseline

Local measurements come from a read-only parse of `~/Library/Application Support/VirtualDJ/database.xml` (file untouched) by
`/private/tmp/claude-501/-Users-abossard-Desktop-projects-qlcplus/8022ed3d-8447-4a1a-90d1-38e3e869e07b/scratchpad/vdj/analyze_vdj_db.py` (run with `python3 -I`). Cited as [local DB]. Grid model: beat period = Scan `Bpm` (s), anchor = `beatgrid` POI `Pos` (missing `Pos` = 0), beat index k = (pos - anchor) / period, bar = 4 beats from the anchor.

## VirtualDJ Scan Bpm, Phase, beatgrid anchor and Scan Version (tempo, beat grid, downbeat)

### Takeaway
Scan `Bpm` is officially the beat period in seconds; `Phase` is undocumented but in this DB equals the `beatgrid` POI position (the "First Beat (Beat Anchor)") in 1795 of 1796 songs. VDJ documents the anchor only as "first detected beat", but its bar numbering is "1 bar = 4 beats" from the grid and the local data shows the anchor sits on a bar start (downbeat) in about 92-95% of songs, judged against independent MIK cues and VDJ End Break points.

### Cited Findings
- "The value stored in database is actually the time (in seconds) between 2 beats"; display BPM = `1 / Scan->Bpm * 60` — [VDJPedia VDJ_database (Atomix)](https://virtualdj.com/wiki/VDJ_database.html)
- `AltBpm` is "the second most possible BPM the scan engine found when scanning the track" — [VDJPedia VDJ_database](https://virtualdj.com/wiki/VDJ_database.html)
- The official Scan field list is Version, Flag, Volume, Bpm, AltBpm, Key. `Phase` is not documented there; no meaning is given for `Version` — [VDJPedia VDJ_database](https://virtualdj.com/wiki/VDJ_database.html)
- A plugin-developer forum question guessed Phase = "sample from start to first beat", unanswered; not a staff statement, so unverified — [VDJ forum 85535 via search](https://virtualdj.com/forums/85535/VirtualDJ_Plugins/XML_Database_file_info.html)
- VDJ creates the "First Beat (Beat Anchor)" POI on first analysis; with Snap on, POIs align to the closest beat — [VDJ manual, POI Editor](https://virtualdj.com/manuals/virtualdj/editors/poieditor.html)
- BPM Editor: the red line is the "Beatgrid anchor (first detected beat)" in Rigid mode; "The grey-scaled background indicates the 4 beat pattern"; buttons "Shifts the downbeat forward or backward by 1 beat", "Sets the downbeat for the bar (4 beats)", "Sets the downbeat for the phrase (16 beats/4 bars)"; fluid BPM tracks show multiple anchors; vendor claims accuracy "in almost all cases" — [VDJ manual, BPM Editor](https://virtualdj.com/manuals/virtualdj/editors/bpmeditor.html)
- VDJScript: `get_firstbeat` = "position of the first beat (in milliseconds)"; `get_bar` "1 bar = 4 beats"; `get_beat_num` 1-4 = beat in the measure; `get_phrase_num` 1-4 = measure number; `goto_beat_in_bar` = "nth beat after the downbeat" — [VDJ manual, VDJScript verbs](https://virtualdj.com/manuals/virtualdj/appendix/vdjscriptverbs.html)
- Local: 1885 songs under ZZ_INCOMING (the brief's 1,911 count includes songs elsewhere or deleted); 1884 have Scan Bpm and Phase, all `Version="801"`; 1796 have a beatgrid POI, none has more than one (no fluid grids); BPM median 126.0, range 54-240 — [local DB]
- Local downbeat test (markers within 50 ms of a beat, per-song dominant beat-in-bar index, songs with at least 3 such markers): MIK Energy cues phase 0 in 185/194 songs (95.4%); VDJ End Break points phase 0 in 96/104 (92.3%); "Cue N" cues 666/848 (78.5%) — [local DB]

### Inferences
- `Phase` = anchor position in seconds (same value as the beatgrid POI). The sample-count interpretation is ruled out by the data (values are seconds-scale, matching `Pos`).
- The anchor is a downbeat in the large majority of songs. VDJ stores no separate downbeat field, so the BPM editor's "shift downbeat" must move the anchor itself; the bar phase is therefore "anchor + 4n beats". Two independent sources (MIK's own grid, VDJ's section detector) agree with phase 0 in 92-95% of songs, which bounds the anchor-not-downbeat rate at roughly 5-8% (upper bound; some disagreement is the other tool's error).
- Usable as baseline for tempo and beat grid (constant-tempo EDM), and as a "probable downbeat" baseline. Not ground truth: no published accuracy figures, and the ~5-8% bar-phase disagreements need manual checking.
- Scan Version 801 is most likely the analysis engine version (uniform across the DB), so all grids come from the same analyzer.

### Gaps
- No official document defines `Phase` or `Scan Version`; no VDJ staff post found that states whether the anchor is guaranteed to be a downbeat.
- No peer-reviewed evaluation of VirtualDJ beat or downbeat tracking found.

## VirtualDJ "remix" POIs (Break N / End Break N) and automix POIs (breakdown regions)

### Takeaway
Remix points are VDJ-generated on analysis where "there is a definitive change in the audio", e.g. a break start or end; VDJ publishes no algorithm or accuracy. Locally, End Break lands on a bar start (84%) and often on an 8-bar line, while Break N lands systematically one beat before a bar start, so Break/End Break regions are typically 32+1, 64+1, 96+1 beats long.

### Cited Findings
- "VirtualDJ will automatically create invisible Remix Points if there is a definitive change in the audio information", e.g. "the beginning or end of a break"; shown on the Remix Points pad page; can also be set manually — [VDJ manual, POI Editor](https://virtualdj.com/manuals/virtualdj/editors/poieditor.html)
- Automix POIs: up to 8 created on first BPM analysis; types Tempo, Cut, Fade, Full, each with start/exit; re-analysis restores them; Smart automix prefers TEMPO, then CUT, FADE, FULL — [VDJ manual, POI Editor](https://virtualdj.com/manuals/virtualdj/editors/poieditor.html)
- Forum users (not staff) report remix-point detection is inconsistent across tracks (some tracks none, some several); unverified — [VDJ forum 244124](https://virtualdj.com/forums/244124/General_Discussion/Remix_Points___automatically_analyzed_by_VirtualDJ_.html)
- Local: 1406/1885 songs have Break remix POIs (2254 Break, 2158 End Break). Remix name counts across the whole DB: 3279 "Break N", 3157 "End Break N" — [local DB]
- Local, Break N: median 3.0 ms to nearest beat, 78.6% within 50 ms (chance 20.9%), p90 214 ms; among on-beat ones beat-in-bar index {0: 368, 1: 155, 2: 258, 3: 990}; per-song dominant index 3 in 60/85 songs — [local DB]
- Local, End Break N: median 3.4 ms, 88.6% within 50 ms; on bar start 84.0% (chance 25%), on 4-bar line 64.5%, 8-bar 48.9% (chance 3.1%), 16-bar 26.3% (chance 1.6%) — [local DB]
- Local, Break to End Break length: median 65 beats; most common 33 (215), 65 (139), 34 (93), 37 (88), 35, 97 (68), 36, 32 (65) — [local DB]

### Inferences
- End Break is a good "section starts here" candidate (phrase-aligned). Break N is placed about one beat early; for a breakdown-start label subtract nothing but snap to the next bar start (+1 beat) before comparing. The 34-37 beat lengths show Break starts are sometimes 2-5 beats off a clean phrase.
- "Break" means VDJ's detected break/breakdown region, but the exact criterion (energy drop, drums removed, spectral change) is undocumented. Treat as a weak baseline for breakdown regions, not ground truth; ~25% of songs have none.
- Automix POIs (realStart, cutStart, tempoStart, ...) are mix-in/mix-out points, not structure labels; at most usable as intro/outro bounds.

### Gaps
- No official or staff description of how remix points are computed, why Break sits one beat before the bar, or their accuracy.
- Database Point names (realStart, fadeStart, cutStart, tempoStart) are not documented by name; the mapping to the manual's Full/Fade/Cut/Tempo types is inferred from naming.

## Mixed In Key "Energy N" cues, "Cue N" cues and comment "10A - 7" (drops, sections, energy)

### Takeaway
MIK writes up to 8 automatic cue points (via Serato markers, which VDJ imports) at "the most important parts" of a track and a 1-10 track Energy Level; MIK documents neither the cue algorithm nor a per-cue energy definition. The comment "10A - 7" is Camelot key plus track Energy Level. Locally, MIK cues sit on phrase boundaries (95% bar start, 85% 4-bar, 75% 8-bar from the VDJ anchor) once a per-batch ~50 ms MP3 offset is removed.

### Cited Findings
- "Energy Level is a 1 to 10 value that helps describe how intense a song feels"; "based on musical characteristics"; "One important factor is high-frequency content"; "there can be outliers"; "a single track-level score cannot describe every breakdown" — [MIK, Use Energy Level Detection](https://mixedinkey.com/workflows/use-energy-level-detection/)
- MIK 11: "up to 8 automatic Cue Points per track"; "Our pioneering algorithm detects the most important parts of your tracks and adds precision cue points"; an "Energy Level breakdown" shows energy "for each segment of your track"; key results "at least 10% more accurate than any software that has ever existed" (no method given) — [MIK, Learn more](https://mixedinkey.com/learn-more/)
- Cue points are "designed to help you jump to useful parts of the song, such as intros, breakdowns, drops, and mix-out sections"; analyze the beat grid in Serato first so "Cue Points line up correctly with the Serato beatgrid"; comment example "2A – Energy 7" (Key and Energy Level) — [MIK + Serato guide](https://mixedinkey.com/workflows/use-mixed-in-key-with-serato/)
- Comment content is configurable: Key only, Energy only, Key and Energy, Key and Tempo, or Key, Tempo and Energy; overwrite or prepend — [MIK + Traktor guide via search](https://mixedinkey.com/workflows/use-mixed-in-key-with-traktor/)
- MIK community threads (2015) show users asking why auto cues show "Energy 1, Energy 2 etc"; a MIK staffer said cue names are not editable in MIK. Thread pages could not be fetched (HTTP 530), so the staff wording is from search snippets only — [MIK community 14417](http://community.mixedinkey.com/Topics/14417/auto-created-cue-points-all-show-as-energy-1-energy-2-etc), [MIK community 15006](https://community.mixedinkey.com/Topics/15006/remove-the-text-energy-3-etc-from-the-cue-point)
- CUE-DETR paper: MIK cues are generated while MIK "simultaneously estimates the beat grid to which it snaps generated cue points" — [Argüello, Lanzendörfer, Wattenhofer, ISMIR 2024, arXiv 2407.06823](https://arxiv.org/html/2407.06823v1)
- Local: 1615/1885 songs have a comment matching `KEY - E`; track energy distribution {2: 9, 3: 18, 4: 26, 5: 80, 6: 527, 7: 637, 8: 298, 9: 20} — [local DB]
- Local: 208 songs have "Energy N" cues (cues/song {4: 3, 5: 8, 6: 4, 7: 18, 8: 175}); value distribution {2: 16, 3: 35, 4: 236, 5: 440, 6: 602, 7: 242, 8: 29, 9: 2}. 925 songs have "Cue N" cues (cues/song, 8 in 607); no song has both. Both sets use the same Num/Color sequence (Serato default cue colours), i.e. both are imported MIK/Serato markers — [local DB]
- Local: track energy equals the max cue energy in 52.4% of songs; |track energy - round(mean cue energy)| <= 1 in 86.1% (n=208) — [local DB]
- Local, Energy N raw: signed offset to nearest beat median +49.1 ms (p90 distance 55.1 ms), only 59.4% within 50 ms. By file type: .mp3 +49.2 ms (n=1551), .aiff -2.1 ms (n=51). By folder: 2025-09, 2025-11, 2025_06, 2025_07, 2026-03 all +50 ms; 2026-09 +24 ms — [local DB]
- Local, Energy N after subtracting 49.1 ms: median 3.3 ms to beat, 92.9% within 50 ms (chance 21.1%); on bar start 94.9% (chance 25%), 4-bar line 84.6%, 8-bar 75.2% (chance 3.1%), 16-bar 48.8% (chance 1.6%); median distance to bar 0.008 beats — [local DB]
- Local, Energy cue spacing: median 64 beats; multiple of 4 beats 99.4%, of 16 90.6%, of 32 82.9%; most common gaps 64 (431), 32 (347), 96 (175), 128 (142) — [local DB]
- Local, "Cue N" cues (older folders, no offset, median -1.3 ms): 91.3% within 50 ms; bar start 77.3%, 4-bar 63.5%, 8-bar 49.8%, 16-bar 29.1% — [local DB]

### Inferences
- "Energy N" in a cue name is MIK's segment energy (1-10) for the part starting at that cue, from the "Energy Level breakdown"; MIK does not define it officially, so the "segment starting at the cue" reading is inferred from the feature description and from track energy tracking the max/mean cue energy.
- MIK cue positions are phrase-boundary candidates (spacing in 16/32-beat multiples), so they mark section changes; nothing official says they are drops specifically. Drop vs breakdown must be inferred from the energy value change between consecutive cues.
- The ~50 ms shift on MP3s (none on AIFF) points to a decoder-delay/encoder-padding mismatch between how MIK wrote the Serato markers and how VDJ decodes the file. It varies by batch (0, +24, +37, +50 ms), so align per file (or per batch) before any ms-level comparison. Beat-level tolerances (T1/2 = half a beat ≈ 238 ms at 126 BPM) are unaffected.
- "Cue N" cues are less phrase-aligned (77% bar start) than "Energy N" cues (95%). Likely an older MIK version or partly user-edited; treat them as a weaker baseline.
- Energy values: usable as a relative per-song baseline (ordering of sections), not as calibrated ground truth; MIK publishes no accuracy for energy.

### Gaps
- No official MIK description of the cue placement algorithm, per-cue energy definition, cue naming option, or the "KEY - E" comment string variant (official examples use "2A – Energy 7"; the "10A - 7" format matches the documented "Key and Energy" option but its exact template is not shown).
- No MIK statement about the MP3 offset.

## Independent (peer-reviewed) evaluations

### Takeaway
The only peer-reviewed number found is CUE-DETR (ISMIR 2024): against DJ-placed cues, MIK 10 reaches F1 0.22 (cues-only, 1-beat tolerance), i.e. MIK cues agree poorly with where club DJs put cues. No peer-reviewed evaluation of VirtualDJ grids, remix points, or MIK energy was found.

### Cited Findings
- EDM-CUE dataset: 4,710 EDM tracks (~380 h), 21,461 cue annotations (~4.6 per track) from "four private collections of professional DJs", exported from rekordbox with tempo, beat grid, cues; 95-190 BPM — [CUE-DETR, arXiv 2407.06823](https://arxiv.org/html/2407.06823v1)
- Test set: 101 held-out tracks, 607 ground-truth cues. Baselines: Mixed In Key 10 and Automix; MIK analyzed "without manual interference" — [CUE-DETR](https://arxiv.org/html/2407.06823v1)
- Table 2, tolerance T1 (1 beat), P/R/F1: MIK 10 cues-only 0.20/0.25/0.22; 16-bars 0.21/0.13/0.16; 8-bars 0.25/0.08/0.12. Automix cues-only 0.14/0.12/0.13. CUE-DETR r8 cues-only 0.32/0.49/0.39, 16-bars 0.53/0.41/0.46 (best) — [CUE-DETR](https://arxiv.org/html/2407.06823v1)
- Table 2, tolerance T1/2 (half beat): MIK 10 cues-only 0.14/0.19/0.16; 16-bars 0.15/0.09/0.12; 8-bars 0.18/0.06/0.09 — [CUE-DETR](https://arxiv.org/html/2407.06823v1)
- Fig. 5 cosine similarity to ground truth: CUE-DETR 0.851, Automix 0.425, MIK 0.371; MIK "shows little improvement over the different scenarios" (phrase-aligned ground truth sets) — [CUE-DETR](https://arxiv.org/html/2407.06823v1)
- Zehren, Alunno, Bientinesi, "Automatic Detection of Cue Points for DJ Mixing" (arXiv 2007.08411, 2020, no journal ref listed) reports ~96% good switch points for its own method; a search snippet attributes "precision achieved by Mixed in Key is 65%" to it, which I could not verify in the PDF text. Peer-review status unconfirmed — [arXiv 2007.08411](https://arxiv.org/abs/2007.08411)

### Inferences
- MIK cues answer "where does a section change" reasonably on the grid (local data) but do not match DJ cue choices; low recall against 8/16-bar ground truth suggests MIK picks a different subset of boundaries, not that its boundaries are off-grid.
- For an EDM structure analyzer: VDJ grid = tempo/beat baseline with probable downbeat; MIK Energy cues = phrase-boundary and relative-energy baseline; VDJ End Break = section-start candidates; VDJ Break = breakdown-start candidate (snap +1 beat). None qualifies as ground truth; a small hand-labelled set is still needed to measure them.

### Gaps
- CUE-DETR tolerances are beat-based; it does not report MIK beat-grid or downbeat accuracy.
- No peer-reviewed study of MIK Energy Level validity or VirtualDJ beat/downbeat/remix-point accuracy found.
