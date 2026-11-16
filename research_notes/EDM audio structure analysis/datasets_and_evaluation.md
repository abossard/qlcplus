# Datasets, Annotations and Evaluation for EDM Audio Analysis

Scope: public datasets and metrics to benchmark or sanity-check offline analysis (structure, drops/buildups, beats/downbeats, vocals, instruments, mood) of ~800 personal EDM MP3s, plus how to build a small personal ground truth. Research date: 2026-11-04.

## Structure datasets (general and EDM-specific, incl. drops and DJ-mix sets)

### Takeaway
General structure sets (Harmonix, SALAMI, RWC, Isophonics/Beatles, SPAM) are pop-centric and use verse/chorus vocabularies; Harmonix has 129 "Dance/Electronic" tracks but almost no build/drop labels. The best EDM-specific resource in 2026 is **Raveform** (TISMIR, April 2026): 1,423 EDM tracks with beats, downbeats and EDM-vocabulary sections (Intro, Buildup, Breakdown, Drop, Cooldown, Bridge, Outro), audio via YouTube IDs. EDM-CUE (cue points, 4,710 tracks, MIT, no audio) and EDM-98 (98 tracks, access unclear) are secondary.

### Cited Findings

**Harmonix Set (Nieto et al., ISMIR 2019)**
- 912 Western pop tracks; annotations: beats and downbeats (time, beat-in-bar, bar number), functional segments (boundary + label), metadata (BPM, time signature, genre, MusicBrainz/AcoustID IDs), one JAMS file per track — [GitHub harmonixset](https://github.com/urinieto/harmonixset)
- Audio not included. Provided: `youtube_urls.csv` (912 URLs), `youtube_alignment_scores.csv` (DTW alignment scores of YouTube audio vs. the annotated audio, plus an alignment notebook), and ~1.2 GB mel spectrograms on Dropbox under a separate license. Repo code/annotations MIT — [GitHub harmonixset](https://github.com/urinieto/harmonixset)
- Genre counts computed from `dataset/metadata.csv` (own analysis of the file): Pop 422, Hip-Hop 140, **Dance/Electronic 129**, Country 39, Rock 36, Alternative 33, Metal 32, R&B 25, Reggaeton 15, Funk/Disco 14, other <10 each — [metadata.csv](https://raw.githubusercontent.com/urinieto/harmonixset/main/dataset/metadata.csv)
- The 129 Dance/Electronic tracks are mostly mainstream dance-pop (e.g. Daft Punk "Around The World", Darude "Sandstorm", Lady Gaga "Poker Face", Alexandra Stan "Mr. Saxobeat"); BPM range 77 to 150, median 128 — own analysis of [metadata.csv](https://raw.githubusercontent.com/urinieto/harmonixset/main/dataset/metadata.csv)
- Segment labels in those 129 tracks (own count from `dataset/segments/*.txt`): chorus 467, verse 255, intro 127, prechorus 112, inst 106, bridge 91, postchorus 62, outro 54, break 35, breakdown 9, **build 4, drop 0**. Harmonix is therefore not usable as drop/buildup ground truth without relabeling — [harmonixset segments](https://github.com/urinieto/harmonixset/tree/main/dataset/segments)
- Alignment scores (undocumented scale, 0 to 1): median 0.984 for all 912 tracks, min 0.80; median 0.984 for Dance/Electronic. Fraction of YouTube links still alive in 2026 not checked — own analysis of [youtube_alignment_scores.csv](https://raw.githubusercontent.com/urinieto/harmonixset/main/dataset/youtube_alignment_scores.csv)
- Harmonix boundaries always fall on beats; one annotation per track — [Nieto et al., TISMIR 2020 review](https://transactions.ismir.net/articles/10.5334/tismir.54)

**SALAMI, SPAM, RWC, Isophonics, Beatles**
- SALAMI: 1,359 tracks, 884 with at least two annotators, hierarchical annotations at three levels; MIREX 2012 used 859 tracks — [TISMIR 2020 review](https://transactions.ismir.net/articles/10.5334/tismir.54). mirdata: audio not distributed, sections only, CC0 1.0 — [mirdata quick reference](https://mirdata.readthedocs.io/en/stable/source/quick_reference.html)
- SPAM: 50 tracks, at least five hierarchical annotations each (45 selected as hard to segment), 32 overlap SALAMI — [TISMIR 2020 review](https://transactions.ismir.net/articles/10.5334/tismir.54)
- RWC (AIST annotations): 300 annotated tracks, copyright-cleared audio (sold, not free), single annotator — [TISMIR 2020 review](https://transactions.ismir.net/articles/10.5334/tismir.54). mirdata lists RWC popular (100 tracks: beats, sections, vocal activity, chords, tempo), audio not available, custom license — [mirdata](https://mirdata.readthedocs.io/en/stable/source/quick_reference.html)
- Isophonics: 300 singly-annotated tracks, mostly pop-rock; TUT Beatles: 174 refined Beatles annotations — [TISMIR 2020 review](https://transactions.ismir.net/articles/10.5334/tismir.54). mirdata Beatles: 180 tracks, beats/chords/sections/key/vocal activity, no audio — [mirdata](https://mirdata.readthedocs.io/en/stable/source/quick_reference.html)
- EDMFormer paper describes SALAMI as pop-centric with minimal EDM content — [arXiv 2603.08759](https://arxiv.org/html/2603.08759v1)

**Raveform (Kim, Kim, Kim, Nam; TISMIR vol. 9, 2026)** — EDM-specific, strongest candidate
- 4,902 DJ mixes (~6,522 h), 56,873 unique tracks, 53,780 transitions; **1,423 tracks with manual structure annotations**: tempo, beats, downbeats, functional segment boundaries and labels — [TISMIR article](https://transactions.ismir.net/articles/288)
- EDM vocabulary: Intro, Buildup, Breakdown, Drop, Cooldown, Bridge, Outro, Ambient-Intro, Ambient-Outro; energy/"tension"-centred view — [TISMIR article](https://transactions.ismir.net/articles/288)
- Three domain experts, ~7 months, one consolidated annotation per track (no independent double annotation, no numeric agreement reported); weekly guideline meetings, rotation, verification and final review — [TISMIR article](https://transactions.ismir.net/articles/288)
- Covers specific EDM subgenres; disco not included — [TISMIR article](https://transactions.ismir.net/articles/288)
- Download: `https://huggingface.co/datasets/taejunkim/raveform/resolve/main/raveform.zip`; code at `mir-aidj/raveform`. Files: `mixes.jsonl`, `tracks.jsonl`, `alignments/*.align.jsonl`, `structures/segments.json`, `structures/beats/<TRACK_KEY>.beat.csv`. Audio not included: tracks are YouTube video IDs; mixes link to SoundCloud/Mixcloud/YouTube. Annotated tracks carry a manually assigned Beatport genre (e.g. "Techno") — [Raveform project page](https://mir-aidj.github.io/raveform/)
- No dataset license stated on the project page; the article itself is CC BY 4.0 — [Raveform project page](https://mir-aidj.github.io/raveform/); [TISMIR article](https://transactions.ismir.net/articles/288)
- Evaluation used: beat/downbeat F1 (±70 ms), CMLt, AMLt (±17.5% of beat period); structure HR.5F, pairwise F (PWF), NCE (Sf) — [TISMIR article](https://transactions.ismir.net/articles/288)
- Project page links the `mir-aidj/all-in-one` structure model; "v2 will be released soon" — [Raveform project page](https://mir-aidj.github.io/raveform/)

**EDM-CUE (Argüello, Lanzendörfer, Wattenhofer; ISMIR 2024)**
- 4,710 EDM tracks (~380 h), 21,461 manually placed DJ cue points (avg 4.6/track), from four professional DJs' rekordbox collections — [arXiv 2407.06823](https://arxiv.org/pdf/2407.06823)
- HF card: Deezer track ID, title, artists, duration, genre (possibly incomplete), key (rekordbox-generated, may be wrong), beat grid (bpm, init_beat, start_pos, time_sig), cue point times; train 4,609 / valid 101; ~293 kB parquet; **MIT**; no audio — [HF disco-eth/edm-cue](https://huggingface.co/datasets/disco-eth/edm-cue/blob/main/README.md)
- Cue points cluster at multiples of 8 and 16 bars — [chatpaper summary](https://chatpaper.com/paper/38517)
- Raveform authors note cue point annotation criteria are inconsistent across annotators, so "cue point" is ambiguous — [TISMIR Raveform](https://transactions.ismir.net/articles/288)

**EDM-98 / EDMFormer (arXiv 2603.08759, March 2026)**
- 98 "professionally annotated" EDM tracks, stratified subset of EDM-CUE across five tempo bins; labels intro, build-up, drop, breakdown, outro, silence, end, ±0.5 s precision. Paper contradicts itself (two annotators vs. single annotator per track). Dataset access/license not stated — [arXiv 2603.08759](https://arxiv.org/html/2603.08759v1)
- Reported HR@0.5s 0.616, HR@3s 0.635, frame label accuracy 0.883 (EDMFormer) vs. 0.569 / 0.608 / 0.148 (SongFormer) — [arXiv 2603.08759](https://arxiv.org/html/2603.08759v1)

**Yadati et al. drop dataset (ISMIR 2014) and "Socially Significant Music Events" (SSME)**
- ISMIR 2014 set: 100 CC-licensed SoundCloud tracks (Dubstep 36, Electro 36, House 28), 60/20/20 split, 190 manually marked drops, 225 user "drop" timed comments; one author marked drops in ~6 h (skipping 30 s after each drop); drop = "where the buildup ends and the bassline is re-introduced"; only 20% of drop comments sit at the actual drop — [Yadati et al. ISMIR 2014 PDF](https://archives.ismir.net/ismir2014/paper/000297.pdf)
- Later SSME dataset: 402 CC-licensed SoundCloud EDM tracks (avg 4.9 min) with timed comments for Drop, Build, Break; hosted at osf.io/eydxk — [SIGMM Records](https://records.sigmm.org/2018/03/07/socially-significant-music-events/). Current availability of OSF files and SoundCloud audio not verified.
- MediaEval 2014 drop task classified 15 s excerpts into three categories — [MediaEval 2014 paper](https://ceur-ws.org/Vol-1263/mediaeval2014_submission_88.pdf)

**DJ-mix datasets**
- UnmixDB: synthetic beat-synchronous DJ mixes (3 track excerpts per mix) with exact ground truth of track positions; Zenodo, Sept 2018 — [Zenodo 1422385](https://zenodo.org/record/1422385); [HAL](https://hal.archives-ouvertes.fr/hal-02010431)
- UnmixDB source tracks: Sonnleitner et al. (ISMIR 2016) CC-licensed tracks of 10 Mixotic netlabel mixes — [UnmixDB LBD PDF](https://zenodo.org/records/1422385/files/schwarz-fourer-ismir2018late-breaking-unmixdb.pdf). Mixotic archive mirrored on Internet Archive — [archive.org mixotic.net_202209](https://archive.org/details/mixotic.net_202209)

**Subgenre sets**
- HouseX-v2 (arXiv 2409.06690): >1,000 tracks (1,035 train/val), 7,104 drop-segment clips (~7.5 s), 8 subgenres (Progressive, Future, Bass, Tech, Deep House, Bigroom, Future Rave, Slap House), BPM 115 to 130; "code and data open-sourced", but raw-audio distribution and license not stated — [arXiv 2409.06690](https://arxiv.org/html/2409.06690v3)

### Inferences
- For drop/buildup benchmarking, Raveform is the only large public set with Drop/Buildup labels plus beats; audio must be re-sourced from YouTube, and many Raveform tracks may overlap with a personal EDM collection (could be matched by artist/title to get free ground truth for owned files).
- Harmonix Dance/Electronic (129) is useful for beats/downbeats and boundary timing on dance-pop, not for drop semantics.
- Treat Raveform's license as unknown until the HF repo card is checked; assume research-only.

### Gaps
- Raveform dataset license and YouTube link survival rate not confirmed.
- Harmonix YouTube link survival in 2026 not measured.
- Yadati/SSME OSF files not checked for availability.
- "EDM-150" and "Beatport-derived structure set": no evidence found of a public dataset with these names.
- No public EDM structure dataset with independent double annotations (hence no EDM-specific human ceiling) was found.

## Beat, tempo and key datasets with EDM

### Takeaway
GiantSteps tempo (664 Beatport 2-min previews, revised crowdsourced v2 labels) and GiantSteps key / Beatport EDM key (500 / 1,486 tracks, CC BY-SA 4.0) are the standard EDM tempo/key benchmarks; Raveform and EDM-CUE add beat grids for full EDM tracks. Ballroom is non-EDM but a common beat sanity check.

### Cited Findings
- GiantSteps tempo: 664 two-minute Beatport previews; `annotations_v2/` holds revised tempo labels; audio fetched with `./audio_dl.sh` — [GitHub GiantSteps/giantsteps-tempo-dataset](https://github.com/GiantSteps/giantsteps-tempo-dataset)
- Original 2015 labels came from a forum of user corrections of Beatport tempos; Schreiber and Müller (ISMIR 2018) re-annotated via crowdsourced tapping (266 participants, half-overlapping 30 s segments), giving per-track tempo distributions — [Schreiber & Müller ISMIR 2018](https://ismir2018.ismir.net/doc/pdfs/220_Paper.pdf); [Knees et al. UPF](https://repositori.upf.edu/items/4e5f12d8-9dc0-43c5-a5e6-337c1e7184c2)
- License conflict: mirdata lists CC BY-SA 4.0; Knees et al. paper says CC BY 4.0 — [mirdata giantsteps_tempo](https://mirdata.readthedocs.io/en/stable/_modules/mirdata/datasets/giantsteps_tempo.html)
- mirdata: GiantSteps tempo 664 tracks, genre + tempo, "DIY" audio; GiantSteps key 500 tracks, audio downloadable, CC BY-SA 4.0; **Beatport EDM key 1,486 tracks, audio downloadable, CC BY-SA 4.0**; Ballroom 698 tracks, beats/tempo/genre, CC0; GTZAN 1,000 (beats/tempo via add-ons), no license; FMA Keys 5,489, CC BY-SA 4.0 — [mirdata quick reference](https://mirdata.readthedocs.io/en/stable/source/quick_reference.html)
- EDM-CUE beat grid comes from rekordbox (bpm, first beat, time signature), not hand-tapped — [HF edm-cue](https://huggingface.co/datasets/disco-eth/edm-cue/blob/main/README.md)

### Inferences
- GiantSteps previews are 2-minute Beatport snippets, so they test tempo/key, not full-track structure. Beatport preview URLs may have rotted; mirdata's "DIY" flag implies no guaranteed mirror.
- For beats on full EDM tracks, Raveform `.beat.csv` files (expert-checked) are preferable to EDM-CUE's rekordbox grids.

### Gaps
- Fraction of GiantSteps audio still downloadable in 2026 not checked.
- Number of tracks covered by GiantSteps `annotations_v2` not confirmed.

## Vocal, instrument and mood datasets

### Takeaway
None of the standard separation/tagging sets is EDM-focused. MUSDB18-HQ and MoisesDB are non-commercial and contain little electronic music; MTG-Jamendo (55k CC tracks, genre/instrument/mood tags) is the most practical for instrument/mood calibration on electronic music, but its metadata is CC BY-NC-SA and use is restricted to non-commercial research.

### Cited Findings
- MUSDB18-HQ: 150 full songs (100 train / 50 test), stems mixture/drums/bass/other/vocals, 44.1 kHz stereo WAV, 22.7 GB zip on Zenodo; "Other (Non-Commercial)", educational purposes only; sources: 100 DSD100, 46 MedleyDB (CC BY-NC-SA 4.0), 2 Native Instruments, 2 The Easton Ellises — [Zenodo 3338373](https://zenodo.org/records/3338373)
- MoisesDB: 240 tracks, 45 artists, 12 genres, stems beyond 4; license given as NC-RCL on GitHub vs. CC BY-NC-SA 4.0 on Hugging Face, commercial use excluded either way — [GitHub moises-db](https://github.com/moises-ai/moises-db); [HF moisesdb](https://huggingface.co/datasets/wearemusicai/moisesdb)
- MedleyDB melody/pitch (108/103 tracks), available on request, CC BY-NC-SA 4.0 — [mirdata](https://mirdata.readthedocs.io/en/stable/source/quick_reference.html)
- MTG-Jamendo: 55,525 full tracks in the autotagging split, 195 tags (87 genre, 40 instrument, 56 mood/theme in splits), 320 kbps MP3 (plus mono low-quality and mel-spectrograms), download via `scripts/download/download.py`; metadata CC BY-NC-SA 4.0, audio per-track CC licenses in `audio_licenses.txt`; "solely for non-commercial research and academic use" unless Jamendo authorizes — [GitHub MTG/mtg-jamendo-dataset](https://github.com/MTG/mtg-jamendo-dataset)
- MTG-Jamendo mood/theme subset: 18,448 tracks, CC BY-NC-SA 4.0, mirdata loader — [mirdata](https://mirdata.readthedocs.io/en/stable/source/quick_reference.html)
- Song Describer Dataset: music captions on audio drawn from MTG-Jamendo, chosen for persistence and explicit licenses — [arXiv 2311.10057](https://arxiv.org/pdf/2311.10057)
- MusicCaps: audio not distributed, only obtainable via YouTube with no persistence guarantee; derived from AudioSet — [Song Describer paper](https://arxiv.org/pdf/2311.10057)
- IRMAS: 9,579 instrument clips, CC BY-NC-SA 3.0; Slakh: 1,710 synthesized multitracks, CC BY 4.0 — [mirdata](https://mirdata.readthedocs.io/en/stable/source/quick_reference.html)
- RWC popular and Beatles include vocal activity annotations (no audio distributed) — [mirdata](https://mirdata.readthedocs.io/en/stable/source/quick_reference.html)

### Inferences
- For vocal presence in EDM, no public EDM-specific vocal-activity set was found; the most realistic calibration is a personal annotation of vocal regions on a sample of the user's own tracks.
- Separation-based vocal detection can be sanity-checked on MUSDB18-HQ, but genre mismatch (rock/pop) limits transfer to EDM vocal chops and pitched vocal leads.

### Gaps
- OpenMIC-2018, DEAM, Jamendo vocal-detection corpus (Ramona et al.): license/size not verified in this session.
- MTG-Jamendo count of "electronic" genre tracks not extracted (available in the repo `stats` folder).
- No EDM-specific mood dataset found.

## Large free-licensed audio corpora with EDM

### Takeaway
FMA (106,574 tracks, artist-chosen CC licenses, CC BY 4.0 metadata) and MTG-Jamendo (55k CC tracks) are the two big open corpora with electronic music; Mixotic (netlabel DJ mixes, archived on Internet Archive) and SoundCloud CC tracks (Yadati) are EDM-specific but small.

### Cited Findings
- FMA: small 8,000 clips (7.2 GiB), medium 25,000 (22 GiB), large 106,574 30 s clips (93 GiB), full 106,574 untrimmed (879 GiB); 161 (or 163) genres; audio under each artist's chosen license, "meant for research purposes"; metadata CC BY 4.0; archives on os.unil.cloud.switch.ch with SHA-1 checksums — [GitHub mdeff/fma](https://github.com/mdeff/fma)
- MTG-Jamendo audio carries per-track CC licenses — [GitHub MTG/mtg-jamendo-dataset](https://github.com/MTG/mtg-jamendo-dataset)
- Mixotic.net DJ set archive on Internet Archive — [archive.org](https://archive.org/details/mixotic.net_202209)

### Inferences
- FMA/Jamendo electronic tracks are mostly amateur/netlabel productions with different mixing and structure conventions than commercial club EDM; they are good for licensing-safe demos, weaker as calibration for an 800-track commercial collection.

### Gaps
- FMA "Electronic" top-genre count not verified this session (in `genres.csv`).
- ccMixter: not researched this session.
- FMA download link liveness in 2026 not tested.

## Evaluation metrics and how to run them

### Takeaway
Use `mir_eval`: `segment.detection` (HR.5F at 0.5 s, HR3F at 3 s), `segment.pairwise`, `segment.nce`; `beat.f_measure` (±70 ms) plus `beat.continuity` (CMLt/AMLt). For drops, use event-level hit rate with a tolerance window (Yadati used 3 to 15 s windows; beat/bar-based windows are tighter and fit EDM's grid better). Human agreement on structure is often quoted around 0.9 boundary F, but that figure is an estimate.

### Cited Findings
**mir_eval segment** — [mir_eval segment docs](https://mir-eval.readthedocs.io/latest/api/segment.html)
- `detection(ref_intervals, est_intervals, window=0.5, beta=1.0, trim=False)` → P/R/F; HR3F = same with `window=3`
- `deviation(..., trim=False)` → median ref→est and est→ref distances
- `pairwise`, `rand_index`, `ari`, `mutual_information`, `nce` (`marginal=False`), `vmeasure` (= nce with marginal=True); all frame-based, `frame_size=0.1`
- `evaluate()` runs all; inputs are (n,2) interval arrays plus labels; annotations must partition the track, use `mir_eval.util.adjust_intervals()`; load with `mir_eval.io.load_labeled_intervals()`

**mir_eval beat** — [mir_eval beat docs](https://mir-eval.readthedocs.io/latest/api/beat.html)
- `trim_beats(min_beat_time=5.0)` removes beats before 5 s (common preprocessing)
- `f_measure` threshold 0.07 s; `cemgil` sigma 0.04; `goto` (0.35/0.2/0.2); `p_score` 0.2 × median IBI; `continuity` phase/period thresholds 0.175 → CMLc, CMLt, AMLc, AMLt; `information_gain` 41 bins
- Downbeats are evaluated with the same functions on downbeat times (as Raveform does) — [TISMIR Raveform](https://transactions.ismir.net/articles/288)

**Structure evaluation guidance (Nieto et al., TISMIR 2020)** — [TISMIR 2020 review](https://transactions.ismir.net/articles/10.5334/tismir.54)
- Hit rate at 0.5 s (Turnbull 2007) or 3 s (Levy & Sandler 2008); prefer hit rate over median deviation (deviation hides outliers)
- Listeners prefer fewer correct boundaries to more imprecise ones → consider F-alpha with alpha < 1 (`beta` parameter)
- Pairwise F is over-sensitive to boundary placement; MIREX's NCE implementation is problematic; use L-measure (McFee et al. 2017) for hierarchies
- Human performance "thought to be around 90%" boundary F1 (cited to Serrà et al. 2014, an estimate, not a measured agreement study); best MIREX systems ~52 to 70% HR.5F, ~69 to 79% HR3F, ~58 to 69% pairwise F
- Single-reference metrics are limited by subjectivity; multiple annotations preferable
- Inter-annotator agreement is argued not to be a strict upper bound on system performance; L-measure paper reports a large gap between human-human and algorithm-human agreement on SALAMI — search summary of [Frontiers in Psychology 2017](https://www.frontiersin.org/article/10.3389/fpsyg.2017.01337) and related work (indirect)

**Reference numbers on EDM**
- EDMFormer on EDM-98: HR@0.5s 0.616, HR@3s 0.635 — [arXiv 2603.08759](https://arxiv.org/html/2603.08759v1)

**Drop / cue event evaluation**
- Yadati 2014: tolerance window of t seconds centred on both predicted and reference drop; tp/fp/fn computed as seconds of window overlap; F1 = 2tp/(2tp+fn+fp); t = 15, 13, 11, 9, 7, 5, 3 s; averaged per track; F1 up to 0.96 at 15 s; drop candidates restricted to segmentation boundaries (mean distance to true drop 2.5 s, <8% missed) — [Yadati et al. ISMIR 2014](https://archives.ismir.net/ismir2014/paper/000297.pdf)
- CUE-DETR: hits within beat-based windows T1 (1 beat, ~344 ms) and T1/2 (half beat, ~172 ms); P/R/F1 and AP; phase-aligned evaluation against 8- and 16-bar phrases; test set 101 tracks / 607 cues; T1 F1: Mixed In Key 0.22, CUE-DETR 0.36 to 0.39 — [arXiv 2407.06823](https://arxiv.org/html/2407.06823v1)

### Inferences
- A practical drop metric: treat each drop onset as an event and call `mir_eval.onset.f_measure(ref, est, window=w)` (or `segment.detection` on boundary lists) at several windows: half a beat (~0.25 s at 128 BPM), 1 bar (~1.9 s), and 4 bars; report label-conditional versions (only Drop boundaries) separately from all-boundary HR.5F/HR3F. EDM drops land on downbeats, so a beat/bar-relative window is more meaningful than Yadati's 3 to 15 s windows.
- Also report "drop count error" per track and "first drop time error", since lighting cues mostly depend on the main drop.
- Use HR3F as the primary structure score for a light-show use case (sub-second accuracy can come from snapping to the beat grid afterwards); report HR.5F as a grid-alignment check.

### Gaps
- Exact SALAMI inter-annotator figures (Smith et al. ISMIR 2011) not retrieved; PDF not accessible this session.
- `mir_eval.onset.f_measure` default window (commonly 0.05 s) not verified in docs this session.
- No published inter-annotator agreement for EDM drop placement found.

## Building a small personal human-rated ground truth

### Takeaway
Correcting model output (pre-filled boundaries, beat grid, drop markers) in a region editor is far cheaper than annotating from scratch; EDM's 8/16-bar phrase grid makes beat-snapped correction fast. Evidence suggests ~6 h per 100 tracks for drops only and many months for full multi-label structure, so a 50 to 100 track stratified set with a 10 to 20 track double-annotated subset is a realistic target.

### Cited Findings
- Drops only: one author marked drops in 100 tracks in ~6 h by skipping 30 s after each drop; drop defined as end of buildup and re-entry of bassline — [Yadati et al. ISMIR 2014](https://archives.ismir.net/ismir2014/paper/000297.pdf)
- Full EDM structure: 3 experts, ~7 months, 1,423 tracks, written guidelines refined in weekly meetings, rotating roles, verification stage and final review; disagreements decreased over time — [TISMIR Raveform](https://transactions.ismir.net/articles/288)
- 30 to 40 labeled tracks were enough to train a reasonable drop classifier in Yadati's experiments (11 s window) — [Yadati et al. ISMIR 2014](https://archives.ismir.net/ismir2014/paper/000297.pdf)
- 101 tracks / 607 cues was the held-out test set size for CUE-DETR; EDM-98 used 98 tracks stratified into five tempo bins — [arXiv 2407.06823](https://arxiv.org/html/2407.06823v1); [arXiv 2603.08759](https://arxiv.org/html/2603.08759v1)
- Weak labels (SoundCloud comments) are noisy: only 20% of drop comments were at the true drop — [Yadati et al. ISMIR 2014](https://archives.ismir.net/ismir2014/paper/000297.pdf)
- Cue points cluster on 8/16-bar multiples, matching EDM phrase structure — [chatpaper summary of EDM-CUE](https://chatpaper.com/paper/38517)
- Raveform label vocabulary (Intro, Buildup, Breakdown, Drop, Cooldown, Bridge, Outro, Ambient-Intro/Outro) is a ready-made guideline basis — [TISMIR Raveform](https://transactions.ismir.net/articles/288)
- Store annotations as JAMS (Harmonix ships JAMS 0.3.3) or Harmonix-style `time label` text files so mir_eval loaders read them directly — [GitHub harmonixset](https://github.com/urinieto/harmonixset); [mir_eval segment docs](https://mir-eval.readthedocs.io/latest/api/segment.html)

### Inferences
- Tools (not verified in sources this session): Sonic Visualiser (time-instant and region layers, exports CSV, can load analysis via Vamp plugins), Audacity label tracks (export `start\tend\tlabel` text, directly loadable by `mir_eval.io.load_labeled_intervals`), wavesurfer.js regions plugin (fits a custom web UI pre-filled with model output), Label Studio audio labeling (multi-user, exports JSON). For one person correcting model output, Audacity labels or a small wavesurfer.js page with beat-snapping is the lowest-friction option.
- Protocol: (1) adopt Raveform's vocabulary and write one-page definitions (e.g. drop = first downbeat where full bass/kick returns after a buildup); (2) pre-fill with the analyzer's output and correct, snapping to downbeats; (3) annotate a 10 to 20 track subset twice, a few weeks apart (or by a second person), and compute HR.5F/HR3F between passes as the personal human ceiling; (4) record a separate 1 to 5 "usability for lighting" rating per track to capture what matters beyond boundary accuracy; (5) freeze the set and never tune on it (keep a dev split).
- Correcting pre-filled output biases annotators toward the model's boundaries (anchoring); mitigate by annotating the double-annotated subset from scratch.
- Size: with per-track metrics averaged over N tracks, a difference of ~0.05 F between two systems typically needs paired comparison over roughly 50+ tracks to be distinguishable; 50 to 100 tracks matches published EDM test sets (98, 101). Use a paired test (Wilcoxon) per track rather than comparing means.
- Sampling: stratify the 800 MP3s by subgenre (from tags or Beatport genre), BPM bin (as EDM-98 did), era, and vocal vs. instrumental; deliberately include hard cases (no-drop tracks, double drops, long ambient intros, DJ edits/extended mixes vs. radio edits).
- Cross-check: match personal tracks by artist/title against Raveform `tracks.jsonl` and EDM-CUE to obtain free external annotations for owned files (verify the version/edit matches by duration).

### Gaps
- No primary source found this session comparing rating-vs-correcting protocols for music structure annotation, or quantifying anchoring bias.
- Annotation tool feature claims (Sonic Visualiser, Audacity, wavesurfer.js, Label Studio) are from general knowledge, not fetched docs.
- No statistical power study specific to music structure benchmarks was found; the 50+ track figure is a reasoned estimate.
